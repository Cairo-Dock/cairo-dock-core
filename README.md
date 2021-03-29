Cairo-Dock
==========

Cairo-Dock is a pretty, light and convenient interface to your desktop, able to replace advantageously your system panel!

This repository contains an experimental fork that adds support for running on Wayland, under compositors that support the [layer shell](https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-layer-shell-unstable-v1.xml) protocol.

Note that this repository is not affiliated in any way with the Cairo-Dock project. Official releases of Cairo-Dock are available in the following repositories:

  - Cairo-Dock-Core (the minimal - https://github.com/Cairo-Dock/cairo-dock-core)
  - Cairo-Dock-Plug-ins (a collection of views, applets, animations, effects, etc. - https://github.com/Cairo-Dock/cairo-dock-plug-ins)
  - Cairo-Dock-Plug-ins-Extras (a collection of third-party applets written in any language - https://github.com/Cairo-Dock/cairo-dock-plug-ins-extras)


Requirements
------------

 - Please see the original [guide](https://www.glx-dock.org/ww_page.php?p=By%20compiling&lang=en) for required dependencies. The following assumes that you are able to compile the official Git version of Cairo-Dock.
 - [gtk-layer-shell](https://github.com/wmww/gtk-layer-shell/) (recommended at least version 0.6 to support keyboard events)
 - GTK version at least 3.22.
 - Compositor support for the [layer-shell](https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-layer-shell-unstable-v1.xml) protocol (recommended at least version 4 to allow the dock to receive keyboard events). See e.g. [here](https://github.com/swaywm/wlroots/wiki/Projects-which-use-wlroots) and [here](https://github.com/solarkraft/awesome-wlroots#compositors) for candidates. Also, [KWin](https://invent.kde.org/plasma/kwin) version 5.20 and later is supported.
 - (Optionally) compositor support for the [foreign-toplevel-management](https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-foreign-toplevel-management-unstable-v1.xml) or the [plasma-window-management](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/plasma-window-management.xml) protocol; this is needed for taskbar functionality.

Tested on Ubuntu 18.04 with [Wayfire](https://github.com/WayfireWM/wayfire), [sway](https://github.com/swaywm/sway/) and [wlroots](https://github.com/swaywm/wlroots/) compiled from recent git. Also tested with [KWin 5.20](https://invent.kde.org/plasma/kwin) (only works without OpenGL; only tested without Plasma shell).


Compilation
-----------

See the original [guide](https://www.glx-dock.org/ww_page.php?p=By%20compiling&lang=en) for the steps to compile and install Cario-Dock; replace the cairo-dock-core repository with this branch. Note that plug-ins should be ABI compatible with Cairo-Dock compiled from this branch. This means that a recent version of the plug-ins from the [official repository](https://github.com/Cairo-Dock/cairo-dock-plug-ins) will work well.

Additional things to consider:
 - The cmake configuration summary will report if gtk-layer-shell is enabled, i.e. you should see `* With gtk-layer-shell: yes` in the output of cmake. If not, check that the gtk-layer-shell library is properly installed.
 - The `enable-gtk-layer-shell` cmake option can be used to manually disable / enable layer-shell support.
 - The `use-new-positioning-on-x11` cmake option can be used to change to some new code paths under X11 as well. This is useful to test for regressions.
 - To use hardware acceleration on Wayland, EGL support should be enabled and GLX support should be disabled. If this is not the case, check that the EGL libraries are available.


Running
-------

The `WAYLAND_DISPLAY` environment variable needs to be set when running on Wayland (typically `wayland-1` on recent wlroots or `wayland-0` on KWin). Cairo-Dock can be forced to try to use the Wayland or X11 backend with the `-L` and `-X` command line switch resepctively. Also, the `-c` switch can be used to disable OpenGL, and the `-o` switch to try to force it. These can be useful especially when running in a nested compositor.


What works:
 - Dock positioning along a screen edge and keeping above / below and reserving space with the layer-shell protocol.
 - Proper positioning of subdocks, menus and dialogs.
 - Hardware acceleration with EGL.
 - Taskbar (if the compositor supports the [foreign-toplevel-management](https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-foreign-toplevel-management-unstable-v1.xml) or [plasma-window-management](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/plasma-window-management.xml) protocol).

Known issues:
 - Subdocks disappear when an app is activated (this is a limitation of using xdg-popups).
 - Hiding / showing the dock does not work (hiding works, but there is no way to recall the dock).
 - Tracking visibility does not work, i.e. it is not detected if the dock overlaps with open windows.
 - Crashes in some situations which I believe are caused by a [bug in wlroots](https://github.com/swaywm/wlroots/issues/2543). [This patch](https://github.com/swaywm/wlroots/pull/2551) should solve it.
 - Multi-monitor support is very limited (the dock will show up on whichever output it thinks is the main one when starting; behavior is buggy when trying to set another output for it).
 - Tracking virtual desktops / workspaces is not supported.
 - Dialogs flicker sometimes (although this seems to happen on X11 too in some cases).
 - EGL rendering under Wayland relies on an officially unsupported interface in GTK (i.e. it uses [gtk_widget_set_double_buffered()](https://developer.gnome.org/gtk3/stable/GtkWidget.html#gtk-widget-set-double-buffered) which is only supported on X11); in practice it works well at least with GTK 3.22.30 on Wayfire and Sway.
 - EGL / OpenGL does not work on KWin (complains about some layer-shell surfaces having a zero size set; this might be related to the above point), run the dock with the `-c` switch.
 - On KWin, clicking on the active app to minimize it does not work most of the time (minimizing from the menu works).
 - Wayland protocols should not be included directly as header files, but should be created dynamically by running `wayland-scanner` as part of the build process. I don't know how to do this with CMake.


Other resources
---------------

Please see the original [repository](https://github.com/Cairo-Dock/cairo-dock-core) and [homepage](https://www.glx-dock.org). Please do not report bugs from this branch there.
