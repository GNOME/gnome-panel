size = 24x24

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
