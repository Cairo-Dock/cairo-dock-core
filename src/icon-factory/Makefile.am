
noinst_LIBRARIES = libicon.a

#INCLUDES = -I@top_srcdir@/

#SUBDIRS = .

libicon_a_CFLAGS = \
	$(PACKAGE_CFLAGS) \
	-I.. \
	-DCAIRO_DOCK_SHARE_DATA_DIR=\""$(pkgdatadir)"\"\
	-DCAIRO_DOCK_SHARE_THEMES_DIR=\""$(pkgdatadir)/themes"\"\
	-DCAIRO_DOCK_CONF_FILE=\""cairo-dock.conf"\"\
	-DCAIRO_DOCK_LAUNCHER_CONF_FILE=\""launcher.desktop"\"\
	-DCAIRO_DOCK_FILE_CONF_FILE=\""file.desktop"\"\
	-DCAIRO_DOCK_CONTAINER_CONF_FILE=\""container.desktop"\"\
	-DCAIRO_DOCK_SEPARATOR_CONF_FILE=\""separator.desktop"\"\
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
	-DCAIRO_DOCK_THEME_CONF_FILE=\""themes.conf"\"\
	-DCAIRO_DOCK_SIMPLE_CONF_FILE=\""cairo-dock-simple.conf"\"\
	-DCAIRO_DOCK_MODULES_DIR=\""$(libdir)/$(PACKAGE)"\"\
	-DCAIRO_DOCK_DATA_DIR=\""cairo-dock"\"\
	-DCAIRO_DOCK_THEMES_DIR=\""$(CAIRO_DOCK_THEMES_DIR)"\"\
	-DCAIRO_DOCK_EXTRAS_DIR=\""extras"\"\
	-DCAIRO_DOCK_CURRENT_THEME_NAME=\""current_theme"\"\
	-DGL_GLEXT_PROTOTYPES=\""1"\"\
	-std=c99\
	-O3\
	-g -ggdb\
	-Wstrict-prototypes -Wextra -Wwrite-strings -Wuninitialized -Werror-implicit-function-declaration 
#-Wunreachable-code -Wno-unused-parameter -Wall

#libicon_a_LIBADD = \
#	$(PACKAGE_LIBS)

libicon_a_SOURCES = \
	cairo-dock-launcher-factory.c\
	cairo-dock-launcher-factory.h\
	cairo-dock-application-factory.c\
	cairo-dock-application-factory.h\
	cairo-dock-separator-factory.c\
	cairo-dock-separator-factory.h\
	cairo-dock-applet-factory.c\
	cairo-dock-applet-factory.h\
	cairo-dock-desktop-file-factory.c\
	cairo-dock-desktop-file-factory.h
	

libicon_aincludedir =\
	$(includedir)/cairo-dock

libicon_asubincludedir =\
	$(includedir)/cairo-dock/icon-factory

libicon_asubinclude_HEADERS =\
	cairo-dock-application-factory.h\
	cairo-dock-launcher-factory.h\
	cairo-dock-separator-factory.h\
	cairo-dock-applet-factory.h\
	cairo-dock-desktop-file-factory.h
