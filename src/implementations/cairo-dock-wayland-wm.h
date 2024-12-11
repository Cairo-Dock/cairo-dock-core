/*
 * cairo-dock-wayland-wm.h -- common interface for window / taskbar
 * 	management facilities on Wayland
 * 
 * Copyright 2020-2023 Daniel Kondor <kondor.dani@gmail.com>
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


#ifndef CAIRO_DOCK_WAYLAND_WM_H
#define CAIRO_DOCK_WAYLAND_WM_H

#include <wayland-client.h>
#include <stdint.h>
#include "cairo-dock-struct.h"

struct _GldiWaylandWindowActor {
	GldiWindowActor actor;
	// foreign-toplevel specific handle to toplevel object
	gpointer handle;
	// store parent handle, if supplied
	// NOTE: windows with parent are ignored (not shown in the taskbar)
	gpointer parent;
	
	gchar* cClassPending; // new app_id received
	gchar* cTitlePending; // new title received
	
	gboolean maximized_pending; // maximized state received
	gboolean minimized_pending; // minimized state received
	gboolean fullscreen_pending; // fullscreen state received
	gboolean attention_pending; // needs-attention state received (only KDE)
	gboolean skip_taskbar; // should not be shown in taskbar (only KDE)
	gboolean sticky_pending; // sticky state received (only KDE)
	gboolean close_pending; // this window has been closed
	gboolean unfocused_pending; // this window has lost focus
	
	gboolean init_done; // initial state has been configured
	gboolean in_queue; // this actor has been added to the s_pending_queue
};
typedef struct _GldiWaylandWindowActor GldiWaylandWindowActor;

// manager for the above, can be extended by more specific implementations
extern GldiObjectManager myWaylandWMObjectMgr;


// functions to update the state of a toplevel and potentially signal
// the taskbar of the changes

void gldi_wayland_wm_title_changed (GldiWaylandWindowActor *wactor, const char *title, gboolean notify);

// note: this might create / remove the corresponding icon
void gldi_wayland_wm_appid_changed (GldiWaylandWindowActor *wactor, const char *app_id, gboolean notify);

void gldi_wayland_wm_maximized_changed (GldiWaylandWindowActor *wactor, gboolean maximized, gboolean notify);
void gldi_wayland_wm_minimized_changed (GldiWaylandWindowActor *wactor, gboolean minimized, gboolean notify);
void gldi_wayland_wm_fullscreen_changed (GldiWaylandWindowActor *wactor, gboolean fullscreen, gboolean notify);
void gldi_wayland_wm_attention_changed (GldiWaylandWindowActor *wactor, gboolean attention, gboolean notify);
void gldi_wayland_wm_skip_changed (GldiWaylandWindowActor *wactor, gboolean skip, gboolean notify);
void gldi_wayland_wm_sticky_changed (GldiWaylandWindowActor *wactor, gboolean sticky, gboolean notify);
void gldi_wayland_wm_activated (GldiWaylandWindowActor *wactor, gboolean activated, gboolean notify);

void gldi_wayland_wm_closed (GldiWaylandWindowActor *wactor, gboolean notify);

void gldi_wayland_wm_done (GldiWaylandWindowActor *wactor);

GldiWindowActor* gldi_wayland_wm_get_active_window ();

GldiWindowActor* gldi_wayland_wm_get_last_active_window ();

GldiWindowActor* gldi_wayland_wm_pick_window (GtkWindow *pParentWindow);

/** Change the stacking order such that actor is on top. Does not send
 *  a notification; the user should do that manually, or call one of
 *  the above functions with notify == TRUE.
 *@param actor  The window to put on top.
 */
void gldi_wayland_wm_stack_on_top (GldiWindowActor *actor);

typedef void (*GldiWaylandWMHandleDestroyFunc)(gpointer handle);
void gldi_wayland_wm_init (GldiWaylandWMHandleDestroyFunc destroy_cb);

GldiWaylandWindowActor* gldi_wayland_wm_new_toplevel (gpointer handle);



#endif // CAIRO_DOCK_WAYLAND_WM_H

