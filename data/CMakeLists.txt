add_subdirectory(default-theme)
add_subdirectory(gauges)
add_subdirectory(emblems)
add_subdirectory(explosion)
add_subdirectory(man)


########### install files ###############

install(FILES  cairo-dock.conf 	cairo-dock-simple.conf 	launcher.desktop 	container.desktop 	separator.desktop 	file.desktop 	themes.conf 	main-dock.conf 	default-icon.svg 	default-icon-appli.svg 	default-indicator.png 	default-class-indicator.svg 	cairo-dock-ok.svg 	cairo-dock-cancel.svg 	hand.svg 	rotate-desklet.svg 	retach-desklet.svg 	depth-rotate-desklet.svg 	cairo-dock-logo.png 	cairo-dock.svg 	readme-default-view 	preview-default.png 	ChangeLog.txt 	icon-gradation.png 	icon-movment.png 	icon-mouse.svg 	icon-subdock.png 	icon-frame.png 	icon-buttons.png 	icon-bubble.png 	icon-wave.png 	icon-icons.svg 	icon-dialogs.svg 	icon-views.svg 	icon-visibility.svg 	icon-labels.svg 	icon-background.svg 	icon-taskbar.png 	icon-desklets.png 	icon-hidden-dock.svg 	icon-indicators.svg 	icon-system.svg 	icon-lock-icons.svg 	icon-position.svg 	icon-behavior.svg 	icon-appearance.svg 	icon-accessories.svg 	icon-desktop.svg 	icon-controler.svg 	icon-connection.svg 	icon-plug-ins.svg 	icon-all.svg 	icon-extensions.svg 	cairo-dock-animated.xpm 	help.conf 	icon-help.svg 	icon-close.png 	icon-restore.png 	icon-minimize.png 	icon-maximize.png 	box-front.png 	box-back.png 	no-input-desklet.png DESTINATION ${pkgdatadir})
install(FILES  cairo-dock.svg DESTINATION ${datadir}/pixmaps)
install(FILES  cairo-dock.desktop 	cairo-dock-cairo.desktop DESTINATION ${datadir}/applications)



#original Makefile.am contents follow:

#
#SUBDIRS = . default-theme gauges emblems explosion
#
#essentialdatadir = ${pkgdatadir}
#
#essentialdata_DATA = \
#	cairo-dock.conf\
#	cairo-dock-simple.conf\
#	launcher.desktop\
#	container.desktop\
#	separator.desktop\
#	file.desktop\
#	themes.conf\
#	main-dock.conf\
#	default-icon.svg\
#	default-icon-appli.svg\
#	default-indicator.png\
#	default-class-indicator.svg\
#	cairo-dock-ok.svg\
#	cairo-dock-cancel.svg\
#	hand.svg\
#	rotate-desklet.svg\
#	retach-desklet.svg\
#	depth-rotate-desklet.svg\
#	cairo-dock-logo.png\
#	cairo-dock.svg\
#	readme-default-view\
#	preview-default.png\
#	ChangeLog.txt\
#	icon-gradation.png\
#	icon-movment.png\
#	icon-mouse.svg\
#	icon-subdock.png\
#	icon-frame.png\
#	icon-buttons.png\
#	icon-bubble.png\
#	icon-wave.png\
#	icon-icons.svg\
#	icon-dialogs.svg\
#	icon-views.svg\
#	icon-visibility.svg\
#	icon-labels.svg\
#	icon-background.svg\
#	icon-taskbar.png\
#	icon-desklets.png\
#	icon-hidden-dock.svg\
#	icon-indicators.svg\
#	icon-system.svg\
#	icon-lock-icons.svg\
#	icon-position.svg\
#	icon-behavior.svg\
#	icon-appearance.svg\
#	icon-accessories.svg\
#	icon-desktop.svg\
#	icon-controler.svg\
#	icon-connection.svg\
#	icon-plug-ins.svg\
#	icon-all.svg\
#	icon-extensions.svg\
#	cairo-dock-animated.xpm\
#	help.conf\
#	icon-help.svg\
#	icon-close.png\
#	icon-restore.png\
#	icon-minimize.png\
#	icon-maximize.png\
#	box-front.png\
#	box-back.png\
#	no-input-desklet.png
#	
#autostartdatadir = $(datadir)/applications
#autostartdata_DATA = \
#	cairo-dock.desktop\
#	cairo-dock-cairo.desktop
#
#icondatadir = $(datadir)/pixmaps
#icondata_DATA = \
#	cairo-dock.svg
#
#EXTRA_DIST = $(essentialdata_DATA)\
#	$(autostartdata_DATA)\
#	$(icondata_DATA)
