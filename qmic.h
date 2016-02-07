#ifndef __QMIC_H__
#define __QMIC_H__

#include <stdbool.h>

#define LIST_HEAD(type, name) \
struct { \
	struct type *head; \
	struct type *tail; \
} name;

#define LIST_ADD(list, elem) \
	if (list.tail) { \
		list.tail->next = elem; \
		list.tail = elem; \
	} else { \
		list.tail = elem; \
		list.head = elem; \
	}

enum {
	TOK_CONST = 256,
	TOK_ID,
	TOK_MESSAGE,
	TOK_NUM,
	TOK_PACKAGE,
	TOK_STRUCT,
	TOK_TYPE,
	TOK_REQUIRED,
	TOK_OPTIONAL,
};

enum {
	TYPE_U8,
	TYPE_U16,
	TYPE_U32,
	TYPE_U64,
	TYPE_STRING,
	TYPE_STRUCT
};

enum message_type {
	MESSAGE_REQUEST,
	MESSAGE_RESPONSE,
	MESSAGE_INDICATION,
};

struct token {
	int id;
	const char *str;
	unsigned num;
	struct qmi_struct *qmi_struct;
};

void yyerror(const char *fmt, ...);

void symbol_add(const char *name, int token, ...);

int token_accept(int id, struct token *tok);
void token_expect(int id, struct token *tok);

void qmi_message_parse(enum message_type message_type);
void qmi_message_source(FILE *fp, const char *package);
void qmi_message_header(FILE *fp, const char *package);

void qmi_struct_parse(void);
void qmi_struct_header(FILE *fp, const char *package);
void qmi_struct_emit_prototype(FILE *fp, const char *package, const char *message, const char *member, unsigned array_size, struct qmi_struct *qs);
void qmi_struct_emit_accessors(FILE *fp, const char *package, const char *message, const char *member, int member_id, unsigned array_size, struct qmi_struct *qs);

#endif
