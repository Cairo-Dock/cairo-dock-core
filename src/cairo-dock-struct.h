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
 *
 * \section intro_sec Introduction
 *
 * Modules are compiled .so files (that is to say, library) that are plugged into the dock at run-time.
 * Due to this fact, they can use any function used by the dock, and have a total interacton freedom on the dock.
 * The advantage is that applets can do anything, in fact they are extensions of the dock itself.
 * The drawback is that a buggy applet can make the dock unstable.
 * Cairo-Dock exports its API through the installed .h headers. We will see here the main structures, and then the framework dedicated to easily write applets.
 *
 * \section install_sec Installation
 * 
 * \section main_struct_sec Main structures
 *
 * \subsection containers Containers
 * 
 * \subsection icons Icons
 * 
 * \subsection dock Dock
 * 
 * \subsection desklet Desklet
 * 
 * \subsection dialog Dialog
 * 
 * \subsection flying Flying Container
 * 
 * 
 *  
 * etc...
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
