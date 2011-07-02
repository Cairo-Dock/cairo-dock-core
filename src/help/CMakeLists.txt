
SET(help_SRCS
	applet-struct.h
	applet-init.c 			applet-init.h
	applet-config.c 			applet-config.h
	applet-notifications.c 		applet-notifications.h
	applet-tips-dialog.c 		applet-tips-dialog.h
	applet-composite.c 		applet-composite.h
	cairo-dock-help.c 		cairo-dock-help.h
)

add_library(help STATIC ${help_SRCS})
add_definitions (-fPIC)

add_definitions (-DMY_APPLET_SHARE_DATA_DIR="${pkgdatadir}")
add_definitions (-DMY_APPLET_PREVIEW_FILE="help-preview.jpg")
add_definitions (-DMY_APPLET_CONF_FILE="Help.conf")
add_definitions (-DMY_APPLET_USER_DATA_DIR="Help")
add_definitions (-DMY_APPLET_VERSION="0.9.99")
add_definitions (-DMY_APPLET_GETTEXT_DOMAIN="${GETTEXT_PACKAGE}")
add_definitions (-DMY_APPLET_DOCK_VERSION="${VERSION}")
add_definitions (-DMY_APPLET_ICON_FILE="icon-help.svg")

link_directories(
	${PACKAGE_LIBRARY_DIRS})

include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/src/icon-factory
	${CMAKE_SOURCE_DIR}/src/gldit)
