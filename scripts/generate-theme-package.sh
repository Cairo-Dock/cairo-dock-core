#!/bin/sh

if test "x$1" = "x"; then
	echo "usage : $0 $1"
	exit (1)
fi
export THEME_NAME="$1"

export CAIRO_DOCK_DIR="$HOME/.config/cairo-dock"
export CURRENT_THEME_DIR="$CAIRO_DOCK_DIR/current_theme"
export CURRENT_CONF_FILE=""
export THEME_SERVER "http://themes.glx-dock.org"
export INSTALL_DIR="/usr/share/cairo-dock"

set_value()
{
	sed -i "/^\[$1\],^\[.*\]/ s/^$2 *=.*/$2 = $3/g" "${CURRENT_CONF_FILE}"
}

get_value()
{
	sed "/^\[$1\],^\[.*\]/ {/^$2 *=.*/p}" "${CURRENT_CONF_FILE}" | sed "s/^$2 *= *//g"
}

set_current_conf_file()
{
	if test -e "$1"; then
		echo "packaging module "${1%.conf}" ..."
		export CURRENT_CONF_FILE="$1"
	else
		export CURRENT_CONF_FILE=""
	fi
	
}

import_file()
{
	if test "x$CURRENT_CONF_FILE" = "x"; then
		return
	fi
	static file=`get_value "$1" "$2"`
	echo "$2 : $file"
	if test ${file:0:1} = "/" -o ${file:0:1} = "~"; then
		static local_file=${file##*/}
		echo "  => $local_file"
		/bin/cp "$file" "$3"
		set_value "Hidden dock" "callback image" "$local_file"
	fi
}


_import_theme()
{
	if test "x$CURRENT_CONF_FILE" = "x"; then
		return
	fi
	static theme=`get_value "$1" "$2"`
	if test "x${theme}" != "; then
		#\__________ On cherche si ce theme est un theme officiel ou non.
		echo " un theme est present ($theme)"
		wget "$THEME_SERVER/$3/liste.txt" -O "liste.tmp" -t 3 -T 30
		if test -f "liste.tmp" ; then
			grep "^${theme}" "liste.tmp"
			if test "$?" != "0" -a  "$theme" != "Classic" -a "$theme" != "default"; then  # pas un theme officiel
				echo "  ce n'est pas un theme officiel"
				#\__________ On cherche le chemin de ce theme.
				static theme_path=""
				if test -e "${INSTALL_DIR}/$4/$3/themes/${theme}"; then
					theme_path="${INSTALL_DIR}/$4/$3/themes/${theme}"
				elif test -e "${CAIRO_DOCK_DIR}/extras/$3/${theme}"; then
					theme_path="${CAIRO_DOCK_DIR}/extras/$3/${theme}"
				fi
				#\__________ On le copie.
				echo "  son chemin actuel est : $theme_path"
				if test "x$theme_path" != "x"; then
					echo "on importe $theme_path dans "`pwd`"/extras/$3/$THEME_NAME"
					mkdir "extras/$3"
					/bin/cp -r "$theme_path" "extras/$3/$THEME_NAME"
					set_value "$1" "$2" "$THEME_NAME"
				fi
			fi
		fi
		rm -f "liste.tmp"
	fi
}

import_theme()
{
	_import_theme "$1" "$2" "$3" "plug-ins"
}

import_gauge()
{
	_import_theme "$1" "$2" "gauges" ""
}


cd "$CURRENT_THEME_DIR"
mkdir extras

set_current_conf_file "cairo-dock.conf"
import_file "Hidden dock"	"callback image"		.
import_file "Background"	"background image"		.
import_file "Icons"			"icons bg"				.
import_file "Icons"			"separator image"		.
import_file "Dialogs"		"button_ok image"		.
import_file "Dialogs"		"button_cancel image"	.
import_file "Desklets"		"bg desklet"			.
import_file "Desklets"		"fg desklet"			.
import_file "Desklets"		"rotate image"			.
import_file "Desklets"		"retach image"			.
import_file "Desklets"		"depth rotate image"	.
import_file "Indicators"	"emblem_2"				.
import_file "Indicators"	"active indicator"		.
import_file "Indicators"	"indicator image"		.
import_file "Indicators"	"class indicator"		.

set_current_conf_file "plug-ins/AlsaMixer/AlsaMixer.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"default icon"	.
import_file "Configuration"	"broken icon"	.
import_file "Configuration"	"mute icon"		.

set_current_conf_file "plug-ins/Cairo-Penguin/Cairo-Penguin.conf"
import_theme "Configuration" "theme"		"Cairo-Penguin"

set_current_conf_file "plug-ins/Clipper/Clipper.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/clock/clock.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_theme "Module"		"theme"			"clock"

set_current_conf_file "plug-ins/compiz-icon/compiz-icon.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"default icon"	.
import_file "Configuration"	"broken icon"	.
import_file "Configuration"	"other icon"	.
import_file "Configuration"	"setting icon"	.
import_file "Configuration"	"emerald icon"	.
import_file "Configuration"	"reload icon"	.
import_file "Configuration"	"expo icon"		.
import_file "Configuration"	"wlayer icon"	.

set_current_conf_file "plug-ins/cpusage/cpusage.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_gauge "Configuration"	"theme"

set_current_conf_file "plug-ins/drop-indicator/drop_indicator.conf"
import_file "Configuration" "drop indicator" .

set_current_conf_file "plug-ins/dustbin/dustbin.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Module"		"empty image"	.
import_file "Module"		"full image"	.
theme=`get_value "Module" "empty image"`
if test "x$theme" = "x"; then  # cas special : les images passent avant le theme.
	theme=`get_value "Module" "full image"`
	if test "x$theme" = "x"; then
		import_theme "Module" "theme" "dustbin"
	fi
fi

set_current_conf_file "plug-ins/GMenu/GMenu.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/keyboard-indicator/keyboard-indicator.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"bg image"		.

set_current_conf_file "plug-ins/logout/logout.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/mail/mail.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"no mail image"	.
import_file "Configuration"	"has mail image" .

set_current_conf_file "plug-ins/netspeed/netspeed.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_gauge "Configuration"	"theme"

set_current_conf_file "plug-ins/nVidia/nVidia.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_gauge "Configuration"	"theme"

set_current_conf_file "plug-ins/powermanager/powermanager.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"battery icon"	.
import_file "Configuration"	"charge icon"	.
import_gauge "Configuration"	"theme"

set_current_conf_file "plug-ins/quick-browser/quick-browser.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/rame/rame.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_gauge "Configuration"	"theme"

set_current_conf_file "plug-ins/rhythmbox/rhythmbox.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"default icon"	.
import_file "Configuration"	"play icon"		.
import_file "Configuration"	"stop icon"		.
import_file "Configuration"	"pause icon"	.
import_file "Configuration"	"broken icon"	.

set_current_conf_file "plug-ins/shortcuts/shortcuts.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/showDesklets/showDesklets.conf"
import_file "Icon"			"show image"	.
import_file "Icon"			"hide image"	.

set_current_conf_file "plug-ins/showDesktop/showDesktop.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/slider/slider.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/stack/stack.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"text icon"		.
import_file "Configuration"	"url icon"		.

set_current_conf_file "plug-ins/switcher/switcher.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"default icon"	.

set_current_conf_file "plug-ins/systray/systray.conf"
import_file "Icon"			"icon"			.

set_current_conf_file "plug-ins/terminal/terminal.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/tomboy/tomboy.conf"
import_file "Icon"			"default icon"	.
import_file "Icon"			"close icon"	.
import_file "Icon"			"broken icon"	.

set_current_conf_file "plug-ins/Toons/Toons.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_theme "Configuration" "theme"		"Toons"

set_current_conf_file "plug-ins/weather/weather.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_theme "Configuration" "theme"		"weather"

set_current_conf_file "plug-ins/weblets/weblets.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/wifi/wifi.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"icon_0"		.
import_file "Configuration"	"icon_1"		.
import_file "Configuration"	"icon_2"		.
import_file "Configuration"	"icon_3"		.
import_file "Configuration"	"icon_4"		.
import_file "Configuration"	"icon_5"		.

set_current_conf_file "plug-ins/Xgamma/Xgamma.conf"
import_file "Icon"			"icon"			.
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.

set_current_conf_file "plug-ins/xmms/xmms.conf"
import_file "Desklet"		"bg desklet"	.
import_file "Desklet"		"fg desklet"	.
import_file "Configuration"	"default icon"	.
import_file "Configuration"	"play icon"		.
import_file "Configuration"	"stop icon"		.
import_file "Configuration"	"pause icon"	.
import_file "Configuration"	"broken icon"	.

cd ..
echo "building of the tarball ..."
tar cfz "${THEME_NAME}.tar.gz" current_theme

echo ""
echo "The theme has been packaged. It is available in ~/.config/cairo-dock"
sleep 2

exit 0
