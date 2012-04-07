add_subdirectory(launchers)
add_subdirectory(plug-ins)
add_subdirectory(images)

########### install files ###############

install(FILES
	cairo-dock.conf
	_MainDock_-2.conf
	preview
	readme
	DESTINATION ${pkgdatadir}/themes/Default-Panel )
