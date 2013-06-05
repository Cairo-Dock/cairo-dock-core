
########### sources ###############

SET(MODULE_SRCS
	applet-struct.h
	applet-init.c 			applet-init.h
	applet-config.c 			applet-config.h
	applet-notifications.c 		applet-notifications.h
	applet-tips-dialog.c 		applet-tips-dialog.h
	applet-composite.c 		applet-composite.h
)

add_library(${PACKAGE_HELP} SHARED ${MODULE_SRCS})

########### compil ###############
add_definitions (-DMY_APPLET_SHARE_DATA_DIR="${helpdatadir}")
add_definitions (-DMY_APPLET_PREVIEW_FILE="icon.svg")
add_definitions (-DMY_APPLET_CONF_FILE="Help.conf")
add_definitions (-DMY_APPLET_USER_DATA_DIR="Help")
add_definitions (-DMY_APPLET_VERSION="${VERSION_HELP}")
add_definitions (-DMY_APPLET_GETTEXT_DOMAIN="${GETTEXT_HELP}")
add_definitions (-DMY_APPLET_DOCK_VERSION="${dock_version}")
add_definitions (-DMY_APPLET_ICON_FILE="icon.svg")
### uncomment the following line to allow multi-instance applet.
#add_definitions (-DCD_APPLET_MULTI_INSTANCE="1")
### uncomment the following line to allow extended OpenGL drawing.
#add_definitions (-DGL_GLEXT_PROTOTYPES="1")

include_directories (
	${PACKAGE_INCLUDE_DIRS}
	${GTK_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/src/gldit
	${CMAKE_SOURCE_DIR}/src/implementations)

link_directories (
	${PACKAGE_LIBRARY_DIRS}
	${GTK_LIBRARY_DIRS})

target_link_libraries (${PACKAGE_HELP}
	${PACKAGE_LIBRARIES}
	${GTK_LIBRARIES})

########### install files ###############

install(TARGETS ${PACKAGE_HELP} DESTINATION ${pluginsdir})
