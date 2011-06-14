#!/bin/sh

###
### initialize all
###
cd ..
export CAIRO_DOCK_DIR=`pwd`/../..
export CORE_DIR=${CAIRO_DOCK_DIR}/cairo-dock-core
export PLUGINS_DIR=${CAIRO_DOCK_DIR}/cairo-dock-plug-ins
export PLUGINS_EXTRA_DIR=${CAIRO_DOCK_DIR}/cairo-dock-plug-ins-extras

while getopts "cpt" flag
do
	echo " option $flag $OPTIND $OPTARG"
	case "$flag" in
	c)
		echo " => core"
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
	for c in data/*.conf.in data/*.desktop
	do # les .conf peuvent être dans un dossier build et il ne reste plus que des .conf.in.
		#if test ${c:0:10} != "cairo-dock"; then  # on exclut les cairo-dock*.desktop
			$CAIRO_DOCK_EXTRACT_MESSAGE $c
		#fi
	done;
	$CAIRO_DOCK_EXTRACT_MESSAGE data/ChangeLog.txt
fi

echo "generating translation files for the core ..."
cd po
$CAIRO_DOCK_GEN_TRANSLATION core

###
### update cairo-dock-plugins.po
###
cd ${PLUGINS_DIR}
echo "extracing the messages of the plug-ins ..."
for plugin in *
do
	if test -d $plugin; then
		cd $plugin
		if test -e CMakeLists.txt; then  # le Makefile peut etre dans un autre dossier
			echo "  extracting sentences from $plugin ..."
			if test -x $CAIRO_DOCK_EXTRACT_MESSAGE; then
				rm -f data/messages
				for c in data/*.conf.in  # provoque un message d'erreur si le plug-in n'a pas de fichier de conf, ce qu'on peut ignorer.
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
$CAIRO_DOCK_GEN_TRANSLATION plug-ins

###
### update cairo-dock-plugins-extra.po
###
cd ${PLUGINS_EXTRA_DIR}
echo "extracing the messages of third-party applets ..."
for applet in *
do
	if test -d $applet; then
		cd $applet
		if test -e auto-load.conf; then
			echo "  extracting sentences from $applet ..."
			if test -x $CAIRO_DOCK_EXTRACT_MESSAGE; then
				rm -f messages
				$CAIRO_DOCK_EXTRACT_MESSAGE ${applet}.conf
			fi
		fi
		cd ..
	fi
done;

echo "generating translation files for third-party applets ..."
cd po
$CAIRO_DOCK_GEN_TRANSLATION extras


echo "done"
exit 0
