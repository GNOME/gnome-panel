/* -*- Mode: C; c-set-style: linux indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* quick-desktop-reader.c - Quick .desktop file reader

   Copyright (C) 1999, 2000 Red Hat Inc.
   Copyright (C) 2001 Sid Vicious
   All rights reserved.

   This file is part of the Gnome Library.

   Developed by Elliot Lee <sopwith@redhat.com> and Sid Vicious
   
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */
/*
  @NOTATION@
 */

#include "config.h"

#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <glib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <glib-object.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-url.h>
#include <locale.h>
#include <popt.h>

#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "panel-util.h"

#include "quick-desktop-reader.h"

/* This shares a lot of code with gnome-desktop-item, hmmm .... */

QuickDesktopItem *
quick_desktop_item_load_file (const char *file,
			      const char *expected_type,
			      gboolean    run_tryexec)
{
        QuickDesktopItem *retval;
	char *uri;

        g_return_val_if_fail (file != NULL, NULL);

	if (g_path_is_absolute (file)) {
		uri = gnome_vfs_get_uri_from_local_path (file);
	} else {
		char *cur = g_get_current_dir ();
		char *full = g_build_filename (cur, file, NULL);
		g_free (cur);
		uri = gnome_vfs_get_uri_from_local_path (full);
		g_free (full);
	}
	retval = quick_desktop_item_load_uri (uri, expected_type, run_tryexec);

	g_free (uri);

	return retval;
}

/* check a key and if it's right return a pointer to the value */
static const char *
is_key (const char *key, gsize keylen, const char *buf)
{
	const char *p;
	if (strncmp (key, buf, keylen) != 0)
		return NULL;
	p = buf + keylen;
	if (*p == ' ')
		p++;
	if (*p != '=')
		return NULL;
	p++;
	if (*p == ' ')
		p++;
	return p;
}
/* nice wrapper for this to still allow some compiler optimization
 * of the strlen */
#define IS_KEY(key,buf) (is_key (key, strlen (key), buf))

/* Get a 'translated' key and it's lang */
static const char *
is_i18n_key (const char *key, gsize keylen, const char *buf, char **lang)
{
	const char *p;

	*lang = NULL;

	if (strncmp (key, buf, keylen) != 0)
		return NULL;

	p = buf + keylen;

	if (*p == '[') {
		const char *lptr;
		p++;
		lptr = p;
		while (*p != ']')
			p++;
		if (p != lptr)
			*lang = g_strndup (lptr, p - lptr);
		if (*p == ']')
			p++;
	}

	if (*p == ' ')
		p++;
	if (*p != '=') {
		g_print ("Parser error on line (%s)\n", buf);
		g_free (*lang);
		*lang = NULL;
		return NULL;
	}
	p++;
	if (*p == ' ')
		p++;
	return p;
}
/* nice wrapper for this to still allow some compiler optimization
 * of the strlen */
#define IS_I18N_KEY(key,buf,lang) (is_i18n_key (key, strlen (key), buf, lang))

static int
get_langlevel (const char *lang)
{
	int i = 0;
	const GList *list;

	if (lang == NULL)
		return G_MAXINT-1;

	list = gnome_i18n_get_language_list ("LC_MESSAGES");
	while (list != NULL) {
		const char *locale = list->data;
		if (strcmp (lang, locale) == 0) {
			return i;
		}
		i++;
		list = list->next;
	}
	/* somewhat evil, -1 is 'lower' and thus 'better' then all
	 * others, but I can't think of how else to indicate failure */
	return -1;
}

enum {
	ENCODING_UNKNOWN,
	ENCODING_UTF8,
	ENCODING_LEGACY_MIXED
};

static const int
guess_encoding (const char *uri,
		const char *name,
		const char *comment,
		gboolean old_kde)
{
	if (/* old kde ones are mixed */
	    old_kde ||
	    /* try to guess by location (old gnome) */
	    strstr (uri, "gnome/apps/") != NULL) {
		return ENCODING_LEGACY_MIXED;
	} else if (g_utf8_validate (name, -1, NULL) &&
		   (comment == NULL || g_utf8_validate (comment, -1, NULL))) {
		return ENCODING_UTF8;
	} else {
		return ENCODING_LEGACY_MIXED;
	}
}

static void
insert_locales (GHashTable *encodings, char *enc, ...)
{
	va_list args;
	char *s;

	va_start (args, enc);
	for (;;) {
		s = va_arg (args, char *);
		if (s == NULL)
			break;
		g_hash_table_insert (encodings, s, enc);
	}
	va_end (args);
}

/* make a standard conversion table from the desktop standard spec */
static GHashTable *
init_encodings (void)
{
	GHashTable *encodings = g_hash_table_new (g_str_hash, g_str_equal);

	insert_locales (encodings, "ARMSCII-8", "by", NULL);
	insert_locales (encodings, "BIG5", "zh_TW", NULL);
	insert_locales (encodings, "CP1251", "be", "bg", NULL);
	insert_locales (encodings, "EUC-CN", "zh_CN", NULL);
	insert_locales (encodings, "EUC-JP", "ja", NULL);
	insert_locales (encodings, "EUC-KR", "ko", NULL);
	/* insert_locales (encodings, "GEORGIAN-ACADEMY", NULL); */
	insert_locales (encodings, "GEORGIAN-PS", "ka", NULL);
	insert_locales (encodings, "ISO-8859-1", "br", "ca", "da", "de", "en", "es", "eu", "fi", "fr", "gl", "it", "nl", "wa", "no", "pt", "pt", "sv", NULL);
	insert_locales (encodings, "ISO-8859-2", "cs", "hr", "hu", "pl", "ro", "sk", "sl", "sq", "sr", NULL);
	insert_locales (encodings, "ISO-8859-3", "eo", NULL);
	insert_locales (encodings, "ISO-8859-5", "mk", "sp", NULL);
	insert_locales (encodings, "ISO-8859-7", "el", NULL);
	insert_locales (encodings, "ISO-8859-9", "tr", NULL);
	insert_locales (encodings, "ISO-8859-13", "lt", "lv", "mi", NULL);
	insert_locales (encodings, "ISO-8859-14", "ga", "cy", NULL);
	insert_locales (encodings, "ISO-8859-15", "et", NULL);
	insert_locales (encodings, "KOI8-R", "ru", NULL);
	insert_locales (encodings, "KOI8-U", "uk", NULL);
	insert_locales (encodings, "TCVN-5712", "vi", NULL);
	insert_locales (encodings, "TIS-620", "th", NULL);
	/* insert_locales (encodings, "VISCII", NULL); */

	return encodings;
}

static const char *
get_encoding_from_locale (const char *locale)
{
	char lang[3];
	const char *encoding;
	static GHashTable *encodings = NULL;

	if (locale == NULL)
		return NULL;

	/* if locale includes encoding, use it */
	encoding = strchr (locale, '.');
	if (encoding != NULL) {
		return encoding+1;
	}

	if (encodings == NULL)
		encodings = init_encodings ();

	/* first try the entire locale (at this point ll_CC) */
	encoding = g_hash_table_lookup (encodings, locale);
	if (encoding != NULL)
		return encoding;

	/* Try just the language */
	strncpy (lang, locale, 2);
	lang[2] = '\0';
	return g_hash_table_lookup (encodings, lang);
}

static const char *
get_encoding (int encoding, const char *lang)
{
	const char *enc = strchr (lang, '.');
	if (enc != NULL) {
		enc++;
		if (strcmp (enc, "UTF-8") == 0)
			return NULL;
		return enc;
	} else if (encoding == ENCODING_LEGACY_MIXED) {
		return get_encoding_from_locale (lang);
	} else {
		/* no decoding */
		return NULL;
	}
}

QuickDesktopItem *
quick_desktop_item_load_uri (const char *uri,
			     const char *expected_type,
			     gboolean run_tryexec)
{
	QuickDesktopItem *retval = NULL;
	ReadBuf *rb;
	char buf[BUFSIZ];
	char *name_lang = NULL;
	int name_level = G_MAXINT;
	char *comment_lang = NULL;
	int comment_level = G_MAXINT;
	int encoding = ENCODING_UNKNOWN;
	gboolean old_kde = FALSE;

	g_return_val_if_fail (uri != NULL, NULL);

	rb = readbuf_open (uri);
	
	if (rb == NULL)
		return NULL;

	while (readbuf_gets (buf, sizeof (buf), rb) != NULL) {
		if (strcmp (buf, "[Desktop Entry]") == 0) {
			retval = g_new0 (QuickDesktopItem, 1);
			break;
		} else if (strcmp (buf, "[KDE Desktop Entry]") == 0) {
			retval = g_new0 (QuickDesktopItem, 1);
			old_kde = TRUE;
			break;
		}
	}

	if (retval == NULL) {
		readbuf_close (rb);
		return NULL;
	}

	while (readbuf_gets (buf, sizeof (buf), rb) != NULL) {
		char *lang = NULL;
		const char *val;

		if (buf[0] == '[') {
			break;
		} else if (buf[0] == '\0') {
			continue;
		} else if ((val = IS_I18N_KEY ("Name", buf, &lang)) != NULL) {
			int lev = get_langlevel (lang);
			if (lev >= 0 && lev < name_level) {
				g_free (name_lang);
				name_lang = lang;
				name_level = lev;
				g_free (retval->name);
				retval->name = g_strdup (val);
			} else {
				g_free (lang);
			}
		} else if ((val = IS_I18N_KEY ("Comment", buf, &lang)) != NULL) {
			int lev = get_langlevel (lang);
			if (lev >= 0 && lev < comment_level) {
				g_free (comment_lang);
				comment_lang = lang;
				comment_level = lev;
				g_free (retval->comment);
				retval->comment = g_strdup (val);
			} else {
				g_free (lang);
			}
		} else if ((val = IS_KEY ("Type", buf)) != NULL) {
			if (expected_type != NULL &&
			    strcmp (val, expected_type) != 0) {
				g_print ("bad type '%s' != '%s': %s\n", val,
					 expected_type, uri);
				quick_desktop_item_destroy (retval);
				retval = NULL;
				break;
			}
			g_free (retval->type);
			retval->type = g_strdup (val);
		} else if ((val = IS_KEY ("Icon", buf)) != NULL) {
			g_free (retval->icon);
			retval->icon = g_strdup (val);
		} else if ((val = IS_KEY ("Encoding", buf)) != NULL) {
			if (strcmp (val, "UTF-8") == 0) {
				encoding = ENCODING_UTF8;
			} else if (strcmp (val, "Legacy-Mixed") == 0) {
				encoding = ENCODING_LEGACY_MIXED;
			} else {
				/* According to spec we need to quit */
				g_print ("bad encoding '%s': %s\n", val, uri);
				quick_desktop_item_destroy (retval);
				retval = NULL;
				break;
			}
		} else if ((val = IS_KEY ("TryExec", buf)) != NULL) {
			if (run_tryexec) {
				char *prog = g_find_program_in_path (val);
				if (prog == NULL) {
					g_print ("Try exec failed '%s': %s\n",
						 val, uri);
					quick_desktop_item_destroy (retval);
					retval = NULL;
					break;
				} else {
					g_free (prog);
				}
			}
			g_free (retval->tryexec);
			retval->tryexec = g_strdup (val);
		} else if ((val = IS_KEY ("SortOrder", buf)) != NULL) {
			g_free (retval->sort_order);
			retval->sort_order = g_strdup (val);
		}
	}

	readbuf_close (rb);

	if (retval != NULL &&
	    retval->name == NULL) {
		g_print ("no name: %s\n", uri);
		quick_desktop_item_destroy (retval);
		retval = NULL;
	}

	/* Convert encodings */

	/* Make sure we have some encoding type */
	if (retval != NULL &&
	    (name_lang != NULL ||
	     comment_lang != NULL) &&
	    encoding == ENCODING_UNKNOWN) {
		encoding = guess_encoding (uri,
					   retval->name,
					   retval->comment,
					   old_kde);
	}

	if (retval != NULL &&
	    name_lang != NULL) {
		const char *enc = get_encoding (encoding, name_lang);
		char *utf8_string = g_convert (retval->name, -1, "UTF-8", enc,
					       NULL, NULL, NULL);
		if (utf8_string != NULL) {
			g_free (retval->name);
			retval->name = utf8_string;
		}
	}

	if (retval != NULL &&
	    retval->comment != NULL &&
	    comment_lang != NULL) {
		const char *enc = get_encoding (encoding, comment_lang);
		char *utf8_string = g_convert (retval->comment, -1,
					       "UTF-8", enc,
					       NULL, NULL, NULL);
		if (utf8_string != NULL) {
			g_free (retval->comment);
			retval->comment = utf8_string;
		}
	}

	g_free (name_lang);
	g_free (comment_lang);

	return retval;
}

char *
quick_desktop_item_find_icon (const char *icon)
{
	if (icon == NULL) {
		return NULL;
	} else if (g_path_is_absolute (icon)) {
		if (g_file_test (icon, G_FILE_TEST_EXISTS)) {
			return g_strdup (icon);
		} else {
			return NULL;
		}
	} else {
		char *full = gnome_program_locate_file (NULL,
							GNOME_FILE_DOMAIN_PIXMAP,
							icon,
							TRUE /* only_if_exists */,
							NULL /* ret_locations */);

		/* FIXME: Maybe check elsewhere, this could be from KDE
		 * or some such */
		return full;
	}
}

void
quick_desktop_item_destroy (QuickDesktopItem *item)
{
        g_return_if_fail (item != NULL);

	g_free (item->type);
	item->type = NULL;

	g_free (item->name);
	item->name = NULL;

	g_free (item->comment);
	item->comment = NULL;

	g_free (item->icon);
	item->icon = NULL;

	g_free (item->tryexec);
	item->tryexec = NULL;

	g_free (item->sort_order);
	item->sort_order = NULL;

	g_free (item);
}

static gpointer
_quick_desktop_item_copy (gpointer boxed)
{
	QuickDesktopItem *item = boxed;
	QuickDesktopItem *newitem = g_new0 (QuickDesktopItem, 1);
	newitem->type = g_strdup (item->type);
	newitem->name = g_strdup (item->name);
	newitem->comment = g_strdup (item->comment);
	newitem->icon = g_strdup (item->icon);
	newitem->tryexec = g_strdup (item->tryexec);
	newitem->sort_order = g_strdup (item->sort_order);
	return newitem;
}

static void
_quick_desktop_item_free (gpointer boxed)
{
	quick_desktop_item_destroy (boxed);
}

GType
quick_desktop_item_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static ("QuickDesktopItem",
						     _quick_desktop_item_copy,
						     _quick_desktop_item_free);
	}

	return type;
}
