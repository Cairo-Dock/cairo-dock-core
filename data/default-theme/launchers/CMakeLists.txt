
########### install files ###############

install(FILES  01gimp.desktop 	01firefox.desktop 	01gnome-terminal.desktop 	01thunderbird.desktop DESTINATION ${pkgdatadir}/themes/_default_/launchers)



#original Makefile.am contents follow:

#launchersdir = ${pkgdatadir}/themes/_default_/launchers
#
#launchers_DATA = \
#	01gimp.desktop\
#	01firefox.desktop\
#	01gnome-terminal.desktop\
#	01thunderbird.desktop
#
#EXTRA_DIST = $(launchers_DATA)
#
