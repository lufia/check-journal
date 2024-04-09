#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <systemd/sd-journal.h>

typedef struct FilterOpts FilterOpts;
struct FilterOpts {
	char *unit;
};

extern char *journal(char *, int, FilterOpts *);

static struct option options[] = {
	{"state-file", required_argument, NULL, 'f'},
	{"user", no_argument, NULL, 1},
	{"unit", required_argument, NULL, 'u'},
	{"help", no_argument, NULL, 'h'},
	{0},
};

void
usage(void)
{
	fprintf(stderr, "usage: check-journal [options]\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "	-f --state-file=FILE\n");
	fprintf(stderr, "	   --user\n");
	fprintf(stderr, "	-u --unit=UNIT\n");
	fprintf(stderr, "	-h --help\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	char *last, *next, *state;
	FilterOpts opts;
	int flags;
	int optind;
	int fd, c;
	size_t nbuf;
	char buf[1024];

	state = NULL;
	optind = 0;
	memset(&opts, 0, sizeof opts);
	flags = SD_JOURNAL_LOCAL_ONLY|SD_JOURNAL_SYSTEM;
	for(;;){
		c = getopt_long(argc, argv, "f:u:h", options, &optind);
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
		case 'h':
			usage();
		}
	}

	last = NULL;
	if(state != NULL){
		fd = open(state, O_RDONLY);
		if(fd < 0){
			if(errno == ENOENT)
				goto run;
			fprintf(stderr, "failed to open '%s': %m\n", state);
			exit(1);
		}
		if((nbuf=read(fd, buf, sizeof buf)) < 0){
			fprintf(stderr, "failed to read '%s': %m\n", state);
			exit(1);
		}
		close(fd);
		buf[nbuf] = '\0';
		last = buf;
	}

run:
	next = journal(last, flags, &opts);

	if(state != NULL && next != NULL){
		fd = creat(state, 0644);
		if(fd < 0){
			fprintf(stderr, "failed to create '%s': %m\n", state);
			exit(1);
		}
		write(fd, next, strlen(next));
		if(fsync(fd) < 0){
			fprintf(stderr, "failed to sync to '%s': %m\n", state);
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
	int n;
	char buf[1024];

	if(sd_journal_open(&j, flags) < 0){
		fprintf(stderr, "failed to open journal: %m\n");
		exit(1);
	}
	sd_journal_set_data_threshold(j, 0); // set threshold to unlimited
	if(opts->unit){
		snprintf(buf, sizeof buf, "UNIT=%s", opts->unit);
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

		// journalctl --output=json-pretty するとみえる
		// _SYSTEMD_UNIT あたりはオプションで与えられると便利
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