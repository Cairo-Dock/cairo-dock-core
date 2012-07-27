
########### install files ###############

# scripts
install (FILES
	cairo-dock-package-theme.sh
	help_scripts.sh
	initial-setup.sh
	DESTINATION ${pkgdatadir}/scripts
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

