EXTRA_DIST = \
	panel.global		\
	panel.config		\
	Applet_Config

Global_directory = $(sysconfdir)/gnome/panel-config
Config_directory = $(sysconfdir)/gnome/panel-config/panel.d/default

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(Global_directory)
	$(mkinstalldirs) $(DESTDIR)$(Config_directory)
	$(INSTALL_DATA) $(srcdir)/panel.global $(DESTDIR)$(Global_directory)/panel
	$(INSTALL_DATA) $(srcdir)/panel.config $(DESTDIR)$(Config_directory)/panel
	$(INSTALL_DATA) $(srcdir)/Applet_Config $(DESTDIR)$(Config_directory)
