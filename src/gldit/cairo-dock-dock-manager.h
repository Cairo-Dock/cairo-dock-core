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

// manager
typedef struct _CairoDocksParam CairoDocksParam;

#ifndef _MANAGER_DEF_
extern CairoDocksParam myDocksParam;
extern GldiManager myDocksMgr;
extern GldiObjectManager myDockObjectMgr;
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

/// signals
typedef enum {
	/// notification called when the mouse enters a dock.
	NOTIFICATION_ENTER_DOCK = NB_NOTIFICATIONS_CONTAINER,
	/// notification called when the mouse leave a dock.
	NOTIFICATION_LEAVE_DOCK,
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


void gldi_dock_make_subdock (CairoDock *pDock, CairoDock *pParentDock, const gchar *cRendererName);

/** Get the name of a Dock.
* @param pDock the dock.
* @return the name of the dock, that identifies it.
*/
#define gldi_dock_get_name(pDock) (pDock)->cDockName

/** Get a readable name for a main Dock, suitable for display (like "Bottom dock"). Sub-Docks names are defined by the user, so you can just use \ref gldi_dock_get_name for them.
* @param pDock the dock.
* @return the readable name of the dock, or NULL if not found. Free it when you're done.
*/
gchar *gldi_dock_get_readable_name (CairoDock *pDock);

/** Get a Dock from a given name.
* @param cDockName the name of the dock.
* @return the dock that has been registerd under this name, or NULL if none exists.
*/
CairoDock *gldi_dock_get (const gchar *cDockName);

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
*@param pDock the dock (optional).
*@param cNewName the new name.
*/
void gldi_dock_rename (CairoDock *pDock, const gchar *cNewName);

/** Execute an action on all docks.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void gldi_docks_foreach (GHFunc pFunction, gpointer pUserData);

/** Execute an action on all main docks.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void gldi_docks_foreach_root (GFunc pFunction, gpointer pUserData);

/** Execute an action on all icons being inside a dock.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void gldi_icons_foreach_in_docks (CairoDockForeachIconFunc pFunction, gpointer pUserData);


/* Recursively hides all the parent docks of a sub-dock.
*@param pDock the (sub)dock.
*/
void cairo_dock_hide_parent_dock (CairoDock *pDock);  // -> dock-factory

/* Recursively hides all the sub-docks of a given dock.
*@param pDock the dock.
* @return TRUE if a sub-dock has been hidden.
*/
gboolean cairo_dock_hide_child_docks (CairoDock *pDock);  // -> dock-factory

/** (Re)load all buffers of all icons in all docks.
@param bUpdateIconSize TRUE to recalculate the icons and docks size.
*/
void cairo_dock_reload_buffers_in_all_docks (gboolean bUpdateIconSize);

void cairo_dock_set_all_views_to_default (int iDockType);


void gldi_rootdock_write_gaps (CairoDock *pDock);

int cairo_dock_convert_icon_size_to_pixels (GldiIconSizeEnum s, double *fMaxScale, double *fReflectSize, int *iIconGap);

GldiIconSizeEnum cairo_dock_convert_icon_size_to_enum (int iIconSize);

/** Add a config file for a root dock. Does not create the dock (use \ref gldi_dock_new for that). If the config file already exists, it is overwritten (use \ref gldi_dock_get to check if the name is already used).
*@param cDockName name of the dock.
*/
void gldi_dock_add_conf_file_for_name (const gchar *cDockName);

/** Add a config file for a new root dock. Does not create the dock (use \ref gldi_dock_new for that).
*@return the unique name for the new dock, to be passed to \ref gldi_dock_new.
*/
gchar *gldi_dock_add_conf_file (void);

/* Redraw all the root docks.
*@param bExceptMainDock whether to redraw the main dock too.
*/
void cairo_dock_redraw_root_docks (gboolean bExceptMainDock);


void cairo_dock_quick_hide_all_docks (void);
void cairo_dock_stop_quick_hide (void);
void cairo_dock_allow_entrance (CairoDock *pDock);
void cairo_dock_disable_entrance (CairoDock *pDock);
gboolean cairo_dock_entrance_is_allowed (CairoDock *pDock);
void cairo_dock_activate_temporary_auto_hide (CairoDock *pDock);
void cairo_dock_deactivate_temporary_auto_hide (CairoDock *pDock);
#define cairo_dock_is_temporary_hidden(pDock) (pDock)->bTemporaryHidden
void gldi_subdock_synchronize_orientation (CairoDock *pSubDock, CairoDock *pDock, gboolean bUpdateDockSize);


/** Set the visibility of a root dock. Perform all the necessary actions.
*@param pDock a root dock.
*@param iVisibility its new visibility.
*/
void gldi_dock_set_visibility (CairoDock *pDock, CairoDockVisibility iVisibility);


void gldi_register_docks_manager (void);

G_END_DECLS
#endif
