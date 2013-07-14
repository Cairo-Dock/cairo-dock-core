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
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-visibility.h"


  /////////////////////
 // Dock visibility //
/////////////////////

static void _show_if_no_overlapping_window (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (cairo_dock_is_temporary_hidden (pDock))
	{
		if (gldi_dock_search_overlapping_window (pDock) == NULL)
		{
			cairo_dock_deactivate_temporary_auto_hide (pDock);
		}
	}
}

static void _hide_if_any_overlap (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (!cairo_dock_is_temporary_hidden (pDock))
	{
		if (gldi_dock_search_overlapping_window (pDock) != NULL)
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
}

static void _hide_if_overlap (CairoDock *pDock, GldiWindowActor *pAppli)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (! cairo_dock_is_temporary_hidden (pDock))
	{
		if (gldi_window_is_on_current_desktop (pAppli) && gldi_dock_overlaps_window (pDock, pAppli))
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
}

static void _hide_if_overlap_or_show_if_no_overlapping_window (CairoDock *pDock, GldiWindowActor *pAppli)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (gldi_dock_overlaps_window (pDock, pAppli))  // cette fenetre peut provoquer l'auto-hide.
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
			if (gldi_dock_search_overlapping_window (pDock) == NULL)
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
	GldiWindowActor *pParentAppli = NULL;
	if (pCurrentAppli && pCurrentAppli->bIsTransientFor)
	{
		pParentAppli = gldi_window_get_transient_for (pCurrentAppli);
	}
	if (_gldi_window_is_on_our_way (pCurrentAppli, pDock) // the new active window is above the dock
	|| (pParentAppli && _gldi_window_is_on_our_way (pParentAppli, pDock))) // it's a transient window; consider its parent too.
	{
		if (!cairo_dock_is_temporary_hidden (pDock))
			cairo_dock_activate_temporary_auto_hide (pDock);
	}
	else if (cairo_dock_is_temporary_hidden (pDock))
		cairo_dock_deactivate_temporary_auto_hide (pDock);
}

static void _hide_if_any_overlap_or_show (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (cairo_dock_is_temporary_hidden (pDock))
	{
		if (gldi_dock_search_overlapping_window (pDock) == NULL)
		{
			cairo_dock_deactivate_temporary_auto_hide (pDock);
		}
	}
	else
	{
		if (gldi_dock_search_overlapping_window (pDock) != NULL)
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

static gboolean _on_window_destroyed (G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GldiWindowActor *actor)
{
	// docks visibility on overlap any
	gldi_docks_foreach_root ((GFunc)_show_if_no_overlapping_window, NULL);
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_size_position_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	// docks visibility on overlap any
	if (! gldi_window_is_on_current_desktop (actor))  // not on this desktop/viewport any more
	{
		gldi_docks_foreach_root ((GFunc)_show_if_no_overlapping_window, actor);
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

void gldi_dock_hide_show_if_current_window_is_on_our_way (CairoDock *pDock)
{
	GldiWindowActor *pCurrentAppli = gldi_windows_get_active ();
	_hide_show_if_on_our_way (pDock, pCurrentAppli);
}

void gldi_dock_hide_if_any_window_overlap_or_show (CairoDock *pDock)
{
	_hide_if_any_overlap_or_show (pDock, NULL);
}

static inline gboolean _window_overlaps_dock (GtkAllocation *pWindowGeometry, gboolean bIsHidden, CairoDock *pDock)
{
	if (pWindowGeometry->width != 0 && pWindowGeometry->height != 0)
	{
		int iDockX, iDockY, iDockWidth, iDockHeight;
		if (pDock->container.bIsHorizontal)
		{
			iDockWidth = pDock->iMinDockWidth;
			iDockHeight = pDock->iMinDockHeight;
			iDockX = pDock->container.iWindowPositionX + (pDock->container.iWidth - iDockWidth)/2;
			iDockY = pDock->container.iWindowPositionY + (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iMinDockHeight : 0);
		}
		else
		{
			iDockWidth = pDock->iMinDockHeight;
			iDockHeight = pDock->iMinDockWidth;
			iDockX = pDock->container.iWindowPositionY + (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iMinDockHeight : 0);
			iDockY = pDock->container.iWindowPositionX + (pDock->container.iWidth - iDockHeight)/2;
		}
		
		if (! bIsHidden && pWindowGeometry->x < iDockX + iDockWidth && pWindowGeometry->x + pWindowGeometry->width > iDockX && pWindowGeometry->y < iDockY + iDockHeight && pWindowGeometry->y + pWindowGeometry->height > iDockY)
		{
			return TRUE;
		}
	}
	else
	{
		cd_warning (" unknown window geometry");
	}
	return FALSE;
}
gboolean gldi_dock_overlaps_window (CairoDock *pDock, GldiWindowActor *actor)
{
	return _window_overlaps_dock (&actor->windowGeometry, actor->bIsHidden, pDock);
}

static gboolean _window_is_overlapping_dock (GldiWindowActor *actor, gpointer data)
{
	CairoDock *pDock = CAIRO_DOCK (data);
	if (gldi_window_is_on_current_desktop (actor) && ! actor->bIsHidden)
	{
		if (gldi_dock_overlaps_window (pDock, actor))
		{
			return TRUE;
		}
	}
	return FALSE;
}
GldiWindowActor *gldi_dock_search_overlapping_window (CairoDock *pDock)
{
	return gldi_windows_find (_window_is_overlapping_dock, pDock);
}


  ////////////
 /// INIT ///
////////////

void gldi_docks_visibility_start (void)
{
	static gboolean first = TRUE;
	
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
	
	// handle current docks visibility
	GldiWindowActor *pCurrentAppli = gldi_windows_get_active ();
	gldi_docks_foreach_root ((GFunc)_hide_show_if_on_our_way, pCurrentAppli);
	
	gldi_docks_foreach_root ((GFunc)_hide_if_any_overlap, NULL);
}

static void _unhide_all_docks (CairoDock *pDock, G_GNUC_UNUSED Icon *icon)
{
	if (cairo_dock_is_temporary_hidden (pDock))
		cairo_dock_deactivate_temporary_auto_hide (pDock);
}
void gldi_docks_visibility_stop (void)  // not used yet
{
	gldi_docks_foreach_root ((GFunc)_unhide_all_docks, NULL);
}
