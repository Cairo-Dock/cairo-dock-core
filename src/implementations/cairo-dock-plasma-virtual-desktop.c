/*
 * cairo-dock-plasma-virtual-desktop.c -- desktop / workspace management
 *  facilities for KWin / KDE Plasma
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

#include "wayland-plasma-virtual-desktop-client-protocol.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-plasma-virtual-desktop.h"

typedef struct _PlasmaDesktop {
	struct org_kde_plasma_virtual_desktop *handle; // protocol object representing this desktop
	gchar *name; // can be NULL if no name was delivered
	gchar *id; // desktop_id used by the protocol and also by plasma-window-management
} PlasmaDesktop;


/// private variables -- track the current state of workspaces
static unsigned int s_iNumDesktops = 0;
static PlasmaDesktop **desktops = NULL; // allocated when the first desktop is added
static unsigned int s_iDesktopCap = 0; // capacity of the above array
static unsigned int s_iRows = 1; // number of rows the desktops are arranged into
static unsigned int s_iCurrent = 0; // index into desktops array with the currently active desktop

static void _update_desktop_layout ()
{
	unsigned int rows = s_iRows;
	if (!rows) rows = 1;
	unsigned int cols = s_iNumDesktops / rows + ((s_iNumDesktops % rows) ? 1 : 0);
	g_desktopGeometry.iNbDesktops = 1;
	g_desktopGeometry.iNbViewportX = cols;
	g_desktopGeometry.iNbViewportY = rows;
}

static void _update_current_desktop ()
{
	g_desktopGeometry.iCurrentDesktop = 0;
	g_desktopGeometry.iCurrentViewportY = s_iCurrent / g_desktopGeometry.iNbViewportX;
	g_desktopGeometry.iCurrentViewportX = s_iCurrent % g_desktopGeometry.iNbViewportX;
}

typedef struct _update_windows_par
{
	int cols_old;
	int desktop_added;
	int desktop_removed;
} update_windows_par;

static void _update_windows (gpointer ptr, gpointer data)
{
	GldiWindowActor *actor = (GldiWindowActor*)ptr;
	update_windows_par *par = (update_windows_par*)data;
	int cols_old = par->cols_old;
	int cols_new = g_desktopGeometry.iNbViewportX;
	
	int ix = actor->iViewPortY * cols_old + actor->iViewPortX;
	// handle the insertion or removal of desktops (which moves the index of desktops beyond it)
	if (par->desktop_added >= 0 && ix >= par->desktop_added) ix++;
	else if (par->desktop_removed >= 0 && ix >= par->desktop_removed && ix > 0) ix--;
	int newY = ix / cols_new;
	int newX = ix % cols_new;
	if (newX != actor->iViewPortX || newY != actor->iViewPortY)
	{
		actor->iViewPortY = newY;
		actor->iViewPortX = newX;
		gldi_object_notify (&myWindowObjectMgr, NOTIFICATION_WINDOW_DESKTOP_CHANGED, actor);
	}
}


static void _remove_desktop (unsigned int i)
{
	unsigned int j;
	for (j = i + 1; j < s_iNumDesktops; j++) desktops[j - 1] = desktops[j];
	s_iNumDesktops--;
	desktops[s_iNumDesktops] = NULL;
	int cols_old = g_desktopGeometry.iNbViewportX;
	_update_desktop_layout ();
	_update_current_desktop ();
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
	// note: this is not a "real" change, but the coordinates likely changed and we need to keep track of these
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	// update windows' position (x and y coordinates have likely changed)
	update_windows_par par = {.cols_old = cols_old, .desktop_added = -1, .desktop_removed = i};
	gldi_windows_foreach_unordered (_update_windows, &par);
}

int gldi_plasma_virtual_desktop_get_index (const char *desktop_id)
{
	unsigned int i;
	for (i = 0; i < s_iNumDesktops; i++) if (!(strcmp(desktops[i]->id, desktop_id))) break;
	return (i < s_iNumDesktops) ? (int)i : -1;
}

const gchar *gldi_plasma_virtual_desktop_get_id (int ix)
{
	if (ix < 0 || (unsigned int)ix >= s_iNumDesktops) return NULL;
	return desktops[ix]->id;
}


static void _desktop_id (void *data, G_GNUC_UNUSED struct org_kde_plasma_virtual_desktop *org_kde_plasma_virtual_desktop,
	const char *desktop_id)
{
	PlasmaDesktop *desktop = (PlasmaDesktop*)data;
	if (strcmp (desktop->id, desktop_id))
		cd_warning ("plasma-virtual-desktop: ID for desktop does not match: expected: %s, got: %s!", desktop->id, desktop_id);
}

static void _name (void *data, G_GNUC_UNUSED struct org_kde_plasma_virtual_desktop *org_kde_plasma_virtual_desktop,
	const char *name)
{
	PlasmaDesktop *desktop = (PlasmaDesktop*)data;
	g_free (desktop->name);
	desktop->name = g_strdup ((gchar *)name);
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_NAMES_CHANGED);
}

static void _activated (void *data, G_GNUC_UNUSED struct org_kde_plasma_virtual_desktop *org_kde_plasma_virtual_desktop)
{
	PlasmaDesktop *desktop = (PlasmaDesktop*)data;
	unsigned int i;
	for (i = 0; i < s_iNumDesktops; i++) if (desktops[i] == desktop) break;
	if (i < s_iNumDesktops)
	{
		s_iCurrent = i;
		_update_current_desktop ();
		gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	}
	else cd_critical ("plasma-virtual-desktop: could not find currently activated desktop!");
}

static void _deactivated (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct org_kde_plasma_virtual_desktop *org_kde_plasma_virtual_desktop)
{
	// no-op (we should get an activated event with the newly active desktop
}

static void _done (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct org_kde_plasma_virtual_desktop *org_kde_plasma_virtual_desktop)
{
	// no-op (we already send a notification when a new desktop is added)
}

static void _removed (void *data, struct org_kde_plasma_virtual_desktop *org_kde_plasma_virtual_desktop)
{
	PlasmaDesktop *desktop = (PlasmaDesktop*)data;
	unsigned int i;
	for (i = 0; i < s_iNumDesktops; i++) if (desktops[i] == desktop) break;
	if (i < s_iNumDesktops) _remove_desktop (i);
	g_free (desktop->id);
	g_free (desktop->name);
	g_free (desktop);
	org_kde_plasma_virtual_desktop_destroy (org_kde_plasma_virtual_desktop);
}


static const struct org_kde_plasma_virtual_desktop_listener desktop_listener = {
	.desktop_id = _desktop_id,
	.name = _name,
	.activated = _activated,
	.deactivated = _deactivated,
	.done = _done,
	.removed = _removed
};


static void _desktop_created (G_GNUC_UNUSED void *data, struct org_kde_plasma_virtual_desktop_management *manager,
	const char *desktop_id, uint32_t position)
{
	int cols_old = g_desktopGeometry.iNbViewportX;
	
	PlasmaDesktop *desktop = g_new0 (PlasmaDesktop, 1);
	desktop->id = g_strdup (desktop_id);
	if (s_iNumDesktops >= s_iDesktopCap)
	{
		desktops = g_renew (PlasmaDesktop*, desktops, s_iNumDesktops + 1);
		s_iDesktopCap = s_iNumDesktops + 1;
	}
	if (position >= s_iNumDesktops) desktops[s_iNumDesktops] = desktop;
	else
	{
		unsigned int i;
		for (i = s_iNumDesktops; i > position; i--) desktops[i] = desktops[i - 1];
		desktops[position] = desktop;
	}
	s_iNumDesktops++;
	_update_desktop_layout ();
	_update_current_desktop ();
	
	desktop->handle = org_kde_plasma_virtual_desktop_management_get_virtual_desktop(manager, desktop_id);
	org_kde_plasma_virtual_desktop_add_listener (desktop->handle, &desktop_listener, desktop);
	
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED, FALSE);
	
	if (s_iRows > 1)
	{
		// update windows' position (x and y coordinates have likely changed)
		update_windows_par par = {.cols_old = cols_old, .desktop_added = position, .desktop_removed = -1};
		gldi_windows_foreach_unordered (_update_windows, &par);
	}
}

static void _desktop_removed (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct org_kde_plasma_virtual_desktop_management *manager,
	const char *desktop_id)
{
	int i = gldi_plasma_virtual_desktop_get_index (desktop_id);
	if (i >= 0) _remove_desktop (i); // i == -1 case if _removed () was already called on the desktop object
}

static void _desktop_done (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct org_kde_plasma_virtual_desktop_management *manager)
{
	// no-op
}

static void _desktop_rows (G_GNUC_UNUSED void *data, G_GNUC_UNUSED struct org_kde_plasma_virtual_desktop_management *manager,
	uint32_t rows)
{
	s_iRows = rows;
	int cols_old = g_desktopGeometry.iNbViewportX;
	_update_desktop_layout ();
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
	// note: this is not a "real" change, but the coordinates likely changed and we need to keep track of these
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	// update windows' position (x and y coordinates have likely changed)
	update_windows_par par = {.cols_old = cols_old, .desktop_added = -1, .desktop_removed = -1};
	gldi_windows_foreach_unordered (_update_windows, &par);
}


static const struct org_kde_plasma_virtual_desktop_management_listener manager_listener = {
	.desktop_created = _desktop_created,
	.desktop_removed = _desktop_removed,
	.done = _desktop_done,
	.rows = _desktop_rows
};


static gchar** _get_desktops_names (void)
{
	gchar **ret = g_new0 (gchar*, s_iNumDesktops + 1); // + 1, so that it is a null-terminated list, as expected by the switcher plugin
	unsigned int i;
	for (i = 0; i < s_iNumDesktops; i++) ret[i] = g_strdup (desktops[i]->name);
	return ret;
}

static gboolean _set_current_desktop (G_GNUC_UNUSED int iDesktopNumber, int iViewportNumberX, int iViewportNumberY)
{
	// desktop number is ignored (it should be 0)
	unsigned int iReq = g_desktopGeometry.iNbViewportX * iViewportNumberY + iViewportNumberX;
	if (iReq < s_iNumDesktops)
	{
		org_kde_plasma_virtual_desktop_request_activate (desktops[iReq]->handle);
		return TRUE; // we don't know if we succeeded
	}
	return FALSE;
}

static struct org_kde_plasma_virtual_desktop_management* s_pmanager = NULL;

static void _add_workspace (void)
{
	if (!s_pmanager) return;
	char *name = g_strdup_printf ("Workspace %u", s_iNumDesktops + 1);
	org_kde_plasma_virtual_desktop_management_request_create_virtual_desktop (s_pmanager, name, s_iNumDesktops);
	g_free (name);
}

static void _remove_workspace (void)
{
	if (s_iNumDesktops <= 1 || !s_pmanager) return;
	org_kde_plasma_virtual_desktop_management_request_remove_virtual_desktop (s_pmanager, desktops[s_iNumDesktops - 1]->id);
}


static uint32_t protocol_id, protocol_version;
static gboolean protocol_found = FALSE;

gboolean gldi_plasma_virtual_desktop_match_protocol (uint32_t id, const char *interface, uint32_t version)
{
	if (!strcmp(interface, org_kde_plasma_virtual_desktop_management_interface.name))
	{
		protocol_found = TRUE;
		protocol_id = id;
		protocol_version = version;
		if ((uint32_t)org_kde_plasma_virtual_desktop_management_interface.version < protocol_version)
			protocol_version = org_kde_plasma_virtual_desktop_management_interface.version;
		return TRUE;
	}
	return FALSE;
}

gboolean gldi_plasma_virtual_desktop_try_init (struct wl_registry *registry)
{
	if (!protocol_found) return FALSE;
	s_pmanager = wl_registry_bind (registry, protocol_id, &org_kde_plasma_virtual_desktop_management_interface, protocol_version);
	if (!s_pmanager) return FALSE;
	
	GldiDesktopManagerBackend dmb;
	memset (&dmb, 0, sizeof (GldiDesktopManagerBackend));
	dmb.set_current_desktop   = _set_current_desktop;
	dmb.get_desktops_names    = _get_desktops_names;
	dmb.add_workspace         = _add_workspace;
	dmb.remove_last_workspace = _remove_workspace;
	gldi_desktop_manager_register_backend (&dmb, "plasma-virtual-desktop");
	
	org_kde_plasma_virtual_desktop_management_add_listener (s_pmanager, &manager_listener, NULL);
	return TRUE;
}

