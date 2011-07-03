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
#include <cairo.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "gldi-config.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-container.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-emblem.h"
#define _MANAGER_DEF_
#include "cairo-dock-flying-container.h"

/**#define HAND_WIDTH 80
#define HAND_HEIGHT 50*/
#define EXPLOSION_NB_FRAMES 10

CairoFlyingManager myFlyingsMgr;

extern CairoContainer *g_pPrimaryContainer;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;

static cairo_surface_t *s_pExplosionSurface = NULL;
static GLuint s_iExplosionTexture = 0;
static double s_fExplosionWidth, s_fExplosionHeight;
static CairoEmblem *s_pEmblem = NULL;

static void _cairo_dock_load_emblem (Icon *pIcon, CairoFlyingContainer *pFlyingContainer)
{
	cairo_dock_free_emblem (s_pEmblem);
	const gchar *cImage = NULL;
	if (CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
	{
		cImage = GTK_STOCK_JUMP_TO"-rtl";  // GTK_STOCK_JUMP_TO only doesn't exist.
	}
	else
	{
		cImage = GTK_STOCK_DELETE;
	}
	gchar *cIcon = cairo_dock_search_icon_s_path (cImage);
	s_pEmblem = cairo_dock_make_emblem (cIcon, pIcon, CAIRO_CONTAINER (pFlyingContainer));
	cairo_dock_set_emblem_position (s_pEmblem, CAIRO_DOCK_EMBLEM_UPPER_LEFT);
	g_free (cIcon);
}
static void _cairo_dock_load_explosion_image (int iWidth)
{
	if (s_pExplosionSurface == NULL)
	{
		gchar *cExplosionFile = g_strdup_printf ("%s/%s", g_cCurrentThemePath, "explosion.png");
		if (g_file_test (cExplosionFile, G_FILE_TEST_EXISTS))
		{
			s_pExplosionSurface = cairo_dock_create_surface_for_icon (cExplosionFile,
				iWidth * EXPLOSION_NB_FRAMES,
				iWidth);
		}
		else
		{
			s_pExplosionSurface = cairo_dock_create_surface_for_icon (GLDI_SHARE_DATA_DIR"/explosion/explosion.png",
				iWidth * EXPLOSION_NB_FRAMES,
				iWidth);
		}
		g_free (cExplosionFile);
		s_fExplosionWidth = iWidth;
		s_fExplosionHeight = iWidth;
	}
	if (s_pExplosionSurface != NULL && g_bUseOpenGL && s_iExplosionTexture == 0)
	{
		s_iExplosionTexture = cairo_dock_create_texture_from_surface (s_pExplosionSurface);
		cairo_surface_destroy (s_pExplosionSurface);
		s_pExplosionSurface = NULL;
	}
}


static gboolean _cairo_dock_update_flying_container_notification (gpointer pUserData, CairoFlyingContainer *pFlyingContainer, gboolean *bContinueAnimation)
{
	if (pFlyingContainer->container.iAnimationStep > 0)
	{
		pFlyingContainer->container.iAnimationStep --;
		if (pFlyingContainer->container.iAnimationStep == 0)
		{
			*bContinueAnimation = FALSE;
			return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
		}
	}
	gtk_widget_queue_draw (pFlyingContainer->container.pWidget);
	
	*bContinueAnimation = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static gboolean _cairo_dock_render_flying_container_notification (gpointer pUserData, CairoFlyingContainer *pFlyingContainer, cairo_t *pCairoContext)
{
	Icon *pIcon = pFlyingContainer->pIcon;
	if (pCairoContext != NULL)
	{
		if (pIcon != NULL)
		{
			cairo_save (pCairoContext);
			cairo_dock_render_one_icon (pIcon, CAIRO_CONTAINER (pFlyingContainer), pCairoContext, 1., FALSE);
			cairo_restore (pCairoContext);
			
			_cairo_dock_apply_emblem_surface (s_pEmblem, pFlyingContainer->container.iWidth, pFlyingContainer->container.iHeight, pCairoContext);
		}
		else if (pFlyingContainer->container.iAnimationStep > 0)
		{
			int x = 0;
			int y = (pFlyingContainer->container.iHeight - pFlyingContainer->container.iWidth) / 2;
			int iCurrentFrame = EXPLOSION_NB_FRAMES - pFlyingContainer->container.iAnimationStep;
			
			cairo_rectangle (pCairoContext,
				x,
				y,
				s_fExplosionWidth,
				s_fExplosionHeight);
			cairo_clip (pCairoContext);
			
			cairo_set_source_surface (pCairoContext,
				s_pExplosionSurface,
				x - (iCurrentFrame * s_fExplosionWidth),
				y);
			cairo_paint (pCairoContext);
		}
	}
	else
	{
		if (pIcon != NULL)
		{
			glPushMatrix ();
			/*glTranslatef (pFlyingContainer->container.iWidth / 2,
				pIcon->fHeight * pIcon->fScale/2,
				- pFlyingContainer->container.iHeight);*/
			cairo_dock_render_one_icon_opengl (pIcon, CAIRO_CONTAINER (pFlyingContainer), 1., FALSE);
			glPopMatrix ();
			
			glPushMatrix ();
			glTranslatef (pFlyingContainer->container.iWidth/2,
				pFlyingContainer->container.iHeight/2,
				0);
			_cairo_dock_enable_texture ();
			_cairo_dock_set_blend_alpha ();
			
			_cairo_dock_apply_emblem_texture (s_pEmblem, pFlyingContainer->container.iWidth, pFlyingContainer->container.iHeight);
			
			_cairo_dock_disable_texture ();
			glPopMatrix ();
		}
		else if (pFlyingContainer->container.iAnimationStep > 0)
		{
			int iCurrentFrame = EXPLOSION_NB_FRAMES - pFlyingContainer->container.iAnimationStep;
			
			glTranslatef (pFlyingContainer->container.iWidth/2,
				pFlyingContainer->container.iHeight/2,
				-3.);
			glBindTexture (GL_TEXTURE_2D, s_iExplosionTexture);
			_cairo_dock_enable_texture ();
			_cairo_dock_set_blend_source ();
			_cairo_dock_set_alpha (1.);
			_cairo_dock_apply_current_texture_portion_at_size_with_offset ((double) iCurrentFrame / EXPLOSION_NB_FRAMES, 1.,
				1. / EXPLOSION_NB_FRAMES, 1.,
				s_fExplosionWidth, s_fExplosionHeight,
				0., 0.);
			
			_cairo_dock_disable_texture ();
		}
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


static gboolean on_expose_flying_icon (GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	CairoFlyingContainer *pFlyingContainer)
{
	if (g_bUseOpenGL)
	{
		GdkGLContext *pGlContext = gtk_widget_get_gl_context (pWidget);
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return FALSE;
		
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity ();
		
		cairo_dock_apply_desktop_background_opengl (CAIRO_CONTAINER (pFlyingContainer));
		
		cairo_dock_notify_on_object (&myFlyingsMgr, NOTIFICATION_RENDER_FLYING_CONTAINER, pFlyingContainer, NULL);
		cairo_dock_notify_on_object (pFlyingContainer, NOTIFICATION_RENDER_FLYING_CONTAINER, pFlyingContainer, NULL);
		
		if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
			gdk_gl_drawable_swap_buffers (pGlDrawable);
		else
			glFlush ();
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
 	else
	{
		cairo_t *pCairoContext = cairo_dock_create_drawing_context_on_container (CAIRO_CONTAINER (pFlyingContainer));
		
		cairo_dock_notify_on_object (&myFlyingsMgr, NOTIFICATION_RENDER_FLYING_CONTAINER, pFlyingContainer, pCairoContext);
		cairo_dock_notify_on_object (pFlyingContainer, NOTIFICATION_RENDER_FLYING_CONTAINER, pFlyingContainer, pCairoContext);
		
		cairo_destroy (pCairoContext);
	}
	
	return FALSE;
}

static gboolean on_configure_flying_icon (GtkWidget* pWidget,
	GdkEventConfigure* pEvent,
	CairoFlyingContainer *pFlyingContainer)
{
	if (pFlyingContainer->container.iWidth != pEvent->width || pFlyingContainer->container.iHeight != pEvent->height)
	{
		pFlyingContainer->container.iWidth = pEvent->width;
		pFlyingContainer->container.iHeight = pEvent->height;
		
		if (g_bUseOpenGL)
		{
			GdkGLContext* pGlContext = gtk_widget_get_gl_context (pWidget);
			GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (pWidget);
			GLsizei w = pEvent->width;
			GLsizei h = pEvent->height;
			if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
				return FALSE;
			
			glViewport(0, 0, w, h);
			
			cairo_dock_set_ortho_view (CAIRO_CONTAINER (pFlyingContainer));
			
			gdk_gl_drawable_gl_end (pGlDrawable);
		}
	}
	return FALSE;
}

CairoFlyingContainer *cairo_dock_create_flying_container (Icon *pFlyingIcon, CairoDock *pOriginDock)
{
	g_return_val_if_fail (pFlyingIcon != NULL, NULL);
	CairoFlyingContainer * pFlyingContainer = g_new0 (CairoFlyingContainer, 1);
	pFlyingContainer->container.iType = CAIRO_DOCK_TYPE_FLYING_CONTAINER;
	GtkWidget* pWindow = cairo_dock_init_container (CAIRO_CONTAINER (pFlyingContainer));
	cairo_dock_install_notifications_on_object (pFlyingContainer, NB_NOTIFICATIONS_FLYING_CONTAINER);
	gtk_window_set_keep_above (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_title (GTK_WINDOW(pWindow), "cairo-dock-flying-icon");
	
	pFlyingContainer->pIcon = pFlyingIcon;
	pFlyingContainer->container.bIsHorizontal = TRUE;
	pFlyingContainer->container.bDirectionUp = TRUE;
	pFlyingContainer->container.fRatio = 1.;
	pFlyingContainer->container.bUseReflect = FALSE;
	
	g_signal_connect (G_OBJECT (pWindow),
		"expose-event",
		G_CALLBACK (on_expose_flying_icon),
		pFlyingContainer);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (on_configure_flying_icon),
		pFlyingContainer);
	
	pFlyingContainer->container.bInside = TRUE;
	pFlyingIcon->bPointed = TRUE;
	pFlyingIcon->fScale = 1.;
	
	pFlyingContainer->container.iWidth = pFlyingIcon->fWidth * pFlyingIcon->fScale * 1.333;
	pFlyingContainer->container.iHeight = pFlyingIcon->fHeight * pFlyingIcon->fScale * 1.333;
	pFlyingIcon->fDrawX = pFlyingIcon->fWidth * pFlyingIcon->fScale / 3;
	pFlyingIcon->fDrawY = pFlyingIcon->fHeight * pFlyingIcon->fScale / 3;
	
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
	gtk_window_present (GTK_WINDOW (pWindow));
	/*cd_debug ("%s (%d;%d %dx%d)\n", __func__ pFlyingContainer->container.iWindowPositionX,
		pFlyingContainer->container.iWindowPositionY,
		pFlyingContainer->container.iWidth,
		pFlyingContainer->container.iHeight);*/
	gdk_window_move_resize (pWindow->window,
		pFlyingContainer->container.iWindowPositionX,
		pFlyingContainer->container.iWindowPositionY,
		pFlyingContainer->container.iWidth,
		pFlyingContainer->container.iHeight);
	/*gtk_window_resize (GTK_WINDOW (pWindow),
		pFlyingContainer->container.iWidth,
		pFlyingContainer->container.iHeight);
	gtk_window_move (GTK_WINDOW (pWindow),
		pFlyingContainer->container.iWindowPositionX,
		pFlyingContainer->container.iWindowPositionY);*/
	
	_cairo_dock_load_emblem (pFlyingIcon, pFlyingContainer);
	_cairo_dock_load_explosion_image (pFlyingContainer->container.iWidth);
	
	struct timeval tv;
	int r = gettimeofday (&tv, NULL);
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
	cairo_dock_finish_container (CAIRO_CONTAINER (pFlyingContainer));
	g_free (pFlyingContainer);
}

void cairo_dock_terminate_flying_container (CairoFlyingContainer *pFlyingContainer)
{
	Icon *pIcon = pFlyingContainer->pIcon;
	pFlyingContainer->pIcon = NULL;
	pFlyingContainer->container.iAnimationStep = EXPLOSION_NB_FRAMES+1;
	cairo_dock_launch_animation (CAIRO_CONTAINER (pFlyingContainer));  // au cas ou pas d'animation.
	
	if (pIcon->cDesktopFileName != NULL)  // c'est un lanceur, ou un separateur manuel, ou un sous-dock.
	{
		cairo_dock_remove_one_icon_from_dock (NULL, pIcon);
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
}


  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	if (s_iExplosionTexture != 0)
	{
		_cairo_dock_delete_texture (s_iExplosionTexture);
		s_iExplosionTexture = 0;
	}
	if (s_pEmblem != NULL)
	{
		cairo_dock_free_emblem (s_pEmblem);
		s_pEmblem = NULL;
	}
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	cairo_dock_register_notification_on_object (&myFlyingsMgr,
		NOTIFICATION_UPDATE_FLYING_CONTAINER,
		(CairoDockNotificationFunc) _cairo_dock_update_flying_container_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
	cairo_dock_register_notification_on_object (&myFlyingsMgr,
		NOTIFICATION_RENDER_FLYING_CONTAINER,
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
	// register
	gldi_register_manager (GLDI_MANAGER(&myFlyingsMgr));
}
