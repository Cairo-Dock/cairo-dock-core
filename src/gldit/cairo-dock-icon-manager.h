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

// manager
typedef struct _CairoIconsParam CairoIconsParam;

#ifndef _MANAGER_DEF_
extern CairoIconsParam myIconsParam;
extern GldiManager myIconsMgr;
extern GldiObjectManager myIconObjectMgr;
#endif

#define CAIRO_DOCK_DEFAULT_ICON_SIZE 128

// params
typedef enum {
	CAIRO_DOCK_NORMAL_SEPARATOR,
	CAIRO_DOCK_FLAT_SEPARATOR,
	CAIRO_DOCK_PHYSICAL_SEPARATOR,
	CAIRO_DOCK_NB_SEPARATOR_TYPES
	} CairoDockSeparatorType;
#define CairoDockSpeparatorType CairoDockSeparatorType

struct _CairoIconsParam {
	// icons
	CairoDockIconGroup tIconTypeOrder[CAIRO_DOCK_NB_GROUPS];
	gdouble fReflectHeightRatio;
	gdouble fAlbedo;
	gdouble fAmplitude;
	gint iSinusoidWidth;
	gint iIconGap;
	gint iStringLineWidth;
	gdouble fStringColor[4];
	gdouble fAlphaAtRest;
	///gdouble fReflectSize;
	gchar *cIconTheme;
	gchar *cBackgroundImagePath;
	gint iIconWidth;  // default icon size
	gint iIconHeight;
	// separators
	CairoDockSeparatorType iSeparatorType;
	gint iSeparatorWidth;
	gint iSeparatorHeight;
	gchar *cSeparatorImage;
	gboolean bRevolveSeparator;
	gboolean bConstantSeparatorSize;
	gdouble fSeparatorColor[4];
	// labels
	CairoDockLabelDescription iconTextDescription;
	CairoDockLabelDescription quickInfoTextDescription;
	gboolean bLabelForPointedIconOnly;
	gint iLabelSize;  // taille des etiquettes des icones, en prenant en compte le contour et la marge.
	gdouble fLabelAlphaThreshold;
	gboolean bTextAlwaysHorizontal;
	};

/// signals
typedef enum {
	/// notification called when an icon's sub-dock is starting to (un)fold. data : {Icon}
	NOTIFICATION_UNFOLD_SUBDOCK = NB_NOTIFICATIONS_OBJECT,
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



/** Execute an action on all icons.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void gldi_icons_foreach (GldiIconFunc pFunction, gpointer pUserData);


void cairo_dock_hide_show_launchers_on_other_desktops (void);

void cairo_dock_set_specified_desktop_for_icon (Icon *pIcon, int iSpecificDesktop);

/** Search the icon size of a GtkIconSize.
 * @param iIconSize a GtkIconSize
 * @return the maximum between the width and the height of the icon size in pixel (or 128 if there is a problem) 
 */
gint cairo_dock_search_icon_size (GtkIconSize iIconSize);

/** Search the path of an icon into the defined icons themes. It also handles the '~' caracter in paths.
 * @param cFileName name of the icon file.
 * @param iDesiredIconSize desired icon size if we use icons from user icons theme.
 * @return the complete path of the icon, or NULL if not found.
 */
gchar *cairo_dock_search_icon_s_path (const gchar *cFileName, gint iDesiredIconSize);

void cairo_dock_add_path_to_icon_theme (const gchar *cPath);

void cairo_dock_remove_path_from_icon_theme (const gchar *cPath);


void gldi_register_icons_manager (void);

G_END_DECLS
#endif
