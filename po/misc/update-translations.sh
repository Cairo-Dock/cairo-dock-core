#!/bin/sh
# Updates the .po files in the core, plug-ins and third-party po folders.

###
### initialize all and get options
###
cd ..
export CAIRO_DOCK_DIR=`pwd`/../..
export CORE_DIR=${CAIRO_DOCK_DIR}/cairo-dock-core
export PLUGINS_DIR=${CAIRO_DOCK_DIR}/cairo-dock-plug-ins
export PLUGINS_EXTRA_DIR=${CAIRO_DOCK_DIR}/cairo-dock-plug-ins-extras

UPDATE_CORE=1
UPDATE_PLUGINS=1
UPDATE_THIRD_PARTY=1
while getopts "cpt" flag  # Charge-Parity-Time symmetry ^_^
do
	echo " option $flag $OPTIND $OPTARG"
	case "$flag" in
	c)
		echo " => core"
		UPDATE_CORE=$(($UPDATE_CORE+1))
		UPDATE_PLUGINS=$(($UPDATE_PLUGINS-1))
		UPDATE_THIRD_PARTY=$(($UPDATE_THIRD_PARTY-1))
		;;
	p)
		echo " => plug-ins"
		UPDATE_CORE=$(($UPDATE_CORE-1))
		UPDATE_PLUGINS=$(($UPDATE_PLUGINS+1))
		UPDATE_THIRD_PARTY=$(($UPDATE_THIRD_PARTY-1))
		;;
	t)
		echo " => third-party"
		UPDATE_CORE=$(($UPDATE_CORE-1))
		UPDATE_PLUGINS=$(($UPDATE_PLUGINS-1))
		UPDATE_THIRD_PARTY=$(($UPDATE_THIRD_PARTY+1))
		;;
	*)
		echo "unexpected argument"
		exit 1
		;;
	esac
done

export CAIRO_DOCK_EXTRACT_MESSAGE=${CORE_DIR}/po/misc/cairo-dock-extract-message
export CAIRO_DOCK_GEN_TRANSLATION=${CORE_DIR}/po/misc/generate-translation.sh

gcc $CAIRO_DOCK_EXTRACT_MESSAGE.c -o $CAIRO_DOCK_EXTRACT_MESSAGE `pkg-config --libs --cflags glib-2.0 gldi`

###
### update cairo-dock.po
###
if test $UPDATE_CORE -gt 0; then
	echo "extracting the messages of the core ..."
	cd ${CORE_DIR}
	if test -x $CAIRO_DOCK_EXTRACT_MESSAGE; then
		rm -f data/messages
		for c in data/*.conf.in data/*.desktop
		do # les .conf peuvent être dans un dossier build et il ne reste plus que des .conf.in.
			$CAIRO_DOCK_EXTRACT_MESSAGE $c
		done;
		$CAIRO_DOCK_EXTRACT_MESSAGE data/ChangeLog.txt
	fi

	echo "generating translation files for the core ..."
	cd po
	$CAIRO_DOCK_GEN_TRANSLATION core
fi

###
### update cairo-dock-plugins.po
###
if test $UPDATE_PLUGINS -gt 0; then
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
fi

###
### update cairo-dock-plugins-extra.po
###
if test $UPDATE_THIRD_PARTY -gt 0; then
	cd ${PLUGINS_EXTRA_DIR}
	echo "extracing the messages of third-party applets ..."
	for applet in `sed -n "/^\[/p" list.conf | tr -d []`; do
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
fi

echo "done"
exit 0
