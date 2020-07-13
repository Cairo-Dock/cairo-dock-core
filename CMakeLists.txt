########### requirements ###############

cmake_minimum_required (VERSION 2.6)
find_package (PkgConfig)
include (CheckLibraryExists)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include ("${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/GNUInstallDirs.cmake")

########### project ###############

project ("cairo-dock")
set (VERSION "3.4.99.alpha1") # no dash, only numbers, dots and maybe alpha/beta/rc, e.g.: 3.3.1 or 3.3.99.alpha1

add_definitions (-std=c99 -Wall -Wextra -Werror-implicit-function-declaration) # -Wextra -Wwrite-strings -Wuninitialized -Werror-implicit-function-declaration -Wstrict-prototypes -Wreturn-type -Wparentheses -Warray-bounds)
if (NOT DEFINED CMAKE_BUILD_TYPE)
	add_definitions (-O3)
endif()
add_definitions (-DGL_GLEXT_PROTOTYPES="1")
add_definitions (-DCAIRO_DOCK_DEFAULT_ICON_NAME="default-icon.svg")
add_definitions (-DCAIRO_DOCK_ICON="cairo-dock.svg")
add_definitions (-DCAIRO_DOCK_LOGO="cairo-dock-logo.png")
add_definitions (-DCAIRO_DOCK_DATA_DIR="cairo-dock")
add_custom_target (uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")


###########     Misc     ###############
macro (enable_if_not_defined MODULE1)
	if (NOT DEFINED ${MODULE1}) # true if not defined
		set (${MODULE1} TRUE)
	endif ()
endmacro (enable_if_not_defined)


##########     Config    ###############

enable_if_not_defined (force-icon-in-menus)
if (force-icon-in-menus)  # we believe that not showing icons in the menus by default is a terrible idea; unfortunately, it's not easily undoable for an end-user; so until this is fixed by a big player (Gnome, Ubuntu or other), we'll force the display, unless "-Dforce-icon-in-menus=OFF" is provided in the cmake command.
	add_definitions (-DCAIRO_DOCK_FORCE_ICON_IN_MENUS=1)
else()
	add_definitions (-DCAIRO_DOCK_FORCE_ICON_IN_MENUS=0)
endif()

if (use-new-positioning-on-x11) # new positioning code should work on both X11 and Wayland, but, by default, it is used only on Wayland -- use this cmake options to enable it on X11 as well for testing it
	add_definitions (-DCAIRO_DOCK_USE_NEW_POSITIONING_ON_X11=1)
else()
	add_definitions (-DCAIRO_DOCK_USE_NEW_POSITIONING_ON_X11=0)
endif()

############ sources tarball ############

set (CPACK_SOURCE_GENERATOR "TGZ")
set (CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${VERSION}")
set (CPACK_SOURCE_IGNORE_FILES 
	"/build/;/.bzr/;/.bzrignore$;/.git/;/.gitignore$;/config.h$;/gldi-module-config.h$;/gldi-config.h$;/doc/;/misc/;~$;/TODO$;.pyc$;${CPACK_SOURCE_IGNORE_FILES}")
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

set (prefix ${CMAKE_INSTALL_PREFIX})  # /usr/local
set (exec_prefix ${prefix})
set (datadir "${prefix}/${CMAKE_INSTALL_DATAROOTDIR}")  # (...)/share
set (pkgdatadir "${datadir}/${CMAKE_PROJECT_NAME}")  # (...)/cairo-dock
set (mandir "${prefix}/${CMAKE_INSTALL_MANDIR}")  # (...)/man

if( CMAKE_SIZEOF_VOID_P EQUAL 8 AND (force-lib64 OR "${FORCE_LIB64}" STREQUAL "yes"))  # 64bits and force install in lib64
	set (libdir "${prefix}/lib64")
elseif (NOT "${LIB_SUFFIX}" STREQUAL "")
	set (libdir "${prefix}/lib${LIB_SUFFIX}")  # (...)/libXX ## some distro use ${LIB_SUFFIX}
else()
	set (libdir "${prefix}/${CMAKE_INSTALL_LIBDIR}")  # (...)/lib or (...)/lib64 or custom ## GNU Install dir
endif()

set (includedir "${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")  # (...)/include
set (bindir "${prefix}/${CMAKE_INSTALL_BINDIR}")  # (...)/bin

set (pluginsdir "${libdir}/cairo-dock")
set (pluginsdatadir "${pkgdatadir}/plug-ins")
if (NOT DEFINED install-pc-path)
	set (install-pc-path "${libdir}/pkgconfig")  # it can be different (for example ${CMAKE_INSTALL_PREFIX}/libdata/pkgconfig on BSD)
endif ()

########### dependencies ###############

# check for mandatory dependencies
set (packages_required "glib-2.0 gthread-2.0 cairo librsvg-2.0 dbus-1 dbus-glib-1 libxml-2.0 gl glu libcurl")  # for the .pc and to have details
STRING (REGEX REPLACE " " ";" packages_required_semicolon ${packages_required})  # replace blank space by semicolon => to have more details if a package is missing
pkg_check_modules ("PACKAGE" REQUIRED "${packages_required_semicolon}")

# check for EGL
set (with_egl no)  # although EGL can be used with X (replacing GLX), it requires drivers that support DRI2, so most of the time GLX works better; so it's disabled by default for now; later, it will be required for Wayland, and we'll have to decide whether we compile with both, or whether we load one of them as a plug-in at run-time. It may depend on EGL having been built to support a given graphic target.
if (enable-egl-support)
	pkg_check_modules ("EGL" "egl")
	if (EGL_FOUND)
		set (HAVE_EGL 1)
		set (with_egl "yes (${EGL_VERSION})")
	endif()
endif()

# check for X11
set (with_x11 no)
set (with_xentend no)
enable_if_not_defined (enable-x11-support) # enabled by default
if (enable-x11-support)
	# check for X11
	set (x11_required "x11")  # for the .pc
	pkg_check_modules ("X11" ${x11_required})
	if (X11_FOUND)
		set (HAVE_X11 1)
		set (with_x11 yes)
	else()
		set (x11_required)
	endif()
	
	# check for GLX
	if (NOT EGL_FOUND)  # currently we only have an X backend so we use either GLX or EGL, not both at once.
		check_library_exists (GL glXMakeCurrent "" HAVE_GLX)  # HAVE_GLX remains undefined if not found, else it's "1"
	endif()
	
	# check for X extensions
	set (xextend_required "xtst xcomposite xrandr xrender")  # for the .pc
	STRING (REGEX REPLACE " " ";" xextend_required_semicolon ${xextend_required})
	pkg_check_modules ("XEXTEND" "${xextend_required_semicolon}")
	
	if (XEXTEND_FOUND)
		set (HAVE_XEXTEND 1)
		set (with_xentend yes)
		
		pkg_check_modules ("XINERAMA" "xinerama")  # check for Xinerama separately, since it is now replaced by Xrandr >= 1.3
		if (XINERAMA_FOUND)
			set (HAVE_XINERAMA 1)
		endif()
	else()
		set (xextend_required)
	endif()
endif()

# check for Wayland
set (with_wayland no)
enable_if_not_defined (enable-wayland-support) # enabled by default
if (enable-wayland-support)
	set (wayland_required "wayland-client")  # for the .pc
	pkg_check_modules ("WAYLAND" ${wayland_required}>=1.0.0)
	if (WAYLAND_FOUND)
		set (HAVE_WAYLAND 1)
		set (with_wayland "yes (${WAYLAND_VERSION})")
	else()
		set (wayland_required)
	endif()
endif()

# GTK 3
set (gtk_required "gtk+-3.0")  # for the .pc
pkg_check_modules ("GTK" REQUIRED "${gtk_required}>=3.4.0")

STRING (REGEX REPLACE "\\..*" "" GTK_MAJOR "${GTK_VERSION}")
STRING (REGEX REPLACE "[0-9]*\\.([^ ]+)" "\\1" GTK_MINOR "${GTK_VERSION}")  # 3.8.2 => 3.8
STRING (REGEX REPLACE "\\.[0-9]*" "" GTK_MINOR "${GTK_MINOR}") # 3.8 => 8

# add_definitions (-DGTK_DISABLE_DEPRECATED="1")
# add_definitions (-DG_DISABLE_DEPRECATED="1")

# We use crypt(3) which may be in libc, or in libcrypt (eg FreeBSD)
check_library_exists (crypt encrypt "" HAVE_LIBCRYPT)
if (HAVE_LIBCRYPT)
	set (LIBCRYPT_LIBS "-lcrypt")
endif()

check_symbol_exists (LC_MESSAGES "locale.h" HAVE_LC_MESSAGES)
check_include_files ("math.h" HAVE_MATH_H)
check_library_exists (m sin "" HAVE_LIBM)
if (NOT HAVE_LIBM OR NOT HAVE_MATH_H)
	message(FATAL_ERROR "Cairo-Dock requires math.h")
endif()

check_include_files ("dlfcn.h" HAVE_DLFCN_H)
check_library_exists (dl dlopen "" HAVE_LIBDL)
if (HAVE_LIBDL)  # dlopen might be in libc directly as in FreeBSD
	set (LIBDL_LIBRARIES "dl")
endif()
if (NOT HAVE_DLFCN_H)
	message(FATAL_ERROR "Cairo-Dock requires dlfcn.h")
endif()

check_library_exists (intl libintl_gettext "" HAVE_LIBINTL)
if (HAVE_LIBINTL)  # on BSD, we have to link to libintl to be able to use gettext.
	set (LIBINTL_LIBRARIES "intl")
endif()

# check for gtk-layer-shell
set (with_gtk_layer_shell no)
enable_if_not_defined (enable-gtk-layer-shell) # enabled by default
if (enable-gtk-layer-shell)
	if (HAVE_WAYLAND)
		pkg_check_modules ("GTKLAYERSHELL" "gtk-layer-shell-0")
		if (GTKLAYERSHELL_FOUND)
			set (HAVE_GTK_LAYER_SHELL 1)
			set (with_gtk_layer_shell yes)
		endif()
	endif()
endif()

########### variables defined at compil time ###############

set (CAIRO_DOCK_SHARE_DATA_DIR ${pkgdatadir})
set (CAIRO_DOCK_SHARE_THEMES_DIR ${pkgdatadir}/themes)
#set (CAIRO_DOCK_MODULES_DIR ${libdir}/cairo-dock)
set (CAIRO_DOCK_LOCALE_DIR "${prefix}/${CMAKE_INSTALL_LOCALEDIR}")
set (CAIRO_DOCK_THEMES_DIR "themes")  # folder name where themes are saved locally, relatively to the root folder of Cairo-Dock.
set (CAIRO_DOCK_DISTANT_THEMES_DIR "themes3.4")  # folder name where themes are on the server, relatively to the root folder of the server files.
set (CAIRO_DOCK_GETTEXT_PACKAGE ${GETTEXT_PACKAGE})

set (GLDI_GETTEXT_PACKAGE ${GETTEXT_PACKAGE})
set (GLDI_SHARE_DATA_DIR ${pkgdatadir})
set (GLDI_MODULES_DIR ${pluginsdir})
set (GLDI_BIN_DIR ${bindir})

########### next steps ###############

add_subdirectory (src)
add_subdirectory (data)
add_subdirectory (po)

############# HELP #################
# this is actually a plug-in for cairo-dock, not for gldi
# it uses some functions of cairo-dock (they are binded dynamically), that's why it can't go with other plug-ins
set (GETTEXT_HELP ${GETTEXT_PACKAGE})
set (VERSION_HELP "0.9.994")
set (PACKAGE_HELP "cd-Help")
set (helpdatadir "${pluginsdatadir}/Help")
set (dock_version "${VERSION}")
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/Help/data/Help.conf.in ${CMAKE_CURRENT_BINARY_DIR}/Help/data/Help.conf)
add_subdirectory (Help)

########### file generation ###############

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/src/gldit/gldi-config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/gldit/gldi-config.h)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/src/gldit/gldi-module-config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/gldit/gldi-module-config.h)  # separated from gldi-config.h because it's architecture-dependant (it contains $libdir), so it can't be installed in /usr/include without causing a conflict between 32 and 64 bits packages.
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake IMMEDIATE @ONLY)

################# Summary #################

MESSAGE (STATUS)
MESSAGE (STATUS "Cairo-Dock ${VERSION} will be compiled with the following options:")
MESSAGE (STATUS " * Installation in     : ${prefix}")
MESSAGE (STATUS " * Lib directory       : ${libdir}")
MESSAGE (STATUS " * GTK version         : ${GTK_MAJOR} (${GTK_VERSION})")
MESSAGE (STATUS " * With X11 support    : ${with_x11}")
MESSAGE (STATUS " * With X11 extensions : ${with_xentend} (${xextend_required})")
if (HAVE_GLX)
	MESSAGE (STATUS " * With GLX support    : yes")
else()
	MESSAGE (STATUS " * With GLX support    : no")
endif()
MESSAGE (STATUS " * With Wayland support: ${with_wayland}")
MESSAGE (STATUS " * With gtk-layer-shell: ${with_gtk_layer_shell}")
MESSAGE (STATUS " * With EGL support    : ${with_egl}")
if (HAVE_LIBCRYPT)
	MESSAGE (STATUS " * Crypt passwords     : yes")
else()
	MESSAGE (STATUS " * Crypt passwords     : no")
endif()
if (enable-desktop-manager)
	set (with_cd_session "yes")
else()
	set (with_cd_session "no (use '-Denable-desktop-manager=ON' to enable it)")
endif()
MESSAGE (STATUS " * Cairo-dock session  : ${with_cd_session}")
MESSAGE (STATUS " * Themes directory    : ${CAIRO_DOCK_DISTANT_THEMES_DIR} (on the server)")
MESSAGE (STATUS)
