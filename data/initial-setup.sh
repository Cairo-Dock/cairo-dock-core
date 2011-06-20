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
	if test -d "~/.config/compiz"; then # compiz < 0.9
		# flat file
		sed -i "/as_active_plugins/ s/.*/&;dbus;scale;expo/g" ~/.config/compiz/compizconfig/Default.ini
		# gconf
		plugins=`gconftool-2 -g /apps/compiz/general/allscreens/options/active_plugins`
		gconftool-2 -s /apps/compiz/general/allscreens/options/active_plugins --type=list --list-type=string "${plugins:0:${#plugins}-1},dbus,scale,expo]"  # plug-ins in double are filtered by Compiz.
	fi
	if test -d "~/.config/compiz-1"; then # compiz >= 0.9 => we can have compiz and compiz-1
		# plug-ins in double are NO LONGER filtered by Compiz in this version... (and if plugins in double, compiz crashes :) )
		pluginsFlat=`grep "s0_active_plugins" ~/.config/compiz-1/compizconfig/Default.ini`
		plugins=`gconftool-2 -g /apps/compiz-1/general/screen0/options/active_plugins`
		for i in dbus scale expo; do
			# flat file
			if test `echo $pluginsFlat | grep -c $i` -eq 0; then
				pluginsFlat="$pluginsFlat""$i;"
				sed -i "/s0_active_plugins/ s/.*/&$i;/g" ~/.config/compiz-1/compizconfig/Default.ini
			fi
			# gconf
			if test `echo $plugins | grep -c $i` -eq 0; then
				plugins=${plugins:0:${#plugins}-1},$i]
				gconftool-2 -s /apps/compiz-1/general/screen0/options/active_plugins --type=list --list-type=string "$plugins"
			fi
		done
	fi
fi

exit 0
