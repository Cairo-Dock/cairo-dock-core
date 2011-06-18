add_subdirectory(default-theme)
add_subdirectory(gauges)
add_subdirectory(explosion)
add_subdirectory(man)
add_subdirectory(icons)


########### install files ###############

# images
install (FILES
	# lib
	rotate-desklet.svg
	retach-desklet.svg
	no-input-desklet.png
	depth-rotate-desklet.svg
	cairo-dock-ok.svg
	cairo-dock-cancel.svg
	default-class-indicator.svg
	default-icon-appli.svg
	default-icon.svg
	default-indicator.png
	# implementations
	preview-default.png
	readme-default-view
	box-back.png
	box-front.png
	# appli
	cairo-dock-animated.xpm
	cairo-dock-logo.png
	cairo-dock.svg
	ChangeLog.txt
	icon-accessories.svg
	icon-all.svg
	icon-appearance.svg
	icon-background.svg
	icon-behavior.svg
	icon-bubble.png
	icon-buttons.png
	icon-close.png
	icon-connection.svg
	icon-controler.svg
	icon-desklets.png
	icon-desktop.svg
	icon-dialogs.svg
	icon-extensions.svg
	icon-files.svg
	icon-frame.png
	icon-fun.svg
	icon-gradation.png
	icon-help.svg
	icon-hidden-dock.svg
	icon-icons.svg
	icon-indicators.svg
	icon-internet.svg
	icon-labels.svg
	icon-lock-icons.svg
	icon-maximize.png
	icon-minimize.png
	icon-mouse.svg
	icon-movment.png
	icon-plug-ins.svg
	icon-position.svg
	icon-restore.png
	icon-subdock.png
	icon-system.svg
	icon-taskbar.png
	icon-views.svg
	icon-visibility.svg
	icon-wave.png
	DESTINATION ${pkgdatadir})

# scripts
install (FILES
	help_scripts.sh
	initial-setup.sh
	DESTINATION ${pkgdatadir}
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# conf files
install (FILES
	# lib
	${CMAKE_CURRENT_BINARY_DIR}/main-dock.conf
	${CMAKE_CURRENT_BINARY_DIR}/cairo-dock.conf
	# appli
	${CMAKE_CURRENT_BINARY_DIR}/cairo-dock-simple.conf
	${CMAKE_CURRENT_BINARY_DIR}/themes.conf
	${CMAKE_CURRENT_BINARY_DIR}/help.conf
	${CMAKE_CURRENT_BINARY_DIR}/launcher.desktop
	${CMAKE_CURRENT_BINARY_DIR}/container.desktop
	${CMAKE_CURRENT_BINARY_DIR}/separator.desktop
	DESTINATION ${pkgdatadir})

# appli icon
install (FILES
	cairo-dock.svg
	DESTINATION ${datadir}/pixmaps)

# appli desktop file
install (FILES
	cairo-dock.desktop 		cairo-dock-cairo.desktop
	DESTINATION ${datadir}/applications)
