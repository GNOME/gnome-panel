#ifndef EXTERN_H
#define EXTERN_H

#include <glib.h>
#include "panel.h"
#include "panel-widget.h"
#include "gnome-panel.h"

#include <bonobo-activation/bonobo-activation.h>

G_BEGIN_DECLS

typedef struct Extern_struct *Extern;

Extern  extern_ref		(Extern ext);
void	extern_unref		(Extern ext);
void	extern_clean		(Extern ext);

GNOME_Applet 
        extern_get_applet       (Extern ext);

void    extern_set_info         (Extern      ext,
				 AppletInfo *info);
void    extern_set_config_string
				(Extern  ext,
				 gchar  *config_string);

PanelOrientation 
        extern_get_orient       (Extern           ext);
void    extern_set_orient       (Extern           ext,
				 PanelOrientation orient);

void	extern_before_remove	(Extern ext);

void	load_extern_applet	(const char  *goad_id,
				 const char  *cfgpath,
				 PanelWidget *panel,
				 int          pos,
				 gboolean     exactpos,
				 gboolean     queue);

void	load_queued_externs	(void);

void	panel_corba_clean_up	(void);

gint	panel_corba_gtk_init	(CORBA_ORB panel_orb);

/* to be called when we want to send a draw signal to an applet */
void	extern_send_draw 	(Extern ext);

void	save_applet		(AppletInfo *info,
				 gboolean    ret);

void	extern_save_last_position 
				(Extern   ext,
				 gboolean sync);

G_END_DECLS

#endif
