/* Gnome panel: multiple applet functionality
 * (C) 1998 the Free Software Foundation
 *
 * Author:  George Lebl
 */
#ifndef MULAPP_H
#define MULAPP_H

BEGIN_GNOME_DECLS

/*is this path in the list of multi applets*/
gint mulapp_is_in_list(const gchar *path);

/*if the parent is already in queue, load the applet or add the param,
  into a queue*/
void mulapp_load_or_add_to_queue(const gchar *path,const gchar *param);

/*add this path to the list of multi applets*/
void mulapp_add_to_list(const gchar *path);

/*we know the ior so let's store that and start all the applets that have
  accumulated in the queue for this executable*/
void mulapp_add_ior_and_free_queue(const gchar *path, const gchar *ior);

/* remove applets which are no longer on the panel from the list of multi
   applets */
void mulapp_remove_empty_from_list(void);

END_GNOME_DECLS

#endif
