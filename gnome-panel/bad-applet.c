#include <applet-widget.h>

static int mystage = 0, assign_stage = 0;

#define BOMB() bomb(NULL, assign_stage++)

static void bomb(GtkWidget *w, int n)
{
  if(mystage == n)
    g_error("Exiting at stage %d", n);
}

static void bomb_change_orient(GtkWidget *w, GNOME_Panel_OrientType orient, gpointer dat)
{
  bomb(NULL, GPOINTER_TO_INT(dat));
}

static void bomb_quit(GtkWidget *w, int n)
{
  applet_widget_panel_quit();
  bomb(NULL, n);
}

static void bomb_back_change (AppletWidget *applet,
			      GNOME_Panel_BackType type,
			      char *pixmap,
			      GdkColor *color,
			      gpointer dat)
{
  bomb(NULL, GPOINTER_TO_INT(dat));
}

static void bomb_tooltip_state (AppletWidget *applet,
				int enabled,
				gpointer dat)
{
  bomb(NULL, GPOINTER_TO_INT(dat));
}

static int bomb_save_session (AppletWidget *applet,
			      char *cfgpath,
			      char *globcfgpath,
			      gpointer dat)
{
  bomb(NULL, GPOINTER_TO_INT(dat));

  return 0;
}

#define TRY_ABORT_REMOVE() \
  if(assign_stage++ == mystage) \
    applet_widget_abort_load(APPLET_WIDGET(w)); \
  if(assign_stage++ == mystage) \
    applet_widget_remove(APPLET_WIDGET(w))

int main(int argc, char *argv[])
{
  GtkWidget *w, *l;
  poptContext ctx;
  char *mystage_str;

  applet_widget_init("bad-applet", "0.0", argc, argv, NULL, 0, &ctx);

  mystage_str = poptGetArg(ctx);
  if(mystage_str) mystage = atoi(mystage_str);
  poptFreeContext(ctx);

  BOMB();

  w = applet_widget_new("bad-applet");
  BOMB();

  TRY_ABORT_REMOVE();


  gtk_signal_connect(GTK_OBJECT(w), "change_orient", GTK_SIGNAL_FUNC(bomb_change_orient),
		     GUINT_TO_POINTER(assign_stage++));
  gtk_signal_connect(GTK_OBJECT(w), "back_change", GTK_SIGNAL_FUNC(bomb_back_change),
		     GUINT_TO_POINTER(assign_stage++));
  gtk_signal_connect(GTK_OBJECT(w), "tooltip_state", GTK_SIGNAL_FUNC(bomb_tooltip_state),
		     GUINT_TO_POINTER(assign_stage++));
  gtk_signal_connect(GTK_OBJECT(w), "save_session", GTK_SIGNAL_FUNC(bomb_save_session),
		     GUINT_TO_POINTER(assign_stage++));

  l = gtk_button_new_with_label("Hi");
  gtk_widget_show(l);
  applet_widget_add(APPLET_WIDGET(w), l);
  BOMB();

  TRY_ABORT_REMOVE();

  BOMB();

  applet_widget_register_stock_callback(APPLET_WIDGET(w), "testitem", GNOME_STOCK_PIXMAP_NEW, "testitem",
					(AppletCallbackFunc)bomb, GINT_TO_POINTER(assign_stage++));

  TRY_ABORT_REMOVE();

  applet_widget_register_stock_callback(APPLET_WIDGET(w), "test2", GNOME_STOCK_PIXMAP_OPEN, "bomb-quit",
					(AppletCallbackFunc)bomb_quit, GINT_TO_POINTER(assign_stage++));

  TRY_ABORT_REMOVE();

  applet_widget_gtk_main();

  return 0;
}
