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

#ifndef __CAIRO_DOCK_ICON_MANAGER__
#define  __CAIRO_DOCK_ICON_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"
#include "cairo-dock-surface-factory.h"  // CairoDockLabelDescription
#include "cairo-dock-icon-factory.h"  // CairoDockIconGroup
G_BEGIN_DECLS

/**
*@file cairo-dock-icon-manager.h This class manages the icons parameters and their associated ressources.
*
* Specialized Icons are handled by the corresponding manager.
*/

typedef struct _CairoIconsParam CairoIconsParam;
typedef struct _CairoIconsManager CairoIconsManager;

#ifndef _MANAGER_DEF_
extern CairoIconsParam myIconsParam;
extern CairoIconsManager myIconsMgr;
#endif

// params
typedef enum {
	CAIRO_DOCK_NORMAL_SEPARATOR,
	CAIRO_DOCK_FLAT_SEPARATOR,
	CAIRO_DOCK_PHYSICAL_SEPARATOR,
	CAIRO_DOCK_NB_SEPARATOR_TYPES
	} CairoDockSpeparatorType;

struct _CairoIconsParam {
	// icons
	gdouble fReflectHeightRatio;
	gdouble fAlbedo;
	gdouble fAmplitude;
	gint iSinusoidWidth;
	gint iIconGap;
	gint iStringLineWidth;
	gdouble fStringColor[4];
	gdouble fAlphaAtRest;
	gdouble fReflectSize;
	gchar *cIconTheme;
	gchar *cBackgroundImagePath;
	gint tIconAuthorizedWidth[CAIRO_DOCK_NB_GROUPS];
	gint tIconAuthorizedHeight[CAIRO_DOCK_NB_GROUPS];
	CairoDockSpeparatorType iSeparatorType;
	gchar *cSeparatorImage;
	gboolean bRevolveSeparator;
	gboolean bConstantSeparatorSize;
	gdouble fSeparatorColor[4];
	CairoDockIconGroup tIconTypeOrder[CAIRO_DOCK_NB_GROUPS];
	// labels
	CairoDockLabelDescription iconTextDescription;
	CairoDockLabelDescription quickInfoTextDescription;
	gboolean bLabelForPointedIconOnly;
	gint iLabelSize;  // taille des etiquettes des icones, en prenant en compte le contour et la marge.
	gdouble fLabelAlphaThreshold;
	gboolean bTextAlwaysHorizontal;
	};

// manager
struct _CairoIconsManager {
	GldiManager mgr;
	void (*free_icon) (Icon *icon);
	void (*foreach_icons) (CairoDockForeachIconFunc pFunction, gpointer pUserData);
	void (*hide_show_launchers_on_other_desktops) (CairoDock *pDock);
	void (*set_specified_desktop_for_icon) (Icon *pIcon, int iSpecificDesktop);
	} ;

/// signals
typedef enum {
	/// notification called when an icon's sub-dock is starting to (un)fold. data : {Icon}
	NOTIFICATION_UNFOLD_SUBDOCK,
	/// notification called when an icon is updated in the fast rendering loop.
	NOTIFICATION_UPDATE_ICON,
	/// notification called when an icon is updated in the slow rendering loop.
	NOTIFICATION_UPDATE_ICON_SLOW,
	/// notification called when the background of an icon is rendered.
	NOTIFICATION_PRE_RENDER_ICON,
	/// notification called when an icon is rendered.
	NOTIFICATION_RENDER_ICON,
	/// notification called when an icon is stopped, for instance before it is removed.
	NOTIFICATION_STOP_ICON,
	/// notification called when someone asks for an animation for a given icon.
	NOTIFICATION_REQUEST_ICON_ANIMATION,
	/// 
	NB_NOTIFICATIONS_ICON
	} CairoIconNotifications;


/** Terminate an Icon and free all its allocated ressources, except its sub-dock.
*@param icon the icon to destroy.
*/
void cairo_dock_free_icon (Icon *icon);

/** Execute an action on all icons.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void cairo_dock_foreach_icons (CairoDockForeachIconFunc pFunction, gpointer pUserData);


void cairo_dock_hide_show_launchers_on_other_desktops (CairoDock *pDock);

void cairo_dock_set_specified_desktop_for_icon (Icon *pIcon, int iSpecificDesktop);

/** Search the path of an icon into the defined icons themes. It also handles the '~' caracter in paths.
 * @param cFileName name of the icon file.
 * @return the complete path of the icon, or NULL if not found.
 */
gchar *cairo_dock_search_icon_s_path (const gchar *cFileName);

void gldi_register_icons_manager (void);

G_END_DECLS
#endif
