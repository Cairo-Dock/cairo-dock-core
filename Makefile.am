#dnl Process this file with automake to produce Makefile.in


SUBDIRS = . src po data


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cairo-dock.pc


dist_bin_SCRIPTS = \
	cairo-dock-package-theme.sh\
	launch-cairo-dock-after-beryl.sh\
	cairo-dock-update.sh


EXTRA_DIST = \
	intltool-extract.in\
	intltool-merge.in\
	intltool-update.in\
	LICENSE
