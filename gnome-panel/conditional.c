/* Gnome conditional: conditional loading
 * (C) 2001 George Lebl
 */

#include "config.h"
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <gnome.h>

#include "panel-include.h"

#include "conditional.h"

static gboolean
linux_battery_exists (void)
{
	FILE *fp;
	char buf[200] = "";
	int foo;

	if ( ! panel_file_exists("/proc/apm"))
		return FALSE;

	fp = fopen ("/proc/apm", "r");
	if (fp == NULL)
		return FALSE;

	if (fgets (buf, sizeof (buf), fp) == NULL) {
		fclose (fp);
		return FALSE;
	}
	fclose (fp);

	foo = -1;
	sscanf (buf,
		"%*s %*d.%*d %*x %*x %*x %*x %d%% %*d %*s\n",
		&foo);

	if (foo >= 0)
		return TRUE;
	else
		return FALSE;
}

static gboolean
battery_exists (void)
{
#ifndef __linux__
	return FALSE;
#else
	/* This is MUUUUCHO ugly, but apparently RH 7.0 with segv the program
 	 * reading /proc/apm on some laptops, and we don't want to crash, thus
 	 * we do the check in a forked process */
	int status;
	pid_t pid;

	pid = fork ();
	if (pid == 0) {
                struct sigaction sa = {{NULL}};

		sa.sa_handler = SIG_DFL;

                sigaction(SIGSEGV, &sa, NULL);
                sigaction(SIGFPE, &sa, NULL);
                sigaction(SIGBUS, &sa, NULL);

		if (linux_battery_exists ())
			_exit (0);
		else
			_exit (1);
	}

	status = 0;
	waitpid (pid, &status, 0);

	if ( ! WIFSIGNALED (status) &&
	    WIFEXITED (status) &&
	    WEXITSTATUS (status) == 0) {
		return TRUE;
	} else {
		return FALSE;
	}
#endif
}

static gboolean
is_tokchar (char c)
{
	return ((c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') ||
		c == '_');
}

static int
get_string (char const *buffer, int len)
{
	int llen = 0;
	const char *p;

	for (p = buffer, llen = 0;
	     *p != '\0' && len > 0;
	     p++, llen++) {
		if (*p == '\'') {
			return llen;
		}
		len --;
	}
	return llen;
}

static const char *
get_word (char const **retbuf, int *len, int *rlen)
{
	const char *buffer = *retbuf;

	*rlen = 0;

	if (buffer == NULL || *buffer == '\0' || *len == 0)
		return NULL;

	while ((*buffer == '\t' ||
		*buffer == ' ' ||
		*buffer == '\r') &&
	       *len > 0) {
		buffer ++;
		(*len) --;
		(*retbuf) ++;
	}

	if (*buffer == '\0' || *len == 0)
		return NULL;

	/* some simple thingies */
	if (*len >= 2 &&
	    (strncmp (buffer, "==", 2) == 0 ||
	     strncmp (buffer, ">=", 2) == 0 ||
	     strncmp (buffer, "<=", 2) == 0 ||
	     strncmp (buffer, "!=", 2) == 0 ||
	     strncmp (buffer, "||", 2) == 0 ||
	     strncmp (buffer, "&&", 2) == 0)) {
		*len -= 2;
		*rlen = 2;
		(*retbuf) += 2;
		return buffer;
	}

	if (is_tokchar (*buffer)) {
		const char *p = buffer;
		*rlen = 0;
		do {
			(*rlen) ++;
			p ++;
			(*len) --;
		} while (is_tokchar (*p) && *len > 0);
		(*retbuf) += *rlen;
		return buffer;
	}

	/* well, this is just a single token thingie */
	(*len) --;
	*rlen = 1;
	(*retbuf) ++;
	return buffer;
}

static int
get_paren (char const *buffer, int len, char open, char close)
{
	int llen = 0;
	const char *p;
	int depth = 0;

	for (p = buffer, llen = 0;
	     *p != '\0' && len > 0;
	     p++, llen++) {
		if (*p == open) {
			depth++;
		} else if (*p == close) {
			depth--;
			if (depth < 0)
				return llen+1;
		}
		len --;
	}
	return llen;
}

#define IS_KWORD(kword,word,len) ((len) == strlen (kword) && \
				  strncmp ((word), (kword), (len)) == 0)

static gboolean
have_cond (const char *word, int len)
{
	if (IS_KWORD ("battery", word, len)) {
		static int got = -1;
		if (got > -1)
			return got;
		got = battery_exists ();
		return got;
	}
	return FALSE;
}

static int
get_value (const char *val, int len)
{
	if (IS_KWORD ("screen_width", val, len)) {
		return gdk_screen_width ();
	} else if (IS_KWORD ("screen_height", val, len)) {
		return gdk_screen_height ();
	} else if (val[0] >= '0' && val[0] <= '9') {
		return atoi (val);
	} else {
		char *str = g_strndup (val, len);
		g_warning ("Unknown value %s", str);
		g_free (str);
		return FALSE;
	}
}

static gboolean
eval_comp (const char *comp, int complen, int value1, int value2)
{
	if (IS_KWORD (">", comp, complen))
		return value1 > value2;
	else if (IS_KWORD ("<", comp, complen))
		return value1 < value2;
	else if (IS_KWORD (">=", comp, complen))
		return value1 >= value2;
	else if (IS_KWORD ("<=", comp, complen))
		return value1 <= value2;
	else if (IS_KWORD ("==", comp, complen) ||
		 IS_KWORD ("=", comp, complen))
		return value1 == value2;
	else if (IS_KWORD ("!=", comp, complen))
		return value1 != value2;
	else {
		g_warning ("Unknown comparison %c%c",
			   *comp,
			   *(comp+1) /* in the worst case this is '\0'*/);
		return FALSE;
	}
}

enum {
	CONN_AND,
	CONN_OR
};

static gboolean
eval_cond (int connector, gboolean a, gboolean b)
{
	if (connector == CONN_AND) {
		return a && b;
	} else {
		return a || b;
	}
}

gboolean
conditional_parse (const char *conditional, int len)
{
	int llen;
	const char *word;
	gboolean last_cond = TRUE;
	int last_connector = CONN_AND;
	gboolean invert = FALSE;

	if (conditional == NULL)
		return TRUE;

	if (len <= 0) {
		len = strlen (conditional);
	}

#if 0
	/* Some debugging */
	{
		char *foo = g_strndup (conditional, len);
		g_print ("Conditional: '%s'\n", foo);
		g_free (foo);
	}
#endif


	word = conditional;
	while ((word = get_word (&conditional, &len, &llen))) {
		/* ignore trailing )'s */
		if (*word == ')' && len == 0) {
			break;
		} else if (*word == '(') {
			int plen = get_paren (conditional, len, '(', ')');
			gboolean cond = conditional_parse (conditional, plen);
			conditional += plen;
			len -= plen;
			last_cond = eval_cond (last_connector,
					       last_cond,
					       invert ? !cond : cond);
			invert = FALSE;
		} else if (IS_KWORD ("not", word, llen) ||
			   *word == '!') {
			invert = TRUE;
		} else if (IS_KWORD ("true", word, llen)) {
			last_cond = eval_cond (last_connector,
					       last_cond,
					       invert ? !TRUE : TRUE);
			invert = FALSE;
		} else if (IS_KWORD ("false", word, llen)) {
			last_cond = eval_cond (last_connector,
					       last_cond,
					       invert ? !FALSE : FALSE);
			invert = FALSE;
		} else if (IS_KWORD ("||", word, llen) ||
			   IS_KWORD ("or", word, llen)) {
			invert = FALSE;
			last_connector = CONN_OR;
		} else if (IS_KWORD ("&&", word, llen) ||
			   IS_KWORD ("and", word, llen)) {
			invert = FALSE;
			last_connector = CONN_AND;
		} else if (IS_KWORD ("have", word, llen)) {
			gboolean cond;
			word = get_word (&conditional, &len, &llen);
			if (word == NULL) {
				g_warning ("Conditional parse error");
				return FALSE;
			}
			cond = have_cond (word, llen);
			last_cond = eval_cond (last_connector,
					       last_cond,
					       invert ? !cond : cond);
			invert = FALSE;
		} else if (IS_KWORD ("exists", word, llen)) {
			char *file;
			gboolean cond;
			word = get_word (&conditional, &len, &llen);
			if (word == NULL) {
				g_warning ("Conditional parse error");
				return FALSE;
			}
			if (*word == '\'') {
				int slen = get_string (conditional, len);
				file = g_strndup (conditional, slen);
				conditional += slen;
				len -= slen;
				if (*conditional == '\'') {
					conditional ++;
					len--;
				}
			} else {
				file = g_strndup (word, llen);
			}

			cond = FALSE;
			if (panel_file_exists (file)) {
				cond = TRUE;
			} else {
				char *full = gnome_datadir_file (file);
				if (full != NULL)
					cond = TRUE;
				g_free (full);
			}
			last_cond = eval_cond (last_connector,
					       last_cond,
					       invert ? !cond : cond);
			invert = FALSE;
			g_free (file);
		} else if (is_tokchar (*word)) {
			int value1 = get_value (word, llen);
			int value2;
			gboolean cond;
			const char *comp;
			int complen;
			word = get_word (&conditional, &len, &llen);
			if (word == NULL) {
				g_warning ("Conditional parse error");
				return FALSE;
			}
			comp = word;
			complen = llen;
			word = get_word (&conditional, &len, &llen);
			if (word == NULL) {
				g_warning ("Conditional parse error");
				return FALSE;
			}
			value2 = get_value (word, llen);
			cond = eval_comp (comp, complen, value1, value2);
			last_cond = eval_cond (last_connector,
					       last_cond,
					       invert ? !cond : cond);
			invert = FALSE;
		} else {
			g_warning ("Conditional parse error");
			return FALSE;
		}

	}
	return last_cond;
}

static char *
get_conditional_value (const char *string, const char *ifempty)
{
	int condlen;
	const char *cond;

	if (*string == '\0')
		return g_strdup (ifempty);
	if (*string != '{')
		return g_strdup (string);

	string ++;
	condlen = get_paren (string, strlen (string), '{', '}');
	cond = string;
	string += condlen; /* full condlen, including the '}' */

	if (*(cond+condlen-1) == '}')
		condlen --; /* wipe the trailing } */
	if (condlen <= 0 /* empty condition means go */ ||
	    conditional_parse (cond, condlen)) {
		int i;
		for (i = 0; string[i] != '\0' && string[i] != '{'; i++)
			;
		if (i > 0)
			return g_strndup (string, i);
		else
			return g_strdup (ifempty);
	} else {
		while (*string != '{' &&
		       *string != '\0')
			string++;
		return get_conditional_value (string, ifempty);
	}
}

char *
conditional_get_string (const char *key, const char *def, gboolean *isdef)
{
	gboolean lisdef;
	char *fullkey;
	char *ret;

	if (def != NULL)
		fullkey = g_strdup_printf ("%s=%s", key, def);
	else
		fullkey = g_strdup (key);

	if (isdef != NULL)
		*isdef = FALSE;

	ret = gnome_config_get_string_with_default (fullkey, &lisdef);
	g_free (fullkey);
	if (lisdef) {
		char *cond;
		g_free (ret);
		fullkey = g_strdup_printf ("%s:Conditional", key);
		cond = gnome_config_get_string_with_default (fullkey, &lisdef);
		g_free (fullkey);
		if (lisdef) {
			if (isdef != NULL)
				*isdef = TRUE;
			g_free (cond);
			return g_strdup (def);
		}
		ret = get_conditional_value (cond, "");
		g_free (cond);
	}
	return ret;
}

gboolean
conditional_get_bool (const char *key, gboolean def, gboolean *isdef)
{
	gboolean lisdef;
	char *fullkey;
	gboolean ret;

	fullkey = g_strdup_printf ("%s=%s", key,
				   def ? "true" : "false");

	if (isdef != NULL)
		*isdef = FALSE;

	ret = gnome_config_get_bool_with_default (fullkey, &lisdef);
	g_free (fullkey);
	if (lisdef) {
		char *cond, *str;
		fullkey = g_strdup_printf ("%s:Conditional", key);
		cond = gnome_config_get_string_with_default (fullkey, &lisdef);
		g_free (fullkey);
		if (lisdef) {
			if (isdef != NULL)
				*isdef = TRUE;
			g_free (cond);
			return def;
		}
		str = get_conditional_value (cond, "false");
		g_free (cond);
		if (*str == 'T' || *str == 't' ||
		    *str == 'Y' || *str == 'y' ||
		    atoi (str) != 0) {
			ret = TRUE;
		} else {
			ret = FALSE;
		}
		g_free (str);
	}
	return ret;
}

int
conditional_get_int (const char *key, int def, gboolean *isdef)
{
	gboolean lisdef;
	char *fullkey;
	int ret;

	fullkey = g_strdup_printf ("%s=%d", key, def);

	if (isdef != NULL)
		*isdef = FALSE;

	ret = gnome_config_get_int_with_default (fullkey, &lisdef);
	g_free (fullkey);
	if (lisdef) {
		char *cond, *str;
		fullkey = g_strdup_printf ("%s:Conditional", key);
		cond = gnome_config_get_string_with_default (fullkey, &lisdef);
		g_free (fullkey);
		if (lisdef) {
			if (isdef != NULL)
				*isdef = TRUE;
			g_free (cond);
			return def;
		}
		str = get_conditional_value (cond, "");
		g_free (cond);
		if (*str == '\0') {
			ret = def;
		} else {
			ret = atoi (str);
		}
		g_free (str);
	}
	return ret;
}

/* get key and treat it as a conditional */
gboolean
conditional_true (const char *key)
{
	gboolean isdef;
	char *cond;
	cond = gnome_config_get_string_with_default (key, &isdef);
	if (string_empty (cond) || isdef) {
		g_free (cond);
		return TRUE;
	} else {
		gboolean ret = conditional_parse (cond, strlen (cond));
		g_free (cond);
		return ret;
	}
}
