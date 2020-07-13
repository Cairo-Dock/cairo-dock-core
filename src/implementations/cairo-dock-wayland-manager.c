/**
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

#include "gldi-config.h"
#ifdef HAVE_WAYLAND

#include <poll.h>

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-utils.h"
#include "cairo-dock-log.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.iMinimizedWindowRenderType
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-class-manager.h"  // gldi_class_startup_notify_end
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-container.h"  // GldiContainerManagerBackend
#include "cairo-dock-egl.h"
#define _MANAGER_DEF_
#include "cairo-dock-wayland-manager.h"

#include "gldi-config.h"
#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
static gboolean s_bHave_Layer_Shell = FALSE;

gboolean g_bDisableLayerShell = FALSE;

#endif


// public (manager, config, data)
GldiManager myWaylandMgr;
GldiObjectManager myWaylandObjectMgr;

// dependencies
extern GldiContainer *g_pPrimaryContainer;

// private
static struct wl_display *s_pDisplay = NULL;

// signals
typedef enum {
	NB_NOTIFICATIONS_WAYLAND_MANAGER = NB_NOTIFICATIONS_WINDOWS
	} CairoWaylandManagerNotifications;

// data
typedef struct _GldiWaylandWindowActor GldiWaylandWindowActor;
struct _GldiWaylandWindowActor {
	GldiWindowActor actor;
	// Wayland-specific
	struct wl_shell_surface *shell_surface;
};

struct desktop
{
	int iCurrentIndex;
	struct wl_output *wl_output;
};
static gboolean s_bInitializing = TRUE;  // each time a callback is called on startup, it will set this to TRUE, and we'll make a roundtrip to the server until no callback is called.

static void _output_geometry_cb (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct wl_output *wl_output,
	int32_t x, int32_t y,
	G_GNUC_UNUSED int32_t physical_width, G_GNUC_UNUSED int32_t physical_height,
	G_GNUC_UNUSED int32_t subpixel, G_GNUC_UNUSED const char *make, G_GNUC_UNUSED const char *model, G_GNUC_UNUSED int32_t output_transform)
{
	cd_debug ("Geometry: %d;%d", x, y);
	g_desktopGeometry.iNbScreens ++;
	if (!g_desktopGeometry.pScreens)
		g_desktopGeometry.pScreens = g_new0 (GtkAllocation, 1);
	else
		g_desktopGeometry.pScreens = g_realloc (g_desktopGeometry.pScreens, g_desktopGeometry.iNbScreens * sizeof(GtkAllocation));
	
	g_desktopGeometry.pScreens[g_desktopGeometry.iNbScreens-1].x = x;
	g_desktopGeometry.pScreens[g_desktopGeometry.iNbScreens-1].y = y;
	s_bInitializing = TRUE;
}

static void _output_mode_cb (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct wl_output *wl_output,
	uint32_t flags, int32_t width, int32_t height, G_GNUC_UNUSED int32_t refresh)
{
	cd_debug ("Output mode: %dx%d, %d", width, height, flags);
	if (flags & WL_OUTPUT_MODE_CURRENT)  // not the current one -> don't bother
	{
		g_desktopGeometry.pScreens[g_desktopGeometry.iNbScreens-1].width = width;
		g_desktopGeometry.pScreens[g_desktopGeometry.iNbScreens-1].height = height;
		g_desktopGeometry.Xscreen.width = width;
		g_desktopGeometry.Xscreen.height = height;
	}
	/// TODO: we should keep the other resolutions so that we can provide them (xrandr-like)...
	
	/// TODO: also, we have to compute the logical Xscreen with all the outputs, taking into account their position and size...
	
	
	s_bInitializing = TRUE;
}

static void _output_done_cb (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct wl_output *wl_output)
{
	cd_debug ("output done");
	s_bInitializing = TRUE;
}

static void _output_scale_cb (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct wl_output *wl_output,
	int32_t factor)
{
	cd_debug ("output scaled : %d", factor);
	s_bInitializing = TRUE;
}

static const struct wl_output_listener output_listener = {
	_output_geometry_cb,
	_output_mode_cb,
	_output_done_cb,
	_output_scale_cb
};

static void _registry_global_cb (G_GNUC_UNUSED void *data, struct wl_registry *registry, uint32_t id, const char *interface, G_GNUC_UNUSED uint32_t version)
{
	cd_debug ("got a new global object, instance of %s, id=%d", interface, id);
	if (!strcmp (interface, "wl_shell"))
	{
		// this is the global that should give us info and signals about the desktop, but currently it's pretty useless ...
		
	}
	else if (!strcmp (interface, "wl_output"))  // global object "wl_output" is now available, create a proxy for it
	{
		struct wl_output *output = wl_registry_bind (registry,
			id,
			&wl_output_interface,
			1);
		wl_output_add_listener (output,
			&output_listener,
			NULL);
	}
#ifdef HAVE_GTK_LAYER_SHELL
	else if (!strcmp (interface, "zwlr_layer_shell_v1"))
	{
		if (!g_bDisableLayerShell)
			s_bHave_Layer_Shell = TRUE;
	}
#endif
	s_bInitializing = TRUE;
}

static void _registry_global_remove_cb (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct wl_registry *registry, uint32_t id)
{
	cd_debug ("got a global object has disappeared: id=%d", id);
	/// TODO: find it and destroy it...
	
	/// TODO: and if it was a wl_output for instance, update the desktop geometry...
	
}

static const struct wl_registry_listener registry_listener = {
	_registry_global_cb,
	_registry_global_remove_cb
};


#ifdef HAVE_GTK_LAYER_SHELL
/// reserve space for the dock; note: most parameters are ignored, only a size is calculated based on coordinates
static void _layer_shell_reserve_space (GldiContainer *pContainer, int left, int right, int top, int bottom,
		G_GNUC_UNUSED int left_start_y, G_GNUC_UNUSED int left_end_y, G_GNUC_UNUSED int right_start_y, G_GNUC_UNUSED int right_end_y,
		G_GNUC_UNUSED int top_start_x, G_GNUC_UNUSED int top_end_x, G_GNUC_UNUSED int bottom_start_x, G_GNUC_UNUSED int bottom_end_x)
{
	GtkWindow* window = GTK_WINDOW (pContainer->pWidget);
	gint r = (pContainer->bIsHorizontal) ? (top - bottom) : (right - left);
	if (r < 0) {
		r *= -1;
	}
	gtk_layer_set_exclusive_zone (window, r);
}

static void _set_layer_shell_anchor (GldiContainer *pContainer, CairoDockPositionType iScreenBorder)
{
	GtkWindow* window = GTK_WINDOW (pContainer->pWidget);
	// Reset old anchors
	gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
	gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, FALSE);
	gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
	gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
	// Set new anchor
	switch (iScreenBorder)
	{
		case CAIRO_DOCK_BOTTOM :
			gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
		break;
		case CAIRO_DOCK_TOP :
			gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
		break;
		case CAIRO_DOCK_RIGHT :
			gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
		break;
		case CAIRO_DOCK_LEFT :
			gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
		break;
		case CAIRO_DOCK_INSIDE_SCREEN :
		case CAIRO_DOCK_NB_POSITIONS :
		break;
	}
}

void _set_layer_shell_layer (GldiContainer *pContainer, GldiContainerLayer iLayer)
{
	GtkWindow* window = GTK_WINDOW (pContainer->pWidget);
	gtk_layer_set_layer (window, iLayer == CAIRO_DOCK_LAYER_TOP ? 	
		GTK_LAYER_SHELL_LAYER_TOP : GTK_LAYER_SHELL_LAYER_BOTTOM);
}

void _layer_shell_init_for_window (GldiContainer *pContainer)
{
	GtkWindow* window = GTK_WINDOW (pContainer->pWidget);
	if (gtk_window_get_transient_for (window))
	{
		// subdock or other surface with a parent
		// we need to call gdk_move_to_rect(), but specifically
		// (1) before the window is mapped, but (2) during it is realized
		// A dummy call to gdk_window_move_to_rect() causes the gtk-layer-shell
		// internals related to this window to be initialized properly (note:
		// this is important if the parent is a "proper" layer-shell window).
		// This _has_ to happen before the GtkWindow is mapped first, but also
		// has to happen after some initial setup. Doing this as a response to
		// a "realize" signal seems to be a good way, but there should be some
		// less hacky solution for this as well. Maybe I'm missing something here?
		// g_signal_connect (window, "realize", G_CALLBACK (sublayer_realize_cb), NULL);
		
		// gldi_container_move_to_rect() will ensure this
		GdkRectangle rect = {0, 0, 1, 1};
		gldi_container_move_to_rect (pContainer, &rect, GDK_GRAVITY_SOUTH,
			GDK_GRAVITY_NORTH_WEST, GDK_ANCHOR_SLIDE, 0, 0);
	}
	else
	{
		gtk_layer_init_for_window (window);
		gtk_layer_set_namespace (window, "cairo-dock");
	}
}
#endif

static gboolean _is_wayland() { return TRUE; }

static void init (void)
{
	//\__________________ listen for Wayland events
	s_pDisplay = wl_display_connect (NULL);
	
	g_desktopGeometry.iNbDesktops = g_desktopGeometry.iNbViewportX = g_desktopGeometry.iNbViewportY = 1;
	
	
	struct wl_registry *registry = wl_display_get_registry (s_pDisplay);
	wl_registry_add_listener (registry, &registry_listener, NULL);
	do
	{
		s_bInitializing = FALSE;
		wl_display_roundtrip (s_pDisplay);
	}
	while (s_bInitializing);
	
	GldiContainerManagerBackend cmb;
	memset (&cmb, 0, sizeof (GldiContainerManagerBackend));	
#ifdef HAVE_GTK_LAYER_SHELL
	if (s_bHave_Layer_Shell)
	{
		cmb.reserve_space = _layer_shell_reserve_space;
		cmb.set_anchor = _set_layer_shell_anchor;
		cmb.set_layer = _set_layer_shell_layer;
		cmb.init_layer = _layer_shell_init_for_window;
	}
#endif
	cmb.is_wayland = _is_wayland;
	gldi_container_manager_register_backend (&cmb);
	
	gldi_register_egl_backend ();
}

void gldi_register_wayland_manager (void)
{
	#ifdef GDK_WINDOWING_WAYLAND  // if GTK doesn't support Wayland, there is no point in trying
	GdkDisplay *dsp = gdk_display_get_default ();  // let's GDK do the guess
	if (! GDK_IS_WAYLAND_DISPLAY (dsp))  // if not an Wayland session
	#endif
	{
		cd_message ("Not an Wayland session");
		return;
	}
	
	// Manager
	memset (&myWaylandMgr, 0, sizeof (GldiManager));
	myWaylandMgr.cModuleName   = "X";
	myWaylandMgr.init          = init;
	myWaylandMgr.load          = NULL;
	myWaylandMgr.unload        = NULL;
	myWaylandMgr.reload        = (GldiManagerReloadFunc)NULL;
	myWaylandMgr.get_config    = (GldiManagerGetConfigFunc)NULL;
	myWaylandMgr.reset_config  = (GldiManagerResetConfigFunc)NULL;
	// Config
	myWaylandMgr.pConfig = (GldiManagerConfigPtr)NULL;
	myWaylandMgr.iSizeOfConfig = 0;
	// data
	myWaylandMgr.iSizeOfData = 0;
	myWaylandMgr.pData = (GldiManagerDataPtr)NULL;
	// register
	gldi_object_init (GLDI_OBJECT(&myWaylandMgr), &myManagerObjectMgr, NULL);
	
	// Object Manager
	memset (&myWaylandObjectMgr, 0, sizeof (GldiObjectManager));
	myWaylandObjectMgr.cName   = "Wayland";
	myWaylandObjectMgr.iObjectSize    = sizeof (GldiWaylandWindowActor);
	// interface
	///myWaylandObjectMgr.init_object    = init_object;
	///myWaylandObjectMgr.reset_object   = reset_object;
	// signals
	gldi_object_install_notifications (&myWaylandObjectMgr, NB_NOTIFICATIONS_WAYLAND_MANAGER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myWaylandObjectMgr), &myWindowObjectMgr);
}

#else
#include "cairo-dock-log.h"
void gldi_register_wayland_manager (void)
{
	cd_message ("Cairo-Dock was not built with Wayland support");
}
#endif
