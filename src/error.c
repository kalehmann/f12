#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

int esprintf(char **strp, const char *fmt, ...)
{
	char *tmp = *strp;
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vasprintf(strp, fmt, ap);
	if (tmp) {
		free(tmp);
	}
	va_end(ap);

	return ret;
}
