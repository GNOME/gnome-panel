SUBDIRS =

EXTRA_DIST = \
	panel.global		\
	panel.config		\
	Applet_Config

Global_directory = $(sysconfdir)/gnome/panel-config
Config_directory = $(sysconfdir)/gnome/panel-config/panel.d/default

install-data-local:
	$(mkinstalldirs) $(Global_directory)
	$(mkinstalldirs) $(Config_directory)
	$(INSTALL_DATA) $(srcdir)/panel.global $(Global_directory)/panel
	$(INSTALL_DATA) $(srcdir)/panel.config $(Config_directory)/panel
	$(INSTALL_DATA) $(srcdir)/Applet_Config $(Config_directory)
