#!/bin/sh
# Thanks to Jiro Kawada for his help !

sed -i 's/\"More precisely/\/\* xgettext:no-c-format \*\/ \"More precisely/g' ../data/messages

xgettext -L C -k_ -k_D -kD_ -kN_ --from-code=UTF-8 --copyright-holder="Cairo-Dock project" --msgid-bugs-address="fabounet@users.berlios.de" -p . ../src/*.c ../data/messages -o cairo-dock.pot
#--omit-header

for lang in `cat LINGUAS`
do
	msgmerge ${lang}.po cairo-dock.pot -o ${lang}.po
	sed -i "/POT-Creation-Date/d" ${lang}.po
done;

# For lp translation tool
if test -e 'en.po' -a -e 'en_GB.po'; then
	cp en.po en_GB.po
fi
