#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <systemd/sd-journal.h>
#include "lib.h"

typedef struct FilterOpts FilterOpts;
struct FilterOpts {
	char *unit;
	int priority;
	int facility;
};

static char *priorities[] = {
	[0] = "emerg",
	[1] = "alert",
	[2] = "crit",
	[3] = "err",
	[4] = "warning",
	[5] = "notice",
	[6] = "info",
	[7] = "debug",
};

static char *facilities[] = {
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

int
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
		if(strcmp(*bp, s) == 0)
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

extern char *journal(char *, int, FilterOpts *);

static struct option options[] = {
	{"state-file", required_argument, NULL, 'f'},
	{"user", no_argument, NULL, 1},
	{"unit", required_argument, NULL, 'u'},
	{"priority", required_argument, NULL, 'p'},
	{"facility", required_argument, NULL, 2},
	{"help", no_argument, NULL, 'h'},
	{0},
};

void
usage(void)
{
	fprintf(stderr, "usage: check-journal [options]\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "\t-f --state-file=FILE\n");
	fprintf(stderr, "\t   --user\n");
	fprintf(stderr, "\t-u --unit=UNIT\n");
	fprintf(stderr, "\t-p --priority=PRIORITY\n");
	fprintf(stderr, "\t   --facility=FACILITY\n");
	fprintf(stderr, "\t-h --help\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	char *last, *next, *state;
	FilterOpts opts;
	int flags;
	int optind;
	int c;

	state = NULL;
	optind = 0;
	memset(&opts, 0, sizeof opts);
	opts.priority = -1;
	opts.facility = -1;
	flags = SD_JOURNAL_LOCAL_ONLY|SD_JOURNAL_SYSTEM;
	for(;;){
		c = getopt_long(argc, argv, "f:u:p:h", options, &optind);
		if(c < 0)
			break;
		switch(c){
		default:
			usage();
		case 'f':
			state = optarg;
			break;
		case 1: /* --user */
			flags = SD_JOURNAL_LOCAL_ONLY|SD_JOURNAL_CURRENT_USER;
			break;
		case 'u':
			opts.unit = optarg;
			break;
		case 'p':
			opts.priority = getpriority(optarg);
			if(opts.priority < 0){
				fprintf(stderr, "invalid priority: %m\n");
				exit(2);
			}
			break;
		case 2: /* --facility */
			opts.facility = getfacility(optarg);
			if(opts.facility < 0){
				fprintf(stderr, "invalid facility: %m\n");
				exit(2);
			}
			break;
		case 'h':
			usage();
		}
	}

	last = NULL;
	if(state != NULL){
		if(readstr(state, &last) < 0){
			fprintf(stderr, "failed to load cursor from %s: %m\n", state);
			exit(1);
		}
	}

	next = journal(last, flags, &opts);

	if(state != NULL && next != NULL){
		if(writestr(state, next) < 0){
			fprintf(stderr, "failed to save cursor to %s: %m\n", state);
			exit(1);
		}
	}
	free(next);
	return 0;
}

char *
journal(char *last, int flags, FilterOpts *opts)
{
	sd_journal *j;
	char *cursor;
	int i, n;
	char buf[1024], *prefix;

	if(sd_journal_open(&j, flags) < 0){
		fprintf(stderr, "failed to open journal: %m\n");
		exit(1);
	}
	sd_journal_set_data_threshold(j, 0); // set threshold to unlimited

	if(opts->unit){
		prefix = (flags&SD_JOURNAL_CURRENT_USER) ? "USER_" : "";
		snprintf(buf, sizeof buf, "%sUNIT=%s", prefix, opts->unit);
		sd_journal_add_match(j, buf, 0);
	}
	for(i = 0; i <= opts->priority; i++){
		snprintf(buf, sizeof buf, "PRIORITY=%d", i);
		sd_journal_add_match(j, buf, 0);
	}
	if(opts->facility >= 0){
		snprintf(buf, sizeof buf, "SYSLOG_FACILITY=%d", opts->facility);
		sd_journal_add_match(j, buf, 0);
	}

	if(last != NULL){
		if(!sd_journal_test_cursor(j, last)){
			fprintf(stderr, "invalid cursor: %m\n");
			exit(2);
		}
		if(sd_journal_seek_cursor(j, last) < 0){
			fprintf(stderr, "failed to seek to the cursor: %m\n");
			exit(1);
		}
		// A position pointed with cursor has been read in previous operation.
		n = sd_journal_next(j);
		if(n < 0){
			fprintf(stderr, "failed to move next: %m\n");
			exit(1);
		}
		if(n == 0){ // no more data
			sd_journal_close(j);
			return NULL;
		}
	}
	n = 0;
	while((n=sd_journal_next(j)) > 0) {
		char *s;
		size_t len;

		if(sd_journal_get_data(j, "MESSAGE", &s, &len) < 0){
			fprintf(stderr, "failed to get data: %m\n");
			exit(1);
		}
		printf("data = '%.*s'\n", len-8, s+8);
	}
	if(n < 0){
		fprintf(stderr, "failed to move next: %m\n");
		exit(1);
	}

	if(sd_journal_get_cursor(j, &cursor) < 0){
		fprintf(stderr, "failed to get cursor: %m\n");
		exit(1);
	}
	sd_journal_close(j);
	return cursor;
}
