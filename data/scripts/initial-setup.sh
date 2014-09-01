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

# Enable dbus, scale and expo plug-ins in Compiz.
if test -n "`which compiz`"; then
	if test -n "`which gconftool-2`"; then
		GCONFTOOL_CHECK=1
	fi

	## Compiz < 0.9 ##

	if test -d "$HOME/.config/compiz"; then # compiz < 0.9
		echo "Compiz: old version detected"
		# flat file
		if test -f "$HOME/.config/compiz/compizconfig/Default.ini"; then
			echo "Compiz: Flat file detected"
			sed -i "/as_active_plugins/ s/.*/&;dbus;scale;expo/g" $HOME/.config/compiz/compizconfig/Default.ini
		fi
		# gconf
		if test -n "$GCONFTOOL_CHECK"; then
			echo "Compiz: GConf backend detected"
			plugins=`gconftool-2 -g /apps/compiz/general/allscreens/options/active_plugins`
			gconftool-2 -s /apps/compiz/general/allscreens/options/active_plugins --type=list --list-type=string "${plugins:0:${#plugins}-1},dbus,scale,expo]"  # plug-ins in double are filtered by Compiz.
		fi
	fi

	## Compiz >= 0.9 ##

	if test -d "$HOME/.config/compiz-1"; then # compiz >= 0.9 => we can have compiz and compiz-1
		echo "Compiz: 0.9 version detected (GConf/Flat)"
		# plug-ins in double are NO LONGER filtered by Compiz in this version... (and if plugins in double, compiz crashes :) )

		## flat file
		if test -f "$HOME/.config/compiz-1/compizconfig/Default.ini"; then
			pluginsFlat=`grep "s0_active_plugins" $HOME/.config/compiz-1/compizconfig/Default.ini`
			for i in dbus scale expo; do
				if test `echo $pluginsFlat | grep -c $i` -eq 0; then
					echo "Flat file: Enable '$i' plugin"
					pluginsFlat="$pluginsFlat""$i;"
					sed -i "/s0_active_plugins/ s/.*/&$i;/g" $HOME/.config/compiz-1/compizconfig/Default.ini
				fi
			done

			# add a staticswitcher (Alt+Tab) if none is enabled and Unity is not running
			switcher=""
			for i in unity staticswitcher ring shift switcher; do
				if test `echo $pluginsFlat | grep -c $i` -gt 0; then
					switcher=$i
					break
				fi
			done
			if test -z "$switcher"; then
				echo "Flat file: Enable 'staticswitcher' plugin"
				pluginsFlat="$pluginsFlat""staticswitcher;"
				sed -i "/s0_active_plugins/ s/.*/&staticswitcher;/g" $HOME/.config/compiz-1/compizconfig/Default.ini
			fi
		fi

		## gconf
		if test -n "$GCONFTOOL_CHECK"; then
			plugins=`gconftool-2 -g /apps/compiz-1/general/screen0/options/active_plugins`
			if test -n "$plugins"; then
				for i in dbus scale expo; do
					if test `echo $plugins | grep -c $i` -eq 0; then
						echo "GConf: Enable '$i' plugin"
						plugins="${plugins:0:${#plugins}-1},$i]"
						gconftool-2 -s /apps/compiz-1/general/screen0/options/active_plugins --type=list --list-type=string "$plugins"
					fi
				done

				# add a staticswitcher (Alt+Tab) if none is enabled and Unity is not running
				switcher=""
				for i in unity staticswitcher ring shift switcher; do
					if test `echo $plugins | grep -c $i` -gt 0; then
						switcher=$i
						break
					fi
				done
				if test -z "$switcher"; then
					plugins="${plugins:0:${#plugins}-1},staticswitcher]"
					echo "GConf: Enable 'staticswitcher' plugin"
					gconftool-2 -s /apps/compiz-1/general/screen0/options/active_plugins --type=list --list-type=string "$plugins"
				fi
			else # it's possible that the gconf is empty
				plugins="[core,composite,opengl,compiztoolbox,decor,vpswitch,snap,mousepoll,resize,place,move,wall,grid,regex,imgpng,session,gnomecompat,animation,fade,staticswitcher,workarounds,scale,expo,ezoom,dbus]"
				gconftool-2 -s /apps/compiz-1/general/screen0/options/active_plugins --type=list --list-type=string "$plugins"
			fi
		fi
	fi

	## GSettings ##

	profile=`gsettings get org.compiz current-profile | sed -e "s/'//g"`
	if test -n "$profile"; then ## >= Compiz 0.9.11
		echo "Compiz: GSettings backend detected ('$profile' profile)"
		# restrict to the current profile except if it's Unity
		# => also update Default if the user wants to use the Cairo-Dock session later.
		if test "$profile" = "unity"; then
			profiles="unity Default"
		else
			profiles="$profile"
		fi

		for p in $profiles; do
			plugins=`gsettings get org.compiz.core:/org/compiz/profiles/$p/plugins/core/ active-plugins`
			if test -n "$plugins"; then
				for i in dbus scale expo; do
					if test `echo $plugins | grep -c "'$i'"` -eq 0; then
						echo "GSettings: Enable '$i' plugin for '$p' profile"
						plugins="${plugins:0:${#plugins}-1}, '$i']" # remove last char (']') and add the new plugin between quotes + ']'
						gsettings set org.compiz.core:/org/compiz/profiles/$p/plugins/core/ active-plugins "$plugins"
					fi
				done

				# add a staticswitcher (Alt+Tab) if none is enabled and Unity is not running
				switcher=""
				for i in unity staticswitcher ring shift switcher; do
					if test `echo $plugins | grep -c "'$i'"` -gt 0; then
						switcher=$i
						break
					fi
				done
				if test -z "$switcher"; then
					plugins="${plugins:0:${#plugins}-1}, 'staticswitcher']"
					echo "GSettings: Enable 'staticswitcher' plugin for '$p' profile"
					gsettings set org.compiz.core:/org/compiz/profiles/$p/plugins/core/ active-plugins "$plugins"
				fi
			else
				echo "GSettings: enable default plugins for '$p' profile"
				plugins="['core', 'composite', 'opengl', 'compiztoolbox', 'decor', 'vpswitch', 'snap', 'mousepoll', 'resize', 'place', 'move', 'wall', 'grid', 'regex', 'imgpng', 'session', 'gnomecompat', 'animation', 'fade', 'workarounds', 'scale', 'expo', 'ezoom', 'dbus', 'staticswitcher']"
				gsettings set org.compiz.core:/org/compiz/profiles/$p/plugins/core/ active-plugins "$plugins"
			fi
		done
	fi
fi

exit 0
