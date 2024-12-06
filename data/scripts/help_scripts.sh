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

compiz_new_replace_list_plugins() {
	if test -d "$HOME/.config/compiz-1"; then # only for compiz 0.9
		# flat file
		if test -f "$HOME/.config/compiz-1/compizconfig/Default.ini"; then
			pluginsList="s0_active_plugins = "`echo $ARG2 |sed -e 's/,/;/g'`";" # , => ;
			sed -i "/s0_active_plugins/ s/.*/$pluginsList/g" $HOME/.config/compiz-1/compizconfig/Default.ini
		fi
	fi
}

case $ARG in
	"compiz_new_replace_list_plugins")
		compiz_new_replace_list_plugins
	;;
esac
exit
