
SET(core_lib_SRCS
	cairo-dock-struct.h
	cairo-dock-global-variables.h 
	cairo-dock-core.c 					cairo-dock-core.h
	cairo-dock-manager.c 				cairo-dock-manager.h
	cairo-dock-object.c 				cairo-dock-object.h
	# icons
	cairo-dock-icon-manager.c 			cairo-dock-icon-manager.h
	cairo-dock-icon-factory.c 			cairo-dock-icon-factory.h
	cairo-dock-icon-facility.c 			cairo-dock-icon-facility.h
	cairo-dock-indicator-manager.c 		cairo-dock-indicator-manager.h
	cairo-dock-applications-manager.c 	cairo-dock-applications-manager.h
	cairo-dock-application-facility.c 	cairo-dock-applications-priv.h
	cairo-dock-launcher-manager.c 		cairo-dock-launcher-manager.h
	cairo-dock-stack-icon-manager.c 	cairo-dock-stack-icon-manager.h
	cairo-dock-class-icon-manager.c 	cairo-dock-class-icon-manager.h
	cairo-dock-user-icon-manager.c 		cairo-dock-user-icon-manager.h
	cairo-dock-applet-manager.c 		cairo-dock-applet-manager.h
	cairo-dock-applet-facility.c 		cairo-dock-applet-facility.h
	cairo-dock-separator-manager.c 		cairo-dock-separator-manager.h
	cairo-dock-module-manager.c 		cairo-dock-module-manager.h             cairo-dock-module-manager-priv.h
	cairo-dock-module-instance-manager.c cairo-dock-module-instance-manager.h
	# containers
	cairo-dock-container.c 				cairo-dock-container.h                  cairo-dock-container-priv.h
	cairo-dock-desklet-manager.c 		cairo-dock-desklet-manager.h
	cairo-dock-desklet-factory.c 		cairo-dock-desklet-factory.h
	cairo-dock-dialog-factory.c 		cairo-dock-dialog-factory.h
	cairo-dock-dialog-manager.c 		cairo-dock-dialog-manager.h
	cairo-dock-dialog-priv.h
	cairo-dock-flying-container.c 		cairo-dock-flying-container.h
	cairo-dock-dock-manager.c 			cairo-dock-dock-manager.h
	cairo-dock-dock-factory.c 			cairo-dock-dock-factory.h
	cairo-dock-dock-facility.c 			cairo-dock-dock-facility.h
	cairo-dock-dock-visibility.c 		cairo-dock-dock-visibility.h
	cairo-dock-dock-priv.h
	cairo-dock-animations.c 			cairo-dock-animations.h
	cairo-dock-backends-manager.c 		cairo-dock-backends-manager.h
	cairo-dock-data-renderer.c 			cairo-dock-data-renderer.h
	cairo-dock-data-renderer-manager.c 	cairo-dock-data-renderer-manager.h
	cairo-dock-file-manager.c 			cairo-dock-file-manager.h
	cairo-dock-themes-manager.c 		cairo-dock-themes-manager.h
	cairo-dock-class-manager.c 			cairo-dock-class-manager.h              cairo-dock-class-manager-priv.h
	cairo-dock-desktop-manager.c		cairo-dock-desktop-manager.h
	cairo-dock-windows-manager.c		cairo-dock-windows-manager.h            cairo-dock-windows-manager-priv.h
	cairo-dock-image-buffer.c			cairo-dock-image-buffer.h 
	cairo-dock-opengl.c 				cairo-dock-opengl.h
	cairo-dock-opengl-path.c 			cairo-dock-opengl-path.h
	cairo-dock-opengl-font.c 			cairo-dock-opengl-font.h
	cairo-dock-surface-factory.c 		cairo-dock-surface-factory.h
	cairo-dock-draw.c 					cairo-dock-draw.h 
	cairo-dock-draw-opengl.c 			cairo-dock-draw-opengl.h
	# utilities
	cairo-dock-log.c 					cairo-dock-log.h
	cairo-dock-gui-manager.c 			cairo-dock-gui-manager.h
	cairo-dock-gui-factory.c 			cairo-dock-gui-factory.h
	cairo-dock-keybinder.c 				cairo-dock-keybinder.h
	cairo-dock-dbus.c 					cairo-dock-dbus.h                       cairo-dock-dbus-priv.h
	cairo-dock-keyfile-utilities.c 		cairo-dock-keyfile-utilities.h
	cairo-dock-packages.c 				cairo-dock-packages.h
	cairo-dock-particle-system.c 		cairo-dock-particle-system.h
	cairo-dock-overlay.c 				cairo-dock-overlay.h
	cairo-dock-task.c 					cairo-dock-task.h
	cairo-dock-config.c 				cairo-dock-config.h
	cairo-dock-utils.c 					cairo-dock-utils.h
	cairo-dock-menu.c 					cairo-dock-menu.h
	cairo-dock-style-manager.c 			cairo-dock-style-manager.h
	cairo-dock-style-facility.c 		cairo-dock-style-facility.h
	cairo-dock-desktop-file-db.c		cairo-dock-desktop-file-db.h
	cdwindow.c                          cdwindow.h
	texture-gradation.h
	gtk3imagemenuitem.c 				gtk3imagemenuitem.h
)

########### compilation ###############

# Make sure the compiler can find include files from the libraries.
include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${GTK_INCLUDE_DIRS}
	${XEXTEND_INCLUDE_DIRS}
	${XINERAMA_INCLUDE_DIRS}
	${EGL_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src/gldit
	${CMAKE_BINARY_DIR}/src/gldit
	${CMAKE_SOURCE_DIR}/src/implementations)

# Make sure the linker can find the libraries.
link_directories(
	${PACKAGE_LIBRARY_DIRS}
	${GTK_LIBRARY_DIRS}
	${EGL_LIBRARY_DIRS}
	${WAYLAND_LIBRARY_DIRS}
	${WAYLAND_EGL_LIBRARY_DIRS}
	${XEXTEND_LIBRARY_DIRS}
	${XINERAMA_LIBRARY_DIRS})

# Define the library
add_library ("gldi" SHARED ${core_lib_SRCS})
STRING (REGEX REPLACE "\\..*" "" SOVERSION "${VERSION}")
set_target_properties ("gldi" PROPERTIES
	# create *nix style library versions + symbolic links
	VERSION ${VERSION}
	SOVERSION ${SOVERSION}
	# allow creating static and shared libs without conflicts
	#CLEAN_DIRECT_OUTPUT 1
	# avoid conflicts between library and binary target names
	#OUTPUT_NAME ${PROJECT_NAME}
)

# Link the result to the libraries.
target_link_libraries("gldi"
	${PACKAGE_LIBRARIES}
	${GTK_LIBRARIES}
	${EGL_LIBRARIES}
	${WAYLAND_LIBRARIES}
	${WAYLAND_EGL_LIBRARIES}
	${XEXTEND_LIBRARIES}
	${XINERAMA_LIBRARIES}
	${LIBCRYPT_LIBS}
	implementations
	${GTKLAYERSHELL_LIBRARIES}
	${LIBDL_LIBRARIES}
	${JSON_LIBRARIES}
	${EVDEV_LIBRARIES})


configure_file (${CMAKE_CURRENT_SOURCE_DIR}/gldi.pc.in ${CMAKE_CURRENT_BINARY_DIR}/gldi.pc)
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/gldi.pc DESTINATION ${install-pc-path})
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/libgldi.so DESTINATION ${libdir})
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/libgldi.so.${VERSION} DESTINATION ${libdir})
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/libgldi.so.${SOVERSION} DESTINATION ${libdir})

########### install files ###############

install(FILES cairo-dock.h DESTINATION ${includedir}/cairo-dock)
install(FILES
	cairo-dock-struct.h 			cairo-dock-global-variables.h
	${CMAKE_CURRENT_BINARY_DIR}/gldi-config.h  # gldi-module-config.h doesn't need to be installed
	gldi-icon-names.h
	cairo-dock-manager.h
	cairo-dock-object.h
	cairo-dock-icon-factory.h		cairo-dock-icon-manager.h
	cairo-dock-icon-facility.h
	cairo-dock-applications-manager.h 	cairo-dock-launcher-manager.h
	cairo-dock-separator-manager.h 		cairo-dock-applet-manager.h
	cairo-dock-stack-icon-manager.h			cairo-dock-user-icon-manager.h
	cairo-dock-class-icon-manager.h
	cairo-dock-backends-manager.h
	
	cairo-dock-packages.h
	cairo-dock-data-renderer.h
	cairo-dock-data-renderer-manager.h
	cairo-dock-dock-manager.h		
	cairo-dock-desklet-manager.h
	cairo-dock-dialog-manager.h
	cairo-dock-indicator-manager.h
	cairo-dock-themes-manager.h
	cairo-dock-gui-manager.h
	cairo-dock-file-manager.h
	cairo-dock-desktop-manager.h
	cairo-dock-windows-manager.h
	cairo-dock-class-manager.h
	cairo-dock-opengl.h
	cairo-dock-image-buffer.h
	cairo-dock-config.h
	cairo-dock-module-manager.h
	cairo-dock-module-instance-manager.h
	cairo-dock-container.h
	cairo-dock-dock-factory.h
	cairo-dock-desklet-factory.h
	cairo-dock-dialog-factory.h
	cairo-dock-flying-container.h
	
	cairo-dock-applet-multi-instance.h 	cairo-dock-applet-single-instance.h
	cairo-dock-applet-canvas.h			cairo-dock-applet-facility.h
	
	cairo-dock-draw.h					cairo-dock-draw-opengl.h
	cairo-dock-opengl-path.h 			cairo-dock-opengl-font.h 
	cairo-dock-particle-system.h		cairo-dock-overlay.h
	cairo-dock-dbus.h
	cairo-dock-keyfile-utilities.h		cairo-dock-surface-factory.h
	cairo-dock-log.h					cairo-dock-keybinder.h
	cairo-dock-dock-facility.h
	cairo-dock-task.h
	cairo-dock-animations.h
	cairo-dock-gui-factory.h
	cairo-dock-menu.h
	cairo-dock-style-manager.h
	cairo-dock-style-facility.h
	cairo-dock-utils.h
	
	DESTINATION ${includedir}/cairo-dock/gldit)

if (disable-gtk-grip)
	add_definitions (-DENABLE_GTK_GRIP=1)
endif()
