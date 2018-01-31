#ifndef __PARSER_H__
#define __PARSER_H__

void qmi_parse(void);

extern const char *qmi_package;

struct qmi_const {
	const char *name;
	unsigned value;

	struct list_head node;
};
extern struct list_head qmi_consts;

#endif
