/*
 * panel-applet-frame.h:
 *
 * Copyright (C) 2001 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_APPLET_FRAME_H__
#define __PANEL_APPLET_FRAME_H__

#include <gtk/gtkeventbox.h>

#include "panel-widget.h"
#include "applet.h"

G_BEGIN_DECLS

#define PANEL_TYPE_APPLET_FRAME         (panel_applet_frame_get_type ())
#define PANEL_APPLET_FRAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_TYPE_APPLET_FRAME, PanelAppletFrame))
#define PANEL_APPLET_FRAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PANEL_TYPE_APPLET_FRAME, PanelAppletFrameClass))
#define PANEL_IS_APPLET_FRAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_TYPE_APPLET_FRAME))
#define PANEL_IS_APPLET_FRAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PANEL_TYPE_APPLET_FRAME))
#define PANEL_APPLET_FRAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PANEL_TYPE_APPLET_FRAME, PanelAppletFrameClass))

typedef struct _PanelAppletFrame        PanelAppletFrame;
typedef struct _PanelAppletFrameClass   PanelAppletFrameClass;
typedef struct _PanelAppletFramePrivate PanelAppletFramePrivate;

struct _PanelAppletFrame{
        GtkEventBox               event_box;

        PanelAppletFramePrivate  *priv;
};

struct _PanelAppletFrameClass {
        GtkEventBoxClass          event_box_class;

	void (* move_focus_out_of_applet) (PanelAppletFrame *frame,
					   GtkDirectionType  direction);
};

GType      panel_applet_frame_get_type  (void) G_GNUC_CONST;

GtkWidget *panel_applet_frame_new       (const char *iid,
					 const char *gconf_key);

GtkWidget *panel_applet_frame_construct (PanelAppletFrame *frame,
					 const char      *iid,
					 const char      *gconf_key);

void       panel_applet_frame_set_info  (PanelAppletFrame *frame,
					 AppletInfo       *info);
					
void       panel_applet_frame_load      (const gchar *iid,
					 PanelWidget *panel,
					 gint         pos,
					 const char  *gconf_key);

void       panel_applet_frame_load_applets  (void);


void       panel_applet_frame_change_orient (PanelAppletFrame *frame,
					     PanelOrient       orient);

void       panel_applet_frame_change_size   (PanelAppletFrame *frame,
					     PanelSize         size);

void       panel_applet_frame_change_background_pixmap (PanelAppletFrame *frame);

void       panel_applet_frame_change_background_color  (PanelAppletFrame *frame,
							guint16           red,
							guint16           green,
							guint16           blue);

void       panel_applet_frame_clear_background         (PanelAppletFrame *frame);

void       panel_applet_frame_get_expand_flags         (PanelAppletFrame *frame,
							gboolean         *expand_major,
							gboolean         *expand_minor);

void       panel_applet_frame_save_to_gconf   (PanelAppletFrame *frame,
					       const char       *gconf_key);

void       panel_applet_frame_load_from_gconf (PanelWidget *panel_widget,
					       gint         position,
					       const char  *gconf_key,
					       gboolean     use_default);

G_END_DECLS

#endif /* __PANEL_APPLET_FRAME_H__ */
