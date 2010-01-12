platform_defines = -DEGG_SM_CLIENT_BACKEND_XSMP
platform_sources = eggsmclient-xsmp.c

INCLUDES =                               \
	-DG_LOG_DOMAIN=\""EggSMClient"\" \
	$(platform_defines)              \
	$(EGG_SMCLIENT_CFLAGS)           \
	$(WARN_CFLAGS)			 \
	$(DISABLE_DEPRECATED_CFLAGS)

noinst_LTLIBRARIES = libegg.la

libegg_la_LIBADD =			\
	$(EGG_SMCLIENT_LIBS)

libegg_la_SOURCES =			\
	eggdesktopfile.c                \
	eggdesktopfile.h		\
	eggsmclient.c                   \
	eggsmclient.h                   \
	eggsmclient-private.h           \
	$(platform_sources)

-include $(top_srcdir)/git.mk
