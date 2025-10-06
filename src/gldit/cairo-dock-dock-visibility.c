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
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-container.h"
#include "cairo-dock-object.h"
#include "cairo-dock-log.h"
#include "cairo-dock-windows-manager-priv.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-priv.h"
#include "cairo-dock-dock-visibility.h"


  /////////////////////
 // Dock visibility //
/////////////////////

static GldiDockVisibilityBackend s_backend = {0};

static inline gboolean _window_overlaps_area (const GldiWindowActor *actor, const GtkAllocation *pArea);
static inline gboolean _window_overlaps_dock (const GldiWindowActor *actor, const CairoDock *pDock);
static inline gboolean _dock_has_overlapping_window (GtkAllocation *pArea);


static void _get_dock_geometry (const CairoDock *pDock, GtkAllocation *pArea)
{
	if (pDock->container.bIsHorizontal)
	{
		pArea->x = pDock->container.iWindowPositionX;
		pArea->y = pDock->container.iWindowPositionY;
	}
	else
	{
		pArea->x = pDock->container.iWindowPositionY;
		pArea->y = pDock->container.iWindowPositionX;
	}
	
	if (gldi_container_is_wayland_backend ())
	{
		// adjust based on the corresponding output's coordinates
		//!! TODO: move this to the container backends !!
		gboolean bFound = FALSE;
		GdkMonitor *monitor = NULL;
		GdkWindow *gdkwindow = gldi_container_get_gdk_window (CAIRO_CONTAINER (pDock));
		if (gdkwindow) monitor = gdk_display_get_monitor_at_window (gdk_display_get_default (), gdkwindow);
		
		if (monitor)
		{
			int i, N;
			GdkMonitor *const *monitors = gldi_desktop_get_monitors (&N);
			for (i = 0; i < N; i++) if (monitors[i] == monitor)
			{
				bFound = TRUE;
				pArea->x += g_desktopGeometry.pScreens[i].x;
				pArea->y += g_desktopGeometry.pScreens[i].y;
				break;
			}
		}
		
		if (!bFound) cd_warning ("Cannot adjust dock position based on desktop geometry!");
	}
	
	
	if (pDock->container.bIsHorizontal)
	{
		pArea->width = pDock->iMinDockWidth;
		pArea->height = pDock->iMinDockHeight;
		pArea->x += (pDock->container.iWidth - pArea->width)/2;
		pArea->y += (pDock->container.bDirectionUp ? pDock->container.iHeight - pArea->height : 0);
	}
	else
	{
		pArea->width = pDock->iMinDockHeight;
		pArea->height = pDock->iMinDockWidth;
		pArea->x += (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iMinDockHeight : 0);
		pArea->y += (pDock->container.iWidth - pDock->iMinDockWidth)/2;
	}
}

static void _show_if_no_overlapping_window (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (cairo_dock_is_temporary_hidden (pDock))
	{
		if (!gldi_dock_has_overlapping_window (pDock))
		{
			cairo_dock_deactivate_temporary_auto_hide (pDock);
		}
	}
}

static void _hide_if_overlap (CairoDock *pDock, GldiWindowActor *pAppli)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (! cairo_dock_is_temporary_hidden (pDock))
	{
		if (gldi_window_is_on_current_desktop (pAppli) && _window_overlaps_dock (pAppli, pDock))
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
}

static void _hide_if_overlap_or_show_if_no_overlapping_window (CairoDock *pDock, GldiWindowActor *pAppli)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	GtkAllocation area;
	_get_dock_geometry (pDock, &area);
	if (_window_overlaps_area (pAppli, &area))  // cette fenetre peut provoquer l'auto-hide.
	{
		if (! cairo_dock_is_temporary_hidden (pDock))
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
	else  // ne gene pas/plus.
	{
		if (cairo_dock_is_temporary_hidden (pDock))
		{
			if (!_dock_has_overlapping_window (&area))
			{
				cairo_dock_deactivate_temporary_auto_hide (pDock);
			}
		}
	}
}

static void _hide_show_if_on_our_way (CairoDock *pDock, GldiWindowActor *pCurrentAppli)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP)
		return ;
	
	/* hide the dock if the active window or its parent if this window doesn't
	 * have any dedicated icon in the dock -> If my window's text editor is
	 * maximised and then I open a 'Search' box, the dock should not appear
	 * above the maximised window
	 */
	gboolean bShow = TRUE;
	if (pCurrentAppli)
	{
		GtkAllocation area;
		_get_dock_geometry (pDock, &area);
		
		if (gldi_window_is_on_current_desktop (pCurrentAppli) && _window_overlaps_area (pCurrentAppli, &area))
			bShow = FALSE;
		else
		{
			GldiWindowActor *pParentAppli = pCurrentAppli->bIsTransientFor ?
				gldi_window_get_transient_for (pCurrentAppli) : NULL;
		
			if (pParentAppli && gldi_window_is_on_current_desktop (pParentAppli) &&
				_window_overlaps_area (pParentAppli, &area))
			{
				bShow = FALSE;
			}
		}
	}
	
	if (bShow)
	{
		if (cairo_dock_is_temporary_hidden (pDock))
			cairo_dock_deactivate_temporary_auto_hide (pDock);
	}
	else if (!cairo_dock_is_temporary_hidden (pDock))
		cairo_dock_activate_temporary_auto_hide (pDock);
}

static void _hide_if_any_overlap_or_show (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (cairo_dock_is_temporary_hidden (pDock))
	{
		if (!gldi_dock_has_overlapping_window (pDock))
		{
			cairo_dock_deactivate_temporary_auto_hide (pDock);
		}
	}
	else
	{
		if (gldi_dock_has_overlapping_window (pDock))
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
}


  ///////////////
 // Callbacks //
///////////////

static gboolean _on_window_created (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	// docks visibility on overlap any
	/// see how to handle modal dialogs ...
	gldi_docks_foreach_root ((GFunc)_hide_if_overlap, actor);
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_destroyed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	// docks visibility on overlap any
	gboolean bIsHidden = actor->bIsHidden;  // the window is already destroyed, but the actor is still valid (it represents the last state of the window); temporarily make it hidden so that it doesn't overlap the dock (that's a bit tricky, we could also add an "except-this-window" parameter to 'gldi_dock_has_overlapping_window()')
	actor->bIsHidden = TRUE;
	gldi_docks_foreach_root ((GFunc)_show_if_no_overlapping_window, NULL);
	actor->bIsHidden = bIsHidden;
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_size_position_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	// docks visibility on overlap any
	if (! gldi_window_is_on_current_desktop (actor))  // not on this desktop/viewport any more
	{
		gldi_docks_foreach_root ((GFunc)_show_if_no_overlapping_window, NULL);
	}
	else  // elle est sur le viewport courant.
	{
		gldi_docks_foreach_root ((GFunc)_hide_if_overlap_or_show_if_no_overlapping_window, actor);
	}
	
	// docks visibility on overlap active
	if (actor == gldi_windows_get_active())  // c'est la fenetre courante qui a change de bureau.
	{
		gldi_docks_foreach_root ((GFunc)_hide_show_if_on_our_way, actor);
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_state_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor, gboolean bHiddenChanged, G_GNUC_UNUSED gboolean bMaximizedChanged, gboolean bFullScreenChanged)
{
	// docks visibility on overlap active
	if (actor == gldi_windows_get_active())  // c'est la fenetre courante qui a change d'etat.
	{
		if (bHiddenChanged || bFullScreenChanged)  // si c'est l'etat maximise qui a change, on le verra au changement de dimensions.
		{
			gldi_docks_foreach_root ((GFunc)_hide_show_if_on_our_way, actor);
		}
	}
	
	// docks visibility on overlap any
	if (bHiddenChanged)
	{
		if (!actor->bIsHidden)  // la fenetre reapparait.
			gldi_docks_foreach_root ((GFunc)_hide_if_overlap, actor);
		else  // la fenetre se cache.
			gldi_docks_foreach_root ((GFunc)_show_if_no_overlapping_window, NULL);
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_desktop_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	// docks visibility on overlap active
	if (actor == gldi_windows_get_active())  // c'est la fenetre courante qui a change de bureau.
	{
		gldi_docks_foreach_root ((GFunc)_hide_show_if_on_our_way, actor);
	}
	
	// docks visibility on overlap any
	if (gldi_window_is_on_current_desktop (actor))  // petite optimisation : si l'appli arrive sur le bureau courant, on peut se contenter de ne verifier qu'elle.
	{
		gldi_docks_foreach_root ((GFunc)_hide_if_overlap, actor);
	}
	else  // la fenetre n'est plus sur le bureau courant.
	{
		gldi_docks_foreach_root ((GFunc)_show_if_no_overlapping_window, NULL);
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_desktop_changed (G_GNUC_UNUSED gpointer data)
{
	// docks visibility on overlap active
	GldiWindowActor *pCurrentAppli = gldi_windows_get_active ();
	gldi_docks_foreach_root ((GFunc)_hide_show_if_on_our_way, pCurrentAppli);
	
	// docks visibility on overlap any
	gldi_docks_foreach_root ((GFunc)_hide_if_any_overlap_or_show, NULL);
	
	return GLDI_NOTIFICATION_LET_PASS;
}


static gboolean _on_active_window_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	// docks visibility on overlap active
	gldi_docks_foreach_root ((GFunc)_hide_show_if_on_our_way, actor);
	
	return GLDI_NOTIFICATION_LET_PASS;
}

  ///////////////
 // Utilities //
///////////////

static inline gboolean _window_overlaps_area (const GldiWindowActor *actor, const GtkAllocation *pArea)
{
	const GtkAllocation *pWindowGeometry = &actor->windowGeometry;
	return (pWindowGeometry->x < pArea->x + pArea->width &&
		pWindowGeometry->x + pWindowGeometry->width > pArea->x &&
		pWindowGeometry->y < pArea->y + pArea->height &&
		pWindowGeometry->y + pWindowGeometry->height > pArea->y);
}

static inline gboolean _window_overlaps_dock (const GldiWindowActor *actor, const CairoDock *pDock)
{
	if (actor->bIsHidden || !actor->bDisplayed) return FALSE;
	GtkAllocation area;
	_get_dock_geometry (pDock, &area);
	return _window_overlaps_area (actor, &area);
}

static gboolean _window_is_overlapping_dock (GldiWindowActor *actor, gpointer data)
{
	const GtkAllocation *pArea = (const GtkAllocation*)data;
	if (gldi_window_is_on_current_desktop (actor) && ! actor->bIsHidden && actor->bDisplayed)
	{
		return _window_overlaps_area (actor, pArea);
	}
	return FALSE;
}

static inline gboolean _dock_has_overlapping_window (GtkAllocation *pArea)
{
	return (gldi_windows_find (_window_is_overlapping_dock, pArea) != NULL);
}


static gboolean _has_overlap (CairoDock *pDock)
{
	GtkAllocation area;
	_get_dock_geometry (pDock, &area);
	return (gldi_windows_find (_window_is_overlapping_dock, &area) != NULL);
}

gboolean gldi_dock_has_overlapping_window (CairoDock *pDock)
{
	if (s_backend.has_overlapping_window) return s_backend.has_overlapping_window (pDock);
	return FALSE;
}


  ////////////
 /// INIT ///
////////////

static void _refresh (CairoDock *pDock)
{	
	if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		_hide_if_any_overlap_or_show (pDock, NULL);
	else if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP)
	{
		GldiWindowActor *pCurrentAppli = gldi_windows_get_active ();
		_hide_show_if_on_our_way (pDock, pCurrentAppli);
	}
}

void gldi_dock_visibility_refresh (CairoDock *pDock)
{
	if (s_backend.refresh) s_backend.refresh (pDock);
}


static void _start_default_backend (void)
{
	static gboolean first = TRUE;
	
	if (s_backend.name) return; // other backend is already registered
	
	// if WM does not provide coordinates, we cannot do anything
	if (! (gldi_window_manager_have_coordinates () && gldi_window_manager_can_track_workspaces ()) )
		return;
	
	// register our backend
	s_backend.refresh = _refresh;
	s_backend.has_overlapping_window = _has_overlap;
	s_backend.name = "default-wm-coords";
	
	// register to events
	if (first)
	{
		first = FALSE;
		gldi_object_register_notification (&myWindowObjectMgr,
			NOTIFICATION_WINDOW_CREATED,
			(GldiNotificationFunc) _on_window_created,
			GLDI_RUN_FIRST, NULL);
		gldi_object_register_notification (&myWindowObjectMgr,
			NOTIFICATION_WINDOW_DESTROYED,
			(GldiNotificationFunc) _on_window_destroyed,
			GLDI_RUN_FIRST, NULL);
		gldi_object_register_notification (&myWindowObjectMgr,
			NOTIFICATION_WINDOW_SIZE_POSITION_CHANGED,
			(GldiNotificationFunc) _on_window_size_position_changed,
			GLDI_RUN_FIRST, NULL);
		gldi_object_register_notification (&myWindowObjectMgr,
			NOTIFICATION_WINDOW_STATE_CHANGED,
			(GldiNotificationFunc) _on_window_state_changed,
			GLDI_RUN_FIRST, NULL);
		gldi_object_register_notification (&myWindowObjectMgr,
			NOTIFICATION_WINDOW_DESKTOP_CHANGED,
			(GldiNotificationFunc) _on_window_desktop_changed,
			GLDI_RUN_FIRST, NULL);
		gldi_object_register_notification (&myDesktopMgr,
			NOTIFICATION_DESKTOP_CHANGED,
			(GldiNotificationFunc) _on_desktop_changed,
			GLDI_RUN_FIRST, NULL);
		gldi_object_register_notification (&myWindowObjectMgr,
			NOTIFICATION_WINDOW_ACTIVATED,
			(GldiNotificationFunc) _on_active_window_changed,
			GLDI_RUN_FIRST, NULL);
	}
}


static void _refresh2 (CairoDock *pDock, G_GNUC_UNUSED gpointer dummy)
{
	gldi_dock_visibility_refresh (pDock);
}

void gldi_docks_visibility_start (void)
{
	_start_default_backend ();
	
	gldi_docks_foreach_root ((GFunc)_refresh2, NULL);
}

/*
static void _unhide_all_docks (CairoDock *pDock, G_GNUC_UNUSED Icon *icon)
{
	if (cairo_dock_is_temporary_hidden (pDock))
		cairo_dock_deactivate_temporary_auto_hide (pDock);
}

 * TODO: add stop function to backends?
void gldi_docks_visibility_stop (void)  // not used yet
{
	gldi_docks_foreach_root ((GFunc)_unhide_all_docks, NULL);
}
*/

void gldi_dock_visibility_register_backend (GldiDockVisibilityBackend *pBackend)
{
	if (s_backend.name)
	{
		cd_error ("dock visibility backend already registered!");
		return;
	}
	
	gpointer *ptr = (gpointer*)&s_backend;
	gpointer *src = (gpointer*)pBackend;
	gpointer *src_end = (gpointer*)(pBackend + 1);
	while (src != src_end)
	{
		*ptr = *src;
		src ++;
		ptr ++;
	}
}

const char *gldi_dock_visbility_get_backend_name (void)
{
	return s_backend.name ? s_backend.name : "none";
}

