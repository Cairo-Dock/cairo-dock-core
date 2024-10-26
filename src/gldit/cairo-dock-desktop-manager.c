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
#include "cairo-dock-gnome-shell-integration.h"
#include "cairo-dock-cinnamon-integration.h"
#include "cairo-dock-wayfire-integration.h"
#include "cairo-dock-wayland-manager.h" // gldi_wayland_release_keyboard (needed on Wayfire)
#define _MANAGER_DEF_
#include "cairo-dock-desktop-manager.h"

// public (manager, config, data)
GldiManager myDesktopMgr;
GldiDesktopGeometry g_desktopGeometry;

// dependancies
extern GldiContainer *g_pPrimaryContainer;

// private
static GldiDesktopBackground *s_pDesktopBg = NULL;  // une fois alloue, le pointeur restera le meme tout le temps.
static GldiDesktopManagerBackend s_backend = {0};
static gchar *s_registered_backends = NULL;

static void _reload_desktop_background (void);


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
		gldi_wayland_release_keyboard (pContainer);
		return s_backend.present_class (cClass);
	}
	return FALSE;
}

gboolean gldi_desktop_present_windows (void)  // scale
{
	if (s_backend.present_windows != NULL)
	{
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

gboolean gldi_desktop_set_nb_desktops (int iNbDesktops, int iNbViewportX, int iNbViewportY)
{
	if (s_backend.set_nb_desktops)
		return s_backend.set_nb_desktops (iNbDesktops, iNbViewportX, iNbViewportY);
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

void gldi_desktop_notify_startup (const gchar *cClass)
{
	if (s_backend.notify_startup)
		s_backend.notify_startup (cClass);
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

  ////////////
 /// INIT ///
////////////

static void init (void)
{
	//\__________________ Init the Window Manager backends.
	cd_init_compiz_backend ();
	cd_init_kwin_backend ();
	cd_init_gnome_shell_backend ();
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
	
	// init
	gldi_object_register_notification (&myDesktopMgr,
		NOTIFICATION_DESKTOP_WALLPAPER_CHANGED,
		(GldiNotificationFunc) on_wallpaper_changed,
		GLDI_RUN_FIRST, NULL);
}

