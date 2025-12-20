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
*@param bNeedsTerminal Whether the command should be launched in a terminal.
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

/** Launch an application with an optional list of URIs or files to open.
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

/** Get the list of mime types that this app supports or NULL if unknown. */
const gchar** gldi_app_info_get_supported_types (GldiAppInfo *app);

/** Get a GldiAppInfo corresponding to the given GDesktopAppInfo. Also registers
* this app with the class manager if this has not been done before.
*@param pDesktopAppInfo a GDesktopAppInfo representing an app installed on the system.
*@return a GldiAppInfo that can be used to launch this app or NULL if pAppInfo is invalid.
* A reference is added and the caller should call gldi_object_unref () when done using it.
*/
GldiAppInfo *gldi_app_info_from_desktop_app_info (GDesktopAppInfo *pDesktopAppInfo);

/** Launch an application given by a GDesktopAppInfo with an optional list of URIs or
* files to open. The app will be registered with the class manager if it has not been
* seen yet.
*
*@param app a GDesktopAppInfo corresponding to an installed app.
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
void gldi_launch_desktop_app_info (GDesktopAppInfo *pDesktopAppInfo, const gchar* const *uris);

/** Override the setting whether this app needs to run in a terminal. */
void gldi_app_info_set_run_in_terminal (GldiAppInfo *app, gboolean bNeedsTerminal);

/*
* Ajoute un inhibiteur a une classe, et lui fait prendre immediatement le controle de la 1ere appli de cette classe trouvee, la detachant du dock. Rajoute l'indicateur si necessaire, et redessine le dock d'ou l'appli a ete enlevee, mais ne redessine pas l'icone inhibitrice.
* @param cClass la classe.
* @param pInhibitorIcon l'inhibiteur.
* @return TRUE si l'inhibiteur a bien ete rajoute a la classe.
*/
gboolean cairo_dock_inhibite_class (const gchar *cClass, Icon *pInhibitorIcon);

/*
* Empeche une icone d'inhiber sa classe; l'icone est enlevee de sa classe, son controle sur une appli est desactive, sa classe remise a 0, et l'appli controlee est inseree dans le dock.
* @param cClass la classe.
* @param pInhibitorIcon l'icone inhibitrice.
*/
void cairo_dock_deinhibite_class (const gchar *cClass, Icon *pInhibitorIcon);

Icon *cairo_dock_get_inhibitor (Icon *pIcon, gboolean bOnlyInDock);

const gchar *cairo_dock_get_class_name (const gchar *cClass);

const gchar *cairo_dock_get_class_icon (const gchar *cClass);


/** Get the app info associated with this class as a GldiAppInfo object.
* @param the class name to search for
* @return The app info or NULL if unknown. The caller should call gldi_object_ref ()
*   on the return value if it wishes to hang on to it.
*/
GldiAppInfo *cairo_dock_get_class_app_info (const gchar *cClass);

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

G_END_DECLS
#endif

