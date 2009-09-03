#!/bin/sh

# Updater for Cairo-Dock
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

#       cairo-dock-update.sh
#
#       Script qui télécharge et installe les packages de cairo-dock (core et plugins) depuis
#         http://download.berlios.de/cairo-dock/ pour la version stable (par défaut)
#         http://fabounet03.free.fr pour la version unstable (option -u)
#
#       Ne PAS lancer en super-administrateur
#
#       @author sombrero [Colin Darie] <colindarie@gmail.com>
#       @version 2007-08-30 23:00:15 +0200
#

#       Merci à Fabounet pour la réalisation du dock !
#       Topic sur ubuntu-fr : http://forum.ubuntu-fr.org/viewtopic.php?id=131714
#       Documentation : http://doc.ubuntu-fr.org/gnome_dock


## Répertoire de travail
## Il n'est PAS supprimé à la fin du script (un fichier y reste pour éviter de re-télécharger 2x la même version)
DIR=`echo $HOME/.cairo-dock/upgrade`

## Commande lancée à la fin du script pour relancer le dock (laisser vide pour ne pas le relancer)
COMMAND_RESTART_DOCK="/usr/bin/cairo-dock"



## Paramètres serveur HTTP
HOST_STABLE="http://download.berlios.de/cairo-dock"
HOST_UNSTABLE="http://fabounet03.free.fr"
FILE_MD5SUM_REMOTE="md5sum.txt"

## Regexp utilisées pour récupérer les noms des fichiers .deb
REGEXP_CORE="cairo-dock_.+*.deb"
REGEXP_PLUGINS="cairo-dock-plug.+*.deb"

## Nom des fichiers md5sum utilisés en local
FILE_MD5SUM_LOCAL="cairo-dock-md5sum"
FILE_MD5SUM_OLD="cairo-dock-md5sum.old"

echo "***************"
echo "*** WARNING ***"
echo "***************"
echo " You are about to install a release candidate package."
echo " That means it was created for testing purpose, and can still contain some bugs."
echo " A repository exists for Ubuntu 7.10 and 8.04, you are encouraged to use it if you're under these OS (It should also work for other debian-based OS)."
echo "Please visit http://cairo-dock.org for an easy way to install Cairo-Dock."
read -p "  Continue anyway ? [y/N]" accept_the_risks
if test ! "$accept_the_risks" = "y" -a ! "$accept_the_risks" = "Y"; then
	exit 0
fi

echo "let's go!"

## Création du repertoire de download s'il n'existe pas
if [ ! -d $DIR ] ; then
  mkdir $DIR
fi

cd $DIR

# Recup des options (pour l'instant uniquement -u pour unstable)
UNSTABLE=0
while getopts "u" opt; do
  case $opt in
    u) UNSTABLE=1 ;;
  esac
done


## Suivant qu'on ait passé l'option --unstable au dock ou pas, on choisit le bon host
if [ $UNSTABLE -eq 1 ] ; then
  HOST=$HOST_UNSTABLE;
else
  HOST=$HOST_STABLE;
fi


## Si on ne trouve pas d'ancien fichier md5sum.old , on en créé un vide
if [ ! -e $FILE_MD5SUM_OLD ] ; then
  echo "" > $FILE_MD5SUM_OLD
fi

## Récupération du fichier md5sum qui permet de connaitre les noms des .deb à télécharger
wget $HOST/$FILE_MD5SUM_REMOTE -O $FILE_MD5SUM_LOCAL
if ! test "$?" = "0"; then

## On compare le fichier md5sum avec l'ancien fichier md5sum pour savoir s'il y a des mises à jour
NEED_UPGRADE=`diff -q $FILE_MD5SUM_LOCAL $FILE_MD5SUM_OLD`

if [ -n "$NEED_UPGRADE" ]; then

  echo "Mises à jour disponibles...";

  ## Récupération des noms des fichiers .deb
  FILE_CORE=`cat $FILE_MD5SUM_LOCAL | grep -oE $REGEXP_CORE`
  FILE_PLUGINS=`cat $FILE_MD5SUM_LOCAL | grep -oE $REGEXP_PLUGINS`

  ## Téléchargements des .deb
  wget $HOST/$FILE_CORE \
   || ! echo "Le fichier $FILE_CORE indiqué par le fichier md5sum n'est pas disponible sur le serveur. Aucun changement effectué." \
      || exit 1

  wget $HOST/$FILE_PLUGINS \
    || ! echo "Le fichier $FILE_PLUGINS indiqué par le fichier md5sum n'est pas disponible sur le serveur. Aucun changement effectué." \
      || ! rm $FILE_CORE || exit 1


  ## Installation des .deb (gksudo demande le mot de passe admin et -S force l'utilisation de sudo à la place de gksudo)
  gksudo -S "dpkg -i $FILE_CORE"
  gksudo -S "dpkg -i $FILE_PLUGINS"


  ## Suppression des fichiers temporaires
  rm $FILE_CORE $FILE_PLUGINS

  ## On relance le dock si demandé
  if [ -n "$COMMAND_RESTART_DOCK" ] ; then
    ## Modif de Vilraleur : tuer les cairo-dock deja ouverts.
    echo "On tue éventuellement tous les cairo-dock d'ouvert"
    killall cairo-dock

    echo "Redémarrage de cairo-dock avec la commande $COMMAND_RESTART_DOCK ..."
    exec $COMMAND_RESTART_DOCK &
  fi

  ## Remplacement du md5sum.old par le nouveau
  mv $FILE_MD5SUM_LOCAL $FILE_MD5SUM_OLD


else
  echo "Pas de mise à jour disponible...";

  ## Suppression du fichier md5sum inutile
  rm $FILE_MD5SUM_LOCAL
fi

echo "Terminé.";

exit
