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


/** Structure representing one workspace group */
typedef struct _ExtWSGroup {
	struct ext_workspace_group_handle_v1 *handle; // protocol object representing this group
	GPtrArray *aDesktops; // array of ExtWS objects for workspaces added to this group (updated in the done event handler)
	
	gboolean bValidX; // if x coordinates are valid for this group
	gboolean bValidY; // if y coordinates are valid for this group
	guint iXOffset; // offset to add to x coordinates (if they don't start from 0)
	guint iYOffset; // offset to add to x coordinates (if they don't start from 0)
	
	gboolean bRemoved; // this workspace group has been removed (considered inert)

	//!! TODO: track outputs? will be needed for moving windows among workspaces
} ExtWSGroup;

/** Structure representing one workspace */
typedef struct _ExtWS {
	struct ext_workspace_handle_v1 *handle; // protocol object representing this desktop
	char *name; // can be NULL if no name was delivered
	guint x, y; // x and y coordinates; (guint)-1 means invalid
	guint pending_x, pending_y;
	char *pending_name;
	ExtWSGroup *group_pending; // ws group (pending until next done event)
	gboolean bRemoved; // set to TRUE when receiving the removed event
	gboolean bHiddenPending; // set to TRUE when received a hidden event
} ExtWS;


#define INVALID_COORD (guint)-1

/// private variables -- track the current state of workspaces
static GPtrArray *s_aDesktops = NULL; // list of all workspaces known to us (irrespective to whether they are added to any workspace group)
static GPtrArray *s_aGroups = NULL; // list of all workspace groups; these two lists are allocated in try_init(), so they are always non-NULL
static ExtWS *s_pCurrent = NULL; // currently active workspace
static ExtWS *s_pPending = NULL; // workspace with pending activation

static struct ext_workspace_manager_v1 *s_pWSManager = NULL;

static guint s_sidNotify = 0;
static gboolean s_bLayoutChanged = FALSE;
static gboolean s_bLayoutNotify = FALSE;
static gboolean s_bNamesNotify = FALSE;

// we don't want arbitrarily large coordinates
#define MAX_DESKTOP_DIM 16
#define MAX_DESKTOP_NUM 128

/* To be called after all workspaces have been delivered, from the workspace manager's done event.
 * Note: caller should update iNbDesktops and pViewportsX/Y in g_desktopGeometry before this. */
static void _update_group_layout (unsigned int ix)
{
	guint rows = 0, cols = 0;
	
	ExtWSGroup *group = (ExtWSGroup*)s_aGroups->pdata[ix];
	
	unsigned int i;
	gboolean have_invalid_x = FALSE;
	gboolean have_invalid_y = FALSE;
	gboolean all_invalid_y = TRUE;
	group->iXOffset = G_MAXUINT;
	group->iYOffset = G_MAXUINT;
	
	ExtWS **desktops = (ExtWS**)group->aDesktops->pdata;
	unsigned int iNumDesktops = group->aDesktops->len;
	
	for (i = 0; i < iNumDesktops; i++)
	{
		guint x = desktops[i]->x;
		guint y = desktops[i]->y;
		
		if (x == INVALID_COORD) have_invalid_x = TRUE;
		else 
		{
			if (x > cols) cols = x;
			if (x < group->iXOffset) group->iXOffset = x;
		}
		
		if (y == INVALID_COORD) have_invalid_y = TRUE;
		else {
			all_invalid_y = FALSE;
			if (y > rows) rows = y;
			if (y < group->iYOffset) group->iYOffset = y;
		}
	}
	
	if (iNumDesktops > 0 && !have_invalid_x)
	{
		if (have_invalid_y && all_invalid_y && cols < MAX_DESKTOP_NUM && cols/8 <= iNumDesktops)
		{
			group->bValidX = TRUE;
			group->bValidY = FALSE;
			g_desktopGeometry.pViewportsX[ix] = cols + 1 - group->iXOffset;
			g_desktopGeometry.pViewportsY[ix] = 1;
			return;
		}
		if (!have_invalid_y && cols < MAX_DESKTOP_DIM && rows < MAX_DESKTOP_DIM && rows*cols < MAX_DESKTOP_NUM && (rows*cols) / 8 <= iNumDesktops)
		{
			group->bValidX = TRUE;
			group->bValidY = TRUE;
			g_desktopGeometry.pViewportsX[ix] = cols + 1 - group->iXOffset;
			g_desktopGeometry.pViewportsY[ix] = rows + 1 - group->iYOffset;
			return;
		}
	}
	
	group->bValidX = FALSE;
	group->bValidY = FALSE;
	g_desktopGeometry.pViewportsX[ix] = iNumDesktops ? iNumDesktops : 1;
	g_desktopGeometry.pViewportsY[ix] = 1;
	
	//!! TODO: update pDesktopSizes based on outputs assigned to this group !!
}

static void _update_desktop_layout (void)
{
	unsigned int iNumGroups = s_aGroups->len;
	g_desktopGeometry.iNbDesktops = iNumGroups;
	g_desktopGeometry.pViewportsX = g_renew (int, g_desktopGeometry.pViewportsX, iNumGroups);
	g_desktopGeometry.pViewportsY = g_renew (int, g_desktopGeometry.pViewportsY, iNumGroups);
	
	unsigned int i;
	for (i = 0; i < iNumGroups; i++) _update_group_layout (i);
}

static gboolean _update_current_desktop (void)
{
	gboolean ret = FALSE;
	
	// check if the current desktop is valid -> it has to belong to a workspace group
	// if we are here, group_pending should be valid
	// check if the current desktop is really valid
	ExtWS *desktop = s_pCurrent;
	if (!desktop || !desktop->group_pending) return ret; // note: we cannot signal "no active desktop" and this should not happen anyway
	
	ExtWSGroup *group = desktop->group_pending;
	unsigned int ix;
	if (!g_ptr_array_find (s_aGroups, group, &ix)) return ret; // should not happen
	
	if ((unsigned int)g_desktopGeometry.iCurrentDesktop != ix)
	{
		g_desktopGeometry.iCurrentDesktop = ix;
		ret = TRUE;
	}
	
	// check if we have valid coordinates
	int iNewX = 0, iNewY = 0;
	if (group->bValidX) iNewX = desktop->x - group->iXOffset;
	if (group->bValidY) iNewY = desktop->y - group->iYOffset;
	
	if (iNewX != g_desktopGeometry.iCurrentViewportX)
	{
		g_desktopGeometry.iCurrentViewportX = iNewX;
		ret = TRUE;
	}
	if (iNewY != g_desktopGeometry.iCurrentViewportY)
	{
		g_desktopGeometry.iCurrentViewportY = iNewY;
		ret = TRUE;
	}
	
	return ret;
}

static gboolean _notify (G_GNUC_UNUSED void *dummy)
{
	if (s_bLayoutNotify) _update_desktop_layout ();
	gboolean bDesktopChanged = _update_current_desktop ();
	
	if (s_bLayoutNotify) gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
	if (s_bLayoutNotify || bDesktopChanged) gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	if (s_bNamesNotify) gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_NAMES_CHANGED);
	
	s_bLayoutNotify = FALSE;
	s_bNamesNotify = FALSE;
	s_sidNotify = 0;
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
static void _id (G_GNUC_UNUSED void* data, G_GNUC_UNUSED struct ext_workspace_handle_v1* handle, G_GNUC_UNUSED const char* id)
{
	/* ID could be used to identify workspaces across sessions. For now, we don't care. */
}

static void _name (void *data, G_GNUC_UNUSED struct ext_workspace_handle_v1* handle, const char *name)
{
	ExtWS *desktop = (ExtWS*)data;
	g_free (desktop->pending_name);
	desktop->pending_name = g_strdup ((gchar *)name);
}

static void _coordinates (void *data, G_GNUC_UNUSED struct ext_workspace_handle_v1* handle, struct wl_array *coords)
{
	ExtWS *desktop = (ExtWS*)data;
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

static void _state (void *data, G_GNUC_UNUSED struct ext_workspace_handle_v1* handle, uint32_t state)
{
	gboolean bActivated = (state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE);
	gboolean bHidden = (state & EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN);
	
	ExtWS *desktop = (ExtWS*)data;
	if (desktop->bHiddenPending != bHidden)
	{
		desktop->bHiddenPending = bHidden;
		s_bLayoutChanged = TRUE;
	}
	if (bActivated) s_pPending = desktop;
}

static void _capabilities (G_GNUC_UNUSED void* data, G_GNUC_UNUSED struct ext_workspace_handle_v1* handle, G_GNUC_UNUSED uint32_t cap)
{
	/* don't care for now */
}

static void _removed (void *data, G_GNUC_UNUSED struct ext_workspace_handle_v1* handle)
{
	ExtWS *desktop = (ExtWS*)data;
	desktop->bRemoved = TRUE;
	if (desktop == s_pPending) s_pPending = NULL;
	s_bLayoutChanged = TRUE;
}


static void _free_workspace (ExtWS *desktop)
{
	g_free (desktop->name);
	g_free (desktop->pending_name);
	ext_workspace_handle_v1_destroy (desktop->handle);
	g_free (desktop);
}

static void _free_workspace2 (void *ptr) { _free_workspace ((ExtWS*)ptr); }


static const struct ext_workspace_handle_v1_listener desktop_listener = {
	.id = _id,
	.name = _name,
	.coordinates = _coordinates,
	.state = _state,
	.capabilities = _capabilities,
	.removed = _removed,
};


static void _group_capabilities (G_GNUC_UNUSED void* data, G_GNUC_UNUSED struct ext_workspace_group_handle_v1* handle, G_GNUC_UNUSED uint32_t cap)
{
	/* don't care */
}

static void _output_enter (G_GNUC_UNUSED void* data, G_GNUC_UNUSED struct ext_workspace_group_handle_v1* handle, G_GNUC_UNUSED struct wl_output* output)
{
	//!! TODO !!
}

static void _output_leave (G_GNUC_UNUSED void* data, G_GNUC_UNUSED struct ext_workspace_group_handle_v1* handle, G_GNUC_UNUSED struct wl_output* output)
{
	//!! TODO !!
}

static gboolean _ws_handle_eq (const void *x, const void *y)
{
	const ExtWS *desktop = (const ExtWS*)x;
	const struct ext_workspace_handle_v1 *handle = (const struct ext_workspace_handle_v1*)y;
	return (desktop->handle == handle);
}

static void _workspace_group_enter (void* data, struct ext_workspace_group_handle_v1* handle, struct ext_workspace_handle_v1 *ws)
{
	unsigned int ix;
	ExtWSGroup *group = (ExtWSGroup*)data;
	if (g_ptr_array_find_with_equal_func (s_aDesktops, ws, _ws_handle_eq, &ix))
	{
		ExtWS *desktop = s_aDesktops->pdata[ix];
		desktop->group_pending = data;
		s_bLayoutChanged = TRUE;
	}
}

static void _workspace_group_leave (void* data, G_GNUC_UNUSED struct ext_workspace_group_handle_v1* handle, struct ext_workspace_handle_v1 *ws)
{
	unsigned int ix;
	ExtWSGroup *group = (ExtWSGroup*)data;
	if (g_ptr_array_find_with_equal_func (s_aDesktops, ws, _ws_handle_eq, &ix))
	{
		ExtWS *desktop = s_aDesktops->pdata[ix];
		if (desktop->group_pending == group)
		{
			desktop->group_pending = NULL;
			s_bLayoutChanged = TRUE;
		}
	}
}

static void _group_removed (void *data, G_GNUC_UNUSED struct ext_workspace_group_handle_v1 *handle)
{
	ExtWSGroup *group = (ExtWSGroup*)data;
	group->bRemoved = TRUE;
	s_bLayoutChanged = TRUE;
}


static const struct ext_workspace_group_handle_v1_listener group_listener = {
	.capabilities = _group_capabilities,
	.output_enter = _output_enter,
	.output_leave = _output_leave,
	.workspace_enter = _workspace_group_enter,
	.workspace_leave = _workspace_group_leave,
	.removed = _group_removed
};


static void _new_workspace_group (G_GNUC_UNUSED void* data, G_GNUC_UNUSED struct ext_workspace_manager_v1* handle, struct ext_workspace_group_handle_v1 *new_group)
{
	ExtWSGroup *group = g_new0 (ExtWSGroup, 1);
	group->handle = new_group;
	group->aDesktops = g_ptr_array_new_full (16, NULL); // note: we do not free workspaces when removing from a group
	g_ptr_array_add (s_aGroups, group);
	ext_workspace_group_handle_v1_add_listener (new_group, &group_listener, group);
	s_bLayoutChanged = TRUE;
}

static void _desktop_created (G_GNUC_UNUSED void* data, G_GNUC_UNUSED struct ext_workspace_manager_v1* handle,
	struct ext_workspace_handle_v1 *new_workspace)
{
	ExtWS *desktop = g_new0 (ExtWS, 1);
	desktop->x = INVALID_COORD;
	desktop->y = INVALID_COORD;
	desktop->pending_x = INVALID_COORD;
	desktop->pending_y = INVALID_COORD;
	desktop->handle = new_workspace;
	g_ptr_array_add (s_aDesktops, desktop);
	ext_workspace_handle_v1_add_listener (new_workspace, &desktop_listener, desktop);
}


static void _free_group (void *data)
{
	ExtWSGroup *group = (ExtWSGroup*)data;
	g_ptr_array_free (group->aDesktops, TRUE); // TRUE: free the underlying storage (but not the array members)
	ext_workspace_group_handle_v1_destroy (group->handle);
	g_free (group);
}

static void _done (G_GNUC_UNUSED void* data, G_GNUC_UNUSED struct ext_workspace_manager_v1* handle)
{
	unsigned int i;
	gboolean bUpdateGroups = s_bLayoutChanged;
	
	if (bUpdateGroups)
	{
		// remove WS groups that have disappeared and re-do the assignment of workspaces to groups
		unsigned int n_groups = s_aGroups->len;
		unsigned int j = 0;
		for (i = 0; i < n_groups; i++)
		{
			ExtWSGroup *group = (ExtWSGroup*)s_aGroups->pdata[i];
			if (group->bRemoved)
			{
				// free this group
				_free_group (group);
			}
			else
			{
				// clear the workspace array (will be added back later)
				g_ptr_array_set_size (group->aDesktops, 0);
				// move up to fill in the space of removed groups
				if (i != j) s_aGroups->pdata[j] = s_aGroups->pdata[i];
				j++;
			}
		}
		s_aGroups->len = j; //!! TODO: is it OK to set this directly?
	}
	
	// check for coordinate changes and possibly re-assign all desktops
	for (i = 0; i < s_aDesktops->len;)
	{
		ExtWS *desktop = (ExtWS*)s_aDesktops->pdata[i];
		if (desktop->bRemoved)
		{
			g_ptr_array_remove_index_fast (s_aDesktops, i);
		}
		else
		{
			if (!desktop->bHiddenPending)
			{
				if (bUpdateGroups && desktop->group_pending)
				{
					ExtWSGroup *group = desktop->group_pending;
					g_ptr_array_add (group->aDesktops, desktop);
				}
			}
			
			if (desktop->pending_x != desktop->x)
			{
				desktop->x = desktop->pending_x;
				if (!desktop->bHiddenPending) s_bLayoutChanged = TRUE;
			}
			if (desktop->pending_y != desktop->y)
			{
				desktop->y = desktop->pending_y;
				if (!desktop->bHiddenPending) s_bLayoutChanged = TRUE;
			}
			if (desktop->pending_name)
			{
				g_free (desktop->name);
				desktop->name = desktop->pending_name;
				desktop->pending_name = NULL;
				if (!desktop->bHiddenPending) s_bNamesNotify = TRUE;
			}
			i++;
		}
	}

	s_bLayoutNotify = s_bLayoutChanged;
	s_bLayoutChanged = FALSE;
	if ((s_pCurrent != s_pPending) || s_bLayoutNotify || s_bNamesNotify)
	{
		s_pCurrent = s_pPending;
		if (!s_sidNotify) s_sidNotify = g_idle_add (_notify, NULL);
	}
}

static void _finished (G_GNUC_UNUSED void* data, struct ext_workspace_manager_v1 *handle)
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
	//!! TODO: this will not work properly, as the order is arbitrary !!
	ExtWS **desktops = (ExtWS**)s_aDesktops->pdata;
	unsigned int s_iNumDesktops = s_aDesktops->len;
	
	gchar **ret = g_new0 (gchar*, s_iNumDesktops + 1); // + 1, so that it is a null-terminated list, as expected by the switcher plugin
	unsigned int i;
	for (i = 0; i < s_iNumDesktops; i++) ret[i] = g_strdup (desktops[i]->name);
	return ret;
}

static unsigned int _get_ix (guint iDesktop, guint x, guint y)
{
	if (iDesktop >= s_aGroups->len) return s_aDesktops->len; // means invalid
	
	ExtWSGroup *group = (ExtWSGroup*)s_aGroups->pdata[iDesktop];
	ExtWS **desktops = (ExtWS**)group->aDesktops->pdata;
	unsigned int len = group->aDesktops->len;
	
	unsigned int i = 0;
	if (group->bValidX)
	{
		for (; i < len; i++)
		{
			if (!desktops[i]->bRemoved && (desktops[i]->x == x + group->iXOffset)
				&& (!group->bValidY || desktops[i]->y == y + group->iYOffset))
					break;
		}
		if (i == len) i = s_aDesktops->len;
	}
	else
	{
		if (x < len) i = x;
		else i = s_aDesktops->len; // means invalid
	}
	return i;
}

static void _set_current_desktop (int iDesktopNumber, int iViewportNumberX, int iViewportNumberY)
{
	if (iDesktopNumber >= 0 && iViewportNumberX >= 0 && iViewportNumberY >= 0)
	{
		unsigned int iReq = _get_ix ((guint)iDesktopNumber, (guint)iViewportNumberX, (guint)iViewportNumberY);
		if (iReq < s_aDesktops->len)
		{
			ExtWS *desktop = (ExtWS*)s_aDesktops->pdata[iReq];
			ext_workspace_handle_v1_activate (desktop->handle);
			ext_workspace_manager_v1_commit (s_pWSManager);
		}
	}
	cd_warning ("cosmic-workspaces: invalid workspace requested!\n");
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
	s_aGroups = g_ptr_array_new_full (16, _free_group);
	
	ext_workspace_manager_v1_add_listener (s_pWSManager, &manager_listener, NULL);
	return TRUE;
}

struct ext_workspace_handle_v1 *gldi_ext_workspaces_get_handle (int iDesktop, int x, int y)
{
	if (iDesktop < 0 && x < 0 && y < 0)
	{
		cd_warning ("cosmic-workspaces: invalid workspace requested!\n");
		return NULL;
	}
	
	unsigned int iReq = _get_ix ((guint)iDesktop, (guint)x, (guint)y);
	if (iReq < s_aDesktops->len)
	{
		ExtWS *desktop = (ExtWS*)s_aDesktops->pdata[iReq];
		return desktop->handle;
	}
	
	cd_warning ("cosmic-workspaces: invalid workspace requested!\n");
	return NULL;
}

gboolean gldi_ext_workspaces_find (struct ext_workspace_handle_v1 *handle, int *iNbDesktop, int *x, int *y)
{
	unsigned int i, j;
	for (i = 0; i < s_aGroups->len; i++)
	{
		ExtWSGroup *group = (ExtWSGroup*)s_aGroups->pdata[i];
		for (j = 0; j < group->aDesktops->len; j++)
		{
			ExtWS *desktop = (ExtWS*)group->aDesktops->pdata[j];
			if (desktop->handle == handle)
			{
				*iNbDesktop = i;
				if (group->bValidX)
				{
					*x = desktop->x - group->iXOffset;
					if (group->bValidY) *y = desktop->y - group->iYOffset;
					else *y = 0;
				}
				else
				{
					*x = j;
					*y = 0;
				}
				return TRUE;
			}
		}
	}
	
	cd_warning ("ext-workspaces: workspace not found!\n");
	return FALSE;
}


