/* Gnome panel: multiple applet functionality
 * (C) 1998 the Free Software Foundation
 *
 * Author:  George Lebl
 */
#include <config.h>
#include <gnome.h>
#include <stdio.h>
#include <string.h>

#include "panel-include.h"

extern GArray *applets;
extern int applet_count;

/*multiple applet load queue*/
typedef struct _MultiLoadQueue MultiLoadQueue;
struct _MultiLoadQueue {
	char *path;
        CORBA_Object obj;
	GList *params; /*this is a list of parameters, each parameter
			 represents one applet to start with that
			 parameter, or an empty string to start an
			 applet without parameters, there is never a NULL*/
};
static GList *multiple_applet_load_list=NULL;

/*check if we are running an extern applet with a certain path*/
static int
is_applet_running(const char *path)
{
	int i;
	AppletInfo *info;

	for(info=(AppletInfo *)applets->data,i=0;i<applet_count;i++,info++) {
		if(info->type == APPLET_EXTERN ||
		    info->type == APPLET_EXTERN_PENDING ||
		    info->type == APPLET_EXTERN_RESERVED) {
			Extern *ext = info->data;
			if(strcmp(ext->path,path)==0)
				return TRUE;
		}
	}
	return FALSE;
}

/* remove applets which are no longer on the panel from the list of multi
   applets */
void
mulapp_remove_empty_from_list(void)
{
	GList *list;
	for(list=multiple_applet_load_list;list!=NULL;list=g_list_next(list)){
		MultiLoadQueue *mq = list->data;
		if(!is_applet_running(mq->path)) {
		        CORBA_Environment ev;
			multiple_applet_load_list =
				g_list_remove_link(multiple_applet_load_list,
						   list);
			g_list_free_1(list);
			if(mq->params) g_warning("Whoops! there were applets "
						 "to be started while the "
						 "main one disappeared, this "
						 "should never happen and "
						 "most likely indicates a bug");

			g_free(mq->path);
			CORBA_exception_init(&ev);
			CORBA_Object_release(mq->obj, &ev);
			CORBA_exception_free(&ev);
			g_free(mq);
			/*since we should restart the loop now*/
			mulapp_remove_empty_from_list();
			return;
		}
	}
}

/*is this path in the list of multi applets*/
int
mulapp_is_in_list(const char *path)
{
	GList *list;
	mulapp_remove_empty_from_list();
	for(list=multiple_applet_load_list;list!=NULL;list=g_list_next(list)){
		MultiLoadQueue *mq = list->data;
		if(strcmp(mq->path,path)==0)
			return TRUE;
	}
	return FALSE;
}

/*if the parent is already in queue, load the applet or add the param,
  into a queue*/
void
mulapp_load_or_add_to_queue(const char *path,const char *param)
{
	GList *list;
	mulapp_remove_empty_from_list();

	for(list=multiple_applet_load_list;list!=NULL;list=g_list_next(list)){
		MultiLoadQueue *mq = list->data;
		if(strcmp(mq->path,path)==0) {
			if(mq->obj)
				send_applet_start_new_applet(mq->obj, param);
			else
				mq->params = g_list_prepend(mq->params,
							    g_strdup(param));
			return;
		}
	}
}

/*add this path to the list of multi applets*/
void
mulapp_add_to_list(const char *path)
{
	MultiLoadQueue *mq;

	mq = g_new(MultiLoadQueue,1);
	mq->path = g_strdup(path);
	mq->obj = CORBA_OBJECT_NIL;
	mq->params = NULL;
	multiple_applet_load_list = g_list_prepend(multiple_applet_load_list,
				                   mq);
}

/*we know the obj so let's store that and start all the applets that have
  accumulated in the queue for this executable */
void
mulapp_add_obj_and_free_queue(const char *path, CORBA_Object obj)
{
	GList *list;
	mulapp_remove_empty_from_list();
	for(list=multiple_applet_load_list;list!=NULL;list=g_list_next(list)){
		MultiLoadQueue *mq = list->data;
		if(strcmp(mq->path,path)==0) {
			GList *li;
			CORBA_Environment ev;
			CORBA_exception_init(&ev);
			if(mq->obj
			   && !CORBA_Object_is_equivalent(mq->obj, obj, &ev))
				g_warning("What? there already was an applet "
					  "before with different objref?");
			else
			        mq->obj = CORBA_Object_duplicate(obj, &ev);
			CORBA_exception_free(&ev);
			if(!mq->params)
				return;
			li = mq->params;
			mq->params = NULL;
			while(li) {
				send_applet_start_new_applet(mq->obj,li->data);
				g_free(li->data);
				li = my_g_list_pop_first(li);
			}
			return;
		}
	}
}

