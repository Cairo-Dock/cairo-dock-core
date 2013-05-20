
SET(icon_SRCS
	cairo-dock-launcher-factory.c 	cairo-dock-launcher-factory.h
	cairo-dock-separator-factory.c 	cairo-dock-separator-factory.h
	cairo-dock-applet-factory.c 		cairo-dock-applet-factory.h
	cairo-dock-desktop-file-factory.c 	cairo-dock-desktop-file-factory.h
)

add_library(icon-factory STATIC ${icon_SRCS})
add_definitions (-fPIC)

link_directories(
	${PACKAGE_LIBRARY_DIRS}
	${GTK_LIBRARY_DIRS})

include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${GTK_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src/gldit)

########### install files ###############

install(FILES
	cairo-dock-launcher-factory.h
	cairo-dock-separator-factory.h
	cairo-dock-applet-factory.h
	cairo-dock-desktop-file-factory.h
	DESTINATION ${includedir}/cairo-dock/icon-factory)
