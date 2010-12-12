
SET(core_lib_SRCS
	cairo-dock-struct.h
	cairo-dock-global-variables.h 
	cairo-dock-core.c 					cairo-dock-core.h
	cairo-dock-manager.c 				cairo-dock-manager.h
	# icons
	cairo-dock-icon-manager.c 			cairo-dock-icon-manager.h
	cairo-dock-icon-factory.c 			cairo-dock-icon-factory.h
	cairo-dock-icon-facility.c 			cairo-dock-icon-facility.h
	cairo-dock-indicator-manager.c 		cairo-dock-indicator-manager.h
	cairo-dock-applications-manager.c 	cairo-dock-applications-manager.h 
	cairo-dock-application-facility.c 	cairo-dock-application-facility.h
	cairo-dock-launcher-manager.c 		cairo-dock-launcher-manager.h
	cairo-dock-applet-manager.c 		cairo-dock-applet-manager.h
	cairo-dock-applet-facility.c 		cairo-dock-applet-facility.h
	cairo-dock-separator-manager.c 		cairo-dock-separator-manager.h
	cairo-dock-module-factory.c 		cairo-dock-module-factory.h
	cairo-dock-module-manager.c 		cairo-dock-module-manager.h
	# containers
	cairo-dock-container.c 				cairo-dock-container.h
	cairo-dock-desklet-manager.c 		cairo-dock-desklet-manager.h
	cairo-dock-desklet-factory.c 		cairo-dock-desklet-factory.h
	cairo-dock-dialog-factory.c 		cairo-dock-dialog-factory.h
	cairo-dock-dialog-manager.c 		cairo-dock-dialog-manager.h
	cairo-dock-flying-container.c 		cairo-dock-flying-container.h
	cairo-dock-dock-manager.c 			cairo-dock-dock-manager.h
	cairo-dock-dock-factory.c 			cairo-dock-dock-factory.h
	cairo-dock-dock-facility.c 			cairo-dock-dock-facility.h
	cairo-dock-callbacks.c 				cairo-dock-callbacks.h 
	cairo-dock-animations.c 			cairo-dock-animations.h
	cairo-dock-notifications.c 			cairo-dock-notifications.h
	cairo-dock-backends-manager.c 		cairo-dock-backends-manager.h
	cairo-dock-data-renderer.c 			cairo-dock-data-renderer.h
	cairo-dock-data-renderer-manager.c 		cairo-dock-data-renderer-manager.h
	cairo-dock-file-manager.c 			cairo-dock-file-manager.h
	cairo-dock-themes-manager.c 		cairo-dock-themes-manager.h
	cairo-dock-class-manager.c 			cairo-dock-class-manager.h
	cairo-dock-X-manager.c 				cairo-dock-X-manager.h
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
	eggaccelerators.c 					eggaccelerators.h
	cairo-dock-keybinder.c 				cairo-dock-keybinder.h
	cairo-dock-dbus.c 					cairo-dock-dbus.h 
	cairo-dock-X-utilities.c 			cairo-dock-X-utilities.h
	cairo-dock-keyfile-utilities.c 		cairo-dock-keyfile-utilities.h
	cairo-dock-packages.c 				cairo-dock-packages.h
	cairo-dock-particle-system.c 		cairo-dock-particle-system.h
	cairo-dock-emblem.c 				cairo-dock-emblem.h
	cairo-dock-task.c 					cairo-dock-task.h
	# parameters
	#cairo-dock-internal-position.c 		cairo-dock-internal-position.h
	#cairo-dock-internal-accessibility.c cairo-dock-internal-accessibility.h
	#cairo-dock-internal-system.c 		cairo-dock-internal-system.h
	#cairo-dock-internal-taskbar.c 		cairo-dock-internal-taskbar.h
	#cairo-dock-internal-dialogs.c 		cairo-dock-internal-dialogs.h
	#cairo-dock-internal-indicators.c 	cairo-dock-internal-indicators.h
	#cairo-dock-internal-views.c 		cairo-dock-internal-views.h
	#cairo-dock-internal-labels.c 		cairo-dock-internal-labels.h
	#cairo-dock-internal-desklets.c 		cairo-dock-internal-desklets.h
	#cairo-dock-internal-background.c 	cairo-dock-internal-background.h
	#cairo-dock-internal-icons.c 		cairo-dock-internal-icons.h
	cairo-dock-config.c 				cairo-dock-config.h 
	texture-gradation.h
	texture-blur.h
)


########### compilation ###############

# Make sure the compiler can find include files from the libraries.
include_directories(
	${PACKAGE_INCLUDE_DIRS}
	${XEXTEND_INCLUDE_DIRS}
	${CMAKE_SOURCE_DIR}/src/gldit
	${CMAKE_SOURCE_DIR}/src/icon-factory
	${CMAKE_SOURCE_DIR}/src/implementations)

# Make sure the linker can find the libraries.
link_directories(
	${PACKAGE_LIBRARY_DIRS}
	${XEXTEND_LIBRARY_DIRS}
	${CMAKE_SOURCE_DIR}/src/gldit
	${CMAKE_SOURCE_DIR}/src/icon-factory
	${CMAKE_SOURCE_DIR}/src/implementations)

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

# Link the result to the librairies.
target_link_libraries("gldi"
	${PACKAGE_LIBRARIES}
	${XEXTEND_LIBRARIES}
	${LIBCRYPT_LIBS}
	icon-factory
	implementations)


configure_file (${CMAKE_CURRENT_SOURCE_DIR}/gldi.pc.in ${CMAKE_CURRENT_BINARY_DIR}/gldi.pc)
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/gldi.pc DESTINATION ${libdir}/pkgconfig)
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/libgldi.so DESTINATION ${libdir})
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/libgldi.so.${VERSION} DESTINATION ${libdir})
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/libgldi.so.${SOVERSION} DESTINATION ${libdir})

########### install files ###############

install(FILES cairo-dock.h DESTINATION ${includedir}/cairo-dock)
install(FILES
	cairo-dock-struct.h 			cairo-dock-global-variables.h
	cairo-dock-core.h			cairo-dock-manager.h
	cairo-dock-icon-factory.h		cairo-dock-icon-manager.h
	cairo-dock-icon-facility.h
	cairo-dock-applications-manager.h 	cairo-dock-launcher-manager.h
	cairo-dock-separator-manager.h 		cairo-dock-applet-manager.h
	cairo-dock-backends-manager.h		cairo-dock-notifications.h
	
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
	cairo-dock-X-manager.h
	cairo-dock-class-manager.h
	cairo-dock-opengl.h
	cairo-dock-image-buffer.h
	cairo-dock-config.h
	cairo-dock-module-factory.h
	cairo-dock-module-manager.h
	cairo-dock-container.h
	cairo-dock-dock-factory.h			cairo-dock-callbacks.h
	cairo-dock-desklet-factory.h
	cairo-dock-dialog-factory.h
	cairo-dock-flying-container.h
	
	cairo-dock-applet-multi-instance.h 	cairo-dock-applet-single-instance.h
	cairo-dock-applet-canvas.h			cairo-dock-applet-facility.h
	
	#cairo-dock-internal-position.h 		cairo-dock-internal-accessibility.h
	#cairo-dock-internal-system.h 		cairo-dock-internal-taskbar.h
	#cairo-dock-internal-dialogs.h 		cairo-dock-internal-indicators.h
	#cairo-dock-internal-views.h 		cairo-dock-internal-labels.h
	#cairo-dock-internal-desklets.h 		cairo-dock-internal-background.h
	#cairo-dock-internal-icons.h
	
	cairo-dock-draw.h					cairo-dock-draw-opengl.h
	cairo-dock-opengl-path.h 			cairo-dock-opengl-font.h 
	cairo-dock-particle-system.h		cairo-dock-emblem.h
	cairo-dock-X-utilities.h			cairo-dock-dbus.h
	cairo-dock-keyfile-utilities.h		cairo-dock-surface-factory.h
	cairo-dock-log.h					cairo-dock-keybinder.h
	cairo-dock-application-facility.h	cairo-dock-dock-facility.h
	cairo-dock-task.h
	cairo-dock-animations.h
	cairo-dock-gui-factory.h
	
	DESTINATION ${includedir}/cairo-dock/cairo-dock)
