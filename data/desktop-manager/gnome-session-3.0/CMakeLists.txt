install (FILES
	cairo-dock.desktop
	cairo-dock-fallback.desktop
	cairo-dock-unity.desktop
	DESTINATION /usr/share/xsessions)

install (FILES
	cairo-dock.session
	cairo-dock-fallback.session
	cairo-dock-unity.session
	cairo-dock-unity-fallback.session
	DESTINATION /usr/share/gnome-session/sessions)
