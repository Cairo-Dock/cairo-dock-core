install (FILES
	cairo-dock-compiz.desktop
	cairo-dock-metacity.desktop
	DESTINATION ${datadir}/xsessions)

install (FILES
	cairo-dock-compiz.session
	cairo-dock-metacity.session
	DESTINATION ${datadir}/gnome-session/sessions)
