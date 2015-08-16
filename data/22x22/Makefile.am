size = 22x22

iconsdir = $(datadir)/icons/hicolor/$(size)/apps

icons_DATA =					\
	gnome-panel-clock.png			\
	gnome-panel-fish.png			\
	gnome-panel-force-quit.png		\
	gnome-panel-hibernate.png		\
	gnome-panel-launcher.png		\
	gnome-panel-notification-area.png	\
	gnome-panel-separator.png		\
	gnome-panel-suspend.png			\
	gnome-panel.png				\
	gnome-panel-window-list.png		\
	gnome-panel-window-menu.png		\
	gnome-panel-workspace-switcher.png

icons_SOURCE =					\
	gnome-panel-clock.svg			\
	gnome-panel-fish.xcf.bz2		\
	gnome-panel-force-quit.xcf.bz2		\
	gnome-panel-launcher.svg		\
	gnome-panel-notification-area.svg	\
	gnome-panel-separator.svg		\
	gnome-panel.svg				\
	gnome-panel-window-list.xcf.bz2		\
	gnome-panel-window-menu.xcf.bz2		\
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
	$(icons_DATA)	\
	$(icons_SOURCE)

-include $(top_srcdir)/git.mk
