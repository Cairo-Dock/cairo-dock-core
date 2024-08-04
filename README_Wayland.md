Wayland support
===============

This version of Cairo-Dock includes support for running on Wayland, under compositors that support the [layer shell](https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-layer-shell-unstable-v1.xml) protocol. Mainly targeting [Wayfire](https://github.com/WayfireWM/wayfire), but possibly others as well, such as [labwc](https://github.com/labwc/labwc), [KWin](https://invent.kde.org/plasma/kwin) or [Cosmic](https://github.com/pop-os/cosmic-comp). Note that GNOME Shell / Mutter, including the default Ubuntu desktop, is unfortunately not supported.

Binary packages for Ubuntu 22.04 and 24.04 are available [here](https://launchpad.net/~cairo-dock-team/+archive/ubuntu/weekly).


Requirements
------------

Compilation:
 - Please see the original [guide](https://www.glx-dock.org/ww_page.php?p=By%20compiling&lang=en) for required dependencies. The following assumes that you are able to compile Cairo-Dock without Wayland support already.
 - [gtk-layer-shell](https://github.com/wmww/gtk-layer-shell/) (recommended at least version 0.6 to support keyboard events)
 - [extra-cmake-modules](https://invent.kde.org/frameworks/extra-cmake-modules)
 - wayland-scanner and Wayland development [libraries](https://gitlab.freedesktop.org/wayland/wayland)
 - GTK version at least 3.22 (also tested with version 3.24).

On recent Ubuntu (at least 22.04) and Debian (at least bookworm) versions, these are available by installing the following packages: `libgtk-layer-shell-dev`, `extra-cmake-modules`, `libwayland-dev` and `libwayland-bin`.

Running:
 - Compositor support for the [layer-shell](https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-layer-shell-unstable-v1.xml) protocol (recommended at least version 4 to allow the dock to receive keyboard events). See e.g. [here](https://gitlab.freedesktop.org/wlroots/wlroots/-/wikis/Projects-which-use-wlroots) and [here](https://github.com/solarkraft/awesome-wlroots#compositors) for candidates. Also, [KWin](https://invent.kde.org/plasma/kwin) version 5.20 and later is supported.
 - Optionally (but highly recommended) compositor support for the [foreign-toplevel-management](https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-foreign-toplevel-management-unstable-v1.xml), the [plasma-window-management](https://invent.kde.org/libraries/plasma-wayland-protocols/-/blob/master/src/protocols/plasma-window-management.xml) or the [cosmic-toplevel-management](https://github.com/pop-os/cosmic-protocols/tree/main/unstable) protocol; this is needed for taskbar functionality.
 - Optionally, if using Wayfire, [this plugin](https://github.com/dkondor/wayfire-scale-ipc) for some additional functionality.

Tested on Ubuntu 18.04, 20.04 and 22.04 with recent versions of [Wayfire](https://github.com/WayfireWM/wayfire) (0.7.0 and 0.8.0), [labwc](https://github.com/labwc/labwc) (0.6.5), [sway](https://github.com/swaywm/sway/) and [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots/) (0.16.2). Also tested with recent versions of [KWin](https://invent.kde.org/plasma/kwin) (5.20 and 5.24), running in standalone mode and part of the Plasma Desktop as well.


Compilation
-----------

See the original [guide](https://www.glx-dock.org/ww_page.php?p=By%20compiling&lang=en) for the steps to compile and install Cario-Dock. The cmake configuration summary will report if the required components are enabled based on the availability of required dependencies:
 - You should see `* With gtk-layer-shell: yes` in the output of cmake. If not, check that the gtk-layer-shell library is properly installed.
 - It will also output `* With Wayland taskbar: yes` to indicate if Wayland protocols required for taskbar functionality can be built. If not, check that [extra-cmake-modules](https://invent.kde.org/frameworks/extra-cmake-modules) and the `wayland-scanner` program are properly installed.
 - The `enable-gtk-layer-shell` cmake option can be used to manually disable / enable layer-shell support.
 - To use hardware acceleration on Wayland, EGL support should be enabled and GLX support should be disabled. If this is not the case, check that the EGL libraries are available.


Running
-------

The `WAYLAND_DISPLAY` environment variable needs to be set when running on Wayland (typically `wayland-1` on recent wlroots or `wayland-0` on KWin). Cairo-Dock can be forced to try to use the Wayland or X11 backend with the `-L` and `-X` command line switch resepctively. Also, the `-c` switch can be used to disable OpenGL, and the `-o` switch to try to force it. These can be useful especially when running in a nested compositor.

When using [systemd](https://systemd.io/), starting Cairo-Dock at login can be enabled via the supplied .service file:
```
systemctl --user enable cairo-dock.service
```

What works:
 - Dock positioning along a screen edge and keeping above / below and reserving space with the layer-shell protocol.
 - Proper positioning of subdocks, menus and dialogs.
 - Hardware acceleration with EGL.
 - Taskbar if the compositor supports the corresponding protocols.
 - Hiding and recalling docks hidden or kept below.

Known issues and limitations:
 - It is not possible to set global keyboard shortcuts for actions.
 - Subdocks disappear when an app is activated (this is a limitation of using xdg-popups; seems not the be an issue on KWin).
 - Detecting if the dock overlaps with open windows only works on KWin.
 - "Keep the dock below" option is buggy on KWin (at least version 5.24). See [here](https://bugs.kde.org/show_bug.cgi?id=480795) for more info; testing on more recent KWin versions is very welcome!
 - Crashes in some situations which I believe are caused by a [bug in wlroots](https://gitlab.freedesktop.org/wlroots/wlroots/-/issues/2543). This is fixed in recent wlroots versions (0.17). For older wlroots versions (0.16), [this](https://github.com/dkondor/wlroots/commit/ed74e3ae4d96196737eb41e9b0756b5fa15379d8) or [this](https://github.com/swaywm/wlroots/pull/2551) patch should solve it.
 - Multi-monitor support is limited: docks can be placed only on a single screen.
 - Tracking virtual desktops / workspaces is not supported.
 - Dialogs flicker sometimes (although this seems to happen on X11 too in some cases).
 - EGL rendering under Wayland relies on an officially unsupported interface in GTK (i.e. it uses [gtk_widget_set_double_buffered()](https://developer.gnome.org/gtk3/stable/GtkWidget.html#gtk-widget-set-double-buffered) which is only supported on X11); in practice it works well at least with GTK versions 3.22.30 and 3.24.33 on Wayfire and Sway.
 - EGL / OpenGL does not work on older KWin versions (5.20). An error message complains about some layer-shell surfaces having a zero size set; this might be related to the above point. Run the dock with the `-c` switch.
 - For issues with HiDPI monitors see [here](https://github.com/dkondor/cairo-dock-core/issues/7); most functionality should work.

