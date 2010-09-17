
########### install files ###############

install(FILES  charge.svg 	drop.svg 	play.svg 	pause.svg 	stop.svg 	broken.svg 	error.svg 	warning.svg 	locked.svg DESTINATION ${pkgdatadir}/emblems )



#original Makefile.am contents follow:

#SUBDIRS = .
#
#emblemsdir = ${pkgdatadir}/emblems
#
#emblems_DATA = \
#	charge.svg\
#	drop.svg\
#	play.svg\
#	pause.svg\
#	stop.svg\
#	broken.svg\
#	error.svg\
#	warning.svg\
#	locked.svg
#	
#EXTRA_DIST = $(emblems_DATA)
