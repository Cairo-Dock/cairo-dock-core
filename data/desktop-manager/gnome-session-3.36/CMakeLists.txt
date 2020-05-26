install (FILES
	cairo-dock-compiz.desktop
	cairo-dock-metacity.desktop
	DESTINATION /usr/share/xsessions)

install (FILES
	cairo-dock-compiz.session
	cairo-dock-metacity.session
	DESTINATION /usr/share/gnome-session/sessions)

install (FILES
	gnome-session-x11@cairo-dock-compiz.target
	gnome-session-x11@cairo-dock-metacity.target
	DESTINATION /usr/lib/systemd/user)
