#!/bin/sh

###
### initialize all
###
cd ..
export CD_ALL="0"
export CAIRO_DOCK_DIR=`pwd`/../..
export CORE_DIR=${CAIRO_DOCK_DIR}/cairo-dock-core
export PLUGINS_DIR=${CAIRO_DOCK_DIR}/cairo-dock-plug-ins

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

export CAIRO_DOCK_EXTRACT_MESSAGE=${CORE_DIR}/po/misc/cairo-dock-extract-message
export CAIRO_DOCK_GEN_TRANSLATION=${CORE_DIR}/po/misc/generate-translation.sh

gcc `pkg-config --libs --cflags glib-2.0 cairo-dock` $CAIRO_DOCK_EXTRACT_MESSAGE.c -o $CAIRO_DOCK_EXTRACT_MESSAGE

###
### update cairo-dock.po
###
echo "extracting the messages of the core ..."
cd ${CORE_DIR}
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

echo "generating translation files for the core ..."
cd po
$CAIRO_DOCK_GEN_TRANSLATION

###
### update cairo-dock-plugins.po
###
cd ${PLUGINS_DIR}
echo "extracing the messages of the plug-ins ..."
for plugin in *
do
	if test -d $plugin; then
		cd $plugin
		if test -e Makefile; then
			echo "  extracting sentences from $plugin ..."
			if test -x $CAIRO_DOCK_EXTRACT_MESSAGE; then
				rm -f data/messages
				for c in data/*.conf  # provoque un message d'erreur si le plug-in n'a pas de fichier de conf, ce qu'on peut ignorer.
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


echo "done"
exit 0
