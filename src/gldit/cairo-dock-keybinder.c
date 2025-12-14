/**
* This file is a part of the Cairo-Dock project
* cairo-dock-keybinder.c
* Login : <ctaf42@localhost.localdomain>
* Started on  Thu Jan 31 03:57:17 2008 Cedric GESTES
* $Id$
*
* Author(s)
*  - Cedric GESTES <ctaf42@gmail.com>
*  - Havoc Pennington
*  - Tim Janik
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
*
* imported from tomboy_keybinder.c
*/

#include <string.h>
#include <sys/types.h>

#include <gdk/gdk.h>
#include "gldi-config.h"
#ifdef HAVE_X11
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>  // we should check for XkbQueryExtension...
#endif
#ifdef HAVE_XEXTEND
#include <X11/extensions/XTest.h>
#endif

#include "cairo-dock-log.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-keybinder.h"

// public (manager, config, data)
GldiObjectManager myShortkeyObjectMgr;

// dependencies

// private
static GSList *s_pKeyBindings = NULL;


static void do_grab_key (GldiShortkey *binding, CairoDockGrabKeyResult cb)
{
	if (binding->keystring == NULL)
		return; // no need to signal failure, let's assume the caller checks this
	
	// parse the shortkey to get the keycode and modifiers
	guint keysym = 0;
	guint *accelerator_codes = NULL;
	gtk_accelerator_parse_with_keycode (binding->keystring,
		&keysym,
		&accelerator_codes,
		&binding->modifiers);
	if (accelerator_codes == NULL)
		return;
	
	binding->keycode = accelerator_codes[0];  // just take the first one
	g_free (accelerator_codes);
	
	// convert virtual modifiers to concrete ones
	GdkKeymap *keymap = gdk_keymap_get_default ();
	gdk_keymap_map_virtual_modifiers (keymap, &binding->modifiers);  // map the Meta, Super, Hyper virtual modifiers to their concrete counterparts
	binding->modifiers &= ~(GDK_SUPER_MASK | GDK_META_MASK | GDK_HYPER_MASK);  // and then remove them
	
	cd_debug ("%s -> %d, %d %d", binding->keystring, keysym, binding->keycode, binding->modifiers);
	
	// now grab the shortkey from the server
	gldi_desktop_grab_shortkey (binding, TRUE, cb);  // TRUE <=> grab
}

static gboolean do_ungrab_key (GldiShortkey *binding)
{
	cd_debug ("Removing grab for '%s'", binding->keystring);
	
	gldi_desktop_grab_shortkey (binding, FALSE, NULL);  // FALSE <=> ungrab
	
	return TRUE;
}

static gboolean _on_shortkey_pressed (G_GNUC_UNUSED gpointer data, guint keycode, guint modifiers)
{
	GSList *iter;
	for (iter = s_pKeyBindings; iter != NULL; iter = iter->next)
	{
		GldiShortkey *binding = (GldiShortkey *) iter->data;

		if (binding->keycode == keycode
		&& binding->modifiers == modifiers)
		{
			cd_debug ("Calling handler for '%s'...", binding->keystring);
			(binding->handler) (binding->keystring, binding->user_data);
		}
	}
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_keymap_changed (G_GNUC_UNUSED gpointer data, gboolean updated)
{
	GSList *iter;
	for (iter = s_pKeyBindings; iter != NULL; iter = iter->next)
	{
		GldiShortkey *binding = (GldiShortkey *) iter->data;

		if (updated) do_grab_key (binding, NULL); // no callback, bSuccess will be updated
		else do_ungrab_key (binding);
	}
	return GLDI_NOTIFICATION_LET_PASS;
}


GldiShortkey *gldi_shortkey_new (const gchar *keystring,
	const gchar *cDemander,
	const gchar *cDescription,
	const gchar *cIconFilePath,
	const gchar *cConfFilePath,
	const gchar *cGroupName,
	const gchar *cKeyName,
	CDBindkeyHandler handler,
	gpointer user_data)
{
	GldiShortkeyAttr attr;
	attr.keystring = keystring;
	attr.cDemander = cDemander;
	attr.cDescription = cDescription;
	attr.cIconFilePath = cIconFilePath;
	attr.cConfFilePath = cConfFilePath;
	attr.cGroupName = cGroupName;
	attr.cKeyName = cKeyName;
	attr.handler = handler;
	attr.user_data = user_data;
	return (GldiShortkey*)gldi_object_new (&myShortkeyObjectMgr, &attr);
}


void _shortkey_rebind_cb (GldiShortkey *binding)
{
	// note: currently, there are no users of this notification
	gldi_object_notify (binding, NOTIFICATION_SHORTKEY_CHANGED, binding);
}

void gldi_shortkey_rebind (GldiShortkey *binding,
	const gchar *cNewKeyString,
	const gchar *cNewDescription)
{
	g_return_if_fail (binding != NULL);
	cd_debug ("%s (%s)", __func__, binding->keystring);
	
	// ensure it's a registered binding
	GSList *iter = g_slist_find (s_pKeyBindings, binding);
	g_return_if_fail (iter != NULL);
	
	// update the description if needed
	if (cNewDescription != NULL)
	{
		g_free (binding->cDescription);
		binding->cDescription = g_strdup (cNewDescription);
	}
	
	// if the shortkey is the same and already bound, no need to re-grab it.
	if (g_strcmp0 (cNewKeyString, binding->keystring) == 0 && binding->bSuccess)
		return;
	
	// unbind its current shortkey
	if (binding->bSuccess)
		do_ungrab_key (binding);

	// rebind it to the new shortkey
	if (cNewKeyString != binding->keystring)
	{
		g_free (binding->keystring);
		binding->keystring = g_strdup (cNewKeyString);
	}
	
	do_grab_key (binding, _shortkey_rebind_cb);
}


void gldi_shortkeys_foreach (GFunc pCallback, gpointer data)
{
	g_slist_foreach (s_pKeyBindings, pCallback, data);
}


#ifdef HAVE_XEXTEND
static gboolean _xtest_is_available (void)
{
	static gboolean s_bChecked = FALSE;
	static gboolean s_bUseXTest = FALSE;
	if (!s_bChecked)
	{
		s_bChecked = TRUE;
		GdkDisplay *gdsp = gdk_display_get_default();
		if (! GDK_IS_X11_DISPLAY(gdsp))
			return FALSE;
		
		Display *display = GDK_DISPLAY_XDISPLAY (gdsp);
		int event_base, error_base, major = 0, minor = 0;
		s_bUseXTest = XTestQueryExtension (display, &event_base, &error_base, &major, &minor);
		if (!s_bUseXTest)
			cd_warning ("XTest extension not available.");
	}
	return s_bUseXTest;
}

gboolean cairo_dock_trigger_shortkey (const gchar *cKeyString)  // the idea was taken from xdo.
{
	g_return_val_if_fail (cKeyString != NULL, FALSE);
	if (! _xtest_is_available ())  // XTest extension not available, or not an X session
		return FALSE;
	cd_message ("%s (%s)", __func__, cKeyString);
	
	// parse the shortkey (let gtk do the job)
	int pKeySyms[7];
	GdkModifierType modifiers;
	guint keysym = 0;
	guint *accelerator_codes = NULL;
	gtk_accelerator_parse_with_keycode (cKeyString,
		&keysym,
		&accelerator_codes,
		&modifiers);
	if (accelerator_codes == NULL)
		return FALSE;
	
	// extract the modifiers keysyms first, and then the key (the order of the modifiers doesn't matter, and any shortkey is made of N modifiers followed by a single key, so we can fill the array easily)
	int i = 0;
	if (modifiers & GDK_SHIFT_MASK)
		pKeySyms[i++] = XStringToKeysym ("Shift_L");
	if (modifiers & GDK_CONTROL_MASK)
		pKeySyms[i++] = XStringToKeysym ("Control_L");
	if (modifiers & GDK_MOD1_MASK)
		pKeySyms[i++] = XStringToKeysym ("Alt_L");
	if (modifiers & GDK_SUPER_MASK)
		pKeySyms[i++] = XStringToKeysym ("Super_L");
	if (modifiers & GDK_HYPER_MASK)
		pKeySyms[i++] = XStringToKeysym ("Hyper_L");
	if (modifiers & GDK_META_MASK)
		pKeySyms[i++] = XStringToKeysym ("Meta_L");
	pKeySyms[i++] = keysym;
	int iNbKeys = i;
	
	// press the keys one by one
	int keycode;
	GdkDisplay *gdsp = gdk_display_get_default();
	if (! GDK_IS_X11_DISPLAY(gdsp))
		return FALSE;
	Display *dpy = GDK_DISPLAY_XDISPLAY (gdsp);
	for (i = 0; i < iNbKeys; i ++)
	{
		keycode = XKeysymToKeycode (dpy, pKeySyms[i]);
		XTestFakeKeyEvent (dpy, keycode, TRUE, CurrentTime);  // TRUE <=> presse.
	}
	
	// and then release them in reverse order, as you would do by hands
	for (i = iNbKeys-1; i >=0; i --)
	{
		keycode = XKeysymToKeycode (dpy, pKeySyms[i]);
		XTestFakeKeyEvent (dpy, keycode, FALSE, CurrentTime);  // FALSE <=> release
	}
	
	XFlush (dpy);
	
	return TRUE;
}
#else
gboolean cairo_dock_trigger_shortkey (G_GNUC_UNUSED const gchar *cKeyString)
{
	cd_warning ("The dock was not compiled with the support of XTest.");  // currently we have no way to do that with Wayland...
	return FALSE;
}
#endif



  ////////////
 /// INIT ///
////////////

static void init (void)
{
	gldi_object_register_notification (&myDesktopMgr,
		NOTIFICATION_SHORTKEY_PRESSED,
		(GldiNotificationFunc) _on_shortkey_pressed,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDesktopMgr,
		NOTIFICATION_KEYMAP_CHANGED,
		(GldiNotificationFunc) _on_keymap_changed,
		GLDI_RUN_AFTER, NULL);	
}


/**static void unload (void)
{
	GSList *iter;
	for (iter = s_pKeyBindings; iter != NULL; iter = iter->next)
	{
		GldiShortkey *binding = (GldiShortkey *) iter->data;
		
		cd_debug (" --- remove key binding '%s'", binding->keystring);
		if (binding->bSuccess)
		{
			do_ungrab_key (binding);
			binding->bSuccess = FALSE;
		}
		gldi_object_notify (&myShortkeyObjectMgr, NOTIFICATION_SHORTKEY_REMOVED, binding);
		
		_free_binding (binding);
	}
	g_slist_free (s_pKeyBindings);
	s_pKeyBindings = NULL;
}*/

  ///////////////
 /// MANAGER ///
///////////////

static void _shortkey_warning_cb (GldiShortkey *pShortkey)
{
	if (! pShortkey->bSuccess)
	{
		cd_warning ("Couldn't bind '%s' (%s: %s)\n This shortkey is probably already used by another applet or another application", pShortkey->keystring, pShortkey->cDemander, pShortkey->cDescription);
	}
}

static void init_object (GldiObject *obj, gpointer attr)
{
	GldiShortkey *pShortkey = (GldiShortkey*)obj;
	GldiShortkeyAttr *sattr = (GldiShortkeyAttr*)attr;
	
	// store the info
	pShortkey->keystring = g_strdup (sattr->keystring);
	pShortkey->cDemander = g_strdup (sattr->cDemander);
	pShortkey->cDescription = g_strdup (sattr->cDescription);
	pShortkey->cIconFilePath = g_strdup (sattr->cIconFilePath);
	pShortkey->cConfFilePath = g_strdup (sattr->cConfFilePath);
	pShortkey->cGroupName = g_strdup (sattr->cGroupName);
	pShortkey->cKeyName = g_strdup (sattr->cKeyName);
	pShortkey->handler = sattr->handler;
	pShortkey->user_data = sattr->user_data;
	
	// register the new shortkey
	s_pKeyBindings = g_slist_prepend (s_pKeyBindings, pShortkey);
	
	// try to grab the key
	if (pShortkey->keystring != NULL)
		do_grab_key (pShortkey, _shortkey_warning_cb);
}

static void reset_object (GldiObject *obj)
{
	GldiShortkey *pShortkey = (GldiShortkey*)obj;
	
	// unbind the shortkey
	if (pShortkey->bSuccess)
		do_ungrab_key (pShortkey);
	
	// remove it from the list
	cd_debug (" --- remove key binding '%s'", pShortkey->keystring);
	s_pKeyBindings = g_slist_remove (s_pKeyBindings, pShortkey);
	
	// free data
	g_free (pShortkey->keystring);
	g_free (pShortkey->cDemander);
	g_free (pShortkey->cDescription);
	g_free (pShortkey->cIconFilePath);
	g_free (pShortkey->cConfFilePath);
	g_free (pShortkey->cGroupName);
	g_free (pShortkey->cKeyName);
}

void gldi_register_shortkeys_manager (void)
{
	// Manager
	memset (&myShortkeyObjectMgr, 0, sizeof (GldiObjectManager));
	myShortkeyObjectMgr.cName        = "Shortkeys";
	myShortkeyObjectMgr.iObjectSize  = sizeof (GldiShortkey);
	// interface
	myShortkeyObjectMgr.init_object  = init_object;
	myShortkeyObjectMgr.reset_object = reset_object;
	// signals
	gldi_object_install_notifications (&myShortkeyObjectMgr, NB_NOTIFICATIONS_SHORTKEYS);
	
	// init (since we don't unload the shortkeys ourselves, and the init can be done immediately, no need for a Manager) 
	init ();
}
