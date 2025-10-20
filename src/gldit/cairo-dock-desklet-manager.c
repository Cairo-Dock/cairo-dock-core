/**
* This file is a part of the Cairo-Dock project
* Login : <ctaf42@gmail.com>
* Started on  Sun Jan 27 18:35:38 2008 Cedric GESTES
* $Id$
*
* Author(s)
*  - Cedric GESTES <ctaf42@gmail.com>
*  - Fabrice REY
*
* Copyright (C) 2008 Cedric GESTES
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
#include <string.h>
#include <math.h>
#include <time.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include "gldi-config.h"
#include "cairo-dock-module-manager.h"
#include "cairo-dock-animations.h"  // cairo_dock_launch_animation
#include "cairo-dock-module-instance-manager.h"  // gldi_module_instance_open_conf_file
#include "cairo-dock-config.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_set_icon_container
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-container-priv.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-style-manager.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-opengl-path.h"
#define _MANAGER_DEF_
#include "cairo-dock-desklet-manager.h"

// public (manager, config, data)
CairoDeskletsParam myDeskletsParam;
GldiManager myDeskletsMgr;
GldiObjectManager myDeskletObjectMgr;

// dependancies
extern gboolean g_bUseOpenGL;
extern CairoDock *g_pMainDock;  // pour savoir s'il faut afficher les boutons rattach.

// private
static CairoDockImageBuffer s_pRotateButtonBuffer;
static CairoDockImageBuffer s_pRetachButtonBuffer;
static CairoDockImageBuffer s_pDepthRotateButtonBuffer;
static CairoDockImageBuffer s_pNoInputButtonBuffer;
static GList *s_pDeskletList = NULL;
static time_t s_iStartupTime = 0;

static gboolean _on_update_desklet_notification (gpointer data, CairoDesklet *pDesklet, gboolean *bContinueAnimation);
static gboolean _on_enter_leave_desklet_notification (gpointer data, CairoDesklet *pDesklet, gboolean *bStartAnimation);
static gboolean _on_render_desklet_notification (gpointer pUserData, CairoDesklet *pDesklet, cairo_t *pCairoContext);

#define _no_input_button_alpha(pDesklet) (pDesklet->bNoInput ? .4+.6*pDesklet->fButtonsAlpha : pDesklet->fButtonsAlpha)
#define ANGLE_MIN .1  // under this angle, the rotation is ignored.

void _load_desklet_buttons (void)
{
	if (myDeskletsParam.cRotateButtonImage != NULL)
	{
		cairo_dock_load_image_buffer (&s_pRotateButtonBuffer,
			myDeskletsParam.cRotateButtonImage,
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			CAIRO_DOCK_FILL_SPACE);
	}
	if (s_pRotateButtonBuffer.pSurface == NULL)
	{
		cairo_dock_load_image_buffer (&s_pRotateButtonBuffer,
			GLDI_SHARE_DATA_DIR"/icons/rotate-desklet.svg",
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			CAIRO_DOCK_FILL_SPACE);
	}
	
	if (myDeskletsParam.cRetachButtonImage != NULL)
	{
		cairo_dock_load_image_buffer (&s_pRetachButtonBuffer,
			myDeskletsParam.cRetachButtonImage,
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			CAIRO_DOCK_FILL_SPACE);
	}
	if (s_pRetachButtonBuffer.pSurface == NULL)
	{
		cairo_dock_load_image_buffer (&s_pRetachButtonBuffer,
			GLDI_SHARE_DATA_DIR"/icons/retach-desklet.svg",
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			CAIRO_DOCK_FILL_SPACE);
	}

	if (myDeskletsParam.cDepthRotateButtonImage != NULL)
	{
		cairo_dock_load_image_buffer (&s_pDepthRotateButtonBuffer,
			myDeskletsParam.cDepthRotateButtonImage,
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			CAIRO_DOCK_FILL_SPACE);
	}
	if (s_pDepthRotateButtonBuffer.pSurface == NULL)
	{
		cairo_dock_load_image_buffer (&s_pDepthRotateButtonBuffer,
			GLDI_SHARE_DATA_DIR"/icons/depth-rotate-desklet.svg",
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			CAIRO_DOCK_FILL_SPACE);
	}
	
	if (myDeskletsParam.cNoInputButtonImage != NULL)
	{
		cairo_dock_load_image_buffer (&s_pNoInputButtonBuffer,
			myDeskletsParam.cNoInputButtonImage,
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			CAIRO_DOCK_FILL_SPACE);
	}
	if (s_pNoInputButtonBuffer.pSurface == NULL)
	{
		cairo_dock_load_image_buffer (&s_pNoInputButtonBuffer,
			GLDI_SHARE_DATA_DIR"/icons/no-input-desklet.png",
			myDeskletsParam.iDeskletButtonSize,
			myDeskletsParam.iDeskletButtonSize,
			CAIRO_DOCK_FILL_SPACE);
	}
}

static void _unload_desklet_buttons (void)
{
	cairo_dock_unload_image_buffer (&s_pRotateButtonBuffer);
	cairo_dock_unload_image_buffer (&s_pRetachButtonBuffer);
	cairo_dock_unload_image_buffer (&s_pDepthRotateButtonBuffer);
	cairo_dock_unload_image_buffer (&s_pNoInputButtonBuffer);
}


  ///////////////
 /// DRAWING ///
///////////////

static inline double _compute_zoom_for_rotation (CairoDesklet *pDesklet)
{
	double w = pDesklet->container.iWidth/2, h = pDesklet->container.iHeight/2;
	double alpha = atan2 (h, w);
	double theta = fabs (pDesklet->fRotation);
	if (theta > G_PI/2)
		theta -= G_PI/2;
	
	double d = sqrt (w * w + h * h);
	double xmax = d * MAX (fabs (cos (alpha + theta)), fabs (cos (alpha - theta)));
	double ymax = d * MAX (fabs (sin (alpha + theta)), fabs (sin (alpha - theta)));
	double fZoom = MIN (w / xmax, h / ymax);
	return fZoom;
}

static void _render_desklet_cairo (CairoDesklet *pDesklet, cairo_t *pCairoContext)
{
	cairo_save (pCairoContext);
	gboolean bUseDefaultColors = pDesklet->bUseDefaultColors;
	
	if (pDesklet->container.fRatio != 1)
	{
		//g_print (" desklet zoom : %.2f (%dx%d)\n", pDesklet->container.fRatio, pDesklet->container.iWidth, pDesklet->container.iHeight);
		cairo_translate (pCairoContext,
			pDesklet->container.iWidth * (1 - pDesklet->container.fRatio)/2,
			pDesklet->container.iHeight * (1 - pDesklet->container.fRatio)/2);
		cairo_scale (pCairoContext, pDesklet->container.fRatio, pDesklet->container.fRatio);
	}
	
	if (fabs (pDesklet->fRotation) > ANGLE_MIN)
	{
		double fZoom = _compute_zoom_for_rotation (pDesklet);
		
		cairo_translate (pCairoContext,
			.5*pDesklet->container.iWidth,
			.5*pDesklet->container.iHeight);
		
		cairo_rotate (pCairoContext, pDesklet->fRotation);
		
		cairo_scale (pCairoContext, fZoom, fZoom);
		
		cairo_translate (pCairoContext,
			-.5*pDesklet->container.iWidth,
			-.5*pDesklet->container.iHeight);
	}
	
	if (bUseDefaultColors)
	{
		cairo_save (pCairoContext);
		cairo_dock_draw_rounded_rectangle (pCairoContext, myStyleParam.iCornerRadius, myStyleParam.iLineWidth, pDesklet->container.iWidth - 2 * (myStyleParam.iCornerRadius + myStyleParam.iLineWidth), pDesklet->container.iHeight - myStyleParam.iLineWidth);
		gldi_style_colors_set_bg_color (pCairoContext);
		cairo_fill_preserve (pCairoContext);
		gldi_style_colors_set_line_color (pCairoContext);
		cairo_set_line_width (pCairoContext, myStyleParam.iLineWidth);
		cairo_stroke (pCairoContext);
		cairo_restore (pCairoContext);
	}
	else if (pDesklet->backGroundImageBuffer.pSurface != NULL)
	{
		cairo_dock_apply_image_buffer_surface (&pDesklet->backGroundImageBuffer, pCairoContext);
	}
	
	cairo_save (pCairoContext);
	if (pDesklet->iLeftSurfaceOffset != 0 || pDesklet->iTopSurfaceOffset != 0 || pDesklet->iRightSurfaceOffset != 0 || pDesklet->iBottomSurfaceOffset != 0)
	{
		cairo_translate (pCairoContext, pDesklet->iLeftSurfaceOffset, pDesklet->iTopSurfaceOffset);
		cairo_scale (pCairoContext,
			1. - (double)(pDesklet->iLeftSurfaceOffset + pDesklet->iRightSurfaceOffset) / pDesklet->container.iWidth,
			1. - (double)(pDesklet->iTopSurfaceOffset + pDesklet->iBottomSurfaceOffset) / pDesklet->container.iHeight);
	}
	
	if (pDesklet->pRenderer != NULL && pDesklet->pRenderer->render != NULL)  // un moteur de rendu specifique a ete fourni.
	{
		pDesklet->pRenderer->render (pCairoContext, pDesklet);
	}
	cairo_restore (pCairoContext);
	
	if (pDesklet->foreGroundImageBuffer.pSurface != NULL)
	{
		cairo_dock_apply_image_buffer_surface (&pDesklet->foreGroundImageBuffer, pCairoContext);
	}
	
	if (! pDesklet->rotating)  // si on est en train de tourner, les boutons suivent le mouvement, sinon ils sont dans les coins.
	{
		cairo_restore (pCairoContext);
		cairo_save (pCairoContext);
	}
	if ((pDesklet->container.bInside || pDesklet->rotating || pDesklet->fButtonsAlpha != 0) && cairo_dock_desklet_is_free (pDesklet))
	{
		if (s_pRotateButtonBuffer.pSurface != NULL)
		{
			cairo_dock_apply_image_buffer_surface_with_offset (&s_pRotateButtonBuffer, pCairoContext,
				0., 0., pDesklet->fButtonsAlpha);
		}
		if (s_pRetachButtonBuffer.pSurface != NULL && g_pMainDock)
		{
			cairo_dock_apply_image_buffer_surface_with_offset (&s_pRetachButtonBuffer, pCairoContext,
				pDesklet->container.iWidth - s_pRetachButtonBuffer.iWidth, 0., pDesklet->fButtonsAlpha);
		}
	}
	if ((pDesklet->container.bInside || pDesklet->bNoInput || pDesklet->fButtonsAlpha) && s_pNoInputButtonBuffer.pSurface != NULL && pDesklet->bAllowNoClickable)
	{
		cairo_dock_apply_image_buffer_surface_with_offset (&s_pNoInputButtonBuffer, pCairoContext,
			pDesklet->container.iWidth - s_pNoInputButtonBuffer.iWidth, pDesklet->container.iHeight - s_pNoInputButtonBuffer.iHeight, _no_input_button_alpha (pDesklet));
	}
	cairo_restore (pCairoContext);
}

static inline void _set_desklet_matrix (CairoDesklet *pDesklet)
{
	double fDepthRotationY = (fabs (pDesklet->fDepthRotationY) > ANGLE_MIN ? pDesklet->fDepthRotationY : 0.);
	double fDepthRotationX = (fabs (pDesklet->fDepthRotationX) > ANGLE_MIN ? pDesklet->fDepthRotationX : 0.);
	glTranslatef (0., 0., -pDesklet->container.iHeight * sqrt(3)/2 - 
		.45 * MAX (pDesklet->container.iWidth * fabs (sin (fDepthRotationY)),
			pDesklet->container.iHeight * fabs (sin (fDepthRotationX)))
		);  // avec 60 deg de perspective
	
	if (pDesklet->container.fRatio != 1)
	{
		glScalef (pDesklet->container.fRatio, pDesklet->container.fRatio, 1.);
	}
	
	if (fabs (pDesklet->fRotation) > ANGLE_MIN)
	{
		double fZoom = _compute_zoom_for_rotation (pDesklet);
		glScalef (fZoom, fZoom, 1.);
		glRotatef (- pDesklet->fRotation / G_PI * 180., 0., 0., 1.);
	}
	
	if (fDepthRotationY != 0)
	{
		glRotatef (- pDesklet->fDepthRotationY / G_PI * 180., 0., 1., 0.);
	}
	
	if (fDepthRotationX != 0)
	{
		glRotatef (- pDesklet->fDepthRotationX / G_PI * 180., 1., 0., 0.);
	}
}

static void _render_desklet_opengl (CairoDesklet *pDesklet)
{
	gboolean bUseDefaultColors = pDesklet->bUseDefaultColors;
	// note: this is called after gldi_gl_container_begin_draw_full() which called glScalef() with the 
	// display scale factor; however, for some reason this is not necessary in this case, so we reset it
	glLoadIdentity ();
	glPushMatrix ();
	///glTranslatef (0*pDesklet->container.iWidth/2, 0*pDesklet->container.iHeight/2, 0.);  // avec une perspective ortho.
	///glTranslatef (0*pDesklet->container.iWidth/2, 0*pDesklet->container.iHeight/2, -pDesklet->container.iWidth*(1.87 +.35*fabs (sin(pDesklet->fDepthRotationY))));  // avec 30 deg de perspective
	_set_desklet_matrix (pDesklet);
	
	if (bUseDefaultColors)
	{
		_cairo_dock_set_blend_alpha ();
		gldi_style_colors_set_bg_color (NULL);
		cairo_dock_draw_rounded_rectangle_opengl (pDesklet->container.iWidth - 2 * (myStyleParam.iCornerRadius + myStyleParam.iLineWidth), pDesklet->container.iHeight - 2*myStyleParam.iLineWidth, myStyleParam.iCornerRadius, 0, NULL);
		
		gldi_style_colors_set_line_color (NULL);
		cairo_dock_draw_rounded_rectangle_opengl (pDesklet->container.iWidth - 2 * (myStyleParam.iCornerRadius + myStyleParam.iLineWidth), pDesklet->container.iHeight - 2*myStyleParam.iLineWidth, myStyleParam.iCornerRadius, myStyleParam.iLineWidth, NULL);
	}
	
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_pbuffer ();
	_cairo_dock_set_alpha (1.);
	if (pDesklet->backGroundImageBuffer.iTexture != 0)
	{
		cairo_dock_apply_image_buffer_texture (&pDesklet->backGroundImageBuffer);
	}
	
	glPushMatrix ();
	if (pDesklet->iLeftSurfaceOffset != 0 || pDesklet->iTopSurfaceOffset != 0 || pDesklet->iRightSurfaceOffset != 0 || pDesklet->iBottomSurfaceOffset != 0)
	{
		glTranslatef ((pDesklet->iLeftSurfaceOffset - pDesklet->iRightSurfaceOffset)/2, (pDesklet->iBottomSurfaceOffset - pDesklet->iTopSurfaceOffset)/2, 0.);
		glScalef (1. - (double)(pDesklet->iLeftSurfaceOffset + pDesklet->iRightSurfaceOffset) / pDesklet->container.iWidth,
			1. - (double)(pDesklet->iTopSurfaceOffset + pDesklet->iBottomSurfaceOffset) / pDesklet->container.iHeight,
			1.);
	}
	
	if (pDesklet->pRenderer != NULL && pDesklet->pRenderer->render_opengl != NULL)  // un moteur de rendu specifique a ete fourni.
	{
		pDesklet->pRenderer->render_opengl (pDesklet);
	}
	glPopMatrix ();
	
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_pbuffer ();
	if (pDesklet->foreGroundImageBuffer.iTexture != 0)
	{
		cairo_dock_apply_image_buffer_texture (&pDesklet->foreGroundImageBuffer);
	}
	
	//if (pDesklet->container.bInside && cairo_dock_desklet_is_free (pDesklet))
	{
		if (! pDesklet->rotating && ! pDesklet->rotatingY && ! pDesklet->rotatingX)
		{
			glPopMatrix ();
			glPushMatrix ();
			glTranslatef (0., 0., -pDesklet->container.iHeight*(sqrt(3)/2));
		}
	}
	
	if ((pDesklet->container.bInside || pDesklet->fButtonsAlpha != 0 || pDesklet->rotating || pDesklet->rotatingY || pDesklet->rotatingX) && cairo_dock_desklet_is_free (pDesklet))
	{
		_cairo_dock_set_blend_alpha ();
		_cairo_dock_set_alpha (sqrt(pDesklet->fButtonsAlpha));
		if (s_pRotateButtonBuffer.iTexture != 0)
		{
			cairo_dock_apply_image_buffer_texture_with_offset (&s_pRotateButtonBuffer,
				-pDesklet->container.iWidth/2 + s_pRotateButtonBuffer.iWidth/2,
				pDesklet->container.iHeight/2 - s_pRotateButtonBuffer.iHeight/2);
		}
		if (s_pRetachButtonBuffer.iTexture != 0 && g_pMainDock)
		{
			cairo_dock_apply_image_buffer_texture_with_offset (&s_pRetachButtonBuffer,
				pDesklet->container.iWidth/2 - s_pRetachButtonBuffer.iWidth/2,
				pDesklet->container.iHeight/2 - s_pRetachButtonBuffer.iHeight/2);
		}
		if (s_pDepthRotateButtonBuffer.iTexture != 0)
		{
			cairo_dock_apply_image_buffer_texture_with_offset (&s_pDepthRotateButtonBuffer,
				0.,
				pDesklet->container.iHeight/2 - s_pDepthRotateButtonBuffer.iHeight/2);
			
			glPushMatrix ();
			glRotatef (90., 0., 0., 1.);
			cairo_dock_apply_image_buffer_texture_with_offset (&s_pDepthRotateButtonBuffer,
				0.,
				pDesklet->container.iWidth/2 - s_pDepthRotateButtonBuffer.iHeight/2);
			glPopMatrix ();
		}
	}
	if ((pDesklet->container.bInside || pDesklet->fButtonsAlpha != 0 || pDesklet->bNoInput) && s_pNoInputButtonBuffer.iTexture != 0 && pDesklet->bAllowNoClickable)
	{
		_cairo_dock_set_blend_alpha ();
		_cairo_dock_set_alpha (_no_input_button_alpha(pDesklet));
		cairo_dock_apply_image_buffer_texture_with_offset (&s_pNoInputButtonBuffer,
			pDesklet->container.iWidth/2 - s_pNoInputButtonBuffer.iWidth/2,
			- pDesklet->container.iHeight/2 + s_pNoInputButtonBuffer.iHeight/2);
	}
	
	_cairo_dock_disable_texture ();
	glPopMatrix ();
}

static gboolean _on_render_desklet_notification (G_GNUC_UNUSED gpointer pUserData, CairoDesklet *pDesklet, cairo_t *pCairoContext)
{
	if (pCairoContext != NULL)
		_render_desklet_cairo (pDesklet, pCairoContext);
	else
		_render_desklet_opengl (pDesklet);
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_enter_leave_desklet_notification (G_GNUC_UNUSED gpointer data, CairoDesklet *pDesklet, gboolean *bStartAnimation)
{
	pDesklet->bButtonsApparition = TRUE;
	*bStartAnimation = TRUE;
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_update_desklet_notification (G_GNUC_UNUSED gpointer data, CairoDesklet *pDesklet, gboolean *bContinueAnimation)
{
	if (!pDesklet->bButtonsApparition && !pDesklet->bGrowingUp)
		return GLDI_NOTIFICATION_LET_PASS;
	
	if (pDesklet->bButtonsApparition)
	{
		pDesklet->fButtonsAlpha += (pDesklet->container.bInside ? .1 : -.1);
		//g_print ("fButtonsAlpha <- %.2f\n", pDesklet->fButtonsAlpha);
		
		if (pDesklet->fButtonsAlpha <= 0 || pDesklet->fButtonsAlpha >= 1)
		{
			pDesklet->bButtonsApparition = FALSE;
			if (pDesklet->fButtonsAlpha < 0)
				pDesklet->fButtonsAlpha = 0.;
			else if (pDesklet->fButtonsAlpha > 1)
				pDesklet->fButtonsAlpha = 1.;
		}
		else
		{
			*bContinueAnimation = TRUE;
		}
	}
	
	if (pDesklet->bGrowingUp)
	{
		pDesklet->container.fRatio += .04;
		//g_print ("pDesklet->container.fRatio:%.2f\n", pDesklet->container.fRatio);
		
		if (pDesklet->container.fRatio >= 1.1)  // la derniere est a x1.1
		{
			pDesklet->container.fRatio = 1;
			pDesklet->bGrowingUp = FALSE;
		}
		else
		{
			*bContinueAnimation = TRUE;
		}
	}
	
	gtk_widget_queue_draw (pDesklet->container.pWidget);
	return GLDI_NOTIFICATION_LET_PASS;
}

static Icon *_cairo_dock_pick_icon_on_opengl_desklet (CairoDesklet *pDesklet)
{
	GLuint selectBuf[4];
	GLint hits=0;
	GLint viewport[4];
	
	if (! gldi_gl_container_make_current (CAIRO_CONTAINER (pDesklet)))
		return NULL;
	
	glGetIntegerv (GL_VIEWPORT, viewport);
	glSelectBuffer (4, selectBuf);
	
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);
	
	GdkWindow* gdkwindow = gldi_container_get_gdk_window (CAIRO_CONTAINER(pDesklet));
	gint scale = gdk_window_get_scale_factor (gdkwindow);
	
	glMatrixMode (GL_PROJECTION);
	glPushMatrix ();
	glLoadIdentity ();
	gluPickMatrix ((GLdouble) pDesklet->container.iMouseX * scale, (GLdouble) (viewport[3] - pDesklet->container.iMouseY * scale), 2.0, 2.0, viewport);
	gluPerspective (60.0, 1.0*(GLfloat)pDesklet->container.iWidth/(GLfloat)pDesklet->container.iHeight, 1., 4*pDesklet->container.iHeight);
	
	glMatrixMode (GL_MODELVIEW);
	glPushMatrix ();
	glLoadIdentity ();
	
	_set_desklet_matrix (pDesklet);
	
	if (pDesklet->iLeftSurfaceOffset != 0 || pDesklet->iTopSurfaceOffset != 0 || pDesklet->iRightSurfaceOffset != 0 || pDesklet->iBottomSurfaceOffset != 0)
	{
		glTranslatef ((pDesklet->iLeftSurfaceOffset - pDesklet->iRightSurfaceOffset)/2, (pDesklet->iBottomSurfaceOffset - pDesklet->iTopSurfaceOffset)/2, 0.);
		glScalef (1. - (double)(pDesklet->iLeftSurfaceOffset + pDesklet->iRightSurfaceOffset) / pDesklet->container.iWidth,
			1. - (double)(pDesklet->iTopSurfaceOffset + pDesklet->iBottomSurfaceOffset) / pDesklet->container.iHeight,
			1.);
	}
	
	glPolygonMode (GL_FRONT, GL_FILL);
	glColor4f (1., 1., 1., 1.);
	
	pDesklet->iPickedObject = 0;
	if (pDesklet->render_bounding_box != NULL)  // surclasse la fonction du moteur de rendu.
	{
		pDesklet->render_bounding_box (pDesklet);
	}
	else if (pDesklet->pRenderer && pDesklet->pRenderer->render_bounding_box != NULL)
	{
		pDesklet->pRenderer->render_bounding_box (pDesklet);
	}
	else  // on le fait nous-memes a partir des coordonnees des icones.
	{
		glTranslatef (-pDesklet->container.iWidth/2, -pDesklet->container.iHeight/2, 0.);
		
		double x, y, w, h;
		Icon *pIcon;
		
		pIcon = pDesklet->pIcon;
		if (pIcon != NULL && pIcon->image.iTexture != 0)
		{
			w = pIcon->fWidth/2;
			h = pIcon->fHeight/2;
			x = pIcon->fDrawX + w;
			y = pDesklet->container.iHeight - pIcon->fDrawY - h;
			
			glLoadName(pIcon->image.iTexture);
			
			glBegin(GL_QUADS);
			glVertex3f(x-w, y+h, 0.);
			glVertex3f(x+w, y+h, 0.);
			glVertex3f(x+w, y-h, 0.);
			glVertex3f(x-w, y-h, 0.);
			glEnd();
		}
		
		GList *ic;
		for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->image.iTexture == 0)
				continue;
			
			w = pIcon->fWidth/2;
			h = pIcon->fHeight/2;
			x = pIcon->fDrawX + w;
			y = pDesklet->container.iHeight - pIcon->fDrawY - h;
			
			glLoadName(pIcon->image.iTexture);
			
			glBegin(GL_QUADS);
			glVertex3f(x-w, y+h, 0.);
			glVertex3f(x+w, y+h, 0.);
			glVertex3f(x+w, y-h, 0.);
			glVertex3f(x-w, y-h, 0.);
			glEnd();
		}
	}
	
	glPopName();
	
	hits = glRenderMode (GL_RENDER);

	glMatrixMode (GL_PROJECTION);
	glPopMatrix ();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix ();
	
	Icon *pFoundIcon = NULL;
	if (hits != 0)
	{
		GLuint id = selectBuf[3];
		Icon *pIcon;
		
		if (pDesklet->render_bounding_box != NULL)
		{
			pDesklet->iPickedObject = id;
			//g_print ("iPickedObject <- %d\n", id);
			pFoundIcon = pDesklet->pIcon;  // il faut mettre qqch, sinon la notification est filtree par la macro CD_APPLET_ON_CLICK_BEGIN.
		}
		else
		{
			pIcon = pDesklet->pIcon;
			if (pIcon != NULL && pIcon->image.iTexture != 0)
			{
				if (pIcon->image.iTexture == id)
				{
					pFoundIcon = pIcon;
				}
			}
			
			if (pFoundIcon == NULL)
			{
				GList *ic;
				for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
				{
					pIcon = ic->data;
					if (pIcon->image.iTexture == id)
					{
						pFoundIcon = pIcon;
						break ;
					}
				}
			}
		}
	}
	
	return pFoundIcon;
}
Icon *gldi_desklet_find_clicked_icon (CairoDesklet *pDesklet)
{
	if (g_bUseOpenGL && pDesklet->pRenderer && pDesklet->pRenderer->render_opengl)
	{
		return _cairo_dock_pick_icon_on_opengl_desklet (pDesklet);
	}
	
	int iMouseX = pDesklet->container.iMouseX, iMouseY = pDesklet->container.iMouseY;
	if (fabs (pDesklet->fRotation) > ANGLE_MIN)
	{
		//g_print (" clic en (%d;%d) rotations : %.2frad\n", iMouseX, iMouseY, pDesklet->fRotation);
		double x, y;  // par rapport au centre du desklet.
		x = iMouseX - pDesklet->container.iWidth/2;
		y = pDesklet->container.iHeight/2 - iMouseY;
		
		double r, t;  // coordonnees polaires.
		r = sqrt (x*x + y*y);
		t = atan2 (y, x);
		
		double z = _compute_zoom_for_rotation (pDesklet);
		r /= z;
		
		x = r * cos (t + pDesklet->fRotation);  // la rotation de cairo est dans le sene horaire.
		y = r * sin (t + pDesklet->fRotation);
		
		iMouseX = x + pDesklet->container.iWidth/2;
		iMouseY = pDesklet->container.iHeight/2 - y;
		//g_print (" => (%d;%d)\n", iMouseX, iMouseY);
	}
	pDesklet->iMouseX2d = iMouseX;
	pDesklet->iMouseY2d = iMouseY;
	
	Icon *icon = pDesklet->pIcon;
	g_return_val_if_fail (icon != NULL, NULL);  // peut arriver au tout debut, car on associe l'icone au desklet _apres_ l'avoir cree, et on fait tourner la gtk_main entre-temps (pour le redessiner invisible).
	if (icon->fDrawX < iMouseX && icon->fDrawX + icon->fWidth * icon->fScale > iMouseX && icon->fDrawY < iMouseY && icon->fDrawY + icon->fHeight * icon->fScale > iMouseY)
	{
		return icon;
	}
	
	if (pDesklet->icons != NULL)
	{
		GList* ic;
		for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			if (icon->fDrawX < iMouseX && icon->fDrawX + icon->fWidth * icon->fScale > iMouseX && icon->fDrawY < iMouseY && icon->fDrawY + icon->fHeight * icon->fScale > iMouseY)
			{
				return icon;
			}
		}
	}
	return NULL;
}


  ///////////////
 /// MANAGER ///
///////////////

CairoDesklet *gldi_desklets_foreach (GldiDeskletForeachFunc pCallback, gpointer user_data)
{
	CairoDesklet *pDesklet;
	GList *dl;
	for (dl = s_pDeskletList; dl != NULL; dl = dl->next)
	{
		pDesklet = dl->data;
		if (pCallback (pDesklet, user_data))
			return pDesklet;
	}
	
	return NULL;
}

static gboolean _foreach_icons_in_desklet (CairoDesklet *pDesklet, gpointer *data)
{
	GldiIconFunc pFunction = data[0];
	gpointer pUserData = data[1];
	if (pDesklet->pIcon != NULL)
		pFunction (pDesklet->pIcon, pUserData);
	GList *ic;
	for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
	{
		pFunction ((Icon*)ic->data, pUserData);
	}
	return FALSE;
}
void gldi_desklets_foreach_icons (GldiIconFunc pFunction, gpointer pUserData)
{
	gpointer data[2] = {pFunction, pUserData};
	gldi_desklets_foreach ((GldiDeskletForeachFunc) _foreach_icons_in_desklet, data);
}


static void _set_one_desklet_visible (CairoDesklet *pDesklet, gpointer data)
{
	gboolean bOnWidgetLayerToo = GPOINTER_TO_INT (data);
	gboolean bIsOnWidgetLayer = (pDesklet->iVisibility == CAIRO_DESKLET_ON_WIDGET_LAYER);
	if (bOnWidgetLayerToo || ! bIsOnWidgetLayer)
	{
		if (bIsOnWidgetLayer)  // on le passe sur la couche visible.
			gldi_desktop_set_on_widget_layer (CAIRO_CONTAINER (pDesklet), FALSE);
		
		gtk_window_set_keep_below (GTK_WINDOW (pDesklet->container.pWidget), FALSE);
		
		gldi_desklet_show (pDesklet);
	}
}
void gldi_desklets_set_visible (gboolean bOnWidgetLayerToo)
{
	cd_debug ("%s (%d)", __func__, bOnWidgetLayerToo);
	CairoDesklet *pDesklet;
	GList *dl;
	for (dl = s_pDeskletList; dl != NULL; dl = dl->next)
	{
		pDesklet = dl->data;
		_set_one_desklet_visible (pDesklet, GINT_TO_POINTER (bOnWidgetLayerToo));
	}
}

static void _set_one_desklet_visibility_to_default (CairoDesklet *pDesklet, CairoDockMinimalAppletConfig *pMinimalConfig)
{
	if (pDesklet->pIcon != NULL)
	{
		GldiModuleInstance *pInstance = pDesklet->pIcon->pModuleInstance;
		GKeyFile *pKeyFile = gldi_module_instance_open_conf_file (pInstance, pMinimalConfig);
		g_key_file_free (pKeyFile);
		
		gldi_desklet_set_accessibility (pDesklet, pMinimalConfig->deskletAttribute.iVisibility, FALSE);
	}
	pDesklet->bAllowMinimize = FALSE;  /// utile ?...
}
void gldi_desklets_set_visibility_to_default (void)
{
	CairoDockMinimalAppletConfig minimalConfig;
	CairoDesklet *pDesklet;
	GList *dl;
	for (dl = s_pDeskletList; dl != NULL; dl = dl->next)
	{
		pDesklet = dl->data;
		_set_one_desklet_visibility_to_default (pDesklet, &minimalConfig);
	}
}


gboolean gldi_desklet_manager_is_ready (void)
{
	static gboolean bReady = FALSE;
	if (!bReady)  // once we are ready, no need to test it again.
	{
		bReady = (time (NULL) > s_iStartupTime + 5);  // 5s delay on startup
	}
	return bReady;
}

static gboolean _on_desktop_geometry_changed (G_GNUC_UNUSED gpointer data)
{
	// when the screen size changes, X will send a 'configure' event to each windows (because size and position might have changed), which will be interpreted by the desklet as a user displacement.
	// we must therefore replace the desklets at their correct position in the new desktop space.
	CairoDesklet *pDesklet;
	CairoDockMinimalAppletConfig *pMinimalConfig;
	GList *dl;
	for (dl = s_pDeskletList; dl != NULL; dl = dl->next)
	{
		pDesklet = dl->data;
		//g_print ("%s : %d;%d\n", pDesklet->pIcon->pModuleInstance->pModule->pVisitCard->cModuleName, pDesklet->container.iWindowPositionX, pDesklet->container.iWindowPositionY);
		
		if (CAIRO_DOCK_IS_APPLET (pDesklet->pIcon))
		{
			// get the position (and other parameters) as defined by the user.
			pMinimalConfig = g_new0 (CairoDockMinimalAppletConfig, 1);
			GKeyFile *pKeyFile = gldi_module_instance_open_conf_file (pDesklet->pIcon->pModuleInstance, pMinimalConfig);
			//g_print ("  %d;%d\n", pMinimalConfig->deskletAttribute.iDeskletPositionX, pMinimalConfig->deskletAttribute.iDeskletPositionY);
		
			// apply the settings to the desklet (actually we only need the position).
			gldi_desklet_configure (pDesklet, &pMinimalConfig->deskletAttribute);
			gldi_module_instance_free_generic_config (pMinimalConfig);
			g_key_file_free (pKeyFile);
		}
	}
	return GLDI_NOTIFICATION_LET_PASS;
}


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoDeskletsParam *pDesklets)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	// register an "automatic" decoration, that takes its colors from the global style (do it now, after the global style has been defined)
	CairoDeskletDecoration * pDecoration = cairo_dock_get_desklet_decoration ("automatic");
	if (pDecoration == NULL)
	{
		pDecoration = g_new0 (CairoDeskletDecoration, 1);
		pDecoration->cDisplayedName = _("Automatic");
		pDecoration->iLeftMargin = pDecoration->iTopMargin = pDecoration->iRightMargin = pDecoration->iBottomMargin = myStyleParam.iLineWidth;
		pDecoration->fBackGroundAlpha = 1.;
		pDecoration->cBackGroundImagePath = g_strdup ("automatic");  // keyword to say we use global style colors rather an image
		cairo_dock_register_desklet_decoration ("automatic", pDecoration);  // we don't actually load an Imagebuffer, 
	}
	
	// register a "custom" decoration, that takes its colors from the config if the use has defined it
	pDesklets->cDeskletDecorationsName = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "decorations", &bFlushConfFileNeeded, NULL, NULL, NULL);
	CairoDeskletDecoration *pUserDeskletDecorations = cairo_dock_get_desklet_decoration ("personnal");
	if (pUserDeskletDecorations == NULL)
	{
		pUserDeskletDecorations = g_new0 (CairoDeskletDecoration, 1);
		pUserDeskletDecorations->cDisplayedName = _("_custom decoration_");
		cairo_dock_register_desklet_decoration ("personnal", pUserDeskletDecorations);
	}
	if (pDesklets->cDeskletDecorationsName != NULL && strcmp (pDesklets->cDeskletDecorationsName, "personnal") == 0)
	{
		g_free (pUserDeskletDecorations->cBackGroundImagePath);
		pUserDeskletDecorations->cBackGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "bg desklet", &bFlushConfFileNeeded, NULL, NULL, NULL);
		g_free (pUserDeskletDecorations->cForeGroundImagePath);
		pUserDeskletDecorations->cForeGroundImagePath = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "fg desklet", &bFlushConfFileNeeded, NULL, NULL, NULL);
		pUserDeskletDecorations->iLoadingModifier = CAIRO_DOCK_FILL_SPACE;
		pUserDeskletDecorations->fBackGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklets", "bg alpha", &bFlushConfFileNeeded, 1.0, NULL, NULL);
		pUserDeskletDecorations->fForeGroundAlpha = cairo_dock_get_double_key_value (pKeyFile, "Desklets", "fg alpha", &bFlushConfFileNeeded, 1.0, NULL, NULL);
		pUserDeskletDecorations->iLeftMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "left offset", &bFlushConfFileNeeded, 0, NULL, NULL);
		pUserDeskletDecorations->iTopMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "top offset", &bFlushConfFileNeeded, 0, NULL, NULL);
		pUserDeskletDecorations->iRightMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "right offset", &bFlushConfFileNeeded, 0, NULL, NULL);
		pUserDeskletDecorations->iBottomMargin = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "bottom offset", &bFlushConfFileNeeded, 0, NULL, NULL);
	}
	
	pDesklets->iDeskletButtonSize = cairo_dock_get_integer_key_value (pKeyFile, "Desklets", "button size", &bFlushConfFileNeeded, 16, NULL, NULL);
	pDesklets->cRotateButtonImage = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "rotate image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	pDesklets->cRetachButtonImage = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "retach image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	pDesklets->cDepthRotateButtonImage = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "depth rotate image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	pDesklets->cNoInputButtonImage = cairo_dock_get_string_key_value (pKeyFile, "Desklets", "no input image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	return bFlushConfFileNeeded;
}

  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (CairoDeskletsParam *pDesklets)
{
	g_free (pDesklets->cDeskletDecorationsName);
	g_free (pDesklets->cRotateButtonImage);
	g_free (pDesklets->cRetachButtonImage);
	g_free (pDesklets->cDepthRotateButtonImage);
	g_free (pDesklets->cNoInputButtonImage);
}

  //////////////
 /// RELOAD ///
//////////////

static void reload (CairoDeskletsParam *pPrevDesklets, CairoDeskletsParam *pDesklets)
{
	if (g_strcmp0 (pPrevDesklets->cRotateButtonImage, pDesklets->cRotateButtonImage) != 0
	|| g_strcmp0 (pPrevDesklets->cRetachButtonImage, pDesklets->cRetachButtonImage) != 0
	|| g_strcmp0 (pPrevDesklets->cDepthRotateButtonImage, pDesklets->cDepthRotateButtonImage) != 0
	|| g_strcmp0 (pPrevDesklets->cNoInputButtonImage, pDesklets->cNoInputButtonImage) != 0)
	{
		_unload_desklet_buttons ();
		_load_desklet_buttons ();
	}
	if (g_strcmp0 (pPrevDesklets->cDeskletDecorationsName, pDesklets->cDeskletDecorationsName) != 0)  // the default theme has changed -> reload all desklets that use it
	{
		CairoDesklet *pDesklet;
		GList *dl;
		for (dl = s_pDeskletList; dl != NULL; dl = dl->next)
		{
			pDesklet = dl->data;
			if (pDesklet->cDecorationTheme == NULL || strcmp (pDesklet->cDecorationTheme, "default") == 0)
			{
				gldi_desklet_load_desklet_decorations (pDesklet);
			}
		}
	}
}

  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	_unload_desklet_buttons ();
}

  ////////////
 /// INIT ///
////////////

static gboolean on_style_changed (G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Desklets: style change to %s", myDeskletsParam.cDeskletDecorationsName);
	gboolean bUseDefaultColors = (!myDeskletsParam.cDeskletDecorationsName || strcmp (myDeskletsParam.cDeskletDecorationsName, "automatic") == 0);
	
	CairoDeskletDecoration * pDecoration = cairo_dock_get_desklet_decoration ("automatic");
	if (pDecoration)
		pDecoration->iLeftMargin = pDecoration->iTopMargin = pDecoration->iRightMargin = pDecoration->iBottomMargin = myStyleParam.iLineWidth;
	
	CairoDesklet *pDesklet;
	GList *dl;
	for (dl = s_pDeskletList; dl != NULL; dl = dl->next)
	{
		pDesklet = dl->data;
		if ( ((pDesklet->cDecorationTheme == NULL || strcmp (pDesklet->cDecorationTheme, "default") == 0) && bUseDefaultColors)
		|| strcmp (pDesklet->cDecorationTheme, "automatic") == 0)
		{
			cd_debug ("Reload desklet's bg...");
			gldi_desklet_load_desklet_decorations (pDesklet);
			cairo_dock_redraw_container (CAIRO_CONTAINER (pDesklet));
		}
	}
	return GLDI_NOTIFICATION_LET_PASS;
}

static void init (void)
{
	gldi_object_register_notification (&myDeskletObjectMgr,
		NOTIFICATION_UPDATE,
		(GldiNotificationFunc) _on_update_desklet_notification,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myDeskletObjectMgr,
		NOTIFICATION_ENTER_DESKLET,
		(GldiNotificationFunc) _on_enter_leave_desklet_notification,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myDeskletObjectMgr,
		NOTIFICATION_LEAVE_DESKLET,
		(GldiNotificationFunc) _on_enter_leave_desklet_notification,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myDeskletObjectMgr,
		NOTIFICATION_RENDER,
		(GldiNotificationFunc) _on_render_desklet_notification,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myDesktopMgr,
		NOTIFICATION_DESKTOP_GEOMETRY_CHANGED,
		(GldiNotificationFunc) _on_desktop_geometry_changed,
		GLDI_RUN_AFTER, NULL);  // replace all desklets that are positionned relatively to the right or bottom edge
	gldi_object_register_notification (&myStyleMgr,
		NOTIFICATION_STYLE_CHANGED,
		(GldiNotificationFunc) on_style_changed,
		GLDI_RUN_AFTER, NULL);
	s_iStartupTime = time (NULL);  // on startup, the WM can take a long time before it has positionned all the desklets. To avoid irrelevant configure events, we set a delay.
}

  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	CairoDesklet *pDesklet = (CairoDesklet*)obj;
	CairoDeskletAttr *pAttributes = (CairoDeskletAttr*)attr;
	g_return_if_fail (pAttributes->pIcon != NULL);
	
	gldi_desklet_init_internals (pDesklet);
	
	// attach the main icon
	Icon *pIcon = pAttributes->pIcon;
	pDesklet->pIcon = pIcon;
	cairo_dock_set_icon_container (pIcon, pDesklet);
	if (CAIRO_DOCK_IS_APPLET (pIcon))
	{
		if (gldi_container_is_wayland_backend ())
		{
			char *tmp = g_strdup_printf ("cairo-dock-desklet-%s", pIcon->pModuleInstance->pModule->pVisitCard->cModuleName);
			gtk_window_set_title (GTK_WINDOW (pDesklet->container.pWidget), tmp);
			g_free (tmp);
		}
		else
			gtk_window_set_title (GTK_WINDOW (pDesklet->container.pWidget), pIcon->pModuleInstance->pModule->pVisitCard->cModuleName);
	}
	
	// configure the desklet
	gldi_desklet_configure (pDesklet, pAttributes);
	
	// load buttons images
	if (s_pRotateButtonBuffer.pSurface == NULL)
	{
		_load_desklet_buttons ();
	}
	
	// register the new desklet
	s_pDeskletList = g_list_prepend (s_pDeskletList, pDesklet);
	
	// start the appearance animation
	if (! cairo_dock_is_loading ())
	{
		pDesklet->container.fRatio = 0.1;
		pDesklet->bGrowingUp = TRUE;
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDesklet));
	}
}

static void reset_object (GldiObject *obj)
{
	CairoDesklet *pDesklet = (CairoDesklet*)obj;
	
	// stop timers
	if (pDesklet->iSidWriteSize != 0)
		g_source_remove (pDesklet->iSidWriteSize);
	if (pDesklet->iSidWritePosition != 0)
		g_source_remove (pDesklet->iSidWritePosition);
	
	// detach the main icon
	Icon *pIcon = pDesklet->pIcon;
	if (pIcon != NULL)
	{
		if (pIcon->pContainer != NULL && pIcon->pContainer != CAIRO_CONTAINER (pDesklet))  // shouldn't happen
			cd_warning ("This icon (%s) has not been detached from its desklet properly !", pIcon->cName);
		else
			cairo_dock_set_icon_container (pIcon, NULL);
	}
	
	// detach the interactive widget
	gldi_desklet_steal_interactive_widget (pDesklet);
	
	// free sub-icons
	if (pDesklet->icons != NULL)
	{
		GList *icons = pDesklet->icons;
		pDesklet->icons = NULL;
		g_list_free_full (icons, (GDestroyNotify)gldi_object_unref);
	}
	
	// free data
	if (pDesklet->pRenderer != NULL)
	{
		if (pDesklet->pRenderer->free_data != NULL)
		{
			pDesklet->pRenderer->free_data (pDesklet);
			pDesklet->pRendererData = NULL;
		}
	}
	
	g_free (pDesklet->cDecorationTheme);
	gldi_desklet_decoration_free (pDesklet->pUserDecoration);
	
	cairo_dock_unload_image_buffer (&pDesklet->backGroundImageBuffer);
	cairo_dock_unload_image_buffer (&pDesklet->foreGroundImageBuffer);
	
	// unregister the desklet
	s_pDeskletList = g_list_remove (s_pDeskletList, pDesklet);
}

void gldi_register_desklets_manager (void)
{
	// Manager
	memset (&myDeskletsMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myDeskletsMgr), &myManagerObjectMgr, NULL);
	myDeskletsMgr.cModuleName  = "Desklets";
	// interface
	myDeskletsMgr.init         = init;
	myDeskletsMgr.load         = NULL;  // data are loaded the first time a desklet is created, to avoid create them for nothing.
	myDeskletsMgr.unload       = unload;
	myDeskletsMgr.reload       = (GldiManagerReloadFunc)reload;
	myDeskletsMgr.get_config   = (GldiManagerGetConfigFunc)get_config;
	myDeskletsMgr.reset_config = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myDeskletsParam, 0, sizeof (CairoDeskletsParam));
	myDeskletsMgr.pConfig = (GldiManagerConfigPtr)&myDeskletsParam;
	myDeskletsMgr.iSizeOfConfig = sizeof (CairoDeskletsParam);
	// data
	memset (&s_pRotateButtonBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&s_pRetachButtonBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&s_pDepthRotateButtonBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&s_pNoInputButtonBuffer, 0, sizeof (CairoDockImageBuffer));
	myDeskletsMgr.iSizeOfData = 0;
	myDeskletsMgr.pData = (GldiManagerDataPtr)NULL;
	
	// Object Manager
	memset (&myDeskletObjectMgr, 0, sizeof (GldiObjectManager));
	myDeskletObjectMgr.cName        = "Desklet";
	myDeskletObjectMgr.iObjectSize  = sizeof (CairoDesklet);
	// interface
	myDeskletObjectMgr.init_object  = init_object;
	myDeskletObjectMgr.reset_object = reset_object;
	// signals
	gldi_object_install_notifications (&myDeskletObjectMgr, NB_NOTIFICATIONS_DESKLET);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myDeskletObjectMgr), &myContainerObjectMgr);
}
