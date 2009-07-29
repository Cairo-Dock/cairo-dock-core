
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

void cairo_dock_initialize_dock_manager (void);

CairoDock *cairo_dock_register_dock (const gchar *cDockName, CairoDock *pDock);

void cairo_dock_unregister_dock (const gchar *cDockName);

void cairo_dock_reset_docks_table (void);


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

CairoDock *cairo_dock_alter_dock_name (const gchar *cDockName, CairoDock *pDock, const gchar *cNewName);

/** Rename a dock. Update the container's name of all of its icons.
*@param cDockName name of the dock.
*@param pDock the dock (optional).
*@param cNewName the new name.
*/
void cairo_dock_rename_dock (const gchar *cDockName, CairoDock *pDock, const gchar *cNewName);

void cairo_dock_reset_all_views (void);
void cairo_dock_set_all_views_to_default (int iDockType);


/* Ecrit les ecarts en x et en y d'un dock racine dans son fichier de conf.
*@param pDock the dock.
*/
void cairo_dock_write_root_dock_gaps (CairoDock *pDock);

/* Recupere le positionnement complet d'un dock racine a partir de son fichier de conf.
*@param cDockName nom du dock.
*@param pDock the dock.
*/
gboolean cairo_dock_get_root_dock_position (const gchar *cDockName, CairoDock *pDock);

/*
* Supprime le fichier de conf d'un dock racine.
*@param cDockName le nom du dock.
*/
void cairo_dock_remove_root_dock_config (const gchar *cDockName);

void cairo_dock_redraw_root_docks (gboolean bExceptMainDock);

void cairo_dock_reposition_root_docks (gboolean bExceptMainDock);


void cairo_dock_activate_temporary_auto_hide (void);
void cairo_dock_quick_hide_all_docks (void);
void cairo_dock_deactivate_temporary_auto_hide (void);
void cairo_dock_stop_quick_hide (void);
void cairo_dock_allow_entrance (CairoDock *pDock);
void cairo_dock_disable_entrance (CairoDock *pDock);
gboolean cairo_dock_entrance_is_allowed (CairoDock *pDock);
gboolean cairo_dock_quick_hide_is_activated (void);

gboolean cairo_dock_window_hovers_dock (GtkAllocation *pWindowGeometry, CairoDock *pDock);

void cairo_dock_synchronize_one_sub_dock_position (CairoDock *pSubDock, CairoDock *pDock, gboolean bReloadBuffersIfNecessary);
void cairo_dock_synchronize_sub_docks_position (CairoDock *pDock, gboolean bReloadBuffersIfNecessary);

void cairo_dock_start_polling_screen_edge (CairoDock *pMainDock);
void cairo_dock_stop_polling_screen_edge (void);
void cairo_dock_pop_up_root_docks_on_screen_edge (CairoDockPositionType iScreenBorder);
void cairo_dock_set_root_docks_on_top_layer (void);

void cairo_dock_reserve_space_for_all_root_docks (gboolean bReserve);


gchar *cairo_dock_get_unique_dock_name (const gchar *cPrefix);
gboolean cairo_dock_check_unique_subdock_name (Icon *pIcon);

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


G_END_DECLS
#endif
