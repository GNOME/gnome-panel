#ifndef PANEL_CMDS_H
#define PANEL_CMDS_H

BEGIN_GNOME_DECLS


#define PANEL_UNKNOWN_APPLET_POSITION -1


typedef enum {
	APPLET_HAS_PROPERTIES = 1L << 0
} AppletFlags;

typedef enum {
	PANEL_CMD_QUIT,
	PANEL_CMD_GET_APPLET_TYPES,
	PANEL_CMD_GET_APPLET_CMD_FUNC,
	PANEL_CMD_CREATE_APPLET,
	PANEL_CMD_REGISTER_TOY,
	PANEL_CMD_SET_TOOLTIP,
	PANEL_CMD_CHANGE_SIZE_NOTIFY,
	PANEL_CMD_PROPERTIES
} PanelCommandType;

typedef struct {
	PanelCommandType cmd;

	union {
		/* Get applet command function parameters */
		struct {
			char *id;
		} get_applet_cmd_func;

		/* Create applet parameters */
		struct {
			char *id;
			char *params;
			int   pos;
		} create_applet;
		
		/* Register toy parameters */
		struct {
			GtkWidget *applet;
			char      *id;
			int        pos;
			long       flags;
		} register_toy;

		/* Tooltip */
		struct {
			GtkWidget *applet;
			char *tooltip;
		} set_tooltip;

		/* Change size notify parameters */
		struct {
			GtkWidget *applet;
		} change_size_notify;
	} params;
} PanelCommand;

typedef gpointer (*PanelCmdFunc) (PanelCommand *cmd);


END_GNOME_DECLS

#endif
