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
