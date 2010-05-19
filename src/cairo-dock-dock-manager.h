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


#ifndef __CAIRO_DOCK_MANAGER__
#define  __CAIRO_DOCK_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-dock-factory.h"
G_BEGIN_DECLS


/**
*@file cairo-dock-dock-manager.h This class manages all the Docks.
* Each Dock has a name that is unique. A Dock can be a sub-dock or a root-dock, whether there exists an icon that points on it or not, but there is no fundamental difference between both.
*/

struct _CairoDockDockManager {
	CairoDockManager mgr;
	CairoDock *(*cairo_dock_create_dock) (const gchar *cDockName, const gchar *cRendererName);
	
	} ;


void cairo_dock_init_dock_manager (void);


/** Create a new root dock.
* @param cDockName name of the dock, used to identify it quickly. If the name is already used, the corresponding dock is returned.
* @param cRendererName name of a renderer. If NULL, the default renderer will be applied.
* @return the newly allocated dock, to destroy with #cairo_dock_destroy_dock
*/
CairoDock *cairo_dock_create_dock (const gchar *cDockName, const gchar *cRendererName);

/** Increase by 1 the number of pointing icons. If the dock was a root dock, it becomes a sub-dock.
* @param pDock a dock.
* @param pParentDock its parent dock, if it becomes a sub-dock, otherwise it can be NULL.
*/
void cairo_dock_reference_dock (CairoDock *pDock, CairoDock *pParentDock);


/** Create a new dock of type "sub-dock", and load a given list of icons inside. The list then belongs to the dock, so it must not be freeed after that. The buffers of each icon are loaded, so they just need to have an image filename and a name.
* @param pIconList a list of icons that will be loaded and inserted into the new dock.
* @param cDockName desired name for the new dock.
* @param pParentDock the parent dock.
* @return the newly allocated dock.
*/
CairoDock *cairo_dock_create_subdock_from_scratch (GList *pIconList, gchar *cDockName, CairoDock *pParentDock);

/** Destroy a dock and its icons.
* @param pDock the dock.
* @param cDockName name for the dock.
*/
void cairo_dock_destroy_dock (CairoDock *pDock, const gchar *cDockName);


void cairo_dock_reset_docks_table (void);

/** Destroy all docks and all icons contained inside, and free all allocated ressources. Applets and Taskbar are stopped beforehand.
*/
void cairo_dock_free_all_docks (void);


/** Search the name of a Dock. It does a linear search in the table of Docks.
* @param pDock the dock.
* @return the name of the dock, or NULL if not found.
*/
const gchar *cairo_dock_search_dock_name (CairoDock *pDock);

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

CairoDock *cairo_dock_alter_dock_name (const gchar *cDockName, CairoDock *pDock, const gchar *cNewName);

/** Rename a dock. Update the container's name of all of its icons.
*@param cDockName name of the dock.
*@param pDock the dock (optional).
*@param cNewName the new name.
*/
void cairo_dock_rename_dock (const gchar *cDockName, CairoDock *pDock, const gchar *cNewName);

/** Execute an action on all icons.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void cairo_dock_foreach_icons (CairoDockForeachIconFunc pFunction, gpointer pUserData);

/** Execute an action on all icons being inside a dock.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void cairo_dock_foreach_icons_in_docks (CairoDockForeachIconFunc pFunction, gpointer pUserData);

/** Execute an action on all icons being inside a desklet.
*@param pFunction the action.
*@param pUserData data passed to the callback.
*/
void cairo_dock_foreach_icons_in_desklets (CairoDockForeachIconFunc pFunction, gpointer pUserData);

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



void cairo_dock_search_max_decorations_size (int *iWidth, int *iHeight);

/** Recursively hides all the parent docks of a sub-dock.
*@param pDock the (sub)dock.
*/
void cairo_dock_hide_parent_dock (CairoDock *pDock);

/** Recursively hides all the sub-docks of a given dock.
*@param pDock the dock.
*/
gboolean cairo_dock_hide_child_docks (CairoDock *pDock);

/** (Re)load all buffers of all icons in all docks.
*@param bReloadAppletsToo TRUE to reload applet icons too.
*/
void cairo_dock_reload_buffers_in_all_docks (gboolean bReloadAppletsToo);

void cairo_dock_draw_subdock_icons (void);

void cairo_dock_reset_all_views (void);
void cairo_dock_set_all_views_to_default (int iDockType);


/* Ecrit les ecarts en x et en y d'un dock racine dans son fichier de conf.
*@param pDock the dock.
*/
void cairo_dock_write_root_dock_gaps (CairoDock *pDock);

/* Recupere le positionnement complet d'un dock racine a partir de son fichier de conf.
*@param cDockName nom du dock.
*@param pDock the dock.
*@return TRUE si la position a ete mise a jour.
*/
gboolean cairo_dock_get_root_dock_position (const gchar *cDockName, CairoDock *pDock);

void cairo_dock_reload_one_root_dock (const gchar *cDockName, CairoDock *pDock);

/* Supprime le fichier de conf d'un dock racine.
*@param cDockName le nom du dock.
*/
void cairo_dock_remove_root_dock_config (const gchar *cDockName);

gchar *cairo_dock_add_root_dock_config (const gchar *cDockName);

void cairo_dock_redraw_root_docks (gboolean bExceptMainDock);

void cairo_dock_reposition_root_docks (gboolean bExceptMainDock);


void cairo_dock_quick_hide_all_docks (void);
void cairo_dock_stop_quick_hide (void);
void cairo_dock_allow_entrance (CairoDock *pDock);
void cairo_dock_disable_entrance (CairoDock *pDock);
gboolean cairo_dock_entrance_is_allowed (CairoDock *pDock);
void cairo_dock_activate_temporary_auto_hide (CairoDock *pDock);
void cairo_dock_deactivate_temporary_auto_hide (CairoDock *pDock);
#define cairo_dock_is_temporary_hidden(pDock) (pDock)->bTemporaryHidden

void cairo_dock_synchronize_one_sub_dock_position (CairoDock *pSubDock, CairoDock *pDock, gboolean bReloadBuffersIfNecessary);
void cairo_dock_synchronize_sub_docks_position (CairoDock *pDock, gboolean bReloadBuffersIfNecessary);

void cairo_dock_start_polling_screen_edge (void);
void cairo_dock_stop_polling_screen_edge (void);
void cairo_dock_unhide_root_docks_on_screen_edge (CairoDockPositionType iScreenBorder);
/**void cairo_dock_pop_up_root_docks_on_screen_edge (CairoDockPositionType iScreenBorder);
void cairo_dock_set_docks_on_top_layer (gboolean bRootDocksOnly);*/

///void cairo_dock_reserve_space_for_all_root_docks (gboolean bReserve);

void cairo_dock_set_dock_visibility (CairoDock *pDock, CairoDockVisibility iVisibility);


G_END_DECLS
#endif
