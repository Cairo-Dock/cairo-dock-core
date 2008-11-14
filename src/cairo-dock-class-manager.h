
#ifndef __CAIRO_DOCK_CLASS_MANAGER__
#define  __CAIRO_DOCK_CLASS_MANAGER__

#include <X11/Xlib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
* Initialise le gestionnaire de classes. Ne fait rien la 2eme fois.
*/
void cairo_dock_initialize_class_manager (void);

/**
* Libere une classe d'appli, en enlevant au passage tous les indicateurs des inhibiteurs de cette classe.
* @param pClassAppli la classe d'appli.
*/
void cairo_dock_free_class_appli (CairoDockClassAppli *pClassAppli);
/**
* Fournit la liste de toutes les applis connues du dock appartenant a cette classe.
* @param cClass la classe.
* @return la liste des applis de cettte classe.
*/
const GList *cairo_dock_list_existing_appli_with_class (const gchar *cClass);

/**
* Enregistre une icone d'appli dans sa classe. Ne fais rien aux inhibiteurs.
* @param pIcon l'icone de l'appli.
* @return TRUE si l'enregistrement s'est effectue correctement ou si l'appli etait deja enregistree, FALSE sinon.
*/
gboolean cairo_dock_add_appli_to_class (Icon *pIcon);
/**
* Desenregistre une icone d'appli de sa classe. Ne fais rien aux inhibiteurs.
* @param pIcon l'icone de l'appli.
* @return TRUE si le desenregistrement s'est effectue correctement ou si elle n'etait pas enregistree, FALSE sinon.
*/
gboolean cairo_dock_remove_appli_from_class (Icon *pIcon);
/**
* Force les applis d'une classe a utiliser ou non les icones fournies par X. Dans le cas ou elles ne les utilisent pas, elle utiliseront les memes icones que leur lanceur correspondant s'il existe. Recharge leur buffer en consequence, mais ne force pas le redessin.
* @param cClass la classe.
* @param bUseXIcon TRUE pour utiliser l'icone fournie par X, FALSE sinon.
* @return TRUE si l'etat a change, FALSE sinon.
*/
gboolean cairo_dock_set_class_use_xicon (const gchar *cClass, gboolean bUseXIcon);
/**
* Ajoute un inhibiteur a une classe, et lui fait prendre immediatement le controle de la 1ere appli de cette classe trouvee, la detachant du dock. Rajoute l'indicateur si necessaire, et redessine le dock d'ou l'appli a ete enlevee, mais ne redessine pas l'icone inhibitrice.
* @param cClass la classe.
* @param pInhibatorIcon l'inhibiteur.
* @return TRUE si l'inhibiteur a bien ete rajoute a la classe.
*/
gboolean cairo_dock_inhibate_class (const gchar *cClass, Icon *pInhibatorIcon);

/**
* Dis si une classe donnee est inhibee par un inhibiteur, libre ou non.
* @return TRUE ssi les applis de cette classe sont inhibees.
*/
gboolean cairo_dock_class_is_inhibated (const gchar *cClass);
/**
* Dis si une classe donnee utilise les icones fournies par X.
* @return TRUE ssi les applis de cette classe utilisent les icones de X.
*/
gboolean cairo_dock_class_is_using_xicon (const gchar *cClass);
/**
* Dis si une appli doit etre inhibee ou pas. Si un inhibiteur libre a ete trouve, il en prendra le controle, et TRUE sera renvoye. Un indicateur lui sera rajoute (ainsi qu'a l'icone du sous-dock si necessaire), et la geometrie de l'icone pour le WM lui est mise, mais il ne sera pas redessine. Dans le cas contraire, FALSE sera renvoye, et l'appli pourra etre inseree dans le dock.
* @param pIcon l'icone d'appli.
* @return TRUE si l'appli a ete inhibee.
*/
gboolean cairo_dock_prevent_inhibated_class (Icon *pIcon);

/**
* Enleve un inhibiteur de la classe donnee.
* @param pInhibatorIcon l'icone inhibitrice.
* @return TRUE ssi la classe est encore inhibee après l'enlèvement, FALSE sinon.
*/
gboolean cairo_dock_remove_icon_from_class (Icon *pInhibatorIcon);
/**
* Empeche une icone d'inhiber sa classe; l'icone est enlevee de sa classe, son controle sur une appli est desactive, sa classe remise a 0, et l'appli controlee est inseree dans le dock.
* @param cClass la classe.
* @param pInhibatorIcon l'icone inhibitrice.
*/
void cairo_dock_deinhibate_class (const gchar *cClass, Icon *pInhibatorIcon);
/**
* Met a jour les inhibiteurs controlant une appli donnee pour leur en faire controler une autre.
* @param Xid l'ID de l'appli.
* @param cClass sa classe.
*/
void cairo_dock_update_Xid_on_inhibators (Window Xid, const gchar *cClass);
/**
* Enleve toutes les applis de toutes les classes, et met a jour les inhibiteurs.
*/
void cairo_dock_remove_all_applis_from_class_table (void);
/**
* Detruit toutes les classes, enlevant tous les inhibiteurs et toutes les applis de toutes les classes.
*/
void cairo_dock_reset_class_table (void);

/**
* Cree la surface d'une appli en utilisant le lanceur correspondant, si la classe n'utilise pas les icones X.
* @param cClass la classe.
* @param pSourceContext un contexte de dessin, ne sera pas altere.
* @param fMaxScale zoom max.
* @param fWidth largeur de la surface, renseignee.
* @param fHeight hauteur de la surface, renseignee.
* @return la surface nouvellement creee, ou NULL si aucun lanceur n'a pu etre trouve ou si l'on veut explicitement les icones X pour cette classe.
*/
cairo_surface_t *cairo_dock_create_surface_from_class (gchar *cClass, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight);

/**
* Met a jour les inhibiteurs controlant une appli donnee pour les faire reagir au changement de visibilite de la fenetre, de la meme maniere que si l'icone etait dans la barre des taches.
* @param cClass la classe.
* @param Xid l'ID de l'appli.
* @param bIsHidden TRUE ssi a fenetre vient de se cacher.
*/
void cairo_dock_update_visibility_on_inhibators (gchar *cClass, Window Xid, gboolean bIsHidden);
/**
* Met a jour les inhibiteurs controlant une appli donnee pour les faire reagir a la prise d'activite de la fenetre, de la meme maniere que si l'icone etait dans la barre des taches.
* @param cClass la classe.
* @param Xid l'ID de l'appli.
*/
void cairo_dock_update_activity_on_inhibators (gchar *cClass, Window Xid);
/**
* Met a jour les inhibiteurs controlant une appli donnee pour les redessiner en mode normal lors de la perte d'activite de la fenetre.
* @param cClass la classe.
* @param Xid l'ID de l'appli.
*/
void cairo_dock_update_inactivity_on_inhibators (gchar *cClass, Window Xid);

Icon *cairo_dock_get_classmate (Icon *pIcon);

G_END_DECLS
#endif
