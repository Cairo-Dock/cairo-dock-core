/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/

#ifndef __CAIRO_DOCK_STRUCT__
#define  __CAIRO_DOCK_STRUCT__

#include <X11/Xlib.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>
#include <glib/gi18n.h>
//#include <X11/extensions/Xdamage.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include <GL/gl.h>
#include <GL/glx.h>

/*! \mainpage Cairo-Dock's API documentation.
 * \ref intro_sec
 * 
 * \ref install_sec
 * 
 * \ref struct_sec
 * -  \ref containers
 * -  \ref icons
 * -  \ref dock
 * -  \ref desklet
 * -  \ref dialog
 * -  \ref flying
 * 
 * \ref applets_sec
 * -  \ref module
 * -  \ref generate
 * -  \ref definition
 * -  \ref sections
 * -  \ref notifications
 * -  \ref opengl
 * -  \ref animation
 * -  \ref tasks
 * -  \ref sub-icons
 * 
 * \ref advanced_sec
 * -  \ref advanced_config
 * -  \ref steal_appli
 * -  \ref data_renderer
 * -  \ref render_container
 * -  \ref multi
 * 
 * 
 * 
 * 
 * \n
 * \section intro_sec Introduction
 *
 * Cairo-Dock's API can be divided into 3 parts :
 * - the definition of the main classes (dock, icon, etc)
 * - utilities functions (interaction with X, GUI, etc)
 * - plug-ins framework.
 * 
 * Each class is defined in its own header. When a class is complex, it is divided into several files :
 * - "factory" define structures/eunms and creation/modification/destruction functions
 * - "manager" manage all the ressources needed by a class and all the instances of a class
 * - "utilities" are a collection of helper functions.
 *
 * Cairo-Dock has a <b>decentralized conception</b> : it has a minimalistic core, and lets external modules extend its functionnalities.\n
 * This is a strong design, because it allows to extend functionnalities easily without having to hack into the core, which makes the project more stable and allows developpers to use high-level functions only, that are very tested and optimized.\n
 * Thus, Cairo-Dock itself has no animation, but has a convenient notification system that allows external plug-ins to animate icons when they want.\n
 * We will describe this system and the plug-ins framework in this document.
 * 
 * \n
 * \section install_sec Installation
 *
 * The installation is very easy. In a terminal, copy-paste the following commands :
 * \code
 *   ### grab the sources
 *   svn co http://svn.berlios.de/svnroot/repos/cairo-dock/trunk CD
 *   cd CD/cairo-dock
 *   ### compil the dock and install it
 *   autoreconf -isf && ./configure --prefix=/usr && make
 *   sudo make install
 *   ### compil the stable plug-ins and install them
 *   cd ../plug-ins
 *   autoreconf -isf && ./configure --prefix=/usr && make
 *   sudo make install
 * \endcode
 * You can compil and install any plug-in individually by using the same commands in its own folder.
 * 
 * 
 * \n
 * \section struct_sec Main structures
 *
 * \subsection containers Containers
 * See \ref _CairoContainer for the definition of a Container, and \ref cairo-dock-container.h for a complete description of the Container class.
 * 
 * \subsection icons Icons
 * See \ref _Icon for the definition of an Icon, and \ref cairo-dock-icons.h for a complete description of the Icon class.
 * 
 * \subsection dock Dock
 * See \ref _CairoDock for the definition of a Dock, and \ref cairo-dock-dock-factory.h for a complete description of the Icon class.
 * 
 * \subsection desklet Desklet
 * See \ref _CairoDesklet for the definition of a Desklet, and \ref cairo-dock-desklet.h for a complete description of the Icon class.
 * 
 * \subsection dialog Dialog
 * See \ref _CairoDialog for the definition of a Dialog, and \ref cairo-dock-dialogs.h for a complete description of the Icon class.
 * 
 * \subsection flying Flying Container
 * See \ref _CairoFlyingContainer for the definition of a Flying Container, and \ref cairo-dock-flying-container.h for a complete description of the Icon class.
 * 
 * 
 * 
 * \n
 * \section applets_sec External Modules
 * 
 * \subsection module First, what is a module ?
 * 
 * Modules are compiled .so files (that is to say, library) that are plugged into the dock at run-time.
 * Due to this fact, they can use any function used by the dock, and have a total interaction freedom on the dock.
 * The advantage is that applets can do anything, in fact they are extensions of the dock itself.
 * The drawback is that a buggy applet can make the dock unstable.
 * 
 * A module has an <b>interface</b> and a <b>visit card</b> :
 *  - the visit card allows it to define itself (name, category, default icon, etc)
 *  - the interface defines the entry points for init, stop, reload, read config, and reset datas.
 * 
 * Modules can be instanciated several times; each time they are, an <b>instance</b> is created. This instance will hold all the data used by the module's functions : the icon and its container, the config structure and its conf file, the data structure and a slot to plug datas into containers and icons. All these parameters are optionnal; a module that has an icon is also called an <b>applet</b>.
 * 
 * When instanciating a module, CD will check the presence of an "Icon" group in the conf file. If there is one, it will create an icon accordingly and insert it into its container. If there is a "Desklet" group, the module is considered as detachable, and can be placed into a desklet.
 * Here we will focus on applets, that is to say, we will have an icon and a container (dock or desklet).
 * 
 * 
 * \subsection generate Let's start, how do I create an empty applet ?
 * 
 * Easy ! just go to the "plug-ins" folder, and run the <i>generate-applet.sh</i> script. Answer the few questions, and you're done ! Don't forget to install the plug-in each time you modify it (<i>sudo make install</i> in your applet's folder).
 * You can see that the script has created for you the architecture of your applet :
 * - in the <b>root</b> folder, you have the "configure.ac", where you can set the version number of your applet, the dependencies, etc
 * - in the <b>src</b> folder, you have the sources of your applet. It is common to put the init/stop/reload in applet-init.c, the get/config/reset_config/reset_data in applet-config.c, the notifications in applet-notifications.c, and the structures in applet-struct.h. Of course, you can add as many files as you want, just don't forget to specify them in the Makefile.am
 * - in the <b>po</b> folder, you have the translation files. Currently the dock is widely translated into French, Japanese, and Italian.
 * - in the <b>data</b> folder, you have the config file, the default icon, and a preview. You will have to choose a default icon that fits your applet, and make a preview that makes users want to try it ;-)
 * If you have other files to install, it's here you will do it.
 * If you change the name of the default icon (for instance you use an SVG file), don't forget to modify the data/Makefile.am and also the src/Makefile.am.
 * 
 * 
 * \subsection definition Ok I have a generic applet, how do I define it ?
 * 
 * As we saw, a module must fill a visit card and an interface, to be acecpted by the dock.
 * This is done very easily by the \ref CD_APPLET_DEFINITION macro. All you have to give is the name of the applet, its category, a brief description/manual (very important !), and your name.
 * When you will have finished, you will be able to make a nice preview and a nice default icon, and place them in the <i>data</i> folder.
 * 
 * 
 * \subsection sections Great, I can see my applet in the dock ! Now, where should I continue ?
 * 
 * We saw that when our applet is activated, an instance is created. It is called <b>myApplet</b>, and it will hold the following :
 * - <b>myIcon</b> : this is your icon ! It will act as a drawing surface to represent whatever you want.
 * - <b>myDrawContext</b> : a cairo context, to draw on your icon with the libcairo.
 * - <b>myContainer</b> : the container your icon belongs. For convenience, the following 2 parameters are availale.
 * - <b>myDock</b> : if your container is a dock, myDock = myContainer, otherwise it is NULL.
 * - <b>myDesklet</b> : if your container is a desklet, myDesklet = myContainer, otherwise it is NULL.
 * - <b>myConfig</b> : the structure holding all the parameters you get in your conf file. You have to define it in applet-struct.h.
 * - <b>myData</b> : the structure holding all the ressources loaded at run-time. You have to define it in applet-struct.h.
 * 
 * The framework defines different <b>sections</b>, and all you have to do is to fill them :
 * 
 * - First of all you will have to get your config parameters. This is done in the <b>\ref CD_APPLET_GET_CONFIG_BEGIN/\ref CD_APPLET_GET_CONFIG_END</b> section, in applet-config.c.
 * - Each time you add a parameter, think of freeing it if it's a dynamic ressource like a string; this is done in the <b>CD_APPLET_RESET_CONFIG_BEGIN/CD_APPLET_RESET_CONFIG_END</b> section.
 * - In a similar way, you will free all the ressources you allocated by myData in the <b>CD_APPLET_RESET_DATA_BEGIN/CD_APPLET_RESET_DATA_END</b> section.
 * - After the instance is created, the dock lets you start. This is done in the CD_APPLET_INIT_BEGIN/CD_APPLET_INIT_END section. At this point, myApplet is already fully defined, and myConfig has been filled. Therefore you can already draw on your icon, launch timers, register to notifications, etc.
 * - Each time the user changes something in its config, or the desklet is resized, your applet is reloaded. This is done in the <b>CD_APPLET_RELOAD_BEGIN/CD_APPLET_RELOAD_END</b> section. The macro <b>CD_APPLET_MY_CONFIG_CHANGED</b> tells you if something has changed in your config or if it's just a resizing.
 * - Last, when your applet is stopped, you have to stop everything you set up in the init (timers, notifications, etc).
 * 
 * 
 * \subsection notifications The notifications system.
 * 
 * When something happens, Cairo-Dock notifies everybody about it, including itself. An applet can register to any notification (see \ref cairo-dock-notifications.h) before or after the dock, to be notified of the event of its choice. When you are notified, the function you registered for this event will be called; it must match the notification prototype as defined in \ref cairo-dock-notifications.h.
 *
 * For instance if you want to know when the user click on your icon, you will register to the "CAIRO_DOCK_CLICK_ICON" notification.
 *
 * To register to a notification, you have the \ref cairo_dock_register_notification function. Always unregister when your applet is stopped, to avoid being notified when you shouldn't, with the function \ref cairo_dock_remove_notification_func.
 * 
 * For convenience, there are sections dedicated to the most common events; you just have to fill the corresponding sections :
 * - \ref CD_APPLET_ON_CLICK_BEGIN/\ref CD_APPLET_ON_CLICK_END for the actions on right click on your icon or one of its sub-dock.
 * - \ref CD_APPLET_ON_MIDDLE_CLICK_BEGIN/\ref CD_APPLET_ON_MIDDLE_CLICK_END for the actions on middle click on your icon or one of its sub-dock.
 * - \ref CD_APPLET_ON_DOUBLE_CLICK_BEGIN/\ref CD_APPLET_ON_DOUBLE_CLICK_END for the actions on double click on your icon or one of its sub-dock.
 * - \ref CD_APPLET_ON_SCROLL_BEGIN/\ref CD_APPLET_ON_SCROLL_END for the actions on scroll on your icon or one of its sub-dock.
 * - \ref CD_APPLET_ON_BUILD_MENU_BEGIN/\ref CD_APPLET_ON_BUILD_MENU_END for the building of the menu on left click on your icon or one of its sub-dock.
 * 
 * To register to these notifications, you can use the convenient macros :
 * - \ref CD_APPLET_REGISTER_FOR_CLICK_EVENT
 * - \ref CD_APPLET_REGISTER_FOR_MIDDLE_CLICK_EVENT
 * - \ref CD_APPLET_REGISTER_FOR_DOUBLE_CLICK_EVENT
 * - \ref CD_APPLET_REGISTER_FOR_SCROLL_EVENT
 * - \ref CD_APPLET_REGISTER_FOR_BUILD_MENU_EVENT
 *
 *
 * \subsection opengl How can I take advantage of the OpenGL ?
 * 
 * There are 3 cases :
 * - your applet just has a static icon; there is nothing to take into account, the common functions to set an image or a surface on an icon already handle the texture mapping.
 * - you draw dynamically on your icon with libcairo (using myDrawContext), but you don't want to bother with OpenGL; all you have to do is to call \ref cairo_dock_update_icon_texture to update your icon's texture after you drawn your surface. This can be done for occasional drawings, like Switcher redrawing its icon each time a window is moved.
 * - you draw your icon differently whether the dock is in OpenGL mode or not; in this case, you just need to put all the OpenGL commands into a \ref CD_APPLET_START_DRAWING_MY_ICON/\ref CD_APPLET_FINISH_DRAWING_MY_ICON section.
 * 
 * 
 * \subsection animation How can I animate my applet to make it more lively ?
 *
 * If you want to animate your icon easily, to signal some action (like Music-Player when a new song starts), you can simply <b>request for one of the registered animations</b> with \ref cairo_dock_request_icon_animation and stop it with \ref cairo_dock_stop_icon_animation. You just specify the name of the animation (like "rotate") and the number of time it will be played.
 * 
 * But you can also make your own animation, like Clock of Cairo-Penguin. You will have to integrate yourself into the rendering loop of your container. Don't panic, here again, Cairo-Dock helps you !
 * 
 * First you will register to the "update container" notification, with the macros \ref CD_APPLET_REGISTER_FOR_UPDATE_ICON_SLOW_EVENT or CD_APPLET_REGISTER_FOR_UPDATE_ICON_EVENT, depending on the refresh frequency you need : ~10Hz or ~33Hz. A high frequency needs of course more CPU, and most of the time the slow frequancy is enough.
 * 
 * Then you will just put all your code in a \ref CD_APPLET_ON_UPDATE_ICON_BEGIN/\ref CD_APPLET_ON_UPDATE_ICON_END section. That's all ! In this section, do what you want, like redrawing your icon, possibly incrementing a counter to know until where you went, etc. See \ref opengl "the previous paragraph" to draw on your icon.
 * Inside the rendering loop, you can skip an iteration with \ref CD_APPLET_SKIP_UPDATE_ICON, and quit the loop with \ref CD_APPLET_STOP_UPDATE_ICON or \ref CD_APPLET_PAUSE_UPDATE_ICON (don't forget to quit the loop when you're done, otherwise your container may continue to redraw itself, which means a needless CPU load).
 *
 *
 * \subsection tasks I have heavy treatments to do, how can I make them without slowing the dock ?
 * 
 * When for instance you want to download a file on the Net, it is likely to take some amount of time, during which the dock will be frozen waiting for you. To avoid such situation, Cairo-Dock defines \ref _CairoTask Tasks. They are <b>asynchronous</b> and <b>periodic</b>. A Task is divided in 2 phases : 
 * - the asynchronous phase will be executed in another thread, while the dock continues to run on its own thread, in parallel. During this phase you will do all the heavy job (like downloading a file or computing something) but you can't interact on the dock.
 * - the synchronous phase will be executed after the first one has finished. There you will update your applet with the result of the first phase.
 * 
 * \attention A data buffer is used to communicate between the 2 phases. It is important that these datas are never accessed outside of the task, and vice versa that the asynchronous thread never access other data than these ones.\n
 * If you want to access these datas outside of the task, you have to copy them in a safe place during the 2nd phase, or to stop the task before (beware that stopping the task means waiting for the 1st phase to finish, which can take some time).
 * 
 * You create a Task with \ref cairo_dock_new_task, launch it with \ref cairo_dock_launch_task, and destroy it with \ref cairo_dock_free_task.
 * A Task can be periodic if you specify a period, and can also be fully synchronous if you don't specify an asynchronous function.
 * 
 * 
 * \subsection sub-icons I need more than one icon, how can I easily get more ?
 * 
 * In dock mode, your icon can have a sub-dock; in desklet mode, you can load a list of icons into your desklet. Cairo-Dock provides a convenient macro to <b>quickly load a list of icons</b> in both cases : \ref CD_APPLET_LOAD_MY_ICONS_LIST to load a list of icons and \ref CD_APPLET_DELETE_MY_ICONS_LIST to destroy it. Thus you don't need to know in which mode you are, neither to care about loading the icons, freeing them, or anything.
 * 
 * You can access to the list of icons with \ref CD_APPLET_MY_ICONS_LIST and to their container with \ref CD_APPLET_MY_ICONS_LIST_CONTAINER.
 * 
 * 
 * \n
 * \section advanced_sec Advanced functionnalities
 * 
 * \subsection advanced_config How can I make my own widgets in the config panel ?
 * 
 * Cairo-Dock can build itself the config panel of your applet from the config file. Moreover, it can do the opposite : update the conf file from the config panel. However, it is limited to the widgets it knows, and there are some cases it is not enough.
 * Because of that, Cairo-Dock offers 2 hooks in the process of building/reading the config panel : 
when defining your applet in the \ref CD_APPLET_DEFINE_BEGIN/\ref CD_APPLET_DEFINE_END section, add to the interface the 2 functions pInterface->load_custom_widget and pInterface->save_custom_widget.
 * 
 *
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
 * A DataRenderer is a generic way to render a set of values on an icon; there are several implementations of this class : Gauge, Graph, Bar, and it is quite easy to implement a new kind of DataRenderer.
 * 
 * Each kind of renderer has a set of attributes that you can use to customize it; you just need to call the \ref CD_APPLET_ADD_DATA_RENDERER_ON_MY_ICON macro with the attributes, and you're done !
 * Then, each time you want to render some new values, simply call \ref CD_APPLET_RENDER_NEW_DATA_ON_MY_ICON with the new values.
 * When your applet is reloaded, you have to reload the DataRenderer as well, using the convenient \ref CD_APPLET_RELOAD_MY_DATA_RENDERER macro. If you don't specify attributes to it, it will simply reload the current DataRenderer, otherwise it will load the new attributes; the previous data are not lost, which is useful in the case of Graph for instance.
 * You can remove it at any time with \ref CD_APPLET_REMOVE_MY_DATA_RENDERER.
 * 
 * 
 * \subsection multi How can I make my applet multi-instanciable ?
 * 
 * 
 * 
 * \subsection render_container How can I draw anywhere on the dock, not only on my icon ?
 * 
 * 
 * 
 * 
 * to be continued ...
 */




typedef struct _CairoDockRenderer CairoDockRenderer;
typedef struct _CairoDeskletRenderer CairoDeskletRenderer;
typedef struct _CairoDeskletDecoration CairoDeskletDecoration;
typedef struct _CairoDialogRenderer CairoDialogRenderer;
typedef struct _CairoDialogDecorator CairoDialogDecorator;

typedef struct _Icon Icon;
typedef struct _CairoContainer CairoContainer;
typedef struct _CairoDock CairoDock;
typedef struct _CairoDesklet CairoDesklet;
typedef struct _CairoDialog CairoDialog;
typedef struct _CairoFlyingContainer CairoFlyingContainer;

typedef struct _CairoDockModule CairoDockModule;
typedef struct _CairoDockModuleInterface CairoDockModuleInterface;
typedef struct _CairoDockModuleInstance CairoDockModuleInstance;
typedef struct _CairoDockVisitCard CairoDockVisitCard;
typedef struct _CairoDockInternalModule CairoDockInternalModule;
typedef struct _CairoDockMinimalAppletConfig CairoDockMinimalAppletConfig;
typedef struct _CairoDockDesktopEnvBackend CairoDockDesktopEnvBackend;
typedef struct _CairoDockClassAppli CairoDockClassAppli;
typedef struct _CairoDockLabelDescription CairoDockLabelDescription;
typedef struct _CairoDialogAttribute CairoDialogAttribute;
typedef struct _CairoDeskletAttribute CairoDeskletAttribute;
typedef struct _CairoDialogButton CairoDialogButton;

typedef struct _CairoDataRenderer CairoDataRenderer;
typedef struct _CairoDataRendererAttribute CairoDataRendererAttribute;
typedef struct _CairoDataRendererInterface CairoDataRendererInterface;
typedef struct _CairoDataToRenderer CairoDataToRenderer;

typedef struct _CairoDockTransition CairoDockTransition;

typedef struct _CairoDockTheme CairoDockTheme;


#define CAIRO_DOCK_NB_DATA_SLOT 12


typedef gboolean (* CairoDockApplyConfigFunc) (gpointer data);


typedef gboolean (* CairoDockForeachDeskletFunc) (CairoDesklet *pDesklet, CairoDockModuleInstance *pInstance, gpointer data);

typedef void (* CairoDockForeachIconFunc) (Icon *icon, CairoContainer *pContainer, gpointer data);


/// Nom du repertoire de travail de cairo-dock.
#define CAIRO_DOCK_DATA_DIR "cairo-dock"
/// Nom du repertoire des extras utilisateur/themes (jauges, clock, etc).
#define CAIRO_DOCK_EXTRAS_DIR "extras"
/// Nom du repertoire des jauges utilisateur/themes.
#define CAIRO_DOCK_GAUGES_DIR "gauges"
/// Nom du repertoire du theme courant.
#define CAIRO_DOCK_CURRENT_THEME_NAME "current_theme"
/// Nom du repertoire des lanceurs.
#define CAIRO_DOCK_LAUNCHERS_DIR "launchers"
/// Nom du repertoire des icones locales.
#define CAIRO_DOCK_LOCAL_ICONS_DIR "icons"
/// Mot cle representant le repertoire local des icones.
#define CAIRO_DOCK_LOCAL_THEME_KEYWORD "_LocalTheme_"
/// Nom du dock principal (le 1er cree).
#define CAIRO_DOCK_MAIN_DOCK_NAME "_MainDock_"
/// Nom de la vue par defaut.
#define CAIRO_DOCK_DEFAULT_RENDERER_NAME N_("default")


#define CAIRO_DOCK_LAST_ORDER -1e9
#define CAIRO_DOCK_NB_MAX_ITERATIONS 1000

#define CAIRO_DOCK_LOAD_ICONS_FOR_DESKLET TRUE

#define CAIRO_DOCK_UPDATE_DOCK_SIZE TRUE
#define CAIRO_DOCK_ANIMATE_ICON TRUE
#define CAIRO_DOCK_INSERT_SEPARATOR TRUE

typedef enum {
	CAIRO_DOCK_MAX_SIZE,
	CAIRO_DOCK_NORMAL_SIZE,
	CAIRO_DOCK_MIN_SIZE
	} CairoDockSizeType;

typedef enum {
	CAIRO_DOCK_UNKNOWN_ENV=0,
	CAIRO_DOCK_GNOME,
	CAIRO_DOCK_KDE,
	CAIRO_DOCK_XFCE,
	CAIRO_DOCK_NB_DESKTOPS
	} CairoDockDesktopEnv;

typedef enum {
	CAIRO_DOCK_BOTTOM = 0,
	CAIRO_DOCK_TOP,
	CAIRO_DOCK_RIGHT,
	CAIRO_DOCK_LEFT,
	CAIRO_DOCK_INSIDE_SCREEN,
	CAIRO_DOCK_NB_POSITIONS
	} CairoDockPositionType;

typedef enum {
	CAIRO_DOCK_LAUNCHER_FROM_DESKTOP_FILE = 0,
	CAIRO_DOCK_LAUNCHER_FOR_CONTAINER,
	CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR,
	CAIRO_DOCK_NB_NEW_LAUNCHER_TYPE
	} CairoDockNewLauncherType;

#endif
