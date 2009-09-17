#!/bin/sh

if test "x$1" = "x"; then
	t="$HOME/.config/cairo-dock/current_theme/icons"
else
	t="$1"
fi

cd "$t"
find . -type l -delete

for suff in "svg" "png"
do
	echo "  creation of the $suff links ..."
	
	if test -e web-browser.$suff; then
		echo "    towards web-browser.$suff"
		ln -s web-browser.$suff firefox.$suff
		ln -s web-browser.$suff opera.$suff
		ln -s web-browser.$suff epiphany.$suff
	fi
	if test -e firefox.$suff; then
		ln -s firefox.$suff firefox-3.0.$suff
		ln -s firefox.$suff firefox-3.5.$suff
		ln -s firefox.$suff firefox-4.0.$suff
	fi
	if test -e file-browser.$suff; then
		echo "    towards file-browser.$suff"
		ln -s file-browser.$suff nautilus.$suff
		ln -s file-browser.$suff konqueror.$suff
		ln -s file-browser.$suff thunar.$suff
		ln -s file-browser.$suff pcmanfm.$suff
	fi
	
	if test -e mail-reader.$suff; then
		echo "    towards mail-reader.$suff"
		ln -s mail-reader.$suff mozilla-thunderbird.$suff
		ln -s mail-reader.$suff thunderbird.$suff
		ln -s mail-reader.$suff kmail.$suff
		ln -s mail-reader.$suff evolution.$suff
	fi
	if test -e image-reader.$suff; then
		echo "    towards image-reader.$suff"
		ln -s image-reader.$suff eog.$suff
		ln -s image-reader.$suff gqview.$suff
		ln -s image-reader.$suff gwenview.$suff
		ln -s image-reader.$suff f-spot.$suff
	fi
	if test -e audio-player.$suff; then
		echo "    towards audio-player.$suff"
		ln -s audio-player.$suff xmms.$suff
		ln -s audio-player.$suff bmp.$suff
		ln -s audio-player.$suff beep-media-player.$suff
		ln -s audio-player.$suff rhythmbox.$suff
		ln -s audio-player.$suff amarok.$suff
	fi
	if test -e video-player.$suff; then
		echo "    towards video-player.$suff"
		ln -s video-player.$suff totem.$suff
		ln -s video-player.$suff mplayer.$suff
		ln -s video-player.$suff vlc.$suff
		ln -s video-player.$suff xine.$suff
		ln -s video-player.$suff kaffeine.$suff
	fi
	
	if test -e writer.$suff; then
		echo "    towards writer.$suff"
		ln -s writer.$suff gedit.$suff
		ln -s writer.$suff kate.$suff
		ln -s writer.$suff ooo-writer.$suff
		ln -s writer.$suff abiword.$suff
		ln -s writer.$suff emacs.$suff
	fi
	if test -e bittorrent.$suff; then
		echo "    towards bittorrent.$suff"
		ln -s bittorrent.$suff transmission.$suff
		ln -s bittorrent.$suff deluge.$suff
		ln -s bittorrent.$suff bittornado.$suff
		ln -s bittorrent.$suff gnome-btdownload.$suff
		ln -s bittorrent.$suff ktorrent.$suff
	fi
	if test -e download.$suff; then
		echo "    towards download.$suff"
		ln -s download.$suff amule.$suff
		ln -s download.$suff emule.$suff
		ln -s download.$suff filezilla.$suff
	fi
	if test -e cd-burner.$suff; then
		echo "    towards cd-burner.$suff"
		ln -s cd-burner.$suff nautilus-cd-burner.$suff
		ln -s cd-burner.$suff graveman.$suff
		ln -s cd-burner.$suff gnome-baker.$suff
		ln -s cd-burner.$suff k3b.$suff
		ln -s cd-burner.$suff brasero.$suff
	fi
	if test -e image.$suff; then
		echo "    towards image.$suff"
		ln -s image.$suff gimp.$suff
		ln -s image.$suff inkscape.$suff
		ln -s image.$suff krita.$suff
	fi
	
	if test -e messenger.$suff; then
		echo "    towards messenger.$suff"
		ln -s messenger.$suff gaim.$suff
		ln -s messenger.$suff pidgin.$suff
		ln -s messenger.$suff kopete.$suff
		ln -s messenger.$suff amsn.$suff
		ln -s messenger.$suff emessene.$suff
	fi
	if test -e irc.$suff; then
		echo "    towards irc.$suff"
		ln -s irc.$suff xchat.$suff
		ln -s irc.$suff konversation.$suff
		ln -s irc.$suff kvirc.$suff
	fi
	
	if test -e terminal.$suff; then
		echo "    towards terminal.$suff"
		ln -s terminal.$suff gnome-terminal.$suff
		ln -s terminal.$suff konsole.$suff
		ln -s terminal.$suff xfce4-terminal.$suff
	fi
	if test -e packages.$suff; then
		echo "    towards packages.$suff"
		ln -s packages.$suff synaptic.$suff
		ln -s packages.$suff adept.$suff
		ln -s packages.$suff pacman-g2.$suff
	fi
	if test -e system-monitor.$suff; then
		echo "    towards system-monitor.$suff"
		ln -s system-monitor.$suff ksysguard.$suff
		ln -s system-monitor.$suff utilities-system-monitor.$suff
	fi
	if test -e calculator.$suff; then
		echo "    towards calculator.$suff"
		ln -s calculator.$suff gnome-calculator.$suff
		ln -s calculator.$suff crunch.$suff
	fi
done;

echo "all links have been generated."
