#include <config.h>
#include "screenshot-dialog.h"
#include "screenshot-save.h"
#include <gnome.h>



static GtkTargetEntry drag_types[] =
{
  { "image/png", 0, 0 },
  { "x-special/gnome-icon-list", 0, 0 },
  { "text/uri-list", 0, 0 }
};

struct ScreenshotDialog
{
  GladeXML *xml;
  GdkPixbuf *screenshot;
  GdkPixbuf *preview_image;
  GtkWidget *save_widget;
};

static gboolean
on_toplevel_key_press_event (GtkWidget *widget,
			     GdkEventKey *key)
{
  if (key->keyval == GDK_F1)
    {
      gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_HELP);
      return TRUE;
    }

  return FALSE;
}

static void
on_preview_expose_event (GtkWidget      *drawing_area,
			 GdkEventExpose *event,
			 gpointer        data)
{
  ScreenshotDialog *dialog = data;

  /* FIXME: Draw it insensitive in that case */
  gdk_draw_pixbuf (drawing_area->window,
		   drawing_area->style->white_gc,
		   dialog->preview_image,
		   event->area.x,
		   event->area.y,
		   event->area.x,
		   event->area.y,
		   event->area.width,
		   event->area.height,
		   GDK_RGB_DITHER_NORMAL,
		   0, 0);
}

static void
on_preview_configure_event (GtkWidget         *drawing_area,
			    GdkEventConfigure *event,
			    gpointer           data)
{
  ScreenshotDialog *dialog = data;

  if (dialog->preview_image)
    g_object_unref (G_OBJECT (dialog->preview_image));

  dialog->preview_image = gdk_pixbuf_scale_simple (dialog->screenshot,
						   event->width,
						   event->height,
						   GDK_INTERP_BILINEAR);
}

static void
drag_data_get (GtkWidget          *widget,
	       GdkDragContext     *context,
	       GtkSelectionData   *selection_data,
	       guint               info,
	       guint               time,
	       gpointer            data)
{
	char *string;

	string = g_strdup_printf ("file:%s\r\n",
				  screenshot_save_get_filename ());
	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, string, strlen (string)+1);
	g_free (string);
}

static void
drag_begin (GtkWidget        *widget,
	    GdkDragContext   *context,
	    ScreenshotDialog *dialog)
{
  static GdkPixmap *pixmap;
  GdkBitmap *mask;

  gdk_pixbuf_render_pixmap_and_mask (dialog->preview_image, &pixmap, &mask, 128);
  gtk_drag_set_icon_pixmap (context, gdk_rgb_get_colormap (), pixmap, mask, 0, 0);
}


ScreenshotDialog *
screenshot_dialog_new (GdkPixbuf *screenshot,
		       char      *initial_uri,
		       gboolean   take_window_shot)
{
  ScreenshotDialog *dialog;
  GtkWidget *toplevel;
  GtkWidget *preview_darea;
  GtkWidget *aspect_frame;
  GtkWidget *file_chooser_frame;
  gint width, height;
  char *current_folder;
  char *current_name;
  GnomeVFSURI *tmp_uri;
  GnomeVFSURI *parent_uri;

  tmp_uri = gnome_vfs_uri_new (initial_uri);
  parent_uri = gnome_vfs_uri_get_parent (tmp_uri);

  current_name = gnome_vfs_uri_extract_short_name (tmp_uri);
  current_folder = gnome_vfs_uri_to_string (parent_uri, GNOME_VFS_URI_HIDE_NONE);
  gnome_vfs_uri_unref (tmp_uri);
  gnome_vfs_uri_unref (parent_uri);

  dialog = g_new0 (ScreenshotDialog, 1);

  dialog->xml = glade_xml_new (GLADEDIR "/gnome-panel-screenshot.glade", NULL, NULL);
  dialog->screenshot = screenshot;

  if (dialog->xml == NULL)
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (NULL, 0,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       _("Glade file for the screenshot program is missing.\n"
					 "Please check your installation of gnome-panel"));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      exit (1);
    }

  width = gdk_pixbuf_get_width (screenshot);
  height = gdk_pixbuf_get_height (screenshot);

  width /= 5;
  height /= 5;

  toplevel = glade_xml_get_widget (dialog->xml, "toplevel");
  aspect_frame = glade_xml_get_widget (dialog->xml, "aspect_frame");
  preview_darea = glade_xml_get_widget (dialog->xml, "preview_darea");
  file_chooser_frame = glade_xml_get_widget (dialog->xml, "file_chooser_frame");

  dialog->save_widget = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog->save_widget), FALSE);
  gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog->save_widget), current_folder);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog->save_widget), current_name);
  g_free (current_folder);
  g_free (current_name);
  gtk_container_add (GTK_CONTAINER (file_chooser_frame), dialog->save_widget);

  gtk_window_set_default_size (GTK_WINDOW (toplevel), width * 2, -1);
  gtk_widget_set_size_request (preview_darea, width, height);
  gtk_aspect_frame_set (GTK_ASPECT_FRAME (aspect_frame), 0.0, 0.5,
			gdk_pixbuf_get_width (screenshot)/
			(gfloat) gdk_pixbuf_get_height (screenshot),
			FALSE);
  g_signal_connect (toplevel, "key_press_event", G_CALLBACK (on_toplevel_key_press_event), dialog);
  g_signal_connect (preview_darea, "expose_event", G_CALLBACK (on_preview_expose_event), dialog);
  g_signal_connect (preview_darea, "configure_event", G_CALLBACK (on_preview_configure_event), dialog);

  if (take_window_shot)
    gtk_frame_set_shadow_type (GTK_FRAME (aspect_frame), GTK_SHADOW_NONE);
  else
    gtk_frame_set_shadow_type (GTK_FRAME (aspect_frame), GTK_SHADOW_IN);

  /* setup dnd */
  g_signal_connect (G_OBJECT (preview_darea), "drag_begin",
		    G_CALLBACK (drag_begin), dialog);
  g_signal_connect (G_OBJECT (preview_darea), "drag_data_get",
		    G_CALLBACK (drag_data_get), dialog);

  gtk_widget_show_all (toplevel);

  return dialog;
}

void
screenshot_dialog_enable_dnd (ScreenshotDialog *dialog)
{
  GtkWidget *preview_darea;

  g_return_if_fail (dialog != NULL);

  preview_darea = glade_xml_get_widget (dialog->xml, "preview_darea");
  gtk_drag_source_set (preview_darea,
		       GDK_BUTTON1_MASK|GDK_BUTTON3_MASK,
		       drag_types, G_N_ELEMENTS (drag_types),
		       GDK_ACTION_COPY);
}

GtkWidget *
screenshot_dialog_get_toplevel (ScreenshotDialog *dialog)
{
  return glade_xml_get_widget (dialog->xml, "toplevel");
}

char *
screenshot_dialog_get_uri (ScreenshotDialog *dialog)
{
  return gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog->save_widget));
}

void
screenshot_dialog_set_busy (ScreenshotDialog *dialog,
			    gboolean          busy)
{
  GtkWidget *toplevel;

  toplevel = screenshot_dialog_get_toplevel (dialog);

  if (busy)
    {
      GdkCursor *cursor;
      /* Change cursor to busy */
      cursor = gdk_cursor_new (GDK_WATCH);
      gdk_window_set_cursor (toplevel->window, cursor);
      gdk_cursor_unref (cursor);
    }
  else
    {
      gdk_window_set_cursor (toplevel->window, NULL);
    }

  gtk_widget_set_sensitive (toplevel, ! busy);

  gdk_flush ();
}
