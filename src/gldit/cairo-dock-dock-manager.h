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

#ifndef __CAIRO_DOCK_DOCK_MANAGER__
#define  __CAIRO_DOCK_DOCK_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-dock-factory.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-dock-manager.h This class manages all the Docks.
* Each Dock has a name that is unique. A Dock can be a sub-dock or a root-dock, whether there exists an icon that points on it or not, but there is no fundamental difference between both.
*/

typedef struct _CairoDocksParam CairoDocksParam;
typedef struct _CairoDocksManager CairoDocksManager;

#ifndef _MANAGER_DEF_
extern CairoDocksParam myDocksParam;
extern CairoDocksManager myDocksMgr;
#endif

typedef enum {
	CAIRO_HIT_SCREEN_BORDER,
	CAIRO_HIT_DOCK_PLACE,
	CAIRO_HIT_SCREEN_CORNER,
	CAIRO_HIT_ZONE,
	CAIRO_HIT_NB_METHODS
} CairoCallbackMethod;

typedef enum {
	ICON_DEFAULT,  // same as main dock
	ICON_TINY,
	ICON_VERY_SMALL,
	ICON_SMALL,
	ICON_MEDIUM,
	ICON_BIG,
	ICON_HUGE
	} GldiIconSizeEnum;

/// TODO: harmonize the values with the simple config -> make some public functions...
typedef enum {
	ICON_SIZE_TINY = 28,
	ICON_SIZE_VERY_SMALL = 36,
	ICON_SIZE_SMALL = 42,
	ICON_SIZE_MEDIUM = 48,
	ICON_SIZE_BIG = 56,
	ICON_SIZE_HUGE = 64
	} GldiIconSize;

// params
struct _CairoDocksParam {
	// frame
	gint iDockRadius;
	gint iDockLineWidth;
	gint iFrameMargin;
	gdouble fLineColor[4];
	gboolean bRoundedBottomCorner;
	// background
	gchar *cBackgroundImageFile;
	gdouble fBackgroundImageAlpha;
	gboolean bBackgroundImageRepeat;
	gint iNbStripes;
	gdouble fStripesWidth;
	gdouble fStripesColorBright[4];
	gdouble fStripesColorDark[4];
	gdouble fStripesAngle;
	gdouble fHiddenBg[4];
	// position
	gint iGapX, iGapY;
	CairoDockPositionType iScreenBorder;
	gdouble fAlign;
	gint iNumScreen;
	// Root dock visibility
	CairoDockVisibility iVisibility;
	gchar *cHideEffect;
	CairoCallbackMethod iCallbackMethod;
	gint iZoneWidth, iZoneHeight;
	gchar *cZoneImage;
	gdouble fZoneAlpha;
	gchar *cRaiseDockShortcut;
	gint iUnhideDockDelay;
	//Sub-Dock visibility
	gboolean bShowSubDockOnClick;
	gint iShowSubDockDelay;
	gint iLeaveSubDockDelay;
	gboolean bAnimateSubDock;
	// others
	gboolean bExtendedMode;
	gboolean bLockIcons;
	gboolean bLockAll;
	};

// manager
struct _CairoDocksManager {
	GldiManager mgr;
	CairoDock *(*cairo_dock_create_dock) (const gchar *cDockName);
	void (*destroy_dock) (CairoDock *pDock, const gchar *cDockName);
	} ;

/// signals
typedef enum {
	/// notification called when the mouse enters a dock.
	NOTIFICATION_ENTER_DOCK = NB_NOTIFICATIONS_CONTAINER,
	/// notification called when the mouse leave a dock.
	NOTIFICATION_LEAVE_DOCK,
	NOTIFICATION_STOP_DOCK_DEPRECATED,
	/// notification called when an icon has just been inserted into a dock. data : {Icon, CairoDock}
	NOTIFICATION_INSERT_ICON,
	/// notification called when an icon is going to be removed from a dock. data : {Icon, CairoDock}
	NOTIFICATION_REMOVE_ICON,
	/// notification called when an icon is moved inside a dock. data : {Icon, CairoDock}
	NOTIFICATION_ICON_MOVED,
	NB_NOTIFICATIONS_DOCKS
	} CairoDocksNotifications;


void cairo_dock_force_docks_above (void);


void cairo_dock_reset_docks_table (void);

/** Create a new root dock.
* @param cDockName name (= ID) of the dock. If the name is already used, the corresponding dock is returned.
* @return the dock, to destroy with #cairo_dock_destroy_dock
*/
CairoDock *cairo_dock_create_dock (const gchar *cDockName);

/** Create a new dock of type "sub-dock", and load a given list of icons inside. The list then belongs to the dock, so it must not be freeed after that. The buffers of each icon are loaded, so they just need to have an image filename and a name.
* @param cDockName desired name for the new dock.
* @param cRendererName name of a renderer. If NULL, the default renderer will be applied.
* @param pParentDock the parent dock.
* @param pIconList a list of icons that will be loaded and inserted into the new dock.
* @return the newly allocated dock.
*/
CairoDock *cairo_dock_create_subdock (const gchar *cDockName, const gchar *cRendererName, CairoDock *pParentDock, GList *pIconList);

void cairo_dock_main_dock_to_sub_dock (CairoDock *pDock, CairoDock *pParentDock, const gchar *cRendererName);


/** Destroy a dock and its icons.
* @param pDock the dock.
* @param cDockName name for the dock.
*/
void cairo_dock_destroy_dock (CairoDock *pDock, const gchar *cDockName);


/** Search the name of a Dock. It does a linear search in the table of Docks.
* @param pDock the dock.
* @return the name of the dock, or NULL if not found.
*/
const gchar *cairo_dock_search_dock_name (CairoDock *pDock);

/** Get a readable name for a main Dock, suitable for display (like "Bottom dock"). Sub-Docks names are defined by the user, so you can just use \ref cairo_dock_search_dock_name for them.
* @param pDock the dock.
* @return the readable name of the dock, or NULL if not found. Free it when you're done.
*/
gchar *cairo_dock_get_readable_name_for_fock (CairoDock *pDock);

/** Search a Dock from a given name. It does a fast search in the table of Docks.
* @param cDockName the name of the dock.
* @return the dock that has been registerd under this name, or NULL if none exists.
*/
CairoDock *cairo_dock_search_dock_from_name (const gchar *cDockName);

/** Search an icon pointing on a dock. If several icons point on it, the first one will be returned.
* @param pDock the dock.
* @param pParentDock if not NULL, this will be filled with the dock containing the icon.
* @return the icon pointing on the dock.
*/
Icon *cairo_dock_search_icon_pointing_on_dock (CairoDock *pDock, CairoDock **pParentDock);

// renvoie un nom de dock unique; cPrefix peut etre NULL.
gchar *cairo_dock_get_unique_dock_name (const gchar *cPrefix);
gboolean cairo_dock_check_unique_subdock_name (Icon *pIcon);

/** Rename a dock. Update the container's name of all of its icons.
*@param cDockName name of the dock.
*@param pDock the dock (optional).
*@param cNewName the new name.
*/
void cairo_dock_rename_dock (const gchar *cDockName, CairoDock *pDock, const gchar *cNewName);

/** Execute an action on all docks.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void cairo_dock_foreach_docks (GHFunc pFunction, gpointer pUserData);

/** Execute an action on all main docks.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void cairo_dock_foreach_root_docks (GFunc pFunction, gpointer pUserData);

/** Execute an action on all icons being inside a dock.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void cairo_dock_foreach_icons_in_docks (CairoDockForeachIconFunc pFunction, gpointer pUserData);


/** Recursively hides all the parent docks of a sub-dock.
*@param pDock the (sub)dock.
*/
void cairo_dock_hide_parent_dock (CairoDock *pDock);

/** Recursively hides all the sub-docks of a given dock.
*@param pDock the dock.
* @return TRUE if a sub-dock has been hidden.
*/
gboolean cairo_dock_hide_child_docks (CairoDock *pDock);

/** (Re)load all buffers of all icons in all docks.
@param bUpdateIconSize TRUE to recalculate the icons and docks size.
*/
void cairo_dock_reload_buffers_in_all_docks (gboolean bUpdateIconSize);

void cairo_dock_draw_subdock_icons (void);

void cairo_dock_set_all_views_to_default (int iDockType);


void cairo_dock_write_root_dock_gaps (CairoDock *pDock);

int cairo_dock_convert_icon_size_to_pixels (GldiIconSizeEnum s, double *fMaxScale, double *fReflectSize, int *iIconGap);

GldiIconSizeEnum cairo_dock_convert_icon_size_to_enum (int iIconSize);

/** Reload the config of a root dock and update it accordingly.
*@param cDockName name of the dock.
*@param pDock the dock.
*/
void cairo_dock_reload_one_root_dock (const gchar *cDockName, CairoDock *pDock);

/** Delete the config of a root dock. Doesn't delete the dock (use \ref cairo_dock_destroy_dock for that), but if it was empty, it won't be created the next time you restart Cairo-Dock.
*@param cDockName name of the dock.
*/
void cairo_dock_remove_root_dock_config (const gchar *cDockName);

/** Add a config file for a root dock. Does not create the dock (use \ref cairo_dock_create_dock for that). If the config file already exists, it is overwritten (use \ref cairo_dock_search_dock_from_name to check if the dock already exists).
*@param cDockName name of the dock.
*/
void cairo_dock_add_root_dock_config_for_name (const gchar *cDockName);

/** Add a config file for a new root dock. Does not create the dock (use \ref cairo_dock_create_dock for that).
*@return the unique name for the new dock, to be passed to \ref cairo_dock_create_dock.
*/
gchar *cairo_dock_add_root_dock_config (void);

/* Redraw all the root docks.
*@param bExceptMainDock whether to redraw the main dock too.
*/
void cairo_dock_redraw_root_docks (gboolean bExceptMainDock);

/* Reposition all the root docks.
*@param bExceptMainDock whether to redraw the main dock too.
*/
void cairo_dock_reposition_root_docks (gboolean bExceptMainDock);


void cairo_dock_quick_hide_all_docks (void);
void cairo_dock_stop_quick_hide (void);
void cairo_dock_allow_entrance (CairoDock *pDock);
void cairo_dock_disable_entrance (CairoDock *pDock);
gboolean cairo_dock_entrance_is_allowed (CairoDock *pDock);
void cairo_dock_activate_temporary_auto_hide (CairoDock *pDock);
void cairo_dock_deactivate_temporary_auto_hide (CairoDock *pDock);
#define cairo_dock_is_temporary_hidden(pDock) (pDock)->bTemporaryHidden

void cairo_dock_synchronize_one_sub_dock_orientation (CairoDock *pSubDock, CairoDock *pDock, gboolean bUpdateDockSize);

void cairo_dock_set_dock_orientation (CairoDock *pDock, CairoDockPositionType iScreenBorder);

void cairo_dock_start_polling_screen_edge (void);
void cairo_dock_stop_polling_screen_edge (void);
void cairo_dock_unhide_root_docks_on_screen_edge (CairoDockPositionType iScreenBorder);

/** Set the visibility of a root dock. Perform all the necessary actions.
*@param pDock a root dock.
*@param iVisibility its new visibility.
*/
void cairo_dock_set_dock_visibility (CairoDock *pDock, CairoDockVisibility iVisibility);


void gldi_register_docks_manager (void);

G_END_DECLS
#endif
