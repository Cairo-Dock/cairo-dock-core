########### requirements ###############

cmake_minimum_required (VERSION 2.6)
find_package (PkgConfig)
include (CheckLibraryExists)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)

########### project ###############

project ("cairo-dock")
set (VERSION "3.0.0.0alpha1")

add_definitions (-std=c99 -Wextra -Wwrite-strings -Wuninitialized -Werror-implicit-function-declaration -Wstrict-prototypes) # removed for stable versions: -Wstrict-prototypes #-Wunreachable-code -Wno-unused-parameter -Wall 
if (NOT ${CMAKE_BUILD_TYPE})
	add_definitions (-O3)
endif()
add_definitions (-DGL_GLEXT_PROTOTYPES="1")
add_definitions (-DCAIRO_DOCK_DEFAULT_ICON_NAME="default-icon.svg")
add_definitions (-DCAIRO_DOCK_ICON="cairo-dock.svg")
add_definitions (-DCAIRO_DOCK_LOGO="cairo-dock-logo.png")
add_definitions (-DCAIRO_DOCK_DATA_DIR="cairo-dock")
add_custom_target (uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

############ sources tarball ############

set (CPACK_SOURCE_GENERATOR "TGZ")
set (CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${VERSION}")
set (CPACK_SOURCE_IGNORE_FILES 
	"/build/;/.bzr/;bzrignore$;/config.h$;/gldi-config.h$;/doc/;/misc/;~$;${CPACK_SOURCE_IGNORE_FILES}")
include (CPack)

add_custom_target( dist
	COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
add_custom_target(dist-bzr
	COMMAND bzr export ${CMAKE_PROJECT_NAME}-${VERSION}.tar.gz
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

########### global variables ###############

if( WIN32 )
	message(FATAL_ERROR "Cairo-Dock requires an air-conditioned room. Please close Windows!")
endif( WIN32 )

set (PACKAGE ${CMAKE_PROJECT_NAME})
set (GETTEXT_PACKAGE ${PACKAGE})
set (prefix ${CMAKE_INSTALL_PREFIX})
set (exec_prefix ${prefix})
set (datadir ${prefix}/share)
set (pkgdatadir ${datadir}/cairo-dock)
set (mandir ${datadir}/man)
set (VERSION_HELP "0.9.99") # needed to parse Help.conf.in

if( CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT "${FORCE_LIB64}" STREQUAL "")  # 64bits and force install in lib64
	set (libdir ${prefix}/lib64)
else()
	set (libdir ${prefix}/lib${LIB_SUFFIX})
endif()

set (includedir ${prefix}/include)
set (bindir ${prefix}/bin)

########### dependencies ###############

set (packages_required "cairo librsvg-2.0 dbus-1 dbus-glib-1 libxml-2.0 xrender gl glu pangox libcurl")  # for the .pc
pkg_check_modules ("PACKAGE" REQUIRED "${packages_required}")

set (xextend_required "xtst xcomposite xinerama")  # for the .pc
pkg_check_modules ("XEXTEND" "${xextend_required}")
if (${XEXTEND_FOUND})
	set (HAVE_XEXTEND 1)
else()
	set (xextend_required)
endif()

if (NOT "${force-gtk3}" STREQUAL "")
	set (gtk_required "gtk+-3.0 gthread-2.0")  # for the .pc
	pkg_check_modules ("GTK" "${gtk_required}")  # the check is not requiered because we can do with GTK2
endif()

if (NOT "${GTK_FOUND}")
	set (gtk_required "gtk+-2.0 gthread-2.0")  # for the .pc
	pkg_check_modules ("GTK" REQUIRED "${gtk_required}")  # the check is requiered this time because we need GTK anyway.
endif()

STRING (REGEX REPLACE "\\..*" "" GTK_MAJOR "${GTK_VERSION}")

# We use crypt(3) which may be in libc, or in libcrypt (eg FreeBSD)
check_library_exists (crypt encrypt "" HAVE_LIBCRYPT)
if (${HAVE_LIBCRYPT})
	set (LIBCRYPT_LIBS "-lcrypt")
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
MESSAGE (STATUS " * GTK version : ${GTK_MAJOR}")
MESSAGE (STATUS)

########### next steps ###############

add_subdirectory (src)
add_subdirectory (data)
add_subdirectory (po)

########### variables defined at compil time ###############

set (CAIRO_DOCK_SHARE_DATA_DIR ${pkgdatadir})
set (CAIRO_DOCK_SHARE_THEMES_DIR ${pkgdatadir}/themes)
#set (CAIRO_DOCK_MODULES_DIR ${libdir}/cairo-dock)
set (CAIRO_DOCK_LOCALE_DIR ${datadir}/locale)
set (CAIRO_DOCK_THEMES_DIR "themes")
set (CAIRO_DOCK_GETTEXT_PACKAGE ${GETTEXT_PACKAGE})

set (GLDI_GETTEXT_PACKAGE ${GETTEXT_PACKAGE})
set (GLDI_SHARE_DATA_DIR ${pkgdatadir})
set (GLDI_MODULES_DIR ${libdir}/cairo-dock)

########### file generation ###############

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/src/gldit/gldi-config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/gldit/gldi-config.h)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/data/cairo-dock.conf.in ${CMAKE_CURRENT_BINARY_DIR}/data/cairo-dock.conf)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/data/cairo-dock-simple.conf.in ${CMAKE_CURRENT_BINARY_DIR}/data/cairo-dock-simple.conf)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/data/main-dock.conf.in ${CMAKE_CURRENT_BINARY_DIR}/data/main-dock.conf)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/data/launcher.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/data/launcher.desktop)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/data/container.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/data/container.desktop)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/data/separator.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/data/separator.desktop)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/data/themes.conf.in ${CMAKE_CURRENT_BINARY_DIR}/data/themes.conf)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/data/Help.conf.in ${CMAKE_CURRENT_BINARY_DIR}/data/Help.conf)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cairo-dock.pc.in ${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.pc)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake IMMEDIATE @ONLY)

########### install files ###############

install (FILES cairo-dock-package-theme
	DESTINATION ${bindir}
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.pc DESTINATION ${libdir}/pkgconfig)

###########     misc      ###############
#set (CPACK_SOURCE_GENERATOR "TGZ")
#set (CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${VERSION}")
#set (CPACK_SOURCE_IGNORE_FILES "/build/;/.bzr/;/.deps/;Makefile$;Makefile.in;.gmo$;/.libs/;.lo$;.la$;POTFILES;stamp-it;aclocal.m4;config.guess$;/config.h$;/config.h.in$;/config.log$;/config.status$;/config.sub$;/configure$;/depcomp$;/install-sh$;/libtool$;/ltmain.sh$;/missing$;/stamp-h1$;/compile;/CMakeFiles/;/_CPack_Packages/;/autom4te.cache/;/install_manifest.txt$;.tar.gz$;/CMakeCache.txt$;/cairo-dock$;/scripts/;/doc/;/data/themes.conf$;/data/cairo-dock.conf$;/data/main-dock.conf$;/data/cairo-dock-simple.conf$;/cairo-dock.pc$;.cmake$;~$;${CPACK_SOURCE_IGNORE_FILES}")
#include (CPack)  # a inclure apres les set.
#add_custom_target( dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)  # pour pouvoir l'appeler avec "make dist"
