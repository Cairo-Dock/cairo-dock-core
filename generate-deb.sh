#!/bin/sh

export CAIRO_DOCK_DIR=`pwd`/..
export FAST_COMPIL="0"
export BUILD_TAR="0"
export MINIMUM_REQUIREMENTS=""
export DEB_DIR="deb"
export DEB_PLUGINS_DIR="deb-plug-ins"
export archi=`uname --machine`

echo "packaging options : "
while getopts "d:fThm" flag
do
	echo " option $flag" $OPTIND $OPTARG
	case "$flag" in
	d)
		echo " => use folder $OPTARG"
		export CAIRO_DOCK_DIR="$OPTARG"
		;;
	f)
		echo " => fast building"
		export FAST_COMPIL="1"
		;;
	T)
		echo " => build tar"
		export BUILD_TAR="1"
		;;
	h)
		echo "-d rep : build in the folder 'rep'"
		echo "-f : fast building (don't re-compil (use it with caution))"
		echo "-T : build the sources tarball"
		exit 0
		;;
	m)
		echo " => minimum requirements"
		export MINIMUM_REQUIREMENTS="-m"
		export DEB_DIR="deb-lpia"
		export DEB_PLUGINS_DIR="deb-plug-ins-lpia"
		export archi=${archi}_light
		;;
	*)
		echo "unexpected argument"
		exit 1
		;;
	esac
done


#\_____________ On nettoie un peu les sources.
read -p "Use folder '$CAIRO_DOCK_DIR' ? [Y/n]" clean
if test "$clean" = "n" -o  "$clean" = "N"; then
	exit 0
fi
cd $CAIRO_DOCK_DIR
find . -name "*~" -delete
find . -name "core*" -delete
find . -name ".#*" -delete
rm -rf plug-ins/conf*[0-9]
rm -f cairo-dock*.tar.bz2 *.deb

if ! test  -d cairo-dock -o ! -d plug-ins -o ! -d $DEB_DIR -o ! -d $DEB_PLUGINS_DIR; then
	echo "Attention : folder missing in $CAIRO_DOCK_DIR !"
	exit 1
fi

cd $CAIRO_DOCK_DIR
sudo rm -rf $DEB_DIR/usr
sudo rm -rf $DEB_PLUGINS_DIR/usr
mv $DEB_DIR/.svn .svn-$DEB_DIR
mv $DEB_PLUGINS_DIR/.svn .svn-$DEB_PLUGINS_DIR

cd $CAIRO_DOCK_DIR/$DEB_DIR
if test -e debian; then
	mv debian DEBIAN
fi
cd $CAIRO_DOCK_DIR/$DEB_PLUGINS_DIR
if test -e debian; then
	mv debian DEBIAN
fi

#\_____________ On compile de zero.
if test "$FAST_COMPIL" = "0"; then
	cd $CAIRO_DOCK_DIR/cairo-dock
	./compile-all.sh -a -c -C -i -d $CAIRO_DOCK_DIR $MINIMUM_REQUIREMENTS
fi

#\_____________ On cree les archives.
if test "$BUILD_TAR" = "1"; then
	echo "***************************"
	echo "* Generating tarballs ... *"
	echo "***************************"
	echo ""
	echo "building dock tarball ..."
	cd $CAIRO_DOCK_DIR/cairo-dock
	make dist-bzip2 > /dev/null
	mv cairo-dock*.tar.bz2 ..
	
	echo "building plug-ins tarball ..."
	cd $CAIRO_DOCK_DIR/plug-ins
	make dist-bzip2 > /dev/null
	mv cairo-dock*.tar.bz2 ..
fi

#\_____________ On cree les paquets.
echo "***************************"
echo "* Generating packages ... *"
echo "***************************"
echo ""
cd $CAIRO_DOCK_DIR
sudo chmod -R 755 $DEB_DIR $DEB_PLUGINS_DIR

echo "building dock package ..."
cd $CAIRO_DOCK_DIR/$DEB_DIR
sudo mkdir usr
sudo mkdir usr/bin
sudo mkdir usr/share
sudo mkdir usr/share/menu
sudo mkdir usr/share/applications
sudo mkdir usr/share/pixmaps
for lang in `cat ../cairo-dock/po/LINGUAS`; do
	sudo mkdir -p usr/share/locale/$lang/LC_MESSAGES
	sudo cp /usr/share/locale/$lang/LC_MESSAGES/cairo-dock.mo usr/share/locale/$lang/LC_MESSAGES
done;

sudo cp /usr/bin/cairo-dock usr/bin
sudo cp /usr/bin/launch-cairo-dock-after-beryl.sh usr/bin
sudo cp /usr/bin/cairo-dock-update.sh usr/bin
sudo cp /usr/bin/cairo-dock-package-theme.sh usr/bin
sudo cp -rpP /usr/share/cairo-dock/ usr/share/
sudo rm -rf usr/share/cairo-dock/plug-ins
sudo cp ../cairo-dock/data/cairo-dock.svg usr/share/pixmaps
sudo cp ../cairo-dock/data/cairo-dock usr/share/menu
sudo chmod 644 usr/share/menu/cairo-dock
sudo cp ../cairo-dock/data/cairo-dock*.desktop usr/share/applications

cd $CAIRO_DOCK_DIR
sed -i "s/^Version:.*/Version: "`cairo-dock --version`"/g" $DEB_DIR/DEBIAN/control
dpkg -b $DEB_DIR "cairo-dock_v`cairo-dock --version`_${archi}.deb"


echo "building plug-ins package ..."
cd $CAIRO_DOCK_DIR/$DEB_PLUGINS_DIR
for lang in `cat ../cairo-dock/po/LINGUAS`; do
	sudo mkdir -p usr/share/locale/$lang/LC_MESSAGES
	sudo cp /usr/share/locale/$lang/LC_MESSAGES/cd-*.mo usr/share/locale/$lang/LC_MESSAGES
done;
sudo mkdir -p usr/share/cairo-dock
sudo cp -r /usr/share/cairo-dock/plug-ins usr/share/cairo-dock
sudo mkdir -p usr/lib
sudo cp -r /usr/lib/cairo-dock usr/lib
sudo rm -f usr/lib/cairo-dock/*.la

cd $CAIRO_DOCK_DIR
sed -i "s/^Version:.*/Version: "`cairo-dock --version`"/g" $DEB_PLUGINS_DIR/DEBIAN/control
dpkg -b $DEB_PLUGINS_DIR "cairo-dock-plug-ins_v`cairo-dock --version`_${archi}.deb"

mv .svn-$DEB_DIR $DEB_DIR/.svn
mv .svn-$DEB_PLUGINS_DIR $DEB_PLUGINS_DIR/.svn


#\_____________ On liste les sommes de controle des fichiers.
cd $CAIRO_DOCK_DIR
rm -f md5sum.txt
for f in *.deb *.bz2; do echo `md5sum $f`>> md5sum.txt; done;
echo "generated files :"
echo "----------------"
cat md5sum.txt
