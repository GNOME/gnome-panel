size = scalable

iconsdir = $(datadir)/icons/hicolor/$(size)/apps

icons_DATA =					\
	gnome-panel-clock.svg			\
	gnome-panel-fish.svg			\
	gnome-panel-force-quit.svg		\
	gnome-panel-hibernate.svg		\
	gnome-panel-launcher.svg		\
	gnome-panel-notification-area.svg	\
	gnome-panel-separator.svg		\
	gnome-panel-suspend.svg			\
	gnome-panel.svg				\
	gnome-panel-window-list.svg		\
	gnome-panel-window-menu.svg		\
	gnome-panel-workspace-switcher.svg

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated. After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST =		\
	$(icons_DATA)

-include $(top_srcdir)/git.mk
