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
#include <X11/extensions/XTest.h>

#include "eggaccelerators.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-keybinder.h"

typedef unsigned int uint;
typedef struct _Binding {
	CDBindkeyHandler        handler;
	gpointer              user_data;
	char                 *keystring;
	uint                  keycode;
	uint                  modifiers;
} Binding;

static GSList *bindings = NULL;
static guint32 last_event_time = 0;
static gboolean processing_event = FALSE;

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
				      Binding   *binding,
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
	int i;

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
do_grab_key (Binding *binding)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GdkWindow *rootwin = gdk_get_default_root_window ();

	EggVirtualModifierType virtual_mods = 0;
	guint keysym = 0;

	if (keymap == NULL || rootwin == NULL)
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
	   g_warning ("Binding '%s' failed!", binding->keystring);
	   return FALSE;
	}

	return TRUE;
}

static gboolean
do_ungrab_key (Binding *binding)
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
		processing_event = TRUE;
		last_event_time = xevent->xkey.time;

		event_mods = xevent->xkey.state & ~(num_lock_mask  |
						    caps_lock_mask |
						    scroll_lock_mask);

		for (iter = bindings; iter != NULL; iter = iter->next) {
			Binding *binding = (Binding *) iter->data;

			if (binding->keycode == xevent->xkey.keycode &&
			    binding->modifiers == event_mods) {

				cd_debug ("Calling handler for '%s'...",
						binding->keystring);

				(binding->handler) (binding->keystring,
						    binding->user_data);
			}
		}

		processing_event = FALSE;
		break;
	case KeyRelease:
		cd_debug ("Got KeyRelease! ");
		break;
	}

	return return_val;
}

static void
keymap_changed (GdkKeymap *map)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GSList *iter;

	cd_debug ("Keymap changed! Regrabbing keys...");

	for (iter = bindings; iter != NULL; iter = iter->next) {
		Binding *binding = (Binding *) iter->data;
		do_ungrab_key (binding);
	}

	lookup_ignorable_modifiers (keymap);

	for (iter = bindings; iter != NULL; iter = iter->next) {
		Binding *binding = (Binding *) iter->data;
		do_grab_key (binding);
	}
}

void
cd_keybinder_init (void)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GdkWindow *rootwin = gdk_get_default_root_window ();

	lookup_ignorable_modifiers (keymap);

	gdk_window_add_filter (rootwin,
		filter_func,
		NULL);

	g_signal_connect (keymap,
		"keys_changed",
		G_CALLBACK (keymap_changed),
		NULL);
}

void
cd_keybinder_stop (void)
{
	GdkKeymap *keymap = gdk_keymap_get_default ();
	GdkWindow *rootwin = gdk_get_default_root_window ();

	num_lock_mask=0, caps_lock_mask=0, scroll_lock_mask=0;

	g_signal_handlers_disconnect_by_func (keymap,
		G_CALLBACK (keymap_changed),
		NULL);
	
	gdk_window_remove_filter (rootwin,
		filter_func,
		NULL);
}


gboolean
cd_keybinder_bind (const char           *keystring,
		       CDBindkeyHandler  handler,
		       gpointer              user_data)
{
	Binding *binding;
	gboolean success;

        if (!keystring)
          return FALSE;
	binding = g_new0 (Binding, 1);
	binding->keystring = g_strdup (keystring);
	binding->handler = handler;
	binding->user_data = user_data;

	/* Sets the binding's keycode and modifiers */
	cd_debug ("%s", keystring);
	success = do_grab_key (binding);

	if (success) {
		bindings = g_slist_prepend (bindings, binding);
	} else {
		cd_warning ("couldnt bind %s", keystring);
		g_free (binding->keystring);
		g_free (binding);
	}

	return success;
}

void
cd_keybinder_unbind (const char           *keystring,
			 CDBindkeyHandler  handler)
{
	GSList *iter;

        if (!keystring)
          return;
	for (iter = bindings; iter != NULL; iter = iter->next) {
		Binding *binding = (Binding *) iter->data;

		if (strcmp (keystring, binding->keystring) != 0 ||
		    handler != binding->handler)
			continue;

		do_ungrab_key (binding);

		bindings = g_slist_remove (bindings, binding);

		g_free (binding->keystring);
		g_free (binding);
		break;
	}
}

/*
 * From eggcellrenderkeys.c.
 */
gboolean
cd_keybinder_is_modifier (guint keycode)
{
	gint i;
	gint map_size;
	XModifierKeymap *mod_keymap;
	gboolean retval = FALSE;

	mod_keymap = XGetModifierMapping (gdk_display);

	map_size = 8 * mod_keymap->max_keypermod;

	i = 0;
	while (i < map_size) {
		if (keycode == mod_keymap->modifiermap[i]) {
			retval = TRUE;
			break;
		}
		++i;
	}

	XFreeModifiermap (mod_keymap);

	return retval;
}

guint32
cd_keybinder_get_current_event_time (void)
{
	if (processing_event)
		return last_event_time;
	else
		return GDK_CURRENT_TIME;
}


gboolean cairo_dock_simulate_key_sequence (gchar *cKeyString)  // the idea was taken from xdo.
{
	#ifdef HAVE_XEXTEND
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
	return FALSE;
	#endif
}


