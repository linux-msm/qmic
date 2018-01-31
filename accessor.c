#include <stdio.h>
#include <stdlib.h>

#include "qmic.h"

static void qmi_struct_header(FILE *fp, const char *package)
{
	struct qmi_struct_member *qsm;
	struct qmi_struct *qs;

	list_for_each_entry(qs, &qmi_structs, node) {
		fprintf(fp, "struct %s_%s {\n",
			    package, qs->name);
		list_for_each_entry(qsm, &qs->members, node) {
			fprintf(fp, "\t%s %s;\n",
				    sz_simple_types[qsm->type], qsm->name);
		}
		fprintf(fp, "};\n"
			    "\n");
	}
}

static void qmi_struct_emit_prototype(FILE *fp,
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

static void qmi_struct_emit_accessors(FILE *fp,
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
	if (qmm->array_size) {
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
	if (qmm->array_size) {
		fprintf(fp, "int %1$s_%2$s_set_%3$s(struct %1$s_%2$s *%2$s, %4$s *val, size_t count)\n"
			    "{\n"
			    "	return qmi_tlv_set_array((struct qmi_tlv*)%2$s, %5$d, %6$d, val, count, sizeof(%4$s));\n"
			    "}\n\n",
			    package, message, qmm->name, sz_simple_types[qmm->type], qmm->id, qmm->array_size);

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
			    package, message, qmm->name, sz_simple_types[qmm->type], qmm->id, qmm->array_size);
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
	if (qmm->array_size) {
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

static void qmi_message_source(FILE *fp, const char *package)
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
				qmi_struct_emit_accessors(fp, package, qm->name, qmm->name, qmm->id, qmm->array_size, qmm->qmi_struct);
				break;
			};
		}
	}
}

static void qmi_message_header(FILE *fp, const char *package)
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
				qmi_struct_emit_prototype(fp, package, qm->name, qmm->name, qmm->array_size, qmm->qmi_struct);
				break;
			};
		}
	}
}

static void emit_header_file_header(FILE *fp)
{
	fprintf(fp, "#include <stdint.h>\n"
		    "#include <stdlib.h>\n\n");
	fprintf(fp, "struct qmi_tlv;\n"
		    "\n"
		    "struct qmi_tlv *qmi_tlv_init(unsigned txn, unsigned msg_id, unsigned type);\n"
		    "struct qmi_tlv *qmi_tlv_decode(void *buf, size_t len, unsigned *txn, unsigned type);\n"
		    "void *qmi_tlv_encode(struct qmi_tlv *tlv, size_t *len);\n"
		    "void qmi_tlv_free(struct qmi_tlv *tlv);\n"
		    "\n"
		    "void *qmi_tlv_get(struct qmi_tlv *tlv, unsigned id, size_t *len);\n"
		    "void *qmi_tlv_get_array(struct qmi_tlv *tlv, unsigned id, unsigned len_size, size_t *len, size_t *size);\n"
		    "int qmi_tlv_set(struct qmi_tlv *tlv, unsigned id, void *buf, size_t len);\n"
		    "int qmi_tlv_set_array(struct qmi_tlv *tlv, unsigned id, unsigned len_size, void *buf, size_t len, size_t size);\n"
		    "\n");
}

void accessor_emit_c(FILE *fp, const char *package)
{
	emit_source_includes(fp, package);
	qmi_message_source(fp, package);
}
	
void accessor_emit_h(FILE *fp, const char *package)
{
	guard_header(fp, qmi_package);
	emit_header_file_header(fp);
	qmi_const_header(fp);
	qmi_struct_header(fp, qmi_package);
	qmi_message_header(fp, qmi_package);
	guard_footer(fp);
}
