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

static void _remove_desktop (unsigned int i)
{
	unsigned int j;
	for (j = i + 1; j < s_iNumDesktops; j++) desktops[j - 1] = desktops[j];
	s_iNumDesktops--;
	desktops[s_iNumDesktops] = NULL;
	g_desktopGeometry.iNbDesktops--;
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
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
		g_desktopGeometry.iCurrentDesktop = i;
		gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_CHANGED);
	}
	else cd_error ("plasma-virtual-desktop: could not find currently activated desktop!");
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
	g_desktopGeometry.iNbDesktops = s_iNumDesktops;
	
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, FALSE);
	
	desktop->handle = org_kde_plasma_virtual_desktop_management_get_virtual_desktop(manager, desktop_id);
	org_kde_plasma_virtual_desktop_add_listener (desktop->handle, &desktop_listener, desktop);
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
	G_GNUC_UNUSED uint32_t rows)
{
	// no-op
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

static gboolean _set_current_desktop (int iDesktopNumber, G_GNUC_UNUSED int iViewportNumberX, G_GNUC_UNUSED int iViewportNumberY)
{
	// viewport is ignored
	if (iDesktopNumber >= 0 && (unsigned int)iDesktopNumber < s_iNumDesktops)
	{
		org_kde_plasma_virtual_desktop_request_activate (desktops[iDesktopNumber]->handle);
		return TRUE; // we don't know if we succeeded
	}
	return FALSE;
}


static uint32_t protocol_id, protocol_version;
static gboolean protocol_found = FALSE;
static struct org_kde_plasma_virtual_desktop_management* s_pmanager = NULL;

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
	dmb.set_current_desktop = _set_current_desktop;
	dmb.get_desktops_names = _get_desktops_names;
	gldi_desktop_manager_register_backend (&dmb, "plasma-virtual-desktop");
	
	org_kde_plasma_virtual_desktop_management_add_listener (s_pmanager, &manager_listener, NULL);
	return TRUE;
}

