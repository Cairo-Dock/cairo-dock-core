
SET(icon_SRCS
	cairo-dock-launcher-factory.c 	cairo-dock-launcher-factory.h
	cairo-dock-application-factory.c 	cairo-dock-application-factory.h
	cairo-dock-separator-factory.c 	cairo-dock-separator-factory.h
	cairo-dock-applet-factory.c 		cairo-dock-applet-factory.h
	cairo-dock-desktop-file-factory.c 	cairo-dock-desktop-file-factory.h
)

add_library(icon-factory STATIC ${icon_SRCS})

link_directories(
	${PACKAGE_LIBRARY_DIRS})

include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src)

########### install files ###############

install(FILES
	cairo-dock-application-factory.h 	cairo-dock-launcher-factory.h
	cairo-dock-separator-factory.h 	cairo-dock-applet-factory.h
	cairo-dock-desktop-file-factory.h
	DESTINATION ${includedir}/cairo-dock/icon-factory)
