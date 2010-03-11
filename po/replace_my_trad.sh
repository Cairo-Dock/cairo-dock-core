#!/bin/sh

# on cherche le fichier contenant le string
COUNT=0
if [ "$1" = "" ]; then exit; fi
if [ "$2" = "" ]; then exit; fi
PH1=`echo $1 | sed ':z;N;s/\n/\\n/;bz' | sed 's/\\%/%/g'`
PH2=`echo $2 | sed ':z;N;s/\n/\\n/;bz' | sed 's/\\%/%/g'`
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
		if test `echo $i | grep -c "c$"` -ge 1 -o `echo $i | grep -c ".h$"` -ge 1; then
			if test `grep "$PH1" $i | grep -c "_("` -ge 1; then
				sed -i "s/_(\"$PH1/_(\"$PH2/g" "$i"
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
ph1=`echo $PH1 | sed ':z;N;s/\n/\n#/;bz'`
ph2=`echo $PH2 | sed ':z;N;s/\n/\n#/;bz'`
if test $PG_check -eq 0; then 
	DATA_MODIF=`grep -s -r "$PH1" ../data/ -l | grep "conf" | grep -v "sh$" | grep -v "~" | grep -v "svg$" | grep -v "png$" | grep -v "jpg"`
else
	DATA_MODIF=`grep -s -r "$PH1" ../*/data/ -l | grep "conf" | grep -v "sh$" | grep -v "~" | grep -v "svg$" | grep -v "png$" | grep -v "jpg$"`
fi
if [ "$DATA_MODIF" != "" ]; then
	COUNT=1
	for i in $DATA_MODIF; do
		# sed -i "s/$ph1/$ph2/g" "$i"
		sed -i "s/] $ph1/] $ph2/g" "$i"
		sed -i "s/\[$ph1/\[$ph2/g" "$i"
		sed -i "s/{$ph1/{$ph2/g" "$i"
		sed -i "s/+ $ph1/+ $ph2/g" "$i"
		sed -i "s/- $ph1/- $ph2/g" "$i"
		sed -i "s/> $ph1/> $ph2/g" "$i"
		sed -i "s/;$ph1;/;$ph2;/g" "$i"
		sed -i "s/;$ph1]/;$ph2]/g" "$i"
		if test $? -ge 1;then
			echo "Phrase <$ph1>, donne une erreur ($i)" >> transfert_translations_log_errors_2.txt
		fi
	done
fi

# ChangeLog.txt : retours à la ligne comme les .c/.h
if [ `pwd | grep -c core` -ge 1 ]; then
	if [ `grep -c " = $PH1" ../data/ChangeLog.txt` -ge 1 ];then
		COUNT=1
		sed -i "s/$PH1/$PH2/g" "../data/ChangeLog.txt"
		if test $? -ge 1; then
			echo "Phrase <$PH1>, donne une erreur (CL)" >> transfert_translations_log_errors_2.txt
		fi
	fi
fi

if test $COUNT -eq 0; then
	echo "Erreur pour <$PH1>" >> transfert_translations_log_error.txt
fi

# obligatoirement dans les po/pot
Ph1=`echo $1 | sed ':z;N;s/\n/\\n\"\n\"/;bz' | sed 's/\\%/%/g'`
Ph2=`echo $2 | sed ':z;N;s/\n/\\n\"\n\"/;bz' | sed 's/\\%/%/g'`
for i in `ls *.po`; do
	sed -i "s/\"$Ph1\"/\"$Ph2\"/g" "$i"
	if test $? -ge 1; then
		echo "Phrase <$Ph1>, donne une erreur ($i)" >> transfert_translations_log_errors_2.txt
	fi
done
sed -i "s/\"$Ph1\"/\"$Ph2\"/g" "cairo-dock.pot"
if [ $? -ge 1 ]; then
	echo "Phrase <$Ph1>, donne une erreur (pot)" >> transfert_translations_log_errors_2.txt
fi
