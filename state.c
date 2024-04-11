#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int
readstr(char *name, char **p)
{
	struct stat st;
	int fd;

	*p = NULL;
	fd = open(name, O_RDONLY);
	if(fd < 0){
		if(errno == ENOENT)
			return 0;
		return -1;
	}
	if(fstat(fd, &st) < 0)
		goto fail;
	*p = malloc(st.st_size+1);
	if(*p == NULL)
		goto fail;
	if(read(fd, *p, st.st_size) < 0)
		goto fail;
	*(*p + st.st_size) = '\0';
	close(fd);
	return 0;

fail:
	close(fd);
	free(*p);
}

int
writestr(char *name, char *p)
{
	int fd;

	fd = creat(name, 0644);
	if(fd < 0)
		return -1;
	write(fd, p, strlen(p));
	if(fsync(fd) < 0){
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}
