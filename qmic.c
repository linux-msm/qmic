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

void qmi_const_header(FILE *fp)
{
	struct qmi_const *qc;

	if (list_empty(&qmi_consts))
		return;

	list_for_each_entry(qc, &qmi_consts, node)
		fprintf(fp, "#define %s %d\n", qc->name, qc->value);

	fprintf(fp, "\n");
}

void emit_source_includes(FILE *fp, const char *package)
{
	fprintf(fp, "#include <errno.h>\n"
		    "#include <string.h>\n"
		    "#include \"qmi_%1$s.h\"\n\n",
		    package);
}

void guard_header(FILE *fp, const char *package)
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

void guard_footer(FILE *fp)
{
	fprintf(fp, "#endif\n");
}

static void usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "Usage: %s [-ak]\n", __progname);
	exit(1);
}

int main(int argc, char **argv)
{
	char fname[256];
	FILE *hfp;
	FILE *sfp;
	int method = 0;
	int opt;

	while ((opt = getopt(argc, argv, "ak")) != -1) {
		switch (opt) {
		case 'a':
			method = 0;
			break;
		case 'k':
			method = 1;
			break;
		default:
			usage();
		}
	}

	qmi_parse();

	snprintf(fname, sizeof(fname), "qmi_%s.c", qmi_package);
	sfp = fopen(fname, "w");
	if (!sfp)
		err(1, "failed to open %s", fname);

	snprintf(fname, sizeof(fname), "qmi_%s.h", qmi_package);
	hfp = fopen(fname, "w");
	if (!hfp)
		err(1, "failed to open %s", fname);

	switch (method) {
	case 0:
		accessor_emit_c(sfp, qmi_package);
		accessor_emit_h(hfp, qmi_package);
		break;
	case 1:
		kernel_emit_c(sfp, qmi_package);
		kernel_emit_h(hfp, qmi_package);
		break;
	}

	fclose(hfp);
	fclose(sfp);

	return 0;
}
