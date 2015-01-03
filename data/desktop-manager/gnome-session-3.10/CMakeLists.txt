# By default the session is GNOME except on Ubuntu: better to use Unity to not
# have specific features of some apps created for Gnome-Shell (e.g. the new
# Nautilus or GThumb interface which are not really compatible with Compiz)

set (DESKTOP_NAME "GNOME")
set (SETTINGS_DAEMON "gnome")

get_filename_component(ISSUE_NET "/etc/issue.net" ABSOLUTE)
if (EXISTS ${ISSUE_NET})
	FILE(READ "${ISSUE_NET}" DISTRO_RELEASE)
	if (NOT "${DISTRO_RELEASE}" STREQUAL "")
		STRING (REGEX REPLACE " (.+)" "" DISTRO_ID ${DISTRO_RELEASE})
		message(STATUS ${DISTRO_ID})
		if ("${DISTRO_ID}" STREQUAL "Ubuntu")
			set (DESKTOP_NAME "Unity")
			if(${GTK_MAJOR} GREATER 3 OR (${GTK_MAJOR} EQUAL 3 AND ${GTK_MINOR} GREATER 12))
				set (SETTINGS_DAEMON "unity")
			endif()
		endif()
	endif()
endif()

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cairo-dock.session.in ${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.session)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cairo-dock.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.desktop)

install (FILES
	${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.desktop
	DESTINATION /usr/share/xsessions)

install (FILES
	${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.session
	DESTINATION /usr/share/gnome-session/sessions)
