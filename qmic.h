#ifndef __QMIC_H__
#define __QMIC_H__

#include <stdbool.h>

#include "list.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

enum {
	TYPE_U8,
	TYPE_U16,
	TYPE_U32,
	TYPE_U64,
	TYPE_STRING,
	TYPE_STRUCT
};

enum message_type {
	MESSAGE_REQUEST = 0,
	MESSAGE_RESPONSE = 2,
	MESSAGE_INDICATION = 4,
};

extern const char *sz_simple_types[];

extern const char *qmi_package;

struct qmi_const {
	const char *name;
	unsigned value;

	struct list_head node;
};

struct qmi_message_member {
	const char *name;
	int type;
	struct qmi_struct *qmi_struct;
	int id;
	bool required;
	unsigned array_size;
	bool array_fixed;

	struct list_head node;
};

struct qmi_message {
	enum message_type type;
	const char *name;
	unsigned msg_id;

	struct list_head node;

	struct list_head members;
};

struct qmi_struct_member {
	const char *name;
	int type;

	struct list_head node;
};

struct qmi_struct {
	const char *name;

	struct list_head node;

	struct list_head members;
};

extern struct list_head qmi_consts;
extern struct list_head qmi_messages;
extern struct list_head qmi_structs;

void qmi_parse(void);

void emit_source_includes(FILE *fp, const char *package);
void guard_header(FILE *fp, const char *package);
void guard_footer(FILE *fp);
void qmi_const_header(FILE *fp);

void accessor_emit_c(FILE *fp, const char *package);
void accessor_emit_h(FILE *fp, const char *package);

void kernel_emit_c(FILE *fp, const char *package);
void kernel_emit_h(FILE *fp, const char *package);

#endif
