/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <stdlib.h>
#include <cairo.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-container.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-flying-container.h"

#define HAND_WIDTH 76
#define HAND_HEIGHT 50
#define EXPLOSION_NB_FRAMES 10

extern CairoDock *g_pMainDock;
extern GdkGLConfig* g_pGlConfig;
extern gchar *g_cCurrentLaunchersPath;
extern gboolean g_bUseOpenGL;
extern gboolean g_bIndirectRendering;
extern int g_iXScreenWidth[2], g_iXScreenHeight[2];

static cairo_surface_t *s_pHandSurface = NULL;
static GLuint s_iHandTexture = 0;
static double s_fHandWidth, s_fHandHeight;
static cairo_surface_t *s_pExplosionSurface = NULL;
static GLuint s_iExplosionTexture = 0;
static double s_fExplosionWidth, s_fExplosionHeight;

static void _cairo_dock_load_hand_image (cairo_t *pCairoContext, int iWidth)
{
	if (s_pHandSurface != NULL || s_iHandTexture != 0)
		return ;
	
	s_pHandSurface = cairo_dock_create_surface_from_image (CAIRO_DOCK_SHARE_DATA_DIR"/hand.svg",
		pCairoContext,
		1.,
		iWidth, 0.,
		CAIRO_DOCK_KEEP_RATIO,
		&s_fHandWidth, &s_fHandHeight,
		NULL, NULL);
	if (s_pHandSurface != NULL && g_bUseOpenGL)
	{
		s_iHandTexture = cairo_dock_create_texture_from_surface (s_pHandSurface);
		cairo_surface_destroy (s_pHandSurface);
		s_pHandSurface = NULL;
	}
}
static void _cairo_dock_load_explosion_image (cairo_t *pCairoContext, int iWidth)
{
	if (s_pExplosionSurface != NULL || s_iExplosionTexture != 0)
		return ;
	
	s_pExplosionSurface = cairo_dock_create_surface_for_icon (CAIRO_DOCK_SHARE_DATA_DIR"/explosion/explosion.png",
		pCairoContext,
		iWidth * EXPLOSION_NB_FRAMES,
		iWidth);
	s_fExplosionWidth = iWidth;
	s_fExplosionHeight = iWidth;
	if (s_pExplosionSurface != NULL && g_bUseOpenGL)
	{
		s_iExplosionTexture = cairo_dock_create_texture_from_surface (s_pExplosionSurface);
		cairo_surface_destroy (s_pExplosionSurface);
		s_pExplosionSurface = NULL;
	}
}


gboolean cairo_dock_update_flying_container_notification (gpointer pUserData, CairoFlyingContainer *pFlyingContainer, gboolean *bContinueAnimation)
{
	if (pFlyingContainer->iAnimationCount > 0)
	{
		pFlyingContainer->iAnimationCount --;
		if (pFlyingContainer->iAnimationCount == 0)
		{
			*bContinueAnimation = FALSE;
			return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
		}
	}
	gtk_widget_queue_draw (pFlyingContainer->pWidget);
	
	*bContinueAnimation = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_render_flying_container_notification (gpointer pUserData, CairoFlyingContainer *pFlyingContainer, cairo_t *pCairoContext)
{
	Icon *pIcon = pFlyingContainer->pIcon;
	if (pCairoContext != NULL)
	{
		if (pIcon != NULL)
		{
			cairo_save (pCairoContext);
			cairo_dock_render_one_icon (pIcon, pFlyingContainer, pCairoContext, 1., TRUE);
			cairo_restore (pCairoContext);
			
			cairo_set_source_surface (pCairoContext, s_pHandSurface, 0., 0.);
			cairo_paint (pCairoContext);
		}
		else if (pFlyingContainer->iAnimationCount > 0)
		{
			int x = 0;
			int y = (pFlyingContainer->iHeight - pFlyingContainer->iWidth) / 2;
			int iCurrentFrame = EXPLOSION_NB_FRAMES - pFlyingContainer->iAnimationCount;
			
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
			glTranslatef (pFlyingContainer->iWidth / 2,
				pIcon->fHeight * pIcon->fScale/2,
				- pFlyingContainer->iHeight);
			cairo_dock_render_one_icon_opengl (pIcon, pFlyingContainer, 1., TRUE);
			glPopMatrix ();
			
			glTranslatef (pFlyingContainer->iWidth / 2,
				pFlyingContainer->iHeight - s_fHandHeight/2,
				- 3.);
			cairo_dock_draw_texture (s_iHandTexture, s_fHandWidth, s_fHandHeight);
		}
		else if (pFlyingContainer->iAnimationCount > 0)
		{
			int x = 0;
			int y = (pFlyingContainer->iHeight - pFlyingContainer->iWidth) / 2;
			int iCurrentFrame = EXPLOSION_NB_FRAMES - pFlyingContainer->iAnimationCount;
			
			glEnable (GL_SCISSOR_TEST);
			glScissor (pFlyingContainer->iWidth/2-s_fExplosionWidth/2,
				pFlyingContainer->iHeight/2-s_fExplosionHeight/2,
				s_fExplosionWidth,
				s_fExplosionHeight);
			
			glTranslatef (pFlyingContainer->iWidth/2 + s_fExplosionWidth * (.5 * EXPLOSION_NB_FRAMES - iCurrentFrame),
				pFlyingContainer->iHeight/2,
				-3.);
			
			glColor4f (1., 1., 1., 1.);
			cairo_dock_draw_texture (s_iExplosionTexture,
				s_fExplosionWidth * EXPLOSION_NB_FRAMES,
				s_fExplosionHeight);
			
			glDisable (GL_SCISSOR_TEST);
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
		
		cairo_dock_apply_desktop_background (CAIRO_CONTAINER (pFlyingContainer));
		
		cairo_dock_notify (CAIRO_DOCK_RENDER_FLYING_CONTAINER, pFlyingContainer, NULL);
		
		if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
			gdk_gl_drawable_swap_buffers (pGlDrawable);
		else
			glFlush ();
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
	else
	{
		cairo_t *pCairoContext = cairo_dock_create_drawing_context (CAIRO_CONTAINER (pFlyingContainer));
		
		cairo_dock_notify (CAIRO_DOCK_RENDER_FLYING_CONTAINER, pFlyingContainer, pCairoContext);
		
		cairo_destroy (pCairoContext);
	}
	
	return FALSE;
}

static gboolean on_configure_flying_icon (GtkWidget* pWidget,
	GdkEventConfigure* pEvent,
	CairoFlyingContainer *pFlyingContainer)
{
	if (pFlyingContainer->iWidth != pEvent->width || pFlyingContainer->iHeight != pEvent->height)
	{
		pFlyingContainer->iWidth = pEvent->width;
		pFlyingContainer->iHeight = pEvent->height;
		
		if (g_bUseOpenGL)
		{
			GdkGLContext* pGlContext = gtk_widget_get_gl_context (pWidget);
			GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (pWidget);
			GLsizei w = pEvent->width;
			GLsizei h = pEvent->height;
			if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
				return FALSE;
			
			glViewport(0, 0, w, h);
			
			cairo_dock_set_ortho_view (w, h);
			
			gdk_gl_drawable_gl_end (pGlDrawable);
		}
	}
	return FALSE;
}

CairoFlyingContainer *cairo_dock_create_flying_container (Icon *pFlyingIcon, CairoDock *pOriginDock, gboolean bDrawHand)
{
	CairoFlyingContainer * pFlyingContainer = g_new0 (CairoFlyingContainer, 1);
	pFlyingContainer->iType = CAIRO_DOCK_TYPE_FLYING_CONTAINER;
	GtkWidget* pWindow = cairo_dock_create_container_window ();
	gtk_window_set_title (GTK_WINDOW(pWindow), "cairo-dock-flying-icon");
	pFlyingContainer->pWidget = pWindow;
	pFlyingContainer->pIcon = pFlyingIcon;
	pFlyingContainer->bIsHorizontal = TRUE;
	pFlyingContainer->bDirectionUp = TRUE;
	pFlyingContainer->fRatio = 1.;
	pFlyingContainer->bUseReflect = FALSE;
	cairo_dock_set_default_animation_delta_t (pFlyingContainer);
	
	g_signal_connect (G_OBJECT (pWindow),
		"expose-event",
		G_CALLBACK (on_expose_flying_icon),
		pFlyingContainer);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (on_configure_flying_icon),
		pFlyingContainer);
	
	pFlyingContainer->bInside = TRUE;
	pFlyingIcon->bPointed = TRUE;
	pFlyingIcon->fScale = 1.;
	pFlyingContainer->iWidth = pFlyingIcon->fWidth * pFlyingIcon->fScale * 3.7;
	pFlyingContainer->iHeight = pFlyingIcon->fHeight * pFlyingIcon->fScale + 1.*pFlyingContainer->iWidth / HAND_WIDTH * HAND_HEIGHT * .6;
	pFlyingIcon->fDrawX = (pFlyingContainer->iWidth - pFlyingIcon->fWidth * pFlyingIcon->fScale) / 2;
	pFlyingIcon->fDrawY = pFlyingContainer->iHeight - pFlyingIcon->fHeight * pFlyingIcon->fScale;
	
	if (pOriginDock->bHorizontalDock)
	{
		pFlyingContainer->iPositionX = pOriginDock->iWindowPositionX + pOriginDock->iMouseX - pFlyingContainer->iWidth/2;
		pFlyingContainer->iPositionY = pOriginDock->iWindowPositionY + pOriginDock->iMouseY - pFlyingContainer->iHeight/2;
	}
	else
	{
		pFlyingContainer->iPositionY = pOriginDock->iWindowPositionX + pOriginDock->iMouseX - pFlyingContainer->iWidth/2;
		pFlyingContainer->iPositionX = pOriginDock->iWindowPositionY + pOriginDock->iMouseY - pFlyingContainer->iHeight/2;
	}
	/*g_print ("%s (%d;%d %dx%d)\n", __func__ pFlyingContainer->iPositionX,
		pFlyingContainer->iPositionY,
		pFlyingContainer->iWidth,
		pFlyingContainer->iHeight);*/
	gdk_window_move_resize (pWindow->window,
		pFlyingContainer->iPositionX,
		pFlyingContainer->iPositionY,
		pFlyingContainer->iWidth,
		pFlyingContainer->iHeight);
	/*gtk_window_resize (GTK_WINDOW (pWindow),
		pFlyingContainer->iWidth,
		pFlyingContainer->iHeight);
	gtk_window_move (GTK_WINDOW (pWindow),
		pFlyingContainer->iPositionX,
		pFlyingContainer->iPositionY);*/
	gtk_window_present (GTK_WINDOW (pWindow));
	
	cairo_t *pSourceContext = cairo_dock_create_context_from_container (CAIRO_CONTAINER (pFlyingContainer));
	_cairo_dock_load_hand_image (pSourceContext, pFlyingContainer->iWidth);
	_cairo_dock_load_explosion_image (pSourceContext, pFlyingContainer->iWidth);
	cairo_destroy (pSourceContext);
	
	pFlyingContainer->bDrawHand = bDrawHand;
	if (bDrawHand)
		cairo_dock_request_icon_animation (pFlyingIcon, pFlyingContainer, bDrawHand ? "pulse" : "bounce", 1e6);
	cairo_dock_launch_animation (pFlyingContainer);  // au cas ou pas d'animation.
	
	return pFlyingContainer;
}

void cairo_dock_drag_flying_container (CairoFlyingContainer *pFlyingContainer, CairoDock *pOriginDock)
{
	if (pOriginDock->bHorizontalDock)
	{
		pFlyingContainer->iPositionX = pOriginDock->iWindowPositionX + pOriginDock->iMouseX - pFlyingContainer->iWidth/2;
		pFlyingContainer->iPositionY = pOriginDock->iWindowPositionY + pOriginDock->iMouseY - pFlyingContainer->iHeight/2;
	}
	else
	{
		pFlyingContainer->iPositionY = pOriginDock->iWindowPositionX + pOriginDock->iMouseX - pFlyingContainer->iWidth/2;
		pFlyingContainer->iPositionX = pOriginDock->iWindowPositionY + pOriginDock->iMouseY - pFlyingContainer->iHeight/2;
	}
	//g_print ("  on tire l'icone volante en (%d;%d)\n", pFlyingContainer->iPositionX, pFlyingContainer->iPositionY);
	gtk_window_move (GTK_WINDOW (pFlyingContainer->pWidget),
		pFlyingContainer->iPositionX,
		pFlyingContainer->iPositionY);
}

void cairo_dock_free_flying_container (CairoFlyingContainer *pFlyingContainer)
{
	g_print ("%s ()\n", __func__);
	gtk_widget_destroy (pFlyingContainer->pWidget);  // enleve les signaux.
	if (pFlyingContainer->iSidGLAnimation != 0)
		g_source_remove (pFlyingContainer->iSidGLAnimation);
	g_free (pFlyingContainer);
}

void cairo_dock_terminate_flying_container (CairoFlyingContainer *pFlyingContainer)
{
	Icon *pIcon = pFlyingContainer->pIcon;
	pFlyingContainer->pIcon = NULL;
	pFlyingContainer->iAnimationCount = EXPLOSION_NB_FRAMES+1;
	
	if (pIcon->acDesktopFileName != NULL)  // c'est un lanceur, ou un separateur manuel, ou un sous-dock.
	{
		gchar *cIconPath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pIcon->acDesktopFileName);
		g_remove (cIconPath);
		g_free (cIconPath);

		if (CAIRO_DOCK_IS_URI_LAUNCHER (pIcon))
		{
			cairo_dock_fm_remove_monitor (pIcon);
		}
		cairo_dock_free_icon (pIcon);
	}
	else if (CAIRO_DOCK_IS_APPLET(pIcon))
	{
		g_print ("le module %s devient un desklet\n", pIcon->pModuleInstance->cConfFilePath);
		
		GError *erreur = NULL;
		GKeyFile *pKeyFile = cairo_dock_open_key_file (pIcon->pModuleInstance->cConfFilePath);
		if (pKeyFile != NULL)
		{
			//\______________ On centre le desklet sur l'icone volante.
			int iDeskletWidth = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "width", NULL, 92, NULL, NULL);
			int iDeskletHeight = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "height", NULL, 92, NULL, NULL);
			
			int iDeskletPositionX = pFlyingContainer->iPositionX + (pFlyingContainer->iWidth - iDeskletWidth)/2;
			int iDeskletPositionY = pFlyingContainer->iPositionY + (pFlyingContainer->iHeight - iDeskletHeight)/2;
			
			int iRelativePositionX = (iDeskletPositionX + iDeskletWidth/2 <= g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL]/2 ? iDeskletPositionX : iDeskletPositionX - g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL]);
			int iRelativePositionY = (iDeskletPositionY + iDeskletHeight/2 <= g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]/2 ? iDeskletPositionY : iDeskletPositionY - g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
			
			g_key_file_set_boolean (pKeyFile, "Desklet", "initially detached", TRUE);
			g_key_file_set_double (pKeyFile, "Desklet", "x position", iDeskletPositionX);
			g_key_file_set_double (pKeyFile, "Desklet", "y position", iDeskletPositionY);
			
			cairo_dock_write_keys_to_file (pKeyFile, pIcon->pModuleInstance->cConfFilePath);
			g_key_file_free (pKeyFile);
			
			//\______________ On detache le module.
			cairo_dock_reload_module_instance (pIcon->pModuleInstance, TRUE);
			
			//\______________ On fait apparaitre le desklet avec un effet de zoom.
			if (pIcon->pModuleInstance->pDesklet)  // normalement toujours vrai.
			{
				while (pIcon->pModuleInstance->pDesklet->iDesiredWidth != 0 && pIcon->pModuleInstance->pDesklet->iDesiredHeight != 0 && (pIcon->pModuleInstance->pDesklet->iKnownWidth != pIcon->pModuleInstance->pDesklet->iDesiredWidth || pIcon->pModuleInstance->pDesklet->iKnownHeight != pIcon->pModuleInstance->pDesklet->iDesiredHeight))
				{
					gtk_main_iteration ();  // on le laisse se charger en plein.
					if (! pIcon->pModuleInstance->pDesklet)  // ne devrait pas arriver.
						break ;
				}
				cairo_dock_zoom_out_desklet (pIcon->pModuleInstance->pDesklet);
			}
		}
	}
}
