#!/bin/sh

export CAIRO_DOCK_DIR=`pwd`/..
export CAIRO_DOCK_PREFIX=/usr
export CAIRO_DOCK_AUTORECONF="0"
export CAIRO_DOCK_CLEAN="0"
export CAIRO_DOCK_COMPIL="0"
export CAIRO_DOCK_UNSTABLE="0"
export CAIRO_DOCK_INSTALL="0"
export CAIRO_DOCK_THEMES="0"
export CAIRO_DOCK_DOC="0"
export CAIRO_DOCK_EXCLUDE="template musicplayer stacks gauge-test"
export CAIRO_DOCK_GLITZ_OPTION=""
export CAIRO_DOCK_PLUG_INS_OPTION=""
export SUDO=sudo
export TIME=time

echo "this script will process : "
while getopts "acCituhd:Dg" flag
do
	echo " option $flag $OPTIND $OPTARG"
	case "$flag" in
	a)
		echo " => re-generation of files"
		export CAIRO_DOCK_AUTORECONF="1"
		;;
	c)
		echo " => cleaning"
		export CAIRO_DOCK_CLEAN="1"
		;;
	C)
		echo " => compilation"
		export CAIRO_DOCK_COMPIL="1"
		;;
	i)
		echo " => installation"
		export CAIRO_DOCK_INSTALL="1"
		;;
	u)
		echo " => include unstable applets"
		export CAIRO_DOCK_UNSTABLE="1"
		;;
	d)
		echo " => use folder $OPTARG"
		export CAIRO_DOCK_DIR="$OPTARG"
		;;
	D)
		echo " => build documentation $OPTARG"
		export CAIRO_DOCK_DOC="1"
		;;
	g)
		echo " => enable glitz"
		export CAIRO_DOCK_GLITZ_OPTION="--enable-glitz"
		;;
	m)
		echo " => minimum requirements"
		export CAIRO_DOCK_PLUG_INS_OPTION="--without-mail --without-weblet"
		;;
	h)
		echo "-a : run autoreconf"
		echo "-c : clean all"
		echo "-C : compil"
		echo "-i : install (will ask root password)"
		echo "-u : include still unstable applets"
		echo "-d rep : compile in the folder 'rep'"
		exit 0
		;;
	*)
		echo "unexpected argument"
		exit 1
		;;
	esac
done
export CAIRO_DOCK_EXTRACT_MESSAGE=${CAIRO_DOCK_DIR}/utils/cairo-dock-extract-message
export CAIRO_DOCK_GEN_TRANSLATION=${CAIRO_DOCK_DIR}/cairo-dock/po/generate-translation.sh

echo ""

rm -f $CAIRO_DOCK_DIR/compile.log

cd $CAIRO_DOCK_DIR
find . -name linguas -execdir mv linguas LINGUAS \;
find . -name potfiles.in -execdir mv potfiles.in POTFILES.in \;


cd $CAIRO_DOCK_DIR/cairo-dock
echo "*********************************"
echo "* Compilation of cairo-dock ... *"
echo "*********************************"
if test "$CAIRO_DOCK_CLEAN" = "1"; then
	rm -f config.* configure configure.lineno intltool-extract intltool-merge intltool-update libtool ltmain.sh Makefile.in Makefile aclocal.m4 install-sh install depcomp missing compile cairo-dock.pc stamp-h1 cairo-dock.conf 
	rm -rf autom4te.cache src/.deps src/.libs src/Makefile src/Makefile.in po/Makefile po/Makefile.in po/*.gmo src/*.o src/*.lo src/*.la
	if test "$CAIRO_DOCK_DOC" = "1"; then
		rm -rf doc/html
		rm -rf doc/latex
	fi
fi
if test "$CAIRO_DOCK_AUTORECONF" = "1"; then
	if test -e po; then
		echo "generating translation files ..."
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
		cd ..
	fi
	echo  "* configuring ..."
	/usr/bin/time -f "  time elapsed : %Us" autoreconf -isf > /dev/null && ./configure --prefix=$CAIRO_DOCK_PREFIX $CAIRO_DOCK_GLITZ_OPTION > /dev/null
	if test ! "$?" = "0"; then
		echo "  Attention : an error has occured !"
		echo "Error while configuring cairo-dock" >> $CAIRO_DOCK_DIR/compile.log
	else
		echo "  -> passed"
	fi
fi
if test "$CAIRO_DOCK_CLEAN" = "1" -a -e Makefile; then
	make clean > /dev/null
fi
if test "$CAIRO_DOCK_COMPIL" = "1"; then
	echo  "* compiling ..."
	/usr/bin/time -f "  time elapsed : %Us" make > /dev/null
	if test ! "$?" = "0"; then
		echo "  Attention : an error has occured !"
		echo "Error while compiling cairo-dock" >> $CAIRO_DOCK_DIR/compile.log
	else
		echo "  -> passed"
	fi
fi
if test "$CAIRO_DOCK_DOC" = "1"; then
	echo  "* generating documentation ..."
	cd doc
	/usr/bin/time -f "  time elapsed : %Us" doxygen dox.config > /dev/null
	if test ! "$?" = "0"; then
		echo "  Attention : an error has occured !"
		echo "Error while generating documentation" >> $CAIRO_DOCK_DIR/compile.log
	else
		echo "  -> passed"
	fi
	cd ..
fi
if test "$CAIRO_DOCK_INSTALL" = "1"; then
	echo "*  installation of cairo-dock..."
	$SUDO rm -f $CAIRO_DOCK_PREFIX/bin/cairo-dock
	$SUDO rm -rf $CAIRO_DOCK_PREFIX/share/cairo-dock
	$SUDO rm -rf $CAIRO_DOCK_PREFIX/lib/cairo-dock
	/usr/bin/time -f "  time elapsed : %Us" $SUDO make install > /dev/null
	if test ! "$?" = "0"; then
		echo "  Attention : an error has occured !"
		echo "Error while installing cairo-dock" >> $CAIRO_DOCK_DIR/compile.log
	else
		echo "  -> passed"
	fi
	$SUDO chmod +x $CAIRO_DOCK_PREFIX/bin/cairo-dock-update.sh
	$SUDO chmod +x $CAIRO_DOCK_PREFIX/bin/launch-cairo-dock-after-beryl.sh
	$SUDO chmod +x $CAIRO_DOCK_PREFIX/bin/cairo-dock-package-theme.sh
fi


### On liste les plug-ins a compiler.
cd $CAIRO_DOCK_DIR/plug-ins
export liste_stable="`sed "/^#/d" Applets.stable`"
export liste_all="`find . -maxdepth 1  -type d | cut -d "/" -f 2 | /bin/grep -v '\.'`"
echo "the following applets will be compiled :"
if test "$CAIRO_DOCK_UNSTABLE" = "1"; then
	echo "$liste_all"
else
	echo "$liste_stable"
fi
echo "*************************************"
echo "* Compilation of stable modules ... *"
echo "*************************************"

### on extrait les messages des plug-ins a traduire.
if test "$CAIRO_DOCK_AUTORECONF" = "1"; then
	echo "extracting sentences to translate..."
	for plugin in $liste_all
	do
		if test -d $plugin; then
			cd $plugin
			if test -e Makefile.am -a "$plugin" != "$CAIRO_DOCK_EXCLUDE"; then
				if test -e po; then
					echo "generating translation files for $plugin ..."
					if test -x $CAIRO_DOCK_EXTRACT_MESSAGE; then
						rm -f data/messages
						for c in data/*.conf
						do
							$CAIRO_DOCK_EXTRACT_MESSAGE $c
						done;
					fi
					cd po
					if test -e *.pot; then
						$CAIRO_DOCK_GEN_TRANSLATION
					fi
					cd ..
				fi
			fi
			cd ..
		fi
	done;
fi

### On compile les plug-ins stables en une passe.
export compil_ok="1"
if test "$CAIRO_DOCK_AUTORECONF" = "1"; then
	echo  "* configuring stable plug-ins ..."
	/usr/bin/time -f "  time elapsed : %Us" autoreconf -isf > /dev/null && ./configure $CAIRO_DOCK_PLUG_INS_OPTION --prefix=$CAIRO_DOCK_PREFIX > /dev/null
	if test ! "$?" = "0"; then
		echo "  Attention : an error has occured !"
		echo "Error while configuring stable plug-ins" >> $CAIRO_DOCK_DIR/compile.log
		export compil_ok="0"
	else
		echo "  -> passed"
	fi
fi
if test "$CAIRO_DOCK_CLEAN" = "1" -a -e Makefile; then
	make clean > /dev/null
fi
if test "$CAIRO_DOCK_COMPIL" = "1"; then
	echo  "* compiling stable plug-ins ..."
	/usr/bin/time -f "  time elapsed : %Us" make > /dev/null
	if test ! "$?" = "0"; then
		echo "  Attention : an error has occured !"
		echo "Error while compiling stable plug-ins" >> $CAIRO_DOCK_DIR/compile.log
		export compil_ok="0"
	else
		echo "  -> passed"
	fi
fi
if test "$CAIRO_DOCK_INSTALL" = "1"; then
	echo "*  installation of stable plug-ins ..."
	/usr/bin/time -f "  time elapsed : %Us" $SUDO make install > /dev/null
	if test ! "$?" = "0"; then
		echo "  Attention : an error has occured !"
		echo "Error while installing stable plug-ins" >> $CAIRO_DOCK_DIR/compile.log
		export compil_ok="0"
	else
		echo "  -> passed"
	fi
fi

### On les compilera un a un si la compil globale a foirre.
if test "$compil_ok" = "0"; then
	if test "$CAIRO_DOCK_UNSTABLE" = "0"; then
		export liste_all="$liste_stable"
	fi
	export liste_stable=""
fi

### On compile un a un les plug-ins instables.
if test "$CAIRO_DOCK_UNSTABLE" = "1" -o "$compil_ok" = "0"; then
	for plugin in $liste_all
	do
		if test "`echo $liste_stable | grep $plugin`" = ""; then
			cd $plugin
			export exluded="0"
			for e in $CAIRO_DOCK_EXCLUDE
			do
				if test "$e" = "$plugin"; then
					export exluded="1"
				fi
			done;
			if test -e Makefile.am -a "$exluded" != "1"; then
				
				echo "************************************"
				echo "* Compilation of module $plugin ... *"
				echo "************************************"
				if test "$CAIRO_DOCK_CLEAN" = "1"; then
					rm -f config.* configure configure.lineno intltool-extract intltool-merge intltool-update libtool ltmain.sh Makefile.in Makefile aclocal.m4 missing stamp-h1 depcomp compile install-sh
					rm -rf autom4te.cache src/.deps src/.libs src/Makefile src/Makefile.in po/Makefile po/Makefile.in po/*.gmo src/*.o src/*.lo src/*.la
				fi
				if test "$CAIRO_DOCK_AUTORECONF" = "1"; then
					if test -e po; then
						if test -x $CAIRO_DOCK_EXTRACT_MESSAGE; then
							rm -f data/messages
							for c in data/*.conf
							do
								$CAIRO_DOCK_EXTRACT_MESSAGE $c
							done;
						fi
						cd po
						if test -e *.pot; then
							$CAIRO_DOCK_GEN_TRANSLATION
						fi
						cd ..
					fi
					echo  "* configuring ..."
					/usr/bin/time -f "  time elapsed : %Us" autoreconf -isf > /dev/null && ./configure --prefix=$CAIRO_DOCK_PREFIX $CAIRO_DOCK_GLITZ_OPTION > /dev/null
					if test ! "$?" = "0"; then
						echo "  Attention : an error has occured !"
						echo "Error while configuring $plugin" >> $CAIRO_DOCK_DIR/compile.log
					else
						echo "  -> passed"
					fi
				fi
				if test "$CAIRO_DOCK_CLEAN" = "1" -a -e Makefile; then
					make clean > /dev/null
				fi
				if test "$CAIRO_DOCK_COMPIL" = "1"; then
					echo  "* compiling ..."
					/usr/bin/time -f "  time elapsed : %Us" make > /dev/null
					if test ! "$?" = "0"; then
						echo "  Attention : an error has occured !"
						echo "Error while compiling $plugin" >> $CAIRO_DOCK_DIR/compile.log
					else
						echo "  -> passed"
					fi
				fi
				if test "$CAIRO_DOCK_INSTALL" = "1"; then
					echo "*  installation of module $plugin..."
					/usr/bin/time -f "  time elapsed : %Us" $SUDO make install > /dev/null
					if test ! "$?" = "0"; then
						echo "  Attention : an error has occured !"
						echo "Error while installing $plugin" >> $CAIRO_DOCK_DIR/compile.log
					else
						echo "  -> passed"
					fi
				fi
			fi
			cd ..
		fi
	done;
fi

if test "$CAIRO_DOCK_INSTALL" = "1"; then
	$SUDO rm -f $CAIRO_DOCK_PREFIX/lib/cairo-dock/*.la
	
	echo "check :"
	echo "------------"
	date +"compil ended at %c"
	ls -l $CAIRO_DOCK_PREFIX/bin/cairo-dock
	ls -l $CAIRO_DOCK_PREFIX/lib/cairo-dock
#	nb_plugins = "`ls $CAIRO_DOCK_PREFIX/lib/cairo-dock/*.so | wc -w`"
fi

cd $CAIRO_DOCK_DIR
echo "number of lines/word/caracters of sources (.c only) :"
sed '/^ *$/d' cairo-dock/src/*.c plug-ins/*/src/*.c | sed '/^\t*$/d'  | sed '/^\t*\/\/*/d' | sed '/\t*\/\*/d' | wc

if test -e compile.log; then
	echo "\033[8;34mSome errors were encountered :\033[0m"
	cat compile.log
else
	echo "Compile is a success !"
fi

exit
