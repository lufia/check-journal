#define nelem(a) (sizeof(a) / sizeof(a)[0])

enum {
	SENSU_OK = 0,
	SENSU_WARNING = 1,
	SENSU_CRIT = 2,
	SENSU_UNKNOWN = 3,
};

extern int threshold;
extern char *argv0;

#define	iscrit(n)	(threshold > 0 && (n) >= threshold)
#define	iswarn(n)	(threshold > 0 && (n) > 0 && (n) < threshold)

extern void exitres(int status, int result);
extern void fatal(int status, char *fmt, ...);

extern int readstr(char *name, char **p);
extern int writestr(char *name, char *p);

extern char *priorities[];
extern char *facilities[];

extern int getpriority(char *s);
extern int getfacility(char *s);
