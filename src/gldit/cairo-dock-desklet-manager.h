/*
** Login : <ctaf42@gmail.com>
** Started on  Sun Jan 27 18:35:38 2008 Cedric GESTES
** $Id$
**
** Author(s)
**  - Cedric GESTES <ctaf42@gmail.com>
**  - Fabrice REY
**
** Copyright (C) 2008 Cedric GESTES
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __CAIRO_DESKLET_MANAGER_H__
#define  __CAIRO_DESKLET_MANAGER_H__

#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-desklet-manager.h This class manages the Desklets, that are Widgets placed directly on your desktop.
* A Desklet is a container that holds 1 applet's icon plus an optionnal list of other icons and an optionnal GTK widget, has a decoration, suports several accessibility types (like Compiz Widget Layer), and has a renderer.
* Desklets can be resized or moved directly with the mouse, and can be rotated in the 3 directions of space.
*/

typedef struct _CairoDeskletsParam CairoDeskletsParam;
typedef struct _CairoDeskletManager CairoDeskletManager;

#ifndef _MANAGER_DEF_
extern CairoDeskletsParam myDeskletsParam;
extern CairoDeskletManager myDeskletsMgr;
#endif

// params
struct _CairoDeskletsParam {
	gchar *cDeskletDecorationsName;
	gint iDeskletButtonSize;
	gchar *cRotateButtonImage;
	gchar *cRetachButtonImage;
	gchar *cDepthRotateButtonImage;
	gchar *cNoInputButtonImage;
	};

// manager
/// Definition of a function that runs through all desklets.
typedef gboolean (* CairoDockForeachDeskletFunc) (CairoDesklet *pDesklet, gpointer data);

struct _CairoDeskletManager {
	GldiManager mgr;
	CairoDesklet* (*create_desklet) (Icon *pIcon, CairoDeskletAttribute *pAttributes);
	void (*destroy_desklet) (CairoDesklet *pDesklet);
	
	void (*foreach_desklet) (CairoDockForeachDeskletFunc pCallback, gpointer user_data);
	void (*foreach_icons_in_desklets) (CairoDockForeachIconFunc pFunction, gpointer pUserData);
	void (*reload_desklets_decorations) (gboolean bDefaultThemeOnly);
	void (*set_all_desklets_visible) (gboolean bOnWidgetLayerToo);
	void (*set_desklets_visibility_to_default) (void);
	void (*get_desklet_by_Xid) (Window Xid);
	
	void (*find_clicked_icon_in_desklet) (CairoDesklet *pDesklet);
	} ;

/// signals
typedef enum {
	/// notification called when the mouse enters a desklet.
	NOTIFICATION_ENTER_DESKLET = NB_NOTIFICATIONS_CONTAINER,
	/// notification called when the mouse leave a desklet.
	NOTIFICATION_LEAVE_DESKLET,
	/// notification called when a desklet is updated in the fast rendering loop.
	NOTIFICATION_UPDATE_DESKLET,
	/// notification called when a desklet is updated in the slow rendering loop.
	NOTIFICATION_UPDATE_DESKLET_SLOW,
	/// notification called when a desklet is rendered.
	NOTIFICATION_RENDER_DESKLET,
	/// notification called when a desklet is stopped, for instance before it is destroyed.
	NOTIFICATION_STOP_DESKLET,
	/// notification called when a desklet is resized or moved on the screen.
	NOTIFICATION_CONFIGURE_DESKLET,
	/// 
	NB_NOTIFICATIONS_DESKLET
	} CairoDeskletNotifications;


void cairo_dock_load_desklet_buttons (void);  // passer en static
void cairo_dock_unload_desklet_buttons (void);  // merge avec unload


/** Create a desklet linked to an Icon, and load its configuration.
*@param pIcon the main icon, or NULL.
*@param pAttributes the configuration attributes, or NULL.
*@return the newly allocated desklet.
*/
CairoDesklet *cairo_dock_create_desklet (Icon *pIcon, CairoDeskletAttribute *pAttributes);

/** Destroy a desklet.
*@param pDesklet a desklet.
*/
void cairo_dock_destroy_desklet (CairoDesklet *pDesklet);


/** Run a function through all the desklets. If the callback returns TRUE, then the loop ends and the function returns the current desklet.
*@param pCallback function to be called on eash desklet. If it returns TRUE, the loop ends and the function returns the current desklet.
*@param user_data data to be passed to the callback.
*@return the found desklet, or NULL.
*/
CairoDesklet *cairo_dock_foreach_desklet (CairoDockForeachDeskletFunc pCallback, gpointer user_data);

/** Execute an action on all icons being inside a desklet.
 *@param pFunction the action.
 *@param pUserData data passed to the callback.
 */
void cairo_dock_foreach_icons_in_desklets (CairoDockForeachIconFunc pFunction, gpointer pUserData);

/** Reload the decorations of all the desklets. 
*@param bDefaultThemeOnly whether to reload only the desklet that have the default decoration theme.
*/
void cairo_dock_reload_desklets_decorations (gboolean bDefaultThemeOnly);

/** Make all desklets visible. Their accessibility is set to #CAIRO_DESKLET_NORMAL. 
*@param bOnWidgetLayerToo TRUE if you want to act on the desklet that are on the WidgetLayer as well.
*/
void cairo_dock_set_all_desklets_visible (gboolean bOnWidgetLayerToo);

/** Reset the desklets accessibility to the state defined in their conf file.
*/
void cairo_dock_set_desklets_visibility_to_default (void);

/** Search the desklet whose X ID matches the given one.
*@param Xid an X ID.
*@return the desklet that matches, or NULL if none match.
*/
CairoDesklet *cairo_dock_get_desklet_by_Xid (Window Xid);


/** Find the currently pointed icon in a desklet, taking into account the 3D rotations.
*@param pDesklet the desklet.
*@return the pointed icon, or NULL if none.
*/
Icon *cairo_dock_find_clicked_icon_in_desklet (CairoDesklet *pDesklet);

gboolean cairo_dock_desklet_manager_is_ready (void);


void gldi_register_desklets_manager (void);

G_END_DECLS

#endif
