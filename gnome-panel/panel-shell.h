#ifndef PANEL_SHELL_H
#define PANEL_SHELL_H

#include "GNOME_Panel.h"

#include <bonobo/bonobo-object.h>

#define PANEL_SHELL_TYPE        (panel_shell_get_type ())
#define PANEL_SHELL(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PANEL_SHELL_TYPE, PanelShell))
#define PANEL_SHELL_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST    ((k), PANEL_SHELL_TYPE, PanelShellClass))
#define PANEL_IS_SHELL(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PANEL_SHELL_TYPE))
#define PANEL_IS_SHELL_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE    ((k), PANEL_SHELL_TYPE))

typedef struct _PanelShellPrivate PanelShellPrivate;

typedef struct {
	BonoboObject base;

	PanelShellPrivate *priv;
} PanelShell;

typedef struct {
	BonoboObjectClass parent_class;

	POA_GNOME_PanelShell__epv epv;
} PanelShellClass;

GType     panel_shell_get_type (void) G_GNUC_CONST;

gboolean  panel_shell_register   (void);
void      panel_shell_unregister (void);

#endif /* PANEL_SHELL_H */
