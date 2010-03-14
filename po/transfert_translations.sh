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
sed -i 's/\*/\\\*/g' en_temp # les *
# sed -i 's/\:/\\\:/g' en_temp # les :
sed -i "s/'/\\'/g" en_temp # les '
sed -i '/^[<space><tab>]*$/d' en_temp # lignes vides

# les .po/.pot
po_pot=`ls *.po`
po_pot=`echo "cairo-dock.pot $po_pot"`
for i in $po_pot; do
	echo $i
	sed -i ':z;N;s/ ""\n"/ "/;bz' $i # on vire tous les \n => msgid ""\n"longue ph..."
	sed -i ':z;N;s/ "\n"/ /;bz' $i # on vire toutes les phrases coupées
	sed -i ':z;N;s/\\n"\n"/\\n/;bz' $i # on veut une seule ligne => \\n"\n" -> \\n
	sed -i ':z;N;s/ ""/ /;bz' $i # on vire les "" isolés
	sed -i 's/""//g' $i # on vire les ""
	sed -i ':z;N;s/msgid \n/msgid ""\n/;bz' $i # les msgid vides
	sed -i ':z;N;s/msgstr \n/msgstr ""\n/;bz' $i # les msgstr vides
done

# scripte pour remplacer les phrases mauvaises ($1) par les bonnes ($2) sans modifier d'autres fichiers
## => replace_my_trad.sh


# le fichier est lit ligne par ligne et les arguments sont envoyés dans le script ci-dessus.
#total_ph=`wc -l en_temp`
while [ "`head -n 1 'en_temp'`" != "" ];do
	head -n 1 en_temp | xargs sh replace_my_trad.sh
	tail -n+2 en_temp > en_temp0
	mv en_temp0 en_temp
	wc -l en_temp
done

rm en_temp en_temp1
#find . -name "*.*" -type f -print | grep -v bzr | xargs sed -i 's/cairo-dock.org/glx-dock.org/g'

for i in `ls *.po`; do
	msguniq $i > "temp"
	mv "temp" $i
done
# 2 fois pour les erreurs
for i in `ls *.po`; do msguniq $i > "temp"; mv "temp" $i; done

##### Prob #####
#sed -e 'N;s/{$ph1/AAAAAAAAAAAA/P;D; # => n'accepte que 2 lignes :/
