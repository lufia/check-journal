#include <stdlib.h>
#include <memory.h>
#include "lib.h"

List *
newlist(void *v)
{
	List *p;

	p = emalloc(sizeof *p);
	memset(p, 0, sizeof *p);
	p->aux = v;
	return p;
}

List *
append(List *top, List *n)
{
	List z, *p;

	memset(&z, 0, sizeof z);
	z.next = top;
	for(p = &z; p->next != NULL; p = p->next)
		;
	p->next = n;
	return z.next;
}
