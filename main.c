#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <regex.h>
#include <systemd/sd-journal.h>
#include "lib.h"

typedef struct FilterOpts FilterOpts;
struct FilterOpts {
	int flags;
	char *unit;
	int priority;
	int facility;

	regex_t *pattern;
};

extern char *journal(char *last, FilterOpts *opts);
extern int match(char *s, int n, FilterOpts *opts);

static struct option options[] = {
	{"state-file", required_argument, NULL, 'f'},
	{"user", no_argument, NULL, 1},
	{"unit", required_argument, NULL, 'u'},
	{"priority", required_argument, NULL, 'p'},
	{"facility", required_argument, NULL, 2},
	{"regexp", required_argument, NULL, 'e'},
	{"ignore-case", required_argument, NULL, 'i'},
/*
	{"invert-match", required_argument, NULL, 'v'},
*/
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
	fprintf(stderr, "\t-e --regexp=PATTERN\n");
	fprintf(stderr, "\t-i --ignore-case\n");
	fprintf(stderr, "\t-h --help\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	char *last, *next, *state, *pat;
	FilterOpts opts;
	int c, optind, e;
	regex_t pattern;
	int rflags;
	char buf[1024];

	memset(&opts, 0, sizeof opts);
	opts.flags = SD_JOURNAL_LOCAL_ONLY|SD_JOURNAL_SYSTEM;
	opts.priority = -1;
	opts.facility = -1;
	rflags = REG_EXTENDED|REG_NOSUB;
	state = NULL;
	optind = 0;
	for(;;){
		c = getopt_long(argc, argv, "f:u:p:e:ih", options, &optind);
		if(c < 0)
			break;
		switch(c){
		default:
			usage();
		case 'f':
			state = optarg;
			break;
		case 1: /* --user */
			opts.flags = SD_JOURNAL_LOCAL_ONLY|SD_JOURNAL_CURRENT_USER;
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
		case 'e':
			pat = optarg;
			break;
		case 'i':
			rflags |= REG_ICASE;
			break;
		case 'h':
			usage();
		}
	}
	e = regcomp(&pattern, pat, rflags);
	if(e != 0){
		regerror(e, &pattern, buf, sizeof buf);
		fprintf(stderr, "syntax error: %s\n", buf);
		exit(2);
	}
	opts.pattern = &pattern;

	last = NULL;
	if(state != NULL){
		if(readstr(state, &last) < 0){
			fprintf(stderr, "failed to load cursor from %s: %m\n", state);
			exit(1);
		}
	}
	next = journal(last, &opts);
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
journal(char *last, FilterOpts *opts)
{
	sd_journal *j;
	char *cursor;
	int i, n;
	char buf[1024], *prefix;

	if(sd_journal_open(&j, opts->flags) < 0){
		fprintf(stderr, "failed to open journal: %m\n");
		exit(1);
	}
	sd_journal_set_data_threshold(j, 0); // set threshold to unlimited

	if(opts->unit){
		prefix = (opts->flags&SD_JOURNAL_CURRENT_USER) ? "USER_" : "";
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

		if(sd_journal_get_data(j, "MESSAGE", (void *)&s, &len) < 0){
			fprintf(stderr, "failed to get data: %m\n");
			exit(1);
		}
		if(match(s, len, opts))
			printf("%.*s\n", len-8, s+8);
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

int
match(char *s, int n, FilterOpts *opts)
{
	char buf[n+1];
	int e;

	memmove(buf, s, n);
	buf[n] = '\0';
	if(opts->pattern != NULL){
		e = regexec(opts->pattern, buf, 0, NULL, 0);
		if(e == REG_NOMATCH)
			return 0;
		if(e != 0){
			regerror(e, opts->pattern, buf, sizeof buf);
			fprintf(stderr, "failed to match: %s\n", buf);
			exit(1);
		}
	}
	return 1;
}
