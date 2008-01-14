static gboolean
intlclock_events_popup_expose_cb (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
        IntlClockEventsPopupPrivate *priv = PRIVATE (user_data);

        cairo_t *cr;

        cr = gdk_cairo_create (widget->window);

        cairo_rectangle (
                cr,
                event->area.x, event->area.y,
                event->area.width, event->area.height);

        cairo_clip (cr);

/* draw window background */

        cairo_rectangle (
                cr,
                widget->allocation.x + 0.5, widget->allocation.y + 0.5,
                widget->allocation.width - 1, widget->allocation.height - 1);

        cairo_set_source_rgb (
                cr,
                widget->style->bg [GTK_STATE_ACTIVE].red   / 65535.0,
                widget->style->bg [GTK_STATE_ACTIVE].green / 65535.0,
                widget->style->bg [GTK_STATE_ACTIVE].blue  / 65535.0);

        cairo_fill_preserve (cr);

/* draw window outline */

        cairo_set_source_rgb (
                cr,
                widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
                widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
                widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0);

        cairo_set_line_width (cr, 1.0);
        cairo_stroke (cr);

/* draw main pane background */

        cairo_rectangle (
                cr,
                priv->main_section->allocation.x + 0.5, priv->main_section->allocation
.y + 0.5,
                priv->main_section->allocation.width - 1, priv->main_section->allocation.height - 1);

        cairo_set_source_rgb (
                cr,
                widget->style->bg [GTK_STATE_NORMAL].red   / 65535.0,
                widget->style->bg [GTK_STATE_NORMAL].green / 65535.0,
                widget->style->bg [GTK_STATE_NORMAL].blue  / 65535.0);

        cairo_fill_preserve (cr);

        cairo_set_source_rgb (
                cr,
                widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
                widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
                widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0);

        cairo_stroke (cr);

        cairo_destroy (cr);

        return FALSE;
}
