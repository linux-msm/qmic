#include <stdio.h>
#include <stdlib.h>
#include "qmic.h"

static const char *sz_simple_types[] = {
	[TYPE_U8] = "uint8_t",
	[TYPE_U16] = "uint16_t",
	[TYPE_U32] = "uint32_t",
	[TYPE_U64] = "uint64_t",
};

struct qmi_struct_member {
	const char *name;
	int type;

	struct qmi_struct_member *next;
};

struct qmi_struct {
	const char *name;

	struct qmi_struct *next;

	LIST_HEAD(qmi_struct_member, members);
};

LIST_HEAD(qmi_struct, qmi_structs);

void qmi_struct_parse(void)
{
	struct qmi_struct_member *qsm;
	struct token struct_id_tok;
	struct qmi_struct *qs;
	struct token type_tok;
	struct token id_tok;

	token_expect(TOK_ID, &struct_id_tok);
	token_expect('{', NULL);

	qs = malloc(sizeof(struct qmi_struct));
	qs->name = struct_id_tok.str;

	while (token_accept(TOK_TYPE, &type_tok)) {
		token_expect(TOK_ID, &id_tok);
		token_expect(';', NULL);

		qsm = malloc(sizeof(struct qmi_struct_member));
		qsm->name = id_tok.str;
		qsm->type = type_tok.num;

		LIST_ADD(qs->members, qsm);
	}

	token_expect('}', NULL);
	token_expect(';', NULL);

	LIST_ADD(qmi_structs, qs);

	symbol_add(qs->name, TOK_TYPE, TYPE_STRUCT, qs);
}

void qmi_struct_header(FILE *fp, const char *package)
{
	struct qmi_struct_member *qsm;
	struct qmi_struct *qs;

	for (qs = qmi_structs.head; qs; qs = qs->next) {
		fprintf(fp, "struct %s_%s {\n",
			    package, qs->name);
		for (qsm = qs->members.head; qsm; qsm = qsm->next) {
			fprintf(fp, "\t%s %s;\n",
				    sz_simple_types[qsm->type], qsm->name);
		}
		fprintf(fp, "};\n"
			    "\n");
	}
}

void qmi_struct_emit_prototype(FILE *fp,
			       const char *package,
			       const char *message,
			       const char *member,
			       unsigned array_size,
			       struct qmi_struct *qs)
{
	if (array_size) {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, struct %1$s_%4$s *val, size_t count);\n",
			    package, message, member, qs->name);

		fprintf(fp, "struct %1$s_%4$s *%1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s, size_t *count);\n\n",
			    package, message, member, qs->name);
	} else {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, struct %1$s_%4$s *val);\n",
			    package, message, member, qs->name);

		fprintf(fp, "struct %1$s_%4$s *%1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s);\n\n",
			    package, message, member, qs->name);
	}
}

void qmi_struct_emit_accessors(FILE *fp,
			       const char *package,
			       const char *message,
			       const char *member,
			       int member_id,
			       unsigned array_size,
			       struct qmi_struct *qs)
{
	if (array_size) {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, struct %1$s_%4$s *val, size_t count)\n"
			    "{\n"
			    "	return qmi_tlv_set_array((struct qmi_tlv*)%2$s, %5$d, %6$d, val, count, sizeof(struct %1$s_%4$s));\n"
			    "}\n\n",
			    package, message, member, qs->name, member_id, array_size);

		fprintf(fp, "struct %1$s_%4$s *%1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s, size_t *count)\n"
			    "{\n"
			    "	size_t size;\n"
			    "	size_t len;\n"
			    "	void *ptr;\n"
			    "\n"
			    "	ptr = qmi_tlv_get_array((struct qmi_tlv*)%2$s, %5$d, %6$d, &len, &size);\n"
			    "	if (!ptr)\n"
			    "		return NULL;\n"
			    "\n"
			    "	if (size != sizeof(struct %1$s_%4$s))\n"
			    "		return NULL;\n"
			    "\n"
			    "	*count = len;\n"
			    "	return ptr;\n"
			    "}\n\n",
			    package, message, member, qs->name, member_id, array_size);
	} else {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, struct %1$s_%4$s *val)\n"
			    "{\n"
			    "	return qmi_tlv_set((struct qmi_tlv*)%2$s, %5$d, val, sizeof(struct %1$s_%4$s));\n"
			    "}\n\n",
			    package, message, member, qs->name, member_id);

		fprintf(fp, "struct %1$s_%4$s *%1$s_%2$s_get_%3$s(struct %1$s_%2$s *%2$s)\n"
			    "{\n"
			    "	size_t len;\n"
			    "	void *ptr;\n"
			    "\n"
			    "	ptr = qmi_tlv_get((struct qmi_tlv*)%2$s, %5$d, &len);\n"
			    "	if (!ptr)\n"
			    "		return NULL;\n"
			    "\n"
			    "	if (len != sizeof(struct %1$s_%4$s))\n"
			    "		return NULL;\n"
			    "\n"
			    "	return ptr;\n"
			    "}\n\n",
			    package, message, member, qs->name, member_id);
	}
}
