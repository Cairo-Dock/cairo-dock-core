add_subdirectory(launchers)
add_subdirectory(plug-ins)
#add_subdirectory(images)

########### install files ###############

install(FILES
	cairo-dock.conf
	preview
	readme
	DESTINATION ${pkgdatadir}/themes/_default_ )
