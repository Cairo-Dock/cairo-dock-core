/*
** cairo-dock-keybinder.c
** Login : <ctaf42@localhost.localdomain>
** Started on  Thu Jan 31 03:57:17 2008 Cedric GESTES
** $Id$
**
** Author(s)
**  - Cedric GESTES <ctaf42@gmail.com>
**  - Havoc Pennington
**  - Tim Janik
**
** Copyright (C) 2008 Cedric GESTES
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*
* imported from tomboy_keybinder.c
*/

#include <string.h>
#include <sys/types.h>
#include <gdk/gdk.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include "gldi-config.h"
#ifdef HAVE_XEXTEND
#include <X11/extensions/XTest.h>
#endif

#include "eggaccelerators.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-keybinder.h"


// public (manager, config, data)
CairoShortkeysManager myShortkeysMgr;

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
				      CairoKeyBinding   *binding,
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
				  GDK_WINDOW_XWINDOW (rootwin),
				  False,
				  GrabModeAsync,
				  GrabModeAsync);
		} else {
			XUngrabKey (GDK_WINDOW_XDISPLAY (rootwin),
				    binding->keycode,
				    binding->modifiers | mod_masks [i],
				    GDK_WINDOW_XWINDOW (rootwin));
		}
	}
}

static gboolean
do_grab_key (CairoKeyBinding *binding)
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

	binding->keycode = XKeysymToKeycode (GDK_WINDOW_XDISPLAY (rootwin),
					     keysym);
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

	if (gdk_error_trap_pop ()) {
	   g_warning ("CairoKeyBinding '%s' failed!", binding->keystring);
	   return FALSE;
	}

	return TRUE;
}

static gboolean
do_ungrab_key (CairoKeyBinding *binding)
{
	GdkWindow *rootwin = gdk_get_default_root_window ();

	cd_debug ("Removing grab for '%s'", binding->keystring);

	grab_ungrab_with_ignorable_modifiers (rootwin,
					      binding,
					      FALSE /* ungrab */);

	return TRUE;
}

static GdkFilterReturn
filter_func (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
	GdkFilterReturn return_val = GDK_FILTER_CONTINUE;
	XEvent *xevent = (XEvent *) gdk_xevent;
	guint event_mods;
	GSList *iter;

	cd_debug ("Got Event! %d, %d", xevent->type, event->type);

	switch (xevent->type) {
	case KeyPress:
		cd_debug ("Got KeyPress! keycode: %d, modifiers: %d",
				xevent->xkey.keycode,
				xevent->xkey.state);

		/*
		 * Set the last event time for use when showing
		 * windows to avoid anti-focus-stealing code.
		 */
		///processing_event = TRUE;
		///last_event_time = xevent->xkey.time;

		event_mods = xevent->xkey.state & ~(num_lock_mask  |
						    caps_lock_mask |
						    scroll_lock_mask);

		for (iter = s_pKeyBindings; iter != NULL; iter = iter->next) {
			CairoKeyBinding *binding = (CairoKeyBinding *) iter->data;

			if (binding->keycode == xevent->xkey.keycode &&
			    binding->modifiers == event_mods) {

				cd_debug ("Calling handler for '%s'...",
						binding->keystring);

				(binding->handler) (binding->keystring,
						    binding->user_data);
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
on_keymap_changed (GdkKeymap *map)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GSList *iter;

	cd_debug ("Keymap changed! Regrabbing keys...");

	for (iter = s_pKeyBindings; iter != NULL; iter = iter->next) {
		CairoKeyBinding *binding = (CairoKeyBinding *) iter->data;
		do_ungrab_key (binding);
	}

	lookup_ignorable_modifiers (keymap);

	for (iter = s_pKeyBindings; iter != NULL; iter = iter->next) {
		CairoKeyBinding *binding = (CairoKeyBinding *) iter->data;
		do_grab_key (binding);
	}
}


CairoKeyBinding *
cd_keybinder_bind (const gchar *keystring,
	const gchar *cDemander,
	const gchar *cDescription,
	const gchar *cIconFilePath,
	const gchar *cConfFilePath,
	const gchar *cGroupName,
	const gchar *cKeyName,
	CDBindkeyHandler handler,
	gpointer user_data)
{
	CairoKeyBinding *binding;
	gboolean success;
	cd_debug ("%s (%s)", __func__, keystring);
	
	// register the new shortkey
	binding = g_new0 (CairoKeyBinding, 1);
	
	binding->keystring = g_strdup (keystring);
	binding->cDemander = g_strdup (cDemander);
	binding->cDescription = g_strdup (cDescription);
	binding->cIconFilePath = g_strdup (cIconFilePath);
	binding->cConfFilePath = g_strdup (cConfFilePath);
	binding->cGroupName = g_strdup (cGroupName);
	binding->cKeyName = g_strdup (cKeyName);
	binding->handler = handler;
	binding->user_data = user_data;
	
	s_pKeyBindings = g_slist_prepend (s_pKeyBindings, binding);
	
	// try to grab the key
	if (keystring != NULL)
	{
		binding->bSuccess = do_grab_key (binding);
		
		if (! binding->bSuccess)
		{
			cd_warning ("Couldn't bind '%s' (%s: %s)\n This shortkey is probably already used by another applet or another application", keystring, cDemander, cDescription);
		}
	}
	
	cairo_dock_notify_on_object (&myShortkeysMgr, NOTIFICATION_SHORTKEY_ADDED, binding);
	
	return binding;
}


static void _free_binding (CairoKeyBinding *binding)
{
	g_free (binding->keystring);
	g_free (binding->cDemander);
	g_free (binding->cDescription);
	g_free (binding->cIconFilePath);
	g_free (binding->cConfFilePath);
	g_free (binding->cGroupName);
	g_free (binding->cKeyName);
	g_free (binding);
}

void
cd_keybinder_unbind (CairoKeyBinding *binding)
{
	if (binding == NULL)
		return;
	g_print ("%s (%s)\n", __func__, binding->keystring);
	
	// ensure it's a registerd binding
	GSList *iter = g_slist_find (s_pKeyBindings, binding);
	g_return_if_fail (iter != NULL);
	
	// unbind the shortkey
	if (binding->bSuccess)
		do_ungrab_key (binding);
	
	// remove it from the list and destroy it
	cd_debug (" --- remove key binding '%s'\n", binding->keystring);
	s_pKeyBindings = g_slist_delete_link (s_pKeyBindings, iter);
	
	cairo_dock_notify_on_object (&myShortkeysMgr, NOTIFICATION_SHORTKEY_REMOVED, binding);
	
	_free_binding (binding);
}


gboolean cd_keybinder_rebind (CairoKeyBinding *binding,
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
	
	cairo_dock_notify_on_object (&myShortkeysMgr, NOTIFICATION_SHORTKEY_CHANGED, binding);
	
	return binding->bSuccess;
}


void cd_keybinder_foreach (GFunc pCallback, gpointer data)
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
		CairoKeyBinding *binding = (CairoKeyBinding *) iter->data;
		
		cd_debug (" --- remove key binding '%s'\n", binding->keystring);
		if (binding->bSuccess)
		{
			do_ungrab_key (binding);
			binding->bSuccess = FALSE;
		}
		cairo_dock_notify_on_object (&myShortkeysMgr, NOTIFICATION_SHORTKEY_REMOVED, binding);
		
		_free_binding (binding);
	}
	g_slist_free (s_pKeyBindings);
	s_pKeyBindings = NULL;
}*/

  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_shortkeys_manager (void)
{
	// Manager
	memset (&myShortkeysMgr, 0, sizeof (CairoShortkeysManager));
	myShortkeysMgr.mgr.cModuleName 	= "Shortkeys";
	myShortkeysMgr.mgr.init 		= init;
	myShortkeysMgr.mgr.load 		= NULL;
	myShortkeysMgr.mgr.unload 		= NULL;  /// unload
	myShortkeysMgr.mgr.reload 		= (GldiManagerReloadFunc)NULL;
	myShortkeysMgr.mgr.get_config 	= (GldiManagerGetConfigFunc)NULL;
	myShortkeysMgr.mgr.reset_config = (GldiManagerResetConfigFunc)NULL;
	// Config
	myShortkeysMgr.mgr.pConfig = (GldiManagerConfigPtr)NULL;
	myShortkeysMgr.mgr.iSizeOfConfig = 0;
	// data
	myShortkeysMgr.mgr.pData = (GldiManagerDataPtr)NULL;
	myShortkeysMgr.mgr.iSizeOfData = 0;
	// signals
	cairo_dock_install_notifications_on_object (&myShortkeysMgr, NB_NOTIFICATIONS_SHORTKEYS);
	// register
	gldi_register_manager (GLDI_MANAGER(&myShortkeysMgr));
}
