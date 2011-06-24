#!/bin/sh
# Thanks to Jiro Kawada for his help !

if test "$1" = "plug-ins"; then
	sources="../*/src/*.[ch] ../*/data/messages"  # plug-ins
	lang=C
elif test "$1" = "extras"; then
	sources=""
	for f in `sed -n "/^\[/p" ../list.conf | tr -d []`; do  # plug-ins-extra
		test -e ../$f/$f -a ! -x ../$f/$f && sources="${sources} ../$f/$f"  # not an executable
		test -e ../$f/messages && sources="${sources} ../$f/messages"
		if test -e $f/translated_files.txt; then
			for g in `cat $f/translated_files.txt`; do  # a file where we can list the specific source files to be translated.
				sources="${sources} ../$f/$g"
			done;
		fi
	done;
	lang=Python
else
	sources="../src/*.[ch] ../src/*/*.[ch] ../data/messages"  # core
	lang=C
fi

xgettext -L $lang -k_ -k_D -kD_ -kN_ --no-wrap --from-code=UTF-8 --copyright-holder="Cairo-Dock project" --msgid-bugs-address="fabounet@glx-dock.org" -p . $sources -o cairo-dock.pot
#--omit-header

for lang in `ls *.po`
do
	echo -n "${lang} :"
	msgmerge ${lang} cairo-dock.pot -o ${lang}
	sed -i "/POT-Creation-Date/d" ${lang}
done;

sed -i "/POT-Creation-Date/d" cairo-dock.pot

# For lp translation tool
if test -e 'en.po' -a -e 'en_GB.po'; then
	ln -sf en_GB.po en.po
fi
