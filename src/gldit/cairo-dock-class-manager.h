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

#include <gio/gdesktopappinfo.h>
#include "cairo-dock-struct.h"
#include "cairo-dock-object.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-class-manager.h This class handles the managment of the applications classes.
* Classes are used to group the windows of a same program, and to bind a launcher to the launched application.
*/

/**
*@struct GldiAppInfo Helper object for launching apps, wrapping a GAppInfo.
* This is needed as unfortunately GDesktopAppInfo does not provide all the
* functionality we need.
* 
* GldiAppInfo derives from GlidObject, so you should use gldi_object_ref () /
* gldi_object_unref () to manage its lifecycle.
* 
* Note: currently, the class-manager automatically creates a GldiAppInfo
* for all apps registered with it, filling out the corresponding icon's
* pAppInfo member as it is created, so there is no need to create this
* object manually. A constructor is provided only to create a GldiAppInfo
* wrapping a custom commnad to run.
*/


/** Create a GldiAppInfo that can be used to start the given command.
*@param cCmdline Command to launch in the format of the XDG Desktop Entry specification.
*@param cName Descriptive name that is suitable to be displayed to the user.
*@param cWorkingDir Optionally, a directory where the command should be launched.
*@param bNeedsTerminal Whether the command should be launched in a terminal. Currently unsupported and ignored.
*@return the newly created GldiAppInfo or NULL if there was an error parsing cCmdline.
*/
GldiAppInfo *gldi_app_info_new_from_commandline (const gchar *cCmdline, const gchar *cName, const gchar *cWorkingDir, gboolean bNeedsTerminal);

/** Launch one of the extra actions supported by this app.
*@param app a GldiAppInfo corresponding to an installed app.
*@param cAction one of the additional actions supported by the app. The must
*  be one of the entries returneed by gldi_app_info_get_desktop_actions ().
* 
* Note: if the app supports DBus activation, it will be used instead of
* directly launching it. See here for more details:
* https://specifications.freedesktop.org/desktop-entry-spec/latest/dbus.html
*
* Apps that do not support DBus activation might be launched directly as a child
* process of Cairo-Dock, or indirectly via the session manager if available
* (i.e. systemd on Linux).
*/
void gldi_app_info_launch_action (GldiAppInfo *app, const gchar *cAction);

/** Launch the application with an optional list of URIs or files to open.
*
*@param app a GldiAppInfo corresponding to an installed app or available command.
*@param uris a NULL-terminated list of file names or URIs to provide as parameters or NULL.
*
* Note: if the app supports DBus activation, it will be used instead of
* directly launching it. See here for more details:
* https://specifications.freedesktop.org/desktop-entry-spec/latest/dbus.html
* 
* Apps that do not support DBus activation might be launched directly as a child
* process of Cairo-Dock, or indirectly via the session manager if available
* (i.e. systemd on Linux).
*/
void gldi_app_info_launch (GldiAppInfo *app, const gchar* const *uris);

/** Get a list of additional actions supported by this app. See
* https://specifications.freedesktop.org/desktop-entry-spec/latest/extra-actions.html
* for a description of extra actions.
*
*@param app a GldiAppInfo corresponding to an installed app.
*@return The list of additional action names as a NULL-terminated array or
* NULL if no additional actions are supported. The returned list and its
* contents are owned by the instance and should not be modified or freed
* by the caller.
*/ 
const gchar * const *gldi_app_info_get_desktop_actions (GldiAppInfo *app);

/** Get the name of an additional action supported by this app that is
* suitable to display to the user (i.e. translated whenever possible).
*
*@param app a GldiAppInfo corresponding to an installed app.
*@param cAction one of the additional actions supported by the app. The must
*  be one of the entries returneed by gldi_app_info_get_desktop_actions ().
*@returns the name of the action in a newly allocated string or NULL of cAction
* is invalid. The caller takes ownership of the return value and is responsible
* for freeing it.
*/
gchar *gldi_app_info_get_desktop_action_name (GldiAppInfo *app, const gchar *cAction);

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
void gldi_window_detach_from_inhibitors (GldiWindowActor *pAppli);
/*
* Enleve toutes les applis de toutes les classes, et met a jour les inhibiteurs.
*/
void cairo_dock_remove_all_applis_from_class_table (void);
/*
* Detruit toutes les classes, enlevant tous les inhibiteurs et toutes les applis de toutes les classes.
*/
void cairo_dock_reset_class_table (void);

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


/** Run a function on each Icon that inhibites a given window.
*@param actor the window actor
*@param callback function to be called
*@param data data passed to the callback
*/
void gldi_window_foreach_inhibitor (GldiWindowActor *actor, GldiIconRFunc callback, gpointer data);


Icon *cairo_dock_get_classmate (Icon *pIcon);

gboolean cairo_dock_check_class_subdock_is_empty (CairoDock *pDock, const gchar *cClass);

void cairo_dock_update_class_subdock_name (const CairoDock *pDock, const gchar *cNewName);


void cairo_dock_set_overwrite_exceptions (const gchar *cExceptions);

void cairo_dock_set_group_exceptions (const gchar *cExceptions);


Icon *cairo_dock_get_prev_next_classmate_icon (Icon *pIcon, gboolean bNext);

Icon *cairo_dock_get_inhibitor (Icon *pIcon, gboolean bOnlyInDock);


void cairo_dock_set_class_order_in_dock (Icon *pIcon, CairoDock *pDock);

void cairo_dock_set_class_order_amongst_applis (Icon *pIcon, CairoDock *pDock);


const gchar *cairo_dock_get_class_name (const gchar *cClass);

const gchar *cairo_dock_get_class_desktop_file (const gchar *cClass);

const gchar *cairo_dock_get_class_icon (const gchar *cClass);

const gchar *cairo_dock_get_class_wm_class (const gchar *cClass);

/** Get the app info associated with this class as a GldiAppInfo object.
* @param the class name to search for
* @return The app info or NULL if unknown. The caller should call gldi_object_ref ()
*   on the return value if it wishes to hang on to it.
*/
GldiAppInfo *cairo_dock_get_class_app_info (const gchar *cClass);

const CairoDockImageBuffer *cairo_dock_get_class_image_buffer (const gchar *cClass);

const gchar* const *cairo_dock_get_class_actions (const gchar *cClass);


gchar *cairo_dock_guess_class (const gchar *cCommand, const gchar *cStartupWMClass);


/** Register an application class from apps installed on the system or find an already registered one.
* @param cSearchTerm query to search for among installed apps (see below for details).
* @param cWmClass StartupWMClass key from a custom launcher to add as an additional key to find this class later.
* @param bCreateAlways if TRUE, a new class is always created with cSearchTerm as its key.
* @return the class ID in a newly allocated string (can be used to retrieve class properties later).
* 
* The cSearchTerm supplied to this function should be either:
*  - a desktop file path which is opened if it exists; if not, the file basename is searched among the
*    desktop IDs of installed apps in a case-insensitive way, but no other heuristics is attempted
*  - a desktop file ID (i.e. a string not starting with "/" and ending with ".desktop"); it is searched
*    among the desktop IDs of installed apps (case-insensitive); if not found, a heuristic search is
*    also attempted
*  - a class name / app-id (from the StartupWMClass key of a launcher) or a command name; this is assumed
*    to be already lowercase, and it is searched among installed apps using heuristics
* 
* Heuristics applied to search are the following:
*  - search among desktop file IDs by applying common suffices (org.gnome., org.kde., org.freedesktop)
*  - duplicating the name (e.g. "firefox" -> "firefox_firefox.desktop", required for Snap)
*  - searching the content of the StartupWMClass or the Exec key (if StartupWMClass is not present)
*    in the .desktop file
* 
* The cWmClass parameter is not used for searching among installed apps, but it is added without any
* modification as an additional key for this class for later retrieval (so it is useful if the app
* uses a WMClass / app-id that is not possible to find with our heuristics).
* 
* The bCreateAlways controls whether cSearchTerm is always used to create a class:
*  - if bCreateAlways == FALSE, and no result is found, no class is created and NULL is returned
*  - if bCreateAlways == TRUE, and an app is found, a class is created as normal, but it is also ensured
*    that cSearchTerm is added as a key for retrieval (in this case, the return value will be cSearchTerm)
*  - if bCreateAlways == TRUE, and no result is found, a "dummy" class is created and registered; this
*    should be used as a last resort to ensure that a launcher has a class registered
*  - if bIsDesktopFile == TRUE, cSearchTerm is assumed to be the name of a .desktop file and is processed accordingly
*    (the .desktop suffix removed and converted to lowercase)
*/
gchar *cairo_dock_register_class2 (const gchar *cSearchTerm, const gchar *cWmClass, gboolean bCreateAlways, gboolean bIsDesktopFile);

/** Register an application class from apps installed on the system.
* @param cSearchTerm query to search for among installed apps
* @return the class ID in a newly allocated string (can be used to retrieve class properties later).
* 
* This function behaves as cairo_dock_register_class2(cSearchTerm, NULL, FALSE).
*/
gchar *cairo_dock_register_class (const gchar *cSearchTerm);

/** Make a launcher derive from a class. Parameters of the icon that are not NULL are not overwritten.
* @param cClass the class name
* @param pIcon the icon
*/
void cairo_dock_set_data_from_class (const gchar *cClass, Icon *pIcon);


void gldi_class_startup_notify (Icon *pIcon);

void gldi_class_startup_notify_end (const gchar *cClass);

gboolean gldi_class_is_starting (const gchar *cClass);

void gldi_register_class_manager (void);

G_END_DECLS
#endif
