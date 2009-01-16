
#ifndef __CAIRO_DOCK_MANAGER__
#define  __CAIRO_DOCK_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


/**
* Initialise la classe des docks. N'a aucun effet la 2eme fois.
*/
void cairo_dock_initialize_dock_manager (void);

/**
* Enregistre un dock dans la table des docks.
* @param cDockName nom du dock.
* @param pDock le dock.
* @return le dock portant ce nom s'il en existait deja un, sinon le dock qui a ete insere.
*/
CairoDock *cairo_dock_register_dock (const gchar *cDockName, CairoDock *pDock);
/**
* Desenregistre un dock de la table des docks.
* @param cDockName le nom du dock.
*/
void cairo_dock_unregister_dock (const gchar *cDockName);
/**
* Vide la table des docks, en detruisant tous les docks et leurs icones.
*/
void cairo_dock_reset_docks_table (void);


/**
* Cherche le nom d'un dock, en parcourant la table des docks jusqu'a trouver celui passe en entree.
* @param pDock le dock.
* @return le nom du dock, ou NULL si ce dock n'existe pas.
*/
const gchar *cairo_dock_search_dock_name (CairoDock *pDock);
/**
* Cherche un dock etant donne son nom.
* @param cDockName le nom du dock.
* @return le dock qui a ete enregistre sous ce nom, ou NULL si aucun ne correspond.
*/
CairoDock *cairo_dock_search_dock_from_name (const gchar *cDockName);
/**
* Cherche l'icone pointant sur un dock. Si plusieurs icones pointent sur ce dock, la premiere sera renvoyee.
* @param pDock le dock.
* @param pParentDock si non NULL, sera renseigne avec le dock contenant l'icone.
* @return l'icone pointant sur le dock.
*/
Icon *cairo_dock_search_icon_pointing_on_dock (CairoDock *pDock, CairoDock **pParentDock);
/**
* Cherche le container contenant l'icone donnee, en parcourant la liste des icones de tous les docks jusqu'a trouver celle passee en entree, ou en renvoyant son desklet dans le cas d'une applet.
* @param icon l'icone.
* @return le container contenant cette icone, ou NULL si aucun n'a ete trouve.
*/
CairoContainer *cairo_dock_search_container_from_icon (Icon *icon);


/**
* Met a jour un fichier .desktop avec la liste des docks.
* @param pKeyFile fichier de conf ouvert.
* @param cDesktopFilePath chemin du fichier de conf.
* @param cGroupName nom du groupe
* @param cKeyName nom de la cle.
*/
void cairo_dock_update_conf_file_with_containers_full (GKeyFile *pKeyFile, gchar *cDesktopFilePath, gchar *cGroupName, gchar *cKeyName);
/**
* Met a jour un fichier .desktop avec la liste des docks dans le champ "Container".
* @param pKeyFile fichier de conf ouvert.
* @param cDesktopFilePath chemin du fichier de conf.
*/
#define cairo_dock_update_conf_file_with_containers(pKeyFile, cDesktopFilePath) cairo_dock_update_conf_file_with_containers_full (pKeyFile, cDesktopFilePath, "Desktop Entry", "Container");
/**
* Met a jour un fichier de conf dd'applet avec la liste des docks dans le champ "dock name".
* @param pKeyFile fichier de conf ouvert.
* @param cDesktopFilePath chemin du fichier de conf.
*/
#define cairo_dock_update_applet_conf_file_with_containers(pKeyFile, cDesktopFilePath) cairo_dock_update_conf_file_with_containers_full (pKeyFile, cDesktopFilePath, "Icon", "dock name");


void cairo_dock_search_max_decorations_size (int *iWidth, int *iHeight);

/**
*Cache recursivement tous les dock peres d'un dock.
*@param pDock le dock fils.
*/
void cairo_dock_hide_parent_dock (CairoDock *pDock);
/**
*Cache recursivement tous les sous-docks fils d'un dock donne.
*@param pDock le dock parent.
*/
gboolean cairo_dock_hide_child_docks (CairoDock *pDock);
/**
*Recharge les buffers de toutes les icones de tous les docks.
*/
void cairo_dock_reload_buffers_in_all_docks (gboolean bReloadAppletsToo);
/**
* Renomme un dock dans la table des docks.
*@param cDockName nom du dock.
*@param pDock le dock (optionnel).
*@param cNewName son nouveau nom.
*@return le dock renomme.
*/
CairoDock *cairo_dock_alter_dock_name (const gchar *cDockName, CairoDock *pDock, const gchar *cNewName);
/**
* Renomme un dock. Met a jour le nom du container de ses icones.
*@param cDockName nom du dock.
*@param pDock le dock (optionnel).
*@param cNewName son nouveau nom.
*/
void cairo_dock_rename_dock (const gchar *cDockName, CairoDock *pDock, const gchar *cNewName);

void cairo_dock_reset_all_views (void);
void cairo_dock_set_all_views_to_default (int iDockType);


/**
* Ecrit les ecarts en x et en y d'un dock racine dans son fichier de conf.
*@param pDock le dock.
*/
void cairo_dock_write_root_dock_gaps (CairoDock *pDock);
/**
* Recupere le positionnement complet d'un dock racine a partir de son fichier de conf.
*@param cDockName nom du dock.
*@param pDock le dock.
*/
gboolean cairo_dock_get_root_dock_position (const gchar *cDockName, CairoDock *pDock);
/**
* Supprime le fichier de conf d'un dock racine.
*@param cDockName le nom du dock.
*/
void cairo_dock_remove_root_dock_config (const gchar *cDockName);

void cairo_dock_redraw_root_docks (gboolean bExceptMainDock);

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

void cairo_dock_show_hide_container (CairoContainer *pContainer);

void cairo_dock_foreach_icons (CairoDockForeachIconFunc pFunction, gpointer data);
void cairo_dock_foreach_docks (GHFunc pFunction);


G_END_DECLS
#endif
