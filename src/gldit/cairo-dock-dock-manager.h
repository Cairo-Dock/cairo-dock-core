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
#include "cairo-dock-style-facility.h"  // GldiColor
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
extern GldiObjectManager myDockObjectMgr;
#endif

typedef enum {
	CAIRO_HIT_SCREEN_BORDER,
	CAIRO_HIT_DOCK_PLACE,
	CAIRO_HIT_SCREEN_CORNER,
	CAIRO_HIT_ZONE,
	CAIRO_HIT_NB_METHODS
} CairoCallbackMethod;

// params
struct _CairoDocksParam {
	// frame
	gint iDockRadius;
	gint iDockLineWidth;
	gint iFrameMargin;
	GldiColor fLineColor;
	gboolean bRoundedBottomCorner;
	// background
	gchar *cBackgroundImageFile;
	gdouble fBackgroundImageAlpha;
	gboolean bBackgroundImageRepeat;
	gint iNbStripes;
	gdouble fStripesWidth;
	GldiColor fStripesColorBright;
	GldiColor fStripesColorDark;
	gdouble fStripesAngle;
	GldiColor fHiddenBg;
	// position
	gint iGapX, iGapY;
	CairoDockPositionType iScreenBorder;
	gdouble fAlign;
	gint iScreenReq;
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
	gboolean bUseDefaultColors;
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


/** Get the name of a Dock.
* @param pDock the dock.
* @return the name of the dock, that identifies it.
*/
#define gldi_dock_get_name(pDock) (pDock)->cDockName

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
void gldi_icons_foreach_in_docks (GldiIconFunc pFunction, gpointer pUserData);


/** (Re)load all buffers of all icons in all docks.
@param bUpdateIconSize TRUE to recalculate the icons and docks size.
*/
void cairo_dock_reload_buffers_in_all_docks (gboolean bUpdateIconSize);

void cairo_dock_set_all_views_to_default (int iDockType);

/** Add a config file for a new root dock. Does not create the dock (use \ref gldi_dock_new for that).
*@return the unique name for the new dock, to be passed to \ref gldi_dock_new.
*/
gchar *gldi_dock_add_conf_file (void);

/** Redraw every root docks.
*/
void gldi_docks_redraw_all_root (void);


void cairo_dock_quick_hide_all_docks (void);
void cairo_dock_stop_quick_hide (void);

G_END_DECLS
#endif
