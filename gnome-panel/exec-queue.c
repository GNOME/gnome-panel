/* Gnome panel: Exec queue
 * (C) 1997 the Free Software Foundation
 *
 * Authors: Federico Mena
 *          Miguel de Icaza
 *          George Lebl
 */

#include <config.h>
#include <string.h>
#include <gnome.h>

#include "panel-include.h"

GArray *applets;
int applet_count;

/*execution queue stuff, execute only one applet in a row, thereby getting
  rid of some problems with applet*/
int current_exec = -1;
guint cur_timeout=0;
typedef struct _ExecQueue ExecQueue;
struct _ExecQueue {
	int applet_id;
	char *path;
	char *param;
};
GList *exec_queue=NULL;

static int really_exec_prog(int applet_id, char *path, char *param);
static void exec_queue_start_next(void);

static int
exec_queue_timeout(gpointer data)
{
	g_warning("TIMED OUT waiting to applet ID: %d!",current_exec);
	cur_timeout = -1;
	exec_queue_start_next();
	return FALSE;
}

static int
really_exec_prog(int applet_id, char *path, char *param)
{
	/*check if this is an applet which is a multi applet and
	  has something already loaded*/
	if(mulapp_is_in_list(path)) {
		printf("multi applet running, contacting [%s]\n",path);
		mulapp_load_or_add_to_queue(path,param);
		return TRUE;
	}  else {
		int pid;

		pid = fork();
		if(pid < 0)
			g_error("Can't fork!");
		if(pid == 0) {
			if(strlen(param)>0)
				execl(path,path,param,NULL);
			else
				execl(path,path,NULL);
			g_error("Can't execl!");
		}

		printf("started applet, pid: %d\n",pid);
		
		current_exec = applet_id;

		/*wait 100 seconds before timing out*/
		cur_timeout = gtk_timeout_add(100*1000,exec_queue_timeout,NULL);

		return FALSE;
	}
}

/*start the next item in the exec queue*/
static void
exec_queue_start_next(void)
{
	ExecQueue *eq;
	int ret;

	current_exec = -1;
	if(cur_timeout>0)
		gtk_timeout_remove(cur_timeout);
	cur_timeout=0;

	do {
		if(!exec_queue)
			return;

		eq = exec_queue->data;

		ret = really_exec_prog(eq->applet_id, eq->path, eq->param);
		g_free(eq->path);
		if(eq->param) g_free(eq->param);
		g_free(eq);

		exec_queue = my_g_list_pop_first(exec_queue);
	/*repeat while we are doing applets that do not require a wait
	  (second invocations of multi applets)*/
	} while(ret);
}

/* this applet has finished loading, if it was the one we were waiting
   on, start the next applet */
void
exec_queue_done(int applet_id)
{
	if(applet_id>-1 && applet_id==current_exec)
		exec_queue_start_next();
}

void
exec_prog(int applet_id, char *path, char *param)
{
	if(current_exec==-1) {
		really_exec_prog(applet_id,path,param);
	} else {
		ExecQueue *eq = g_new(ExecQueue,1);
		eq->applet_id = applet_id;
		eq->path = g_strdup(path);
		if(param)
			eq->param = g_strdup(param);
		else
			eq->param = NULL;
		exec_queue = g_list_append(exec_queue,eq);
	}
}
