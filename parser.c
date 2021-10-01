#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "list.h"
#include "qmic.h"

/* Allocate and zero a block of memory; and exit if it fails */
#define memalloc(size) ({						\
		void *__p = malloc(size);				\
									\
		if (!__p)						\
			errx(1, "malloc() failed in %s(), line %d\n",	\
				__func__, __LINE__);			\
		memset(__p, 0, size);					\
		__p;							\
	 })

const char *qmi_package;

struct list_head qmi_consts = LIST_INIT(qmi_consts);
struct list_head qmi_messages = LIST_INIT(qmi_messages);
struct list_head qmi_structs = LIST_INIT(qmi_structs);

enum token_id {
	/* Also any non-NUL (7-bit) ASCII character */
	TOK_CONST = CHAR_MAX + 1,
	TOK_ID,
	TOK_MESSAGE,
	TOK_NUM,
	TOK_PACKAGE,
	TOK_STRUCT,
	TOK_TYPE,
	TOK_REQUIRED,
	TOK_OPTIONAL,
	TOK_EOF,
};

struct token {
	enum token_id id;
	char *str;
	unsigned num;
	struct qmi_struct *qmi_struct;
};

static int yyline = 1;

static void yyerror(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	fprintf(stderr, "%s: parse error on line %u:\n\t",
		program_invocation_short_name, yyline);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");

	va_end(ap);

	exit(1);
}

static char input()
{
	int ch;

	ch = fgetc(stdin);
	if (ch < 0)
		return 0;	/* End of input */

	if (ch == '\n')
		yyline++;
	else if (!isascii(ch))
		yyerror("invalid non-ASCII character");
	else if (!ch)
		yyerror("invalid NUL character");

	return (char)ch;
}

static void unput(int ch)
{
	if (ch == '\n')
		yyline--;
	if (ungetc(ch, stdin) != ch)
		yyerror("ungetc error");
}

struct symbol {
	enum token_id token_id;
	const char *name;

	union {
		enum message_type message_type;		/* TOK_MESSAGE */
		struct {				/* TOK_TYPE */
			enum symbol_type symbol_type;
			/* TYPE_STRUCT also has a struct pointer */
			struct qmi_struct *qmi_struct;
		};
	};

	struct list_head node;
};

static struct list_head symbols = LIST_INIT(symbols);

static void symbol_add(const char *name, enum token_id token_id, ...)
{
	struct symbol *sym;
	va_list ap;

	va_start(ap, token_id);

	sym = memalloc(sizeof(struct symbol));
	sym->token_id = token_id;
	sym->name = name;

	switch (token_id) {
	case TOK_MESSAGE:
		sym->message_type = va_arg(ap, enum message_type);
		break;
	case TOK_TYPE:
		sym->symbol_type = va_arg(ap, enum symbol_type);
		if (sym->symbol_type == TYPE_STRUCT)
			sym->qmi_struct = va_arg(ap, struct qmi_struct *);
		break;
	default:
		break;	/* Most tokens are standalone */
	}

	list_add(&symbols, &sym->node);

	va_end(ap);
}

/* Skip over white space and comments (which start with '#', end with '\n') */
static bool skip(char ch)
{
	static bool in_comment = false;

	if (in_comment) {
		if (ch == '\n')
			in_comment = false;
		return true;
	}

	if (isspace(ch))
		return true;

	if (ch == '#')
		in_comment = true;

	return in_comment;
}

static struct token yylex()
{
	struct symbol *sym;
	struct token token = {};
	char buf[128];
	char *p = buf;
	int base;
	char ch;

	while ((ch = input()) && skip(ch))
		;

	if (isalpha(ch)) {
		do {
			*p++ = ch;
			ch = input();
		} while (isalnum(ch) || ch == '_');
		unput(ch);
		*p = '\0';

		token.str = strdup(buf);
		if (!token.str)
			yyerror("strdup() failed in %s(), line %d\n",
				__func__, __LINE__);
		list_for_each_entry(sym, &symbols, node) {
			if (strcmp(buf, sym->name))
				continue;

			token.id = sym->token_id;
			switch (token.id) {
			case TOK_MESSAGE:
				token.num = sym->message_type;
				break;
			case TOK_TYPE:
				token.num = sym->symbol_type;
				token.qmi_struct = sym->qmi_struct;
				break;
			default:
				break;
			}

			return token;
		}

		token.id = TOK_ID;

		return token;
	} else if (isdigit(ch)) {
		do {
			*p++ = ch;
			ch = input();
		} while (isdigit(ch) || (p - buf == 1 && ch == 'x'));
		unput(ch);
		*p = '\0';

		if (buf[0] == '0' && buf[1] == 'x')
			base = 16;
		else if (buf[0] == '0')
			base = 8;
		else
			base = 10;

		token.id = TOK_NUM;
		token.num = strtol(buf, NULL, base);
		return token;
	} else if (!ch) {
		token.id = TOK_EOF;

		return token;
	}

	token.id = ch;
	return token;
}

static struct token curr_token;

static void token_init(void)
{
	curr_token = yylex();
}

static int token_accept(enum token_id token_id, struct token *tok)
{
	if (curr_token.id == token_id) {
		if (tok)
			*tok = curr_token;
		else if (curr_token.str)
			free(curr_token.str);

		curr_token = yylex();
		return 1;
	} else {
		return 0;
	}
}

static void token_expect(enum token_id token_id, struct token *tok)
{
	if (!token_accept(token_id, tok)) {
		switch (token_id) {
		case TOK_CONST:
			yyerror("expected const");
		case TOK_ID:
			yyerror("expected identifier");
		case TOK_MESSAGE:
			yyerror("expected message");
		case TOK_NUM:
			yyerror("expected num");
		case TOK_PACKAGE:
			yyerror("expected package");
		case TOK_STRUCT:
			yyerror("expected struct");
		case TOK_TYPE:
			yyerror("expected type");
		case TOK_REQUIRED:
			yyerror("expected required");
		case TOK_OPTIONAL:
			yyerror("expected optional");
		default:
			yyerror("expected '%c'", token_id);
		}
	}
}

static const char *parse_package()
{
	struct token tok;

	token_expect(TOK_ID, &tok);
	token_expect(';', NULL);
	return tok.str;
}

static void qmi_const_parse()
{
	struct qmi_const *qc;
	struct token num_tok;
	struct token id_tok;

	token_expect(TOK_ID, &id_tok);
	token_expect('=', NULL);
	token_expect(TOK_NUM, &num_tok);
	token_expect(';', NULL);

	qc = memalloc(sizeof(struct qmi_const));
	qc->name = id_tok.str;
	qc->value = num_tok.num;

	list_add(&qmi_consts, &qc->node);
}

static void qmi_message_parse(enum message_type message_type)
{
	struct qmi_message_member *qmm;
	struct qmi_message *qm;
	struct token msg_id_tok;
	struct token type_tok;
	struct token num_tok;
	struct token id_tok;
	unsigned array_size;
	bool array_fixed;
	bool required;

	token_expect(TOK_ID, &msg_id_tok);
	token_expect('{', NULL);

	qm = memalloc(sizeof(struct qmi_message));
	qm->name = msg_id_tok.str;
	qm->type = message_type;
	list_init(&qm->members);

	while (!token_accept('}', NULL)) {
		if (token_accept(TOK_REQUIRED, NULL))
			required = true;
		else if (token_accept(TOK_OPTIONAL, NULL))
			required = false;
		else
			yyerror("expected required, optional or '}'");

		token_expect(TOK_TYPE, &type_tok);
		token_expect(TOK_ID, &id_tok);

		if (token_accept('[', NULL)) {
			token_expect(TOK_NUM, &num_tok);
			array_size = num_tok.num;
			token_expect(']', NULL);
			array_fixed = true;
		} else if (token_accept('(', NULL)) {
			token_expect(TOK_NUM, &num_tok);
			array_size = num_tok.num;
			token_expect(')', NULL);
			array_fixed = false;
		} else {
			array_size = 0;
			array_fixed = false;
		}

		token_expect('=', NULL);
		token_expect(TOK_NUM, &num_tok);
		token_expect(';', NULL);

		qmm = memalloc(sizeof(struct qmi_message_member));
		qmm->name = id_tok.str;
		qmm->type = type_tok.num;
		if (type_tok.str)
			free(type_tok.str);
		qmm->qmi_struct = type_tok.qmi_struct;
		qmm->id = num_tok.num;
		qmm->required = required;
		qmm->array_size = array_size;
		qmm->array_fixed = array_fixed;

		list_add(&qm->members, &qmm->node);
	}

	if (token_accept('=', NULL)) {
		token_expect(TOK_NUM, &num_tok);

		qm->msg_id = num_tok.num;
	}

	token_expect(';', NULL);

	list_add(&qmi_messages, &qm->node);
}

static void qmi_struct_parse(void)
{
	struct qmi_struct_member *qsm;
	struct token struct_id_tok;
	struct qmi_struct *qs;
	struct token type_tok;
	struct token id_tok;

	token_expect(TOK_ID, &struct_id_tok);
	token_expect('{', NULL);

	qs = memalloc(sizeof(struct qmi_struct));
	qs->name = struct_id_tok.str;
	list_init(&qs->members);

	while (token_accept(TOK_TYPE, &type_tok)) {
		token_expect(TOK_ID, &id_tok);
		token_expect(';', NULL);

		qsm = memalloc(sizeof(struct qmi_struct_member));
		qsm->name = id_tok.str;
		qsm->type = type_tok.num;
		if (type_tok.str)
			free(type_tok.str);

		list_add(&qs->members, &qsm->node);
	}

	token_expect('}', NULL);
	token_expect(';', NULL);

	list_add(&qmi_structs, &qs->node);

	symbol_add(qs->name, TOK_TYPE, TYPE_STRUCT, qs);
}

void qmi_parse(void)
{
	struct token tok;

	/* PACKAGE ID<string> ';' */
	/* CONST ID<string> '=' NUM<num> ';' */
	/* STRUCT ID<string> '{' ... '}' ';' */
		/* TYPE<type*> ID<string> ';' */
	/* MESSAGE ID<string> '{' ... '}' ';' */
		/* (REQUIRED | OPTIONAL) TYPE<type*> ID<string> '=' NUM<num> ';' */

	symbol_add("const", TOK_CONST);
	symbol_add("optional", TOK_OPTIONAL);
	symbol_add("message", TOK_MESSAGE, MESSAGE_RESPONSE); /* backward compatible with early hacking */
	symbol_add("request", TOK_MESSAGE, MESSAGE_REQUEST);
	symbol_add("response", TOK_MESSAGE, MESSAGE_RESPONSE);
	symbol_add("indication", TOK_MESSAGE, MESSAGE_INDICATION);
	symbol_add("package", TOK_PACKAGE);
	symbol_add("required", TOK_REQUIRED);
	symbol_add("struct", TOK_STRUCT);
	symbol_add("string", TOK_TYPE, TYPE_STRING);
	symbol_add("u8", TOK_TYPE, TYPE_U8);
	symbol_add("u16", TOK_TYPE, TYPE_U16);
	symbol_add("u32", TOK_TYPE, TYPE_U32);
	symbol_add("u64", TOK_TYPE, TYPE_U64);

	token_init();
	while (!token_accept(TOK_EOF, NULL)) {
		if (token_accept(TOK_PACKAGE, NULL)) {
			qmi_package = parse_package();
		} else if (token_accept(TOK_CONST, NULL)) {
			qmi_const_parse();
		} else if (token_accept(TOK_STRUCT, NULL)) {
			qmi_struct_parse();
		} else if (token_accept(TOK_MESSAGE, &tok)) {
			qmi_message_parse(tok.num);
			free(tok.str);
		} else {
			yyerror("unexpected symbol");
			break;
		}
	}
}
