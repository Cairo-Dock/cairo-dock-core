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
#include "cairo-dock-desklet-manager.h"  // cairo_dock_foreach_desklet
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-draw-opengl.h"  // cairo_dock_create_texture_from_surface
#include "cairo-dock-compiz-integration.h"
#include "cairo-dock-kwin-integration.h"
#include "cairo-dock-cinnamon-integration.h"
#include "cairo-dock-wayfire-integration.h"
#include "cairo-dock-wayland-manager.h" // gldi_wayland_release_keyboard (needed on Wayfire)
#define _MANAGER_DEF_
#include "cairo-dock-desktop-manager.h"

// public (manager, config, data)
GldiManager myDesktopMgr;
GldiDesktopGeometry g_desktopGeometry;

// dependencies
extern GldiContainer *g_pPrimaryContainer;

// private
static GldiDesktopBackground *s_pDesktopBg = NULL;  // une fois alloue, le pointeur restera le meme tout le temps.
static GldiDesktopManagerBackend s_backend = {0};
static gchar *s_registered_backends = NULL;

static void _reload_desktop_background (void);

// track monitors with GDK (originally in Wayland manager, but should be used generally)
// list of monitors -- corresponds to pScreens in g_desktopGeometry from
// cairo-dock-desktop-manager.c, and kept in sync with that
GdkMonitor **s_pMonitors = NULL;
// we keep track of the primary monitor
GdkMonitor *s_pPrimaryMonitor = NULL;


  //////////////////////
 /// desktop access ///
//////////////////////

void gldi_desktop_get_current (int *iCurrentDesktop, int *iCurrentViewportX, int *iCurrentViewportY)
{
	*iCurrentDesktop = g_desktopGeometry.iCurrentDesktop;
	*iCurrentViewportX = g_desktopGeometry.iCurrentViewportX;
	*iCurrentViewportY = g_desktopGeometry.iCurrentViewportY;
}


  //////////////////////////////
 /// DESKTOP MANAGER BACKEND ///
//////////////////////////////

static gboolean _set_desklets_on_widget_layer (CairoDesklet *pDesklet, G_GNUC_UNUSED gpointer data)
{
	if (pDesklet->iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER)
		gldi_desktop_set_on_widget_layer (CAIRO_CONTAINER (pDesklet), TRUE);
	return FALSE;  // continue
}
void gldi_desktop_manager_register_backend (GldiDesktopManagerBackend *pBackend, const gchar *name)
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
	
	if (!s_registered_backends) s_registered_backends = g_strdup (name);
	else
	{
		gchar *tmp = s_registered_backends;
		s_registered_backends = g_strdup_printf ("%s; %s", tmp, name);
		g_free (tmp);
	}
	
	// since we have a backend, set up the desklets that are supposed to be on the widget layer.
	if (s_backend.set_on_widget_layer != NULL)
	{
		gldi_desklets_foreach ((GldiDeskletForeachFunc) _set_desklets_on_widget_layer, NULL);
	}
}

const gchar *gldi_desktop_manager_get_backend_names (void)
{
	return s_registered_backends ? s_registered_backends : "none";
}

gboolean gldi_desktop_present_class (const gchar *cClass, GldiContainer *pContainer)  // scale matching class
{
	g_return_val_if_fail (cClass != NULL, FALSE);
	if (s_backend.present_class != NULL)
	{
		gldi_wayland_release_keyboard (pContainer, GLDI_KEYBOARD_RELEASE_PRESENT_WINDOWS);
		return s_backend.present_class (cClass);
	}
	return FALSE;
}

gboolean gldi_desktop_present_windows (GldiContainer *pContainer)  // scale
{
	if (s_backend.present_windows != NULL)
	{
		gldi_wayland_release_keyboard (pContainer, GLDI_KEYBOARD_RELEASE_PRESENT_WINDOWS);
		return s_backend.present_windows ();
	}
	return FALSE;
}

gboolean gldi_desktop_present_desktops (void)  // expose
{
	if (s_backend.present_desktops != NULL)
	{
		return s_backend.present_desktops ();
	}
	return FALSE;
}

gboolean gldi_desktop_show_widget_layer (void)  // widget
{
	if (s_backend.show_widget_layer != NULL)
	{
		return s_backend.show_widget_layer ();
	}
	return FALSE;
}

gboolean gldi_desktop_set_on_widget_layer (GldiContainer *pContainer, gboolean bOnWidgetLayer)
{
	if (s_backend.set_on_widget_layer != NULL)
	{
		return s_backend.set_on_widget_layer (pContainer, bOnWidgetLayer);
	}
	return FALSE;
}

gboolean gldi_desktop_can_present_class (void)
{
	return (s_backend.present_class != NULL);
}

gboolean gldi_desktop_can_present_windows (void)
{
	return (s_backend.present_windows != NULL);
}

gboolean gldi_desktop_can_present_desktops (void)
{
	return (s_backend.present_desktops != NULL);
}

gboolean gldi_desktop_can_show_widget_layer (void)
{
	return (s_backend.show_widget_layer != NULL);
}

gboolean gldi_desktop_can_set_on_widget_layer (void)
{
	return (s_backend.set_on_widget_layer != NULL);
}


gboolean gldi_desktop_show_hide (gboolean bShow)
{
	if (s_backend.show_hide_desktop)
	{
		s_backend.show_hide_desktop (bShow);
		return TRUE;
	}
	return FALSE;
}
gboolean gldi_desktop_is_visible (void)
{
	if (s_backend.desktop_is_visible)
		return s_backend.desktop_is_visible ();
	return FALSE;  // default state = not visible
}
gchar** gldi_desktop_get_names (void)
{
	if (s_backend.get_desktops_names)
		return s_backend.get_desktops_names ();
	return NULL;
}
gboolean gldi_desktop_set_names (gchar **cNames)
{
	if (s_backend.set_desktops_names)
		return s_backend.set_desktops_names (cNames);
	return FALSE;
}

static cairo_surface_t *_get_desktop_bg_surface (void)
{
	if (s_backend.get_desktop_bg_surface)
		return s_backend.get_desktop_bg_surface ();
	return NULL;
}

gboolean gldi_desktop_set_current (int iDesktopNumber, int iViewportNumberX, int iViewportNumberY)
{
	if (s_backend.set_current_desktop)
		return s_backend.set_current_desktop (iDesktopNumber, iViewportNumberX, iViewportNumberY);
	return FALSE;
}

void gldi_desktop_add_workspace (void)
{
	if (s_backend.add_workspace) s_backend.add_workspace ();
}

void gldi_desktop_remove_last_workspace (void)
{
	if (s_backend.remove_last_workspace) s_backend.remove_last_workspace ();
}



void gldi_desktop_refresh (void)
{
	if (s_backend.refresh)
		s_backend.refresh ();
}

gboolean gldi_desktop_grab_shortkey (guint keycode, guint modifiers, gboolean grab)
{
	if (s_backend.grab_shortkey)
		return s_backend.grab_shortkey (keycode, modifiers, grab);
	return FALSE;
}

  //////////////////
 /// DESKTOP BG ///
//////////////////

GldiDesktopBackground *gldi_desktop_background_get (gboolean bWithTextureToo)
{
	//g_print ("%s (%d, %d)\n", __func__, bWithTextureToo, s_pDesktopBg?s_pDesktopBg->iRefCount:-1);
	if (s_pDesktopBg == NULL)
	{
		s_pDesktopBg = g_new0 (GldiDesktopBackground, 1);
	}
	if (s_pDesktopBg->pSurface == NULL)
	{
		s_pDesktopBg->pSurface = _get_desktop_bg_surface ();
	}
	if (s_pDesktopBg->iTexture == 0 && bWithTextureToo)
	{
		s_pDesktopBg->iTexture = cairo_dock_create_texture_from_surface (s_pDesktopBg->pSurface);
	}
	
	s_pDesktopBg->iRefCount ++;
	if (s_pDesktopBg->iSidDestroyBg != 0)
	{
		//g_print ("cancel pending destroy\n");
		g_source_remove (s_pDesktopBg->iSidDestroyBg);
		s_pDesktopBg->iSidDestroyBg = 0;
	}
	return s_pDesktopBg;
}

static gboolean _destroy_bg (GldiDesktopBackground *pDesktopBg)
{
	//g_print ("%s ()\n", __func__);
	g_return_val_if_fail (pDesktopBg != NULL, 0);
	if (pDesktopBg->pSurface != NULL)
	{
		cairo_surface_destroy (pDesktopBg->pSurface);
		pDesktopBg->pSurface = NULL;
		//g_print ("--- surface destroyed\n");
	}
	if (pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (pDesktopBg->iTexture);
		pDesktopBg->iTexture = 0;
	}
	pDesktopBg->iSidDestroyBg = 0;
	return FALSE;
}
void gldi_desktop_background_destroy (GldiDesktopBackground *pDesktopBg)
{
	//g_print ("%s ()\n", __func__);
	if (!pDesktopBg)
		return;
	if (pDesktopBg->iRefCount > 0)
		pDesktopBg->iRefCount --;
	if (pDesktopBg->iRefCount == 0 && pDesktopBg->iSidDestroyBg == 0)
	{
		//g_print ("add pending destroy\n");
		pDesktopBg->iSidDestroyBg = g_timeout_add_seconds (3, (GSourceFunc)_destroy_bg, pDesktopBg);
	}
}

cairo_surface_t *gldi_desktop_background_get_surface (GldiDesktopBackground *pDesktopBg)
{
	g_return_val_if_fail (pDesktopBg != NULL, NULL);
	return pDesktopBg->pSurface;
}

GLuint gldi_desktop_background_get_texture (GldiDesktopBackground *pDesktopBg)
{
	g_return_val_if_fail (pDesktopBg != NULL, 0);
	return pDesktopBg->iTexture;
}

static void _reload_desktop_background (void)
{
	//g_print ("%s ()\n", __func__);
	if (s_pDesktopBg == NULL)  // rien a recharger.
		return ;
	if (s_pDesktopBg->pSurface == NULL && s_pDesktopBg->iTexture == 0)  // rien a recharger.
		return ;
	
	if (s_pDesktopBg->pSurface != NULL)
	{
		cairo_surface_destroy (s_pDesktopBg->pSurface);
		//g_print ("--- surface destroyed\n");
	}
	s_pDesktopBg->pSurface = _get_desktop_bg_surface ();
	
	if (s_pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (s_pDesktopBg->iTexture);
		s_pDesktopBg->iTexture = cairo_dock_create_texture_from_surface (s_pDesktopBg->pSurface);
	}
}


  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	/*if (s_pDesktopBg != NULL && s_pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (s_pDesktopBg->iTexture);
		s_pDesktopBg->iTexture = 0;
	}*/
	if (s_pDesktopBg)  // on decharge le desktop-bg de force.
	{
		if (s_pDesktopBg->iSidDestroyBg != 0)
		{
			g_source_remove (s_pDesktopBg->iSidDestroyBg);
			s_pDesktopBg->iSidDestroyBg = 0;
		}
		s_pDesktopBg->iRefCount = 0;
		_destroy_bg (s_pDesktopBg);  // detruit ses ressources immediatement, mais pas le pointeur.
	}
}


static gboolean on_wallpaper_changed (G_GNUC_UNUSED gpointer data)
{
	_reload_desktop_background ();
	return GLDI_NOTIFICATION_LET_PASS;
}


  ////////////////////////
 /// DESKTOP GEOMETRY ///
////////////////////////


// refresh s_desktopGeometry.xscreen
static void _calculate_xscreen ()
{
	int i;
	g_desktopGeometry.Xscreen.x = 0;
	g_desktopGeometry.Xscreen.y = 0;
	g_desktopGeometry.Xscreen.width = 0;
	g_desktopGeometry.Xscreen.height = 0;
	
	if (g_desktopGeometry.iNbScreens >= 0)
		for (i = 0; i < g_desktopGeometry.iNbScreens; i++)
		{
			int tmpwidth = g_desktopGeometry.pScreens[i].x + g_desktopGeometry.pScreens[i].width;
			int tmpheight = g_desktopGeometry.pScreens[i].y + g_desktopGeometry.pScreens[i].height;
			if (tmpwidth > g_desktopGeometry.Xscreen.width)
				g_desktopGeometry.Xscreen.width = tmpwidth;
			if (tmpheight > g_desktopGeometry.Xscreen.height)
				g_desktopGeometry.Xscreen.height = tmpheight;
		}
}

// handle changes in screen / monitor configuration -- note: user_data is a boolean that determines if we should emit a signal
static void _monitor_added (GdkDisplay *display, GdkMonitor* monitor, gpointer user_data)
{
	int iNumScreen = 0;
	if (!(g_desktopGeometry.pScreens && s_pMonitors && g_desktopGeometry.iNbScreens))
	{
		if (g_desktopGeometry.iNbScreens || g_desktopGeometry.pScreens || s_pMonitors)
			cd_warning ("_monitor_added() inconsistent state of screens / monitors\n");
		g_desktopGeometry.iNbScreens = 1;
		g_free (s_pMonitors);
		g_free (g_desktopGeometry.pScreens);
		s_pMonitors = g_new0 (GdkMonitor*, g_desktopGeometry.iNbScreens);
		g_desktopGeometry.pScreens = g_new0 (GtkAllocation, g_desktopGeometry.iNbScreens);
	}
	else
	{
		iNumScreen = g_desktopGeometry.iNbScreens++;
		s_pMonitors = g_renew (GdkMonitor*, s_pMonitors, g_desktopGeometry.iNbScreens);
		g_desktopGeometry.pScreens = g_renew (GtkAllocation, g_desktopGeometry.pScreens, g_desktopGeometry.iNbScreens);
	}
	s_pMonitors[iNumScreen] = monitor;
	// note: GtkAllocation is the same as GdkRectangle
	gdk_monitor_get_geometry (monitor, g_desktopGeometry.pScreens + iNumScreen);
	if (monitor == gdk_display_get_primary_monitor (display))
		s_pPrimaryMonitor = monitor;
	_calculate_xscreen ();
	
	cd_debug ("iNbScreens: %d, Xscreen: %dx%d", g_desktopGeometry.iNbScreens,
		g_desktopGeometry.Xscreen.width, g_desktopGeometry.Xscreen.height);
	
	if (user_data) gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, TRUE);
	
	// we always send notification on the desktop manager object
	gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_MONITOR_ADDED, monitor);
}

// handle if a monitor was removed
static void _monitor_removed (GdkDisplay* display, GdkMonitor* monitor, G_GNUC_UNUSED gpointer user_data)
{
	if (!(g_desktopGeometry.pScreens && s_pMonitors && g_desktopGeometry.iNbScreens > 0))
		cd_warning ("_monitor_removed() inconsistent state of screens / monitors\n");
	else
	{
		int i;
		for (i = 0; i < g_desktopGeometry.iNbScreens; i++)
			if (s_pMonitors[i] == monitor) break;
		if (i == g_desktopGeometry.iNbScreens)
		{
			cd_warning ("_monitor_removed() inconsistent state of screens / monitors\n");
			return;
		}
		for (; i +1 < g_desktopGeometry.iNbScreens; i++)
		{
			s_pMonitors[i] = s_pMonitors[i+1];
			g_desktopGeometry.pScreens[i] = g_desktopGeometry.pScreens[i+1];
		}
		s_pPrimaryMonitor = gdk_display_get_primary_monitor (display);
		if (!s_pPrimaryMonitor) s_pPrimaryMonitor = s_pMonitors[0];
		g_desktopGeometry.iNbScreens--;
		s_pMonitors = g_renew (GdkMonitor*, s_pMonitors, g_desktopGeometry.iNbScreens);
		g_desktopGeometry.pScreens = g_renew (GtkAllocation, g_desktopGeometry.pScreens, g_desktopGeometry.iNbScreens);
		_calculate_xscreen ();
		
		cd_debug ("iNbScreens: %d, Xscreen: %dx%d", g_desktopGeometry.iNbScreens,
		g_desktopGeometry.Xscreen.width, g_desktopGeometry.Xscreen.height);
		
		gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, TRUE);
		
		// we always send notification on the desktop manager object
		gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_MONITOR_REMOVED, monitor);
	}
}

// refresh the size of all monitors -- note: user_data is a boolean that determines if we should emit a signal
static void _refresh_monitors_size(G_GNUC_UNUSED GdkScreen *screen, gpointer user_data)
{
	int i;
	for (i = 0; i < g_desktopGeometry.iNbScreens; i++)
		gdk_monitor_get_geometry (s_pMonitors[i], g_desktopGeometry.pScreens + i);
	_calculate_xscreen ();
	
	cd_debug ("iNbScreens: %d, Xscreen: %dx%d", g_desktopGeometry.iNbScreens,
		g_desktopGeometry.Xscreen.width, g_desktopGeometry.Xscreen.height);
	
	if (user_data) gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, TRUE);
}

// refresh all monitors (if needed) -- note: user_data is a boolean that determines if we should emit a signal
static void _refresh_monitors (GdkScreen *screen, gpointer user_data)
{
	GdkDisplay *display = gdk_screen_get_display (screen);
	int iNumScreen = gdk_display_get_n_monitors (display);
	if (iNumScreen != g_desktopGeometry.iNbScreens)
	{
		// we don't try to guess what has changed, just refresh all monitors
		GdkMonitor **tmp = s_pMonitors;
		s_pMonitors = NULL;
		int iNumOld = g_desktopGeometry.iNbScreens;
		g_free (g_desktopGeometry.pScreens);
		g_desktopGeometry.iNbScreens = iNumScreen;
		s_pMonitors = g_renew (GdkMonitor*, s_pMonitors, g_desktopGeometry.iNbScreens);
		g_desktopGeometry.pScreens = g_renew (GtkAllocation, g_desktopGeometry.pScreens, g_desktopGeometry.iNbScreens);
		int i;
		for (i = 0; i < iNumScreen; i++)
		{
			s_pMonitors[i] = gdk_display_get_monitor (display, i);
			gdk_monitor_get_geometry (s_pMonitors[i], g_desktopGeometry.pScreens + i);
		}
		s_pPrimaryMonitor = gdk_display_get_primary_monitor (display);
		if (!s_pPrimaryMonitor) s_pPrimaryMonitor = s_pMonitors[0];
		_calculate_xscreen ();
		cd_debug ("iNbScreens: %d, Xscreen: %dx%d", g_desktopGeometry.iNbScreens,
			g_desktopGeometry.Xscreen.width, g_desktopGeometry.Xscreen.height);
		
		if (user_data) gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_GEOMETRY_CHANGED, TRUE);
		
		// figure out if any monitors were added / removed and emit the Wayland-specific notifications
		for (i = 0; i < iNumScreen; i++)
		{
			int j;
			GdkMonitor *mon = s_pMonitors[i];
			for (j = 0; j < iNumOld; j++) if (tmp[j] == mon) break;
			if (j < iNumOld)
			{
				// this monitor was present before
				if (j + 1 < iNumOld) tmp[j] = tmp[iNumOld - 1];
				iNumOld--;
			}
			else gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_MONITOR_ADDED, mon);
		}
		// send notifications for monitors
		for (i = 0; i < iNumOld; i++) gldi_object_notify (&myDesktopMgr, NOTIFICATION_DESKTOP_MONITOR_REMOVED, tmp[i]);
		
		g_free(tmp);
	}
	else _refresh_monitors_size (screen, user_data);
}

GdkMonitor *const *gldi_desktop_get_monitors (int *iNumMonitors)
{
	*iNumMonitors = g_desktopGeometry.iNbScreens;
	return s_pMonitors;
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	//\__________________ Init the Window Manager backends.
	cd_init_compiz_backend ();
	cd_init_kwin_backend ();
	cd_init_cinnamon_backend ();
	cd_init_wayfire_backend ();
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_desktop_manager (void)
{
	// Manager
	memset (&myDesktopMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myDesktopMgr), &myManagerObjectMgr, NULL);
	myDesktopMgr.cModuleName  = "Desktop";
	// interface
	myDesktopMgr.init         = init;
	myDesktopMgr.load         = NULL;
	myDesktopMgr.unload       = unload;
	myDesktopMgr.reload       = (GldiManagerReloadFunc)NULL;
	myDesktopMgr.get_config   = (GldiManagerGetConfigFunc)NULL;
	myDesktopMgr.reset_config = (GldiManagerResetConfigFunc)NULL;
	// Config
	myDesktopMgr.pConfig = (GldiManagerConfigPtr)NULL;
	myDesktopMgr.iSizeOfConfig = 0;
	// data
	myDesktopMgr.iSizeOfData = 0;
	myDesktopMgr.pData = (GldiManagerDataPtr)NULL;
	memset (&s_backend, 0, sizeof (GldiDesktopManagerBackend));
	// signals
	gldi_object_install_notifications (&myDesktopMgr, NB_NOTIFICATIONS_DESKTOP);  // we don't have a Desktop Object, so let's put the signals here
	
	// get the properties of screens / monitors, set up signals
	g_desktopGeometry.iNbScreens = 0;
	g_desktopGeometry.pScreens = NULL;
	GdkDisplay *dsp = gdk_display_get_default ();
	GdkScreen *screen = gdk_display_get_default_screen (dsp);
	// fill out the list of screens / monitors
	_refresh_monitors (screen, (gpointer)FALSE);
	// set up notifications for screens added / removed
	g_signal_connect (G_OBJECT (screen), "monitors-changed", G_CALLBACK (_refresh_monitors), (gpointer)TRUE);
	g_signal_connect (G_OBJECT (screen), "size-changed", G_CALLBACK (_refresh_monitors_size), (gpointer)TRUE);
	g_signal_connect (G_OBJECT (dsp), "monitor-added", G_CALLBACK (_monitor_added), (gpointer)TRUE);
	g_signal_connect (G_OBJECT (dsp), "monitor-removed", G_CALLBACK (_monitor_removed), (gpointer)TRUE);
	
	// init
	gldi_object_register_notification (&myDesktopMgr,
		NOTIFICATION_DESKTOP_WALLPAPER_CHANGED,
		(GldiNotificationFunc) on_wallpaper_changed,
		GLDI_RUN_FIRST, NULL);
}

