
add_subdirectory(icon-factory)
add_subdirectory(implementations)

SET(cairo-dock_SRCS
	cairo-dock-struct.h
	cairo-dock-global-variables.h 
	cairo-dock-icons.c 					cairo-dock-icons.h
	cairo-dock-icon-loader.c 			cairo-dock-icon-loader.h
	cairo-dock-indicator-manager.c 		cairo-dock-indicator-manager.h
	cairo-dock-applications-manager.c 	cairo-dock-applications-manager.h 
	cairo-dock-application-facility.c 	cairo-dock-application-facility.h
	cairo-dock-launcher-manager.c 		cairo-dock-launcher-manager.h
	cairo-dock-applet-manager.c 		cairo-dock-applet-manager.h
	cairo-dock-applet-facility.c 		cairo-dock-applet-facility.h
	cairo-dock-separator-manager.c 		cairo-dock-separator-manager.h
	cairo-dock-modules.c 				cairo-dock-modules.h
	cairo-dock-container.c 				cairo-dock-container.h
	cairo-dock-desklet-manager.c 		cairo-dock-desklet-manager.h
	cairo-dock-desklet-factory.c 		cairo-dock-desklet-factory.h
	cairo-dock-dialog-factory.c 		cairo-dock-dialog-factory.h
	cairo-dock-dialog-manager.c 		cairo-dock-dialog-manager.h
	cairo-dock-flying-container.c 		cairo-dock-flying-container.h
	cairo-dock-dock-manager.c 			cairo-dock-dock-manager.h
	cairo-dock-dock-factory.c 			cairo-dock-dock-factory.h
	cairo-dock-callbacks.c 				cairo-dock-callbacks.h 
	cairo-dock-animations.c 			cairo-dock-animations.h
	cairo-dock-notifications.c 			cairo-dock-notifications.h
	cairo-dock-backends-manager.c 		cairo-dock-backends-manager.h
	cairo-dock-data-renderer.c 			cairo-dock-data-renderer.h
	cairo-dock-file-manager.c 			cairo-dock-file-manager.h
	cairo-dock-themes-manager.c 		cairo-dock-themes-manager.h
	cairo-dock-class-manager.c 			cairo-dock-class-manager.h
	cairo-dock-X-manager.c 				cairo-dock-X-manager.h
	cairo-dock-opengl.c 				cairo-dock-opengl.h
	cairo-dock-opengl-path.c 			cairo-dock-opengl-path.h
	cairo-dock-opengl-font.c 			cairo-dock-opengl-font.h
	cairo-dock-gui-manager.c 			cairo-dock-gui-manager.h
	cairo-dock-gui-factory.c 			cairo-dock-gui-factory.h
	cairo-dock-load.c 					cairo-dock-load.h 
	eggaccelerators.c 					eggaccelerators.h
	cairo-dock-keybinder.c 				cairo-dock-keybinder.h
	cairo-dock-surface-factory.c 		cairo-dock-surface-factory.h
	cairo-dock-dbus.c 					cairo-dock-dbus.h 
	cairo-dock-draw.c 					cairo-dock-draw.h 
	cairo-dock-draw-opengl.c 			cairo-dock-draw-opengl.h
	cairo-dock-log.c 					cairo-dock-log.h
	cairo-dock-X-utilities.c 			cairo-dock-X-utilities.h
	cairo-dock-keyfile-utilities.c 		cairo-dock-keyfile-utilities.h
	cairo-dock-particle-system.c 		cairo-dock-particle-system.h
	cairo-dock-emblem.c 				cairo-dock-emblem.h
	cairo-dock-task.c 					cairo-dock-task.h
	cairo-dock-dock-facility.c 			cairo-dock-dock-facility.h
	cairo-dock-internal-position.c 		cairo-dock-internal-position.h
	cairo-dock-internal-accessibility.c cairo-dock-internal-accessibility.h
	cairo-dock-internal-system.c 		cairo-dock-internal-system.h
	cairo-dock-internal-taskbar.c 		cairo-dock-internal-taskbar.h
	cairo-dock-internal-dialogs.c 		cairo-dock-internal-dialogs.h
	cairo-dock-internal-indicators.c 	cairo-dock-internal-indicators.h
	cairo-dock-internal-views.c 		cairo-dock-internal-views.h
	cairo-dock-internal-labels.c 		cairo-dock-internal-labels.h
	cairo-dock-internal-desklets.c 		cairo-dock-internal-desklets.h
	cairo-dock-internal-background.c 	cairo-dock-internal-background.h
	cairo-dock-internal-icons.c 		cairo-dock-internal-icons.h
	cairo-dock-config.c 				cairo-dock-config.h 
	texture-gradation.h
	texture-blur.h
	
	cairo-dock.c
	cairo-dock-default-view.c 			cairo-dock-default-view.h
	cairo-dock-menu.c 				cairo-dock-menu.h 
	cairo-dock-user-interaction.c 		cairo-dock-user-interaction.h 
	cairo-dock-gui-main.c 				cairo-dock-gui-main.h
	cairo-dock-gui-simple.c 			cairo-dock-gui-simple.h
	cairo-dock-gui-launcher.c 			cairo-dock-gui-launcher.h
	cairo-dock-gui-themes.c 			cairo-dock-gui-themes.h
)

########### compilation ###############

# Make sure the compiler can find include files from the libraries.
include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${XEXTEND_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/src/icon-factory
	${CMAKE_SOURCE_DIR}/src/implementations)

# Make sure the linker can find the libraries.
link_directories(
	${PACKAGE_LIBRARY_DIRS}
	${XEXTEND_LIBRARY_DIRS}
	${CMAKE_SOURCE_DIR}/src/icon-factory
	${CMAKE_SOURCE_DIR}/src/implementations)

# Add executable that is built from the source files.
add_executable(${PROJECT_NAME}
	${cairo-dock_SRCS})

# Link the executable to the librairies.
target_link_libraries(${PROJECT_NAME}
	${PACKAGE_LIBRARIES}
	${XEXTEND_LIBRARIES}
	${LIBCRYPT_LIBS}
	icon-factory
	implementations)

# install the program once it is built.
install(
	TARGETS ${PACKAGE}
	DESTINATION bin)


########### install files ###############

install(FILES cairo-dock.h DESTINATION ${includedir}/cairo-dock)
install(FILES
	cairo-dock-struct.h 			cairo-dock-global-variables.h
	cairo-dock-icons.h 			cairo-dock-icon-loader.h
	cairo-dock-applications-manager.h 	cairo-dock-launcher-manager.h
	cairo-dock-separator-manager.h 	cairo-dock-applet-manager.h
	cairo-dock-backends-manager.h		cairo-dock-notifications.h
	
	cairo-dock-data-renderer.h
	cairo-dock-dock-manager.h		
	cairo-dock-desklet-manager.h
	cairo-dock-dialog-manager.h
	cairo-dock-indicator-manager.h
	cairo-dock-themes-manager.h
	cairo-dock-gui-manager.h
	cairo-dock-file-manager.h
	cairo-dock-X-manager.h
	cairo-dock-class-manager.h
	cairo-dock-opengl.h
	cairo-dock-load.h
	cairo-dock-config.h
	cairo-dock-modules.h
	cairo-dock-container.h
	cairo-dock-dock-factory.h		cairo-dock-callbacks.h
	cairo-dock-desklet-factory.h
	cairo-dock-dialog-factory.h
	cairo-dock-flying-container.h
	
	cairo-dock-applet-multi-instance.h 	cairo-dock-applet-single-instance.h
	cairo-dock-applet-canvas.h		cairo-dock-applet-facility.h
	
	cairo-dock-internal-position.h 	cairo-dock-internal-accessibility.h
	cairo-dock-internal-system.h 	cairo-dock-internal-taskbar.h
	cairo-dock-internal-dialogs.h 	cairo-dock-internal-indicators.h
	cairo-dock-internal-views.h 	cairo-dock-internal-labels.h
	cairo-dock-internal-desklets.h 	cairo-dock-internal-background.h
	cairo-dock-internal-icons.h
	
	cairo-dock-draw.h				cairo-dock-draw-opengl.h
	cairo-dock-opengl-path.h 		cairo-dock-opengl-font.h 
	cairo-dock-particle-system.h	cairo-dock-emblem.h
	cairo-dock-X-utilities.h		cairo-dock-dbus.h
	cairo-dock-keyfile-utilities.h	cairo-dock-surface-factory.h
	cairo-dock-log.h				cairo-dock-keybinder.h
	cairo-dock-application-facility.h	cairo-dock-dock-facility.h
	cairo-dock-task.h
	cairo-dock-animations.h
	cairo-dock-gui-factory.h
	
	DESTINATION ${includedir}/cairo-dock/cairo-dock)



#original Makefile.am contents follow:

#
#SUBDIRS = icon-factory
#
#bin_PROGRAMS = \
#	cairo-dock
#
#if WITH_GLITZ
#have_glitz=-DHAVE_GLITZ="1"
#endif
#
#if WITH_XEXTEND
#have_xextend=-DHAVE_XEXTEND="1"
#endif
#
#if HAVE_LIBCRYPT
#have_libcrypt=-DHAVE_LIBCRYPT=1
#endif
#
#cairo_dock_CFLAGS = \
#	$(PACKAGE_CFLAGS) \
#	$(GLITZ_CFLAGS) \
#	-Iicon-factory \
#	$(have_glitz)\
#	$(XEXTEND_CFLAGS) \
#	$(have_xextend)\
#	$(have_libcrypt) \
#	-DCAIRO_DOCK_SHARE_DATA_DIR=\""$(pkgdatadir)"\"\
#	-DCAIRO_DOCK_SHARE_THEMES_DIR=\""$(pkgdatadir)/themes"\"\
#	-DCAIRO_DOCK_CONF_FILE=\""cairo-dock.conf"\"\
#	-DCAIRO_DOCK_MAIN_DOCK_CONF_FILE=\""main-dock.conf"\"\
#	-DCAIRO_DOCK_DEFAULT_ICON_NAME=\""default-icon.svg"\"\
#	-DCAIRO_DOCK_DEFAULT_APPLI_ICON_NAME=\""default-icon-appli.svg"\"\
#	-DCAIRO_DOCK_DEFAULT_INDICATOR_NAME=\""default-indicator.png"\"\
#	-DCAIRO_DOCK_DEFAULT_CLASS_INDICATOR_NAME=\""default-class-indicator.svg"\"\
#	-DCAIRO_DOCK_VERSION=\""$(VERSION)"\"\
#	-DCAIRO_DOCK_LOCALE_DIR=\""$(datadir)/locale"\"\
#	-DCAIRO_DOCK_GETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\"\
#	-DCAIRO_DOCK_ICON=\""cairo-dock.svg"\"\
#	-DCAIRO_DOCK_LOGO=\""cairo-dock-logo.png"\"\
#	-DCAIRO_DOCK_THEME_CONF_FILE=\""themes.conf"\"\
#	-DCAIRO_DOCK_SIMPLE_CONF_FILE=\""cairo-dock-simple.conf"\"\
#	-DCAIRO_DOCK_MODULES_DIR=\""$(libdir)/$(PACKAGE)"\"\
#	-DCAIRO_DOCK_DATA_DIR=\""cairo-dock"\"\
#	-DCAIRO_DOCK_THEMES_DIR=\""$(CAIRO_DOCK_THEMES_DIR)"\"\
#	-DCAIRO_DOCK_EXTRAS_DIR=\""extras"\"\
#	-DCAIRO_DOCK_CURRENT_THEME_NAME=\""current_theme"\"\
#	-DGL_GLEXT_PROTOTYPES=\""1"\"\
#	-std=c99\
#	-O3\
#	-g -ggdb\
#	-Wstrict-prototypes -Wextra -Wwrite-strings -Wuninitialized -Werror-implicit-function-declaration 
##-Wunreachable-code -Wno-unused-parameter -Wall
#
#cairo_dock_LDFLAGS = \
#	-Wl,--export-dynamic
#
#cairo_dock_LDADD = \
#	icon-factory/libicon.a\
#	$(GLITZ_LIBS) \
#	$(XEXTEND_LIBS) \
#	$(LIBCRYPT_LIBS) \
#	$(PACKAGE_LIBS)
#
#cairo_dock_SOURCES = \
#	cairo-dock-struct.h\
#	cairo-dock-global-variables.h \
#	cairo-dock-icons.h \
#	cairo-dock-icons.c \
#	cairo-dock-applications-manager.c \
#	cairo-dock-applications-manager.h \
#	cairo-dock-application-facility.c\
#	cairo-dock-application-facility.h\
#	cairo-dock-launcher-manager.c\
#	cairo-dock-launcher-manager.h\
#	cairo-dock-applet-manager.c\
#	cairo-dock-applet-manager.h\
#	cairo-dock-applet-facility.c\
#	cairo-dock-applet-facility.h\
#	cairo-dock-separator-manager.c\
#	cairo-dock-separator-manager.h\
#	cairo-dock-modules.c \
#	cairo-dock-modules.h\
#	cairo-dock-container.c\
#	cairo-dock-container.h\
#	cairo-dock-desklet-manager.c\
#	cairo-dock-desklet-manager.h\
#	cairo-dock-desklet-factory.c\
#	cairo-dock-desklet-factory.h\
#	cairo-dock-dialog-factory.c\
#	cairo-dock-dialog-factory.h\
#	cairo-dock-dialog-manager.c\
#	cairo-dock-dialog-manager.h\
#	cairo-dock-flying-container.c\
#	cairo-dock-flying-container.h\
#	cairo-dock-dock-manager.c\
#	cairo-dock-dock-manager.h\
#	cairo-dock-dock-factory.c\
#	cairo-dock-dock-factory.h\
#	cairo-dock-callbacks.c \
#	cairo-dock-callbacks.h \
#	cairo-dock-animations.c\
#	cairo-dock-animations.h\
#	cairo-dock-notifications.c\
#	cairo-dock-notifications.h\
#	cairo-dock-renderer-manager.c\
#	cairo-dock-renderer-manager.h\
#	cairo-dock-data-renderer.c\
#	cairo-dock-data-renderer.h\
#	cairo-dock-file-manager.c\
#	cairo-dock-file-manager.h\
#	cairo-dock-themes-manager.c\
#	cairo-dock-themes-manager.h\
#	cairo-dock-class-manager.c\
#	cairo-dock-class-manager.h\
#	cairo-dock-X-manager.c\
#	cairo-dock-X-manager.h\
#	cairo-dock-opengl.c\
#	cairo-dock-opengl.h\
#	cairo-dock-gui-manager.c\
#	cairo-dock-gui-manager.h\
#	cairo-dock-gui-factory.c\
#	cairo-dock-gui-factory.h\
#	cairo-dock-load.c \
#	cairo-dock-load.h \
#	eggaccelerators.c\
#	eggaccelerators.h\
#	cairo-dock-keybinder.c\
#	cairo-dock-keybinder.h\
#	cairo-dock-surface-factory.c\
#	cairo-dock-surface-factory.h\
#	cairo-dock-dbus.c \
#	cairo-dock-dbus.h \
#	cairo-dock-draw.c \
#	cairo-dock-draw.h \
#	cairo-dock-draw-opengl.c\
#	cairo-dock-draw-opengl.h\
#	cairo-dock-log.c\
#	cairo-dock-log.h\
#	cairo-dock-X-utilities.c\
#	cairo-dock-X-utilities.h\
#	cairo-dock-keyfile-utilities.c\
#	cairo-dock-keyfile-utilities.h\
#	cairo-dock-particle-system.c\
#	cairo-dock-particle-system.h\
#	cairo-dock-emblem.c\
#	cairo-dock-emblem.h\
#	cairo-dock-task.c\
#	cairo-dock-task.h\
#	cairo-dock-dock-facility.c\
#	cairo-dock-dock-facility.h\
#	cairo-dock-gauge.c\
#	cairo-dock-gauge.h\
#	cairo-dock-graph.c\
#	cairo-dock-graph.h\
#	cairo-dock-internal-position.c\
#	cairo-dock-internal-position.h\
#	cairo-dock-internal-accessibility.c\
#	cairo-dock-internal-accessibility.h\
#	cairo-dock-internal-system.c\
#	cairo-dock-internal-system.h\
#	cairo-dock-internal-taskbar.c\
#	cairo-dock-internal-taskbar.h\
#	cairo-dock-internal-hidden-dock.c\
#	cairo-dock-internal-hidden-dock.h\
#	cairo-dock-internal-dialogs.c\
#	cairo-dock-internal-dialogs.h\
#	cairo-dock-internal-indicators.c\
#	cairo-dock-internal-indicators.h\
#	cairo-dock-internal-views.c\
#	cairo-dock-internal-views.h\
#	cairo-dock-internal-labels.c\
#	cairo-dock-internal-labels.h\
#	cairo-dock-internal-desklets.c\
#	cairo-dock-internal-desklets.h\
#	cairo-dock-internal-background.c\
#	cairo-dock-internal-background.h\
#	cairo-dock-internal-icons.c\
#	cairo-dock-internal-icons.h\
#	cairo-dock-config.c \
#	cairo-dock-config.h \
#	texture-gradation.h\
#	texture-blur.h\
#	cairo-dock.c\
#	cairo-dock-default-view.c\
#	cairo-dock-default-view.h\
#	cairo-dock-menu.c \
#	cairo-dock-menu.h \
#	cairo-dock-gui-main.c\
#	cairo-dock-gui-main.h\
#	cairo-dock-gui-simple.c\
#	cairo-dock-gui-simple.h\
#	cairo-dock-gui-launcher.c\
#	cairo-dock-gui-launcher.h\
#	cairo-dock-gui-themes.c\
#	cairo-dock-gui-themes.h
#	
#
#cairo_dockincludedir =\
#	$(includedir)/cairo-dock
#
#cairo_dockinclude_HEADERS =\
#	cairo-dock.h
#
#cairo_docksubincludedir =\
#	$(includedir)/cairo-dock/cairo-dock
#
#cairo_docksubinclude_HEADERS =\
#	cairo-dock-struct.h\
#	cairo-dock-callbacks.h\
#	cairo-dock-dbus.h\
#	cairo-dock-draw.h\
#	cairo-dock-config.h\
#	cairo-dock-load.h\
#	cairo-dock-icons.h\
#	cairo-dock-modules.h\
#	cairo-dock-global-variables.h\
#	cairo-dock-applications-manager.h\
#	cairo-dock-launcher-manager.h\
#	cairo-dock-separator-manager.h\
#	cairo-dock-applet-manager.h\
#	cairo-dock-application-facility.h\
#	cairo-dock-keyfile-utilities.h\
#	cairo-dock-dock-factory.h\
#	cairo-dock-themes-manager.h\
#	cairo-dock-animations.h\
#	cairo-dock-notifications.h\
#	cairo-dock-dialog-factory.h\
#	cairo-dock-dialog-manager.h\
#	cairo-dock-surface-factory.h\
#	cairo-dock-renderer-manager.h\
#	cairo-dock-data-renderer.h\
#	cairo-dock-default-view.h\
#	cairo-dock-applet-facility.h\
#	cairo-dock-file-manager.h\
#	cairo-dock-keybinder.h\
#	cairo-dock-desklet-manager.h\
#	cairo-dock-desklet-factory.h\
#	cairo-dock-log.h\
#	cairo-dock-X-utilities.h\
#	cairo-dock-X-manager.h\
#	cairo-dock-gauge.h\
#	cairo-dock-dock-manager.h\
#	cairo-dock-class-manager.h\
#	cairo-dock-emblem.h\
#	cairo-dock-applet-single-instance.h\
#	cairo-dock-applet-multi-instance.h\
#	cairo-dock-applet-canvas.h\
#	cairo-dock-flying-container.h\
#	cairo-dock-graph.h\
#	cairo-dock-opengl.h\
#	cairo-dock-draw-opengl.h\
#	cairo-dock-particle-system.h\
#	cairo-dock-gui-factory.h\
#	cairo-dock-gui-manager.h\
#	cairo-dock-internal-position.h\
#	cairo-dock-internal-accessibility.h\
#	cairo-dock-internal-system.h\
#	cairo-dock-internal-taskbar.h\
#	cairo-dock-internal-hidden-dock.h\
#	cairo-dock-internal-dialogs.h\
#	cairo-dock-internal-indicators.h\
#	cairo-dock-internal-views.h\
#	cairo-dock-internal-labels.h\
#	cairo-dock-internal-desklets.h\
#	cairo-dock-internal-background.h\
#	cairo-dock-internal-icons.h\
#	cairo-dock-data-renderer.h\
#	cairo-dock-dock-facility.h\
#	cairo-dock-task.h\
#	cairo-dock-container.h\
#	cairo-dock-menu.h
#
## 
## lib_LTLIBRARIES = \
## 	libcairo-dock.la
## 
## libcairo_dock_la_SOURCES = \
## 	cairo-dock-struct.h\
## 	cairo-dock-icons.h\
## 	cairo-dock-icons.c
## 
## libcairo_dock_la_CFLAGS = \
## 	$(cairo_dock_CFLAGS)
## 
## libcairo_dock_la_LIBADD = \
## 	$(PACKAGE_LIBS) -lm
## 
## libcairo_dock_la_LDFLAGS =\
## # 	-avoid-version -module
