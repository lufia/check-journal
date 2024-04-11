#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "lib.h"

char *priorities[] = {
	[0] = "emerg",
	[1] = "alert",
	[2] = "crit",
	[3] = "err",
	[4] = "warning",
	[5] = "notice",
	[6] = "info",
	[7] = "debug",
};

char *facilities[] = {
	[0] = "kern",
	[1] = "user",
	[2] = "mail",
	[3] = "daemon",
	[4] = "auth",
	[5] = "syslog",
	[6] = "lpr",
	[7] = "news",
	[8] = "uucp",
	[9] = "cron",
	[10] = "authpriv",
	[11] = "ftp",
	/* 12 = NTP subsystem */
	/* 13 = log audit */
	/* 14 = log alert */
	/* 15 = clock daemon (note 2) */
	[16] = "local0",
	[17] = "local1",
	[18] = "local2",
	[19] = "local3",
	[20] = "local4",
	[21] = "local5",
	[22] = "local6",
	[23] = "local7",
};

static int
getindex(char **a, int na, char *s)
{
	int n;
	char *p, **bp, **ep;

	p = NULL;
	n = strtol(s, &p, 10);
	if(*p == '\0'){
		if(n < 0 || n >= na){
			errno = ERANGE;
			return -1;
		}
		return n;
	}
	ep = a + na;
	for(bp = a; bp < ep; bp++)
		if(*bp != NULL && strcmp(*bp, s) == 0)
			return bp - a;

	errno = EINVAL;
	return -1;
}

int
getpriority(char *s)
{
	return getindex(priorities, nelem(priorities), s);
}

int
getfacility(char *s)
{
	return getindex(facilities, nelem(facilities), s);
}
