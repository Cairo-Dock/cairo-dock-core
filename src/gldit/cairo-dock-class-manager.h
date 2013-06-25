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


#ifndef __CAIRO_DOCK_CLASS_MANAGER__
#define  __CAIRO_DOCK_CLASS_MANAGER__

#include <X11/Xlib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-class-manager.h This class handles the managment of the applications classes.
* Classes are used to group the windows of a same program, and to bind a launcher to the launched application.
*/

/// Definition of a Class of application.
struct _CairoDockClassAppli {
	/// TRUE if the appli must use the icon provided by X instead the one from the theme.
	gboolean bUseXIcon;
	/// TRUE if the appli doesn't group togather with its class.
	gboolean bExpand;
	/// List of the inhibitors of the class.
	GList *pIconsOfClass;
	/// List of the appli icons of this class.
	GList *pAppliOfClass;
	gboolean bSearchedAttributes;
	gchar *cDesktopFile;
	gchar **pMimeTypes;
	gchar *cCommand;
	gchar *cStartupWMClass;
	gchar *cIcon;
	gchar *cName;
	gchar *cWorkingDirectory;
	GList *pMenuItems;
	gint iAge;  // age of the first created window of this class
	gchar *cDockName;  // unique name of the class sub-dock
};

/*
* Initialise le gestionnaire de classes. Ne fait rien la 2eme fois.
*/
void cairo_dock_initialize_class_manager (void);

/*
* Fournit la liste de toutes les applis connues du dock appartenant a cette classe.
* @param cClass la classe.
* @return la liste des applis de cettte classe.
*/
const GList *cairo_dock_list_existing_appli_with_class (const gchar *cClass);

CairoDock *cairo_dock_get_class_subdock (const gchar *cClass);

CairoDock* cairo_dock_create_class_subdock (const gchar *cClass, CairoDock *pParentDock);

/*
* Enregistre une icone de lanceur/applet dans sa classe. N'inhinibe pas la classe.
* @param pIcon l'inhibiteur.
* @return TRUE si l'enregistrement s'est effectue correctement ou si l'icone etait deja enregistree, FALSE sinon.
*/
//gboolean cairo_dock_add_inhibitor_to_class (const gchar *cClass, Icon *pIcon);
/*
* Enregistre une icone d'appli dans sa classe. Ne fais rien aux inhibiteurs.
* @param pIcon l'icone de l'appli.
* @return TRUE si l'enregistrement s'est effectue correctement ou si l'appli etait deja enregistree, FALSE sinon.
*/
gboolean cairo_dock_add_appli_icon_to_class (Icon *pIcon);
/*
* Desenregistre une icone d'appli de sa classe. Ne fais rien aux inhibiteurs.
* @param pIcon l'icone de l'appli.
* @return TRUE si le desenregistrement s'est effectue correctement ou si elle n'etait pas enregistree, FALSE sinon.
*/
gboolean cairo_dock_remove_appli_from_class (Icon *pIcon);
/*
* Force les applis d'une classe a utiliser ou non les icones fournies par X. Dans le cas ou elles ne les utilisent pas, elle utiliseront les memes icones que leur lanceur correspondant s'il existe. Recharge leur buffer en consequence, mais ne force pas le redessin.
* @param cClass la classe.
* @param bUseXIcon TRUE pour utiliser l'icone fournie par X, FALSE sinon.
* @return TRUE si l'etat a change, FALSE sinon.
*/
gboolean cairo_dock_set_class_use_xicon (const gchar *cClass, gboolean bUseXIcon);
/*
* Ajoute un inhibiteur a une classe, et lui fait prendre immediatement le controle de la 1ere appli de cette classe trouvee, la detachant du dock. Rajoute l'indicateur si necessaire, et redessine le dock d'ou l'appli a ete enlevee, mais ne redessine pas l'icone inhibitrice.
* @param cClass la classe.
* @param pInhibitorIcon l'inhibiteur.
* @return TRUE si l'inhibiteur a bien ete rajoute a la classe.
*/
gboolean cairo_dock_inhibite_class (const gchar *cClass, Icon *pInhibitorIcon);

/*
* Dis si une classe donnee est inhibee par un inhibiteur, libre ou non.
* @param cClass la classe.
* @return TRUE ssi les applis de cette classe sont inhibees.
*/
gboolean cairo_dock_class_is_inhibited (const gchar *cClass);
/*
* Dis si une classe donnee utilise les icones fournies par X.
* @param cClass la classe.
* @return TRUE ssi les applis de cette classe utilisent les icones de X.
*/
gboolean cairo_dock_class_is_using_xicon (const gchar *cClass);
/*
* Dis si une classe donnee peut etre groupee en sous-dock ou non.
* @param cClass la classe.
* @return TRUE ssi les applis de cette classe ne sont pas groupees.
*/
gboolean cairo_dock_class_is_expanded (const gchar *cClass);

/*
* Dis si une appli doit etre inhibee ou pas. Si un inhibiteur libre a ete trouve, il en prendra le controle, et TRUE sera renvoye. Un indicateur lui sera rajoute (ainsi qu'a l'icone du sous-dock si necessaire), et la geometrie de l'icone pour le WM lui est mise, mais il ne sera pas redessine. Dans le cas contraire, FALSE sera renvoye, et l'appli pourra etre inseree dans le dock.
* @param pIcon l'icone d'appli.
* @return TRUE si l'appli a ete inhibee.
*/
gboolean cairo_dock_prevent_inhibited_class (Icon *pIcon);

/*
* Enleve un inhibiteur de la classe donnee.
* @param pInhibitorIcon l'icone inhibitrice.
* @return TRUE ssi la classe est encore inhibee après l'enlèvement, FALSE sinon.
*/
//gboolean cairo_dock_remove_icon_from_class (Icon *pInhibitorIcon);
/*
* Empeche une icone d'inhiber sa classe; l'icone est enlevee de sa classe, son controle sur une appli est desactive, sa classe remise a 0, et l'appli controlee est inseree dans le dock.
* @param cClass la classe.
* @param pInhibitorIcon l'icone inhibitrice.
*/
void cairo_dock_deinhibite_class (const gchar *cClass, Icon *pInhibitorIcon);
/*
* Met a jour les inhibiteurs controlant une appli donnee pour leur en faire controler une autre.
* @param pAppli l'appli.
* @param cClass sa classe.
*/
void cairo_dock_detach_Xid_from_inhibitors (GldiWindowActor *pAppli, const gchar *cClass);
/*
* Enleve toutes les applis de toutes les classes, et met a jour les inhibiteurs.
*/
void cairo_dock_remove_all_applis_from_class_table (void);
/*
* Detruit toutes les classes, enlevant tous les inhibiteurs et toutes les applis de toutes les classes.
*/
void cairo_dock_reset_class_table (void);


cairo_surface_t *cairo_dock_duplicate_inhibitor_surface_for_appli (Icon *pInhibitorIcon, int iWidth, int ifHeight);
/*
* Cree la surface d'une appli en utilisant le lanceur correspondant, si la classe n'utilise pas les icones X.
* @param cClass la classe.
* @param pSourceContext un contexte de dessin, ne sera pas altere.
* @param fMaxScale zoom max.
* @param fWidth largeur de la surface, renseignee.
* @param fHeight hauteur de la surface, renseignee.
* @return la surface nouvellement creee, ou NULL si aucun lanceur n'a pu etre trouve ou si l'on veut explicitement les icones X pour cette classe.
*/
cairo_surface_t *cairo_dock_create_surface_from_class (const gchar *cClass, int iWidth, int ifHeight);

/*
* Met a jour les inhibiteurs controlant une appli donnee pour les faire reagir au changement de visibilite de la fenetre, de la meme maniere que si l'icone etait dans la barre des taches.
* @param cClass la classe.
* @param pAppli l'appli.
* @param bIsHidden TRUE ssi a fenetre vient de se cacher.
*/
void cairo_dock_update_visibility_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli, gboolean bIsHidden);
/*
* Met a jour les inhibiteurs controlant une appli donnee pour les faire reagir a la prise d'activite de la fenetre, de la meme maniere que si l'icone etait dans la barre des taches.
* @param cClass la classe.
* @param pAppli l'appli.
*/
void cairo_dock_update_activity_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli);
/*
* Met a jour les inhibiteurs controlant une appli donnee pour les redessiner en mode normal lors de la perte d'activite de la fenetre.
* @param cClass la classe.
* @param pAppli l'appli.
*/
void cairo_dock_update_inactivity_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli);

void cairo_dock_update_name_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli, gchar *cNewName);

Icon *cairo_dock_get_classmate (Icon *pIcon);

gboolean cairo_dock_check_class_subdock_is_empty (CairoDock *pDock, const gchar *cClass);


void cairo_dock_set_overwrite_exceptions (const gchar *cExceptions);

void cairo_dock_set_group_exceptions (const gchar *cExceptions);


Icon *cairo_dock_get_prev_next_classmate_icon (Icon *pIcon, gboolean bNext);

Icon *cairo_dock_get_inhibitor (Icon *pIcon, gboolean bOnlyInDock);


void cairo_dock_set_class_order_in_dock (Icon *pIcon, CairoDock *pDock);

void cairo_dock_set_class_order_amongst_applis (Icon *pIcon, CairoDock *pDock);


const gchar *cairo_dock_get_class_command (const gchar *cClass);

const gchar *cairo_dock_get_class_name (const gchar *cClass);

const gchar **cairo_dock_get_class_mimetypes (const gchar *cClass);

const gchar *cairo_dock_get_class_desktop_file (const gchar *cClass);

const gchar *cairo_dock_get_class_icon (const gchar *cClass);

const GList *cairo_dock_get_class_menu_items (const gchar *cClass);

const gchar *cairo_dock_get_class_wm_class (const gchar *cClass);

const CairoDockImageBuffer *cairo_dock_get_class_image_buffer (const gchar *cClass);


gchar *cairo_dock_guess_class (const gchar *cCommand, const gchar *cStartupWMClass);

gchar *cairo_dock_register_class_full (const gchar *cDesktopFile, const gchar *cClassName, const gchar *cWmClass);

/** Register a class corresponding to a desktop file. Launchers can then derive from the class.
* @param cDesktopFile the desktop file path or name; if it's a name or if the path couldn't be found, it will be searched in the common directories.
* @return the class ID in a newly allocated string.
*/
#define cairo_dock_register_class(cDesktopFile) cairo_dock_register_class_full (cDesktopFile, NULL, NULL)

/** Make a launcher derive from a class. Parameters of the icon that are not NULL are not overwritten.
* @param cClass the class name
* @param pIcon the icon
*/
void cairo_dock_set_data_from_class (const gchar *cClass, Icon *pIcon);

G_END_DECLS
#endif
