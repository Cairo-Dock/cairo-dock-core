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
#include "cairo-dock-module-instance-manager.h"  // gldi_module_instance_detach_at_position
#include "cairo-dock-applet-manager.h"  // GLDI_OBJECT_IS_APPLET_ICON
#include "cairo-dock-log.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-container.h"
#include "cairo-dock-animations.h"
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

// public (manager, config, data)
GldiManager myFlyingsMgr;
GldiObjectManager myFlyingObjectMgr;

// dependancies
extern GldiContainer *g_pPrimaryContainer;
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;

// private
static CairoDockImageBuffer *s_pExplosion = NULL;
static CairoDockImageBuffer *s_pEmblem = NULL;

static void _load_emblem (Icon *pIcon)
{
	const gchar *cImage = NULL;
	if (GLDI_OBJECT_IS_APPLET_ICON (pIcon))
	{
		/// GTK_STOCK is now deprecated, here is a temporary fix to avoid compilation errors
		#if GTK_CHECK_VERSION(3, 9, 8)
		cImage = "gtk-jump-to-rtl";
		#else
		cImage = GTK_STOCK_JUMP_TO"-rtl";  // GTK_STOCK_JUMP_TO only doesn't exist.
		#endif
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
static void _load_explosion_image (int iWidth)
{
	cairo_dock_free_image_buffer (s_pExplosion);
	gchar *cExplosionFile = cairo_dock_search_image_s_path ("explosion.png");
	s_pExplosion = cairo_dock_create_image_buffer (cExplosionFile?cExplosionFile:GLDI_SHARE_DATA_DIR"/explosion/explosion.png", iWidth, iWidth, CAIRO_DOCK_FILL_SPACE | CAIRO_DOCK_ANIMATED_IMAGE);
	cairo_dock_image_buffer_set_timelength (s_pExplosion, .4);
	g_free (cExplosionFile);
}


static gboolean _on_update_flying_container_notification (G_GNUC_UNUSED gpointer pUserData, CairoFlyingContainer *pFlyingContainer, gboolean *bContinueAnimation)
{
	if (! cairo_dock_image_buffer_is_animated (s_pExplosion))	
	{
		*bContinueAnimation = FALSE;  // cancel any other update
		return GLDI_NOTIFICATION_INTERCEPT;  // and intercept the notification
	}
	gboolean bLastFrame = cairo_dock_image_buffer_next_frame_no_loop (s_pExplosion);
	if (bLastFrame)  // last frame reached -> stop here
	{
		*bContinueAnimation = FALSE;  // cancel any other update
		return GLDI_NOTIFICATION_INTERCEPT;  // and intercept the notification
	}
	gtk_widget_queue_draw (pFlyingContainer->container.pWidget);
	
	*bContinueAnimation = TRUE;
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_render_flying_container_notification (G_GNUC_UNUSED gpointer pUserData, CairoFlyingContainer *pFlyingContainer, cairo_t *pCairoContext)
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
	return GLDI_NOTIFICATION_LET_PASS;
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
		if (! gldi_gl_container_begin_draw (CAIRO_CONTAINER (pFlyingContainer)))
			return FALSE;
		
		gldi_object_notify (pFlyingContainer, NOTIFICATION_RENDER, pFlyingContainer, NULL);
		
		gldi_gl_container_end_draw (CAIRO_CONTAINER (pFlyingContainer));
	}
 	else
	{
		cairo_t *pCairoContext = cairo_dock_create_drawing_context_on_container (CAIRO_CONTAINER (pFlyingContainer));
		
		gldi_object_notify (pFlyingContainer, NOTIFICATION_RENDER, pFlyingContainer, pCairoContext);
		
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
			if (! gldi_gl_container_make_current (CAIRO_CONTAINER (pFlyingContainer)))
				return FALSE;
			
			cairo_dock_set_ortho_view (CAIRO_CONTAINER (pFlyingContainer));
		}
	}
	
	gtk_widget_queue_draw (pWidget);
	return FALSE;
}

static gboolean _animation_loop (GldiContainer *pContainer)
{
	CairoFlyingContainer *pFlyingContainer = CAIRO_FLYING_CONTAINER (pContainer);
	gboolean bContinue = FALSE;
	
	if (pFlyingContainer->pIcon != NULL)
	{
		gboolean bIconIsAnimating = FALSE;
		
		gldi_object_notify (pFlyingContainer->pIcon, NOTIFICATION_UPDATE_ICON, pFlyingContainer->pIcon, pFlyingContainer, &bIconIsAnimating);
		if (! bIconIsAnimating)
			pFlyingContainer->pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
		else
			bContinue = TRUE;
	}
	
	gldi_object_notify (pFlyingContainer, NOTIFICATION_UPDATE, pFlyingContainer, &bContinue);
	
	if (! bContinue)  // end of the explosion animation, destroy the container.
	{
		gldi_object_unref (GLDI_OBJECT(pFlyingContainer));
		return FALSE;
	}
	else
		return TRUE;
}

CairoFlyingContainer *gldi_flying_container_new (Icon *pFlyingIcon, CairoDock *pOriginDock)
{
	g_return_val_if_fail (pFlyingIcon != NULL, NULL);
	CairoFlyingAttr attr;
	memset (&attr, 0, sizeof (CairoFlyingAttr));
	attr.pIcon = pFlyingIcon;
	attr.pOriginDock = pOriginDock;
	return (CairoFlyingContainer*)gldi_object_new (&myFlyingObjectMgr, &attr);
}

void gldi_flying_container_drag (CairoFlyingContainer *pFlyingContainer, CairoDock *pOriginDock)
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

void gldi_flying_container_terminate (CairoFlyingContainer *pFlyingContainer)
{
	// detach the icon from the container
	Icon *pIcon = pFlyingContainer->pIcon;
	pFlyingContainer->pIcon = NULL;
	cairo_dock_set_icon_container (pIcon, NULL);
	
	// destroy it, or place it in a desklet.
	if (pIcon->cDesktopFileName != NULL)  // a launcher/sub-dock/separator, that is part of the theme
	{
		gldi_object_delete (GLDI_OBJECT(pIcon));
	}
	else if (CAIRO_DOCK_IS_APPLET(pIcon))  /// faire une fonction dans la factory ...
	{
		cd_debug ("le module %s devient un desklet", pIcon->pModuleInstance->cConfFilePath);
		gldi_module_instance_detach_at_position (pIcon->pModuleInstance,
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
	gldi_object_register_notification (&myFlyingObjectMgr,
		NOTIFICATION_UPDATE,
		(GldiNotificationFunc) _on_update_flying_container_notification,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myFlyingObjectMgr,
		NOTIFICATION_RENDER,
		(GldiNotificationFunc) _on_render_flying_container_notification,
		GLDI_RUN_AFTER, NULL);
}

  ///////////////
 /// MANAGER ///
///////////////

static void _detach_icon (GldiContainer *pContainer, G_GNUC_UNUSED Icon *pIcon)
{
	CairoFlyingContainer *pFlyingContainer = (CairoFlyingContainer*)pContainer;
	// remove icon
	pFlyingContainer->pIcon = NULL;
}

static void _insert_icon (GldiContainer *pContainer, Icon *pIcon, G_GNUC_UNUSED gboolean bAnimateIcon)
{
	CairoFlyingContainer *pFlyingContainer = (CairoFlyingContainer*)pContainer;
	// insert icon
	g_return_if_fail (pFlyingContainer->pIcon == NULL || pFlyingContainer->pIcon == pIcon);
	pFlyingContainer->pIcon = pIcon;
}

static void init_object (GldiObject *obj, gpointer attr)
{
	CairoFlyingContainer *pFlyingContainer = (CairoFlyingContainer*)obj;
	CairoFlyingAttr *pAttribute = (CairoFlyingAttr*)attr;
	Icon *pFlyingIcon = pAttribute->pIcon;
	CairoDock *pOriginDock = pAttribute->pOriginDock;  // since the icon is already detached, we need its origin dock in the attributes
	
	pFlyingContainer->container.iface.animation_loop = _animation_loop;
	pFlyingContainer->container.iface.insert_icon = _insert_icon;
	pFlyingContainer->container.iface.detach_icon = _detach_icon;
	
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
	_load_emblem (pFlyingIcon);
	_load_explosion_image (iWidth);
	
	struct timeval tv;
	int r = gettimeofday (&tv, NULL);
	if (r == 0)
		pFlyingContainer->fCreationTime = tv.tv_sec + tv.tv_usec * 1e-6;
}

static void reset_object (GldiObject *obj)
{
	CairoFlyingContainer *pFlyingContainer = (CairoFlyingContainer*)obj;
	// detach the icon
	if (pFlyingContainer->pIcon != NULL)
		cairo_dock_set_icon_container (pFlyingContainer->pIcon, NULL);
	// free data
	cairo_dock_free_image_buffer (s_pEmblem);
	s_pEmblem = NULL;
}

void gldi_register_flying_manager (void)
{
	// Manager
	memset (&myFlyingsMgr, 0, sizeof (GldiManager));
	myFlyingsMgr.cModuleName 	= "Flyings";
	myFlyingsMgr.init 			= init;
	myFlyingsMgr.load 			= NULL;  // data loaded on the 1st creation.
	myFlyingsMgr.unload 		= unload;
	myFlyingsMgr.reload 		= (GldiManagerReloadFunc)NULL;
	myFlyingsMgr.get_config 	= (GldiManagerGetConfigFunc)NULL;
	myFlyingsMgr.reset_config 	= (GldiManagerResetConfigFunc)NULL;
	// Config
	myFlyingsMgr.pConfig = (GldiManagerConfigPtr)NULL;
	myFlyingsMgr.iSizeOfConfig = 0;
	// data
	myFlyingsMgr.pData = (GldiManagerDataPtr)NULL;
	myFlyingsMgr.iSizeOfData = 0;
	// register
	gldi_object_init (GLDI_OBJECT(&myFlyingsMgr), &myManagerObjectMgr, NULL);
	
	// Object Manager
	memset (&myFlyingObjectMgr, 0, sizeof (GldiObjectManager));
	myFlyingObjectMgr.cName 	     = "Flying";
	myFlyingObjectMgr.iObjectSize    = sizeof (CairoFlyingContainer);
	// interface
	myFlyingObjectMgr.init_object    = init_object;
	myFlyingObjectMgr.reset_object   = reset_object;
	// signals
	gldi_object_install_notifications (&myFlyingObjectMgr, NB_NOTIFICATIONS_FLYING_CONTAINER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myFlyingObjectMgr), &myContainerObjectMgr);
}
