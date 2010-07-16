#!/bin/sh

ARG=$1
ARG2=$2
#ARG3=$3

up_install() {
	if [ "$ARG2" != "no" ]; then
		apt-get update
		apt-get install cairo-dock cairo-dock-plug-ins
	fi
}

repository() {
	echo "deb http://repository.glx-dock.org/ubuntu $(lsb_release -sc) cairo-dock ## Cairo-Dock-Stable" | tee -a /etc/apt/sources.list
	wget -q http://repository.glx-dock.org/cairo-dock.gpg -O- | apt-key add -
	up_install
}

ppa() {
	echo "deb http://ppa.launchpad.net/cairo-dock-team/ppa/ubuntu $(lsb_release -sc) main ## Cairo-Dock-PPA" | tee -a /etc/apt/sources.list
	apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E80D6BF5
	up_install
}

weekly() {
	echo "deb http://ppa.launchpad.net/cairo-dock-team/weekly/ubuntu $(lsb_release -sc) main ## Cairo-Dock-PPA-Weekly" | tee -a /etc/apt/sources.list
	apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E80D6BF5
	up_install
}

compiz_plugin() {
	COMPIZ_PLUGINS=`gconftool-2 --get /apps/compiz/general/allscreens/options/active_plugins`
	if [ `echo $COMPIZ_PLUGINS |grep -c $ARG2` -eq 0 ]; then
		NEW_PG_LIST="`echo $COMPIZ_PLUGINS |cut -d] -f1`,$ARG2]"
		gconftool-2 --type list --list-type string --set /apps/compiz/general/allscreens/options/active_plugins "$NEW_PG_LIST"
	fi
}

case $ARG in
	"repository")
		repository
	;;
	"ppa")
		ppa
	;;
	"weekly")
		weekly
	;;
	"compiz_plugin")
		compiz_plugin
	;;
esac
exit
