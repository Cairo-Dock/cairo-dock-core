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

ARG=$1
ARG2=$2
#ARG3=$3
ERROR=0

up_install() {
	if [ "$ARG2" != "no" ]; then
		apt-get update
		apt-get install cairo-dock cairo-dock-plug-ins
	fi
}

addRepo() {
	# $1, repository address
	# $2, a few comments
	if [ "$1" = "" ]; then
		exit
	else
		myRepo="$1"
	fi
	if [ "$2" = "" ]; then
		comments="Additional Repository"
	else
		comments="$2"
	fi

	grep -r "$myRepo" /etc/apt/sources.list* > /dev/null
	if [ $? -eq 1 ]; then
		# the repository isn't in the list.
		echo "$myRepo ## $comments" | sudo tee -a /etc/apt/sources.list
	fi
}

repository() {
	addRepo "deb http://repository.glx-dock.org/ubuntu $(lsb_release -sc) cairo-dock" "Cairo-Dock-Stable"
	wget -q http://repository.glx-dock.org/cairo-dock.gpg -O- | apt-key add -
	up_install
}

ppa() {
	addRepo "deb http://ppa.launchpad.net/cairo-dock-team/ppa/ubuntu $(lsb_release -sc) main" "Cairo-Dock-PPA"
	apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E80D6BF5
	up_install
}

weekly() {
	addRepo "deb http://ppa.launchpad.net/cairo-dock-team/weekly/ubuntu $(lsb_release -sc) main" "Cairo-Dock-PPA-Weekly"
	apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E80D6BF5
	up_install
}

debian_stable() {
	addRepo "deb http://repository.glx-dock.org/debian stable cairo-dock" "Cairo-Dock-Stable"
	wget -q http://repository.glx-dock.org/cairo-dock.gpg -O- | apt-key add -
	up_install
}

debian_unstable() {
	addRepo "deb http://repository.glx-dock.org/debian unstable cairo-dock" "Cairo-Dock-Stable"
	wget -q http://repository.glx-dock.org/cairo-dock.gpg -O- | apt-key add -
	up_install
}

compiz_plugin() {
	if test -d "$HOME/.config/compiz"; then # compiz < 0.9
		# flat file
		if test -f "$HOME/.config/compiz/compizconfig/Default.ini"; then
			sed -i "/as_active_plugins/ s/.*/&;$ARG2/g" $HOME/.config/compiz/compizconfig/Default.ini
		fi
		# gconf
		plugins=`gconftool-2 -g /apps/compiz/general/allscreens/options/active_plugins`
		gconftool-2 -s /apps/compiz/general/allscreens/options/active_plugins --type=list --list-type=string "${plugins:0:${#plugins}-1},$ARG2]"  # plug-ins in double are filtered by Compiz.
	fi

	if test -d "$HOME/.config/compiz-1"; then # compiz >= 0.9 => we can have compiz and compiz-1
		# plug-ins in double are NO LONGER filtered by Compiz in this version... (and if plugins in double, compiz crashes :) )
		# flat file
		if test -f "$HOME/.config/compiz-1/compizconfig/Default.ini"; then
			pluginsFlat=`grep "s0_active_plugins" $HOME/.config/compiz-1/compizconfig/Default.ini`
			if test `echo $pluginsFlat | grep -c $ARG2` -eq 0; then
				pluginsFlat="$pluginsFlat""$ARG2;"
				sed -i "/s0_active_plugins/ s/.*/&$ARG2;/g" $HOME/.config/compiz-1/compizconfig/Default.ini
			fi
		fi
		# gconf
		plugins=`gconftool-2 -g /apps/compiz-1/general/screen0/options/active_plugins`
		if test `echo $plugins | grep -c $ARG2` -eq 0; then
			plugins=${plugins:0:${#plugins}-1},$ARG2]
			gconftool-2 -s /apps/compiz-1/general/screen0/options/active_plugins --type=list --list-type=string "$plugins"
		fi
	fi
}

compiz_new_replace_list_plugins() {
	if test -d "$HOME/.config/compiz-1"; then # only for compiz 0.9
		# flat file
		if test -f "$HOME/.config/compiz-1/compizconfig/Default.ini"; then
			pluginsList="s0_active_plugins = "`echo $ARG2 |sed -e 's/,/;/g'`";" # , => ;
			sed -i "/s0_active_plugins/ s/.*/$pluginsList/g" $HOME/.config/compiz-1/compizconfig/Default.ini
		fi
		# gconf
		gconftool-2 -s /apps/compiz-1/general/screen0/options/active_plugins --type=list --list-type=string "[$ARG2]"
	fi
}

case $ARG in
	"repository")
		repository
	;;
	"ppa")
		ppa
	;;
	"weekly")
		weekly
	;;
	"debian_stable")
		debian_stable
	;;
	"debian_unstable")
		debian_unstable
	;;
	"compiz_plugin")
		compiz_plugin
	;;
	"compiz_new_replace_list_plugins")
		compiz_new_replace_list_plugins
	;;
esac
exit
