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
#include "qmic.h"

const char *sz_simple_types[] = {
	[TYPE_U8] = "uint8_t",
	[TYPE_U16] = "uint16_t",
	[TYPE_U32] = "uint32_t",
	[TYPE_U64] = "uint64_t",
	[TYPE_STRING] = "char *",
};

static void qmi_const_header(FILE *fp)
{
	struct qmi_const *qc;

	if (list_empty(&qmi_consts))
		return;

	list_for_each_entry(qc, &qmi_consts, node)
		fprintf(fp, "#define %s %d\n", qc->name, qc->value);

	fprintf(fp, "\n");
}

static void guard_header(FILE *fp, const char *package)
{
	char *upper;
	char *p;

	upper = p = strdup(package);
	while (*p) {
		*p = toupper(*p);
		p++;
	}

	fprintf(fp, "#ifndef __QMI_%s_H__\n", upper);
	fprintf(fp, "#define __QMI_%s_H__\n", upper);
	fprintf(fp, "\n");
}

static void guard_footer(FILE *fp)
{
	fprintf(fp, "#endif\n");
}

int main(int argc, char **argv)
{
	char fname[256];
	FILE *hfp;
	FILE *sfp;

	qmi_parse();

	snprintf(fname, sizeof(fname), "qmi_%s.c", qmi_package);
	sfp = fopen(fname, "w");
	if (!sfp)
		err(1, "failed to open %s", fname);

	snprintf(fname, sizeof(fname), "qmi_%s.h", qmi_package);
	hfp = fopen(fname, "w");
	if (!hfp)
		err(1, "failed to open %s", fname);

	fprintf(sfp, "#include <errno.h>\n"
		     "#include <string.h>\n"
		     "#include \"qmi_%1$s.h\"\n\n",
		     qmi_package);
	qmi_message_source(sfp, qmi_package);

	guard_header(hfp, qmi_package);
	fprintf(hfp, "#include <stdint.h>\n"
		     "#include <stdlib.h>\n\n");
	fprintf(hfp, "struct qmi_tlv;\n"
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
	qmi_const_header(hfp);
	qmi_struct_header(hfp, qmi_package);
	qmi_message_header(hfp, qmi_package);
	guard_footer(hfp);

	fclose(hfp);
	fclose(sfp);

	return 0;
}
