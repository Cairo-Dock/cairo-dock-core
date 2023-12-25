/*
 * cairo-dock-wayfire-shell.c
 * 
 * Functions to interact with Wayfire for screen edge hotspots.
 * 
 * Copyright 2021 Daniel Kondor <kondor.dani@gmail.com>
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

#include "wayland-wayfire-shell-client-protocol.h"
#include "cairo-dock-wayfire-shell.h"
#include "cairo-dock-wayland-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-log.h"
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>

typedef struct _output_hotspots {
	struct zwf_output_v2 *output;
	struct zwf_hotspot_v2 *hotspots[4]; // each output has four possible hotspots
	gboolean need_hotspot[4]; // indicator whether we need a hotspot
	// order corresponds to values in the CairoDockPositionType enum
} output_hotspots;


/// Private variables

/// Number of outputs and mapping from GdkMonitors to wf_outputs
static int s_iNumOutputs = 0;
static GdkMonitor** s_pMonitors = NULL;
static output_hotspots** outputs = NULL;

static struct zwf_shell_manager_v2 *s_pshell_manager = NULL;


static void _dummy (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwf_output_v2 *zwf_output_v2)
{
	
}

static struct zwf_output_v2_listener output_listener = { _dummy, _dummy };

static void _monitor_added (G_GNUC_UNUSED gpointer user_data, GdkMonitor *monitor)
{
	if (!s_pshell_manager) return;
	
	int i = s_iNumOutputs;
	s_iNumOutputs++;
	s_pMonitors = g_renew(GdkMonitor*, s_pMonitors, s_iNumOutputs);
	s_pMonitors[i] = monitor;
	
	outputs = g_renew(output_hotspots*, outputs, s_iNumOutputs);
	outputs[i] = g_new0(output_hotspots, 1);
	outputs[i]->output = zwf_shell_manager_v2_get_wf_output (s_pshell_manager,
		gdk_wayland_monitor_get_wl_output (monitor));
	if (!outputs[i]->output)
	{
		/// TODO: error handling
		return;
	}
	
	zwf_output_v2_add_listener (outputs[i]->output, &output_listener, NULL);
}

static void _monitor_removed (G_GNUC_UNUSED gpointer user_data, GdkMonitor *monitor)
{
	int i, j;
	for (i = 0; i < s_iNumOutputs; i++) if(s_pMonitors[i] == monitor) break;
	if (i == s_iNumOutputs) {
		/// TODO: error handling
		return;
	}
	
	for (j = 0; j < 4; j++) if (outputs[i]->hotspots[j]) zwf_hotspot_v2_destroy (outputs[i]->hotspots[j]);
	zwf_output_v2_destroy (outputs[i]->output);
	g_free(outputs[i]);
	
	s_iNumOutputs--;
	if (i < s_iNumOutputs)
	{
		s_pMonitors[i] = s_pMonitors[s_iNumOutputs];
		outputs[i] = outputs[s_iNumOutputs];
	}
}

/// Init function: install notifications for adding / removing monitors
static void _init()
{
	gldi_object_register_notification (&myWaylandMgr, NOTIFICATION_WAYLAND_MONITOR_ADDED, (GldiNotificationFunc)_monitor_added, GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWaylandMgr, NOTIFICATION_WAYLAND_MONITOR_REMOVED, (GldiNotificationFunc)_monitor_removed, GLDI_RUN_FIRST, NULL);
	
	int iNumMonitors = 0;
	GdkMonitor *const *monitors = gldi_wayland_get_monitors (&iNumMonitors);
	int i;
	for (i = 0; i < iNumMonitors; i++) _monitor_added (NULL, monitors[i]);
}

/// Try to bind the wayfire shell manager object (should be called for objects in the wl_registry)
gboolean gldi_zwf_shell_try_bind (struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp (interface, zwf_shell_manager_v2_interface.name))
	{
		if (version > (uint32_t)zwf_shell_manager_v2_interface.version)
		{
			version = zwf_shell_manager_v2_interface.version;
		}
        s_pshell_manager = wl_registry_bind (registry, id, &zwf_shell_manager_v2_interface, version);
		if (s_pshell_manager)
		{
			_init();
			return TRUE;
		}
		else cd_message ("Could not bind zwf-shell!");
    }
    return FALSE;
}

typedef struct _hotspot_hit {
	int iMonitor;
	CairoDockPositionType pos;
} hotspot_hit;


static void _hotspot_hit_cb (CairoDock *pDock, gpointer user_data)
{
	if (! pDock->bAutoHide && pDock->iVisibility != CAIRO_DOCK_VISI_KEEP_BELOW) return;
	
	const hotspot_hit *hit = (const hotspot_hit*)user_data;
	GdkMonitor *mon = gldi_dock_wayland_get_monitor (pDock);
	if (mon != s_pMonitors[hit->iMonitor]) return; // we only care if this dock is on this monitor
	CairoDockPositionType pos = gldi_wayland_get_edge_for_dock (pDock);
	if (pos != hit->pos) return; // we only care if this is the correct edge
	
	// fprintf (stderr, "Hotspot hit for dock %s\n", pDock->cDockName);
	cairo_dock_unhide_dock_delayed (pDock, 0);
}

/// function called when a hotspot is triggered
static void _hotspot_enter_cb (void *data, struct zwf_hotspot_v2 *hotspot)
{
	if (!(hotspot && data)) return;
	output_hotspots* output = (output_hotspots*)data;
	
	hotspot_hit hit;
	hit.iMonitor = 0;
	hit.pos = 0;
	
	for (; hit.iMonitor < s_iNumOutputs; hit.iMonitor++) if (outputs[hit.iMonitor] == output) break;
	if (hit.iMonitor == s_iNumOutputs) return; // TODO: error message
	
	for (; hit.pos < 4; hit.pos++) if (outputs[hit.iMonitor]->hotspots[hit.pos] == hotspot) break;
	if (hit.pos == 4) return; // TODO: error message
	
	// fprintf (stderr, "Hotspot %p hit on output %p, position %d\n", hotspot, output, hit.pos);
	
	// check for each root dock if it is affected
	gldi_docks_foreach_root ((GFunc)_hotspot_hit_cb, &hit);
}

static void _hotspot_leave_cb (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwf_hotspot_v2 *hotspot)
{
	
}


struct zwf_hotspot_v2_listener hotspot_listener = {
	.enter = _hotspot_enter_cb,
	.leave = _hotspot_leave_cb
};

/// Update the hotspots listened to for this dock if needed
static void _update_dock_hotspots (CairoDock *pDock, G_GNUC_UNUSED gpointer user_data)
{
	if (! pDock->bAutoHide && pDock->iVisibility != CAIRO_DOCK_VISI_KEEP_BELOW) return;
	
	CairoDockPositionType pos = gldi_wayland_get_edge_for_dock (pDock);
	if (pos == CAIRO_DOCK_INSIDE_SCREEN) return;
	
	GdkMonitor *mon = gldi_dock_wayland_get_monitor (pDock);
	if (!mon) return;
	
	int i;
	for (i = 0; i < s_iNumOutputs; i++) if (s_pMonitors[i] == mon) break;
	if (i == s_iNumOutputs) return; // TODO: error message here?
	
	outputs[i]->need_hotspot[pos] = TRUE;
	if (!outputs[i]->hotspots[pos])
	{
		uint32_t pos2 = 0;
		switch (pos)
		{
			case CAIRO_DOCK_BOTTOM:
				pos2 = ZWF_OUTPUT_V2_HOTSPOT_EDGE_BOTTOM;
				break;
			case CAIRO_DOCK_TOP:
				pos2 = ZWF_OUTPUT_V2_HOTSPOT_EDGE_TOP;
				break;
			case CAIRO_DOCK_LEFT:
				pos2 = ZWF_OUTPUT_V2_HOTSPOT_EDGE_LEFT;
				break;
			case CAIRO_DOCK_RIGHT:
				pos2 = ZWF_OUTPUT_V2_HOTSPOT_EDGE_RIGHT;
				break;
			case CAIRO_DOCK_INSIDE_SCREEN:
			case CAIRO_DOCK_NB_POSITIONS:
				// this case is already handled above
				break;
		}
		
		uint32_t thres = (myDocksParam.iCallbackMethod == CAIRO_HIT_ZONE) ? 
			myDocksParam.iZoneHeight : 1;
		
		outputs[i]->hotspots[pos] = zwf_output_v2_create_hotspot (outputs[i]->output,
			pos2, thres, myDocksParam.iUnhideDockDelay);
		if (!outputs[i]->hotspots[pos]) return;
		
		// fprintf (stderr, "Created hotspot %p for output %p, position %d\n", outputs[i]->hotspots[pos], outputs[i]->output, pos);
		zwf_hotspot_v2_add_listener (outputs[i]->hotspots[pos], &hotspot_listener, outputs[i]);
	}
}

/// Update the hotspots we are listening to (based on the configuration of all root docks)
void gldi_zwf_shell_update_hotspots (void)
{
	int i, j;
	/// reset flag to false for all hotspots
	for (i = 0; i < s_iNumOutputs; i++) for (j = 0; j < 4; j++) outputs[i]->need_hotspot[j] = FALSE;
	
	gldi_docks_foreach_root ((GFunc) _update_dock_hotspots, NULL);
	
	for (i = 0; i < s_iNumOutputs; i++) for (j = 0; j < 4; j++)
		if(!outputs[i]->need_hotspot[j] && outputs[i]->hotspots[j])
		{
			// fprintf (stderr, "Destroying hotspot %p for output %p, position %d\n", outputs[i]->hotspots[j], outputs[i]->output, j);
			zwf_hotspot_v2_destroy (outputs[i]->hotspots[j]);
			outputs[i]->hotspots[j] = NULL;
		}
}

/// Stop listening to all hotspots
void gldi_zwf_manager_stop (void)
{
	int i, j;
	for (i = 0; i < s_iNumOutputs; i++) for (j = 0; j < 4; j++)
		if (outputs[i]->hotspots[j])
		{
			// fprintf (stderr, "Destroying hotspot %p for output %p, position %d\n", outputs[i]->hotspots[j], outputs[i]->output, j);
			zwf_hotspot_v2_destroy (outputs[i]->hotspots[j]);
			outputs[i]->hotspots[j] = NULL;
		}
}


