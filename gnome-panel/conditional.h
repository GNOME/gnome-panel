#ifndef CONDITIONAL_H
#define CONDITIONAL_H

G_BEGIN_DECLS

/* get key, if key doesn't exist append "Conditional" to key and treat
 * value as "{<conditional1>}<value1>{<conditional2>}<value2>...",
 * key should not include "=default", which should be given as argument */
char *		conditional_get_string	(const char *key,
					 const char *def,
					 gboolean *isdef);
gboolean	conditional_get_bool	(const char *key,
					 gboolean def,
					 gboolean *isdef);
int		conditional_get_int	(const char *key,
					 int def,
					 gboolean *isdef);

/* get key and treat it as a conditional, if missing, it's true */
gboolean	conditional_true	(const char *key);

G_END_DECLS

#endif
