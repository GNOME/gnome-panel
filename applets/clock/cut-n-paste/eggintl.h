#ifndef __EGG_INTL_H__
#define __EGG_INTL_H__

#include <libintl.h>

#ifndef _
#define _(x) gettext(x)
#endif

#ifndef N_
#define N_(x) x
#endif

#endif /* __EGG_INTL_H__ */
