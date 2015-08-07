Cairo-Dock
==========

Cairo-Dock is a pretty, light and convenient interface to your desktop, able to replace advantageously your system panel! It features multi-docks, taskbar, launchers and a lot of useful applets. Applets can be detached from the dock to act as desktop widgets. Numerous ready-to-use themes are available in 1 click, and can be easily customized at your convenience. It can use hardware acceleration to be very fast and low on CPU.

[![Preview](http://download.tuxfamily.org/glxdock/communication/images/3.3/cd-panel-mix-600.jpg)](http://download.tuxfamily.org/glxdock/communication/images/3.3/cd-panel-mix.png)
Other screenshots are available at [Glx-Dock.org](http://www.glx-dock.org/mc_album.php?a=3).

This project is split in 3 parts:

  - Cairo-Dock-Core (the minimal - https://github.com/Cairo-Dock/cairo-dock-core)
  - Cairo-Dock-Plug-ins (a collection of views, applets, animations, effects, etc. - https://github.com/Cairo-Dock/cairo-dock-plug-ins)
  - Cairo-Dock-Plug-ins-Extras (a collection of third-party applets written in any language - https://github.com/Cairo-Dock/cairo-dock-plug-ins-extras)

The Cairo-Dock-Session project aims to provide an easy way to install a session that is desktop agnostic and uses Cairo-Dock as a Shell (the main interface).


Installation
------------

Please see our [Wiki at Glx-Dock.org](http://www.glx-dock.org/ww_page.php?p=By%20distributions&lang=en) for an excellent installation guide.

Tarballs of the sources are available at: https://launchpad.net/cairo-dock-core and https://launchpad.net/cairo-dock-plug-ins

A stable **PPA** is available here : https://launchpad.net/~cairo-dock-team/+archive/ppa


Need some help?
---------------

See our wiki! http://www.glx-dock.org/ww_page.php

You can find: how to launch the dock at startup, how to customise it, how to resolve some recurrent problems (black background, messages at startup, etc.), how to help us, who we are, etc.
A useful tutorial is also available to help you customize your dock: http://www.glx-dock.org/ww_page.php?p=Tutorial-Customisation&lang=en

If you have other questions not solved on our wiki you can post them on our forum (in English or French): http://www.glx-dock.org/bg_forumlist.php


Join the project
----------------

* Want to **HACK** into the dock or write an **APPLET**? see the complete C API here: http://doc.glx-dock.org
* You don't like C and still want to write an **APPLET** for the dock or **CONTROL** it from a script or the terminal? see its powerful DBus API here: http://www.glx-dock.org/ww_page.php?p=Control_your_dock_with_DBus&lang=en
* You have some **IDEAS / PROPOSITIONS**? You need some help to develop an applet? Feel free post a topic on our forum! We'll be happy to answer you!
* You want to **TRANSLATE** Cairo-Dock in your language? Use the 'Translations' tab in Launchpad, it's very simple to use it and helpful for us!


Bug reports
-----------

You can report bugs in **Launchpad** under the '[Bugs](https://bugs.launchpad.net/cairo-dock)' section. If possible:

* Read our wiki specially the '[Recurring Problems](http://www.glx-dock.org/ww_page.php?p=Recurrents%20problems&lang=en)' section which can help you to resolve some bugs like a black background, messages at startup, etc.
* Please include also some information like your distribution, your achitecture (32/64bits), your Desktop Manager (Gnome, KDE, XFCE,...) your Window Manager (Compiz, Metacity, Kwin, etc.), if you use Cairo-Dock with or without OpenGL, with which theme, etc.
* Include the method to reproduce the bug (which actions, which options activated)
* Run the dock with the command
          cairo-dock -l debug > debug.txt
reproduce the bug and join the content of debug.txt.
* If it's a crash, please give us a backtrace of it with GDB. It's easy thanks to this [wiki page](http://wiki.glx-dock.org/?p=ddd).

Thank you for your help!
