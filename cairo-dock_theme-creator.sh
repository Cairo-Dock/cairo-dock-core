#!/bin/bash

# Theme creator for Cairo-Dock
#
# Copyright : (C) 2008 by Fabounet
# E-mail    : fabounet@users.berlios.de
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# http://www.gnu.org/licenses/licenses.html#GPL

# Auteur : Nochka85
# Contact : nochka85@glx-dock.org
# Version : 05/07/08

# Changelog
# 05/07/08 :	Ajout catégorie drive_internet ET specific_vlc_icon
# 05/07/08 :	Refonte totale du scipt + Ajout de fonctionnalités (choix des catégories dans une liste coché ou décoché)... ;-)
# 02/07/08 :	Ajout de l'option --test - 1ere mise en ligne sur le FTP ;-)
# 01/07/08 :	Ajout des catégories specific_firefox_icon, specific_thunderbird_icon, specific_inkscape_icon, specific_gimp_icon, launcher, settings, gohome ET system_search
# 30/06/08 :	Ajout de catégories + Ajout d'icônes spécifiques + Ajout options --relink --[category] --help 
# 29/06/08 :	Ajout detection format + répertoire source + amelioration diverse
# 29/06/08 :	Ebauche du programme
		

DEBUG=0 # Attention, ne pas oublier de remettre a 0 pour une MAJ du script ... quand ce sera dispo ;-)

# ICONS_DIR="/home/$USER/cairo-dock_theme-creator/"  ## Pour moi ;-)
ICONS_DIR="/home/$USER/.cairo-dock/icons_themes/"

# MAIN_CAT="web_browser mail_reader video_messenger video_player"  # Encore pour moi :-)
MAIN_CAT="web_browser file_browser mail_reader image_reader audio_player video_player writer bittorrent downloader cd_burner settings gohome system_search launcher image_editor text_messenger video_messenger terminal packages system_monitor calculator virtual_machine remote_control logo_distribution home_folder default_folder desktop computer webcam irc"

DRIVES_CAT="drive_usb_mounted drive_usb_unmounted network_folder harddrive drive_optical floppy_unmounted drive_internet"

#SPECIFICS_CAT="specific_firefox_icon"	# Encore pour moi :-)
SPECIFICS_CAT="googleearth specific_skype_icon specific_amsn_icon specific_gaim_icon specific_oowriter_icon specific_oodraw_icon specific_oocalc_icon specific_ooimpress_icon specific_oobase_icon specific_oomath_icon specific_update_manager specific_firefox_icon specific_thunderbird_icon specific_inkscape_icon specific_gimp_icon specific_vlc_icon"



## Catégories MAIN_CAT :
LINKS_WEB_BROWSER="firefox firefox-3.0 opera epiphany"
LINKS_FILE_BROWSER="nautilus konqueror thunar pcmanfm"
LINKS_MAIL_READER="thunderbird kmail evolution"
LINKS_IMAGE_READER="eog gqview gwenview f-spot"
LINKS_AUDIO_PLAYER="rhythmbox exaile xmms listen audacious beep-media-player bmp amarok"
LINKS_VIDEO_PLAYER="totem mplayer vlc xine kaffeine smplayer"
LINKS_WRITER="gedit kate oowriter ooo-writer abiword emacs mousepad"
LINKS_BITTORRENT="transmission deluge bittornado gnome-btdownload ktorrent"
LINKS_DOWNLOADER="amule emule filezilla"
LINKS_CD_BURNER="nautilus-burner graveman k3b brasero gnomebaker"
LINKS_SETTINGS="gnome-settings"
LINKS_GOHOME="gohome"
LINKS_SYSTEM_SEARCH="system-search tracker-search-tool"
LINKS_LAUNCHER="gnome-panel-launcher"
LINKS_IMAGE_EDITOR="gimp inkscape krita"
LINKS_TEXT_MESSENGER="gaim pidgin kopete emessene"
LINKS_VIDEO_MESSENGER="skype ekiga amsn"
LINKS_TERMINAL="gnome-terminal konsole xfce4-terminal"
LINKS_PACKAGES="synaptic adept pacman-g2 update-manager"
LINKS_SYSTEM_MONITOR="ksysguard gnome-system-monitor utilities-system-monitor conky"
LINKS_CALCULATOR="gnome-calculator accessories-calculator gcalctool crunch"
LINKS_VIRTUAL_MACHINE="VBox vmware-server"
LINKS_REMOTE_CONTROL="tsclient"
LINKS_LOGO_DISTRIBUTION="distributor-logo gnome-main-menu start-here"
LINKS_HOME_FOLDER="user-home gnome-fs-home folder_home"
LINKS_DEFAULT_FOLDER="folder gnome-fs-directory"
LINKS_DESKTOP="desktop"
LINKS_COMPUTER="computer"
LINKS_WEBCAM="cheese"
LINKS_IRC="xchat konversation kvirc"

## Catégories DRIVES_CAT :
LINKS_DRIVE_USB_MOUNTED="drive-harddisk-usb"
LINKS_DRIVE_USB_UNMOUNTED="drive-removable-media-usb"
LINKS_NETWORK_FOLDER="network folder-remote"
LINKS_HARDDRIVE="gnome-dev-harddisk drive-harddisk"
LINKS_DRIVE_OPTICAL="drive-optical"
LINKS_FLOPPY_UNMOUNTED="drive-removable-media"
LINKS_DRIVE_INTERNET="applications-internet gnome-globe package_network redhat-internet stock_internet xfce-internet"

## Catégories SPECIFICS_CAT :
LINKS_GOOGLEEARTH="googleearth"
LINKS_SPECIFIC_SKYPE_ICON="skype"
LINKS_SPECIFIC_AMSN_ICON="amsn"
LINKS_SPECIFIC_GAIM_ICON="gaim"
LINKS_SPECIFIC_OOWRITER_ICON="oowriter ooo-writer"
LINKS_SPECIFIC_OODRAW_ICON="oodraw ooo-draw"
LINKS_SPECIFIC_OOCALC_ICON="oocalc ooo-calc"
LINKS_SPECIFIC_OOIMPRESS_ICON="ooimpress ooo-impress"
LINKS_SPECIFIC_OOBASE_ICON="oobase ooo-base"
LINKS_SPECIFIC_OOMATH_ICON="oomath ooo-math"
LINKS_SPECIFIC_UPDATE_MANAGER="update-manager"
LINKS_SPECIFIC_FIREFOX_ICON="firefox firefox-3.0"
LINKS_SPECIFIC_THUNDERBIRD_ICON="thunderbird"
LINKS_SPECIFIC_INKSCAPE_ICON="inkscape"
LINKS_SPECIFIC_GIMP_ICON="gimp"
LINKS_SPECIFIC_VLC_ICON="vlc"







SCRIPT="cairo-dock_theme-creator.sh"		#OK
SCRIPT_SAVE="cairo-dock_theme-creator.sh.save"	#OK
SCRIPT_NEW="cairo-dock_theme-creator.sh.new"	#OK
HOST="http://theme_creator.glx-dock.org"	#OK




IFS=" "				#OK
START_MENU=""			#OK
TODO_LIST=""			#OK
TODO_FINAL=""			#OK
THEME_NAME=""			#OK
WORK_DIR=""			#OK
LINKS=""			#OK
CURRENT_ICON=""			#OK
START_DIR="/home/$USER/"	#OK
REDO_DIR=""			#OK
GLOBAL_NAME=""			#OK
FILENAME=""			#OK
FORMAT=""			#OK
DETAIL_LIST=""			#OK
TRUE_FALSE=""			#OK
TEST_FILE="test_links.log"	#OK
LOG_FILE="log.txt"


NORMAL="\\033[0;39m"
BLEU="\\033[1;34m"
VERT="\\033[1;32m" 
ROUGE="\\033[1;31m"


#############################################################################################################
links_definition() {
## Catégories MAIN_CAT :
	if [ $current_category = "web_browser" ]; then LINKS=$LINKS_WEB_BROWSER;
	elif [ $current_category = "file_browser" ]; then LINKS=$LINKS_FILE_BROWSER;
	elif [ $current_category = "mail_reader" ]; then LINKS=$LINKS_MAIL_READER;
	elif [ $current_category = "image_reader" ]; then LINKS=$LINKS_IMAGE_READER;
	elif [ $current_category = "audio_player" ]; then LINKS=$LINKS_AUDIO_PLAYER;
	elif [ $current_category = "video_player" ]; then LINKS=$LINKS_VIDEO_PLAYER;
	elif [ $current_category = "writer" ]; then LINKS=$LINKS_WRITER;
	elif [ $current_category = "bittorrent" ]; then LINKS=$LINKS_BITTORRENT;
	elif [ $current_category = "downloader" ]; then LINKS=$LINKS_DOWNLOADER;
	elif [ $current_category = "cd_burner" ]; then LINKS=$LINKS_CD_BURNER;
	elif [ $current_category = "settings" ]; then LINKS=$LINKS_SETTINGS;
	elif [ $current_category = "gohome" ]; then LINKS=$LINKS_GOHOME;
	elif [ $current_category = "system_search" ]; then LINKS=$LINKS_SYSTEM_SEARCH;
	elif [ $current_category = "launcher" ]; then LINKS=$LINKS_LAUNCHER;
	elif [ $current_category = "image_editor" ]; then LINKS=$LINKS_IMAGE_EDITOR;
	elif [ $current_category = "text_messenger" ]; then LINKS=$LINKS_TEXT_MESSENGER;
	elif [ $current_category = "video_messenger" ]; then LINKS=$LINKS_VIDEO_MESSENGER;
	elif [ $current_category = "terminal" ]; then LINKS=$LINKS_TERMINAL;
	elif [ $current_category = "packages" ]; then LINKS=$LINKS_PACKAGES;
	elif [ $current_category = "system_monitor" ]; then LINKS=$LINKS_SYSTEM_MONITOR;
	elif [ $current_category = "calculator" ]; then LINKS=$LINKS_CALCULATOR;
	elif [ $current_category = "virtual_machine" ]; then LINKS=$LINKS_VIRTUAL_MACHINE;
	elif [ $current_category = "remote_control" ]; then LINKS=$LINKS_REMOTE_CONTROL;
	elif [ $current_category = "logo_distribution" ]; then LINKS=$LINKS_LOGO_DISTRIBUTION;
	elif [ $current_category = "home_folder" ]; then LINKS=$LINKS_HOME_FOLDER;
	elif [ $current_category = "default_folder" ]; then LINKS=$LINKS_DEFAULT_FOLDER;
	elif [ $current_category = "desktop" ]; then LINKS=$LINKS_DESKTOP;
	elif [ $current_category = "computer" ]; then LINKS=$LINKS_COMPUTER;
	elif [ $current_category = "webcam" ]; then LINKS=$LINKS_WEBCAM;
	elif [ $current_category = "irc" ]; then LINKS=$LINKS_IRC;


## Catégories DRIVES_CAT :
	elif [ $current_category = "drive_usb_mounted" ]; then LINKS=$LINKS_DRIVE_USB_MOUNTED;
	elif [ $current_category = "drive_usb_unmounted" ]; then LINKS=$LINKS_DRIVE_USB_UNMOUNTED;
	elif [ $current_category = "network_folder" ]; then LINKS=$LINKS_NETWORK_FOLDER;
	elif [ $current_category = "harddrive" ]; then LINKS=$LINKS_HARDDRIVE;
	elif [ $current_category = "drive_optical" ]; then LINKS=$LINKS_DRIVE_OPTICAL;
	elif [ $current_category = "floppy_unmounted" ]; then LINKS=$LINKS_FLOPPY_UNMOUNTED;
	elif [ $current_category = "drive_internet" ]; then LINKS=$LINKS_DRIVE_INTERNET;

## Catégories SPECIFICS_CAT :
	elif [ $current_category = "googleearth" ]; then LINKS=$LINKS_GOOGLEEARTH;
	elif [ $current_category = "specific_skype_icon" ]; then LINKS=$LINKS_SPECIFIC_SKYPE_ICON;
	elif [ $current_category = "specific_amsn_icon" ]; then LINKS=$LINKS_SPECIFIC_AMSN_ICON;
	elif [ $current_category = "specific_gaim_icon" ]; then LINKS=$LINKS_SPECIFIC_GAIM_ICON;
	elif [ $current_category = "specific_oowriter_icon" ]; then LINKS=$LINKS_SPECIFIC_OOWRITER_ICON;
	elif [ $current_category = "specific_oodraw_icon" ]; then LINKS=$LINKS_SPECIFIC_OODRAW_ICON;
	elif [ $current_category = "specific_oocalc_icon" ]; then LINKS=$LINKS_SPECIFIC_OOCALC_ICON;
	elif [ $current_category = "specific_ooimpress_icon" ]; then LINKS=$LINKS_SPECIFIC_OOIMPRESS_ICON;
	elif [ $current_category = "specific_oobase_icon" ]; then LINKS=$LINKS_SPECIFIC_OOBASE_ICON;
	elif [ $current_category = "specific_oomath_icon" ]; then LINKS=$LINKS_SPECIFIC_OOMATH_ICON;
	elif [ $current_category = "specific_update_manager" ]; then LINKS=$LINKS_SPECIFIC_UPDATE_MANAGER;
	elif [ $current_category = "specific_firefox_icon" ]; then LINKS=$LINKS_SPECIFIC_FIREFOX_ICON;
	elif [ $current_category = "specific_thunderbird_icon" ]; then LINKS=$LINKS_SPECIFIC_THUNDERBIRD_ICON;
	elif [ $current_category = "specific_inkscape_icon" ]; then LINKS=$LINKS_SPECIFIC_INKSCAPE_ICON;
	elif [ $current_category = "specific_gimp_icon" ]; then LINKS=$LINKS_SPECIFIC_GIMP_ICON;
	elif [ $current_category = "specific_vlc_icon" ]; then LINKS=$LINKS_SPECIFIC_VLC_ICON;
	fi
}


#############################################################################################################
check_new_script() {
	cp $SCRIPT $SCRIPT_SAVE #pour moi :)
	echo -e "$NORMAL"""
	echo "Vérification de la disponibilité d'un nouveau script"
	wget $HOST/$SCRIPT -q -O $SCRIPT_NEW	
	diff $SCRIPT $SCRIPT_NEW >/dev/null
	if [ $? -eq 1 ]; then
		echo -e "$ROUGE"		
		echo "Veuillez relancer le script, une mise à jour a été téléchargée"
		echo -e "$NORMAL"
		mv $SCRIPT_NEW $SCRIPT
		chmod u+x $SCRIPT
		zenity --info --title="Cairo-Dock Theme Creator" --text="Une mise à jour a été téléchargée.	Cliquez sur Ok pour fermer le terminal."
		exit
	else
		echo ""
		echo -e "$VERT""Vous possédez la dernière version du script de Nochka85"
	fi
	echo -e "$NORMAL"
	rm $SCRIPT_NEW
}


#############################################################################################################
start_menu() {
# Remise à zéro des variables:
IFS=" "
START_MENU=""			#OK
TODO_LIST=""			#OK
TODO_FINAL=""			#OK
THEME_NAME=""			#OK
WORK_DIR=""			#OK
LINKS=""			#OK
CURRENT_ICON=""			#OK
START_DIR="/home/$USER/"	#OK
REDO_DIR=""			#OK
GLOBAL_NAME=""			#OK
FILENAME=""			#OK
FORMAT=""			#OK
DETAIL_LIST=""			#OK
TRUE_FALSE=""			#OK






START_MENU=$(zenity --width=800 --height=400 --list --separator " " --column="Choix" --column="Action" --column="Observation" --radiolist --text="Choisissez l'action à effectuer (Cliquez sur "Annuler" pour quitter) :" --title="CD Theme Creator" \
true "NOUVEAU THEME D'ICÔNES" "Créez un nouveau thème d'icônes standard pour Cairo-dock" \
false "Re-créer les liens d'un thème d'icônes standard" "A effectuer en cas de liens cassés et/ou manquants dans un thème d'icônes créé avec ce script" \
false "Sélectionner les catégories dans une liste" "Sélectionnez vous même les catégories à traiter dans une liste (TOUT DECOCHE par défaut)" \
false "Désélectionner les catégories dans une liste" "Sélectionnez vous même les catégories à traiter dans une liste (TOUT COCHE par défaut)" \
false "Tester ou contrôler un thème d'icônes existant" "Retrouvez facilement les catégories et les liens qu'il manque à votre thème d'icônes")


if [ "$START_MENU" = "" ]; then 
	exit
else
	IFS="|"
	for temp in $START_MENU
		do

		if [ $temp = "NOUVEAU THEME D'ICÔNES" ]; then
			new_theme
			start_menu
						
		elif [ $temp = "Re-créer les liens d'un thème d'icônes standard" ]; then
			relink
			start_menu

		elif [ $temp = "Sélectionner les catégories dans une liste" ]; then 
			select_false
			start_menu

		elif [ $temp = "Désélectionner les catégories dans une liste" ]; then
			select_true
			start_menu

		elif [ $temp = "Tester ou contrôler un thème d'icônes existant" ]; then
			do_report
			start_menu

		fi
		done
fi
IFS=" "
}

#############################################################################################################
new_theme() {
choose_category		# Definition de $TODO_FINAL
working_directory	# Definition du nom du répertoire $WORK_DIR
new_directory		# Création du nouveau répertoire
cd $WORK_DIR
REDO_DIR="$START_DIR"
copy_categories		# Copie des icônes des différentes catégories			
relink_icons		# Création des liens
zenity --info --title="CD Theme Creator" --text="Votre thème a été créé dans le répertoire:\n$WORK_DIR"
}

#############################################################################################################
relink() {
tempo_text="Spécifier le répertoire dans lequel re-créer les liens"
todo_relink
relink_icons
zenity --info --title="CD Theme Creator" --text="Tous les liens ont été re-créés dans :\n$WORK_DIR\n\nCliquer sur Valider pour revenir au menu principal..."
}

#############################################################################################################
select_false() {
TRUE_FALSE="FALSE"
select_true_false
}

#############################################################################################################
select_true() {
TRUE_FALSE="TRUE"
select_true_false
}

#############################################################################################################
select_true_false() {
choose_category
detail_list		# Definition de $TODO_FINAL et $WORK_DIR
cd $WORK_DIR
REDO_DIR="$START_DIR"
copy_categories		# Copie des icônes des différentes catégories			
relink_icons		# Création des liens
zenity --info --title="CD Theme Creator" --text="Toutes les catégories sélectionnées ont été créés dans le répertoire:\n$WORK_DIR"
}


#############################################################################################################
choose_category() {
IFS=" "
TODO_LIST=$(zenity --width=750 --height=400 --list --column="Choix" --column="Catégories" --column="Observation" --checklist --text="Choisissez les catégories d'icônes à traiter\n\nATTENTION : Les icônes génériques sont OBLIGATOIRES\n" --title="CD Theme Creator" \
true "Icônes génériques" "Ce sont les icônes OBLIGATOIRES pour un thème standard cairo-dock" \
false "Icônes des VOLUMES" "Conseillés pour certaines applets de cairo-dock" \
false "Icônes spécifiques" "Permettent de "surcharger" les icônes génériques pour certaines applications (exemple : Firefox, Thunderbird,etc...)")

if [ "$TODO_LIST" = "" ]; then start_menu

else
	IFS="|"
	for temp in $TODO_LIST
		do

		if [ $temp = "Icônes génériques" ]; then 
						for categorie in $MAIN_CAT
							do 
								TODO_FINAL="$TODO_FINAL$categorie "
							done

		elif [ $temp = "Icônes des VOLUMES" ]; then 
						for categorie in $DRIVES_CAT
							do 
								TODO_FINAL="$TODO_FINAL$categorie "
							done
		
		elif [ $temp = "Icônes spécifiques" ]; then 
						for categorie in $SPECIFICS_CAT
							do 
								TODO_FINAL="$TODO_FINAL$categorie "
							done
		fi
		done
fi
IFS=" "
}


#############################################################################################################
working_directory() {
THEME_NAME=$(zenity --entry --title="CD Theme Creator" --text="Entrez le nom de votre thème d'icônes (il sera placé dans $ICONS_DIR)")
if [ "$THEME_NAME" = "" ]; then
				zenity --error --text "Aucun nom n'a été rentré ou l'opération a été annulée..."
				TODO_FINAL=""
				start_menu
else
WORK_DIR="$ICONS_DIR$THEME_NAME"/
fi
}


#############################################################################################################
new_directory() {
 if test -e $ICONS_DIR$THEME_NAME; then
		zenity --error --text "Le répertoire $THEME_NAME existe déjà dans \n $ICONS_DIR\nVeuillez entrer un autre nom pour votre thème d'icônes..."
		working_directory
		new_directory
 else
	mkdir -p $WORK_DIR
 fi
}

#############################################################################################################
copy_categories() {
for current_category in $TODO_FINAL
		do
			choose_icon
			if [ "$CURRENT_ICON" != "" ]; then
				rm -f $WORK_DIR.$current_category.*
				cp -f $CURRENT_ICON $WORK_DIR.$current_category.$FORMAT
			fi
		done
}


#############################################################################################################
choose_icon() {
	zenity --info --title="CD Theme Creator" --text="Choisissez une image pour "$current_category"\n\nAnnuler pour passer\n"
	CURRENT_ICON=$(zenity --file-selection --filename $REDO_DIR --title="CD Theme Creator - Image "$current_category)
	if [ "$CURRENT_ICON" != "" ]; then
		REDO_DIR=`dirname $CURRENT_ICON`/		# Répertoire de l'icône
		GLOBAL_NAME=`basename $CURRENT_ICON`		# Nom de l'image avec son extension
		FILENAME=${GLOBAL_NAME%.*}			# Nom du fichier SANS extension
		FORMAT=${GLOBAL_NAME##*.}			# Extension SEULE
	fi
}


#############################################################################################################
todo_relink() {
TODO_FINAL=""
for categorie in $MAIN_CAT
	do 
		TODO_FINAL="$TODO_FINAL$categorie "
	done

for categorie in $DRIVES_CAT
	do 
		TODO_FINAL="$TODO_FINAL$categorie "
	done

for categorie in $SPECIFICS_CAT
	do 
		TODO_FINAL="$TODO_FINAL$categorie "
	done

zenity --info --title="CD Theme Creator" --text="$tempo_text"
WORK_DIR=$(zenity --file-selection --filename $ICONS_DIR --directory --title="CD Theme Creator")/

if [ "$WORK_DIR" = "/" ]; then
				zenity --error --text "L'opération a été annulée..."
				TODO_FINAL=""
				start_menu
fi
}



#############################################################################################################
relink_icons() {
IFS=" "
	for current_category in $TODO_FINAL
		do
			for FORMAT in "svg" "png"
				do
					if test -e $WORK_DIR.$current_category.$FORMAT; then
						cd $WORK_DIR
						links_definition
						for links in $LINKS
							do
								rm -f $links.*
								ln -s -f .$current_category.$FORMAT $links.$FORMAT
							done
					fi
				done
		done
}



#############################################################################################################
detail_list() {
DETAIL_LIST=""
for current_category in $TODO_FINAL
	do
		links_definition
		temp=""
		for links in $LINKS
			do
			temp="$temp[$links]"
			done

		DETAIL_LIST="$DETAIL_LIST$TRUE_FALSE $current_category $temp "
	done

TODO_FINAL=""
TODO_FINAL=$(zenity --width=750 --height=400 --list --column="Choix" --column="Catégories" --column="Liste des applications concernées" --checklist --text="Choisissez les catégories d'icônes à traiter\n\nATTENTION : Les icônes génériques sont OBLIGATOIRES\n" --title="CD Theme Creator" $DETAIL_LIST)
DETAIL_LIST="$TODO_FINAL"
TODO_FINAL=""
IFS="|"
for temp in $DETAIL_LIST
	do
		TODO_FINAL="$TODO_FINAL$temp "
	done
IFS=" "

if [ "$TODO_FINAL" = "" ]; then
				zenity --error --text "Aucune catégorie n'a été sélectionnée ou l'opération a été annulée..."
				TODO_FINAL=""
				start_menu
fi



zenity --info --title="CD Theme Creator" --text="Spécifier le répertoire dans lequel (re-)créer les liens"
WORK_DIR=$(zenity --file-selection --filename $ICONS_DIR --directory --title="CD Theme Creator")/


if [ "$WORK_DIR" = "/" ]; then
				zenity --error --text "Aucun répertoire n'a été spécifié ou l'opération a été annulée..."
				TODO_FINAL=""
				start_menu
fi


}


#############################################################################################################
do_report() {
tempo_text="Spécifier le répertoire où tester les liens"
todo_relink		# Definition $TODO_FINAL et $WORK_DIR
cd $WORK_DIR
rm -f $TEST_FILE
IFS=" "

for current_category in $TODO_FINAL
	do
		for FORMAT in "png" "svg"
			do
				if test -e .$current_category.$FORMAT; then
					echo "OK : La categorie $current_category EXISTE au format $FORMAT">>$TEST_FILE
					CURRENT=.$current_category.$FORMAT
					links_definition
					for links in $LINKS
						do
							if test -e $links.$FORMAT; then
								cd $WORK_DIR
							else
								echo "          ATTENTION : le lien $links.$FORMAT est absent de la categorie $current_category">>$TEST_FILE
							fi
						done
				else
					echo "La categorie $current_category n'a pas ete creee en $FORMAT">>$TEST_FILE
				fi
			done
		echo "">>$TEST_FILE
		echo "">>$TEST_FILE
	done

zenity --info --title="CD Theme Creator" --text="Un fichier de rapport $TEST_FILE a été créé dans $WORK_DIR\nCliquer sur Valider pour continuer"
cat $TEST_FILE | zenity --text-info --title="CD Theme Creator - $TEST_FILE" --width 550 --height=700
}




#############################################################################################################
# DEBUT DU PROGRAMME
#############################################################################################################


if [ $DEBUG -ne 1 ]; then
	check_new_script
fi


case $1 in

	"--help")
		echo -e "$VERT""Usage:"		
		echo -e "$NORMAL""$0""$VERT""         			Créer un nouveau thème" 
		echo -e "$NORMAL""$0 --relink""$VERT""        		Re-créer les liens dans un répertoire avec des liens cassés"
		echo -e "$NORMAL""$0 --choose_all""$VERT""        	Specifier la (ou les) catégorie(s) à traiter (Tout coché par défaut)"
		echo -e "$NORMAL""$0 --choose_none""$VERT""        	Specifier la (ou les) catégorie(s) à traiter (Tout décoché par défaut)"
		echo -e "$NORMAL""$0 --test""$VERT""        		Vérifier qu'un thème est complet"
		echo -e "$NORMAL""$0 --help""$VERT""          		Afficher ce menu d'aide "
		echo ""
		echo ""
		echo -e "$NORMAL""Auteur : Nochka85"
		echo -e "$NORMAL""Contact : nochka85@glx-dock.org"
		echo ""
		exit
	;;

	"--relink")
		relink
		exit
	;;

	"--choose_all")
		select_true
		exit
	;;

	"--choose_none")
		select_false
		exit
	;;
		
	"--test")
		do_report
		exit
	;;
esac

start_menu
exit



