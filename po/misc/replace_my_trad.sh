#!/bin/sh

# on cherche le fichier contenant le string
COUNT=0
if [ "$1" = "" ]; then exit; fi
if [ "$2" = "" ]; then exit; fi
if [ "$2" = "$1" ]; then exit; fi
PH1=`echo $1 | sed 's/\\\n/\\\\\\\n/g' | sed 's/\\%/%/g'`
PH2=`echo $2 | sed 's/\\\n/\\\\\\\n/g' | sed 's/\\%/%/g'`
PG_check=`pwd | grep -c "plug-ins"` # 0 = core
if test $PG_check -eq 0; then 
	FILES_MODIF=`grep -s -r "$PH1" ../src/ -l | grep -v Makefile | grep -v config | grep -v ".bzr" | grep -v "deps" | grep -v "libs" | grep -v depcomp | grep -v "en_temp" | grep -v "sub$" | grep -v "log$" | grep -v "status" | grep -v copyright | grep -v "pc$" | grep -v ChangeLog | grep -v docum | grep -v INSTALL | grep -v "install" | grep -v intltool | grep -v missing | grep -v stamp | grep -v autom4te | grep -v "~$"`
else
	FILES_MODIF=`grep -s -r "$PH1" ../*/src/ -l | grep -v Makefile | grep -v config | grep -v ".bzr" | grep -v "deps" | grep -v "libs" | grep -v depcomp | grep -v "en_temp" | grep -v "sub$" | grep -v "log$" | grep -v "status" | grep -v copyright | grep -v "pc$" | grep -v ChangeLog | grep -v docum | grep -v INSTALL | grep -v "install" | grep -v intltool | grep -v missing | grep -v stamp | grep -v autom4te | grep -v "~$"`
fi
# double checkcheck
if [ "$FILES_MODIF" != "" ]; then
	COUNT=1
	for i in $FILES_MODIF; do
		# les .c et .h
		if test `echo $i | grep -c "\.c$"` -ge 1 -o `echo $i | grep -c "\.h$"` -ge 1; then
			if test `grep "$PH1" $i | grep -c "_("` -ge 1; then
				sed -i "s/_(\"$PH1\"/_(\"$PH2\"/g" "$i"
				if test $? -ge 1; then
					echo "Phrase <$PH1>, donne une erreur ($i)" >> transfert_translations_log_errors_2.txt
				fi
			fi
		## les autres
		#else
		#	sed -i "s/$PH1/$PH2/g" "$i"
		fi
	done
fi

# CONF : retours à la ligne avec des '\n# '
#ph1=`echo $1 | sed 's/\\\n/\n#/g' | sed 's/\\%/%/g'`
#ph2=`echo $2 | sed 's/\\\n/\n#/g' | sed 's/\\%/%/g'`
ph1=`echo $1 | sed 's/\\\n/\\\\\\\n#/g' | sed 's/\\%/%/g'`
ph2=`echo $2 | sed 's/\\\n/\\\\\\\n#/g' | sed 's/\\%/%/g'`
if test $PG_check -eq 0; then 
	DATA_MODIF=`find ../data/ -name "*conf*" -type f -print | grep -v "\.sh$"`
else
	DATA_MODIF=`ls ../*/data/*.conf.in`
fi
if [ "$DATA_MODIF" != "" ]; then
	COUNT=1
	for i in $DATA_MODIF; do
		sed -i ':z;N;s/\n#/\\n#/;bz' "$i" # => tout sur une ligne
		# sed -i "s/$ph1/$ph2/g" "$i"
		sed -i "s/] $ph1/] $ph2/g" "$i"
		sed -i "s/\[$ph1/\[$ph2/g" "$i"
		sed -i "s/{$ph1/{$ph2/g" "$i"
		sed -i "s/+ $ph1/+ $ph2/g" "$i"
		sed -i "s/- $ph1/- $ph2/g" "$i"
		sed -i "s/> $ph1/> $ph2/g" "$i"
		sed -i "s/;$ph1;/;$ph2;/g" "$i"
		sed -i "s/;$ph1]/;$ph2]/g" "$i"
		sed -i "s/#b $ph1/#b $ph2/g" "$i"
		sed -i "s/#B $ph1/#B $ph2/g" "$i"
		sed -i "s/#c $ph1/#c $ph2/g" "$i"
		sed -i "s/#C $ph1/#C $ph2/g" "$i"
		sed -i "s/#k $ph1/#k $ph2/g" "$i"
		sed -i "s/#s $ph1/#s $ph2/g" "$i"
		sed -i "s/#s99 $ph1/#s99 $ph2/g" "$i"
		sed -i "s/#S $ph1/#S $ph2/g" "$i"
		sed -i "s/#d $ph1/#d $ph2/g" "$i"
		sed -i "s/#D $ph1/#D $ph2/g" "$i"
		sed -i "s/#D99 $ph1/#D99 $ph2/g" "$i"
		sed -i "s/#K $ph1/#K $ph2/g" "$i"
		sed -i "s/#_ $ph1/#_ $ph2/g" "$i"
		sed -i "s/#u $ph1/#u $ph2/g" "$i"
		sed -i "s/#U $ph1/#U $ph2/g" "$i"
		sed -i "s/#a $ph1/#a $ph2/g" "$i"
		sed -i "s/#p $ph1/#p $ph2/g" "$i"
		if test $? -ge 1;then
			echo "Phrase Le fichier /opt/cairo-dock_bzr/cairo-dock-core/po/testttt a été modifié sur le disque.<$ph1>, donne une erreur ($i)" >> transfert_translations_log_errors_2.txt
		fi
		sed -i 's/\\n#/\n#/g' "$i" # => on remet comme avant
	done
fi

# ChangeLog.txt : retours à la ligne comme les .c/.h
if [ `pwd | grep -c core` -ge 1 ]; then
	if [ `grep -c "$PH1" ../data/ChangeLog.txt` -ge 1 ];then
		COUNT=1
		sed -i "s/ = $PH1/ = $PH2/g" "../data/ChangeLog.txt"
		if test $? -ge 1; then
			echo "Phrase <$PH1>, donne une erreur (CL)" >> transfert_translations_log_errors_2.txt
		fi
	fi
fi

if test $COUNT -eq 0; then
	echo "Erreur pour <$PH1>" >> transfert_translations_log_error.txt
fi

# obligatoirement dans les po/pot : retours à la ligne comme les .c/.h
#Ph1=`echo $1 | sed ':z;N;s/\n/\\n\"\n\"/;bz' | sed 's/\\%/%/g'`
#Ph2=`echo $2 | sed ':z;N;s/\n/\\n\"\n\"/;bz' | sed 's/\\%/%/g'`
# pas de g => seulement la 1ere occurence
for i in `ls *.po`; do
	sed -i "s/\"$PH1\"/\"$PH2\"/" "$i"
	if test $? -ge 1; then
		echo "Phrase <$PH1>, donne une erreur ($i)" >> transfert_translations_log_errors_2.txt
	fi
done
sed -i "s/\"$PH1\"/\"$PH2\"/" "cairo-dock.pot"
if [ $? -ge 1 ]; then
	echo "Phrase <$PH1>, donne une erreur (pot)" >> transfert_translations_log_errors_2.txt
fi
