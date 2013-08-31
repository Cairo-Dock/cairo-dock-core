/**
* This file is a part of the Cairo-Dock project
* 
* Login : <ctaf42@gmail.com>
* Started on  Sun Jan 27 18:35:38 2008 Cedric GESTES
* $Id$
*
* Author(s)
*  - Cedric GESTES <ctaf42@gmail.com>
*  - Fabrice REY
*
* Copyright (C) 2008 Cedric GESTES
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

#ifndef __CAIRO_DESKLET_MANAGER_H__
#define  __CAIRO_DESKLET_MANAGER_H__

#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"
#include "cairo-dock-desklet-factory.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-desklet-manager.h This class manages the Desklets, that are Widgets placed directly on your desktop.
* A Desklet is a container that holds 1 applet's icon plus an optionnal list of other icons and an optionnal GTK widget, has a decoration, suports several accessibility types (like Compiz Widget Layer), and has a renderer.
* Desklets can be resized or moved directly with the mouse, and can be rotated in the 3 directions of space.
*/

// manager
typedef struct _CairoDeskletsParam CairoDeskletsParam;

#ifndef _MANAGER_DEF_
extern CairoDeskletsParam myDeskletsParam;
extern GldiManager myDeskletsMgr;
extern GldiObjectManager myDeskletObjectMgr;
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

/// Definition of a function that runs through all desklets.
typedef gboolean (* GldiDeskletForeachFunc) (CairoDesklet *pDesklet, gpointer data);

/// signals
typedef enum {
	/// notification called when the mouse enters a desklet.
	NOTIFICATION_ENTER_DESKLET = NB_NOTIFICATIONS_CONTAINER,
	/// notification called when the mouse leave a desklet.
	NOTIFICATION_LEAVE_DESKLET,
	NOTIFICATION_STOP_DESKLET_DEPRECATED,
	/// notification called when a desklet is resized or moved on the screen.
	NOTIFICATION_CONFIGURE_DESKLET,
	/// notification called when a new desklet is created.
	NOTIFICATION_NEW_DESKLET,
	NB_NOTIFICATIONS_DESKLET
	} CairoDeskletNotifications;


/** Run a function through all the desklets. If the callback returns TRUE, then the loop ends and the function returns the current desklet.
*@param pCallback function to be called on eash desklet. If it returns TRUE, the loop ends and the function returns the current desklet.
*@param user_data data to be passed to the callback.
*@return the found desklet, or NULL.
*/
CairoDesklet *gldi_desklets_foreach (GldiDeskletForeachFunc pCallback, gpointer user_data);

/** Execute an action on all icons being inside a desklet.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void gldi_desklets_foreach_icons (GldiIconFunc pFunction, gpointer pUserData);


/** Make all desklets visible. Their accessibility is set to #CAIRO_DESKLET_NORMAL. 
*@param bOnWidgetLayerToo TRUE if you want to act on the desklet that are on the WidgetLayer as well.
*/
void gldi_desklets_set_visible (gboolean bOnWidgetLayerToo);

/** Reset the desklets accessibility to the state defined in their conf file.
*/
void gldi_desklets_set_visibility_to_default (void);


gboolean gldi_desklet_manager_is_ready (void);  // internals for the factory


Icon *gldi_desklet_find_clicked_icon (CairoDesklet *pDesklet);  // internals for the factory; placed here because it uses the same code as the rendering


void gldi_register_desklets_manager (void);

G_END_DECLS

#endif
