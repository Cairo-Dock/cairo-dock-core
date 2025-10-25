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

#ifndef __CAIRO_DOCK_APPLICATIONS_PRIV__
#define  __CAIRO_DOCK_APPLICATIONS_PRIV__

// #include <glib.h>

#include "cairo-dock-applications-manager.h"

G_BEGIN_DECLS


// manager
typedef struct _CairoTaskbarParam CairoTaskbarParam;
typedef struct _Icon AppliIcon;

#ifndef _MANAGER_DEF_
extern CairoTaskbarParam myTaskbarParam;
#endif


typedef enum {
	CAIRO_APPLI_BEFORE_FIRST_ICON,
	CAIRO_APPLI_BEFORE_FIRST_LAUNCHER,
	CAIRO_APPLI_AFTER_LAST_LAUNCHER,
	CAIRO_APPLI_AFTER_LAST_ICON,
	CAIRO_APPLI_AFTER_ICON,
	CAIRO_APPLI_NB_PLACEMENTS
} CairoTaskbarPlacement;

typedef enum {
	CAIRO_APPLI_ACTION_NONE,
	CAIRO_APPLI_ACTION_CLOSE,
	CAIRO_APPLI_ACTION_MINIMISE,
	CAIRO_APPLI_ACTION_LAUNCH_NEW,
	CAIRO_APPLI_ACTION_LOWER,
	CAIRO_APPLI_ACTION_NB_ACTIONS
} CairoTaskbarAction;

// params
struct _CairoTaskbarParam {
	gboolean bShowAppli;
	gboolean bGroupAppliByClass;
	gint iAppliMaxNameLength;
	gboolean bMinimizeOnClick;
	gboolean bPresentClassOnClick;
	CairoTaskbarAction iActionOnMiddleClick;
	gboolean bHideVisibleApplis;
	gdouble fVisibleAppliAlpha;
	gboolean bAppliOnCurrentDesktopOnly;
	gboolean bDemandsAttentionWithDialog;
	gint iDialogDuration;
	gchar *cAnimationOnDemandsAttention;
	gchar *cAnimationOnActiveWindow;
	gboolean bOverWriteXIcons;
	gint iMinimizedWindowRenderType;
	gboolean bMixLauncherAppli;
	gchar *cOverwriteException;
	gchar *cGroupException;
	gchar *cForceDemandsAttention;
	CairoTaskbarPlacement iIconPlacement;
	gchar *cRelativeIconName;
	gboolean bSeparateApplis;
	} ;

// signals
typedef enum {
	NB_NOTIFICATIONS_TASKBAR = NB_NOTIFICATIONS_ICON,
	} CairoTaskbarNotifications;


/* Functions defined in cairo-dock-applications-manager.c */

/** Start the applications manager. It will load all the appli-icons, and keep monitoring them. If enabled, it will insert them into the dock.
*@param pDock the main dock
*/
void cairo_dock_start_applications_manager (CairoDock *pDock);

/** Get the icon of the currently active window, if any.
*@return the icon (maybe not inside a dock, maybe NULL).
*/
Icon *cairo_dock_get_current_active_icon (void);

void cairo_dock_set_icons_geometry_for_window_manager (CairoDock *pDock);

void gldi_register_applications_manager (void);


/* Functions defined in cairo-dock-application-facility.c */

void gldi_appli_icon_demands_attention (Icon *icon);

void gldi_appli_icon_stop_demanding_attention (Icon *icon);

void gldi_appli_icon_animate_on_active (Icon *icon, CairoDock *pParentDock);

CairoDock *gldi_appli_icon_insert_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bAnimate);

CairoDock *gldi_appli_icon_detach (Icon *pIcon);

/** Set the minimize location for an application window. This is used by the window
* manager / compositor when the window is minimize as the "target" of the minimize
* animation. On at least Compiz, it is also used as an area that triggers showing
* window previews when entered with the mouse.
*@param pAppli the application whose minimize position to set
*@param icon the icon that should be the minimize target (can be different from pAppli's icon)
*@param pDock the dock that contains icon
*/
void gldi_appli_icon_set_geometry_for_window_manager_full (GldiWindowActor *pAppli, Icon *icon, CairoDock *pDock);
/** Set the minimize location for an application window. Simplified version where
* the corresponding icon is used.
*@param icon an application icon
*@param pDock the dock that contains icon
*/
void gldi_appli_icon_set_geometry_for_window_manager (Icon *icon, CairoDock *pDock);

/** Reserve a minimize location for an application window whose associated icon
* might not be displayed currently (i.e. it is detached). The location will be set
* to where the icon will appear if it is inserted to the given dock.
*@param pAppli the application whose minimize position to set
*@param icon the icon corresponding to this application
*@param pMainDock the dock to use if no more appropriate dock is found 
* Note: this function does nothing if icon is not a detached icon (i.e. if it is already displayed in a dock).
*/
void gldi_appli_reserve_geometry_for_window_manager (GldiWindowActor *pAppli, Icon *icon, CairoDock *pMainDock);

void gldi_window_inhibitors_set_name (GldiWindowActor *actor, const gchar *cNewName);

void gldi_window_inhibitors_set_active_state (GldiWindowActor *actor, gboolean bActive);

void gldi_window_inhibitors_set_hidden_state (GldiWindowActor *actor, gboolean bIsHidden);

G_END_DECLS
#endif

