/* Gnome panel: panel widget
 * (C) 1997-1998 the Free Software Foundation
 *
 * Authors:  George Lebl
 */
/* This widget, although slightly written as a general purpose widget, it
   has MANY interdependencies, which makes it almost impossible to use in
   anything else but the panel, what it would need is some serious
   cleaning up*/
#ifndef PANEL_WIDGET_H
#define PANEL_WIDGET_H

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "panel-types.h"
#include "panel-toplevel.h"

G_BEGIN_DECLS

#define PANEL_TYPE_WIDGET          	(panel_widget_get_type ())
#define PANEL_WIDGET(object)          	(G_TYPE_CHECK_INSTANCE_CAST ((object), PANEL_TYPE_WIDGET, PanelWidget))
#define PANEL_WIDGET_CLASS(klass)  	(G_TYPE_CHECK_CLASS_CAST ((klass), PANEL_TYPE_WIDGET, PanelWidgetClass))
#define PANEL_IS_WIDGET(object)       	(G_TYPE_CHECK_INSTANCE_TYPE ((object), PANEL_TYPE_WIDGET)) 

#define PANEL_APPLET_DATA "panel_applet_data"

#ifndef TYPEDEF_PANEL_WIDGET
typedef struct _PanelWidget		PanelWidget;
#define TYPEDEF_PANEL_WIDGET
#endif /* TYPEDEF_PANEL_WIDGET */

typedef struct _PanelWidgetClass	PanelWidgetClass;

typedef struct _AppletData		AppletData;

struct _AppletData
{
	GtkWidget *	applet;

	PanelObjectPackType pack_type;
	int		    pack_index;

	int             position;
	int             size;

	guint           expand_major : 1;
	guint           expand_minor : 1;
};

struct _PanelWidget
{
	GtkFixed        fixed;

	GList          *applet_list;

	int             size;
	GtkOrientation  orient;
	int             sz;

	guint           icon_resize_id;

	AppletData     *currently_dragged_applet;
	guint           dragged_state;

	GtkWidget      *drop_widget;     /* widget that the panel checks for
	                                  * the cursor on drops usually the
	                                  * panel widget itself
	                                  */
	
	PanelToplevel  *toplevel;

	guint           packed : 1;
};

struct _PanelWidgetClass
{
	GtkFixedClass parent_class;

	void (* applet_move) (PanelWidget *panel,
			      GtkWidget *applet);
	void (* applet_added) (PanelWidget *panel,
			       GtkWidget *applet);
	void (* applet_removed) (PanelWidget *panel,
				 GtkWidget *applet);
	void (* push_move) (PanelWidget		*panel,
                            GtkDirectionType	 dir);
	void (* switch_move) (PanelWidget	*panel,
                              GtkDirectionType	 dir);
	void (* tab_move) (PanelWidget	*panel,
                           gboolean	 next);
	void (* end_move) (PanelWidget	*panel);
};

GType		panel_widget_get_type		(void) G_GNUC_CONST;

GtkWidget *	panel_widget_new		(PanelToplevel  *toplevel,
						 gboolean        packed,
						 GtkOrientation  orient,
						 int             sz);
/* add an applet to the panel; if use_pack_index is FALSE, pack_index is ignored
 * and the applet is appended at the end of the pack list for pack_style */
void		panel_widget_add		(PanelWidget         *panel,
						 GtkWidget           *applet,
						 PanelObjectPackType  pack_style,
						 int                  pack_index,
						 gboolean             use_pack_index);

/*move applet to a different panel*/
int		panel_widget_reparent		(PanelWidget         *old_panel,
						 PanelWidget         *new_panel,
						 GtkWidget           *applet,
						 PanelObjectPackType  pack_type,
						 int                  pack_index);

/*drag*/
gboolean        panel_applet_is_in_drag         (void);
void		panel_widget_applet_drag_start	(PanelWidget *panel,
						 GtkWidget   *applet,
						 guint32      time_);
void		panel_widget_applet_drag_end	(PanelWidget *panel);

void            panel_widget_set_packed         (PanelWidget    *panel_widget,
						 gboolean        packed);
void            panel_widget_set_orientation    (PanelWidget    *panel_widget,
						 GtkOrientation  orientation);
void            panel_widget_set_size           (PanelWidget    *panel_widget,
						 int             size);

/*get pos of the cursor location in panel coordinates*/
int		panel_widget_get_cursorloc	(PanelWidget *panel);
/* get pack type & index for insertion at the cursor location in panel */
void            panel_widget_get_insert_at_cursor (PanelWidget         *widget,
						   PanelObjectPackType *pack_type,
						   int                 *pack_index);
/* get index for insertion with pack type */
int                 panel_widget_get_new_pack_index   (PanelWidget          *panel,
						       PanelObjectPackType   pack_type);

/*needed for other panel types*/
gboolean	panel_widget_is_cursor		(PanelWidget *panel,
						 int overlap);
/* set the focus on the panel */
void            panel_widget_focus              (PanelWidget *panel);

PanelOrientation panel_widget_get_applet_orientation (PanelWidget *panel);

void     panel_widget_set_applet_expandable       (PanelWidget *panel,
						   GtkWidget   *applet,
						   gboolean     major,
						   gboolean     minor);

GSList  *panel_widget_get_panels (void);

guint panel_widget_get_icon_size (PanelWidget *self);

G_END_DECLS

#endif /* PANEL_WIDGET_H */
