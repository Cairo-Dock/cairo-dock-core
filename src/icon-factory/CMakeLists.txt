
SET(icon_STAT_SRCS
	cairo-dock-launcher-factory.c 		cairo-dock-launcher-factory.h
	cairo-dock-application-factory.c 	cairo-dock-application-factory.h
	cairo-dock-separator-factory.c 		cairo-dock-separator-factory.h
	cairo-dock-applet-factory.c 		cairo-dock-applet-factory.h
	cairo-dock-desktop-file-factory.c 	cairo-dock-desktop-file-factory.h
)

add_library(icon STATIC ${icon_STAT_SRCS})

link_directories(
	${PACKAGE_LIBRARY_DIRS})

include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src)

########### install files ###############

#install(FILES  DESTINATION /include/cairo-dock)
install(FILES  cairo-dock-application-factory.h 	cairo-dock-launcher-factory.h 	cairo-dock-separator-factory.h 	cairo-dock-applet-factory.h 	cairo-dock-desktop-file-factory.h DESTINATION ${includedir}/cairo-dock/cairo-dock)



#original Makefile.am contents follow:

#
#noinst_LIBRARIES = libicon.a
#
#libicon_a_CFLAGS = \
#	$(PACKAGE_CFLAGS) \
#	-I.. \
#	-DCAIRO_DOCK_SHARE_DATA_DIR=\""$(pkgdatadir)"\"\
#	-DCAIRO_DOCK_LAUNCHER_CONF_FILE=\""launcher.desktop"\"\
#	-DCAIRO_DOCK_FILE_CONF_FILE=\""file.desktop"\"\
#	-DCAIRO_DOCK_CONTAINER_CONF_FILE=\""container.desktop"\"\
#	-DCAIRO_DOCK_SEPARATOR_CONF_FILE=\""separator.desktop"\"\
#	-DCAIRO_DOCK_VERSION=\""$(VERSION)"\"\
#	-std=c99\
#	-O3\
#	-g -ggdb\
#	-Wstrict-prototypes -Wextra -Wwrite-strings -Wuninitialized -Werror-implicit-function-declaration
##-Wunreachable-code -Wno-unused-parameter -Wall
#
##libicon_a_LIBADD = \
##	$(PACKAGE_LIBS)
#
#libicon_a_SOURCES = \
#	cairo-dock-launcher-factory.c\
#	cairo-dock-launcher-factory.h\
#	cairo-dock-application-factory.c\
#	cairo-dock-application-factory.h\
#	cairo-dock-separator-factory.c\
#	cairo-dock-separator-factory.h\
#	cairo-dock-applet-factory.c\
#	cairo-dock-applet-factory.h\
#	cairo-dock-desktop-file-factory.c\
#	cairo-dock-desktop-file-factory.h
#	
#
#libicon_aincludedir =\
#	$(includedir)/cairo-dock
#
#libicon_asubincludedir =\
#	$(includedir)/cairo-dock/icon-factory
#
#libicon_asubinclude_HEADERS =\
#	cairo-dock-application-factory.h\
#	cairo-dock-launcher-factory.h\
#	cairo-dock-separator-factory.h\
#	cairo-dock-applet-factory.h\
#	cairo-dock-desktop-file-factory.h
