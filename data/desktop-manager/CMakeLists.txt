install (FILES
	cairo-dock.desktop
	cairo-dock-fallback.desktop
	DESTINATION /usr/share/xsessions)

install (FILES
	cairo-dock.session
	cairo-dock-fallback.session
	DESTINATION /usr/share/gnome-session/sessions)

install (FILES cairo-dock-session
	DESTINATION ${bindir}
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
