
########### install files ###############

install(FILES  background.svg 	foreground.svg 	needle.svg 	theme.xml 	readme DESTINATION ${pkgdatadir}/gauges/Turbo-night-fuel )



#original Makefile.am contents follow:

#themedir = ${pkgdatadir}/gauges/Turbo-night-fuel
#
#theme_DATA = \
#	background.svg\
#	foreground.svg\
#	needle.svg\
#	theme.xml\
#	readme
#
#EXTRA_DIST = $(theme_DATA)
