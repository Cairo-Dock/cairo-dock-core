#!/bin/sh

tail -n +18 en_GB.po | grep -v \# > en_temp1 # on vire les lignes et commentaires inutiles
sed -i '/^[<space><tab>]*$/d' en_temp1 # lignes vides
tr -d "\n" <en_temp1 >en_temp # on vire tous les \n
sed -i 's/""//g' en_temp # on vire les ""
sed -i 's/msgid/\n/g' en_temp # on vire ce qui est hors des "(...)"
sed -i 's/msgstr//g' en_temp
sed -i 's/  / /g' en_temp # les doubles espaces
sed -i 's/^ //g' en_temp # les espaces au début
sed -i 's/\%/\\\%/g' en_temp # les %
sed -i 's/&/\\&/g' en_temp # les &
sed -i 's/\//\\\//g' en_temp # les /
sed -i 's/\\/\\\\/g' en_temp # les \
# sed -i 's/\:/\\\:/g' en_temp # les :
sed -i "s/'/\\'/g" en_temp # les '
sed -i "s/\./\\./g" en_temp # les .
sed -i '/^[<space><tab>]*$/d' en_temp # lignes vides

# les .po
for i in `ls *.po`; do
	sed -i ':z;N;s/ ""\n"/ "/;bz' $i # on vire tous les \n
	sed -i ':z;N;s/ "\n"/ /;bz' $i # on vire tous les \n
	sed -i ':z;N;s/ ""/ /;bz' $i # on vire les "" isolés
	sed -i 's/""//g' $i # on vire les ""
	sed -i ':z;N;s/msgid \n/msgid ""\n/;bz' $i # les msgid vides
	sed -i ':z;N;s/msgstr \n/msgstr ""\n/;bz' $i # les msgstr vides
done

# le .pot
sed -i ':z;N;s/ ""\n"/ "/;bz' "cairo-dock.pot" # on vire tous les \n
sed -i ':z;N;s/ "\n"/ /;bz' "cairo-dock.pot" # on vire tous les \n
sed -i ':z;N;s/ ""/ /;bz' "cairo-dock.pot" # on vire les "" isolés
sed -i 's/""//g' "cairo-dock.pot" # on vire les ""
sed -i ':z;N;s/msgid \n/msgid ""\n/;bz' "cairo-dock.pot" # les msgid vides
sed -i ':z;N;s/msgstr \n/msgstr ""\n/;bz' "cairo-dock.pot" # les msgstr vides

# scripte pour remplacer les phrases mauvaises ($1) par les bonnes ($2) sans modifier d'autres fichiers
## => replace_my_trad.sh


# le fichier est lit ligne par ligne et les arguments sont envoyés dans le script ci-dessus.
while [ "`head -n 1 'en_temp'`" != "" ];do
	head -n 1 en_temp | xargs sh replace_my_trad.sh
	tail -n+2 en_temp > en_temp0
	mv en_temp0 en_temp
done

rm en_temp en_temp1
#find . -name "*.*" -type f -print | grep -v bzr | xargs sed -i 's/cairo-dock.org/glx-dock.org/g'

##### Prob #####
# .5 -> .
# 1000 -> 00

# -	pLink = gtk_link_button_new_with_label (CAIRO_DOCK_PLUGINS_EXTRAS_URL, _("Cairo-Dock-Plug-ins-Extras"));
# +	pLink = gtk_link_button_new_with_label (CAIRO_DOCK_PLUGINS_EXTRAS_URL, _(""));

#-		gtk_widget_set_tooltip_text (pMenuItem, _("set behaviour in Compiz to: (name=cairo-dock & type=utility)"));
#+		gtk_widget_set_tooltip_text (pMenuItem, _("Set behaviour in Compiz to: (name=cairo-dock _("set behaviour in Compiz to: (name=cairo-dock & type=utility) type=utility)"));
