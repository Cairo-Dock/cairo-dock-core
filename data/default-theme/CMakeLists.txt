add_subdirectory(launchers)
add_subdirectory(plug-ins)

########### install files ###############

install(FILES
	cairo-dock.conf
	active.png
	bg.svg
	indicator.png
	preview
	readme
	DESTINATION ${pkgdatadir}/themes/_default_ )
