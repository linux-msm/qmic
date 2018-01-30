#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "list.h"
#include "qmic.h"

static const char *sz_simple_types[] = {
	[TYPE_U8] = "uint8_t",
	[TYPE_U16] = "uint16_t",
	[TYPE_U32] = "uint32_t",
	[TYPE_U64] = "uint64_t",
};

struct qmi_message_member {
	const char *name;
	int type;
	struct qmi_struct *qmi_struct;
	int id;
	bool required;
	unsigned array;

	struct list_head node;
};

struct qmi_message {
	enum message_type type;
	const char *name;
	unsigned msg_id;

	struct list_head node;

	struct list_head members;
};

static struct list_head qmi_messages = LIST_INIT(qmi_messages);

void qmi_message_parse(enum message_type message_type)
{
	struct qmi_message_member *qmm;
	struct qmi_message *qm;
	struct token msg_id_tok;
	struct token type_tok;
	struct token num_tok;
	struct token id_tok;
	unsigned array;
	bool required;

	token_expect(TOK_ID, &msg_id_tok);
	token_expect('{', NULL);

	qm = malloc(sizeof(struct qmi_message));
	qm->name = msg_id_tok.str;
	qm->type = message_type;
	list_init(&qm->members);

	while (!token_accept('}', NULL)) {
		array = 0;

		if (token_accept(TOK_REQUIRED, NULL))
			required = true;
		else if (token_accept(TOK_OPTIONAL, NULL))
			required = false;
		else
			yyerror("expected required, optional or '}'");

		token_expect(TOK_TYPE, &type_tok);
		token_expect(TOK_ID, &id_tok);

		if (token_accept('[', NULL)) {
			array = 1;
			if (token_accept(TOK_NUM, &num_tok)) {
				if (num_tok.num & 0xffff0000)
					array = 4;
				else if (num_tok.num & 0xff00)
					array = 2;
			}

			token_expect(']', NULL);
		}

		token_expect('=', NULL);
		token_expect(TOK_NUM, &num_tok);
		token_expect(';', NULL);

		qmm = malloc(sizeof(struct qmi_message_member));
		qmm->name = id_tok.str;
		qmm->type = type_tok.num;
		qmm->qmi_struct = type_tok.qmi_struct;
		qmm->id = num_tok.num;
		qmm->required = required;
		qmm->array = array;

		list_add(&qm->members, &qmm->node);
	}

	if (token_accept('=', NULL)) {
		token_expect(TOK_NUM, &num_tok);

		qm->msg_id = num_tok.num;
	}

	token_expect(';', NULL);

	list_add(&qmi_messages, &qm->node);
}

static void qmi_message_emit_message_type(FILE *fp,
					  const char *package,
					  const char *message)
{
	fprintf(fp, "struct %s_%s;\n", package, message);
}

static void qmi_message_emit_message_prototype(FILE *fp,
					       const char *package,
					       const char *message)
{
	fprintf(fp, "/*\n"
		    " * %1$s_%2$s message\n"
		    " */\n",
		    package, message);

	fprintf(fp, "struct %1$s_%2$s *%1$s_%2$s_alloc(unsigned txn);\n",
		    package, message);

	fprintf(fp, "struct %1$s_%2$s *%1$s_%2$s_parse(void *buf, size_t len, unsigned *txn);\n",
		    package, message);

	fprintf(fp, "void *%1$s_%2$s_encode(struct %1$s_%2$s *%2$s, size_t *len);\n",
		    package, message);

	fprintf(fp, "void %1$s_%2$s_free(struct %1$s_%2$s *%2$s);\n\n",
		    package, message);
}

static void qmi_message_emit_message(FILE *fp,
				     const char *package,
				     struct qmi_message *qm)
{
	fprintf(fp, "struct %1$s_%2$s *%1$s_%2$s_alloc(unsigned txn)\n"
		    "{\n"
		    "	return (struct %1$s_%2$s*)qmi_tlv_init(txn, %3$d, %4$d);\n"
		    "}\n\n",
		    package, qm->name, qm->msg_id, qm->type);

	fprintf(fp, "struct %1$s_%2$s *%1$s_%2$s_parse(void *buf, size_t len, unsigned *txn)\n"
		    "{\n"
		    "	return (struct %1$s_%2$s*)qmi_tlv_decode(buf, len, txn, %3$d);\n"
		    "}\n\n",
		    package, qm->name, qm->type);

	fprintf(fp, "void *%1$s_%2$s_encode(struct %1$s_%2$s *%2$s, size_t *len)\n"
		    "{\n"
		    "	return qmi_tlv_encode((struct qmi_tlv*)%2$s, len);\n"
		    "}\n\n",
		    package, qm->name);

	fprintf(fp, "void %1$s_%2$s_free(struct %1$s_%2$s *%2$s)\n"
		    "{\n"
		    "	qmi_tlv_free((struct qmi_tlv*)%2$s);\n"
		    "}\n\n",
		    package, qm->name);
}

static void qmi_message_emit_simple_prototype(FILE *fp,
					      const char *package,
					      const char *message,
					      struct qmi_message_member *qmm)
{
	if (qmm->array) {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, %4$s *val, size_t count);\n",
			    package, message, qmm->name, sz_simple_types[qmm->type]);

		fprintf(fp, "%4$s *%1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s, size_t *count);\n\n",
			    package, message, qmm->name, sz_simple_types[qmm->type]);
	} else {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, %4$s val);\n",
			    package, message, qmm->name, sz_simple_types[qmm->type]);

		fprintf(fp, "int %1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s, %4$s *val);\n\n",
			    package, message, qmm->name, sz_simple_types[qmm->type]);
	}
}


static void qmi_message_emit_simple_accessors(FILE *fp,
					      const char *package,
					      const char *message,
					      struct qmi_message_member *qmm)
{
	if (qmm->array) {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, %4$s *val, size_t count)\n"
			    "{\n"
			    "	return qmi_tlv_set_array((struct qmi_tlv*)%2$s, %5$d, %6$d, val, count, sizeof(%4$s));\n"
			    "}\n\n",
			    package, message, qmm->name, sz_simple_types[qmm->type], qmm->id, qmm->array);

		fprintf(fp, "%4$s *%1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s, size_t *count)\n"
			    "{\n"
			    "	%4$s *ptr;\n"
			    "	size_t size;\n"
			    "	size_t len;\n"
			    "\n"
			    "	ptr = qmi_tlv_get_array((struct qmi_tlv*)%2$s, %5$d, %6$d, &len, &size);\n"
			    "	if (!ptr)\n"
			    "		return NULL;\n"
			    "\n"
			    "	if (size != sizeof(%4$s))\n"
			    "		return NULL;\n"
			    "\n"
			    "	*count = len;\n"
			    "	return ptr;\n"
			    "}\n\n",
			    package, message, qmm->name, sz_simple_types[qmm->type], qmm->id, qmm->array);
	} else {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, %4$s val)\n"
			    "{\n"
			    "	return qmi_tlv_set((struct qmi_tlv*)%2$s, %5$d, &val, sizeof(%4$s));\n"
			    "}\n\n",
			    package, message, qmm->name, sz_simple_types[qmm->type], qmm->id);

		fprintf(fp, "int %1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s, %4$s *val)\n"
			    "{\n"
			    "	%4$s *ptr;\n"
			    "	size_t len;\n"
			    "\n"
			    "	ptr = qmi_tlv_get((struct qmi_tlv*)%2$s, %5$d, &len);\n"
			    "	if (!ptr)\n"
			    "		return -ENOENT;\n"
			    "\n"
			    "	if (len != sizeof(%4$s))\n"
			    "		return -EINVAL;\n"
			    "\n"
			    "	*val = *(%4$s*)ptr;\n"
			    "	return 0;\n"
			    "}\n\n",
			    package, message, qmm->name, sz_simple_types[qmm->type], qmm->id);
	}
}

static void qmi_message_emit_string_prototype(FILE *fp,
					      const char *package,
					      const char *message,
					      struct qmi_message_member *qmm)
{
	if (qmm->array) {
		fprintf(stderr, "Dont' know how to encode string arrays yet");
		exit(1);
	} else {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, char *buf, size_t len);\n",
			    package, message, qmm->name);

		fprintf(fp, "int %1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s, char *buf, size_t buflen);\n\n",
			    package, message, qmm->name);
	}
}

static void qmi_message_emit_string_accessors(FILE *fp,
					      const char *package,
					      const char *message,
					      struct qmi_message_member *qmm)
{
	fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, char *buf, size_t len)\n"
		    "{\n"
		    "	return qmi_tlv_set((struct qmi_tlv*)%2$s, %4$d, buf, len);\n"
		    "}\n\n",
		    package, message, qmm->name, qmm->id);

	fprintf(fp, "int %1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s, char *buf, size_t buflen)\n"
		    "{\n"
		    "	size_t len;\n"
		    "	char *ptr;\n"
		    "\n"
		    "	ptr = qmi_tlv_get((struct qmi_tlv*)%2$s, %4$d, &len);\n"
		    "	if (!ptr)\n"
		    "		return -ENOENT;\n"
		    "\n"
		    "	if (len >= buflen)\n"
		    "		return -ENOMEM;\n"
		    "\n"
		    "	memcpy(buf, ptr, len);\n"
		    "	buf[len] = '\\0';\n"
		    "	return len;\n"
		    "}\n\n",
		    package, message, qmm->name, qmm->id);

}

void qmi_message_source(FILE *fp, const char *package)
{
	struct qmi_message_member *qmm;
	struct qmi_message *qm;

	list_for_each_entry(qm, &qmi_messages, node) {
		qmi_message_emit_message(fp, package, qm);

		list_for_each_entry(qmm, &qm->members, node) {
			switch (qmm->type) {
			case TYPE_U8:
			case TYPE_U16:
			case TYPE_U32:
			case TYPE_U64:
				qmi_message_emit_simple_accessors(fp, package, qm->name, qmm);
				break;
			case TYPE_STRING:
				qmi_message_emit_string_accessors(fp, package, qm->name, qmm);
				break;
			case TYPE_STRUCT:
				qmi_struct_emit_accessors(fp, package, qm->name, qmm->name, qmm->id, qmm->array, qmm->qmi_struct);
				break;
			};
		}
	}
}

void qmi_message_header(FILE *fp, const char *package)
{
	struct qmi_message_member *qmm;
	struct qmi_message *qm;

	list_for_each_entry(qm, &qmi_messages, node)
		qmi_message_emit_message_type(fp, package, qm->name);

	fprintf(fp, "\n");

	list_for_each_entry(qm, &qmi_messages, node) {
		qmi_message_emit_message_prototype(fp, package, qm->name);

		list_for_each_entry(qmm, &qm->members, node) {
			switch (qmm->type) {
			case TYPE_U8:
			case TYPE_U16:
			case TYPE_U32:
			case TYPE_U64:
				qmi_message_emit_simple_prototype(fp, package, qm->name, qmm);
				break;
			case TYPE_STRING:
				qmi_message_emit_string_prototype(fp, package, qm->name, qmm);
				break;
			case TYPE_STRUCT:
				qmi_struct_emit_prototype(fp, package, qm->name, qmm->name, qmm->array, qmm->qmi_struct);
				break;
			};
		}
	}
}
