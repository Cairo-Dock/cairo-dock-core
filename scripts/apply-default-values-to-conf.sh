#!/bin/sh

export CAIRO_DOCK_DIR="$HOME/.config/cairo-dock"

if test "x$1" = "x"; then
	export CURRENT_THEME_DIR="$CAIRO_DOCK_DIR/current_theme"
	echo "repertoire du theme : ${CURRENT_THEME_DIR}"
else
	export CURRENT_THEME_DIR="$1"
fi

if test ! -d "$CURRENT_THEME_DIR"; then
	echo "wrong theme path ($CURRENT_THEME_DIR)"
	exit 1
fi

export CURRENT_CONF_FILE=""

set_value()  # (group, key, value)
{
	sed -i "/^\[$1\]/,/^\[.*\]/ s/^$2 *=.*/$2 = $3/g" "${CURRENT_CONF_FILE}"
	echo -n "."
}

get_value()  # (group, key) -> value
{
	sed -n "/^\[$1\]/,/^\[.*\]/ {/^$2 *=.*/p}" "${CURRENT_CONF_FILE}" | sed "s/^$2 *= *//g"
}

set_value_on_all_groups()  # (key, value)
{
	sed -i "s/^$1 *=.*/$1 = $2/g" "${CURRENT_CONF_FILE}"
	echo -n "."
}

set_current_conf_file()  # (conf file)
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
set_value "Position"		"x gap"				0
set_value "Position"		"y gap"				0
set_value "Position"		"xinerama"			false
set_value "Accessibility"	"max autorized width"	0
set_value "Accessibility"	"visibility"		4
set_value "Accessibility"	"leaving delay"		250
set_value "Accessibility"	"show delay"		300
set_value "Accessibility"	"lock icons"		false
set_value "Accessibility"	"lock all"			false
set_value "Accessibility"	"show_on_click"		0
set_value "TaskBar"		"show applications"		true
#set_value "TaskBar"		"hide visible"			false
#set_value "TaskBar"		"current desktop only"		false
#set_value "TaskBar"		"group by class"		true
set_value "TaskBar"		"group exception"		""
set_value "TaskBar"		"mix launcher appli"	true
set_value "TaskBar"		"overwrite xicon"		true
set_value "TaskBar"		"overwrite exception"	""
set_value "TaskBar"		"minimized"				1
set_value "TaskBar"		"minimize on click"		true
set_value "TaskBar"		"close on middle click"		true
set_value "TaskBar"		"demands attention with dialog" true
set_value "TaskBar"		"demands attention with dialog" true
set_value "TaskBar"		"duration" 				2
set_value "TaskBar"		"animation on active window" 	"wobbly"
set_value "TaskBar"		"max name length"		20
set_value "TaskBar"		"visibility alpha"		"0.35"
set_value "TaskBar"		"animate subdocks"		true
set_value "System"		"unfold duration"		300
set_value "System"		"grow nb steps" 		10
set_value "System"		"shrink nb steps"		8
set_value "System"		"move up nb steps"		10
set_value "System"		"move down nb steps"	16
set_value "System"		"refresh frequency"		35
set_value "System"		"dynamic reflection"	false
set_value "System"		"opengl anim freq"		33
set_value "System"		"cairo anim freq"		25
set_value "System"		"always horizontal"		true
set_value "System"		"show hidden files"		false
set_value "System"		"fake transparency"		false
set_value "System"		"config transparency"	false
set_value "System"		"conn use proxy"		false
set_value "System"		"conn timeout"			7
set_value "Dialogs"		"custom"				false
set_value "Labels"		"custom"				false
modules=`get_value "System" "modules"`
echo $modules | grep "icon effects"
if test $? = 1; then
	modules="${modules};icon effects"
	set_value "System" "modules" "$modules"
fi
echo $modules | grep "illusion"
if test $? = 1; then
	modules="${modules};illusion"
	set_value "System" "modules" "$modules"
fi
echo $modules | grep "Dbus"
if test $? = 1; then
	modules="${modules};Dbus"
	set_value "System" "modules" "$modules"
fi
#set_value "System"		"modules"				"dock rendering;dialog rendering;Animated icons;drop indicator;clock;logout;dustbin;stack;shortcuts;GMenu;switcher;icon effects;illusion"

set_current_conf_file "plug-ins/Animated-icons/Animated-icons.conf"
set_value "Rotation"		"color"						"1;1;1;0"

set_current_conf_file "plug-ins/Clipper/Clipper.conf"
set_value "Configuration"	"item type"					3
set_value "Configuration"	"paste selection"			true
set_value "Configuration"	"paste clipboard"			true
set_value "Configuration"	"persistent"				""
set_value "Configuration"	"enable actions"			false

set_current_conf_file "plug-ins/clock/clock.conf"
set_value "Icon"			"name"						""
desklet=`get_value "Desklet" "initially detached"`
if test "$desklet" = "false"; then
	set_value "Configuration"	"show date"				2
	set_value "Configuration"	"show seconds"			false
fi

set_current_conf_file "plug-ins/drop_indicator/drop_indicator.conf"
set_value "Drag and drop indicator"	"speed" 			2

set_current_conf_file "plug-ins/dustbin/dustbin.conf"
set_value "Icon"			"name"						"Dustbin"
set_value "Configuration"	"additionnal directories"	""
set_value "Configuration"	"alternative file browser"	""

set_current_conf_file "plug-ins/GMenu/GMenu.conf"
set_value "Icon"			"name"						"Applications Menu"
set_value "Configuration"	"has icons"					true
set_value "Configuration"	"show recent"				true

set_current_conf_file "plug-ins/logout/logout.conf"
set_value "Icon"			"name"						"Log-out"
set_value "Configuration"	"click"						0
set_value "Configuration"	"middle-click"				1

set_current_conf_file "plug-ins/mail/mail.conf"
set_value_on_all_groups		"username"					""
set_value_on_all_groups		"password"					""

set_current_conf_file "plug-ins/quick-browser/quick-browser.conf"
set_value "Icon"			"name"						""
set_value "Configuration"	"dir path"					""

set_current_conf_file "plug-ins/rendering/rendering.conf"
set_value "Inclinated Plane" "vanishing point y"		300
set_value "Curve"			"curvature"					70
set_value "Parabolic"		"curvature"					".3"
set_value "Parabolic"		"ratio"						5
set_value "Slide"			"simple_iconGapX"			50	
set_value "Slide"			"simple_iconGapY"			50	
set_value "Slide"			"simple_fScaleMax"			"1.5"
set_value "Slide"			"simple_arrowShift"			0
set_value "Slide"			"simple_arrowHeight"		15
set_value "Slide"			"simple_arrowWidth"			30
set_value "Slide"			"simple_wide_grid"			true
set_value "Slide"			"simple_max_size"			".7"

set_current_conf_file "plug-ins/RSSreader/RSSreader.conf"
set_value "Icon"			"name"						""
set_value "Configuration"	"url_rss_feed"				""

set_current_conf_file "plug-ins/shortcuts/shortcuts.conf"
set_value "Configuration"	"list network"				false
set_value "Configuration"	"use separator"				false
set_value "Configuration"	"disk usage"				4
set_value "Configuration"	"check interval"			10

set_current_conf_file "plug-ins/slider/slider.conf"
set_value "Configuration"	"directory"					""

set_current_conf_file "plug-ins/stack/stack.conf"
set_value "Configuration"	"stack dir"					""
set_value "Configuration"	"selection_"				false

set_current_conf_file "plug-ins/switcher/switcher.conf"
set_value "Icon"			"name"						""
set_value "Configuration"	"preserve ratio"			false
set_value "Configuration"	"Draw Windows"				true

set_current_conf_file "plug-ins/weather/weather.conf"
set_value "Icon"			"name"						""
set_value "Configuration"	"nb days"					5
set_value "Configuration"	"check interval"			15
set_value "Configuration"	"IS units"					true
set_value "Configuration"	"display temperature"		true

set_current_conf_file "plug-ins/weblets/weblets.conf"
set_value "Configuration"	"weblet URI"				"http:\/\/www.google.com"
set_value "Configuration"	"uri list"					""

set_current_conf_file "plug-ins/Xgamma/Xgamma.conf"
set_value "Configuration"	"initial gamma"				0

echo ""
echo "le theme a ete mis a jour."
exit 0
