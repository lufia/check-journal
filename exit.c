#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "lib.h"

void
exitres(int status, int result)
{
	if(threshold > 0)
		exit(result);
	exit(status);
}

void
fatal(int status, char *fmt, ...)
{
	FILE *fout;
	va_list arg;

	fout = stderr;
	if(threshold > 0)
		fout = stdout;
	va_start(arg, fmt);
	vfprintf(fout, fmt, arg);
	va_end(arg);
	exitres(status, SENSU_UNKNOWN);
}
