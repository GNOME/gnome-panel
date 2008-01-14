gchar *
intlclock_format_time (IntlClock *this, struct tm *now,
		       gboolean show_date, gboolean show_12_hours,
		       gboolean show_seconds, gboolean show_year,
                       gboolean show_day,
		       gchar *tzname, gboolean use_pango_markup)
{
        IntlClockPrivate *priv = PRIVATE (this);

	char buf[256];
	gchar *format;
	gchar *ret;
	gint hour = now->tm_hour;

        time_t local_t;
        struct tm local_now;
        time (&local_t);
        localtime_r (&local_t, &local_now);

	/* temporary string translations I know I'll need but haven't
	 * written into the code yet */
	gchar *tmp;
	tmp = _("%b %%d, %Y\n%%d:%M <span size=\"smaller\">%p %%s</span>");

	if (show_12_hours) {
		hour = hour % 12;
		if (hour == 0)
			hour = 12;
	}

	/* Use these double printfs, because strftime really
	 * likes to put a space before single-digit numbers */

	if (show_date) {
		if (show_seconds && show_year) {
			format = _("%b %%d, %Y\n%%d:%M:%S");
		} else if (show_year) {
			format = _("%b %%d, %Y\n%%d:%M");
		} else if (show_seconds) {
			format = _("%b %%d, %%d:%M:%S");
		}
		else {
			format = _("%b %%d, %%d:%M");
		}
			
                if (show_day) {
                        format = g_strconcat ("%a ", format, NULL);
                        strftime (buf, sizeof(buf)-1, format, now);
                        g_free (format);
                } else {
                        strftime (buf, sizeof(buf)-1, format, now);
                }

		ret = g_strdup_printf (buf, now->tm_mday, hour);
	} else {
		if (show_seconds)
			format = _("%%d:%M:%S");
		else
			format = _("%%d:%M");

		strftime (buf, sizeof(buf)-1, format, now);
		ret = g_strdup_printf (buf, hour);
	}

	if (show_12_hours) {
		char *tmp;

		if (tzname) {
			/* AM/PM and time zone */
                        if (local_now.tm_wday != now->tm_wday) {
                                if (use_pango_markup) {
                                        format = _(" <small>%p %%s (%A)</small>");
                                } else {
                                        format = " %p %%s (%A)";
                                }
                        } else {
                                if (use_pango_markup) {
                                        format = _(" <small>%p %%s</small>");
                                } else {
                                        format = _( "%p %%s");
                                }
                        }
			strftime (buf, sizeof(buf)-1, format, now);
			tmp = g_strconcat (ret, buf, NULL);
			g_free (ret);
			ret = tmp;

			tmp = g_strdup_printf (tmp, tzname);
			g_free (ret);
			ret = tmp;
		} else {
                        if (local_now.tm_wday != now->tm_wday) {
                                format = " <small>%p (%A)</small>";
                        } else {
                                format = " <small>%p</small>";
                        }
			strftime (buf, sizeof(buf)-1, format, now);
			tmp = g_strconcat (ret, buf, NULL);

			g_free (ret);
			ret = tmp;
		}
	} else {
		if (tzname) {
                        if (use_pango_markup) {
                                format = " <small>%s</small>";
                        } else {
                                format = " %s";
                        }

                        tmp = g_strdup_printf (format, tzname);
                        ret = g_strconcat (ret, tmp, NULL);
			g_free (tmp);
		}
        }

        tmp = g_locale_to_utf8 (ret, -1, NULL, NULL, NULL);
        g_free (ret);

	return tmp;
}
