#!/bin/bash

# Enable icons in menus and buttons in Gnome
if test -n "`which gsettings`"; then
	gsettings set org.gnome.desktop.interface buttons-have-icons true 2> /dev/null
	gsettings set org.gnome.desktop.interface menus-have-icons true 2> /dev/null
fi
if test -n "`which gconftool-2`"; then  # both gsettings and gconf can be present on the system, so we do both.
	gconftool-2 -s '/apps/gnome/desktop/interface/buttons-have-icons' --type bool true
	gconftool-2 -s '/apps/gnome/desktop/interface/menus-have-icons' --type bool true
fi

# Enable dbus, scale and expo plug-ins in Compiz.
if test -n "`which compiz`"; then
	# flat file
	sed -i "/as_active_plugins/ s/.*/&;dbus;scale;expo/g" ~/.config/compiz/compizconfig/Default.ini
	# gconf
	plugins=`gconftool-2 -g /apps/compiz/general/allscreens/options/active_plugins`
	gconftool-2 -s /apps/compiz/general/allscreens/options/active_plugins --type=list --list-type=string "${plugins:0:${#plugins}-1},dbus,scale,expo]"  # plug-ins in double are filtered by Compiz.
fi

exit 0
