########### requirements ###############

cmake_minimum_required (VERSION 3.16.0)

project ("cairo-dock" C)

find_package (PkgConfig)
include (CheckLibraryExists)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (GNUInstallDirs)

########### project ###############

set (VERSION "3.6.3") # no dash, only numbers, dots and maybe alpha/beta/rc, e.g.: 3.3.1 or 3.3.99.alpha1

add_compile_options (-std=c99 -Wall -Wextra -Werror-implicit-function-declaration) # -Wextra -Wwrite-strings -Wuninitialized -Wstrict-prototypes -Wreturn-type -Wparentheses -Warray-bounds)
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
string (TIMESTAMP GLDI_COMPILE_DATE)
set (GETTEXT_PACKAGE ${PACKAGE})

set (prefix ${CMAKE_INSTALL_PREFIX})  # /usr/local
set (exec_prefix ${prefix})
set (datadir "${prefix}/${CMAKE_INSTALL_DATAROOTDIR}")  # (...)/share
set (pkgdatadir "${datadir}/${CMAKE_PROJECT_NAME}")  # (...)/cairo-dock
set (mandir "${prefix}/${CMAKE_INSTALL_MANDIR}")  # (...)/man
set (unitdir "${prefix}/lib/systemd/user") # for systemd units (if enabled)

if( CMAKE_SIZEOF_VOID_P EQUAL 8 AND (force-lib64 OR "${FORCE_LIB64}" STREQUAL "yes"))  # 64bits and force install in lib64
	set (libdir "${prefix}/lib64")
elseif (NOT "${LIB_SUFFIX}" STREQUAL "")
	set (libdir "${prefix}/lib${LIB_SUFFIX}")  # (...)/libXX ## some distro use ${LIB_SUFFIX}
else()
	set (libdir "${prefix}/${CMAKE_INSTALL_LIBDIR}")  # (...)/lib or (...)/lib64 or custom ## GNU Install dir
endif()

set (includedir "${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")  # (...)/include
set (bindir "${prefix}/${CMAKE_INSTALL_BINDIR}")  # (...)/bin

if (NOT DEFINED plugins-prefix)
	set (plugins-prefix "${prefix}")
endif()
set (pluginsdir "${plugins-prefix}/${CMAKE_INSTALL_LIBDIR}/cairo-dock")
set (pluginsdatarootdir "${plugins-prefix}/${CMAKE_INSTALL_DATAROOTDIR}/${CMAKE_PROJECT_NAME}")
set (plugins-locale "${plugins-prefix}/${CMAKE_INSTALL_LOCALEDIR}")
set (plugins-gettext "cairo-dock-plugins") # gettext domain used by plugins

if (NOT DEFINED install-pc-path)
	set (install-pc-path "${libdir}/pkgconfig")  # it can be different (for example ${CMAKE_INSTALL_PREFIX}/libdata/pkgconfig on BSD)
endif ()

list(APPEND CMAKE_INSTALL_RPATH ${libdir})

########### dependencies ###############

# check for mandatory dependencies
# for pkg_check_modules -- use semicolon to have more details if a package is missing
set (packages_required_semicolon "glib-2.0 >= 2.40.0;gthread-2.0;cairo;librsvg-2.0;dbus-1;dbus-glib-1;libxml-2.0;gl;glu;libcurl")
# for gldi.pc -- replace semicolons with spaces
STRING (REPLACE ";" " " packages_required "${packages_required_semicolon}") # note: need to use quotes around ${packages_required_semicolon} for this to work!
pkg_check_modules ("PACKAGE" REQUIRED ${packages_required_semicolon})
find_library(MLIB m)
if (MLIB)
  set (PACKAGE_LIBRARIES ${PACKAGE_LIBRARIES} ${MLIB})
  set (lm_required "-lm") # for the .pc -- note: this might result in an error if -lm is not the correct flag
else()
  set (lm_required "")
endif()


# check for Wayland
set (with_wayland no)
set (WAYLAND_FOUND 0)
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

if (WAYLAND_FOUND)
	enable_if_not_defined (enable-wayland-protocols) # enabled by default
	if (enable-wayland-protocols)
		find_package(ECM NO_MODULE)
		if (ECM_FOUND)
			set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})
			include (FindWaylandScanner)
			if (WaylandScanner_FOUND)
				set (with_wayland_taskbar "yes")
				set (HAVE_WAYLAND_PROTOCOLS 1)
			else()
				set (with_wayland_taskbar "no (cannot find wayland-scanner)")
			endif()
		else()
			set (with_wayland_taskbar "no (cannot find extra-cmake-modules)")
		endif()
	else()
		set (with_wayland_taskbar "no (disabled)")
	endif()
	# json (for Wayfire IPC interface)
	pkg_check_modules ("JSON" "json-c")
	if (JSON_FOUND)
		set (HAVE_JSON 1)
	endif()
endif()

# check for EGL -- enabled by default, although on X, we still use GLX by default
set (with_egl no)
enable_if_not_defined (enable-egl-support)
if (enable-egl-support)
	pkg_check_modules ("EGL" "egl")
	if (EGL_FOUND)
		set (HAVE_EGL 1)
		set (with_egl "yes (${EGL_VERSION})")
	endif()
	if (WAYLAND_FOUND)
		# check separately for libwayland-egl1, which is used in this case
		pkg_check_modules ("WAYLAND_EGL" REQUIRED "wayland-egl")
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
	
	# check for GLX -- TODO: using 'pkg_check_modules ("GLX" "glx")' should work, or very old versions do not provide glx.pc?
	enable_if_not_defined (enable-glx-support)
	if (enable-glx-support)
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

# GTK 3
set (gtk_required "gtk+-3.0")  # for the .pc
pkg_check_modules ("GTK" REQUIRED "${gtk_required}>=3.22.0")

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
		pkg_check_modules ("GTKLAYERSHELL" "gtk-layer-shell-0>=0.6.0")
		if (GTKLAYERSHELL_FOUND)
			set (HAVE_GTK_LAYER_SHELL 1)
			set (with_gtk_layer_shell yes)
		endif()
	endif()
endif()

########### desktop manager ###############

if (enable-desktop-manager)
	if (NOT DEFINED gnome-session-use-systemd)
		set (gnome-session-use-systemd FALSE)
		find_program (GS_COMMAND gnome-session)
		if (GS_COMMAND)
			# note: --debug is needed as older versions would redirect output to the system log, see:
			# https://gitlab.gnome.org/GNOME/gnome-session/-/issues/64
			execute_process (COMMAND ${GS_COMMAND} --version --debug OUTPUT_VARIABLE GS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
			# output format: /usr/libexec/gnome-session-binary 46.0 -- we need the last component
			STRING (REGEX MATCH "[0-9.]+$" GS_VERSION ${GS_VERSION}) # keep only the version number
			if ("${GS_VERSION}" VERSION_GREATER_EQUAL "49.0")
				set (gnome-session-use-systemd TRUE)
			endif ()
		endif ()
	endif ()
	if (gnome-session-use-systemd)
		if (NOT DEFINED enable-systemd-service)
			set (enable-systemd-service TRUE)
		else ()
			if (NOT enable-systemd-service)
				message(FATAL_ERROR "Desktop session for gnome-session >= 49 requires systemd!")
			endif ()
		endif()
		set (DESKTOP_EXTRA_VAR "X-GNOME-HiddenUnderSystemd=true")
	endif ()
endif ()

# systemd service
if (NOT DEFINED enable-systemd-service)
	# detect if system is running systemd
	set (enable-systemd-service FALSE)
	find_program (SYSTEMD_COMMAND systemd-notify)
	if (SYSTEMD_COMMAND)
		execute_process(COMMAND ${SYSTEMD_COMMAND} --booted RESULT_VARIABLE SYSTEMD_RUNNING)
		if (SYSTEMD_RUNNING EQUAL 0)
			set (enable-systemd-service TRUE)
		endif()
	endif()
	if (enable-systemd-service)
		set (with_systemd_service "yes (use '-Denable-systemd-service=False' to disable it)")
	else()
		set (with_systemd_service "no (use '-Denable-systemd-service=True' to enable it)")
	endif()
else()
	if (enable-systemd-service)
		set (with_systemd_service "yes")
	else()
		set (with_systemd_service "no")
	endif()
endif()

########### variables defined at compil time ###############

set (CAIRO_DOCK_SHARE_DATA_DIR ${pkgdatadir})
set (CAIRO_DOCK_SHARE_THEMES_DIR ${pkgdatadir}/themes)
#set (CAIRO_DOCK_MODULES_DIR ${libdir}/cairo-dock)
set (CAIRO_DOCK_LOCALE_DIR "${prefix}/${CMAKE_INSTALL_LOCALEDIR}")
set (CAIRO_DOCK_PLUGINS_LOCALE_DIR "${plugins-locale}")
set (CAIRO_DOCK_THEMES_DIR "themes")  # folder name where themes are saved locally, relatively to the root folder of Cairo-Dock.
set (CAIRO_DOCK_DISTANT_THEMES_DIR "themes3.4")  # folder name where themes are on the server, relatively to the root folder of the server files.
set (CAIRO_DOCK_GETTEXT_PACKAGE ${GETTEXT_PACKAGE})
set (CAIRO_DOCK_PLUGINS_GETTEXT_PACKAGE "${plugins-gettext}")

set (GLDI_GETTEXT_PACKAGE ${GETTEXT_PACKAGE})
set (GLDI_SHARE_DATA_DIR ${pkgdatadir})
if (NOT "${pluginsdatarootdir}" STREQUAL "${pkgdatadir}")
	set (GLDI_PLUGINS_DATA_ROOT "${pluginsdatarootdir}")
endif()
set (GLDI_MODULES_DIR ${pluginsdir})
if (NOT "${pluginsdir}" STREQUAL "${libdir}/cairo-dock")
	set (GLDI_MODULES_DIR_CORE "${libdir}/cairo-dock")
endif()
set (GLDI_BIN_DIR ${bindir})

########### next steps ###############

add_subdirectory (src)
add_subdirectory (data)
add_subdirectory (po)

############# HELP #################
# this is actually a plug-in for cairo-dock, not for gldi
# it uses some functions of cairo-dock (they are binded dynamically), that's why it can't go with other plug-ins
set (GETTEXT_HELP ${GETTEXT_PACKAGE})
set (VERSION_HELP "0.11.1")
set (PACKAGE_HELP "cd-Help")
set (helpdatadir "${pkgdatadir}/plug-ins/Help")
set (dock_version "${VERSION}")
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/Help/data/Help.conf.in ${CMAKE_CURRENT_BINARY_DIR}/Help/data/Help.conf)
add_subdirectory (Help)

########### file generation ###############

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/src/gldit/gldi-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/gldit/gldi-config.h)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/src/gldit/gldi-module-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/gldit/gldi-module-config.h)  # separated from gldi-config.h because it's architecture-dependant (it contains $libdir), so it can't be installed in /usr/include without causing a conflict between 32 and 64 bits packages.
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
MESSAGE (STATUS " * With EGL support    : ${with_egl}")
MESSAGE (STATUS " * With Wayland support: ${with_wayland}")
if (WAYLAND_FOUND)
	MESSAGE (STATUS " * With Wayland taskbar: ${with_wayland_taskbar}")
	if (JSON_FOUND)
		MESSAGE (STATUS " * With Wayfire IPC    : yes")
	else()
		MESSAGE (STATUS " * With Wayfire IPC    : no")
	endif()
endif()
MESSAGE (STATUS " * With gtk-layer-shell: ${with_gtk_layer_shell}")
if (HAVE_LIBCRYPT)
	MESSAGE (STATUS " * Crypt passwords     : yes")
else()
	MESSAGE (STATUS " * Crypt passwords     : no")
endif()
if (enable-desktop-manager)
	if (gnome-session-use-systemd)
		set (with_cd_session "yes (new gnome-session, purely systemd-based)")
	else ()
		set (with_cd_session "yes (old gnome-session, using components)")
	endif ()
else()
	set (with_cd_session "no (use '-Denable-desktop-manager=True' to enable it)")
endif()
MESSAGE (STATUS " * Cairo-dock session  : ${with_cd_session}")
MESSAGE (STATUS " * Systemd service unit: ${with_systemd_service}")
MESSAGE (STATUS " * Themes directory    : ${CAIRO_DOCK_DISTANT_THEMES_DIR} (on the server)")
MESSAGE (STATUS)
