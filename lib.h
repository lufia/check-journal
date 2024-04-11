#define nelem(a) (sizeof(a) / sizeof(a)[0])

extern int readstr(char *name, char **p);
extern int writestr(char *name, char *p);
