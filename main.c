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
	regex_t *invert;
};

/*
 * If threshold is zero or negative, check-journal runs as like grep(1).
 * Otherwise check-journal behaves as Sensu plugin.
 */
int threshold;

char *argv0;

extern int journal(char *last, FilterOpts *opts, char **cursor);
extern int match(char *s, int n, FilterOpts *opts);

static struct option options[] = {
	{"state-file", required_argument, NULL, 'f'},
	{"user", no_argument, NULL, 1},
	{"unit", required_argument, NULL, 'u'},
	{"priority", required_argument, NULL, 'p'},
	{"facility", required_argument, NULL, 2},
	{"regexp", required_argument, NULL, 'e'},
	{"ignore-case", no_argument, NULL, 'i'},
	{"invert-match", required_argument, NULL, 'v'},
	{"check", optional_argument, NULL, 3},
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
	fprintf(stderr, "\t-v --invert-match=PATTERN\n");
	fprintf(stderr, "\t   --check[=NUM]\n");
	fprintf(stderr, "\t-h --help\n");
}

int
main(int argc, char **argv)
{
	char *last, *next, *state;
	char *pat, *invpat;
	FilterOpts opts;
	int c, optind, e;
	regex_t pattern, invert;
	int rflags;
	char buf[1024];

	argv0 = argv[0];
	opts = (FilterOpts){
		.flags = SD_JOURNAL_LOCAL_ONLY|SD_JOURNAL_SYSTEM,
		.priority = -1,
		.facility = -1,
	};
	rflags = REG_EXTENDED|REG_NOSUB;
	state = NULL;
	pat = NULL;
	invpat = NULL;
	optind = 0;
	for(;;){
		c = getopt_long(argc, argv, "f:u:p:e:iv:h", options, &optind);
		if(c < 0)
			break;
		switch(c){
		default:
			usage();
			exitres(2, SENSU_UNKNOWN);
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
			if(opts.priority < 0)
				fatal(2, "invalid priority '%s': %m\n", optarg);
			break;
		case 2: /* --facility */
			opts.facility = getfacility(optarg);
			if(opts.facility < 0)
				fatal(2, "invalid facility '%s': %m\n", optarg);
			break;
		case 'e':
			pat = optarg;
			break;
		case 'i':
			rflags |= REG_ICASE;
			break;
		case 'v':
			invpat = optarg;
			break;
		case 3: /* --check */
			threshold = 1;
			if(optarg != NULL)
				threshold = atoi(optarg);
			if(threshold <= 0)
				fatal(2, "invalid number '%s'\n", optarg);
			break;
		case 'h':
			usage();
			exitres(0, SENSU_OK);
		}
	}
	if(pat != NULL){
		e = regcomp(&pattern, pat, rflags);
		if(e != 0){
			regerror(e, &pattern, buf, sizeof buf);
			fatal(2, "syntax error: %s\n", buf);
		}
		opts.pattern = &pattern;
	}
	if(invpat != NULL){
		e = regcomp(&invert, invpat, rflags);
		if(e != 0){
			regerror(e, &invert, buf, sizeof buf);
			fatal(2, "syntax error: %s\n", buf);
		}
		opts.invert = &invert;
	}

	last = next = NULL;
	if(state != NULL){
		if(readstr(state, &last) < 0)
			fatal(1, "failed to load cursor from %s: %m\n", state);
	}
	c = journal(last, &opts, &next);
	if(state != NULL && next != NULL){
		if(writestr(state, next) < 0)
			fatal(1, "failed to save cursor to %s: %m\n", state);
	}
	free(next);
	if(iscrit(c))
		exitres(0, SENSU_CRIT);
	else if(iswarn(c))
		exitres(0, SENSU_WARNING);
	return 0;
}

int
journal(char *last, FilterOpts *opts, char **cursor)
{
	sd_journal *j;
	int i, n, nmatched;
	char buf[1024], *prefix;

	if(sd_journal_open(&j, opts->flags) < 0)
		fatal(1, "failed to open journal: %m\n");
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
		if(!sd_journal_test_cursor(j, last))
			fatal(2, "invalid cursor: %m\n");
		if(sd_journal_seek_cursor(j, last) < 0)
			fatal(1, "failed to seek to the cursor: %m\n");
		// A position pointed with cursor has been read in previous operation.
		n = sd_journal_next(j);
		if(n < 0)
			fatal(1, "failed to move next: %m\n");
		if(n == 0){ // no more data
			sd_journal_close(j);
			*cursor = NULL;
			return 0;
		}
	}
	nmatched = 0;
	for(i = 0; (n=sd_journal_next(j)) > 0; i++){
		char *s;
		size_t len;

		if(sd_journal_get_data(j, "MESSAGE", (void *)&s, &len) < 0)
			fatal(1, "failed to get data: %m\n");
		if(match(s, len, opts)){
			printf("%.*s\n", len-8, s+8);
			nmatched++;
		}
	}
	if(n < 0)
		fatal(1, "failed to move next: %m\n");
	if(i == 0){ // no data
		sd_journal_close(j);
		*cursor = NULL;
		return 0;
	}

	if(sd_journal_get_cursor(j, cursor) < 0)
		fatal(1, "failed to get cursor: %m\n");
	sd_journal_close(j);
	return nmatched;
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
			fatal(1, "failed to match: %s\n", buf);
		}
	}
	if(opts->invert != NULL){
		e = regexec(opts->invert, buf, 0, NULL, 0);
		if(e == 0)
			return 0;
		if(e != REG_NOMATCH){
			regerror(e, opts->pattern, buf, sizeof buf);
			fatal(1, "failed to match: %s\n", buf);
		}
	}
	return 1;
}
