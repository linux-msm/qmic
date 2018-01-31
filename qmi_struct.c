#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "qmic.h"

void qmi_struct_header(FILE *fp, const char *package)
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
