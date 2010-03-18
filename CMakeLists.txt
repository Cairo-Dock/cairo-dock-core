########### requirements ###############

cmake_minimum_required (VERSION 2.6)
find_package (PkgConfig)
include (CheckLibraryExists)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)

########### project ###############

project ("cairo-dock")
set (VERSION "2.1.4-0beta0")
set (CPACK_SOURCE_GENERATOR "TBZ2")
set (CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${VERSION}")
set (CPACK_SOURCE_IGNORE_FILES "/build/;/.bzr/;~$;${CPACK_SOURCE_IGNORE_FILES}")
include (CPack)  # a inclure apres les set.
add_custom_target( dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)  # pour pouvoir l'appeler avec "make dist"
add_definitions (-std=c99 -Wstrict-prototypes -Wextra -Wwrite-strings -Wuninitialized -Werror-implicit-function-declaration) #-Wunreachable-code -Wno-unused-parameter -Wall 
if (NOT ${CMAKE_BUILD_TYPE})
	add_definitions (-O3)
endif()
add_definitions (-DGL_GLEXT_PROTOTYPES="1")
add_definitions (-DCAIRO_DOCK_DEFAULT_ICON_NAME="default-icon.svg")
add_definitions (-DCAIRO_DOCK_ICON="cairo-dock.svg")
add_definitions (-DCAIRO_DOCK_LOGO="cairo-dock-logo.png")
add_definitions (-DCAIRO_DOCK_DATA_DIR="cairo-dock")

########### global variables ###############

set (PACKAGE ${CMAKE_PROJECT_NAME})
set (GETTEXT_PACKAGE ${PACKAGE})
set (prefix ${CMAKE_INSTALL_PREFIX})
set (exec_prefix ${prefix})
set (datadir ${prefix}/share)
set (pkgdatadir ${datadir}/cairo-dock)
set (libdir ${prefix}/lib)
set (includedir ${prefix}/include)
set (bindir ${prefix}/bin)

########### dependencies ###############

pkg_check_modules ("PACKAGE" REQUIRED "gtk+-2.0" "gthread-2.0" "cairo" "librsvg-2.0" "dbus-1" "dbus-glib-1" "libxml-2.0" "gtkglext-1.0" "xrender")

pkg_check_modules ("XEXTEND" "xtst" "xcomposite" "xinerama")
if (${XEXTEND_FOUND})
	set (HAVE_XEXTEND 1)
	set (xextend_required "xtst xcomposite xinerama")  # pour le .pc
endif()

# We use crypt(3) which may be in libc, or in libcrypt (eg FreeBSD)
check_library_exists (crypt encrypt "" HAVE_LIBCRYPT)
if (${HAVE_LIBCRYPT})
	set (LIBCRYPT_LIBS "-lcrypt")
endif()

pkg_check_modules ("LIBGIO" "gio-2.0")
if (${LIBGIO_FOUND})
	set (HAVE_LIBGIO 1)
	set (gio_detected 1)
	set (libgio_required "gio-2.0")  # pour le .pc
else()
	set (gio_detected 0)
endif()

check_include_files ("math.h" HAVE_MATH_H)
check_symbol_exists (LC_MESSAGES "locale.h" HAVE_LC_MESSAGES)
CHECK_LIBRARY_EXISTS (m sin "" HAVE_LIBM)

MESSAGE (STATUS)
MESSAGE (STATUS "Cairo-Dock will be compiled with the following options :")
if (${XEXTEND_FOUND})
	MESSAGE (STATUS " * Use X Extensions : yes")
else()
	MESSAGE (STATUS " * Use X Extensions : no")
endif()
if (${HAVE_LIBCRYPT})
	MESSAGE (STATUS " * crypt passwords : yes")
else()
	MESSAGE (STATUS " * crypt passwords : no")
endif()
MESSAGE (STATUS)

########### next steps ###############

add_subdirectory (src)
add_subdirectory (data)
add_subdirectory (po)

########### variables defined at compil time ###############

set (CAIRO_DOCK_SHARE_DATA_DIR ${pkgdatadir})
set (CAIRO_DOCK_SHARE_THEMES_DIR ${pkgdatadir}/themes)
set (CAIRO_DOCK_MODULES_DIR ${libdir}/cairo-dock)
set (CAIRO_DOCK_LOCALE_DIR ${datadir}/locale)
set (CAIRO_DOCK_THEMES_DIR "themes")
set (CAIRO_DOCK_GETTEXT_PACKAGE ${GETTEXT_PACKAGE})

########### file generation ###############

configure_file (config.h.cmake config.h)
configure_file (src/cairo-dock-gio-vfs.h.in src/cairo-dock-gio-vfs.h)
configure_file (data/cairo-dock.conf.in data/cairo-dock.conf)
configure_file (data/cairo-dock-simple.conf.in data/cairo-dock-simple.conf)
configure_file (data/main-dock.conf.in data/main-dock.conf)
configure_file (data/themes.conf.in data/themes.conf)
configure_file (cairo-dock.pc.in cairo-dock.pc)

########### install files ###############

install (FILES  cairo-dock-package-theme launch-cairo-dock-after-compiz DESTINATION ${bindir})
install (FILES  cairo-dock.pc DESTINATION ${libdir}/pkgconfig)

#original Makefile.am contents follow:

##dnl Process this file with automake to produce Makefile.in
#
#
#SUBDIRS = . src po data
#
#
#pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = cairo-dock.pc
#
#
#dist_bin_SCRIPTS = \
#	cairo-dock-package-theme\
#	launch-cairo-dock-after-compiz
#
#
#EXTRA_DIST = \
#	intltool-extract.in\
#	intltool-merge.in\
#	intltool-update.in\
#	LICENSE\
#	LGPL-2\
#	ChangeLog\
#	copyright
