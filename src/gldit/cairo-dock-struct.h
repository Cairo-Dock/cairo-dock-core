/*
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CAIRO_DOCK_STRUCT__
#define  __CAIRO_DOCK_STRUCT__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <cairo.h>

/*
 * We include rsvg stuff here, because it's such a mess that it's better to not
 *  duplicate that.
 * We need to include glib.h before because it uses G_BEGIN_DECL without
 * including glib first! (bug fixed with the version 2.36.2)
 * Note: with this 2.36.2 version, rsvg-cairo.h and librsvg-features.h are
 *  included in rsvg.h and including these header files directly are now
 *  deprecated. But we need 'librsvg-features.h' to use LIBRSVG_CHECK_VERSION
 *  macro in order to know if we can include (or not) librsvg-cairo.h...
 *   Yeah, a bit strange :)
 */
#include <librsvg/rsvg.h>
#ifndef LIBRSVG_CHECK_VERSION
#include <librsvg/librsvg-features.h>
#endif
#if !LIBRSVG_CHECK_VERSION (2, 36, 2)
#include <librsvg/rsvg-cairo.h>
#endif

#include <glib/gi18n.h>

#include <GL/gl.h>

/*! \mainpage Cairo-Dock's API documentation.
 * \ref intro_sec
 * 
 * \ref install_sec
 * 
 * \ref struct_sec
 * -  \ref objects
 * -  \ref managers
 * -  \ref containers
 * -  \ref icons
 * -  \ref dock
 * -  \ref desklet
 * -  \ref dialog
 * -  \ref modules
 * -  \ref module-instances
 * -  \ref drawing
 * -  \ref windows
 * 
 * \ref applets_sec
 * -  \ref creation
 * -  \ref first_steps
 * -  \ref further
 * -  \ref opengl
 * -  \ref animation
 * -  \ref tasks
 * -  \ref key_binding
 * -  \ref sub_icons
 * -  \ref translations
 * 
 * \ref advanced_sec
 * -  \ref advanced_config
 * -  \ref steal_appli
 * -  \ref data_renderer
 * -  \ref multi
 * -  \ref render_container
 * -  \ref system_applet
 * -  \ref auto_load
 * 
 * 
 * \n
 * \section intro_sec Introduction
 *
 * This documentation presents the core library of Cairo-Dock: <i>libgldi</i> (GL Desktop Interface).
 *
 * It is useful if you want to write a plug-in, add new features in the core, or just love C.\n
 * Note: to write applets in any language very easily, see https://github.com/Cairo-Dock/cairo-dock-core/wiki/Writing-an-applet.
 *
 * It has a <b>decentralized conception</b> and is built of several modules: internal modules (\ref managers) and external modules (\ref modules) that can extend it.\n
 * It also has an \ref objects architecture.
 * 
 * \n
 * \section install_sec Installation
 *
 * The installation is very easy and uses <i>cmake</i>. In a terminal, copy-paste the following commands :
 * \code
 *   ### grab the sources of the core
 *   mkdir CD && cd CD
 *   bzr checkout --lightweight lp:cairo-dock-core
 *   ### compil the dock and install it
 *   cd cairo-dock-core
 *   cmake CMakeLists.txt -DCMAKE_INSTALL_PREFIX=/usr
 *   make
 *   sudo make install
 *   ### grab the sources of the plug-ins
 *   cd ..
 *   bzr checkout --lightweight lp:cairo-dock-plug-ins
 *   ### compil the stable plug-ins and install them
 *   cmake CMakeLists.txt -DCMAKE_INSTALL_PREFIX=/usr
 *   make
 *   sudo make install
 * \endcode
 * To install unstable plug-ins, add -Denable-xxx=yes to the cmake command, where xxx is the lower-case name of the applet.
 * 
 * 
 * \n
 * \section struct_sec Main structures
 *
 * \subsection objects Objects
 * Any element in <i>libgldi</i> is a _GldiObject.\n
 * An Object is created by an ObjectManager, which defines the properties and notifications of its children.\n
 * It has a reference counter, can be deleted from the current theme, and can be reloaded.\n
 * An Object can cast <b>notifications</b>; notifications are broadcasted on its ObjectManager.\n
 * An ObjectManager can inherit from another ObjectManager; in this case, all methods of the parent ObjectManagers are called recursively, and likewise all notifications on an Object are casted recursively to all parent ObjectManagers.
 * 
 * See _GldiObject and cairo-dock-object.h for more details.
 * 
 * \subsection managers Managers
 * The core is divided in several internal modules, called Managers.\n
 * Each Manager manages a set of parameters and objects (for instance, the Dock Manager manages the list of all Docks and their parameters).
 * 
 * See _GldiManager and cairo-dock-manager.h for more details.
 * 
 * \subsection containers Containers
 * Containers are generic animated windows. They can hold Icons and support cairo/OpenGL drawing.
 * 
 * See _GldiContainer and cairo-dock-container.h for more details.
 * 
 * \subsection icons Icons
 * Icons are elements inside a Container on which the user can interact. For instance, a Launcher is an Icon that launches a program on left-click.
 * 
 * See _Icon and cairo-dock-icon-factory.h for more details.
 * 
 * \subsection dock Dock
 * Docks are a kind of Container that sits on a border of the screen.
 * 
 * See _CairoDock and cairo-dock-dock-factory.h for more details.
 * 
 * \subsection desklet Desklet
 * Desklets are a kind of Container that stays on the desktop and holds one or many icons.
 * 
 * See _CairoDesklet and cairo-dock-desklet-factory.h for more details.
 * 
 * \subsection dialog Dialog
 * Dialogs are a kind of Container that holds no icon, but rather point to an icon, and are used to display some information or interact with the user.
 * 
 * See _CairoDialog and cairo-dock-dialog-factory.h for more details.
 * 
 * \subsection modules Modules
 * A Module is an Object representing a plug-in for <i>libgldi</i>.\n
 * It defines a set of properties and an interface for init/stop/reload.\n
 * A Module that adds an Icon is called an <i>"applet"</i>.
 * 
 * See _GldiModule and cairo-dock-module-manager.h for more details.
 * 
 * Note: the <a href="https://launchpad.net/cairo-dock-plug-ins">cairo-dock-plug-ins</a> project is a set of modules in the form of loadable libraries (.so files).\n
 * the <a href="https://launchpad.net/cairo-dock-plug-ins-extras">cairo-dock-plug-ins-extra</a> project is a set of modules in the form of scripts (Python or any language) that interact on the core through Dbus.
 *
 * \subsection module-instances Module-Instances
 * A Module-Instance is an actual instance of a Module.\n
 * It holds a set of parameters and data (amongst them the Applet-Icon if it's an applet).\n
 * A Module can have several instances.
 * 
 * See _GldiModuleInstance and cairo-dock-module-instance-manager.h for more details.
 * 
 * 
 * \subsection drawing Drawing with cairo/opengl
 * 
 * libgldi defines \ref _CairoDockImageBuffer, a generic Image that works for both cairo and OpenGL.\n
 * See cairo-dock-image-buffer.h for more details.
 * 
 * It is possible to add small images above Icons; they are called \ref _CairoOverlay.\n
 * For instance quick-info and progress-bars are Overlays.\n
 * See cairo-dock-overlay.h for more details.
 * 
 * 
 * \subsection windows Windows management
 * 
 * libgldi keeps track of all the currently existing windows, with all their properties, and notifies everybody of any change. It is used for the Taskbar.\n
 * Each window has a corresponding \ref _GldiWindowActor object.\n
 * See cairo-dock-windows-manager.h for more details.
 * 
 * \subsection hidpi Display scaling
 * 
 * Cairo-Dock now supports integer display scale factors > 1 (fractional scaling is not supported and will likely result in a higher integer scale factor used). Display scale is determined by querying it from GDK using gdk_window_get_scale_factor () of the window to be rendered, or the primary dock (g_pMainDock).
 * 
 * Most of the code is independent of the scale factor and uses logical pixels according to how rendering in GTK works. Specifically, the Cairo rendering code does this entirely, as the context supplied by GTK has the proper scale factor set. We need to consider the scale factor in the following cases though:\n
 *   - OpenGL rendering: Contrary to Cairo, we need to manually set up a "scaling" before rendering. This is done by calling glScalef () with the correct scale factor before rendering (so that the rest of the code can work with logical coordinates) and by giving the physical pixel size of surfaces when setting up our viewports (\ref gldi_gl_container_set_ortho_view () and \ref gldi_gl_container_set_perspective_view () in cairo-dock-opengl.c).
 *   - EGL: when using EGL with Wayland, we need to give the correct pixel size when creating surfaces corresponding to our windows; also, we need to explicitly set the surface scale in some cases (cairo-dock-egl.c)
 *   - Creating surfaces: when creating Cairo surfaces and OpenGL textures that we draw to, we need to ensure that these have the correct pixel size (i.e. logical size multiplied by the scale factor). For Cairo surfaces, we need to set a scale factor as well with cairo_surface_set_device_scale (). This is done in cairo-dock-surface-factory.c, using the scale factor from g_pMainDock for now (but could be extended to take the scale factor as a parameter to handle situations when rendering to multiple displays with different scale factors).
 *   - Loading images: most importantly, when loading any image (e.g. icons corresponding to windows in the taskbar / applets), we need to ensure that it gets loaded with the correct size, so that it will be rendered without being blurry. This is supported by a combination of multiple components: (1) The \ref cairo_dock_search_icon_s_path () function is used to find the actual image files to load in most cases. This function automatically uses the scale factor of g_pMainDock when searching images (with gtk_icon_theme_lookup_icon_for_scale ()) to find image files with the correct resolution. (2) Icons to be displayed in the dock are then loaded in their natural size, which should then be large enough to be rendered correctly. (3) In some cases, we load an image at a specified size; we take care to supply the scaled size here. (4) SVG images are loaded by "rendering" them to a Cairo surface with the correct pixel size and scale. E.g. if you just need an icon at a specified size, use \ref cairo_dock_create_surface_from_icon () that will ensure that the correct scale is used.
 *   - Tracking window locations on X11: when communicating directly with the X server, we get unscaled coordinates (that correspond to physical pixels on the screen). This is relevant for tracking window locations, checking if they overlap with the dock or sending windows to specific viewport / display. We handle these by dividing the coordinates by the scale factor got from GDK, so that we can work in logical coordinates here as well. Note: this assumes all monitors have the same scale which seems to be the case always for X11.
 * 
 * \n
 * \section applets_sec External Modules
 * 
 * \subsection creation Create a new applet
 * 
 * Go to the "plug-ins" folder, and run the <i>generate-applet.sh</i> script. Answer the few questions, and you're done!\n
 * The script creates a <module-name> folder, with <i>src</i> and <i>data</i> sub-folders, which contain the following:\n
 *   - data/icon.png: the default icon of your applet
 *   - data/preview.jpg: a preview of your applet, around 200x200 pixels
 *   - data/<module-name>.conf.in: the config file of your applet
 *   - src/applet-init.c: contains the <i>init</i>, <i>stop</i> and <i>reload</i> methods, as well as the definition of your applet.
 *   - src/applet-config.c: container the <i>get_config</i> and <i>reset_config</i> methods
 *   - src/applet-notifications.c: contains the callbacks of your applet (ie, the code that is called on events, for instance on click on the icon)
 *   - src/applet-struct.h: contains the structures (Config, Data, and any other you may need)
 * 
 * Note: when adding a new file, don't forget to add it in the CMakeLists.txt.\n
 * when changing something in the config file, don't forget to update the version number of the applet, in the main CMakeLists.txt.\n
 * when changing anything, don't forget to install (<i>sudo make install</i>)
 * 
 * 
 * \subsection first_steps First steps
 * 
 * Edit the file <i>src/applet-inic.c</i>; the macro \ref CD_APPLET_DEFINITION2 is a convenient way to define an applet: just fill its name, its category, a brief description, and your name.
 * 
 * In the section CD_APPLET_INIT_BEGIN/CD_APPLET_INIT_END, write the code that will run on startup.
 * 
 * In the section CD_APPLET_STOP_BEGIN/CD_APPLET_STOP_END, write the code that will run when the applet is deactivated: remove any timer, destroy any allocated ressources, unregister notifications, etc.
 * 
 * In the section CD_APPLET_RELOAD_BEGIN/CD_APPLET_RELOAD_END section, write the code that will run when the applet is reloaded; this can happen in 2 cases:
 *   - when the configuration is changed (\ref CD_APPLET_MY_CONFIG_CHANGED is TRUE, for instance when the user edits the applet)
 *   - when something else changed (\ref CD_APPLET_MY_CONFIG_CHANGED is FALSE, for instance when the icon theme is changed, or the icon size is changed); in this case, most of the time you have nothing to do, except if you loaded some ressources yourself.
 * 
 * Edit the file <i>src/applet-config.c</i>;
 * In the section CD_APPLET_GET_CONFIG_BEGIN/CD_APPLET_GET_CONFIG_END, get all your config parameters (don't forget to define them in applet-struct.h). Use the CD_CONFIG_GET_* macros (defined in cairo-dock-applet-facility.h) to do so conveniently.
 * 
 * In the section  CD_APPLET_RESET_CONFIG_BEGIN/CD_APPLET_RESET_CONFIG_END, free any config parameter that was allocated (for instance, strings).
 * 
 * Edit the file <i>src/applet-notifications.c</i>;
 * 
 * In the section CD_APPLET_ON_CLICK_BEGIN/CD_APPLET_ON_CLICK_END, write the code that will run when the user clicks on the icon (or an icon of the sub-dock).
 * 
 * There are other similar sections available:
 * - \ref CD_APPLET_ON_MIDDLE_CLICK_BEGIN/\ref CD_APPLET_ON_MIDDLE_CLICK_END for the actions on middle click on your icon or one of its sub-dock.
 * - \ref CD_APPLET_ON_DOUBLE_CLICK_BEGIN/\ref CD_APPLET_ON_DOUBLE_CLICK_END for the actions on double click on your icon or one of its sub-dock.
 * - \ref CD_APPLET_ON_SCROLL_BEGIN/\ref CD_APPLET_ON_SCROLL_END for the actions on scroll on your icon or one of its sub-dock.
 * - \ref CD_APPLET_ON_BUILD_MENU_BEGIN/\ref CD_APPLET_ON_BUILD_MENU_END for the building of the menu on left click on your icon or one of its sub-dock.
 * 
 * To register to an event, use one of the following convenient macro during the init:
 * - \ref CD_APPLET_REGISTER_FOR_CLICK_EVENT
 * - \ref CD_APPLET_REGISTER_FOR_MIDDLE_CLICK_EVENT
 * - \ref CD_APPLET_REGISTER_FOR_DOUBLE_CLICK_EVENT
 * - \ref CD_APPLET_REGISTER_FOR_SCROLL_EVENT
 * - \ref CD_APPLET_REGISTER_FOR_BUILD_MENU_EVENT
 *
 * Note: don't forget to unregister during the stop.
 * 
 * 
 * \subsection further Go further
 * 
 * A lot of useful macros are provided in cairo-dock-applet-facility.h to make your life easier.
 * 
 * The applet instance is <b>myApplet</b>, and it holds the following:
 * - <b>myIcon</b> : this is your icon !
 * - <b>myContainer</b> : the container your icon belongs to (a Dock or a Desklet). For convenience, the following 2 parameters are available.
 * - <b>myDock</b> : if your container is a dock, myDock = myContainer, otherwise it is NULL.
 * - <b>myDesklet</b> : if your container is a desklet, myDesklet = myContainer, otherwise it is NULL.
 * - <b>myConfig</b> : the structure holding all the parameters you get in your config file. You have to define it in <i>applet-struct.h</i>.
 * - <b>myData</b> : the structure holding all the ressources loaded at run-time. You have to define it in <i>applet-struct.h</i>.
 * - <b>myDrawContext</b> : a cairo context, if you need to draw on the icon with the libcairo.
 * 
 * - To get values contained inside your <b>conf file</b>, you can use the following :\n
 * \ref CD_CONFIG_GET_BOOLEAN & cie
 * 
 * - To <b>build your menu</b>, you can use the following :\n
 * \ref CD_APPLET_ADD_SUB_MENU & cie
 * 
 * - To directly <b>set an image on your icon</b>, you can use the following :\n
 * \ref CD_APPLET_SET_IMAGE_ON_MY_ICON & cie
 * 
 * - To modify the <b>label</b> of your icon, you can use the following :\n
 * \ref CD_APPLET_SET_NAME_FOR_MY_ICON & cie
 * 
 * - To set a <b>quick-info</b> on your icon, you can use the following :\n
 * \ref CD_APPLET_SET_QUICK_INFO_ON_MY_ICON & cie
 * 
 * - To <b>create a surface</b> that fits your icon from an image, you can use the following :\n
 * \ref CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET & cie
 * 
 * - To trigger the <b>refresh</b> of your icon or container after you drew something, you can use the following :\n
 * \ref CD_APPLET_REDRAW_MY_ICON & \ref CAIRO_DOCK_REDRAW_MY_CONTAINER
 * 
 *
 * \subsection opengl How can I take advantage of the OpenGL ?
 * 
 * There are 3 cases :
 * - your applet just has a static icon; there is nothing to take into account, the common functions to set an image or a surface on an icon already handle the texture mapping.
 * - you draw dynamically on your icon with libcairo (using myDrawContext), but you don't want to bother with OpenGL; all you have to do is to call /ref cairo_dock_update_icon_texture to update your icon's texture after you drawn your surface. This can be done for occasional drawings, like Switcher redrawing its icon each time a window is moved.
 * - you draw your icon differently whether the dock is in OpenGL mode or not; in this case, you just need to put all the OpenGL commands into a CD_APPLET_START_DRAWING_MY_ICON/CD_APPLET_FINISH_DRAWING_MY_ICON section inside your code. If your applet relies on OpenGL for its core function, such that it does not make sense to use it without (e.g. it adds OpenGL effects to the dock or icons etc.), add CAIRO_DOCK_MODULE_REQUIRES_OPENGL to the flags (second argument) in CD_APPLET_DEFINITION2. This will result in the applet not loaded at all if OpenGL is not available.
 * 
 * There are also a lot of convenient functions you can use to draw in OpenGL. See cairo-dock-draw-opengl.h for loading and drawing textures and paths, and cairo-dock-particle-system.h for an easy way to draw particle systems.
 * 
 * 
 * \subsection animation How can I animate my applet to make it more lively ?
 *
 * If you want to animate your icon easily, to signal some action (like <i>Music-Player</i> when a new song starts), you can simply <b>request for one of the registered animations</b> with \ref CD_APPLET_ANIMATE_MY_ICON and stop it with \ref CD_APPLET_STOP_ANIMATING_MY_ICON. You just need to specify the name of the animation (like "rotate" or "pulse") and the number of time it will be played.
 * 
 * But you can also make your own animation, like <i>Clock</i> of <i>Cairo-Penguin</i>. You will have to integrate yourself into the rendering loop of your container. Don't panic, here again, Cairo-Dock helps you !
 * 
 * First you will register to the "update container" notification, with a simple call to \ref CD_APPLET_REGISTER_FOR_UPDATE_ICON_SLOW_EVENT or \ref CD_APPLET_REGISTER_FOR_UPDATE_ICON_EVENT, depending on the refresh frequency you need : ~10Hz or ~33Hz. A high frequency needs of course more CPU, and most of the time the slow frequancy is enough.
 * 
 * Then you will just put all your code in a \ref CD_APPLET_ON_UPDATE_ICON_BEGIN/\ref CD_APPLET_ON_UPDATE_ICON_END section. That's all ! In this section, do what you want, like redrawing your icon, possibly incrementing a counter to know until where you went, etc. See \ref opengl "the previous paragraph" to draw on your icon.
 * Inside the rendering loop, you can skip an iteration with \ref CD_APPLET_SKIP_UPDATE_ICON, and quit the loop with \ref CD_APPLET_STOP_UPDATE_ICON or \ref CD_APPLET_PAUSE_UPDATE_ICON (don't forget to quit the loop when you're done, otherwise your container may continue to redraw itself, which means a needless CPU load).
 * 
 * To know the size allocated to your icon, use the convenient \ref CD_APPLET_GET_MY_ICON_EXTENT.
 *
 *
 * \subsection tasks I have heavy treatments to do, how can I make them without slowing the dock ?
 * 
 * Say for instance you want to download a file on the Net, it is likely to take some amount of time, during which the dock will be frozen, waiting for you. To avoid such a situation, Cairo-Dock defines \ref _GldiTask "Tasks". They perform their job <b>asynchronously</b>, and can be <b>periodic</b>. See cairo-dock-task.h for a quick explanation on how a Task works.
 * 
 * You create a Task with \ref cairo_dock_new_task, launch it with \ref cairo_dock_launch_task, and either cancel it with \ref cairo_dock_discard_task or destroy it with \ref cairo_dock_free_task.
 * 
 * 
 * \subsection key_binding Key binding
 * 
 * You can bind an action to a shortkey with the following macro: \ref CD_APPLET_BIND_KEY.\n
 * For instance, the GMenu applet displays the menu on ctrl+F1.\n
 * You get a GldiShortkey that you simply destroy when the applet stops (with \ref gldi_object_unref).
 * 
 * See cairo-dock-keybinder.h for more details.
 * 
 * 
 * \subsection sub_icons I need more than one icon, how can I easily get more ?
 * 
 * In dock mode, your icon can have a sub-dock; in desklet mode, you can load a list of icons into your desklet. Cairo-Dock provides a convenient macro to <b>quickly load a list of icons</b> in both cases : \ref CD_APPLET_LOAD_MY_ICONS_LIST to load a list of icons and \ref CD_APPLET_DELETE_MY_ICONS_LIST to destroy it. Thus you don't need to know in which mode you are, neither to care about loading the icons, freeing them, or anything.
 * 
 * You can get the list of icons with \ref CD_APPLET_MY_ICONS_LIST and to their container with \ref CD_APPLET_MY_ICONS_LIST_CONTAINER.
 * 
 * 
 * \subsection translations How do I provide translations (internationalization) for my applet ?
 * 
 * Cairo-Dock uses the standard gettext library to provide translated strings in its user interface. Specifically, the dgettext() function is used when creating menus, the configuration interface, etc. For plugins that are part of the official plugins package, the "cairo-dock-plugins" message domain is used and this is set by default when loading a plugin. For external plugins, it is recommended to define your own message domain and install translations accordingly (after compiling them with msgfmt). Within the plugin code, use dgettext() when displaying text to the user. To ensure that translation of configuration options work, you should provide the message domain also when your plugin is loaded. This is achieved by using the CD_APPLET_DEFINE2_BEGIN macro (see \ref system_applet) and setting pVisitCard->cGettextDomain to the message domain of your plugin. You should also call the bindtextdomain() function at this point to supply where translations are installed.
 * 
 * 
 * \n
 * \section advanced_sec Advanced functionnalities
 * 
 * \subsection advanced_config How can I make my own widgets in the config panel ?
 * 
 * Cairo-Dock can build itself the config panel of your applet from the config file. Moreover, it can do the opposite : update the conf file from the config panel. However, it is limited to the widgets it knows, and there are some cases it is not enough.
 * Because of that, Cairo-Dock offers 2 hooks in the process of building/reading the config panel : 
 * when defining your applet in the CD_APPLET_DEFINE2_BEGIN/CD_APPLET_DEFINE2_END section (see below), add to the interface the 2 functions pInterface->load_custom_widget and pInterface->save_custom_widget.
 * They will be respectively called when the config panel of your applet is raised, and when it is validated.
 * 
 * If you want to modify the content of an existing widget, you can grab it with \ref cairo_dock_gui_find_group_key_widget_in_list.
 * To add your custom widgets, insert in the conf file an empty widget (with the prefix '_'), then grab it and pack some GtkWidget inside.
 * If you want to dynamically alter the config panel (like having a "new" button that would make appear new widgets on click), you can add in the conf file the new widgets, and then call \ref cairo_dock_reload_current_module_widget to reload the config panel.
 * See the AlsaMixer or Weather applets for an easy example, and Clock or Mail for a more advanced example.
 *
 * 
 * \subsection steal_appli How can my applet control the window of an application ?
 * 
 * Say your applet launches an external application that has its own window. It is logical to <b>make your applet control this application</b>, rather than letting the Taskbar do.
 * All you need to do is to call the macro \ref CD_APPLET_MANAGE_APPLICATION, indicating which application you wish to manage (you need to enter the class of the application, as you can get from "xprop | grep CLASS"). Your applet will then behave like a launcher that has stolen the appli icon.
 * 
 * 
 * \subsection data_renderer How can I render some numerical values on my icon ?
 * 
 * Cairo-Dock offers a powerful and versatile architecture for this case : \ref _CairoDataRenderer.
 * A DataRenderer is a generic way to render a set of values on an icon; there are several implementations of this class : Gauge, CairoDockGraph, Bar, and it is quite easy to implement a new kind of DataRenderer.
 * 
 * Each kind of renderer has a set of attributes that you can use to customize it; you just need to call the \ref CD_APPLET_ADD_DATA_RENDERER_ON_MY_ICON macro with the attributes, and you're done !
 * Then, each time you want to render some new values, simply call \ref CD_APPLET_RENDER_NEW_DATA_ON_MY_ICON with the new values.
 * 
 * When your applet is reloaded, you have to reload the DataRenderer as well, using the convenient \ref CD_APPLET_RELOAD_MY_DATA_RENDERER macro. If you don't specify attributes to it, it will simply reload the current DataRenderer, otherwise it will load the new attributes; the previous data are not lost, which is useful in the case of Graph for instance.
 * 
 * You can remove it at any time with \ref CD_APPLET_REMOVE_MY_DATA_RENDERER.
 * 
 * 
 * \subsection multi How can I make my applet multi-instanciable ?
 *
 * Applets can be launched several times, an instance will be created each time. To ensure your applet can be instanciated several times, you just need to pass myApplet to any function that uses one of its fields (myData, myIcon, etc). Then, to indicate Cairo-Dock that your applet is multi-instanciable, you'll have to define the macro CD_APPLET_MULTI_INSTANCE in each file. A convenient way to do that is to define it in the CMakeLists.txt by adding the following line: \code add_definitions (-DCD_APPLET_MULTI_INSTANCE="1") \endcode.
 * 
 * 
 * \subsection render_container How can I draw anywhere on the dock, not only on my icon ?
 * 
 * Say you want to draw directly on your container, like <i>CairoPenguin</i> or <i>ShowMouse</i> do. This can be achieved easily by registering to the \ref NOTIFICATION_RENDER notification. You will then be notified eash time a Dock or a Desklet is drawn. Register AFTER so that you will draw after the view.
 * 
 * 
 * \subsection system_applet Applets with advanced initialization steps or providing core functionality
 * 
 * Some applets need more control over their life-cycle. Beyond applets that define their own message domain for internationalization (see \ref translations), this typically relates to applets that provide some essential functionality for the dock (e.g. renderers, desktop environment integration, etc.). In this case, instead of defining the applet with CD_APPLET_DEFINITION2, use the macro pair CD_APPLET_DEFINE2_BEGIN / CD_APPLET_DEFINE2_END and
 * - Add early initialization steps between these; this will be run when the applet is first opened (loaded from disk), regardless wether it is enabled yet. Be careful that the applet's instance variable (myApplet) and configuration (myConfig) are not available at this point (so you will need to use static variables to save any state). Also, no docks, cairo or OpenGL contexts exist, and the current theme has not been loaded at this point as well. It is thus recommended to keep things here absolute minimal.
 * - Also you have to manually define the applet's interface here (load, stop, config functions, etc.); see the GldiModuleInterface struct from cairo-dock-module-manager.h for a description of each function and what it does, or use the CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE macro that defines these with the usual function names thus you can use the same macros as you would with CD_APPLET_DEFINITION2.
 * 
 * 
 * \subsection auto_load Auto-loaded applets
 * 
 * A special class of applets is considered "auto-loaded": this means that the applet is always enabled and cannot be disabled by the user. Note that the init() and stop() functions are still called as normal whenever Cairo-Dock reloads its full configuration (i.e. when changing the current theme). However, there are slight differences:
 * - The configuration of auto-loaded applets is read (CD_APPLET_GET_CONFIG_BEGIN / read_conf_file ()) earlier than for normal applets. Specifically, this happens before creating any of the docks, but after Cairo-Dock's core configuration has been loaded. This means that such applets should not rely on the existence of any docks, or their drawing context when reading their configuration (this is not recommended anyway for normal applets). On the other hand, such applets can provide parameters when creating docks (e.g. dock-rendering is an example of this).
 * - Initialization of such applets (CD_APPLET_INIT_BEGIN / initModule ()) happens earlier than for normal applets. Specifically, this is done after the main dock has been created and core functionality has started, but before adding any launchers (normal applets' init happens after loading launchers).
 * - Auto-loaded applets are stopped after regular ones.
 * 
 * An applet is defined as auto-loaded if it's type is CAIRO_DOCK_MODULE_IS_PLUGIN and at least one of the following is true:
 * - it does not have an initModule () function
 * - it does not have a stopModule () function
 * - it extends a core functionality ("manager") of Cairo-Dock: this is achieved by using the CD_APPLET_EXTEND_MANAGER macro when the module is read
 * 
 * Note: all of the above need to be defined by using CD_APPLET_DEFINE2_BEGIN / CD_APPLET_DEFINE2_END
 * 
 */

typedef struct _CairoDockRenderer CairoDockRenderer;
typedef struct _CairoDeskletRenderer CairoDeskletRenderer;
typedef struct _CairoDeskletDecoration CairoDeskletDecoration;
typedef struct _CairoDialogRenderer CairoDialogRenderer;
typedef struct _CairoDialogDecorator CairoDialogDecorator;

typedef struct _IconInterface IconInterface;
typedef struct _Icon Icon;
typedef struct _GldiAppInfo GldiAppInfo;
typedef struct _GldiContainer GldiContainer;
typedef struct _GldiContainerInterface GldiContainerInterface;
typedef struct _CairoDock CairoDock;
typedef struct _CairoDesklet CairoDesklet;
typedef struct _CairoDialog CairoDialog;
typedef struct _CairoFlyingContainer CairoFlyingContainer;

typedef struct _GldiModule GldiModule;
typedef struct _GldiModuleInterface GldiModuleInterface;
typedef struct _GldiVisitCard GldiVisitCard;
typedef struct _GldiModuleInstance GldiModuleInstance;
typedef struct _CairoDockMinimalAppletConfig CairoDockMinimalAppletConfig;
typedef struct _CairoDockDesktopEnvBackend CairoDockDesktopEnvBackend;
typedef struct _CairoDialogAttribute CairoDialogAttribute;
typedef struct _CairoDeskletAttribute CairoDeskletAttribute;
typedef struct _CairoDialogButton CairoDialogButton;

typedef struct _CairoDataRenderer CairoDataRenderer;
typedef struct _CairoDataRendererAttribute CairoDataRendererAttribute;
typedef struct _CairoDataRendererInterface CairoDataRendererInterface;
typedef struct _CairoDataToRenderer CairoDataToRenderer;
typedef struct _CairoDataRendererEmblemParam CairoDataRendererEmblemParam;
typedef struct _CairoDataRendererEmblem CairoDataRendererEmblem;
typedef struct _CairoDataRendererTextParam CairoDataRendererTextParam;
typedef struct _CairoDataRendererText CairoDataRendererText;
typedef struct _CairoDockDataRendererRecord CairoDockDataRendererRecord;

typedef struct _CairoDockAnimationRecord CairoDockAnimationRecord;

typedef struct _CairoDockHidingEffect CairoDockHidingEffect;

typedef struct _CairoIconContainerRenderer CairoIconContainerRenderer;

typedef struct _CairoDockTransition CairoDockTransition;

typedef struct _CairoDockPackage CairoDockPackage;

typedef struct _CairoDockGroupKeyWidget CairoDockGroupKeyWidget;

typedef struct _CairoDockGLConfig CairoDockGLConfig;

typedef struct _CairoDockGLFont CairoDockGLFont;

typedef struct _CairoDockGLPath CairoDockGLPath;

typedef struct _CairoDockImageBuffer CairoDockImageBuffer;

typedef struct _CairoOverlay CairoOverlay;

typedef struct _GldiTask GldiTask;

typedef struct _GldiShortkey GldiShortkey;

typedef struct _GldiManager GldiManager;

typedef struct _GldiObject GldiObject;

typedef struct _GldiObjectManager GldiObjectManager;

typedef struct _GldiDesktopGeometry GldiDesktopGeometry;

typedef struct _GldiDesktopBackground GldiDesktopBackground;

typedef struct _GldiDesktopManagerBackend GldiDesktopManagerBackend;

typedef struct _GldiWindowManagerBackend GldiWindowManagerBackend;

typedef struct _GldiWindowActor GldiWindowActor;

typedef struct _GldiContainerManagerBackend GldiContainerManagerBackend;

typedef struct _GldiGLManagerBackend GldiGLManagerBackend;

typedef void (*_GldiIconFunc) (Icon *icon, gpointer data);
typedef _GldiIconFunc GldiIconFunc;
typedef gboolean (*_GldiIconRFunc) (Icon *icon, gpointer data);  // TRUE to continue
typedef _GldiIconRFunc GldiIconRFunc;

typedef struct _GldiTextDescription GldiTextDescription;
typedef struct _GldiColor GldiColor;


#define CAIRO_DOCK_NB_DATA_SLOT 12

// Nom du fichier de conf principal du theme.
#define CAIRO_DOCK_CONF_FILE "cairo-dock.conf"
// Nom du fichier de conf d'un dock racine.
#define CAIRO_DOCK_MAIN_DOCK_CONF_FILE "main-dock.conf"

// Nom du dock principal (le 1er cree).
#define CAIRO_DOCK_MAIN_DOCK_NAME "_MainDock_"
// Nom de la vue par defaut.
#define CAIRO_DOCK_DEFAULT_RENDERER_NAME N_("Default")


#define CAIRO_DOCK_LAST_ORDER -1e9
#define CAIRO_DOCK_NB_MAX_ITERATIONS 1000

#endif
