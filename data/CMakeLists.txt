add_subdirectory(themes)
add_subdirectory(gauges)
add_subdirectory(explosion)
add_subdirectory(man)
add_subdirectory(icons)
add_subdirectory(images)
add_subdirectory(scripts)
if ("${enable-desktop-manager}" STREQUAL "yes")
	add_subdirectory(desktop-manager)
endif()


########### file generation ###############

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cairo-dock.conf.in ${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.conf)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cairo-dock-simple.conf.in ${CMAKE_CURRENT_BINARY_DIR}/cairo-dock-simple.conf)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/main-dock.conf.in ${CMAKE_CURRENT_BINARY_DIR}/main-dock.conf)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/launcher.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/launcher.desktop)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/container.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/container.desktop)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/separator.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/separator.desktop)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/themes.conf.in ${CMAKE_CURRENT_BINARY_DIR}/themes.conf)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/Help.conf.in ${CMAKE_CURRENT_BINARY_DIR}/Help.conf)


########### install files ###############

# misc
install (FILES
	cairo-dock.svg
	readme-default-view
	ChangeLog.txt
	DESTINATION ${pkgdatadir})

# appli icon
install (FILES
	cairo-dock.svg
	DESTINATION ${datadir}/pixmaps)

# conf files
install (FILES
	# lib
	${CMAKE_CURRENT_BINARY_DIR}/main-dock.conf
	${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.conf
	${CMAKE_CURRENT_BINARY_DIR}/launcher.desktop
	${CMAKE_CURRENT_BINARY_DIR}/container.desktop
	${CMAKE_CURRENT_BINARY_DIR}/separator.desktop
	# appli
	${CMAKE_CURRENT_BINARY_DIR}/cairo-dock-simple.conf
	${CMAKE_CURRENT_BINARY_DIR}/themes.conf
	${CMAKE_CURRENT_BINARY_DIR}/Help.conf
	DESTINATION ${pkgdatadir})

# appli desktop file
install (FILES
	cairo-dock.desktop
	cairo-dock-cairo.desktop
	DESTINATION ${datadir}/applications)
