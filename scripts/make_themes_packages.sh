#!/bin/sh
# A script to package a theme or all the themes. 
# The result (to upload on the server) is in the "FTP" folder.
# made by Fabounet
# last modif: 27/03/2011

# grab the list of themes to package from the list.conf or from the command line
if test "$1" = ""; then  # no theme provided, let's package all themes listed in the list.conf
	list=`sed -n "/^\[.*\]/p" list.conf | tr -d "[]"`
	if test -d FTP; then  # if the FTP folder exists, make the user delete/rename it.
		echo "You have to remove FTP directory"
		exit 1
	fi
	mkdir FTP
else  # a theme is provided on the command line
	list=`echo "$1" | tr -d "/"`
	if test ! -d FTP; then
		mkdir FTP
	fi
	if test -d "FTP/$1"; then
		rm -rf "FTP/$1"
	fi
fi

cp list.conf FTP
for f in $list; do
	echo "make $f"
	
	# remove unwanted files.
	rm -f "$f/*~" "$f/cairo-dock-simple.conf"
	
	# build the tarball.
	tar cfz "$f.tar.gz" "$f" --exclude="last-modif" --exclude="preview.png" --exclude="preview.jpg"
	
	# place it in its folder.
	mkdir "FTP/$f"
	mv "$f.tar.gz" "FTP/$f"
	cp "$f/preview" "FTP/$f"
	cp "$f/readme" "FTP/$f"
	
	# update the modif date for this theme in the list.conf
	sed -i "/^\[$f\]/,/^\[/ {/^last modif/ s/=.*/=`date +%Y%m%d`/}" list.conf
done;

# don't forget to upload the list.conf
echo "Think to upload the list.conf ;-)"

exit 0
