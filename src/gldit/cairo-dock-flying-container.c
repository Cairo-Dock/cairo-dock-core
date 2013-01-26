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

#include <stdlib.h>
#include <sys/time.h>

#include "gldi-config.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-factory.h"
#define _MANAGER_DEF_
#include "cairo-dock-flying-container.h"

// 1/4 + 1/4
//     + 1
/**
 ______
|  ____|
| |    |
| |    |
|_|____|

*/

CairoFlyingManager myFlyingsMgr;

extern CairoContainer *g_pPrimaryContainer;
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;

static CairoDockImageBuffer *s_pExplosion = NULL;
static CairoDockImageBuffer *s_pEmblem = NULL;

static void _cairo_dock_load_emblem (Icon *pIcon)
{
	const gchar *cImage = NULL;
	if (CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
	{
		cImage = GTK_STOCK_JUMP_TO"-rtl";  // GTK_STOCK_JUMP_TO only doesn't exist.
	}
	else
	{
		cImage = GTK_STOCK_DELETE;
	}
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
	gchar *cIcon = cairo_dock_search_icon_s_path (cImage, MAX (iWidth/2, iHeight/2));
	cairo_dock_free_image_buffer (s_pEmblem);
	s_pEmblem = cairo_dock_create_image_buffer (cIcon, iWidth/2, iHeight/2, 0);
	g_free (cIcon);
}
static void _cairo_dock_load_explosion_image (int iWidth)
{
	cairo_dock_free_image_buffer (s_pExplosion);
	gchar *cExplosionFile = cairo_dock_search_image_s_path ("explosion.png");
	s_pExplosion = cairo_dock_create_image_buffer (cExplosionFile?cExplosionFile:GLDI_SHARE_DATA_DIR"/explosion/explosion.png", iWidth, iWidth, CAIRO_DOCK_FILL_SPACE | CAIRO_DOCK_ANIMATED_IMAGE);
	cairo_dock_image_buffer_set_timelength (s_pExplosion, .4);
	g_free (cExplosionFile);
}


static gboolean _cairo_dock_update_flying_container_notification (G_GNUC_UNUSED gpointer pUserData, CairoFlyingContainer *pFlyingContainer, gboolean *bContinueAnimation)
{
	if (! cairo_dock_image_buffer_is_animated (s_pExplosion))	
	{
		*bContinueAnimation = FALSE;  // cancel any other update
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;  // and intercept the notification
	}
	gboolean bLastFrame = cairo_dock_image_buffer_next_frame_no_loop (s_pExplosion);
	if (bLastFrame)  // last frame reached -> stop here
	{
		*bContinueAnimation = FALSE;  // cancel any other update
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;  // and intercept the notification
	}
	gtk_widget_queue_draw (pFlyingContainer->container.pWidget);
	
	*bContinueAnimation = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static gboolean _cairo_dock_render_flying_container_notification (G_GNUC_UNUSED gpointer pUserData, CairoFlyingContainer *pFlyingContainer, cairo_t *pCairoContext)
{
	Icon *pIcon = pFlyingContainer->pIcon;
	if (pCairoContext != NULL)
	{
		if (pIcon != NULL)
		{
			cairo_save (pCairoContext);
			
			cairo_translate (pCairoContext, pIcon->fDrawX, pIcon->fDrawY);
			if (pIcon->image.pSurface != NULL)  // we can't use cairo_dock_render_one_icon() here since it's not a dock, and anyway we don't need it.
			{
				cairo_save (pCairoContext);
				
				cairo_dock_set_icon_scale_on_context (pCairoContext, pIcon, pFlyingContainer->container.bIsHorizontal, pFlyingContainer->container.fRatio, pFlyingContainer->container.bDirectionUp);
				cairo_set_source_surface (pCairoContext, pIcon->image.pSurface, 0.0, 0.0);
				cairo_paint (pCairoContext);
				
				cairo_restore (pCairoContext);
			}
			
			cairo_restore (pCairoContext);
			
			if (s_pEmblem)
			{
				cairo_dock_apply_image_buffer_surface (s_pEmblem, pCairoContext);
			}
		}
		else
		{
			cairo_dock_apply_image_buffer_surface_with_offset (s_pExplosion, pCairoContext,
				(pFlyingContainer->container.iWidth - s_pExplosion->iWidth / s_pExplosion->iNbFrames) / 2,
				(pFlyingContainer->container.iHeight - s_pExplosion->iHeight) / 2,
				1.);
		}
	}
	else
	{
		glClear (GL_COLOR_BUFFER_BIT);
		_cairo_dock_set_blend_source ();
		_cairo_dock_set_alpha (1.);
		
		gldi_glx_apply_desktop_background (CAIRO_CONTAINER (pFlyingContainer));
		glLoadIdentity ();
		
		if (pIcon != NULL)
		{
			glPushMatrix ();

			cairo_dock_translate_on_icon_opengl (pIcon, CAIRO_CONTAINER (pFlyingContainer), 1.);
			cairo_dock_draw_icon_texture (pIcon, CAIRO_CONTAINER (pFlyingContainer));

			glPopMatrix ();
			
			_cairo_dock_enable_texture ();
			_cairo_dock_set_blend_alpha ();
			
			if (s_pEmblem && s_pEmblem->iTexture != 0)
			{
				cairo_dock_apply_image_buffer_texture_with_offset (s_pEmblem, s_pEmblem->iWidth/2, pFlyingContainer->container.iHeight - s_pEmblem->iHeight/2);
			}
			
			_cairo_dock_disable_texture ();
		}
		else
		{
			_cairo_dock_enable_texture ();
			cairo_dock_apply_image_buffer_texture_with_offset (s_pExplosion,
				pFlyingContainer->container.iWidth/2,
				pFlyingContainer->container.iHeight/2);
			_cairo_dock_disable_texture ();
		}
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


static gboolean on_expose_flying_icon (G_GNUC_UNUSED GtkWidget *pWidget,
#if (GTK_MAJOR_VERSION < 3)
	G_GNUC_UNUSED GdkEventExpose *pExpose,
#else
	G_GNUC_UNUSED cairo_t *ctx,
#endif
	CairoFlyingContainer *pFlyingContainer)
{
	if (g_bUseOpenGL)
	{
		if (! gldi_glx_begin_draw_container (CAIRO_CONTAINER (pFlyingContainer)))
			return FALSE;
		
		cairo_dock_notify_on_object (pFlyingContainer, NOTIFICATION_RENDER, pFlyingContainer, NULL);
		
		gldi_glx_end_draw_container (CAIRO_CONTAINER (pFlyingContainer));
	}
 	else
	{
		cairo_t *pCairoContext = cairo_dock_create_drawing_context_on_container (CAIRO_CONTAINER (pFlyingContainer));
		
		cairo_dock_notify_on_object (pFlyingContainer, NOTIFICATION_RENDER, pFlyingContainer, pCairoContext);
		
		cairo_destroy (pCairoContext);
	}
	
	return FALSE;
}

static gboolean on_configure_flying_icon (GtkWidget* pWidget,
	GdkEventConfigure* pEvent,
	CairoFlyingContainer *pFlyingContainer)
{
	//g_print ("%s (%dx%d / %dx%d)\n", __func__, pFlyingContainer->container.iWidth, pFlyingContainer->container.iHeight, pEvent->width, pEvent->height);
	if (pFlyingContainer->container.iWidth != pEvent->width || pFlyingContainer->container.iHeight != pEvent->height)
	{
		pFlyingContainer->container.iWidth = pEvent->width;
		pFlyingContainer->container.iHeight = pEvent->height;
		
		if (g_bUseOpenGL)
		{
			if (! gldi_glx_make_current (CAIRO_CONTAINER (pFlyingContainer)))
				return FALSE;
			
			cairo_dock_set_ortho_view (CAIRO_CONTAINER (pFlyingContainer));
		}
	}
	
	gtk_widget_queue_draw (pWidget);
	return FALSE;
}

static gboolean _cairo_flying_container_animation_loop (CairoContainer *pContainer)
{
	CairoFlyingContainer *pFlyingContainer = CAIRO_FLYING_CONTAINER (pContainer);
	gboolean bContinue = FALSE;
	
	if (pFlyingContainer->pIcon != NULL)
	{
		gboolean bIconIsAnimating = FALSE;
		
		cairo_dock_notify_on_object (pFlyingContainer->pIcon, NOTIFICATION_UPDATE_ICON, pFlyingContainer->pIcon, pFlyingContainer, &bIconIsAnimating);
		if (! bIconIsAnimating)
			pFlyingContainer->pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
		else
			bContinue = TRUE;
	}
	
	cairo_dock_notify_on_object (pFlyingContainer, NOTIFICATION_UPDATE, pFlyingContainer, &bContinue);
	
	if (! bContinue)
	{
		cairo_dock_free_flying_container (pFlyingContainer);
		return FALSE;
	}
	else
		return TRUE;
}

CairoFlyingContainer *cairo_dock_create_flying_container (Icon *pFlyingIcon, CairoDock *pOriginDock)
{
	g_return_val_if_fail (pFlyingIcon != NULL, NULL);
	// create a flying-container
	CairoFlyingContainer * pFlyingContainer = gldi_container_new (CairoFlyingContainer, &myFlyingsMgr, CAIRO_DOCK_TYPE_FLYING_CONTAINER);
	
	pFlyingContainer->container.bIsHorizontal = TRUE;
	pFlyingContainer->container.bDirectionUp = TRUE;
	pFlyingContainer->container.fRatio = 1.;
	pFlyingContainer->container.bUseReflect = FALSE;
	pFlyingContainer->container.iface.animation_loop = _cairo_flying_container_animation_loop;
	
	// insert the icon inside
	pFlyingContainer->pIcon = pFlyingIcon;
	cairo_dock_set_icon_container (pFlyingIcon, pFlyingContainer);
	pFlyingIcon->bPointed = TRUE;
	pFlyingIcon->fAlpha = 1.;
	
	// set up the window
	GtkWidget *pWindow = pFlyingContainer->container.pWidget;
	gtk_window_set_keep_above (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_title (GTK_WINDOW(pWindow), "cairo-dock-flying-icon");
	
	g_signal_connect (G_OBJECT (pWindow),
		#if (GTK_MAJOR_VERSION < 3)
		"expose-event",
		#else
		"draw",
		#endif
		G_CALLBACK (on_expose_flying_icon),
		pFlyingContainer);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (on_configure_flying_icon),
		pFlyingContainer);
	
	pFlyingContainer->container.bInside = TRUE;
	
	int iWidth = pFlyingIcon->fWidth * pFlyingIcon->fScale * 1.2;  // enlarge a little, so that the emblem is half on the icon.
	int iHeight = pFlyingIcon->fHeight * pFlyingIcon->fScale * 1.2;
	pFlyingIcon->fDrawX = iWidth - pFlyingIcon->fWidth * pFlyingIcon->fScale;
	pFlyingIcon->fDrawY = iHeight - pFlyingIcon->fHeight * pFlyingIcon->fScale;
	
	if (pOriginDock->container.bIsHorizontal)
	{
		pFlyingContainer->container.iWindowPositionX = pOriginDock->container.iWindowPositionX + pOriginDock->container.iMouseX - iWidth/2;
		pFlyingContainer->container.iWindowPositionY = pOriginDock->container.iWindowPositionY + pOriginDock->container.iMouseY - iHeight/2;
	}
	else
	{
		pFlyingContainer->container.iWindowPositionY = pOriginDock->container.iWindowPositionX + pOriginDock->container.iMouseX - iWidth/2;
		pFlyingContainer->container.iWindowPositionX = pOriginDock->container.iWindowPositionY + pOriginDock->container.iMouseY - iHeight/2;
	}
	gtk_window_present (GTK_WINDOW (pWindow));
	/*cd_debug ("%s (%d;%d %dx%d)", __func__ pFlyingContainer->container.iWindowPositionX,
		pFlyingContainer->container.iWindowPositionY,
		pFlyingContainer->container.iWidth,
		pFlyingContainer->container.iHeight);*/
	gdk_window_move_resize (gldi_container_get_gdk_window (CAIRO_CONTAINER (pFlyingContainer)),
		pFlyingContainer->container.iWindowPositionX,
		pFlyingContainer->container.iWindowPositionY,
		iWidth,
		iHeight);
	
	// load the images
	_cairo_dock_load_emblem (pFlyingIcon);
	_cairo_dock_load_explosion_image (iWidth);
	
	struct timeval tv;
	int r = gettimeofday (&tv, NULL);
	if (r == 0)
		pFlyingContainer->fCreationTime = tv.tv_sec + tv.tv_usec * 1e-6;
	
	return pFlyingContainer;
}

void cairo_dock_drag_flying_container (CairoFlyingContainer *pFlyingContainer, CairoDock *pOriginDock)
{
	if (pOriginDock->container.bIsHorizontal)
	{
		pFlyingContainer->container.iWindowPositionX = pOriginDock->container.iWindowPositionX + pOriginDock->container.iMouseX - pFlyingContainer->container.iWidth/2;
		pFlyingContainer->container.iWindowPositionY = pOriginDock->container.iWindowPositionY + pOriginDock->container.iMouseY - pFlyingContainer->container.iHeight/2;
	}
	else
	{
		pFlyingContainer->container.iWindowPositionY = pOriginDock->container.iWindowPositionX + pOriginDock->container.iMouseX - pFlyingContainer->container.iWidth/2;
		pFlyingContainer->container.iWindowPositionX = pOriginDock->container.iWindowPositionY + pOriginDock->container.iMouseY - pFlyingContainer->container.iHeight/2;
	}
	//g_print ("  on tire l'icone volante en (%d;%d)\n", pFlyingContainer->container.iWindowPositionX, pFlyingContainer->container.iWindowPositionY);
	gtk_window_move (GTK_WINDOW (pFlyingContainer->container.pWidget),
		pFlyingContainer->container.iWindowPositionX,
		pFlyingContainer->container.iWindowPositionY);
}

void cairo_dock_free_flying_container (CairoFlyingContainer *pFlyingContainer)
{
	cd_debug ("%s ()", __func__);
	// detach the icon
	if (pFlyingContainer->pIcon != NULL)
		cairo_dock_set_icon_container (pFlyingContainer->pIcon, NULL);
	// free the container
	cairo_dock_finish_container (CAIRO_CONTAINER (pFlyingContainer));
	g_free (pFlyingContainer);
	cairo_dock_free_image_buffer (s_pEmblem);
	s_pEmblem = NULL;
}

void cairo_dock_terminate_flying_container (CairoFlyingContainer *pFlyingContainer)
{
	// detach the icon from the container
	Icon *pIcon = pFlyingContainer->pIcon;
	pFlyingContainer->pIcon = NULL;
	cairo_dock_set_icon_container (pIcon, NULL);
	
	// destroy it, or place it in a desklet.
	if (pIcon->cDesktopFileName != NULL)  // a launcher/sub-dock/separator, that is part of the theme
	{
		cairo_dock_delete_icon_from_current_theme (pIcon);
		cairo_dock_free_icon (pIcon);
	}
	else if (CAIRO_DOCK_IS_APPLET(pIcon))  /// faire une fonction dans la factory ...
	{
		cd_debug ("le module %s devient un desklet", pIcon->pModuleInstance->cConfFilePath);
		cairo_dock_stop_icon_animation (pIcon);
		
		cairo_dock_detach_module_instance_at_position (pIcon->pModuleInstance,
			pFlyingContainer->container.iWindowPositionX + pFlyingContainer->container.iWidth/2,
			pFlyingContainer->container.iWindowPositionY + pFlyingContainer->container.iHeight/2);
	}
	
	// start the explosion animation
	cairo_dock_launch_animation (CAIRO_CONTAINER (pFlyingContainer));
}


  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	if (s_pExplosion != NULL)
	{
		cairo_dock_free_image_buffer (s_pExplosion);
		s_pExplosion = NULL;
	}
	if (s_pEmblem != NULL)
	{
		cairo_dock_free_image_buffer (s_pEmblem);
		s_pEmblem = NULL;
	}
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	cairo_dock_register_notification_on_object (&myFlyingsMgr,
		NOTIFICATION_UPDATE,
		(CairoDockNotificationFunc) _cairo_dock_update_flying_container_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myFlyingsMgr,
		NOTIFICATION_RENDER,
		(CairoDockNotificationFunc) _cairo_dock_render_flying_container_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_flying_manager (void)
{
	// Manager
	memset (&myFlyingsMgr, 0, sizeof (CairoFlyingManager));
	myFlyingsMgr.mgr.cModuleName 	= "Flying";
	myFlyingsMgr.mgr.init 			= init;
	myFlyingsMgr.mgr.load 			= NULL;  // data loaded on the 1st creation.
	myFlyingsMgr.mgr.unload 		= unload;
	myFlyingsMgr.mgr.reload 		= (GldiManagerReloadFunc)NULL;
	myFlyingsMgr.mgr.get_config 	= (GldiManagerGetConfigFunc)NULL;
	myFlyingsMgr.mgr.reset_config 	= (GldiManagerResetConfigFunc)NULL;
	// Config
	myFlyingsMgr.mgr.pConfig = (GldiManagerConfigPtr)NULL;
	myFlyingsMgr.mgr.iSizeOfConfig = 0;
	// data
	myFlyingsMgr.mgr.pData = (GldiManagerDataPtr)NULL;
	myFlyingsMgr.mgr.iSizeOfData = 0;
	// signals
	cairo_dock_install_notifications_on_object (&myFlyingsMgr, NB_NOTIFICATIONS_FLYING_CONTAINER);
	gldi_object_set_manager (GLDI_OBJECT (&myFlyingsMgr), GLDI_MANAGER (&myContainersMgr));
	// register
	gldi_register_manager (GLDI_MANAGER(&myFlyingsMgr));
}
