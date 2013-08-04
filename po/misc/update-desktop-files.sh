#!/bin/sh
# It will add translations in .desktop files
# Note: this script has to be launched after having installed new .mo files

cd .. # po

added_translations() {
	KEY=$1 # Name
	TEXT=$2 # Cairo-Dock (Fallback Mode)
	FILE1=$3 # ../data/cairo-dock-cairo.desktop
	FILE2=$4 # ../data/cairo-dock.desktop or nothing

	echo "\n$KEY=$TEXT" >> $FILE1
	test -n "$FILE2" && echo "\n$KEY=$TEXT" >> $FILE2

	for i in *.po; do
		PO_FILE=`echo $i | cut -d. -f1`
		TRANSLATION=`env LANGUAGE=$PO_FILE gettext -d cairo-dock "$TEXT"`
		if [ "$TRANSLATION" != "$TEXT" ]; then
			NEW_LINE="$KEY[$PO_FILE]=$TRANSLATION"
			echo "$NEW_LINE" >> $FILE1
			test -n "$FILE2" && echo "$NEW_LINE" >> $FILE2
		fi
	done
}

DESKTOP_CAIRO="../data/cairo-dock-cairo.desktop"
DESKTOP_OPENGL="../data/cairo-dock.desktop"

echo "
[Desktop Entry]
Type=Application
Exec=cairo-dock -A
Icon=cairo-dock
Terminal=false" > $DESKTOP_CAIRO

echo "
[Desktop Entry]
Type=Application
Exec=cairo-dock
Icon=cairo-dock
Terminal=false

Name=Cairo-Dock" > $DESKTOP_OPENGL

added_translations "Name" "Cairo-Dock (Fallback Mode)" "$DESKTOP_CAIRO"
added_translations "Comment" "A light and eye-candy dock and desklets for your desktop." "$DESKTOP_CAIRO" "$DESKTOP_OPENGL"
added_translations "GenericName" "Multi-purpose Dock and Desklets" "$DESKTOP_CAIRO" "$DESKTOP_OPENGL"

echo "
Categories=System;" >> $DESKTOP_CAIRO
echo "
Categories=System;" >> $DESKTOP_OPENGL

cd misc
