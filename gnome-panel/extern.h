#ifndef EXTERN_H
#define EXTERN_H

#include <glib.h>
#include "panel.h"
#include "panel-widget.h"
#include "GNOME_Panel.h"

G_BEGIN_DECLS

typedef struct Extern_struct *Extern;

typedef enum {
	EXTERN_SUCCESS,
	EXTERN_FAILURE,
	EXTERN_ALREADY_ACTIVE
} ExternResult;

ExternResult extern_init                  (void);

GNOME_Applet extern_get_applet            (Extern ext);

void         extern_set_config_string     (Extern  ext,
					   gchar  *config_string);

gboolean     extern_handle_back_change    (Extern       ext,
					   PanelWidget *panel);

gboolean     extern_handle_freeze_changes (Extern ext);

gboolean     extern_handle_thaw_changes   (Extern ext);

gboolean     extern_handle_change_orient  (Extern ext,
					   int    orient);

gboolean     extern_handle_change_size    (Extern ext,
					   int    size);

gboolean     extern_handle_do_callback    (Extern  ext,
					   char   *name);

gboolean     extern_handle_set_tooltips_state
					  (Extern   ext,
					   gboolean enabled);

void	     extern_before_remove         (Extern ext);

void         extern_load_applet           (const char  *iid,
					   const char  *cfgpath,
					   PanelWidget *panel,
					   int          pos,
					   gboolean     exactpos,
					   gboolean     queue);

void	     extern_load_queued           (void);

void         extern_shutdown              (void);

/* to be called when we want to send a draw signal to an applet */
void	     extern_send_draw             (Extern ext);

void         extern_save_applet           (AppletInfo *info,
					   gboolean    ret);

void         extern_save_last_position    (Extern   ext,
					  gboolean sync);

G_END_DECLS

#endif
