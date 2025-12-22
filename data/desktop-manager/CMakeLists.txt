if(gnome-session-use-systemd)
	add_subdirectory(gnome-session-49)
else()
	# Here we assume some link between GTK versions and gnome-session/gnome-flashback versions
	if(${GTK_MAJOR} EQUAL 3 AND ${GTK_MINOR} LESS 24)  # GTK < 3.24 (3.22)
		add_subdirectory(gnome-session-3.28)
	else()  # GTK >= 3.24
		add_subdirectory(gnome-session-3.36)
	endif()
endif()
