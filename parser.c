#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "list.h"
#include "parser.h"
#include "qmic.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static char scratch_buf[128];
static int scratch_pos;

static int yyline = 1;

static int input()
{
	static char input_buf[128];
	static int input_pos;
	static int input_len;
	int ret;
	int ch;

	if (scratch_pos) {
		ch = scratch_buf[--scratch_pos];
		goto out;
	}

	if (input_pos < input_len) {
		ch = input_buf[input_pos++];
		goto out;
	}

	ret = read(0, input_buf, sizeof(input_buf));
	if (ret <= 0)
		return ret;

	input_pos = 0;
	input_len = ret;

	ch = input_buf[input_pos++];
out:
	if (ch == '\n')
		yyline++;
	return ch;
}

static void unput(int ch)
{
	assert(scratch_pos < 128);
	scratch_buf[scratch_pos++] = ch;

	if (ch == '\n')
		yyline--;
}

struct symbol {
	int token;
	const char *name;

	int type;
	struct qmi_struct *qmi_struct;

	struct list_head node;
};

static struct list_head symbols = LIST_INIT(symbols);

void symbol_add(const char *name, int token, ...)
{
	struct symbol *sym;
	va_list ap;

	va_start(ap, token);

	sym = malloc(sizeof(struct symbol));
	sym->token = token;
	sym->name = name;

	switch (token) {
	case TOK_MESSAGE:
		sym->type = va_arg(ap, int);
		break;
	case TOK_TYPE:
		sym->type = va_arg(ap, int);
		if (sym->type == TYPE_STRUCT)
			sym->qmi_struct = va_arg(ap, struct qmi_struct *);
		break;
	}

	list_add(&symbols, &sym->node);

	va_end(ap);
}

static struct token yylex()
{
	struct symbol *sym;
	struct token token = {};
	char buf[128];
	char *p = buf;
	int base;
	int ch;

	list_for_each_entry(sym, &symbols, node);

	while ((ch = input()) && isspace(ch))
		;

	if (isalpha(ch)) {
		do {
			*p++ = ch;
			ch = input();
		} while (isalnum(ch) || ch == '_');
		unput(ch);
		*p = '\0';

		token.str = strdup(buf);
		list_for_each_entry(sym, &symbols, node) {
			if (strcmp(buf, sym->name) == 0) {
				token.id = sym->token;
				token.num = sym->type;
				token.qmi_struct = sym->qmi_struct;
				return token;
			}
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
	}

	token.id = ch;
	return token;
}

static struct token curr_token;

void yyerror(const char *fmt, ...)
{
	char buf[128];
	va_list ap;

	va_start(ap, fmt);

	vsprintf(buf, fmt, ap);
	printf("parse error on line %d: %s\n", yyline, buf);

	va_end(ap);
	exit(1);
}

static void token_init(void)
{
	curr_token = yylex();
}

int token_accept(int id, struct token *tok)
{
	if (curr_token.id == id) {
		if (tok)
			*tok = curr_token;

		curr_token = yylex();
		return 1;
	} else {
		return 0;
	}
}

void token_expect(int id, struct token *tok)
{
	if (!token_accept(id, tok)) {
		switch (id) {
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
			yyerror("expected '%c'", id);
		}
	}
}

static const char *parse_package()
{
	struct token tok;

	if (!token_accept(TOK_ID, &tok))
		yyerror("expected identifier");

	token_expect(';', NULL);
	return tok.str;
}

struct list_head qmi_consts = LIST_INIT(qmi_consts);

static void qmi_const_parse()
{
	struct qmi_const *qc;
	struct token num_tok;
	struct token id_tok;

	token_expect(TOK_ID, &id_tok);
	token_expect('=', NULL);
	token_expect(TOK_NUM, &num_tok);
	token_expect(';', NULL);

	qc = malloc(sizeof(struct qmi_const));
	qc->name = id_tok.str;
	qc->value = num_tok.num;

	list_add(&qmi_consts, &qc->node);
}

const char *qmi_package;

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
	while (!token_accept(0, NULL)) {
		if (token_accept(TOK_PACKAGE, NULL)) {
			qmi_package = parse_package();
		} else if (token_accept(TOK_CONST, NULL)) {
			qmi_const_parse();
		} else if (token_accept(TOK_STRUCT, NULL)) {
			qmi_struct_parse();
		} else if (token_accept(TOK_MESSAGE, &tok)) {
			qmi_message_parse(tok.num);
		} else {
			yyerror("unexpected symbol");
			break;
		}
	}
}