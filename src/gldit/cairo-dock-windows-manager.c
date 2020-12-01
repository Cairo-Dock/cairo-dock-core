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

#include "cairo-dock-log.h"
#include "cairo-dock-desktop-manager.h"  // g_desktopGeometry
#include "cairo-dock-utils.h"  // cairo_dock_remove_version_from_string
#define _MANAGER_DEF_
#include "cairo-dock-windows-manager.h"

// public (manager, config, data)
GldiObjectManager myWindowObjectMgr;

// dependancies

// private
GList *s_pWindowsList = NULL;  // list of all window actors
static gboolean s_bSortedByZ = FALSE;  // whether the list is currently sorted by z-order
static gboolean s_bSortedByAge = FALSE;  // whether the list is currently sorted by age
static GldiWindowManagerBackend s_backend;


static gboolean on_zorder_changed (G_GNUC_UNUSED gpointer data)
{
	s_bSortedByZ = FALSE;  // invalidate the sorting
	return GLDI_NOTIFICATION_LET_PASS;
}

static int _compare_z_order (GldiWindowActor *actor1, GldiWindowActor *actor2)
{
	if (actor1->iStackOrder < actor2->iStackOrder)
		return -1;
	else if (actor1->iStackOrder > actor2->iStackOrder)
		return 1;
	else
		return 0;
}

static int _compare_age (GldiWindowActor *actor1, GldiWindowActor *actor2)
{
	if (actor1->iAge < actor2->iAge)
		return -1;
	else if (actor1->iAge > actor2->iAge)
		return 1;
	else
		return 0;
}

void gldi_windows_foreach (gboolean bOrderedByZ, GFunc callback, gpointer data)
{
	if (bOrderedByZ && ! s_bSortedByZ)
	{
		s_pWindowsList = g_list_sort (s_pWindowsList, (GCompareFunc)_compare_z_order);
		s_bSortedByZ = TRUE;
		s_bSortedByAge = FALSE;
	}
	else if (! bOrderedByZ && ! s_bSortedByAge)
	{
		s_pWindowsList = g_list_sort (s_pWindowsList, (GCompareFunc)_compare_age);
		s_bSortedByAge = TRUE;
		s_bSortedByZ = FALSE;
	}
	g_list_foreach (s_pWindowsList, callback, data);
}

GldiWindowActor *gldi_windows_find (gboolean (*callback) (GldiWindowActor*, gpointer), gpointer data)
{
	GldiWindowActor *actor;
	GList *a;
	for (a = s_pWindowsList; a != NULL; a = a->next)
	{
		actor = a->data;
		if (callback (actor, data))
			return actor;
	}
	return NULL;
}


  ///////////////
 /// BACKEND ///
///////////////

void gldi_windows_manager_register_backend (GldiWindowManagerBackend *pBackend)
{
	gpointer *ptr = (gpointer*)&s_backend;
	gpointer *src = (gpointer*)pBackend;
	gpointer *src_end = (gpointer*)(pBackend + 1);
	while (src != src_end)
	{
		if (*src != NULL)
			*ptr = *src;
		src ++;
		ptr ++;
	}
}

void gldi_window_move_to_desktop (GldiWindowActor *actor, int iNumDesktop, int iNumViewportX, int iNumViewportY)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.move_to_nth_desktop)
		s_backend.move_to_nth_desktop (actor,
			iNumDesktop,
			(iNumViewportX - g_desktopGeometry.iCurrentViewportX) * gldi_desktop_get_width(),
			(iNumViewportY - g_desktopGeometry.iCurrentViewportY) * gldi_desktop_get_height());
}

void gldi_window_show (GldiWindowActor *actor)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.show)
		s_backend.show (actor);
}


void gldi_window_close (GldiWindowActor *actor)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.close)
		s_backend.close (actor);
}

void gldi_window_kill (GldiWindowActor *actor)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.kill)
		s_backend.kill (actor);
}

void gldi_window_minimize (GldiWindowActor *actor)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.minimize)
		s_backend.minimize (actor);
}

void gldi_window_lower (GldiWindowActor *actor)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.lower)
		s_backend.lower (actor);
}

void gldi_window_maximize (GldiWindowActor *actor, gboolean bMaximize)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.maximize)
		s_backend.maximize (actor, bMaximize);
}

void gldi_window_set_fullscreen (GldiWindowActor *actor, gboolean bFullScreen)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.set_fullscreen)
		s_backend.set_fullscreen (actor, bFullScreen);
}

void gldi_window_set_above (GldiWindowActor *actor, gboolean bAbove)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.set_above)
		s_backend.set_above (actor, bAbove);
}

void gldi_window_set_minimize_position (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.set_minimize_position)
		s_backend.set_minimize_position (actor, pContainerWidget, x, y);
}

void gldi_window_set_thumbnail_area (GldiWindowActor *actor, GtkWidget* pContainerWidget, int x, int y, int w, int h)
{
	g_return_if_fail (actor != NULL);
	if (s_backend.set_thumbnail_area)
		s_backend.set_thumbnail_area (actor, pContainerWidget, x, y, w, h);
}

GldiWindowActor *gldi_windows_get_active (void)
{
	if (s_backend.get_active_window)
		return s_backend.get_active_window ();
	return NULL;
}

void gldi_window_set_border (GldiWindowActor *actor, gboolean bWithBorder)
{
	if (s_backend.set_window_border)
		s_backend.set_window_border (actor, bWithBorder);
}

cairo_surface_t *gldi_window_get_icon_surface (GldiWindowActor *actor, int iWidth, int iHeight)
{
	g_return_val_if_fail (actor != NULL, NULL);
	if (s_backend.get_icon_surface)
		return s_backend.get_icon_surface (actor, iWidth, iHeight);
	return NULL;
}

cairo_surface_t *gldi_window_get_thumbnail_surface (GldiWindowActor *actor, int iWidth, int iHeight)
{
	g_return_val_if_fail (actor != NULL, NULL);
	if (s_backend.get_thumbnail_surface)
		return s_backend.get_thumbnail_surface (actor, iWidth, iHeight);
	return NULL;
}

GLuint gldi_window_get_texture (GldiWindowActor *actor)
{
	g_return_val_if_fail (actor != NULL, 0);
	if (s_backend.get_texture)
		return s_backend.get_texture (actor);
	return 0;
}

GldiWindowActor *gldi_window_get_transient_for (GldiWindowActor *actor)
{
	g_return_val_if_fail (actor != NULL, NULL);
	if (s_backend.get_transient_for)
		return s_backend.get_transient_for (actor);
	return NULL;
}

void gldi_window_is_above_or_below (GldiWindowActor *actor, gboolean *bIsAbove, gboolean *bIsBelow)
{
	if (s_backend.set_window_border)
		s_backend.is_above_or_below (actor, bIsAbove, bIsBelow);
	else
	{
		*bIsAbove = FALSE;
		*bIsBelow = FALSE;
	}
}

gboolean gldi_window_is_sticky (GldiWindowActor *actor)
{
	return actor->bIsSticky;
}

void gldi_window_set_sticky (GldiWindowActor *actor, gboolean bSticky)
{
	if (s_backend.set_sticky)
		s_backend.set_sticky (actor, bSticky);
}

void gldi_window_can_minimize_maximize_close (GldiWindowActor *actor, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose)
{
	if (s_backend.can_minimize_maximize_close)
		s_backend.can_minimize_maximize_close (actor, bCanMinimize, bCanMaximize, bCanClose);
	else  // assume that the window can mnimize/maximize/close (default behavior)
	{
		*bCanMinimize = TRUE;
		*bCanMaximize = TRUE;
		*bCanClose = TRUE;
	}
}

guint gldi_window_get_id (GldiWindowActor *actor)
{
	if (actor && s_backend.get_id)
		return s_backend.get_id (actor);
	return 0;
}

GldiWindowActor *gldi_window_pick (GtkWindow *pParentWindow)
{
	if (s_backend.pick_window)
		return s_backend.pick_window (pParentWindow);
	return NULL;
}


  /////////////////
 /// UTILITIES ///
/////////////////

static inline gboolean _window_is_on_current_desktop (GtkAllocation *pWindowGeometry, int iWindowDesktopNumber)
{
	int iGlobalPositionX, iGlobalPositionY, iWidthExtent, iHeightExtent;  // coordonnees du coin haut gauche dans le referentiel du viewport actuel.
	iGlobalPositionX = pWindowGeometry->x;
	iGlobalPositionY = pWindowGeometry->y;
	iWidthExtent = pWindowGeometry->width;
	iHeightExtent = pWindowGeometry->height;
	
	return ( (iWindowDesktopNumber == g_desktopGeometry.iCurrentDesktop || iWindowDesktopNumber == -1) &&
		iGlobalPositionX + iWidthExtent > 0 &&
		iGlobalPositionX < gldi_desktop_get_width() &&
		iGlobalPositionY + iHeightExtent > 0 &&
		iGlobalPositionY < gldi_desktop_get_height() );
}
gboolean gldi_window_is_on_current_desktop (GldiWindowActor *actor)
{
	///return (actor->iNumDesktop == -1 || actor->iNumDesktop == g_desktopGeometry.iCurrentDesktop) && actor->iViewPortX == g_desktopGeometry.iCurrentViewportX && actor->iViewPortY == g_desktopGeometry.iCurrentViewportY;  /// TODO: check that it works
	return actor->bIsSticky || _window_is_on_current_desktop (&actor->windowGeometry, actor->iNumDesktop);
}


gboolean gldi_window_is_on_desktop (GldiWindowActor *pAppli, int iNumDesktop, int iNumViewportX, int iNumViewportY)
{
	if (pAppli->bIsSticky || pAppli->iNumDesktop == -1)  // a sticky window is by definition on all desktops/viewports
		return TRUE;
	// On calcule les coordonnees en repere absolu.
	int x = pAppli->windowGeometry.x;  // par rapport au viewport courant.
	x += g_desktopGeometry.iCurrentViewportX * gldi_desktop_get_width();  // repere absolu
	if (x < 0)
		x += g_desktopGeometry.iNbViewportX * gldi_desktop_get_width();
	int y = pAppli->windowGeometry.y;
	y += g_desktopGeometry.iCurrentViewportY * gldi_desktop_get_height();
	if (y < 0)
		y += g_desktopGeometry.iNbViewportY * gldi_desktop_get_height();
	int w = pAppli->windowGeometry.width, h = pAppli->windowGeometry.height;
	
	// test d'intersection avec le viewport donne.
	return ((pAppli->iNumDesktop == -1 || pAppli->iNumDesktop == iNumDesktop) &&
		x + w > iNumViewportX * gldi_desktop_get_width() &&
		x < (iNumViewportX + 1) * gldi_desktop_get_width() &&
		y + h > iNumViewportY * gldi_desktop_get_height() &&
		y < (iNumViewportY + 1) * gldi_desktop_get_height());
}

void gldi_window_move_to_current_desktop (GldiWindowActor *pAppli)
{
	gldi_window_move_to_desktop (pAppli,
		g_desktopGeometry.iCurrentDesktop,
		g_desktopGeometry.iCurrentViewportX,
		g_desktopGeometry.iCurrentViewportY);  // on ne veut pas decaler son viewport par rapport a nous.
}


gchar* gldi_window_parse_class(const gchar* res_class, const gchar* res_name) {
	gchar *cClass = NULL, *cWmClass = NULL;
	if (res_class)
	{
		cd_debug ("  res_name : %s(%x); res_class : %s(%x)", res_name, res_name, res_class, res_class);
		if (strcmp (res_class, "Wine") == 0 && res_name && (g_str_has_suffix (res_name, ".exe") || g_str_has_suffix (res_name, ".EXE")))  // wine application: use the name instead, because we don't want to group all wine apps togather
		{
			cd_debug ("  wine application detected, changing the class '%s' to '%s'", res_class, res_name);
			cClass = g_ascii_strdown (res_name, -1);
		}
		// chromium web apps (not the browser): same remark as for wine apps
		else if (res_name && res_name[0] != '\0' && res_class[0] != '\0'
		         && (
		          ((res_class[0] == 'c' || res_class[0] == 'C') && (strcmp(res_class+1, "hromium-browser") == 0 || strcmp(res_class+1, "hromium") == 0))
		          || strcmp (res_class, "Google-chrome") == 0    // from Google
		          || strcmp (res_class, "Google-chrome-beta") == 0
		          || strcmp (res_class, "Google-chrome-unstable") == 0)
		         && strcmp (res_class+1, res_name+1) != 0) // skip first letter (upper/lowercase)
		{
			cClass = g_ascii_strdown (res_name, -1);

			/* Remove spaces. Why do they add spaces here?
			 * (e.g.: Google-chrome-unstable (/home/$USER/.config/google-chrome-unstable))
			 */
			gchar *str = strchr (cClass, ' ');
			if (str != NULL)
				*str = '\0';

			/* Replace '.' to '_' (e.g.: www.google.com__calendar). It's to not
			 * just have 'www' as class (we will drop the rest just here after)
			 */
			for (int i = 0; cClass[i] != '\0'; i++)
			{
				if (cClass[i] == '.')
					cClass[i] = '_';
			}
			cd_debug ("  chromium application detected, changing the class '%s' to '%s'", res_class, cClass);
		}
		else if (*res_class == '/' && (g_str_has_suffix (res_class, ".exe") || g_str_has_suffix (res_name, ".EXE")))  // case of Mono applications like tomboy ...
		{
			gchar *str = strrchr (res_class, '/');
			if (str)
				str ++;
			else
				str = res_class;
			cClass = g_ascii_strdown (str, -1);
			cClass[strlen (cClass) - 4] = '\0';
		}
		else
		{
			cClass = g_ascii_strdown (res_class, -1);  // down case because some apps change the case depending of their windows...
		}

		cairo_dock_remove_version_from_string (cClass);  // we remore number of version (e.g. Openoffice.org-3.1)

		gchar *str = strchr (cClass, '.');  // we remove all .xxx otherwise we can't detect the lack of extension when looking for an icon (openoffice.org) or it's a problem when looking for an icon (jbrout.py).
		if (str != NULL)
			*str = '\0';
		cd_debug ("got an application with class '%s'", cClass);
	}
	return cClass;
}


  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, G_GNUC_UNUSED gpointer attr)
{
	GldiWindowActor *actor = (GldiWindowActor*)obj;
	s_pWindowsList = g_list_prepend (s_pWindowsList, actor);
}

static void reset_object (GldiObject *obj)
{
	GldiWindowActor *actor = (GldiWindowActor*)obj;
	g_free (actor->cName);
	g_free (actor->cClass);
	g_free (actor->cWmClass);
	g_free (actor->cLastAttentionDemand);
	s_pWindowsList = g_list_remove (s_pWindowsList, actor);
}

void gldi_register_windows_manager (void)
{
	// Object Manager
	memset (&myWindowObjectMgr, 0, sizeof (GldiObjectManager));
	myWindowObjectMgr.cName          = "WindowActor";
	myWindowObjectMgr.iObjectSize    = sizeof (GldiWindowActor);
	// interface
	myWindowObjectMgr.init_object    = init_object;
	myWindowObjectMgr.reset_object   = reset_object;
	// signals
	gldi_object_install_notifications (&myWindowObjectMgr, NB_NOTIFICATIONS_WINDOWS);
	
	// init
	memset (&s_backend, 0, sizeof (GldiWindowManagerBackend));
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_Z_ORDER_CHANGED,
		(GldiNotificationFunc) on_zorder_changed,
		GLDI_RUN_FIRST, NULL);
}

