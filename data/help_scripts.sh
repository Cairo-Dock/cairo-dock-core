#!/bin/sh

ARG=$1
ARG2=$2
#ARG3=$3
ERROR=0

up_install() {
	if [ "$ARG2" != "no" ]; then
		apt-get update
		apt-get install cairo-dock cairo-dock-plug-ins
	fi
}

addRepo() {
	# $1, repository address
	# $2, a few comments
	if [ "$1" = "" ]; then
		exit
	else
		myRepo="$1"
	fi
	if [ "$2" = "" ]; then
		comments="Additional Repository"
	else
		comments="$2"
	fi

	grep -r "$myRepo" /etc/apt/sources.list* > /dev/null
	if [ $? -eq 1 ]; then
		# the repository isn't in the list.
		echo "$myRepo ## $comments" | sudo tee -a /etc/apt/sources.list
	fi
}

repository() {
	addRepo "deb http://repository.glx-dock.org/ubuntu $(lsb_release -sc) cairo-dock" "Cairo-Dock-Stable"
	wget -q http://repository.glx-dock.org/cairo-dock.gpg -O- | apt-key add -
	up_install
}

ppa() {
	addRepo "deb http://ppa.launchpad.net/cairo-dock-team/ppa/ubuntu $(lsb_release -sc) main" "Cairo-Dock-PPA"
	apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E80D6BF5
	up_install
}

weekly() {
	addRepo "deb http://ppa.launchpad.net/cairo-dock-team/weekly/ubuntu $(lsb_release -sc) main" "Cairo-Dock-PPA-Weekly"
	apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E80D6BF5
	up_install
}

debian_stable() {
	addRepo "deb http://repository.glx-dock.org/debian stable cairo-dock" "Cairo-Dock-Stable"
	wget -q http://repository.glx-dock.org/cairo-dock.gpg -O- | apt-key add -
	up_install
}

debian_unstable() {
	addRepo "deb http://repository.glx-dock.org/debian unstable cairo-dock" "Cairo-Dock-Stable"
	wget -q http://repository.glx-dock.org/cairo-dock.gpg -O- | apt-key add -
	up_install
}

debian_stable_weekly() {
	addRepo "deb http://ppa.launchpad.net/cairo-dock-team/weekly/ubuntu hardy main" "Cairo-Dock-PPA-Weekly"
	apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E80D6BF5
	up_install
}

debian_unstable_weekly() {
	addRepo "deb http://ppa.launchpad.net/cairo-dock-team/weekly/ubuntu maverick main" "Cairo-Dock-PPA-Weekly"
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
	"debian_stable")
		debian_stable
	;;
	"debian_unstable")
		debian_unstable
	;;
	"debian_stable_weekly")
		debian_stable_weekly
	;;
	"debian_unstable_weekly")
		debian_unstable_weekly
	;;
	"compiz_plugin")
		compiz_plugin
	;;
esac
exit
