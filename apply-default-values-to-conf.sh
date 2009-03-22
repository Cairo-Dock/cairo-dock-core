#!/bin/sh
if test "x$1"="x"; then
	export CONF_FILE="cairo-dock.conf"
else
	export CONF_FILE="$1/cairo-dock.conf"

set_value()
{
	sed -i "s/^$1 *=.*/$1 = $2/g" "${CONF_FILE}"
	echo -n "."
}

set_value "auto-hide"				false
set_value "reserve space"			true
set_value "x gap"					0
set_value "y gap"					0
set_value "xinerama"				false
set_value "max autorized width"		0
set_value "pop-up"					false
set_value "raise shortcut"			""
set_value "leaving delay"			250
set_value "show delay"				300
set_value "lock icons"				false
set_value "show applications"		true
set_value "unique PID"				false
set_value "group by class"			false
set_value "group exception"			""
set_value "hide visible"			false
set_value "current desktop only"	false
set_value "mix launcher appli"		true
set_value "overwrite xicon"			true
set_value "overwrite exception"		"pidgin;xchat;amsn"
set_value "window thumbnail"		true
set_value "minimize on click"		true
set_value "close on middle click"	true
set_value "auto quick hide"			false
set_value "auto quick hide on max"	false
set_value "demands attention with dialog" true
set_value "animation on demands attention" "rotate"
set_value "animation on active window" "wobbly"
set_value "max name length"			15
set_value "visibility alpha"		"0.2"
set_value "animate subdocks"		true
set_value "unfold factor"		8
set_value "grow nb steps" 		10
set_value "shrink nb steps"		8
#set_value "move down speed"		"0.25"
set_value "refresh frequency"		35
set_value "dynamic reflection"		false
set_value "opengl anim freq"		33
set_value "cairo anim freq"			25
set_value "always horizontal"		true
set_value "show hidden files"		false
set_value "fake transparency"		false
set_value "modules"					"dock-rendering;dialog-rendering;Animated icons;clock;logout;dustbin;stack;shortcuts;GMenu;switcher;icon effects;illusion"

echo ""
echo "le thème a été mis à jour."
exit 0
