install (FILES
	cairo-dock-compiz.desktop
	cairo-dock-metacity.desktop
	DESTINATION ${datadir}/xsessions)

install (FILES
	cairo-dock-compiz.session
	cairo-dock-metacity.session
	DESTINATION ${datadir}/gnome-session/sessions)

install (FILES
	gnome-session-x11@cairo-dock-compiz.target
	gnome-session-x11@cairo-dock-metacity.target
	cairo-dock-session-metacity.service
	cairo-dock-session-compiz.service
	DESTINATION ${unitdir})
