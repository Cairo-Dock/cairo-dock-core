/*
 * cairo-dock-foreign-toplevel.c
 * 
 * Interact with Wayland clients via the zwlr_foreign_toplevel_manager
 * protocol. See e.g. https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-foreign-toplevel-management-unstable-v1.xml
 * 
 * Copyright 2020 Daniel Kondor <kondor.dani@gmail.com>
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
 */

#include "gldi-config.h"
#ifdef HAVE_WAYLAND

#include <gdk/gdkwayland.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "cairo-dock-windows-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-X-manager.h"
#include "cairo-dock-utils.h"
#include "cairo-dock-log.h"
#include "cairo-dock-foreign-toplevel.h"

#include <stdio.h>


static GldiObjectManager myWFTObjectMgr;

typedef struct zwlr_foreign_toplevel_handle_v1 wfthandle;


struct _GldiWFTWindowActor {
	GldiWindowActor actor;
	// foreign-toplevel specific
	// handle to toplevel object
	wfthandle *handle;
};
typedef struct _GldiWFTWindowActor GldiWFTWindowActor;



// window manager interface

static void _show (GldiWindowActor *actor)
{
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	GdkDisplay *dsp = gdk_display_get_default ();
	GdkSeat *seat = gdk_display_get_default_seat (dsp);
	struct wl_seat* wl_seat = gdk_wayland_seat_get_wl_seat (seat);
	zwlr_foreign_toplevel_handle_v1_activate (wactor->handle, wl_seat);
}
static void _close (GldiWindowActor *actor)
{
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	zwlr_foreign_toplevel_handle_v1_close (wactor->handle);
}
static void _minimize (GldiWindowActor *actor)
{
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	zwlr_foreign_toplevel_handle_v1_set_minimized (wactor->handle);
}
static void _maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	GldiWFTWindowActor *wactor = (GldiWFTWindowActor *)actor;
	if (bMaximize) zwlr_foreign_toplevel_handle_v1_set_maximized (wactor->handle);
	else zwlr_foreign_toplevel_handle_v1_unset_maximized (wactor->handle);
}
static GldiWindowActor* s_pActiveWindow = NULL;
static GldiWindowActor* _get_active_window (void)
{
	return s_pActiveWindow;
}
static void _can_minimize_maximize_close ( G_GNUC_UNUSED GldiWindowActor *actor, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose)
{
	// we don't know, set everything to true
	*bCanMinimize = TRUE;
	*bCanMaximize = TRUE;
	*bCanClose = TRUE;
}


// callbacks 
void _gldi_toplevel_title_cb (void *data, G_GNUC_UNUSED wfthandle *handle, const char *title)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	g_free (actor->cName);
	actor->cName = g_strdup ((gchar *)title);
	if (actor->cClass) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_NAME_CHANGED, actor);
}

void _gldi_toplevel_appid_cb (void *data, G_GNUC_UNUSED wfthandle *handle, const char *app_id)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	
	if (!app_id) return;
	else
	{
		char* cOldClass = actor->cClass;
		actor->cClass = g_strdup ((gchar *)app_id);
		if (!cOldClass)
		{
			actor->bDisplayed = TRUE;
			gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_CREATED, actor);
		}
		else
		{
			gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_CLASS_CHANGED, actor, cOldClass, NULL);
			g_free (cOldClass);
		}
	}
}

void _gldi_toplevel_output_enter_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED wfthandle *handle, G_GNUC_UNUSED struct wl_output *output)
{
	/* TODO -- or maybe we don't care about this? */
}
void _gldi_toplevel_output_leave_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED wfthandle *handle, G_GNUC_UNUSED struct wl_output *output)
{
	
}

void _gldi_toplevel_state_cb (void *data, G_GNUC_UNUSED wfthandle *handle, struct wl_array *state)
{
	if (!data) return;
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	gboolean activated = FALSE, maximized = FALSE, minimized = FALSE;
	int i;
	uint32_t* stdata = (uint32_t*)state->data;
	for (i = 0; i*sizeof(uint32_t) < state->size; i++)
	{
		if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            activated = TRUE;
        else if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
            maximized = TRUE;
        else if (stdata[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
            minimized = TRUE;
	}
	if (activated)
	{
		s_pActiveWindow = actor;
		if (actor->cClass) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_ACTIVATED, actor);
	}
	gboolean bHiddenChanged     = (minimized != actor->bIsHidden);
	gboolean bMaximizedChanged  = (maximized != actor->bIsMaximized);
	gboolean bFullScreenChanged = FALSE;
	actor->bIsHidden     = minimized;
	actor->bIsMaximized  = maximized;
					
	if (bHiddenChanged || bMaximizedChanged)
		if (actor->cClass) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_STATE_CHANGED, actor, bHiddenChanged, bMaximizedChanged, bFullScreenChanged);
}

void _gldi_toplevel_done_cb ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED wfthandle *handle)
{
	
}

void _gldi_toplevel_closed_cb (void *data, G_GNUC_UNUSED wfthandle *handle)
{
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)data;
	GldiWindowActor* actor = (GldiWindowActor*)wactor;
	if (actor->cClass) gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESTROYED, actor);
	gldi_object_unref (GLDI_OBJECT(actor));
}


struct zwlr_foreign_toplevel_handle_v1_listener gldi_toplevel_handle_interface = {
    .title        = _gldi_toplevel_title_cb,
    .app_id       = _gldi_toplevel_appid_cb,
    .output_enter = _gldi_toplevel_output_enter_cb,
    .output_leave = _gldi_toplevel_output_leave_cb,
    .state        = _gldi_toplevel_state_cb,
    .done         = _gldi_toplevel_done_cb,
    .closed       = _gldi_toplevel_closed_cb 
};


typedef enum {
	NB_NOTIFICATIONS_WFT_MANAGER = NB_NOTIFICATIONS_WINDOWS
	} CairoWFTManagerNotifications;

static void init_object (GldiObject *obj, gpointer attr)
{
	GldiWFTWindowActor* actor = (GldiWFTWindowActor*)obj;
	actor->handle = (wfthandle*)attr;
}

static void reset_object (GldiObject* obj)
{
	GldiWFTWindowActor* actor = (GldiWFTWindowActor*)obj;
	if (obj && actor->handle) zwlr_foreign_toplevel_handle_v1_destroy(actor->handle);
}

/* register new toplevel */
static void _new_toplevel ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwlr_foreign_toplevel_manager_v1 *manager,
	wfthandle *handle)
{
	gboolean bIsHidden = FALSE, bIsFullScreen = FALSE, bIsMaximized = FALSE, bDemandsAttention = FALSE;
	
	GldiWFTWindowActor* wactor = (GldiWFTWindowActor*)gldi_object_new (&myWFTObjectMgr, handle);
	zwlr_foreign_toplevel_handle_v1_set_user_data (handle, wactor);
	GldiWindowActor *actor = (GldiWindowActor*)wactor;
	actor->bDisplayed = FALSE; // we do not try to display this window until we can app_id / cClass
	actor->cClass = NULL;
	actor->cWmClass = NULL;
	actor->bIsHidden = bIsHidden;
	actor->bIsMaximized = bIsMaximized;
	actor->bIsFullScreen = bIsFullScreen;
	actor->bDemandsAttention = bDemandsAttention;
	// hack required for minimize on click to work -- it messes up hiding the dock though
	actor->windowGeometry.width = 1;
	actor->windowGeometry.height = 1;
	
	/* note: we cannot do anything as long as we get app_id */
	zwlr_foreign_toplevel_handle_v1_add_listener (handle, &gldi_toplevel_handle_interface, wactor);
}

/* sent when toplevel management is no longer available -- maybe unload this module? */
static void _toplevel_manager_finished ( G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwlr_foreign_toplevel_manager_v1 *manager)
{
    cd_message ("Toplevel manager exited, taskbar will not be available!");
}



static struct zwlr_foreign_toplevel_manager_v1_listener gldi_toplevel_manager = {
    .toplevel = _new_toplevel,
    .finished = _toplevel_manager_finished,
};

static struct zwlr_foreign_toplevel_manager_v1* s_ptoplevel_manager = NULL;

gboolean gldi_zwlr_foreign_toplevel_manager_try_bind (struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name))
    {
		if (version > 1u) version = 1u;
        s_ptoplevel_manager = wl_registry_bind (registry, id, &zwlr_foreign_toplevel_manager_v1_interface, version);
		if (s_ptoplevel_manager) return TRUE;
		else cd_message ("Could not bind foreign-toplevel-manager!");
    }
    return FALSE;
}

void gldi_zwlr_foreign_toplevel_manager_init ()
{
	if (!s_ptoplevel_manager) return;
	
	// register window manager
	GldiWindowManagerBackend wmb;
	memset (&wmb, 0, sizeof (GldiWindowManagerBackend));
	wmb.get_active_window = _get_active_window;
	// wmb.move_to_nth_desktop = _move_to_nth_desktop;
	wmb.show = _show;
	wmb.close = _close;
	// wmb.kill = _kill;
	wmb.minimize = _minimize;
	// wmb.lower = _lower;
	wmb.maximize = _maximize;
	// wmb.set_fullscreen = _set_fullscreen;
	// wmb.set_above = _set_above;
	// wmb.set_minimize_position = _set_minimize_position;
	// wmb.set_thumbnail_area = _set_thumbnail_area;
	// wmb.set_window_border = _set_window_border;
	// wmb.get_icon_surface = _get_icon_surface;
	// wmb.get_thumbnail_surface = _get_thumbnail_surface;
	// wmb.get_texture = _get_texture;
	// wmb.get_transient_for = _get_transient_for;
	// wmb.is_above_or_below = _is_above_or_below;
	// wmb.is_sticky = _is_sticky;
	// wmb.set_sticky = _set_sticky;
	// wmb.can_minimize_maximize_close = _can_minimize_maximize_close;
	// wmb.get_id = _get_id;
	// wmb.pick_window = _pick_window;
	gldi_windows_manager_register_backend (&wmb);
	
	
	// register toplevel object manager
	// Object Manager
	memset (&myWFTObjectMgr, 0, sizeof (GldiObjectManager));
	myWFTObjectMgr.cName   = "Wayland-wlr-foreign-toplevel-manager";
	myWFTObjectMgr.iObjectSize    = sizeof (GldiWFTWindowActor);
	// interface
	myWFTObjectMgr.init_object    = init_object;
	myWFTObjectMgr.reset_object   = reset_object;
	// signals
	gldi_object_install_notifications (&myWFTObjectMgr, NB_NOTIFICATIONS_WFT_MANAGER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myWFTObjectMgr), &myWindowObjectMgr);
	
	zwlr_foreign_toplevel_manager_v1_add_listener(s_ptoplevel_manager, &gldi_toplevel_manager, NULL);
}


#endif

