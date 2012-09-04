
add_subdirectory (icon-factory)
add_subdirectory (gldit)
add_subdirectory (implementations)
add_subdirectory (help)

SET(cairo_dock_SRCS
	cairo-dock.c
	cairo-dock-menu.c 					cairo-dock-menu.h 
	cairo-dock-user-interaction.c 		cairo-dock-user-interaction.h 
	cairo-dock-gui-advanced.c 			cairo-dock-gui-advanced.h
	cairo-dock-gui-simple.c 			cairo-dock-gui-simple.h
	cairo-dock-gui-backend.c 			cairo-dock-gui-backend.h
	cairo-dock-gui-commons.c 			cairo-dock-gui-commons.h
	cairo-dock-widget.c 				cairo-dock-widget.h
	cairo-dock-widget-themes.c 			cairo-dock-widget-themes.h
	cairo-dock-widget-items.c 			cairo-dock-widget-items.h
	cairo-dock-widget-config.c 			cairo-dock-widget-config.h
	cairo-dock-widget-plugins.c 		cairo-dock-widget-plugins.h
	cairo-dock-widget-module.c			cairo-dock-widget-module.h
	cairo-dock-widget-config-group.c 	cairo-dock-widget-config-group.h
	cairo-dock-widget-shortkeys.c		cairo-dock-widget-shortkeys.h
)

########### compilation ###############

# Make sure the compiler can find include files from the libraries.
include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${GTK_INCLUDE_DIRS}
	${XEXTEND_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/src/gldit
	${CMAKE_SOURCE_DIR}/src/icon-factory
	${CMAKE_SOURCE_DIR}/src/implementations)

# Make sure the linker can find the libraries.
link_directories(
	${PACKAGE_LIBRARY_DIRS}
	${GTK_LIBRARY_DIRS}
	${XEXTEND_LIBRARY_DIRS}
	${CMAKE_SOURCE_DIR}/src/gldit
	${CMAKE_SOURCE_DIR}/src/help)

# Add executable that is built from the source files.
add_executable (${PROJECT_NAME}
	${cairo_dock_SRCS} )

# Link the executable to the librairies.
target_link_libraries (${PROJECT_NAME}
	${PACKAGE_LIBRARIES}
	${GTK_LIBRARIES}
	${XEXTEND_LIBRARIES}
	${LIBCRYPT_LIBS}
	gldi
	icon-factory
	implementations
	help)

# install the program once it is built.
install(
	TARGETS ${PACKAGE}
	DESTINATION bin)
