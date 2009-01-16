
#ifndef __CAIRO_DOCK_ICONS__
#define  __CAIRO_DOCK_ICONS__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*TRUE ssi l'icone est une icone de lanceur.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_LAUNCHER(icon) (icon != NULL && (icon->acCommand != NULL || (icon->pSubDock != NULL && icon->pModuleInstance == NULL && icon->Xid == 0)))
/**
*TRUE ssi l'icone est une icone d'appli.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_APPLI(icon) (icon != NULL && icon->Xid != 0)
/**
*TRUE ssi l'icone est une icone d'applet.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_APPLET(icon) (icon != NULL && icon->pModuleInstance != NULL)
/**
*TRUE ssi l'icone est une icone de separateur.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_SEPARATOR(icon) (icon != NULL && (icon->acName == NULL && icon->pModuleInstance == NULL && icon->Xid == 0))  /* (icon->iType & 1) || */

/**
*TRUE ssi l'icone est une icone de lanceur defini par un fichier .desktop.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_NORMAL_LAUNCHER(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && icon->acDesktopFileName != NULL)
/**
*TRUE ssi l'icone est une icone de lanceur representant une URI.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_URI_LAUNCHER(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && icon->cBaseURI != NULL)
/**
*TRUE ssi l'icone est une icone de separateur ajoutee automatiquement.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR(icon) (CAIRO_DOCK_IS_SEPARATOR (icon) && icon->acDesktopFileName == NULL)
/**
*TRUE ssi l'icone est une icone de separateur ajoutee par l'utilisateur.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_USER_SEPARATOR(icon) (CAIRO_DOCK_IS_SEPARATOR (icon) && icon->acDesktopFileName != NULL)
/**
*TRUE ssi l'icone est une icone d'appli seulement.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_NORMAL_APPLI(icon) (CAIRO_DOCK_IS_APPLI (icon) && icon->acDesktopFileName == NULL && icon->pModuleInstance == NULL)
/**
*TRUE ssi l'icone est une icone d'applet detachable en desklet.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_DETACHABLE_APPLET(icon) (CAIRO_DOCK_IS_APPLET (icon) && icon->pModuleInstance->bCanDetach)

/**
* Desactive une icone, et libere tous ses buffers, ainsi qu'elle-meme. Le sous-dock pointee par elle n'est pas dereferencee, cela doit etre fait au prealable. Gere egalement la classe.
*@param icon l'icone a liberer.
*/
void cairo_dock_free_icon (Icon *icon);
/**
* Libere tous les buffers d'une incone.
*@param icon l'icone.
*/
void cairo_dock_free_icon_buffers (Icon *icon);


#define cairo_dock_get_group_order(iType) (iType < CAIRO_DOCK_NB_TYPES ? myIcons.tIconTypeOrder[iType] : iType)
#define cairo_dock_get_icon_order(icon) cairo_dock_get_group_order (icon->iType)

/**
*Donne le type d'une icone relativement a son contenu.
*@param icon l'icone.
*/
CairoDockIconType cairo_dock_get_icon_type (Icon *icon);
/**
*Compare 2 icones grace a la relation d'ordre sur le couple (position du type , ordre).
*@param icon1 une icone.
*@param icon2 une autre icone.
*@return -1 si icone1 < icone2, 1 si icone1 > icone2, 0 si icone1 = icone2 (au sens de la relation d'ordre).
*/
int cairo_dock_compare_icons_order (Icon *icon1, Icon *icon2);
/**
*Compare 2 icones grace a la relation d'ordre sur le nom (ordre alphabetique sur les noms passes en minuscules).
*@param icon1 une icone.
*@param icon2 une autre icone.
*@return -1 si icone1 < icone2, 1 si icone1 > icone2, 0 si icone1 = icone2 (au sens de la relation d'ordre).
*/
int cairo_dock_compare_icons_name (Icon *icon1, Icon *icon2);
/**
*Compare 2 icones grace a la relation d'ordre sur l'extension des URIs (ordre alphabetique compte en passant en minuscules).
*@param icon1 une icone.
*@param icon2 une autre icone.
*@return -1 si icone1 < icone2, 1 si icone1 > icone2, 0 si icone1 = icone2 (au sens de la relation d'ordre).
*/
int cairo_dock_compare_icons_extension (Icon *icon1, Icon *icon2);

/**
*Trie une liste en se basant sur la relation d'ordre sur le couple (position du type , ordre).
*@param pIconList la liste d'icones.
*@return la liste triee. Les elements sont les memes que ceux de la liste initiale, seul leur ordre a change.
*/
GList *cairo_dock_sort_icons_by_order (GList *pIconList);
/**
*Trie une liste en se basant sur la relation d'ordre alphanumerique sur le nom des icones.
*@param pIconList la liste d'icones.
*@return la liste triee. Les elements sont les memes que ceux de la liste initiale, seul leur ordre a change. Les ordres des icones sont mis a jour pour refleter le nouvel ordre global.
*/
GList *cairo_dock_sort_icons_by_name (GList *pIconList);

/**
*Renvoie la 1ere icone d'une liste d'icones.
*@param pIconList la liste d'icones.
*@return la 1ere icone, ou NULL si la liste est vide.
*/

Icon *cairo_dock_get_first_icon (GList *pIconList);
/**
*Renvoie la derniere icone d'une liste d'icones.
*@param pIconList la liste d'icones.
*@return la derniere icone, ou NULL si la liste est vide.
*/
Icon *cairo_dock_get_last_icon (GList *pIconList);
/**
*Renvoie la 1ere icone a etre dessinee dans un dock (qui n'est pas forcement la 1ere icone de la liste, si l'utilisateur a scrolle).
*@param pDock le dock.
*@return la 1ere icone a etre dessinee, ou NULL si la liste est vide.
*/
Icon *cairo_dock_get_first_drawn_icon (CairoDock *pDock);
/**
*Renvoie la derniere icone a etre dessinee dans un dock (qui n'est pas forcement la derniere icone de la liste, si l'utilisateur a scrolle).
*@param pDock le dock.
*@return la derniere icone a etre dessinee, ou NULL si la liste est vide.
*/
Icon *cairo_dock_get_last_drawn_icon (CairoDock *pDock);
/**
*Renvoie la 1ere icone du type donne.
*@param pIconList la liste d'icones.
*@param iType le type d'icone recherche.
*@return la 1ere icone trouvee ayant ce type, ou NULL si aucune icone n'est trouvee.
*/
Icon *cairo_dock_get_first_icon_of_type (GList *pIconList, CairoDockIconType iType);
/**
*Renvoie la derniere icone du type donne.
*@param pIconList la liste d'icones.
*@param iType le type d'icone recherche.
*@return la derniere icone trouvee ayant ce type, ou NULL si aucune icone n'est trouvee.
*/
Icon *cairo_dock_get_last_icon_of_type (GList *pIconList, CairoDockIconType iType);
/**
*Renvoie la 1ere icone dont le groupe correspond.
*@param pIconList la liste d'icones.
*@param iType le type/groupe dont on recherche l'ordre.
*@return la 1ere icone trouvee ayant cet ordre, ou NULL si aucune icone n'est trouvee.
*/
Icon* cairo_dock_get_first_icon_of_order (GList *pIconList, CairoDockIconType iType);
/**
*Renvoie la derniere icone dont le groupe correspond.
*@param pIconList la liste d'icones.
*@param iType le type/groupe d'icone recherche.
*@return la derniere icone trouvee ayant cet ordre, ou NULL si aucune icone n'est trouvee.
*/
Icon* cairo_dock_get_last_icon_of_order (GList *pIconList, CairoDockIconType iType);
/**
*Renvoie l'icone actuellement pointee parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@return l'icone dont le champ bPointed a TRUE, ou NULL si aucune icone n'est pointee.
*/
Icon *cairo_dock_get_pointed_icon (GList *pIconList);
/**
*Renvoie l'icone actuellement en cours d'insertion ou de suppression parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@return la 1ere icone dont le champ fPersonnalScale est non nul ou NULL si aucune icone n'est en cours d'insertion / suppression.
*/
Icon *cairo_dock_get_removing_or_inserting_icon (GList *pIconList);
/**
*Renvoie l'icone suivante dans la liste d'icones. Cout en O(n).
*@param pIconList la liste d'icones.
*@param pIcon l'icone dont on veut le voisin.
*@return l'icone dont le voisin de gauche est pIcon, ou NULL si pIcon est la derniere icone de la liste.
*/
Icon *cairo_dock_get_next_icon (GList *pIconList, Icon *pIcon);
/**
*Renvoie l'icone precedente dans la liste d'icones. Cout en O(n).
*@param pIconList la liste d'icones.
*@param pIcon l'icone dont on veut le voisin.
*@return l'icone dont le voisin de droite est pIcon, ou NULL si pIcon est la 1ere icone de la liste.
*/
Icon *cairo_dock_get_previous_icon (GList *pIconList, Icon *pIcon);
/**
*Renvoie le prochain element dans la liste, en bouclant si necessaire.
*@param ic l'element courant.
*@param list la liste d'icones.
*@return l'element suivant de la liste bouclee.
*/
#define cairo_dock_get_next_element(ic, list) (ic->next == NULL ? list : ic->next)
/**
*Renvoie l'element precedent dans la liste, en bouclant si necessaire.
*@param ic l'element courant.
*@param list la liste d'icones.
*@return l'element precedent de la liste bouclee.
*/
#define cairo_dock_get_previous_element(ic, list) (ic->prev == NULL ? g_list_last (list) : ic->prev)
/**
*Cherche l'icone ayant une commande donnee parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param cCommand la chaine de commande.
*@return la 1ere icone ayant le champ 'acExec' identique a la commande fournie, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_command (GList *pIconList, gchar *cCommand);
/**
*Cherche l'icone ayant une URI de base donnee parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param cBaseURI l'URI.
*@return la 1ere icone ayant le champ 'cBaseURI' identique a l'URI fournie, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_base_uri (GList *pIconList, const gchar *cBaseURI);

Icon *cairo_dock_get_icon_with_name (GList *pIconList, const gchar *cName);

/**
*Cherche l'icone pointant sur un sous-dock donne parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param pSubDock le sous-dock.
*@return la 1ere icone ayant le champ 'pSubDock' identique au sous-dock fourni, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_subdock (GList *pIconList, CairoDock *pSubDock);
/**
*Cherche l'icone correspondante a un module donne parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param pModule le module.
*@return la 1ere icone ayant le champ 'pModule' identique au module fourni, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_module (GList *pIconList, CairoDockModule *pModule);
/**
*Cherche l'icone d'une application de classe donnee parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param cClass la classe d'application.
*@return la 1ere icone ayant le champ 'cClass' identique a la classe fournie, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_class (GList *pIconList, gchar *cClass);

#define cairo_dock_get_first_launcher(pIconList) cairo_dock_get_first_icon_of_type (pIconList, CAIRO_DOCK_LAUNCHER)
#define cairo_dock_get_last_launcher(pIconList) cairo_dock_get_last_icon_of_type (pIconList, CAIRO_DOCK_LAUNCHER)
#define cairo_dock_get_first_appli(pIconList) cairo_dock_get_first_icon_of_type (pIconList, CAIRO_DOCK_APPLI)
#define cairo_dock_get_last_appli(pIconList) cairo_dock_get_last_icon_of_type (pIconList, CAIRO_DOCK_APPLI)


void cairo_dock_normalize_icons_order (GList *pIconList, CairoDockIconType iType);
void cairo_dock_swap_icons (CairoDock *pDock, Icon *icon1, Icon *icon2);
void cairo_dock_move_icon_after_icon (CairoDock *pDock, Icon *icon1, Icon *icon2);

/**
*Detache une icone de son dock, en enlevant les separateurs superflus si necessaires. L'icone n'est pas detruite, et peut etre re-inseree autre part telle qu'elle; elle garde son sous-dock, mais perd son dialogue.
*@param icon l'icone a detacher.
*@param pDock le dock contenant l'icone.
*@param bCheckUnusedSeparator si TRUE, alors teste si des separateurs sont devenus superflus, et les enleve le cas echeant.
*@return TRUE ssi l'icone a effectivement ete enlevee, FALSE si elle l'etait deja.
*/
gboolean cairo_dock_detach_icon_from_dock (Icon *icon, CairoDock *pDock, gboolean bCheckUnusedSeparator);
/**
*Detache une icone de son dock, sans verifier la presence de separateurs superflus. L'icone n'est pas detruite, et garde son sous-dock, mais perd son dialogue et est fermee (son .desktop est detruit, son module est desactive, et son Xid est effacee du registre (la classe est geree aussi)).
*@param pDock le dock contenant l'icone.
*@param icon l'icone a detacher.
*/
void cairo_dock_remove_one_icon_from_dock (CairoDock *pDock, Icon *icon);
/**
*Detache une icone de son dock, en enlevant les separateurs superflus si necessaires. L'icone n'est pas detruite, et garde son sous-dock, mais perd son dialogue et est fermee (son .desktop est detruit, son module est desactive, et son Xid est effacee du registre (la classe est geree aussi)).
*@param pDock le dock contenant l'icone.
*@param icon l'icone a detacher.
*/
void cairo_dock_remove_icon_from_dock (CairoDock *pDock, Icon *icon);
/**
*Enleve et detruit toutes les icones dont le type est celui fourni.
*@param pDock le dock contenant l'icone.
*@param iType le type d'icones a supprimer.
*/
void cairo_dock_remove_icons_of_type (CairoDock *pDock, CairoDockIconType iType);
/**
*Enleve et detruit toutes les icones d'applications.
*@param pDock le dock duquel supprimer les icones.
*/
#define cairo_dock_remove_all_applis(pDock) cairo_dock_remove_icons_of_type (pDock, CAIRO_DOCK_APPLI)
/**
*Enleve et detruit toutes les icones d'applets.
*@param pDock le dock duquel supprimer les icones.
*/
#define cairo_dock_remove_all_applets(pDock) cairo_dock_remove_icons_of_type (pDock, CAIRO_DOCK_APPLET)
/**
*Effectue une action sur toutes les icones d'un type donne. L'action peut meme detruire et enlever de la liste l'icone courante.
*@param pIconList la liste d'icones a parcourir.
*@param iType le type d'icone.
*@param pFuntion l'action a effectuer sur chaque icone.
*@param data un pointeur qui sera passe en entree de l'action.
*@return le separateur avec le type a gauche si il y'en a, NULL sinon.
*/
Icon *cairo_dock_foreach_icons_of_type (GList *pIconList, CairoDockIconType iType, CairoDockForeachIconFunc pFuntion, gpointer data);
/**
*Enleve et detruit toutes les icones de separateurs automatiques.
*@param pDock le dock duquel supprimer les icones.
*/
void cairo_dock_remove_all_separators (CairoDock *pDock);

/**
*Ajoute des separateurs automatiques entre les differents types d'icones.
*@param pDock le dock auquel rajouter les separateurs.
*/
void cairo_dock_insert_separators_in_dock (CairoDock *pDock);


GList * cairo_dock_calculate_icons_positions_at_rest_linear (GList *pIconList, double fFlatDockWidth, int iXOffset);

void cairo_dock_update_removing_inserting_icon_size_default (Icon *icon);

Icon * cairo_dock_calculate_wave_with_position_linear (GList *pIconList, GList *pFirstDrawnElement, int x_abs, gdouble fMagnitude, double fFlatDockWidth, int iWidth, int iHeight, double fAlign, double fLateralFactor, gboolean bDirectionUp);

Icon *cairo_dock_apply_wave_effect (CairoDock *pDock);

void cairo_dock_check_if_mouse_inside_linear (CairoDock *pDock);

void cairo_dock_manage_mouse_position (CairoDock *pDock);


double cairo_dock_calculate_max_dock_width (CairoDock *pDock, GList *pFirstDrawnElement, double fFlatDockWidth, double fWidthConstraintFactor, double fExtraWidth);


void cairo_dock_check_can_drop_linear (CairoDock *pDock);
void cairo_dock_stop_marking_icons (CairoDock *pDock);

/**
*Met a jour le fichier .desktop d'un lanceur avec le nom de son nouveau conteneur.
*@param icon l'icone du lanceur.
*@param cNewParentDockName le nom de son nouveau conteneur.
*/
void cairo_dock_update_icon_s_container_name (Icon *icon, const gchar *cNewParentDockName);

#define cairo_dock_set_icon_static(icon) ((icon)->bStatic = TRUE)


G_END_DECLS
#endif

