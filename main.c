#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <regex.h>
#include <systemd/sd-journal.h>
#include "lib.h"

typedef struct FilterOpts FilterOpts;
struct FilterOpts {
	int flags;
	char *unit;
	int priority;
	List *facilities;

	List *patterns;
	List *inverts;
};

/*
 * If threshold is zero or negative, check-journal runs as like grep(1).
 * Otherwise check-journal behaves as Sensu plugin.
 */
int threshold;

char *argv0;
int quiet;

static int journal(char *last, FilterOpts *opts, char **cursor);
static void compile(List *p, int cflags);
static char *getdata(sd_journal *j, char *name);
static int match(char *s, FilterOpts *opts);
static char *errstr(int e);

static struct option options[] = {
	{"state-file", required_argument, NULL, 'f'},
	{"user", no_argument, NULL, 1},
	{"unit", required_argument, NULL, 'u'},
	{"priority", required_argument, NULL, 'p'},
	{"facility", required_argument, NULL, 2},
	{"regexp", required_argument, NULL, 'e'},
	{"ignore-case", no_argument, NULL, 'i'},
	{"invert-match", required_argument, NULL, 'v'},
	{"quiet", no_argument, NULL, 'q'},
	{"check", optional_argument, NULL, 3},
	{"help", no_argument, NULL, 'h'},
	{0},
};

static void
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
	fprintf(stderr, "\t-q --quiet\n");
	fprintf(stderr, "\t   --check[=NUM]\n");
	fprintf(stderr, "\t-h --help\n");
}

int
main(int argc, char **argv)
{
	char *last, *next, *state;
	FilterOpts opts;
	Regexp *r;
	int c, optind, facility, cflags;

	argv0 = argv[0];
	state = NULL;
	opts = (FilterOpts){
		.flags = SD_JOURNAL_LOCAL_ONLY|SD_JOURNAL_SYSTEM,
		.priority = -1,
	};
	optind = 0;
	cflags = REG_EXTENDED|REG_NOSUB;
	for(;;){
		c = getopt_long(argc, argv, "f:u:p:e:iv:qh", options, &optind);
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
			facility = getfacility(optarg);
			if(facility < 0)
				fatal(2, "invalid facility '%s': %m\n", optarg);
			opts.facilities = append(opts.facilities, newlist((void *)facility));
			break;
		case 'e':
			r = newregexp(optarg);
			opts.patterns = append(opts.patterns, newlist(r));
			break;
		case 'i':
			cflags |= REG_ICASE;
			break;
		case 'v':
			r = newregexp(optarg);
			opts.inverts = append(opts.inverts, newlist(r));
			break;
		case 'q':
			quiet = 1;
			break;
		case 3: /* --check */
			threshold = 1;
			if(optarg)
				threshold = atoi(optarg);
			if(threshold <= 0)
				fatal(2, "invalid number '%s'\n", optarg);
			break;
		case 'h':
			usage();
			exitres(0, SENSU_OK);
		}
	}
	compile(opts.patterns, cflags);
	compile(opts.inverts, cflags);

	last = next = NULL;
	if(state){
		if(readstr(state, &last) < 0)
			fatal(1, "failed to load cursor from %s: %m\n", state);
	}
	c = journal(last, &opts, &next);
	if(state && next){
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

static void
compile(List *p, int cflags)
{
	Regexp *r;

	for(; p; p = p->next){
		r = (Regexp *)p->aux;
		eregcomp(&r->r, r->s, cflags);
	}
}

static int
journal(char *last, FilterOpts *opts, char **cursor)
{
	sd_journal *j;
	int i, e, nmatched;
	char buf[1024];
	List *p;

	if((e=sd_journal_open(&j, opts->flags)) < 0)
		fatal(1, "failed to open journal: %s\n", errstr(-e));
	sd_journal_set_data_threshold(j, 0); // set threshold to unlimited

	if(opts->unit){
		/* generates `(_SYSTEMD_UNIT=<u> OR UNIT=<u>) AND ...` matches */
		snprintf(buf, sizeof buf, "_SYSTEMD_UNIT=%s", opts->unit);
		sd_journal_add_match(j, buf, 0);
		sd_journal_add_disjunction(j);
		snprintf(buf, sizeof buf, "UNIT=%s", opts->unit);
		sd_journal_add_match(j, buf, 0);
		sd_journal_add_conjunction(j);
	}
	/* TODO(lufia): add user-unit */
	for(i = 0; i <= opts->priority; i++){
		snprintf(buf, sizeof buf, "PRIORITY=%d", i);
		sd_journal_add_match(j, buf, 0);
	}
	for(p = opts->facilities; p; p = p->next){
		snprintf(buf, sizeof buf, "SYSLOG_FACILITY=%d", (int)p->aux);
		sd_journal_add_match(j, buf, 0);
	}

	if(last){
		if((e=sd_journal_seek_cursor(j, last)) < 0)
			fatal(1, "failed to seek to the cursor: %s\n", errstr(-e));
		// A position pointed with cursor has been read in previous operation.
		e = sd_journal_next(j);
		if(e < 0)
			fatal(1, "failed to move next: %s\n", errstr(-e));
		if(e == 0){ // no more data
			sd_journal_close(j);
			*cursor = NULL;
			return 0;
		}
		if((e=sd_journal_test_cursor(j, last)) < 0)
			fatal(1, "failed to test the cursor: %s\n", errstr(-e));
	}
	nmatched = 0;
	for(i = 0; (e=sd_journal_next(j)) > 0; i++){
		char *s, *u;

		s = getdata(j, "MESSAGE");
		if(match(s, opts)){
			if(!quiet){
				u = getdata(j, "UNIT");
				if(u == NULL)
					u = getdata(j, "_SYSTEMD_UNIT");
				printf("%s: %s\n", u ? u : "(null)", s);
				free(u);
			}
			nmatched++;
		}
		free(s);
	}
	if(e < 0)
		fatal(1, "failed to move next: %m\n");
	if(i == 0){ // no data
		sd_journal_close(j);
		*cursor = NULL;
		return 0;
	}

	if((e=sd_journal_get_cursor(j, cursor)) < 0)
		fatal(1, "failed to get cursor: %s\n", errstr(-e));
	sd_journal_close(j);
	return nmatched;
}

static char *
getdata(sd_journal *j, char *name)
{
	char *s, *t;
	size_t n, len;
	int e;

	e = sd_journal_get_data(j, name, (void *)&s, &n);
	if(e == -ENOENT)
		return NULL;
	if(e < 0)
		fatal(1, "failed to get %s: %s\n", name, errstr(-e));
	len = strlen(name) + 1; /* name + '=' */
	t = emalloc(n-len+1);
	memmove(t, s+len, n-len);
	t[n-len] = '\0';
	return t;
}

static int
match(char *s, FilterOpts *opts)
{
	List *p;
	Regexp *r;

	for(p = opts->patterns; p; p = p->next){
		r = (Regexp *)p->aux;
		if(eregexec(&r->r, s, 0) == REG_NOMATCH)
			return 0;
	}
	if(opts->inverts == NULL)
		return 1;
	for(p = opts->inverts; p; p = p->next){
		r = (Regexp *)p->aux;
		if(eregexec(&r->r, s, 0) == REG_NOMATCH)
			return 1;
	}
	return 0;
}

/* errstr is not thread-safe */
static char *
errstr(int e)
{
	/* see sd_journal_get_data(3) */
	static char *errors[] = {
		[EINVAL] = "One of the required parameters is NULL or invalid",
		[ECHILD] = "The journal object was created in a different process, library or module instance",
		[EADDRNOTAVAIL] = "The read pointer is not positioned at a valid entry",
		[ENOENT] = "The current entry does not include the specified field",
		[ENOMEM] = "Memory allocation failed",
		[ENOBUFS] = "A compressed entry is too large",
		[E2BIG] = "The data field is too large for this computer architecture",
		[EPROTONOSUPPORT] = "The journal is compressed with an unsupported method or the journal uses an unsupported feature",
		[EBADMSG] = "The journal is corrupted",
		[EIO] = "An I/O error was reported by the kernel",
	};

	if(e <= 0)
		return NULL;
	if(e >= nelem(errors) || errors[e] == NULL)
		return strerror(e);
	return errors[e];
}
