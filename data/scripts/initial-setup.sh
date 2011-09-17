#!/bin/bash

# Script for the Help applet of Cairo-Dock
#
# Copyright : (C) see the 'copyright' file.
# E-mail    : see the 'copyright' file.
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 3
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# http://www.gnu.org/licenses/licenses.html#GPL

# Note: we have to use $HOME instead of '~'

# Enable icons in menus and buttons in Gnome
if test -n "`which gsettings`"; then
	gsettings set org.gnome.desktop.interface buttons-have-icons true 2> /dev/null
	gsettings set org.gnome.desktop.interface menus-have-icons true 2> /dev/null
fi
if test -n "`which gconftool-2`"; then  # both gsettings and gconf can be present on the system, so we do both.
	GCONFTOOL_CHECK=1
	gconftool-2 -s '/desktop/gnome/interface/buttons_have_icons' --type bool true
	gconftool-2 -s '/desktop/gnome/interface/menus_have_icons' --type bool true
fi

# Enable dbus, scale and expo plug-ins in Compiz.
if test -n "`which compiz`"; then
	if test -d "$HOME/.config/compiz"; then # compiz < 0.9
		# flat file
		if test -f "$HOME/.config/compiz/compizconfig/Default.ini"; then
			sed -i "/as_active_plugins/ s/.*/&;dbus;scale;expo/g" $HOME/.config/compiz/compizconfig/Default.ini
		fi
		# gconf
		if test -n "$GCONFTOOL_CHECK"; then
			plugins=`gconftool-2 -g /apps/compiz/general/allscreens/options/active_plugins`
			gconftool-2 -s /apps/compiz/general/allscreens/options/active_plugins --type=list --list-type=string "${plugins:0:${#plugins}-1},dbus,scale,expo]"  # plug-ins in double are filtered by Compiz.
		fi
	fi
	if test -d "$HOME/.config/compiz-1"; then # compiz >= 0.9 => we can have compiz and compiz-1
		# plug-ins in double are NO LONGER filtered by Compiz in this version... (and if plugins in double, compiz crashes :) )
		# flat file
		if test -f "$HOME/.config/compiz-1/compizconfig/Default.ini"; then
			pluginsFlat=`grep "s0_active_plugins" $HOME/.config/compiz-1/compizconfig/Default.ini`
			for i in dbus scale expo; do
				if test `echo $pluginsFlat | grep -c $i` -eq 0; then
					pluginsFlat="$pluginsFlat""$i;"
					sed -i "/s0_active_plugins/ s/.*/&$i;/g" $HOME/.config/compiz-1/compizconfig/Default.ini
				fi
			done
		fi
		# gconf
		if test -n "$GCONFTOOL_CHECK"; then
			plugins=`gconftool-2 -g /apps/compiz-1/general/screen0/options/active_plugins`
			for i in dbus scale expo; do
				if test `echo $plugins | grep -c $i` -eq 0; then
					plugins=${plugins:0:${#plugins}-1},$i]
					gconftool-2 -s /apps/compiz-1/general/screen0/options/active_plugins --type=list --list-type=string "$plugins"
				fi
			done
		fi
	fi
fi

exit 0
