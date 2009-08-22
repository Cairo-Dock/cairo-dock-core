#!/bin/sh

export CAIRO_DOCK_DIR="$HOME/.config/cairo-dock"
if test "x$1" = "x"; then
	export CURRENT_THEME_DIR="$CAIRO_DOCK_DIR/current_theme"
	echo "repertoire par defaut : ${CONF_FILE}"
else
	export CURRENT_THEME_DIR="$1"
fi

export CURRENT_CONF_FILE=""

set_value()
{
	sed -i "/^\[$1\]/,/^\[.*\]/ s/^$2 *=.*/$2 = $3/g" "${CURRENT_CONF_FILE}"
	echo -n "."
}

get_value()
{
	sed -n "/^\[$1\]/,/^\[.*\]/ {/^$2 *=.*/p}" "${CURRENT_CONF_FILE}" | sed "s/^$2 *= *//g"
}

set_value_on_all_groups()
{
	sed -i "s/^$1 *=.*/$1 = $2/g" "${CURRENT_CONF_FILE}"
	echo -n "."
}

set_current_conf_file()
{
	if test -e "$1"; then
		echo "applying default values to "${1%.conf}" ..."
		export CURRENT_CONF_FILE="$1"
	else
		export CURRENT_CONF_FILE=""
	fi
}

cd "$CURRENT_THEME_DIR"

set_current_conf_file "cairo-dock.conf"
set_value "Position"		"x gap"					0
set_value "Position"		"y gap"					0
set_value "Position"		"xinerama"				false
set_value "Accessibility"	"auto-hide"				false
set_value "Accessibility"	"reserve space"			true
set_value "Accessibility"	"max autorized width"	0
set_value "Accessibility"	"pop-up"				false
set_value "Accessibility"	"raise shortcut"		""
set_value "Accessibility"	"leaving delay"			250
set_value "Accessibility"	"show delay"			300
set_value "Accessibility"	"lock icons"			false
set_value "TaskBar"			"show applications"		true
set_value "TaskBar"			"unique PID"			false
set_value "TaskBar"			"group by class"		false
set_value "TaskBar"			"group exception"		""
set_value "TaskBar"			"hide visible"			false
set_value "TaskBar"			"current desktop only"	false
set_value "TaskBar"			"mix launcher appli"	true
set_value "TaskBar"			"overwrite xicon"		true
set_value "TaskBar"			"overwrite exception"	"pidgin;xchat;amsn;gimp"
set_value "TaskBar"			"window thumbnail"		true
set_value "TaskBar"			"minimize on click"		true
set_value "TaskBar"			"close on middle click"	true
set_value "TaskBar"			"auto quick hide"		false
set_value "TaskBar"			"auto quick hide on max" false
set_value "TaskBar"			"demands attention with dialog" true
set_value "TaskBar"			"animation on demands attention" "rotate"
set_value "TaskBar"			"animation on active window" "wobbly"
set_value "TaskBar"			"max name length"		15
set_value "TaskBar"			"visibility alpha"		"0.4"
set_value "TaskBar"			"animate subdocks"		true
set_value "System"			"unfold factor"			8
set_value "System"			"grow nb steps" 		10
set_value "System"			"shrink nb steps"		8
#set_value "System"			"move down speed"		"0.25"
set_value "System"			"refresh frequency"		35
set_value "System"			"dynamic reflection"	false
set_value "System"			"opengl anim freq"		33
set_value "System"			"cairo anim freq"		25
set_value "System"			"always horizontal"		true
set_value "System"			"show hidden files"		false
set_value "System"			"fake transparency"		false
#set_value "System"			"modules"				"dock rendering;dialog rendering;Animated icons;drop indicator;clock;logout;dustbin;shortcuts;GMenu;switcher;icon effects;illusion"

set_current_conf_file "plug-ins/mail/mail.conf"
set_value_on_all_groups		"username"				"toto"
set_value_on_all_groups		"password"				"***"

set_current_conf_file "plug-ins/slider/slider.conf"
set_value "Configuration"	"directory"				""

set_current_conf_file "plug-ins/stack/stack.conf"
set_value "Configuration"	"stack dir"				""

set_current_conf_file "plug-ins/Clipper/Clipper.conf"
set_value "Configuration"	"persistent"			""

set_current_conf_file "plug-ins/shortcuts/shortcuts.conf"
set_value "Module"			"list network"			false

set_current_conf_file "plug-ins/Xgamma/Xgamma.conf"
set_value "Configuration"	"initial gamma"			0

set_current_conf_file "plug-ins/weblets/weblets.conf"
set_value "Configuration"	"weblet URI"			"http://www.google.com"
set_value "Configuration"	"uri list"				""

echo ""
echo "le theme a ete mis a jour."
exit 0
