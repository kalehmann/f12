#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

static int evasprintf(char **strp, const char *fmt, va_list ap)
{
	char *tmp = *strp;
	int ret = vasprintf(strp, fmt, ap);
	if (tmp) {
		free(tmp);
	}

	return ret;
}

int esprintf(char **strp, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = evasprintf(strp, fmt, ap);
	va_end(ap);

	return ret;
}

int print_error(FILE * fp, struct lf12_metadata *f12_meta, char **strp,
		char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (NULL != fp) {
		fclose(fp);
	}
	if (NULL != f12_meta) {
		lf12_free_metadata(f12_meta);
	}

	evasprintf(strp, fmt, ap);
	va_end(ap);

	return EXIT_FAILURE;
}
