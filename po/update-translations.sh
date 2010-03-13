#!/bin/sh

###
### initialize all
###

export CD_ALL="0"
export CAIRO_DOCK_DIR=`pwd`/../..

while getopts "a" flag
do
	echo " option $flag $OPTIND $OPTARG"
	case "$flag" in
	a)
		echo " => all languages"
		export CD_ALL="1"
		;;
	*)
		echo "unexpected argument"
		exit 1
		;;
	esac
done

export CAIRO_DOCK_EXTRACT_MESSAGE=${CAIRO_DOCK_DIR}/cairo-dock-core/po/cairo-dock-extract-message
export CAIRO_DOCK_GEN_TRANSLATION=${CAIRO_DOCK_DIR}/cairo-dock-core/po/generate-translation.sh

gcc `pkg-config --libs --cflags glib-2.0 cairo-dock` $CAIRO_DOCK_EXTRACT_MESSAGE.c -o $CAIRO_DOCK_EXTRACT_MESSAGE

###
### update cairo-dock.po
###
echo "extracting the messages of the dock ..."
cd $CAIRO_DOCK_DIR/cairo-dock-core
if test -x $CAIRO_DOCK_EXTRACT_MESSAGE; then
	rm -f data/messages
	for c in data/*.conf data/*.desktop
	do
		#if test ${c:0:10} != "cairo-dock"; then  # on exclut les cairo-dock*.desktop
			$CAIRO_DOCK_EXTRACT_MESSAGE $c
		#fi
	done;
	$CAIRO_DOCK_EXTRACT_MESSAGE data/ChangeLog.txt
fi

echo "generating translation files for the dock ..."
cd po
$CAIRO_DOCK_GEN_TRANSLATION

###
### update cairo-dock-plugins.po
###
cd $CAIRO_DOCK_DIR/cairo-dock-plug-ins
echo "extracing the messages of the plug-ins ..."
for plugin in *
do
	if test -d $plugin; then
		cd $plugin
		if test -e Makefile; then
			echo "  extracting sentences from $plugin ..."
			if test -x $CAIRO_DOCK_EXTRACT_MESSAGE; then
				rm -f data/messages
				for c in data/*.conf
				do
					$CAIRO_DOCK_EXTRACT_MESSAGE "$c"
				done;
			fi
		fi
		cd ..
	fi
done;

echo "generating translation files for plug-ins ..."
cd po
$CAIRO_DOCK_GEN_TRANSLATION all
cd ../..


echo "done"
exit 0
