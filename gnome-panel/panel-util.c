#include <glib.h>
#include <fcntl.h>

#include "panel-util.h"

/* this function might be a slight overkill, but it should work
   perfect, hopefully it should be 100% buffer overrun safe too*/
char *
get_full_path(char *argv0)
{
	char buf[PATH_MAX+2];
	int cmdsize=100;
	char *cmdbuf;
	int i;
	int fd[2];

	if(!argv0)
		return NULL;

	if(*argv0 == '/')
		return g_strdup(argv0);


	if(strchr(argv0,'/')) {
		char *curpath = getcwd(NULL,0);
		char *outbuf;

		if(!curpath)
			return NULL;

		outbuf = g_copy_strings(curpath,"/",argv0,NULL);
		free(curpath);

		realpath(outbuf,buf);
	
		return g_strdup(buf);
	}

	if(pipe(fd) == -1)
		return NULL;

	/*dynamically reallocates cmdbuf until the command fits*/
	for(;;) {
		cmdbuf = (char *)g_malloc(cmdsize);
		if(g_snprintf(cmdbuf, cmdsize, "sh -c 'which %s > /dev/fd/%d'",
			      argv0,fd[1])>-1)
			break;
	
		g_free(cmdbuf);
		cmdsize*=2;
	}
		
	system(cmdbuf);
	g_free(cmdbuf);

	i=read(fd[0],buf,PATH_MAX+1);
	close(fd[0]);
	close(fd[1]);
	if(i <= 0)
		return NULL;

	buf[i]='\0';
	if(buf[i-1]=='\n')
		buf[i-1]='\0';

	if(buf[0]=='\0')
		return NULL;
	
	return g_strdup(buf);
}
