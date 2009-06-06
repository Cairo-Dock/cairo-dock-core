
SUBDIRS = .

bin_PROGRAMS = \
	cairo-dock


if WITH_GLITZ
have_glitz=-DHAVE_GLITZ="1"
endif

if WITH_XEXTEND
have_xextend=-DHAVE_XEXTEND="1"
endif

if HAVE_LIBCRYPT
have_libcrypt=-DHAVE_LIBCRYPT=1
endif

cairo_dock_CFLAGS = \
	$(PACKAGE_CFLAGS) \
	$(GLITZ_CFLAGS) \
	$(have_glitz)\
	$(XEXTEND_CFLAGS) \
	$(have_xextend)\
	$(have_libcrypt) \
	-DCAIRO_DOCK_SHARE_DATA_DIR=\""$(pkgdatadir)"\"\
	-DCAIRO_DOCK_SHARE_THEMES_DIR=\""$(pkgdatadir)/themes"\"\
	-DCAIRO_DOCK_MODULES_DIR=\""$(libdir)/$(PACKAGE)"\"\
	-DCAIRO_DOCK_CONF_FILE=\""cairo-dock.conf"\"\
	-DCAIRO_DOCK_LAUNCHER_CONF_FILE=\""launcher.conf"\"\
	-DCAIRO_DOCK_CONTAINER_CONF_FILE=\""container.conf"\"\
	-DCAIRO_DOCK_SEPARATOR_CONF_FILE=\""separator.conf"\"\
	-DCAIRO_DOCK_THEME_CONF_FILE=\""themes.conf"\"\
	-DCAIRO_DOCK_MAIN_DOCK_CONF_FILE=\""main-dock.conf"\"\
	-DCAIRO_DOCK_DEFAULT_ICON_NAME=\""default-icon.svg"\"\
	-DCAIRO_DOCK_DEFAULT_APPLI_ICON_NAME=\""default-icon-appli.svg"\"\
	-DCAIRO_DOCK_DEFAULT_INDICATOR_NAME=\""default-indicator.png"\"\
	-DCAIRO_DOCK_DEFAULT_CLASS_INDICATOR_NAME=\""default-class-indicator.svg"\"\
	-DCAIRO_DOCK_VERSION=\""$(VERSION)"\"\
	-DCAIRO_DOCK_LOCALE_DIR=\""$(datadir)/locale"\"\
	-DCAIRO_DOCK_GETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\"\
	-DCAIRO_DOCK_ICON=\""cairo-dock.svg"\"\
	-DCAIRO_DOCK_LOGO=\""cairo-dock-logo.png"\"\
	-DCAIRO_DOCK_THEMES_DIR=\""$(CAIRO_DOCK_THEMES_DIR)"\"\
	-DGL_GLEXT_PROTOTYPES=\""1"\"\
	-std=c99\
	-O3\
	-g -ggdb\
	-Wno-unused-parameter
#	-Wall -Wunreachable-code -Wstrict-prototypes -Wextra -Wwrite-strings -Wuninitialized -Werror-implicit-function-declaration

cairo_dock_LDFLAGS = \
	-Wl,--export-dynamic

cairo_dock_LDADD = \
	$(GLITZ_LIBS) \
	$(XEXTEND_LIBS) \
	$(LIBCRYPT_LIBS) \
	$(PACKAGE_LIBS)

cairo_dock_SOURCES = \
	cairo-dock-struct.h\
	cairo-dock-global-variables.h \
	cairo-dock-callbacks.c \
	cairo-dock-callbacks.h \
	cairo-dock.c \
	cairo-dock-dbus.c \
	cairo-dock-dbus.h \
	cairo-dock-draw.c \
	cairo-dock-draw.h \
	cairo-dock-config.c \
	cairo-dock-config.h \
	cairo-dock-load.c \
	cairo-dock-load.h \
	cairo-dock-icons.h \
	cairo-dock-icons.c \
	cairo-dock-applications-manager.c \
	cairo-dock-applications-manager.h \
	cairo-dock-menu.c \
	cairo-dock-menu.h \
	cairo-dock-modules.c \
	cairo-dock-modules.h\
	cairo-dock-launcher-factory.c\
	cairo-dock-launcher-factory.h\
	cairo-dock-application-factory.c\
	cairo-dock-application-factory.h\
	cairo-dock-applet-factory.c\
	cairo-dock-applet-factory.h\
	cairo-dock-separator-factory.c\
	cairo-dock-separator-factory.h\
	cairo-dock-desktop-file-factory.c\
	cairo-dock-desktop-file-factory.h\
	cairo-dock-keyfile-utilities.c\
	cairo-dock-keyfile-utilities.h\
	cairo-dock-dock-factory.c\
	cairo-dock-dock-factory.h\
	cairo-dock-animations.c\
	cairo-dock-animations.h\
	cairo-dock-themes-manager.c\
	cairo-dock-themes-manager.h\
	cairo-dock-notifications.c\
	cairo-dock-notifications.h\
	cairo-dock-dialogs.c\
	cairo-dock-dialogs.h\
	cairo-dock-surface-factory.c\
	cairo-dock-surface-factory.h\
	cairo-dock-renderer-manager.c\
	cairo-dock-renderer-manager.h\
	cairo-dock-default-view.c\
	cairo-dock-default-view.h\
	cairo-dock-applet-facility.c\
	cairo-dock-applet-facility.h\
	cairo-dock-file-manager.c\
	cairo-dock-file-manager.h\
	eggaccelerators.c\
	eggaccelerators.h\
	cairo-dock-keybinder.c\
	cairo-dock-keybinder.h\
	cairo-dock-desklet.c\
	cairo-dock-desklet.h\
	cairo-dock-log.c\
	cairo-dock-log.h\
	cairo-dock-X-utilities.c\
	cairo-dock-X-utilities.h\
	cairo-dock-gauge.c\
	cairo-dock-gauge.h\
	cairo-dock-gauge2.c\
	cairo-dock-gauge2.h\
	cairo-dock-dock-manager.c\
	cairo-dock-dock-manager.h\
	cairo-dock-class-manager.c\
	cairo-dock-class-manager.h\
	cairo-dock-emblem.c\
	cairo-dock-emblem.h\
	cairo-dock-flying-container.c\
	cairo-dock-flying-container.h\
	cairo-dock-graph.c\
	cairo-dock-graph.h\
	cairo-dock-draw-opengl.c\
	cairo-dock-draw-opengl.h\
	cairo-dock-particle-system.c\
	cairo-dock-particle-system.h\
	cairo-dock-gui-factory.c\
	cairo-dock-gui-factory.h\
	cairo-dock-gui-callbacks.c\
	cairo-dock-gui-callbacks.h\
	cairo-dock-gui-manager.c\
	cairo-dock-gui-manager.h\
	cairo-dock-gui-filter.c\
	cairo-dock-gui-filter.h\
	cairo-dock-internal-position.c\
	cairo-dock-internal-position.h\
	cairo-dock-internal-accessibility.c\
	cairo-dock-internal-accessibility.h\
	cairo-dock-internal-system.c\
	cairo-dock-internal-system.h\
	cairo-dock-internal-taskbar.c\
	cairo-dock-internal-taskbar.h\
	cairo-dock-internal-hidden-dock.c\
	cairo-dock-internal-hidden-dock.h\
	cairo-dock-internal-dialogs.c\
	cairo-dock-internal-dialogs.h\
	cairo-dock-internal-indicators.c\
	cairo-dock-internal-indicators.h\
	cairo-dock-internal-views.c\
	cairo-dock-internal-views.h\
	cairo-dock-internal-labels.c\
	cairo-dock-internal-labels.h\
	cairo-dock-internal-desklets.c\
	cairo-dock-internal-desklets.h\
	cairo-dock-internal-background.c\
	cairo-dock-internal-background.h\
	cairo-dock-internal-icons.c\
	cairo-dock-internal-icons.h\
	cairo-dock-data-renderer.c\
	cairo-dock-data-renderer.h\
	cairo-dock-dock-facility.c\
	cairo-dock-dock-facility.h\
	cairo-dock-container.c\
	cairo-dock-container.h\
	texture-gradation.h
	

cairo_dockincludedir =\
	$(includedir)/cairo-dock

cairo_dockinclude_HEADERS =\
	cairo-dock.h

cairo_docksubincludedir =\
	$(includedir)/cairo-dock/cairo-dock

cairo_docksubinclude_HEADERS =\
	cairo-dock-struct.h\
	cairo-dock-callbacks.h\
	cairo-dock-dbus.h\
	cairo-dock-draw.h\
	cairo-dock-config.h\
	cairo-dock-load.h\
	cairo-dock-icons.h\
	cairo-dock-applications-manager.h\
	cairo-dock-menu.h\
	cairo-dock-modules.h\
	cairo-dock-global-variables.h\
	cairo-dock-launcher-factory.h\
	cairo-dock-application-factory.h\
	cairo-dock-applet-factory.h\
	cairo-dock-separator-factory.h\
	cairo-dock-desktop-file-factory.h\
	cairo-dock-keyfile-utilities.h\
	cairo-dock-dock-factory.h\
	cairo-dock-themes-manager.h\
	cairo-dock-animations.h\
	cairo-dock-notifications.h\
	cairo-dock-dialogs.h\
	cairo-dock-surface-factory.h\
	cairo-dock-renderer-manager.h\
	cairo-dock-data-renderer.h\
	cairo-dock-default-view.h\
	cairo-dock-applet-facility.h\
	cairo-dock-file-manager.h\
	cairo-dock-keybinder.h\
	cairo-dock-desklet.h\
	cairo-dock-log.h\
	cairo-dock-X-utilities.h\
	cairo-dock-gauge.h\
	cairo-dock-gauge2.h\
	cairo-dock-dock-manager.h\
	cairo-dock-class-manager.h\
	cairo-dock-emblem.h\
	cairo-dock-applet-single-instance.h\
	cairo-dock-applet-multi-instance.h\
	cairo-dock-applet-canvas.h\
	cairo-dock-flying-container.h\
	cairo-dock-graph.h\
	cairo-dock-draw-opengl.h\
	cairo-dock-particle-system.h\
	cairo-dock-gui-factory.h\
	cairo-dock-gui-callbacks.h\
	cairo-dock-gui-manager.h\
	cairo-dock-gui-filter.h\
	cairo-dock-internal-position.h\
	cairo-dock-internal-accessibility.h\
	cairo-dock-internal-system.h\
	cairo-dock-internal-taskbar.h\
	cairo-dock-internal-hidden-dock.h\
	cairo-dock-internal-dialogs.h\
	cairo-dock-internal-indicators.h\
	cairo-dock-internal-views.h\
	cairo-dock-internal-labels.h\
	cairo-dock-internal-desklets.h\
	cairo-dock-internal-background.h\
	cairo-dock-internal-icons.h\
	cairo-dock-data-renderer.h\
	cairo-dock-dock-facility.h\
	cairo-dock-container.h
