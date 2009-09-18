#!/bin/sh

###
### initialize all
###

export CD_ALL="0"
export CAIRO_DOCK_DIR=`pwd`/../../..

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

export CAIRO_DOCK_EXTRACT_MESSAGE=${CAIRO_DOCK_DIR}/cairo-dock/utils/cairo-dock-extract-message
export CAIRO_DOCK_GEN_TRANSLATION=${CAIRO_DOCK_DIR}/cairo-dock/cairo-dock-core/po/generate-translation.sh

###
### update cairo-dock.po
###
cd $CAIRO_DOCK_DIR/cairo-dock/cairo-dock-core
echo "generating translation files for the dock ..."
if test -x $CAIRO_DOCK_EXTRACT_MESSAGE; then
	rm -f data/messages
	for c in data/*.conf
	do
		$CAIRO_DOCK_EXTRACT_MESSAGE $c
	done;
	$CAIRO_DOCK_EXTRACT_MESSAGE data/ChangeLog.txt
fi

cd po
$CAIRO_DOCK_GEN_TRANSLATION

###
### update cairo-dock-plugins.po
###
cd $CAIRO_DOCK_DIR/plug-ins/cairo-dock-plug-ins
echo "generating translation files for plug-ins ..."
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

cd po
$CAIRO_DOCK_GEN_TRANSLATION all
cd ../..


echo "done"
exit 0
