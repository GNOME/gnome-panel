/*
 * Copyright (C) 2018 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gp-initial-setup-dialog-private.h"

#include <glib/gi18n-lib.h>

struct _GpInitialSetupDialog
{
  GtkWindow               parent;

  GtkWidget              *header_bar;
  GtkWidget              *done;

  GpInitialSetupCallback  setup_callback;
  gpointer                setup_user_data;
  GDestroyNotify          setup_user_data_free_func;

  gpointer                content_user_data;
  GDestroyNotify          content_user_data_free_func;

  GHashTable             *settings;
};

enum
{
  CLOSE,

  LAST_SIGNAL
};

static guint dialog_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GpInitialSetupDialog, gp_initial_setup_dialog, GTK_TYPE_WINDOW)

static void
cancel_clicked_cb (GtkButton            *button,
                   GpInitialSetupDialog *dialog)
{
  g_assert (dialog->setup_callback != NULL);
  dialog->setup_callback (dialog, TRUE, dialog->setup_user_data);
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
done_clicked_cb (GtkButton            *button,
                 GpInitialSetupDialog *dialog)
{
  g_assert (dialog->setup_callback != NULL);
  dialog->setup_callback (dialog, FALSE, dialog->setup_user_data);
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
close_cb (GpInitialSetupDialog *self,
          gpointer              user_data)
{
  g_assert (self->setup_callback != NULL);
  self->setup_callback (self, TRUE, self->setup_user_data);
  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
gp_initial_setup_dialog_finalize (GObject *object)
{
  GpInitialSetupDialog *dialog;

  dialog = GP_INITIAL_SETUP_DIALOG (object);

  if (dialog->setup_user_data_free_func != NULL)
    {
      dialog->setup_user_data_free_func (dialog->setup_user_data);

      dialog->setup_user_data_free_func = NULL;
      dialog->setup_user_data = NULL;
    }

  if (dialog->content_user_data_free_func != NULL)
    {
      dialog->content_user_data_free_func (dialog->content_user_data);

      dialog->content_user_data_free_func = NULL;
      dialog->content_user_data = NULL;
    }

  g_clear_pointer (&dialog->settings, g_hash_table_destroy);

  G_OBJECT_CLASS (gp_initial_setup_dialog_parent_class)->finalize (object);
}

static gboolean
gp_initial_setup_dialog_delete_event (GtkWidget   *widget,
                                      GdkEventAny *event)
{
  GpInitialSetupDialog *dialog;

  dialog = GP_INITIAL_SETUP_DIALOG (widget);

  g_assert (dialog->setup_callback != NULL);
  dialog->setup_callback (dialog, TRUE, dialog->setup_user_data);

  return FALSE;
}

static void
install_signals (void)
{
  dialog_signals[CLOSE] =
    g_signal_new ("close",
                  GP_TYPE_INITIAL_SETUP_DIALOG,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
gp_initial_setup_dialog_class_init (GpInitialSetupDialogClass *dialog_class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkBindingSet *binding_set;

  object_class = G_OBJECT_CLASS (dialog_class);
  widget_class = GTK_WIDGET_CLASS (dialog_class);

  object_class->finalize = gp_initial_setup_dialog_finalize;

  widget_class->delete_event = gp_initial_setup_dialog_delete_event;

  install_signals ();

  binding_set = gtk_binding_set_by_class (widget_class);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);
}

static void
gp_initial_setup_dialog_init (GpInitialSetupDialog *dialog)
{
  GtkWidget *header_bar;
  GtkWidget *cancel;
  GtkWidget *done;
  GtkStyleContext *style;

  dialog->settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                            (GDestroyNotify) g_variant_unref);

  header_bar = dialog->header_bar = gtk_header_bar_new ();
  gtk_header_bar_set_title (GTK_HEADER_BAR (header_bar), _("Initial Setup"));
  gtk_window_set_titlebar (GTK_WINDOW (dialog), header_bar);
  gtk_widget_show (header_bar);

  cancel = gtk_button_new_with_label (_("Cancel"));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (dialog->header_bar), cancel);
  gtk_widget_show (cancel);

  done = dialog->done = gtk_button_new_with_label (_("Done"));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (dialog->header_bar), done);
  gtk_widget_set_sensitive (done, FALSE);
  gtk_widget_show (done);

  style = gtk_widget_get_style_context (done);
  gtk_style_context_add_class (style, GTK_STYLE_CLASS_SUGGESTED_ACTION);

  g_signal_connect (cancel, "clicked", G_CALLBACK (cancel_clicked_cb), dialog);
  g_signal_connect (done, "clicked", G_CALLBACK (done_clicked_cb), dialog);

  g_signal_connect (dialog, "close", G_CALLBACK (close_cb), NULL);
}

GpInitialSetupDialog *
gp_initial_setup_dialog_new (void)
{
  return g_object_new (GP_TYPE_INITIAL_SETUP_DIALOG, NULL);
}

void
gp_initial_setup_dialog_add_callback (GpInitialSetupDialog   *dialog,
                                      GpInitialSetupCallback  callback,
                                      gpointer                user_data,
                                      GDestroyNotify          free_func)
{
  dialog->setup_callback = callback;
  dialog->setup_user_data = user_data;
  dialog->setup_user_data_free_func = free_func;
}

void
gp_initial_setup_dialog_add_content_widget (GpInitialSetupDialog *dialog,
                                            GtkWidget            *content,
                                            gpointer              user_data,
                                            GDestroyNotify        free_func)
{
  dialog->content_user_data = user_data;
  dialog->content_user_data_free_func = free_func;

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
  gtk_container_add (GTK_CONTAINER (dialog), content);
  gtk_widget_show (content);
}

/**
 * gp_initial_setup_dialog_get_setting:
 * @dialog: a #GpInitialSetupDialog
 * @key: the setting key
 *
 * Gets a setting for @key.
 *
 * Returns: (transfer none): a #GVariant, or %NULL.
 */
GVariant *
gp_initial_setup_dialog_get_setting (GpInitialSetupDialog *dialog,
                                     const char           *key)
{
  return g_hash_table_lookup (dialog->settings, key);
}

/**
 * gp_initial_setup_dialog_set_setting:
 * @dialog: a #GpInitialSetupDialog
 * @key: the setting key
 * @value: (allow-none): the setting value, or %NULL to unset the setting
 *
 * Sets a setting for @key with value @value. If @value is %NULL,
 * a previously set setting for @key is unset.
 *
 * If @value is floating, it is consumed.
 */
void
gp_initial_setup_dialog_set_setting (GpInitialSetupDialog *dialog,
                                     const char           *key,
                                     GVariant             *value)
{
  if (value != NULL)
    {
      g_hash_table_insert (dialog->settings, g_strdup (key),
                           g_variant_ref_sink (value));
    }
  else
    {
      g_hash_table_remove (dialog->settings, key);
    }
}

GVariant *
gp_initial_setup_dialog_get_settings (GpInitialSetupDialog *dialog)
{
  GVariantBuilder builder;
  GHashTableIter iter;
  gpointer key;
  gpointer data;
  GVariant *settings;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_hash_table_iter_init (&iter, dialog->settings);

  while (g_hash_table_iter_next (&iter, &key, &data))
    g_variant_builder_add (&builder, "{sv}", key, data);

  settings = g_variant_builder_end (&builder);

  return g_variant_ref_sink (settings);
}

void
gp_initial_setup_dialog_set_settings (GpInitialSetupDialog *dialog,
                                      GVariant             *settings)
{
  GVariantIter iter;
  char *key;
  GVariant *value;

  if (settings == NULL)
    return;

  g_variant_iter_init (&iter, settings);
  while (g_variant_iter_next (&iter, "{sv}", &key, &value))
    {
      gp_initial_setup_dialog_set_setting (dialog, key, value);

      g_free (key);
      g_variant_unref (value);
    }
}

void
gp_initial_setup_dialog_set_done (GpInitialSetupDialog *dialog,
                                  gboolean              done)
{
  gtk_widget_set_sensitive (dialog->done, done);
}
