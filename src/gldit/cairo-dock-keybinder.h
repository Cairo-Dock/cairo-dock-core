/*
* cairo-dock-keybinder.h
* This file is a part of the Cairo-Dock project
* Login : <ctaf42@localhost.localdomain>
* Started on  Thu Jan 31 03:57:17 2008 Cedric GESTES
* $Id$
*
* Author(s)
*  - Cedric GESTES <ctaf42@gmail.com>
*  - Havoc Pennington
*  - Tim Janik
*
* Copyright : (C) 2008 Cedric GESTES
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
*
* imported from tomboy_key_binder.h
*/

#ifndef __CD_KEY_BINDER_H__
#define __CD_KEY_BINDER_H__

#include <glib/gtypes.h>
#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-keybinder.h This class contains functions to easily bind a keyboard shortcut to an action. These shortkeys are defined globally in your session, that is to say they will be effective whatever window has the focus.
* Shortkeys are of the form &lt;alt&gt;F1 or &lt;ctrl&gt;&lt;shift&gt;s.
* 
* You bind an action to a shortkey with \ref cd_keybinder_bind, and unbind it with \ref cd_keybinder_unbind.
*/


/// Definition of a callback, called when a shortcut is pressed by the user.
typedef void (* CDBindkeyHandler) (const gchar *keystring, gpointer user_data);

struct _CairoKeyBinding {
	gchar            *keystring;
	CDBindkeyHandler  handler;
	gpointer          user_data;
	guint             keycode;
	guint             modifiers;
	gboolean          bSuccess;
	gchar            *cDemander;
	gchar            *cDescription;
	gchar            *cIconFilePath;
	gchar            *cConfFilePath;
	gchar            *cGroupName;
	gchar            *cKeyName;
} ;


typedef struct _CairoShortkeysManager CairoShortkeysManager;

#ifndef _MANAGER_DEF_
extern CairoShortkeysManager myShortkeysMgr;
#endif

// params

// manager
struct _CairoShortkeysManager {
	GldiManager mgr;
	gboolean (*bind) (const gchar *keystring, CDBindkeyHandler handler, gpointer user_data);
	void (*unbind) (const gchar *keystring, CDBindkeyHandler handler);
	gboolean (*trigger_shortkey) (const gchar *cKeyString);
	} ;

// signals
typedef enum {
	NOTIFICATION_SHORTKEY_ADDED,
	NOTIFICATION_SHORTKEY_REMOVED,
	NOTIFICATION_SHORTKEY_CHANGED,
	NB_NOTIFICATIONS_SHORTKEYS
	} CairoShortkeysNotifications;


/** Bind a shortkey to an action. Unbind it when you don't want it anymore, or when 'user_data' is freed.
 * @param keystring a shortcut.
 * @param cDemander the actor making the demand
 * @param cDescription a short description of the action
 * @param cIconFilePath an icon that represents the demander
 * @param cConfFilePath conf file where the shortkey stored
 * @param cGroupName group name where it's stored in the conf file
 * @param cKeyName key name where it's stored in the conf file
 * @param handler function called when the shortkey is pressed by the user
 * @param user_data data passed to the callback
 * @return the key binding
*/
CairoKeyBinding *cd_keybinder_bind (const gchar *keystring,
	const gchar *cDemander,
	const gchar *cDescription,
	const gchar *cIconFilePath,
	const gchar *cConfFilePath,
	const gchar *cGroupName,
	const gchar *cKeyName,
	CDBindkeyHandler handler,
	gpointer user_data);

/** Says if the shortkey of a key binding could be grabbed.
 * @param binding a key binding.
 * @return TRUE iif the shortkey has been successfuly grabbed by the key binding.
*/
#define cd_keybinder_could_grab(binding) ((binding)->bSuccess)

/** Unbind a shortkey. The binding is destroyed.
 * @param binding a key binding.
*/
void cd_keybinder_unbind (CairoKeyBinding *binding);


/** Rebind a shortkey to a new one.
 * @param binding a key binding.
 * @param .cNewKeyString the new shortkey
 * @return TRUE on success
*/
gboolean cd_keybinder_rebind (CairoKeyBinding *binding,
	const gchar *cNewKeyString);


void cd_keybinder_foreach (GFunc pCallback, gpointer data);

/** Trigger the given shortkey. It will be as if the user effectively pressed the shortkey on its keyboard. It uses the 'XTest' X extension.
 * @param cKeyString a shortkey.
 * @return TRUE if success.
*/
gboolean cairo_dock_trigger_shortkey (const gchar *cKeyString);


void gldi_register_shortkeys_manager (void);

G_END_DECLS

#endif /* __CD_KEY_BINDER_H__ */

