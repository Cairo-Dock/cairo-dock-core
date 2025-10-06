/*
 * cairo-dock-wayland-hotspots.c
 * 
 * Functions to monitor screen edges for recalling a hidden dock.
 * 
 * Copyright 2021-2025 Daniel Kondor <kondor.dani@gmail.com>
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
#include "cairo-dock-wayland-hotspots.h"
#include "cairo-dock-wayland-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dock-priv.h"
#include "cairo-dock-log.h"
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
extern gboolean g_bDisableLayerShell;
#endif

#ifdef HAVE_WAYLAND_PROTOCOLS
#include "wayland-wayfire-shell-client-protocol.h"
#endif



typedef struct _hotspot_hit {
	GdkMonitor *monitor;
	CairoDockPositionType pos;
	gint timeout;
} hotspot_hit;

typedef struct _output_hotspots_base {
	GdkMonitor *monitor;
	guint hotspot_width[4]; // area of hotspots needed on each egde (0: none)
	guint hotspot_height[4]; // order corresponds to values in the CairoDockPositionType enum
	hotspot_hit hit[4]; // convenience structure used as parameter to the hotspot hit callback function
} output_hotspots_base;


/// interface of individual hotspot managers
typedef struct _hotspots_interface {
	output_hotspots_base* (*new_output) (GdkMonitor *monitor);
	void (*output_removed) (output_hotspots_base *output);
	void (*update_hotspot) (output_hotspots_base *base, int j);
	void (*destroy_hotspot) (output_hotspots_base *base, int j);
} hotspots_interface;

static hotspots_interface s_backend = {0};

/// Number of outputs and mapping from GdkMonitors to wf_outputs
static int s_iNumOutputs = 0;
static output_hotspots_base** outputs = NULL;
static int s_iOutputsSize = 0; // size of the above array



static GdkMonitor* _get_monitor_for_dock (CairoDock *pDock)
{
	int iNumScreen = pDock->iNumScreen;
	if (iNumScreen < 0) return NULL;
	int iNumMonitors = 0;
	GdkMonitor *const *pMonitors = gldi_desktop_get_monitors (&iNumMonitors);
	return (iNumScreen < iNumMonitors) ? pMonitors[iNumScreen] : NULL;
}

static void _hotspot_hit_cb (CairoDock *pDock, gpointer user_data)
{
	if (! pDock->bAutoHide && pDock->iVisibility != CAIRO_DOCK_VISI_KEEP_BELOW) return;
	
	const hotspot_hit *hit = (const hotspot_hit*)user_data;
	GdkMonitor *mon = _get_monitor_for_dock (pDock);
	if (!mon || mon != hit->monitor) return; // we only care if this dock is on this monitor
	CairoDockPositionType pos = gldi_wayland_get_edge_for_dock (pDock);
	if (pos != hit->pos) return; // we only care if this is the correct edge
	
	cd_debug ("Hotspot hit for dock %s\n", pDock->cDockName);
	cairo_dock_unhide_dock_delayed (pDock, 0); // will only have effect on idle
}

/***********************************************************************
 * wf-shell implementation                                             */

#ifdef HAVE_WAYLAND_PROTOCOLS
/// Private variables
static struct zwf_shell_manager_v2 *s_pshell_manager = NULL;
static uint32_t wf_shell_id, wf_shell_version;
static gboolean wf_shell_found = FALSE;
static guint s_sidMenu = 0;

typedef struct _wf_hotspots {
	struct _output_hotspots_base base;
	struct zwf_output_v2 *output;
	struct zwf_hotspot_v2 *hotspots[4]; // each output has four possible hotspots
} wf_hotspots;


static void _dummy (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwf_output_v2 *zwf_output_v2)
{
	
}

static gboolean _menu_request (G_GNUC_UNUSED void* ptr)
{
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_MENU_REQUEST);
	s_sidMenu = 0;
	return FALSE;
}

static void _toggle_menu (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwf_output_v2 *zwf_output_v2)
{
	cd_debug ("wayfire-shell: menu request\n");
	if (!s_sidMenu) s_sidMenu = g_idle_add (_menu_request, NULL);
}

static struct zwf_output_v2_listener output_listener = { _dummy, _dummy, _toggle_menu };

static output_hotspots_base *_wf_shell_new_output (GdkMonitor *monitor)
{
	wf_hotspots *ret = g_new0 (wf_hotspots, 1);
	ret->output = zwf_shell_manager_v2_get_wf_output (s_pshell_manager,
		gdk_wayland_monitor_get_wl_output (monitor));
	if (!ret->output)
	{
		g_free (ret);
		return NULL;
	}
	zwf_output_v2_add_listener (ret->output, &output_listener, NULL);
	return (output_hotspots_base*)ret;
}

static void _wf_shell_output_removed (output_hotspots_base *output)
{
	wf_hotspots *tmp = (wf_hotspots*)output;
	int j;
	for (j = 0; j < 4; j++) if (tmp->hotspots[j]) zwf_hotspot_v2_destroy (tmp->hotspots[j]);
	zwf_output_v2_destroy (tmp->output);
	g_free(tmp);
}

/// function called when a hotspot is triggered
static void _wf_shell_hotspot_enter_cb (void *data, G_GNUC_UNUSED struct zwf_hotspot_v2 *hotspot)
{
	if (!data) return;
	// check for each root dock if it is affected
	gldi_docks_foreach_root ((GFunc)_hotspot_hit_cb, data);
}

static void _wf_shell_hotspot_leave_cb (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwf_hotspot_v2 *hotspot)
{
	
}


struct zwf_hotspot_v2_listener hotspot_listener = {
	.enter = _wf_shell_hotspot_enter_cb,
	.leave = _wf_shell_hotspot_leave_cb
};

static uint32_t _wf_shell_get_anchor (int j)
{
	switch (j)
	{
		case CAIRO_DOCK_BOTTOM:
			return ZWF_OUTPUT_V2_HOTSPOT_EDGE_BOTTOM;
		case CAIRO_DOCK_TOP:
			return ZWF_OUTPUT_V2_HOTSPOT_EDGE_TOP;
		case CAIRO_DOCK_LEFT:
			return ZWF_OUTPUT_V2_HOTSPOT_EDGE_LEFT;
		case CAIRO_DOCK_RIGHT:
			return ZWF_OUTPUT_V2_HOTSPOT_EDGE_RIGHT;
		default:
			return 0; // will not happen since j goes from 0 to 3 in the caller
	}
}

/// Update the hotspots listened to for this dock if needed
static void _wf_shell_update_dock_hotspots (output_hotspots_base *base, int j)
{
	wf_hotspots *output = (wf_hotspots*)base;
	
	if (!output->hotspots[j])
	{
		uint32_t pos2 = _wf_shell_get_anchor (j);
		uint32_t thres = base->hotspot_height[j];
		
		output->hotspots[j] = zwf_output_v2_create_hotspot (output->output,
			pos2, thres, myDocksParam.iUnhideDockDelay);
		if (!output->hotspots[j]) return;
		
		cd_debug ("Created hotspot %p for output %p, position %d\n", output->hotspots[j], output->output, j);
		zwf_hotspot_v2_add_listener (output->hotspots[j], &hotspot_listener, base->hit + j);
	}
}

static void _wf_shell_destroy_hotspot (output_hotspots_base *base, int j)
{
	wf_hotspots *output = (wf_hotspots*)base;
	if (output->hotspots[j])
	{
		zwf_hotspot_v2_destroy (output->hotspots[j]);
		output->hotspots[j] = NULL;
	}
}
#endif //HAVE_WAYLAND_PROTOCOLS

/***********************************************************************
 * gtk-layer-shell implementation                                      */

#ifdef HAVE_GTK_LAYER_SHELL

typedef struct _layer_shell_hotspots {
	struct _output_hotspots_base base;
	GtkWidget *hotspots[4]; // each output has four possible hotspots, each hotspot has a corresponding GtkWindow
} layer_shell_hotspots;


static output_hotspots_base *_layer_shell_new_output (G_GNUC_UNUSED GdkMonitor* pMonitor)
{
	return (output_hotspots_base*) g_new0 (layer_shell_hotspots, 1);
}

static void _layer_shell_destroy_hotspot (output_hotspots_base *base, int j)
{
	cd_debug ("_layer_shell_destroy_hotspot\n");
	layer_shell_hotspots *output = (layer_shell_hotspots*)base;
	
	if (base->hit[j].timeout)
	{
		g_source_remove (base->hit[j].timeout);
		base->hit[j].timeout = 0;
	}
	if (output->hotspots[j])
	{
		gtk_widget_destroy (output->hotspots[j]);
		output->hotspots[j] = NULL;
	}
}

static void _layer_shell_output_removed (output_hotspots_base *output)
{
	int i;
	for (i = 0; i < 4; i++) _layer_shell_destroy_hotspot (output, i);
	g_free (output);
}

static gboolean _layer_shell_hotspot_trigger (hotspot_hit *hit)
{
	cd_debug ("_layer_shell_hotspot_trigger\n");
	if (hit) gldi_docks_foreach_root ((GFunc)_hotspot_hit_cb, hit);
	hit->timeout = 0;
	return FALSE;
}

static void _layer_shell_enter (GtkWidget* pWidget, G_GNUC_UNUSED GdkEventCrossing* pEvent, layer_shell_hotspots *output)
{
	cd_debug ("_layer_shell_enter\n");
	int j;
	for (j = 0; j < 4; j++) if (output->hotspots[j] == pWidget) break;
	if (j == 4) return;
	
	output->base.hit[j].timeout = g_timeout_add (myDocksParam.iUnhideDockDelay, (GSourceFunc)_layer_shell_hotspot_trigger, output->base.hit + j);
}

static void _layer_shell_leave (GtkWidget* pWidget, G_GNUC_UNUSED GdkEventCrossing* pEvent, layer_shell_hotspots *output)
{
	cd_debug ("_layer_shell_leave\n");
	int j;
	for (j = 0; j < 4; j++) if (output->hotspots[j] == pWidget) break;
	if (j == 4) return;
	
	if (output->base.hit[j].timeout)
	{
		g_source_remove (output->base.hit[j].timeout);
		output->base.hit[j].timeout = 0;
	}
}


static uint32_t _layer_shell_get_anchor (int j)
{
	switch (j)
	{
		case CAIRO_DOCK_BOTTOM:
			return GTK_LAYER_SHELL_EDGE_BOTTOM;
		case CAIRO_DOCK_TOP:
			return GTK_LAYER_SHELL_EDGE_TOP;
		case CAIRO_DOCK_LEFT:
			return GTK_LAYER_SHELL_EDGE_LEFT;
		case CAIRO_DOCK_RIGHT:
			return GTK_LAYER_SHELL_EDGE_RIGHT;
		default:
			return 0; // will not happen since j goes from 0 to 3 in the caller
	}
}

static void _layer_shell_update_hotspot (output_hotspots_base *base, int j)
{
	layer_shell_hotspots *output = (layer_shell_hotspots*)base;
	gint W, H;
	
	if (j == CAIRO_DOCK_LEFT || j == CAIRO_DOCK_RIGHT)
	{
		W = base->hotspot_height[j];
		H = base->hotspot_width[j];
	}
	else
	{
		H = base->hotspot_height[j];
		W = base->hotspot_width[j];
	}
	
	if (output->hotspots[j])
	{
		gint w, h;
		gtk_window_get_size (GTK_WINDOW (output->hotspots[j]), &w, &h);
		if (w != W || h != H) gtk_window_resize (GTK_WINDOW (output->hotspots[j]), W, H);
		return;
	}
	else
	{
		cd_debug ("creating hotspot (%d, %dx%d)\n", j, W, H);
		
		GtkWidget *widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		GtkWindow *window = GTK_WINDOW (widget);
		if (!widget) return;
		
		gtk_layer_init_for_window (window);
		gtk_window_set_default_size(window, W, H);
		gtk_widget_set_size_request(widget, W, H);
		gtk_widget_set_app_paintable (widget, TRUE);
		gtk_window_set_decorated (window, FALSE);
		gtk_widget_set_opacity (widget, 0.); // not sure if this is needed (by default, the window is transparent?)
		
		gtk_layer_set_keyboard_mode (window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
		gtk_layer_set_namespace (window, "cairo-dock-edge-hotspot");
		gtk_layer_set_monitor (window, base->monitor);
		gtk_layer_set_anchor (window, _layer_shell_get_anchor (j), TRUE);
		gtk_layer_set_exclusive_zone (window, -1); // -1 means keep at the screen edge even if there are other layer-shell surfaces there
		gtk_layer_set_layer (window, GTK_LAYER_SHELL_LAYER_TOP);

		gtk_widget_add_events (widget, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
		g_signal_connect (G_OBJECT (widget), "enter-notify-event", G_CALLBACK (_layer_shell_enter), output);
		g_signal_connect (G_OBJECT (widget), "leave-notify-event", G_CALLBACK (_layer_shell_leave), output);
	
		
		output->hotspots[j] = widget;
		
		gtk_widget_show_all (widget);
	}
}
#endif // HAVE_GTK_LAYER_SHELL

/***********************************************************************
 * shared functions                                                    */

static gboolean _monitor_added (G_GNUC_UNUSED gpointer user_data, GdkMonitor *monitor)
{
	int i = s_iNumOutputs;
	s_iNumOutputs++;
	if (s_iNumOutputs > s_iOutputsSize)
	{
		outputs = g_renew(output_hotspots_base*, outputs, s_iNumOutputs);
		s_iOutputsSize = s_iNumOutputs;
	}
	
	outputs[i] = s_backend.new_output (monitor);
	if (outputs[i])
	{
		outputs[i]->monitor = monitor;
		for (int j = 0; j < 4; j++)
		{
			outputs[i]->hit[j].monitor = monitor;
			outputs[i]->hit[j].pos = j;
		}
	}
	else s_iNumOutputs--;
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _monitor_removed (G_GNUC_UNUSED gpointer user_data, GdkMonitor *monitor)
{
	int i;
	for (i = 0; i < s_iNumOutputs; i++) if(outputs[i]->monitor == monitor) break;
	if (i == s_iNumOutputs) {
		/// TODO: error handling
		return GLDI_NOTIFICATION_LET_PASS;
	}
	
	s_backend.output_removed (outputs[i]);
	
	s_iNumOutputs--;
	if (i < s_iNumOutputs) outputs[i] = outputs[s_iNumOutputs];
	
	return GLDI_NOTIFICATION_LET_PASS;
}

/// Update the hotspots listened to for this dock if needed
static void _update_dock_hotspots (CairoDock *pDock, G_GNUC_UNUSED gpointer user_data)
{
	if (! (pDock->bAutoHide && pDock->fHideOffset > 0.0) && ! pDock->bIsBelow) return;
	
	CairoDockPositionType pos = gldi_wayland_get_edge_for_dock (pDock);
	if (pos == CAIRO_DOCK_INSIDE_SCREEN) return;
	
	GdkMonitor *mon = _get_monitor_for_dock (pDock);
	if (!mon) return;
	
	int i;
	for (i = 0; i < s_iNumOutputs; i++) if (outputs[i]->monitor == mon) break;
	if (i == s_iNumOutputs) return; // TODO: error message here?
	
	/// note: if multiple docks request a hotspot on the same screen edge,
	/// the hotspot area will be the union of individual areas which is
	/// likely not what we would want
	uint32_t thres = (myDocksParam.iCallbackMethod == CAIRO_HIT_ZONE) ? 
		myDocksParam.iZoneHeight : 1;
	if (thres > outputs[i]->hotspot_height[pos]) outputs[i]->hotspot_height[pos] = thres;
	thres = (myDocksParam.iCallbackMethod == CAIRO_HIT_ZONE) ? 
		myDocksParam.iZoneWidth : gldi_dock_get_screen_width(pDock);
	if (thres > outputs[i]->hotspot_width[pos]) outputs[i]->hotspot_width[pos] = thres;
}

/// Update the hotspots we are listening to (based on the configuration of all root docks)
void gldi_wayland_hotspots_update (void)
{
	int i, j;
	/// reset flag to false for all hotspots
	for (i = 0; i < s_iNumOutputs; i++) for (j = 0; j < 4; j++) 
	{
		outputs[i]->hotspot_height[j] = 0;
		outputs[i]->hotspot_width[j] = 0;
	}
	
	gldi_docks_foreach_root ((GFunc) _update_dock_hotspots, NULL);
	
	for (i = 0; i < s_iNumOutputs; i++) for (j = 0; j < 4; j++)
	{
		if (outputs[i]->hotspot_height[j] > 0) s_backend.update_hotspot (outputs[i], j);
		else s_backend.destroy_hotspot (outputs[i], j);
	}
}


/// Try to match Wayland protocols that we need to use
gboolean gldi_wayland_hotspots_match_protocol (uint32_t id, const char *interface, uint32_t version)
{
#ifdef HAVE_WAYLAND_PROTOCOLS
	if (!strcmp (interface, zwf_shell_manager_v2_interface.name))
	{
		wf_shell_id = id;
		wf_shell_version = version;
		wf_shell_found = TRUE;
		return TRUE;
	}
#else
	// avoid warnings for unused variables
	(void)id;
	(void)interface;
	(void)version;
#endif
	return FALSE;
}

/// Try to init based on the protocols found
GldiWaylandHotspotsType gldi_wayland_hotspots_try_init (struct wl_registry *registry)
{
	GldiWaylandHotspotsType type = GLDI_WAYLAND_HOTSPOTS_NONE;
	
#ifdef HAVE_WAYLAND_PROTOCOLS
	if (wf_shell_found)
	{
		if (wf_shell_version > (uint32_t)zwf_shell_manager_v2_interface.version)
		{
			wf_shell_version = zwf_shell_manager_v2_interface.version;
		}
        s_pshell_manager = wl_registry_bind (registry, wf_shell_id, &zwf_shell_manager_v2_interface, wf_shell_version);
		if (s_pshell_manager)
		{
			s_backend.new_output = _wf_shell_new_output;
			s_backend.output_removed = _wf_shell_output_removed;
			s_backend.update_hotspot = _wf_shell_update_dock_hotspots;
			s_backend.destroy_hotspot = _wf_shell_destroy_hotspot;
			type = GLDI_WAYLAND_HOTSPOTS_WAYFIRE;
		}
		else cd_message ("Could not bind zwf-shell!");
    }
#else
	// avoid warning for unused variable
	(void)registry;
#endif

#ifdef HAVE_GTK_LAYER_SHELL
    if (type == GLDI_WAYLAND_HOTSPOTS_NONE && !g_bDisableLayerShell && gtk_layer_is_supported ())
    {
		s_backend.new_output = _layer_shell_new_output;
		s_backend.output_removed = _layer_shell_output_removed;
		s_backend.update_hotspot = _layer_shell_update_hotspot;
		s_backend.destroy_hotspot = _layer_shell_destroy_hotspot;
		type = GLDI_WAYLAND_HOTSPOTS_LAYER_SHELL;
	}
#endif
	
    if (type != GLDI_WAYLAND_HOTSPOTS_NONE)
    {
		gldi_object_register_notification (&myDesktopMgr, NOTIFICATION_DESKTOP_MONITOR_ADDED, (GldiNotificationFunc)_monitor_added, GLDI_RUN_FIRST, NULL);
		gldi_object_register_notification (&myDesktopMgr, NOTIFICATION_DESKTOP_MONITOR_REMOVED, (GldiNotificationFunc)_monitor_removed, GLDI_RUN_FIRST, NULL);
		
		int iNumMonitors = 0;
		GdkMonitor *const *monitors = gldi_desktop_get_monitors (&iNumMonitors);
		int i;
		for (i = 0; i < iNumMonitors; i++) _monitor_added (NULL, monitors[i]);
	}
    return type;
}

#endif // HAVE_WAYLAND

