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
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include "gldi-config.h"
#ifdef HAVE_XEXTEND
#include <X11/extensions/XTest.h>
#endif

#include "eggaccelerators.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-keybinder.h"

// public (manager, config, data)
GldiObjectManager myShortkeyObjectMgr;

// dependancies

// private
static GSList *s_pKeyBindings = NULL;
///static guint32 last_event_time = 0;
///static gboolean processing_event = FALSE;

static guint num_lock_mask=0, caps_lock_mask=0, scroll_lock_mask=0;

static void
lookup_ignorable_modifiers (GdkKeymap *keymap)
{
	egg_keymap_resolve_virtual_modifiers (keymap,
					      EGG_VIRTUAL_LOCK_MASK,
					      &caps_lock_mask);

	egg_keymap_resolve_virtual_modifiers (keymap,
					      EGG_VIRTUAL_NUM_LOCK_MASK,
					      &num_lock_mask);

	egg_keymap_resolve_virtual_modifiers (keymap,
					      EGG_VIRTUAL_SCROLL_LOCK_MASK,
					      &scroll_lock_mask);
}

static void
grab_ungrab_with_ignorable_modifiers (GdkWindow *rootwin,
				      GldiShortkey   *binding,
				      gboolean   grab)
{
	guint mod_masks [] = {
		0, /* modifier only */
		num_lock_mask,
		caps_lock_mask,
		scroll_lock_mask,
		num_lock_mask  | caps_lock_mask,
		num_lock_mask  | scroll_lock_mask,
		caps_lock_mask | scroll_lock_mask,
		num_lock_mask  | caps_lock_mask | scroll_lock_mask,
	};
	
	guint i;
	for (i = 0; i < G_N_ELEMENTS (mod_masks); i++) {
		if (grab) {
			XGrabKey (GDK_WINDOW_XDISPLAY (rootwin),
				  binding->keycode,
				  binding->modifiers | mod_masks [i],
				  GDK_WINDOW_XID (rootwin),
				  False,
				  GrabModeAsync,
				  GrabModeAsync);
		} else {
			XUngrabKey (GDK_WINDOW_XDISPLAY (rootwin),
				    binding->keycode,
				    binding->modifiers | mod_masks [i],
				    GDK_WINDOW_XID (rootwin));
		}
	}
}

static gboolean
do_grab_key (GldiShortkey *binding)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GdkWindow *rootwin = gdk_get_default_root_window ();

	EggVirtualModifierType virtual_mods = 0;
	guint keysym = 0;

	if (keymap == NULL || rootwin == NULL || binding->keystring == NULL)
		return FALSE;

	if (!egg_accelerator_parse_virtual (binding->keystring,
			&keysym,
			&virtual_mods))
		return FALSE;

	cd_debug ("Got accel %d, %d", keysym, virtual_mods);

	binding->keycode = XKeysymToKeycode (GDK_WINDOW_XDISPLAY (rootwin), keysym);
	if (binding->keycode == 0)
		return FALSE;

	cd_debug ("Got keycode %d", binding->keycode);

	egg_keymap_resolve_virtual_modifiers (keymap,
		virtual_mods,
		&binding->modifiers);

	cd_debug ("Got modmask %d", binding->modifiers);

	gdk_error_trap_push ();

	grab_ungrab_with_ignorable_modifiers (rootwin,
		binding,
		TRUE /* grab */);

	gdk_flush ();

	if (gdk_error_trap_pop ())
	{
		g_warning ("GldiShortkey '%s' failed!", binding->keystring);
		return FALSE;
	}

	return TRUE;
}

static gboolean
do_ungrab_key (GldiShortkey *binding)
{
	GdkWindow *rootwin = gdk_get_default_root_window ();
	cd_debug ("Removing grab for '%s'", binding->keystring);

	grab_ungrab_with_ignorable_modifiers (rootwin,
		binding,
		FALSE /* ungrab */);

	return TRUE;
}

static GdkFilterReturn
filter_func (GdkXEvent *gdk_xevent, GdkEvent *event, G_GNUC_UNUSED gpointer data)
{
	GdkFilterReturn return_val = GDK_FILTER_CONTINUE;
	XEvent *xevent = (XEvent *) gdk_xevent;
	guint event_mods;
	GSList *iter;

	cd_debug ("Got Event! %d, %d", xevent->type, event->type);

	switch (xevent->type)
	{
		case KeyPress:
			cd_debug ("Got KeyPress! keycode: %d, modifiers: %d", xevent->xkey.keycode, xevent->xkey.state);
			/*
			 * Set the last event time for use when showing
			 * windows to avoid anti-focus-stealing code.
			 */
			///processing_event = TRUE;
			///last_event_time = xevent->xkey.time;

			event_mods = xevent->xkey.state & ~(num_lock_mask | caps_lock_mask | scroll_lock_mask);

			for (iter = s_pKeyBindings; iter != NULL; iter = iter->next) {
				GldiShortkey *binding = (GldiShortkey *) iter->data;

				if (binding->keycode == xevent->xkey.keycode
				&& binding->modifiers == event_mods)
				{
					cd_debug ("Calling handler for '%s'...", binding->keystring);
					(binding->handler) (binding->keystring, binding->user_data);
				}
			}
			///processing_event = FALSE;
		break;
		case KeyRelease:
			cd_debug ("Got KeyRelease! ");
		break;
	}

	return return_val;
}

static void
on_keymap_changed (G_GNUC_UNUSED GdkKeymap *map)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GSList *iter;

	cd_debug ("Keymap changed! Regrabbing keys...");

	for (iter = s_pKeyBindings; iter != NULL; iter = iter->next)
	{
		GldiShortkey *binding = (GldiShortkey *) iter->data;
		do_ungrab_key (binding);
	}

	lookup_ignorable_modifiers (keymap);

	for (iter = s_pKeyBindings; iter != NULL; iter = iter->next)
	{
		GldiShortkey *binding = (GldiShortkey *) iter->data;
		do_grab_key (binding);
	}
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


gboolean gldi_shortkey_rebind (GldiShortkey *binding,
	const gchar *cNewKeyString,
	const gchar *cNewDescription)
{
	g_return_val_if_fail (binding != NULL, FALSE);
	cd_debug ("%s (%s)", __func__, binding->keystring);
	
	// ensure it's a registerd binding
	GSList *iter = g_slist_find (s_pKeyBindings, binding);
	g_return_val_if_fail (iter != NULL, FALSE);
	
	// update the description if needed
	if (cNewDescription != NULL)
	{
		g_free (binding->cDescription);
		binding->cDescription = g_strdup (cNewDescription);
	}
	
	// if the shortkey is the same and already bound, no need to re-grab it.
	if (g_strcmp0 (cNewKeyString, binding->keystring) == 0 && binding->bSuccess)
		return TRUE;
	
	// unbind its current shortkey
	if (binding->bSuccess)
		do_ungrab_key (binding);

	// rebind it to the new shortkey
	if (cNewKeyString != binding->keystring)
	{
		g_free (binding->keystring);
		binding->keystring = g_strdup (cNewKeyString);
	}
	
	binding->bSuccess = do_grab_key (binding);
	
	gldi_object_notify (binding, NOTIFICATION_SHORTKEY_CHANGED, binding);
	
	return binding->bSuccess;
}


void gldi_shortkeys_foreach (GFunc pCallback, gpointer data)
{
	g_slist_foreach (s_pKeyBindings, pCallback, data);
}


gboolean cairo_dock_trigger_shortkey (const gchar *cKeyString)  // the idea was taken from xdo.
{
#ifdef HAVE_XEXTEND
	g_return_val_if_fail (cairo_dock_xtest_is_available (), FALSE);
	g_return_val_if_fail (cKeyString != NULL, FALSE);
	cd_message ("%s (%s)", __func__, cKeyString);
	
	int iNbKeys = 0;
	int *pKeySyms = egg_keystring_to_keysyms (cKeyString, &iNbKeys);

	int i;
	int keycode;
	Display *dpy = cairo_dock_get_Xdisplay ();
	for (i = 0; i < iNbKeys; i ++)
	{
		keycode = XKeysymToKeycode (dpy, pKeySyms[i]);
		XTestFakeKeyEvent (dpy, keycode, TRUE, CurrentTime);  // TRUE <=> presse.
	}
	
	for (i = iNbKeys-1; i >=0; i --)
	{
		keycode = XKeysymToKeycode (dpy, pKeySyms[i]);
		XTestFakeKeyEvent (dpy, keycode, FALSE, CurrentTime);  // TRUE <=> presse.
	}
	
	XFlush (dpy);
	
	return TRUE;
#else
	cd_warning ("The dock was not compiled with the support of XTest.");
	return FALSE;
#endif
}



  ////////////
 /// INIT ///
////////////

static void init (void)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GdkWindow *rootwin = gdk_get_default_root_window ();

	lookup_ignorable_modifiers (keymap);

	gdk_window_add_filter (rootwin,
		filter_func,
		NULL);

	g_signal_connect (keymap,
		"keys_changed",
		G_CALLBACK (on_keymap_changed),
		NULL);
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
	{
		pShortkey->bSuccess = do_grab_key (pShortkey);
		
		if (! pShortkey->bSuccess)
		{
			cd_warning ("Couldn't bind '%s' (%s: %s)\n This shortkey is probably already used by another applet or another application", pShortkey->keystring, pShortkey->cDemander, pShortkey->cDescription);
		}
	}
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
