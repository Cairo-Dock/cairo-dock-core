/*
 * cairo-dock-cosmic-workspaces.c -- desktop / workspace management
 *  facilities for Cosmic and compatible
 * 
 * Copyright 2024 Daniel Kondor <kondor.dani@gmail.com>
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

#include "cairo-dock-log.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-cosmic-workspaces.h"

struct wl_output *s_ws_output = NULL;

/*
 * notes:
 *   Labwc: does not send coordinates (but seems to send the workspaces in the correct order),
 *      non-active workspaces have the "hidden" flag, workspaces can be activated, but no other
 *      action is supported; multiple outputs share workspaces, always only one workspace group
 *   Cosmic: coordinates: x starts from 1, y all zero (default config / layout), workspaces
 *      can be activated, no other action
 */


typedef struct _CosmicWS {
	struct zcosmic_workspace_handle_v1 *handle; // protocol object representing this desktop
	char *name; // can be NULL if no name was delivered
	guint x, y; // x and y coordinates; (guint)-1 means invalid
	guint pending_x, pending_y;
	char *pending_name;
	gboolean bRemoved; // set to TRUE when receiving the removed event
} CosmicWS;

#define INVALID_COORD (guint)-1

/// private variables -- track the current state of workspaces
static unsigned int s_iNumDesktops = 0;
static CosmicWS **desktops = NULL; // allocated when the first workspace is added
static unsigned int s_iDesktopCap = 0; // capacity of the above array
static unsigned int s_iCurrent = 0; // index into desktops array with the currently active desktop
static unsigned int s_iPending = 0; // workspace with pending activation

static gboolean s_bPendingAdded = FALSE; // workspace was added or removed, need to recalculate layout

static struct zcosmic_workspace_manager_v1 *s_pWSManager = NULL;
static struct zcosmic_workspace_group_handle_v1 *s_pWSGroup = NULL; // we support having only one workspace group for now

static gboolean bValidX = FALSE; // if x coordinates are valid
static gboolean bValidY = FALSE; // if y coordinates are valid
static guint s_iXOffset = 0; // offset to add to x coordinates (if they don't start from 0)
static guint s_iYOffset = 0; // offset to add to x coordinates (if they don't start from 0)

// we don't want arbitrarily large coordinates
#define MAX_DESKTOP_DIM 16
#define MAX_DESKTOP_NUM 128

/* to be called after all workspaces have been delivered, from the workspace manager's done event */
static void _update_desktop_layout ()
{
	guint rows = 0, cols = 0;
	g_desktopGeometry.iNbDesktops = 1;
	
	unsigned int i;
	gboolean have_invalid_x = FALSE;
	gboolean have_invalid_y = FALSE;
	gboolean all_invalid_y = TRUE;
	s_iXOffset = G_MAXUINT;
	s_iYOffset = G_MAXUINT;
	for (i = 0; i < s_iNumDesktops; i++)
	{
		guint x = desktops[i]->x;
		guint y = desktops[i]->y;
		
		if (x == (guint)-1) have_invalid_x = TRUE;
		else 
		{
			if (x > cols) cols = x;
			if (x < s_iXOffset) s_iXOffset = x;
		}
		
		if (y == (guint)-1) have_invalid_y = TRUE;
		else {
			all_invalid_y = FALSE;
			if (y > rows) rows = y;
			if (y < s_iYOffset) s_iYOffset = y;
		}
	}
	
	if (s_iNumDesktops > 0 && !have_invalid_x)
	{
		if (have_invalid_y && all_invalid_y && cols < MAX_DESKTOP_NUM && cols/8 <= s_iNumDesktops)
		{
			bValidX = TRUE;
			bValidY = FALSE;
			g_desktopGeometry.iNbViewportX = cols + 1 - s_iXOffset;
			g_desktopGeometry.iNbViewportY = 1;
			return;
		}
		if (!have_invalid_y && cols < MAX_DESKTOP_DIM && rows < MAX_DESKTOP_DIM && rows*cols < MAX_DESKTOP_NUM && (rows*cols) / 8 <= s_iNumDesktops)
		{
			bValidX = TRUE;
			bValidY = TRUE;
			g_desktopGeometry.iNbViewportX = cols + 1 - s_iXOffset;
			g_desktopGeometry.iNbViewportY = rows + 1 - s_iYOffset;
			return;
		}
	}
	
	bValidX = FALSE;
	bValidY = FALSE;
	g_desktopGeometry.iNbViewportX = s_iNumDesktops ? s_iNumDesktops : 1;
	g_desktopGeometry.iNbViewportY = 1;
}

static void _update_current_desktop (void)
{
	g_desktopGeometry.iCurrentDesktop = 0;
	if (bValidX && s_iCurrent < s_iNumDesktops)
	{
		CosmicWS *desktop = desktops[s_iCurrent];
		g_desktopGeometry.iCurrentViewportX = desktop->x - s_iXOffset;
		if (bValidY) g_desktopGeometry.iCurrentViewportY = desktop->y - s_iYOffset;
		else g_desktopGeometry.iCurrentViewportY = 0;
	}
	else
	{			
		g_desktopGeometry.iCurrentViewportX = s_iCurrent;
		g_desktopGeometry.iCurrentViewportY = 0;
	}
}

static void _name (void *data, struct zcosmic_workspace_handle_v1* handle, const char *name)
{
	cd_warning ("workspace name: %p, %s", handle, name ? name : "(null)");
	CosmicWS *desktop = (CosmicWS*)data;
	g_free (desktop->pending_name);
	desktop->pending_name = g_strdup ((gchar *)name);
}

static void _coordinates (void *data, struct zcosmic_workspace_handle_v1* handle, struct wl_array *coords)
{
	CosmicWS *desktop = (CosmicWS*)data;
	uint32_t *cdata = (uint32_t*)coords->data;
	size_t size = coords->size;
	if (size > 2*sizeof(uint32_t) || size < sizeof(uint32_t))
	{
		// too many or no coordinates, we cannot use them
		desktop->pending_x = (guint)-1;
		desktop->pending_y = (guint)-1;
	}
	else
	{
		// we have at least one coordinate
		desktop->pending_x = cdata[0];
		if (size == 2*sizeof(uint32_t)) desktop->pending_y = cdata[1];
		else desktop->pending_y = (guint)-1;
	}
	cd_warning ("workspace coordinates: %p, size: %lu, x: %u, y: %u", handle, size, desktop->pending_x, desktop->pending_y);
}

static void _state (void *data, struct zcosmic_workspace_handle_v1* handle, struct wl_array *state)
{
	gboolean bActivated = FALSE;
	gboolean bUrgent = FALSE;
	gboolean bHidden = FALSE;
	int i;
	uint32_t* stdata = (uint32_t*)state->data;
	for (i = 0; i*sizeof(uint32_t) < state->size; i++)
	{
		if (stdata[i] == ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_ACTIVE)
			bActivated = TRUE;
		else if (stdata[i] == ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_URGENT)
			bUrgent = TRUE;
		else if (stdata[i] == ZCOSMIC_WORKSPACE_HANDLE_V1_STATE_HIDDEN)
			bHidden = TRUE;
	}
	cd_warning ("workspace state: %p, activated: %d, urgent: %d, hidden: %d", handle, !!bActivated, !!bUrgent, !!bHidden);
	
	if (bActivated)
	{
		CosmicWS *desktop = (CosmicWS*)data;
		unsigned int j;
		for (j = 0; j < s_iNumDesktops; j++)
			if (desktops[j] == desktop)
			{
				s_iPending = j;
				return;
			}
		cd_critical ("cosmic-workspaces: could not find currently activated desktop!");
	}
}

static void _capabilities (void*, struct zcosmic_workspace_handle_v1* handle, struct wl_array* cap)
{
	uint32_t* capdata = (uint32_t*)cap->data;
	cd_warning ("workspace capabilities: %p", handle);
	int i;
	for (i = 0; i*sizeof(uint32_t) < cap->size; i++)
	{
		if (capdata[i] == ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_ACTIVATE)
			g_print ("workspace can be activated\n");
		else if (capdata[i] == ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_DEACTIVATE)
			g_print ("workspace can be deactivated\n");
		else if (capdata[i] == ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_REMOVE)
			g_print ("workspace can be removed\n");
		else if (capdata[i] == ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_RENAME)
			g_print ("workspace can be renamed\n");
		else if (capdata[i] == ZCOSMIC_WORKSPACE_HANDLE_V1_ZCOSMIC_WORKSPACE_CAPABILITIES_V1_SET_TILING_STATE)
			g_print ("workspace can set tiling state\n");
	}
}

static void _removed (void *data, struct zcosmic_workspace_handle_v1 *handle)
{
	cd_warning ("workspace removed: %p", handle);
	CosmicWS *desktop = (CosmicWS*)data;
	desktop->bRemoved = TRUE;
}


static void _free_workspace (CosmicWS *desktop)
{
	g_free (desktop->name);
	g_free (desktop->pending_name);
	zcosmic_workspace_handle_v1_destroy (desktop->handle);
	g_free (desktop);
}


static void _tiling_state (void*, struct zcosmic_workspace_handle_v1*, uint32_t)
{
	/* don't care */
}

static const struct zcosmic_workspace_handle_v1_listener desktop_listener = {
	.name = _name,
	.coordinates = _coordinates,
	.state = _state,
	.capabilities = _capabilities,
	.remove = _removed,
	.tiling_state = _tiling_state
};


static void _group_capabilities (void*, struct zcosmic_workspace_group_handle_v1* handle, struct wl_array* cap)
{
	/* don't care */
	uint32_t* capdata = (uint32_t*)cap->data;
	int i;
	for (i = 0; i*sizeof(uint32_t) < cap->size; i++)
	{
		if (capdata[i] == ZCOSMIC_WORKSPACE_GROUP_HANDLE_V1_CREATE_WORKSPACE)
			cd_warning ("workspace group can be add workspaces: %p\n", handle);
	}
}

static void _output_enter (void*, struct zcosmic_workspace_group_handle_v1* handle, struct wl_output* output)
{
	if (handle == s_pWSGroup) s_ws_output = output;
}

static void _output_leave (void*, struct zcosmic_workspace_group_handle_v1* handle, struct wl_output* output)
{
	if (handle == s_pWSGroup && output == s_ws_output) s_ws_output = NULL;
}

static void _desktop_created (void*, struct zcosmic_workspace_group_handle_v1 *manager,
	struct zcosmic_workspace_handle_v1 *new_workspace)
{
	cd_warning ("new workspace: %p, %p", manager, new_workspace);
	if (manager != s_pWSGroup)
	{
		// this is not "our" manager, we don't care
		zcosmic_workspace_handle_v1_destroy (new_workspace);
		return;
	}
	
	CosmicWS *desktop = g_new0 (CosmicWS, 1);
	desktop->x = INVALID_COORD;
	desktop->y = INVALID_COORD;
	desktop->pending_x = INVALID_COORD;
	desktop->pending_y = INVALID_COORD;
	if (s_iNumDesktops >= s_iDesktopCap)
	{
		desktops = g_renew (CosmicWS*, desktops, s_iNumDesktops + 16);
		s_iDesktopCap = s_iNumDesktops + 1;
	}
	desktops[s_iNumDesktops] = desktop;
	s_iNumDesktops++;
	
	desktop->handle = new_workspace;
	zcosmic_workspace_handle_v1_add_listener (new_workspace, &desktop_listener, desktop);
	s_bPendingAdded = TRUE;
}

static void _group_removed (void*, struct zcosmic_workspace_group_handle_v1 *handle)
{
	cd_warning ("workspace group removed: %p", handle);
	if (handle == s_pWSGroup)
	{
		
		if (s_iNumDesktops)
		{
			// the compositor should have deleted all desktop handles before
			cd_critical ("cosmic-workspaces: non-empty workspace group removed!");
			do {
				--s_iNumDesktops;
				_free_workspace (desktops[s_iNumDesktops]);
			} while(s_iNumDesktops);
		}
		
		g_free (desktops);
		desktops = NULL;
		s_iDesktopCap = 0;
		_update_desktop_layout ();
		gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
		s_pWSGroup = NULL;
	}
	zcosmic_workspace_group_handle_v1_destroy (handle);
}


static const struct zcosmic_workspace_group_handle_v1_listener group_listener = {
	.capabilities = _group_capabilities,
	.output_enter = _output_enter,
	.output_leave = _output_leave,
	.workspace = _desktop_created,
	.remove = _group_removed
};


static void _new_workspace_group (void*, struct zcosmic_workspace_manager_v1*, struct zcosmic_workspace_group_handle_v1 *new_group)
{
	cd_warning ("new workspace group: %p", new_group);
	if (s_pWSGroup)
	{
		cd_warning ("cosmic-workspaces: multiple workspace groups are not supported!\n");
		zcosmic_workspace_group_handle_v1_add_listener (new_group, &group_listener, NULL);
		zcosmic_workspace_group_handle_v1_destroy (new_group);
	}
	else
	{
		s_pWSGroup = new_group;
		zcosmic_workspace_group_handle_v1_add_listener (new_group, &group_listener, NULL);
	}
}

static void _done (void*, struct zcosmic_workspace_manager_v1*)
{
	cd_warning ("workspace manager done");
	gboolean bRemoved = FALSE; // if any workspace was removed
	gboolean bCoords = FALSE; // any of the coordinates changed
	gboolean bName = FALSE; // any of the names changed
	// check all workspaces
	unsigned int i, j = 0;
	for (i = 0; i < s_iNumDesktops; i++)
	{
		if (desktops[i]->bRemoved)
		{
			_free_workspace (desktops[i]);
			desktops[i] = NULL;
			bRemoved = TRUE;
		}
		else {
			if (desktops[i]->pending_x != desktops[i]->x)
			{
				desktops[i]->x = desktops[i]->pending_x;
				bCoords = TRUE;
			}
			if (desktops[i]->pending_y != desktops[i]->y)
			{
				desktops[i]->y = desktops[i]->pending_y;
				bCoords = TRUE;
			}
			if (desktops[i]->pending_name)
			{
				g_free (desktops[i]->name);
				desktops[i]->name = desktops[i]->pending_name;
				desktops[i]->pending_name = NULL;
				bName = TRUE;
			}
			
			if (i != j) desktops[j] = desktops[i];
			j++;
		}
	}
	
	s_iNumDesktops = j; // remaining number of desktops
	
	if (bRemoved || bCoords || s_bPendingAdded)
	{
		s_iCurrent = s_iPending;
		if (s_iCurrent >= s_iNumDesktops) s_iCurrent = s_iNumDesktops ? (s_iNumDesktops - 1) : 0;
		_update_desktop_layout ();
		_update_current_desktop ();
		gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
		gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	}
	else if (s_iCurrent != s_iPending)
	{
		s_iCurrent = s_iPending;
		_update_current_desktop ();
		gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	}
	
	if (bName) gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_NAMES_CHANGED);
	s_bPendingAdded = FALSE;
}

static void _finished (void*, struct zcosmic_workspace_manager_v1 *handle)
{
	cd_warning ("workspace manager finished");
	zcosmic_workspace_manager_v1_destroy (handle);
}

static const struct zcosmic_workspace_manager_v1_listener manager_listener = {
	.workspace_group = _new_workspace_group,
	.done = _done,
	.finished = _finished
};


static gchar** _get_desktops_names (void)
{
	gchar **ret = g_new0 (gchar*, s_iNumDesktops + 1); // + 1, so that it is a null-terminated list, as expected by the switcher plugin
	unsigned int i;
	for (i = 0; i < s_iNumDesktops; i++) ret[i] = g_strdup (desktops[i]->name);
	return ret;
}

static unsigned int _get_ix (guint x, guint y)
{
	unsigned int i = 0;
	if (bValidX)
	{
		for (; i < s_iNumDesktops; i++)
		{
			if (!desktops[i]->bRemoved && (desktops[i]->x == x + s_iXOffset)
				&& (!bValidY || desktops[i]->y == y + s_iYOffset))
					break;
		}
	}
	else i = x;
	return i;
}

static gboolean _set_current_desktop (G_GNUC_UNUSED int iDesktopNumber, int iViewportNumberX, int iViewportNumberY)
{
	// desktop number is ignored (it should be 0)
	if (iViewportNumberX >= 0 && iViewportNumberY >= 0)
	{
		unsigned int iReq = _get_ix ((guint)iViewportNumberX, (guint)iViewportNumberY);
		if (iReq < s_iNumDesktops)
		{
			zcosmic_workspace_handle_v1_activate (desktops[iReq]->handle);
			zcosmic_workspace_manager_v1_commit (s_pWSManager);
			return TRUE; // we don't know if we succeeded
		}
	}
	cd_warning ("cosmic-workspaces: invalid workspace requested!\n");
	return FALSE;
}

/* currently not supported on either Cosmic or Labwc, easier to disable 
static void _add_workspace (void)
{
	if (s_pWSGroup && s_pWSManager)
	{
		char *name = g_strdup_printf ("Workspace %u", s_iNumDesktops + 1);
		zcosmic_workspace_group_handle_v1_create_workspace (s_pWSGroup, name);
		zcosmic_workspace_manager_v1_commit (s_pWSManager);
		g_free (name);
	}
}

static void _remove_workspace (void)
{
	if (s_iNumDesktops <= 1 || !s_pWSManager) return;
	zcosmic_workspace_handle_v1_remove (desktops[s_iNumDesktops - 1]->handle);
	zcosmic_workspace_manager_v1_commit (s_pWSManager);
}
*/

static uint32_t protocol_id, protocol_version;
static gboolean protocol_found = FALSE;

gboolean gldi_cosmic_workspaces_match_protocol (uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp(interface, zcosmic_workspace_manager_v1_interface.name))
	{
		protocol_found = TRUE;
		protocol_id = id;
		protocol_version = version;
		if ((uint32_t)zcosmic_workspace_manager_v1_interface.version < protocol_version)
			protocol_version = zcosmic_workspace_manager_v1_interface.version;
		return TRUE;
	}
	return FALSE;
}

gboolean gldi_cosmic_workspaces_try_init (struct wl_registry *registry)
{
	if (!protocol_found) return FALSE;
	s_pWSManager = wl_registry_bind (registry, protocol_id, &zcosmic_workspace_manager_v1_interface, protocol_version);
	if (!s_pWSManager) return FALSE;
	
	GldiDesktopManagerBackend dmb;
	memset (&dmb, 0, sizeof (GldiDesktopManagerBackend));
	dmb.set_current_desktop   = _set_current_desktop;
	dmb.get_desktops_names    = _get_desktops_names;
//	dmb.add_workspace         = _add_workspace;
//	dmb.remove_last_workspace = _remove_workspace;
	gldi_desktop_manager_register_backend (&dmb, "cosmic-workspaces");
	
	zcosmic_workspace_manager_v1_add_listener (s_pWSManager, &manager_listener, NULL);
	return TRUE;
}

struct zcosmic_workspace_handle_v1 *gldi_cosmic_workspaces_get_handle (int x, int y)
{
	unsigned int iReq = _get_ix ((guint)x, (guint)y);
	if (iReq < s_iNumDesktops)
		return desktops[iReq]->handle;
	cd_warning ("cosmic-workspaces: invalid workspace requested!\n");
	return NULL;
}

void gldi_cosmic_workspaces_update_window (GldiWindowActor *actor, struct zcosmic_workspace_handle_v1 *handle)
{
	unsigned int i;
	for (i = 0; i < s_iNumDesktops; i++)
	{
		if (desktops[i]->handle == handle)
		{
			actor->iNumDesktop = 0;
			if (bValidX)
			{
				actor->iViewPortX = desktops[i]->x - s_iXOffset;
				if (bValidY) actor->iViewPortY = desktops[i]->y - s_iYOffset;
				else actor->iViewPortY = 0;
			}
			else
			{
				actor->iViewPortX = i;
				actor->iViewPortY = 0;
			}
			gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, actor);
			return;
		}
	}
	cd_warning ("cosmic-workspaces: workspace not found!\n");
}


