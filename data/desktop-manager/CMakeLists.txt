# (we assume GTK and Gnome-Session are using the same version)
if (${GTK_MAJOR} LESS 3 OR (${GTK_MAJOR} EQUAL 3 AND ${GTK_MINOR} LESS 8)) # GTK < 3.8
	add_subdirectory(gnome-session-3.0)
elseif(${GTK_MAJOR} EQUAL 3 AND ${GTK_MINOR} EQUAL 8)
	add_subdirectory(gnome-session-3.8)
else()
	add_subdirectory(gnome-session-3.10)
endif()

install (FILES cairo-dock-session
	DESTINATION ${bindir}
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
