/*
 * cairo-dock-wayland-hotspots.c
 * 
 * Functions to monitor screen edges for recalling a hidden dock.
 * 
 * Copyright 2021-2024 Daniel Kondor <kondor.dani@gmail.com>
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
#include "cairo-dock-wayland-hotspots.h"
#include "cairo-dock-wayland-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-log.h"
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>

typedef struct _output_hotspots_base {
	gboolean need_hotspot[4]; // indicator whether we need a hotspot
	// order corresponds to values in the CairoDockPositionType enum
} output_hotspots_base;


/// interface of individual hotspot managers
typedef struct _hotspots_interface {
	output_hotspots_base* (*new_output) (GdkMonitor *monitor);
	void (*output_removed) (output_hotspots_base *output);
	void (*update_dock_hotspots) (CairoDock *pDock, gpointer user_data);
	void (*destroy_hotspot) (output_hotspots_base *base, int j);
} hotspots_interface;

static hotspots_interface s_backend = {0};


/// Private variables, wf-shell related

/// Number of outputs and mapping from GdkMonitors to wf_outputs
static int s_iNumOutputs = 0;
static GdkMonitor** s_pMonitors = NULL;
static output_hotspots_base** outputs = NULL;
static int s_iOutputsSize = 0; // size of the above arrays

static struct zwf_shell_manager_v2 *s_pshell_manager = NULL;
static uint32_t wf_shell_id, wf_shell_version;
static gboolean wf_shell_found = FALSE;

typedef struct _wf_hotspots {
	struct _output_hotspots_base base;
	struct zwf_output_v2 *output;
	struct zwf_hotspot_v2 *hotspots[4]; // each output has four possible hotspots
} wf_hotspots;


static void _dummy (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct zwf_output_v2 *zwf_output_v2)
{
	
}

static struct zwf_output_v2_listener output_listener = { _dummy, _dummy };

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


static gboolean _monitor_added (G_GNUC_UNUSED gpointer user_data, GdkMonitor *monitor)
{
	int i = s_iNumOutputs;
	s_iNumOutputs++;
	if (s_iNumOutputs > s_iOutputsSize)
	{
		s_pMonitors = g_renew(GdkMonitor*, s_pMonitors, s_iNumOutputs);
		outputs = g_renew(output_hotspots_base*, outputs, s_iNumOutputs);
		s_iOutputsSize = s_iNumOutputs;
	}	
	
	s_pMonitors[i] = monitor;
	outputs[i] = s_backend.new_output (monitor);
	if (!outputs[i])
	{
		s_pMonitors[i] = NULL;
		s_iNumOutputs--;
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _monitor_removed (G_GNUC_UNUSED gpointer user_data, GdkMonitor *monitor)
{
	int i;
	for (i = 0; i < s_iNumOutputs; i++) if(s_pMonitors[i] == monitor) break;
	if (i == s_iNumOutputs) {
		/// TODO: error handling
		return GLDI_NOTIFICATION_LET_PASS;
	}
	
	s_backend.output_removed (outputs[i]);
	
	s_iNumOutputs--;
	if (i < s_iNumOutputs)
	{
		s_pMonitors[i] = s_pMonitors[s_iNumOutputs];
		outputs[i] = outputs[s_iNumOutputs];
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
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
	output_hotspots_base *output = (output_hotspots_base*)data;
	
	hotspot_hit hit;
	hit.iMonitor = 0;
	hit.pos = 0;
	
	for (; hit.iMonitor < s_iNumOutputs; hit.iMonitor++) if (outputs[hit.iMonitor] == output) break;
	if (hit.iMonitor == s_iNumOutputs) return; // TODO: error message
	
	wf_hotspots *output1 = (wf_hotspots*)outputs[hit.iMonitor];
	for (; hit.pos < 4; hit.pos++) if (output1->hotspots[hit.pos] == hotspot) break;
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
static void _wf_shell_update_dock_hotspots (CairoDock *pDock, G_GNUC_UNUSED gpointer user_data)
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
	
	wf_hotspots *output = (wf_hotspots*)outputs[i];
	if (!output->hotspots[pos])
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
		
		output->hotspots[pos] = zwf_output_v2_create_hotspot (output->output,
			pos2, thres, myDocksParam.iUnhideDockDelay);
		if (!output->hotspots[pos]) return;
		
		// fprintf (stderr, "Created hotspot %p for output %p, position %d\n", outputs[i]->hotspots[pos], outputs[i]->output, pos);
		zwf_hotspot_v2_add_listener (output->hotspots[pos], &hotspot_listener, output);
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

/// Update the hotspots we are listening to (based on the configuration of all root docks)
void gldi_wayland_hotspots_update (void)
{
	int i, j;
	/// reset flag to false for all hotspots
	for (i = 0; i < s_iNumOutputs; i++) for (j = 0; j < 4; j++) outputs[i]->need_hotspot[j] = FALSE;
	
	gldi_docks_foreach_root ((GFunc) s_backend.update_dock_hotspots, NULL);
	
	for (i = 0; i < s_iNumOutputs; i++) for (j = 0; j < 4; j++)
		if(!outputs[i]->need_hotspot[j])
		{
			// fprintf (stderr, "Destroying hotspot %p for output %p, position %d\n", outputs[i]->hotspots[j], outputs[i]->output, j);
			s_backend.destroy_hotspot (outputs[i], j);
		}
}

/// Stop listening to all hotspots
void gldi_wayland_hotspots_stop (void)
{
	int i, j;
	for (i = 0; i < s_iNumOutputs; i++) for (j = 0; j < 4; j++)
		s_backend.destroy_hotspot (outputs[i], j);
}


/// Try to match Wayland protocols that we need to use
gboolean gldi_wayland_hotspots_match_protocol (uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp (interface, zwf_shell_manager_v2_interface.name))
	{
		wf_shell_id = id;
		wf_shell_version = version;
		wf_shell_found = TRUE;
		return TRUE;
	}
	return FALSE;
}

/// Try to init based on the protocols found
gboolean gldi_wayland_hotspots_try_init (struct wl_registry *registry)
{
	gboolean can_init = FALSE;
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
			s_backend.update_dock_hotspots = _wf_shell_update_dock_hotspots;
			s_backend.destroy_hotspot = _wf_shell_destroy_hotspot;
			can_init = TRUE;
		}
		else cd_message ("Could not bind zwf-shell!");
    }
    
    if (can_init) _init();
    return can_init;
}
