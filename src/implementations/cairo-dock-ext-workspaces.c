/*
 * cairo-dock-ext-workspaces.c -- desktop / workspace management
 *  facilities based on the ext-workspace Wayland protocol
 * 
 * Copyright 2024-2025 Daniel Kondor <kondor.dani@gmail.com>
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
#include "cairo-dock-ext-workspaces.h"
#include "cairo-dock-wayland-wm.h"

struct wl_output *s_ws_output = NULL;

/*
 * notes:
 *   Labwc: does not send coordinates (but seems to send the workspaces in the correct order),
 *      non-active workspaces have the "hidden" flag, workspaces can be activated, but no other
 *      action is supported; multiple outputs share workspaces, always only one workspace group
 *   Cosmic: coordinates: x starts from 1, y all zero (default config / layout; however, workspaces
 * 		are arranged vertically), workspaces can be activated, no other action
 */


typedef struct _CosmicWS {
	struct ext_workspace_handle_v1 *handle; // protocol object representing this desktop
	char *name; // can be NULL if no name was delivered
	guint x, y; // x and y coordinates; (guint)-1 means invalid
	guint pending_x, pending_y;
	char *pending_name;
	struct ext_workspace_group_handle_v1 *group_pending; // ws group (pending until next done event)
	gboolean bRemoved; // set to TRUE when receiving the removed event
	gboolean bHiddenPending; // set to TRUE when received a hidden event
} CosmicWS;

#define INVALID_COORD (guint)-1

/// private variables -- track the current state of workspaces
static GPtrArray *s_aDesktops = NULL; // list of valid workspaces -- allocated to empty array when initializing (so it is always safe to access members)
static GPtrArray *s_aInvalid = NULL; // list of invalid workspaces that we know of -- these either don't belong to our WS group or are marked as hidden
static CosmicWS *s_pCurrent = NULL; // index into desktops array with the currently active desktop
static CosmicWS *s_pPending = NULL; // workspace with pending activation

static struct ext_workspace_manager_v1 *s_pWSManager = NULL;
static struct ext_workspace_group_handle_v1 *s_pWSGroup = NULL; // we support having only one workspace group for now

static gboolean bValidX = FALSE; // if x coordinates are valid
static gboolean bValidY = FALSE; // if y coordinates are valid
static guint s_iXOffset = 0; // offset to add to x coordinates (if they don't start from 0)
static guint s_iYOffset = 0; // offset to add to x coordinates (if they don't start from 0)

static guint s_sidNotify = 0;
static guint s_sidNotifyName = 0;
static gboolean s_bLayoutChanged = FALSE;

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
	
	CosmicWS **desktops = (CosmicWS**)s_aDesktops->pdata;
	unsigned int s_iNumDesktops = s_aDesktops->len;
	
	for (i = 0; i < s_iNumDesktops; i++)
	{
		guint x = desktops[i]->x;
		guint y = desktops[i]->y;
		
		if (x == INVALID_COORD) have_invalid_x = TRUE;
		else 
		{
			if (x > cols) cols = x;
			if (x < s_iXOffset) s_iXOffset = x;
		}
		
		if (y == INVALID_COORD) have_invalid_y = TRUE;
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
	// check if the current desktop is really valid
	unsigned int ix;
	if (!s_pCurrent || !g_ptr_array_find (s_aDesktops, s_pCurrent, &ix))
	{
		ix = 0;
		if (ix < s_aDesktops->len) s_pCurrent = s_aDesktops->pdata[ix];
		else s_pCurrent = NULL;
	}
	
	if (bValidX && s_pCurrent)
	{
		CosmicWS *desktop = s_pCurrent;
		g_desktopGeometry.iCurrentViewportX = desktop->x - s_iXOffset;
		if (bValidY) g_desktopGeometry.iCurrentViewportY = desktop->y - s_iYOffset;
		else g_desktopGeometry.iCurrentViewportY = 0;
	}
	else
	{			
		g_desktopGeometry.iCurrentViewportX = ix;
		g_desktopGeometry.iCurrentViewportY = 0;
	}
}

static gboolean _notify (void*)
{
	if (s_bLayoutChanged) _update_desktop_layout ();
	_update_current_desktop ();
	if (s_bLayoutChanged) gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	
	s_bLayoutChanged = FALSE;
	s_sidNotify = 0;
	return FALSE;
}

static gboolean _notify_name (void*)
{
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_NAMES_CHANGED);
	
	s_sidNotifyName = 0;
	return FALSE;
}

static void _wm_notify (void)
{
	if (s_sidNotify) 
	{
		g_source_remove (s_sidNotify);
		_notify (NULL);
	}
}

/**
 * Workspace events
 */
static void _id (void*, struct ext_workspace_handle_v1*, const char*)
{
	/* ID could be used to identify workspaces across sessions. For now, we don't care. */
}

static void _name (void *data, struct ext_workspace_handle_v1*, const char *name)
{
	CosmicWS *desktop = (CosmicWS*)data;
	g_free (desktop->pending_name);
	desktop->pending_name = g_strdup ((gchar *)name);
}

static void _coordinates (void *data, struct ext_workspace_handle_v1*, struct wl_array *coords)
{
	CosmicWS *desktop = (CosmicWS*)data;
	uint32_t *cdata = (uint32_t*)coords->data;
	size_t size = coords->size;
	if (size > 2*sizeof(uint32_t) || size < sizeof(uint32_t))
	{
		// too many or no coordinates, we cannot use them
		desktop->pending_x = INVALID_COORD;
		desktop->pending_y = INVALID_COORD;
	}
	else
	{
		// we have at least one coordinate
		desktop->pending_x = cdata[0];
		if (size == 2*sizeof(uint32_t)) desktop->pending_y = cdata[1];
		else desktop->pending_y = INVALID_COORD;
	}
}

static void _state (void *data, struct ext_workspace_handle_v1*, uint32_t state)
{
	gboolean bActivated = (state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE);
	gboolean bHidden = (state & EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN);
	
	CosmicWS *desktop = (CosmicWS*)data;
	desktop->bHiddenPending = bHidden;
	if (bActivated) s_pPending = desktop;
}

static void _capabilities (void*, G_GNUC_UNUSED struct ext_workspace_handle_v1* handle, G_GNUC_UNUSED uint32_t cap)
{
	/* don't care for now */
}

static void _removed (void *data, struct ext_workspace_handle_v1*)
{
	CosmicWS *desktop = (CosmicWS*)data;
	desktop->bRemoved = TRUE;
	if (desktop == s_pPending) s_pPending = NULL;
}


static void _free_workspace (CosmicWS *desktop)
{
	g_free (desktop->name);
	g_free (desktop->pending_name);
	ext_workspace_handle_v1_destroy (desktop->handle);
	g_free (desktop);
}

static void _free_workspace2 (void *ptr) { _free_workspace ((CosmicWS*)ptr); }


static const struct ext_workspace_handle_v1_listener desktop_listener = {
	.id = _id,
	.name = _name,
	.coordinates = _coordinates,
	.state = _state,
	.capabilities = _capabilities,
	.removed = _removed,
};


static void _group_capabilities (void*, G_GNUC_UNUSED struct ext_workspace_group_handle_v1* handle, G_GNUC_UNUSED uint32_t cap)
{
	/* don't care */
}

static void _output_enter (void*, struct ext_workspace_group_handle_v1* handle, struct wl_output* output)
{
	if (handle == s_pWSGroup) s_ws_output = output;
}

static void _output_leave (void*, struct ext_workspace_group_handle_v1* handle, struct wl_output* output)
{
	if (handle == s_pWSGroup && output == s_ws_output) s_ws_output = NULL;
}

static gboolean _ws_handle_eq (const void *x, const void *y)
{
	const CosmicWS *desktop = (const CosmicWS*)x;
	const struct ext_workspace_handle_v1 *handle = (const struct ext_workspace_handle_v1*)y;
	return (desktop->handle == handle);
}

static void _workspace_group_enter (void*, struct ext_workspace_group_handle_v1* handle, struct ext_workspace_handle_v1 *ws)
{
	unsigned int ix;
	if (g_ptr_array_find_with_equal_func (s_aDesktops, ws, _ws_handle_eq, &ix))
	{
		CosmicWS *desktop = s_aDesktops->pdata[ix];
		desktop->group_pending = handle;
	}
	else if (g_ptr_array_find_with_equal_func (s_aInvalid, ws, _ws_handle_eq, &ix))
	{
		CosmicWS *desktop = s_aInvalid->pdata[ix];
		desktop->group_pending = handle;
	}
}

static void _workspace_group_leave (void*, struct ext_workspace_group_handle_v1*, struct ext_workspace_handle_v1 *ws)
{
	_workspace_group_enter (NULL, NULL, ws);
}

static void _group_removed (void*, struct ext_workspace_group_handle_v1 *handle)
{
	if (handle == s_pWSGroup)
	{
		
		if (s_aDesktops->len)
		{
			// the compositor should have deleted all desktop handles before
			cd_critical ("cosmic-workspaces: non-empty workspace group removed!");
			g_ptr_array_set_size (s_aDesktops, 0); // will call _free_workspace () for each element
		}
		
		s_bLayoutChanged = TRUE;
		s_pWSGroup = NULL;
		if (!s_sidNotify) s_sidNotify = g_idle_add (_notify, NULL);
	}
	ext_workspace_group_handle_v1_destroy (handle);
}


static const struct ext_workspace_group_handle_v1_listener group_listener = {
	.capabilities = _group_capabilities,
	.output_enter = _output_enter,
	.output_leave = _output_leave,
	.workspace_enter = _workspace_group_enter,
	.workspace_leave = _workspace_group_leave,
	.removed = _group_removed
};


static void _new_workspace_group (void*, struct ext_workspace_manager_v1*, struct ext_workspace_group_handle_v1 *new_group)
{
	if (s_pWSGroup)
	{
		cd_warning ("ext-workspaces: multiple workspace groups are not supported!\n");
		ext_workspace_group_handle_v1_destroy (new_group);
	}
	else
	{
		s_pWSGroup = new_group;
		ext_workspace_group_handle_v1_add_listener (new_group, &group_listener, NULL);
	}
}

static void _desktop_created (void*, struct ext_workspace_manager_v1*,
	struct ext_workspace_handle_v1 *new_workspace)
{
	CosmicWS *desktop = g_new0 (CosmicWS, 1);
	desktop->x = INVALID_COORD;
	desktop->y = INVALID_COORD;
	desktop->pending_x = INVALID_COORD;
	desktop->pending_y = INVALID_COORD;
	desktop->handle = new_workspace;
	g_ptr_array_add (s_aInvalid, desktop);
	ext_workspace_handle_v1_add_listener (new_workspace, &desktop_listener, desktop);
}

static void _done (void*, struct ext_workspace_manager_v1*)
{
	GPtrArray *to_invalid = NULL;
	
	gboolean bRemoved = FALSE; // if any workspace was removed
	gboolean bAdded = FALSE; // if any workspace needs to be added
	gboolean bCoords = FALSE; // any of the coordinates changed
	gboolean bName = FALSE; // any of the names changed
	
	// check all valid workspaces
	CosmicWS **desktops = (CosmicWS**)s_aDesktops->pdata;
	unsigned int s_iNumDesktops = s_aDesktops->len;
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
			gboolean bInvalid = FALSE;
			if (desktops[i]->bHiddenPending) bInvalid = TRUE;
			if (desktops[i]->group_pending != s_pWSGroup) bInvalid = TRUE;
			
			if (desktops[i]->pending_x != desktops[i]->x)
			{
				desktops[i]->x = desktops[i]->pending_x;
				if (!bInvalid) bCoords = TRUE;
			}
			if (desktops[i]->pending_y != desktops[i]->y)
			{
				desktops[i]->y = desktops[i]->pending_y;
				if (!bInvalid) bCoords = TRUE;
			}
			if (desktops[i]->pending_name)
			{
				g_free (desktops[i]->name);
				desktops[i]->name = desktops[i]->pending_name;
				desktops[i]->pending_name = NULL;
				if (!bInvalid) bName = TRUE;
			}
			
			if (bInvalid)
			{
				if (!to_invalid) to_invalid = g_ptr_array_new ();
				g_ptr_array_add (to_invalid, desktops[i]);
				desktops[i] = NULL;
				bRemoved = TRUE;
			}
			else
			{
				if (i != j) desktops[j] = desktops[i];
				j++;
			}
		}
	}
	s_aDesktops->len = j;
	// set remaining elements to NULL (just in case)
	for (; j < s_iNumDesktops; j++) desktops[j] = NULL;
	
	// check all invalid workspaces if any needs to be added
	j = 0;
	desktops = (CosmicWS**)s_aInvalid->pdata;
	for (i = 0; i < s_aInvalid->len; i++)
	{
		if (desktops[i]->bRemoved)
		{
			_free_workspace (desktops[i]);
			desktops[i] = NULL;
		}
		else
		{
			gboolean bInvalid = FALSE;
			if (desktops[i]->bHiddenPending) bInvalid = TRUE;
			if (desktops[i]->group_pending != s_pWSGroup) bInvalid = TRUE;
			
			if (desktops[i]->pending_x != desktops[i]->x)
				desktops[i]->x = desktops[i]->pending_x;
			if (desktops[i]->pending_y != desktops[i]->y)
				desktops[i]->y = desktops[i]->pending_y;
			if (desktops[i]->pending_name)
			{
				g_free (desktops[i]->name);
				desktops[i]->name = desktops[i]->pending_name;
				desktops[i]->pending_name = NULL;
				if (!bInvalid) bName = TRUE;
			}
			
			if (bInvalid)
			{
				// keep here
				if (i != j) desktops[j] = desktops[i];
				j++;
			}
			else
			{
				// move to the list of valid desktops
				g_ptr_array_add (s_aDesktops, desktops[i]);
				desktops[i] = NULL;
				bAdded = TRUE;
			}
		}
	}
	s_iNumDesktops = s_aInvalid->len;
	s_aInvalid->len = j;
	for (; j < s_iNumDesktops; j++) desktops[j] = NULL;
	
	// add the newly invalid workspaces (if any)
	if (to_invalid) g_ptr_array_extend_and_steal (s_aInvalid, to_invalid); // this will free to_invalid
	
	if (bRemoved || bCoords || bAdded)
		s_bLayoutChanged = TRUE;
	if ((s_pCurrent != s_pPending) || bRemoved || bCoords || bAdded)
	{
		s_pCurrent = s_pPending;
		if (!s_sidNotify) s_sidNotify = g_idle_add (_notify, NULL);
	}
	
	if (bName && !s_sidNotifyName) s_sidNotifyName = g_idle_add (_notify_name, NULL);
}

static void _finished (void*, struct ext_workspace_manager_v1 *handle)
{
	ext_workspace_manager_v1_destroy (handle);
}

static const struct ext_workspace_manager_v1_listener manager_listener = {
	.workspace_group = _new_workspace_group,
	.workspace = _desktop_created,
	.done = _done,
	.finished = _finished
};


static gchar** _get_desktops_names (void)
{
	CosmicWS **desktops = (CosmicWS**)s_aDesktops->pdata;
	unsigned int s_iNumDesktops = s_aDesktops->len;
	
	gchar **ret = g_new0 (gchar*, s_iNumDesktops + 1); // + 1, so that it is a null-terminated list, as expected by the switcher plugin
	unsigned int i;
	for (i = 0; i < s_iNumDesktops; i++) ret[i] = g_strdup (desktops[i]->name);
	return ret;
}

static unsigned int _get_ix (guint x, guint y)
{
	CosmicWS **desktops = (CosmicWS**)s_aDesktops->pdata;
	unsigned int s_iNumDesktops = s_aDesktops->len;
	
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
		CosmicWS **desktops = (CosmicWS**)s_aDesktops->pdata;
		unsigned int s_iNumDesktops = s_aDesktops->len;
		
		unsigned int iReq = _get_ix ((guint)iViewportNumberX, (guint)iViewportNumberY);
		if (iReq < s_iNumDesktops)
		{
			ext_workspace_handle_v1_activate (desktops[iReq]->handle);
			ext_workspace_manager_v1_commit (s_pWSManager);
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
		ext_workspace_group_handle_v1_create_workspace (s_pWSGroup, name);
		zcosmic_workspace_manager_v1_commit (s_pWSManager);
		g_free (name);
	}
}

static void _remove_workspace (void)
{
	if (s_iNumDesktops <= 1 || !s_pWSManager) return;
	ext_workspace_handle_v1_remove (desktops[s_iNumDesktops - 1]->handle);
	zcosmic_workspace_manager_v1_commit (s_pWSManager);
}
*/

static uint32_t protocol_id, protocol_version;
static gboolean protocol_found = FALSE;

gboolean gldi_ext_workspaces_match_protocol (uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp(interface, ext_workspace_manager_v1_interface.name))
	{
		protocol_found = TRUE;
		protocol_id = id;
		protocol_version = version;
		if ((uint32_t)ext_workspace_manager_v1_interface.version < protocol_version)
			protocol_version = ext_workspace_manager_v1_interface.version;
		return TRUE;
	}
	return FALSE;
}

gboolean gldi_ext_workspaces_try_init (struct wl_registry *registry)
{
	if (!protocol_found) return FALSE;
	s_pWSManager = wl_registry_bind (registry, protocol_id, &ext_workspace_manager_v1_interface, protocol_version);
	if (!s_pWSManager) return FALSE;
	
	GldiDesktopManagerBackend dmb;
	memset (&dmb, 0, sizeof (GldiDesktopManagerBackend));
	dmb.set_current_desktop   = _set_current_desktop;
	dmb.get_desktops_names    = _get_desktops_names;
//	dmb.add_workspace         = _add_workspace;
//	dmb.remove_last_workspace = _remove_workspace;
	gldi_desktop_manager_register_backend (&dmb, "ext-workspace-v1");
	gldi_wayland_wm_set_pre_notify_function (_wm_notify);
	
	s_aDesktops = g_ptr_array_new_full (16, _free_workspace2);
	s_aInvalid = g_ptr_array_new_full (16, _free_workspace2);
	
	ext_workspace_manager_v1_add_listener (s_pWSManager, &manager_listener, NULL);
	return TRUE;
}

struct ext_workspace_handle_v1 *gldi_ext_workspaces_get_handle (int x, int y)
{
	CosmicWS **desktops = (CosmicWS**)s_aDesktops->pdata;
	unsigned int s_iNumDesktops = s_aDesktops->len;
	
	unsigned int iReq = _get_ix ((guint)x, (guint)y);
	if (iReq < s_iNumDesktops)
		return desktops[iReq]->handle;
	cd_warning ("cosmic-workspaces: invalid workspace requested!\n");
	return NULL;
}

gboolean gldi_ext_workspaces_find (struct ext_workspace_handle_v1 *handle, int *x, int *y)
{
	CosmicWS **desktops = (CosmicWS**)s_aDesktops->pdata;
	unsigned int s_iNumDesktops = s_aDesktops->len;
	
	unsigned int i;
	for (i = 0; i < s_iNumDesktops; i++)
	{
		if (desktops[i]->handle == handle)
		{
			if (bValidX)
			{
				*x = desktops[i]->x - s_iXOffset;
				if (bValidY) *y = desktops[i]->y - s_iYOffset;
				else *y = 0;
			}
			else
			{
				*x = i;
				*y = 0;
			}
			return TRUE;
		}
	}
	cd_warning ("ext-workspaces: workspace not found!\n");
	return FALSE;
}


