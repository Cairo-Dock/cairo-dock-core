add_subdirectory(launchers)
add_subdirectory(plug-ins)


########### install files ###############

install(FILES  cairo-dock.conf 	active.png 	indicator.png 	 DESTINATION ${pkgdatadir}/themes/_default_ )



#original Makefile.am contents follow:

#SUBDIRS = . launchers icons plug-ins
#
#themedir = ${pkgdatadir}/themes/_default_
#
#theme_DATA = \
#	cairo-dock.conf\
#	active.png\
#	bg.png\
#	cairo-dock-cancel.svg\
#	cairo-dock-ok.svg\
#	indicator.png\
#	separator.png
#	
#
#EXTRA_DIST = $(theme_DATA)
#
