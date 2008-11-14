#dnl Process this file with automake to produce Makefile.in


SUBDIRS = . src po data


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cairo-dock.pc

scriptdir = ${bindir}
script_DATA = \
	launch-cairo-dock-after-beryl.sh\
	cairo-dock-update.sh

EXTRA_DIST = \
	${script_DATA}\
	intltool-extract.in\
	intltool-merge.in\
	intltool-update.in\
	LICENSE
