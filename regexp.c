#include <regex.h>
#include <stddef.h>
#include <memory.h>
#include "lib.h"

void
eregcomp(regex_t *r, char *s, int cflags)
{
	int e;
	char buf[1024];

	e = regcomp(r, s, cflags);
	if(e != 0){
		regerror(e, r, buf, sizeof buf);
		fatal(2, "syntax error: %s\n", buf);
	}
}

int
eregexec(regex_t *r, char *s, int rflags)
{
	int e;
	char buf[1024];

	e = regexec(r, s, 0, NULL, rflags);
	if(e != 0 && e != REG_NOMATCH){
		regerror(e, r, buf, sizeof buf);
		fatal(1, "failed to match: %s\n", buf);
	}
	return e;
}

Regexp *
newregexp(char *s)
{
	Regexp *r;

	r = emalloc(sizeof *r);
	memset(r, 0, sizeof *r);
	r->s = estrdup(s);
	return r;
}
