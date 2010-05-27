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

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iconv.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cairo.h>
#include <pango/pango.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#ifdef HAVE_GLITZ
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include <X11/extensions/Xrender.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#include "cairo-dock-load.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-draw-opengl.h"

#include "texture-gradation.h"
#define RADIAN (G_PI / 180.0)  // Conversion Radian/Degres
#define DELTA_ROUND_DEGREE 3

extern GLuint g_pGradationTexture[2];

extern CairoDock *g_pMainDock;

extern CairoDockImageBuffer g_pIconBackgroundImageBuffer;
extern CairoDockImageBuffer g_pVisibleZoneBuffer;

extern gboolean g_bUseOpenGL;
extern CairoDockGLConfig g_openglConfig;

extern gboolean g_bEasterEggs;

/**static void _cairo_dock_draw_appli_indicator_opengl (Icon *icon, gboolean bIsHorizontal, double fRatio, gboolean bDirectionUp)
{
	if (! myIndicators.bRotateWithDock)
		bDirectionUp = bIsHorizontal = TRUE;
	
	//\__________________ On place l'indicateur.
	double z = 1 + myIcons.fAmplitude;
	double w = g_pIndicatorBuffer.iWidth;
	double h = g_pIndicatorBuffer.iHeight;
	double dy = myIndicators.iIndicatorDeltaY / z;
	double fY;
	if (myIndicators.bLinkIndicatorWithIcon)  // il se deforme et rebondit avec l'icone.
	{
		w /= z;
		h /= z;
		fY = - icon->fHeight * icon->fScale/2
			+ (h/2 - dy) * fRatio * icon->fScale;
		
		if (bIsHorizontal)
		{
			if (! bDirectionUp)
				fY = - fY;
			glTranslatef (0., fY, 0.);
			glScalef (w * fRatio * icon->fScale * icon->fWidthFactor, (bDirectionUp ? 1:-1) * h * fRatio * icon->fScale * icon->fHeightFactor, 1.);
		}
		else
		{
			if (bDirectionUp)
				fY = - fY;
			glTranslatef (fY, 0., 0.);
			glRotatef (90, 0., 0., 1.);
			glScalef (w * fRatio * icon->fScale * icon->fWidthFactor, (bDirectionUp ? 1:-1) * h * fRatio * icon->fScale * icon->fHeightFactor, 1.);
		}
		
	}
	else  // il est fixe, en bas de l'icone.
	{
		fY = - icon->fHeight * icon->fScale/2
			+ (h/2 - dy) * fRatio;
		if (bIsHorizontal)
		{
			if (! bDirectionUp)
				fY = - fY;
			glTranslatef (0., fY, 1.);
			glScalef (w * fRatio * 1., (bDirectionUp ? 1:-1) * h * fRatio * 1., 1.);
		}
		else
		{
			if (bDirectionUp)
				fY = - fY;
			glTranslatef (fY, 0., 1.);
			glRotatef (90, 0., 0., 1.);
			glScalef (w * fRatio * 1., (bDirectionUp ? 1:-1) * h * fRatio * 1., 1.);
		}
	}

	//\__________________ On dessine l'indicateur.
	cairo_dock_draw_texture_with_alpha (g_pIndicatorBuffer.iTexture, 1., 1., 1.);
}
static void _cairo_dock_draw_active_window_indicator_opengl (Icon *icon, CairoDock *pDock, double fRatio)
{
	cairo_dock_set_icon_scale (icon, CAIRO_CONTAINER (pDock), 1.);
	
	_cairo_dock_enable_texture ();
	
	_cairo_dock_set_blend_pbuffer ();  // rend mieux que les 2 autres.
	
	_cairo_dock_apply_texture_at_size_with_alpha (g_pActiveIndicatorBuffer.iTexture, 1., 1., 1.);
	
	_cairo_dock_disable_texture ();
	
}
static void _cairo_dock_draw_class_indicator_opengl (Icon *icon, gboolean bIsHorizontal, double fRatio, gboolean bDirectionUp)
{
	if (myIndicators.bZoomClassIndicator)
		fRatio *= icon->fScale;
	double w = g_pClassIndicatorBuffer.iWidth;
	double h = g_pClassIndicatorBuffer.iHeight;
	
	if (! bIsHorizontal)
		glRotatef (90., 0., 0., 1.);
	if (! bDirectionUp)
		glScalef (1., -1., 1.);
	glTranslatef (icon->fWidth * icon->fScale/2 - w * fRatio/2,
		icon->fHeight * icon->fScale/2 - h * fRatio/2,
		0.);
	
	cairo_dock_draw_texture_with_alpha (g_pClassIndicatorBuffer.iTexture,
		w * fRatio,
		h * fRatio,
		1.);
}*/

void cairo_dock_set_icon_scale (Icon *pIcon, CairoContainer *pContainer, double fZoomFactor)
{
	double fSizeX, fSizeY;
	cairo_dock_get_current_icon_size (pIcon, pContainer, &fSizeX, &fSizeY);
	glScalef (fSizeX * fZoomFactor, fSizeY * fZoomFactor, fSizeY * fZoomFactor);
}


void cairo_dock_combine_argb_argb (void)  // taken from glitz 0.5.6
{
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glActiveTexture (GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glColor4f(0., 0., 0., 1.);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	
	glActiveTexture (GL_TEXTURE1);
	glEnable(GL_TEXTURE_2D);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	
	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_ALPHA);
	
	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PREVIOUS);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
}

void cairo_dock_draw_icon_opengl (Icon *pIcon, CairoDock *pDock)
{
	//\_____________________ On dessine l'icone.
	double fSizeX, fSizeY;
	cairo_dock_get_current_icon_size (pIcon, CAIRO_CONTAINER (pDock), &fSizeX, &fSizeY);
	
	_cairo_dock_enable_texture ();
	if (pIcon->fAlpha == 1)
		_cairo_dock_set_blend_over ();
	else
		_cairo_dock_set_blend_alpha ();
	
	_cairo_dock_apply_texture_at_size_with_alpha (pIcon->iIconTexture, fSizeX, fSizeY, pIcon->fAlpha);
	
	//\_____________________ On dessine son reflet.
	if (pDock->container.bUseReflect)
	{
		if (pDock->pRenderer->bUseStencil)
		{
			glEnable (GL_STENCIL_TEST);
			glStencilFunc (GL_EQUAL, 1, 1);
			glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
		}
		glPushMatrix ();
		double x0, y0, x1, y1;
		double fScale = ((myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (pIcon)) ? 1. : pIcon->fScale);
		double fReflectSize = MIN (myIcons.fReflectSize, pIcon->fHeight/pDock->container.fRatio*fScale);
		double fReflectRatio = fReflectSize * pDock->container.fRatio / pIcon->fHeight / fScale  / pIcon->fHeightFactor;
		double fOffsetY = pIcon->fHeight * fScale/2 + fReflectSize * pDock->container.fRatio/2 + pIcon->fDeltaYReflection;
		if (pDock->container.bIsHorizontal)
		{
			if (pDock->container.bDirectionUp)
			{
				glTranslatef (0., - fOffsetY, 0.);
				glScalef (pIcon->fWidth * pIcon->fWidthFactor * fScale, - fReflectSize * pDock->container.fRatio, 1.);  // taille du reflet et on se retourne.
				x0 = 0.;
				y0 = 1. - fReflectRatio;
				x1 = 1.;
				y1 = 1.;
			}
			else
			{
				glTranslatef (0., fOffsetY, 0.);
				glScalef (pIcon->fWidth * pIcon->fWidthFactor * fScale, fReflectSize * pDock->container.fRatio, 1.);
				x0 = 0.;
				y0 = fReflectRatio;
				x1 = 1.;
				y1 = 0.;
			}
		}
		else
		{
			if (pDock->container.bDirectionUp)
			{
				glTranslatef (fOffsetY, 0., 0.);
				glScalef (- fReflectSize * pDock->container.fRatio, pIcon->fWidth * pIcon->fWidthFactor * fScale, 1.);
				x0 = 1. - fReflectRatio;
				y0 = 0.;
				x1 = 1.;
				y1 = 1.;
			}
			else
			{
				glTranslatef (- fOffsetY, 0., 0.);
				glScalef (fReflectSize * pDock->container.fRatio, pIcon->fWidth * pIcon->fWidthFactor * fScale, 1.);
				x0 = fReflectRatio;
				y0 = 0.;
				x1 = 0.;
				y1 = 1.;
			}
		}
		
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, pIcon->iIconTexture);
		glEnable(GL_BLEND);
		_cairo_dock_set_blend_alpha ();
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		
		//glBlendColor(1., 1., 1., 1.);  // utile ?
		
		glPolygonMode (GL_FRONT, GL_FILL);
		glColor4f(1., 1., 1., 1.);
		
		glBegin(GL_QUADS);
		
		double fReflectAlpha = myIcons.fAlbedo * pIcon->fAlpha;
		if (pDock->container.bIsHorizontal)
		{
			glTexCoord2f (x0, y0);
			glColor4f (1., 1., 1., fReflectAlpha * pIcon->fReflectShading);
			glVertex3f (-.5, .5, 0.);  // Bottom Left Of The Texture and Quad
			
			glTexCoord2f (x1, y0);
			glColor4f (1., 1., 1., fReflectAlpha * pIcon->fReflectShading);
			glVertex3f (.5, .5, 0.);  // Bottom Right Of The Texture and Quad
			
			glTexCoord2f (x1, y1);
			glColor4f (1., 1., 1., fReflectAlpha);
			glVertex3f (.5, -.5, 0.);  // Top Right Of The Texture and Quad
			
			glTexCoord2f (x0, y1);
			glColor4f (1., 1., 1., fReflectAlpha);
			glVertex3f (-.5, -.5, 0.);  // Top Left Of The Texture and Quad
		}
		else
		{
			glTexCoord2f (x0, y0);
			glColor4f (1., 1., 1., fReflectAlpha * pIcon->fReflectShading);
			glVertex3f (-.5, .5, 0.);  // Bottom Left Of The Texture and Quad
			
			glTexCoord2f (x1, y0);
			glColor4f (1., 1., 1., fReflectAlpha);
			glVertex3f (.5, .5, 0.);  // Bottom Right Of The Texture and Quad
			
			glTexCoord2f (x1, y1);
			glColor4f (1., 1., 1., fReflectAlpha);
			glVertex3f (.5, -.5, 0.);  // Top Right Of The Texture and Quad
			
			glTexCoord2f (x0, y1);
			glColor4f (1., 1., 1., fReflectAlpha * pIcon->fReflectShading);
			glVertex3f (-.5, -.5, 0.);  // Top Left Of The Texture and Quad
		}
		glEnd();
		
		glPopMatrix ();
		if (pDock->pRenderer->bUseStencil)
		{
			glDisable (GL_STENCIL_TEST);
		}
	}
	
	_cairo_dock_disable_texture ();
}


static inline void _compute_icon_coordinate (Icon *icon, CairoContainer *pContainer, double fDockMagnitude, double *pX, double *pY)
{
	double fX=0, fY=0;
	double fRatio = pContainer->fRatio;
	double fGlideScale;
	if (icon->fGlideOffset != 0)
	{
		double fPhase =  icon->fPhase + icon->fGlideOffset * icon->fWidth / fRatio / myIcons.iSinusoidWidth * G_PI;
		if (fPhase < 0)
		{
			fPhase = 0;
		}
		else if (fPhase > G_PI)
		{
			fPhase = G_PI;
		}
		fGlideScale = (1 + fDockMagnitude * myIcons.fAmplitude * sin (fPhase)) / icon->fScale;  // c'est un peu hacky ... il faudrait passer l'icone precedente en parametre ...
		if (! pContainer->bDirectionUp)
			if (pContainer->bIsHorizontal)
				fY = (1-fGlideScale)*icon->fHeight*icon->fScale;
			else
				fX = (1-fGlideScale)*icon->fHeight*icon->fScale;
	}
	else
		fGlideScale = 1;
	icon->fGlideScale = fGlideScale;
	
	if (pContainer->bIsHorizontal)
	{
		fY += pContainer->iHeight - icon->fDrawY;  // ordonnee du haut de l'icone.
		fX += icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1);  // abscisse du milieu de l'icone.
	}
	else
	{
		fY += icon->fDrawY;  // ordonnee du haut de l'icone.
		fX +=  pContainer->iWidth - (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1));
	}
	*pX = fX;
	*pY = fY;
}

void cairo_dock_translate_on_icon_opengl (Icon *icon, CairoContainer *pContainer, double fDockMagnitude)
{
	double fX=0, fY=0;
	_compute_icon_coordinate (icon, pContainer, fDockMagnitude, &fX, &fY);
	
	if (pContainer->bIsHorizontal)
		glTranslatef (floor (fX), floor (fY - icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2)), - icon->fHeight * (1+myIcons.fAmplitude));
	else
		glTranslatef (floor (fY + icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2)), floor (fX), - icon->fHeight * (1+myIcons.fAmplitude));
}

void cairo_dock_render_one_icon_opengl (Icon *icon, CairoDock *pDock, double fDockMagnitude, gboolean bUseText)
{
	if (icon->iIconTexture == 0)
		return ;
	double fRatio = pDock->container.fRatio;
	
	if (g_pGradationTexture[pDock->container.bIsHorizontal] == 0)
	{
		//g_pGradationTexture[pDock->container.bIsHorizontal] = cairo_dock_load_local_texture (pDock->container.bIsHorizontal ? "texture-gradation-vert.png" : "texture-gradation-horiz.png", CAIRO_DOCK_SHARE_DATA_DIR);
		g_pGradationTexture[pDock->container.bIsHorizontal] = cairo_dock_load_texture_from_raw_data (gradationTex,
			pDock->container.bIsHorizontal ? 1:48,
			pDock->container.bIsHorizontal ? 48:1);
		cd_debug ("g_pGradationTexture(%d) <- %d", pDock->container.bIsHorizontal, g_pGradationTexture[pDock->container.bIsHorizontal]);
	}
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.fVisibleAppliAlpha != 0 && ! CAIRO_DOCK_IS_APPLET (icon) && !(icon->iBackingPixmap != 0 && icon->bIsHidden))
	{
		double fAlpha = (icon->bIsHidden ? MIN (1 - myTaskBar.fVisibleAppliAlpha, 1) : MIN (myTaskBar.fVisibleAppliAlpha + 1, 1));
		if (fAlpha != 1)
			icon->fAlpha = fAlpha;  // astuce bidon pour pas multiplier 2 fois.
	}
	
	//\_____________________ On se place au centre de l'icone.
	double fX=0, fY=0;
	_compute_icon_coordinate (icon, CAIRO_CONTAINER (pDock), fDockMagnitude * pDock->fMagnitudeMax, &fX, &fY);
	
	glPushMatrix ();
	if (pDock->container.bIsHorizontal)
		glTranslatef (fX, fY - icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2), - icon->fHeight * (1+myIcons.fAmplitude));
	else
		glTranslatef (fY + icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2), fX, - icon->fHeight * (1+myIcons.fAmplitude));
	
	//\_____________________ On positionne l'icone.
	glPushMatrix ();
	if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
	{
		if (pDock->container.bIsHorizontal)
		{
			glTranslatef (0., (pDock->container.bDirectionUp ? icon->fHeight * (- icon->fScale + 1)/2 : icon->fHeight * (icon->fScale - 1)/2), 0.);
		}
		else
		{
			glTranslatef ((!pDock->container.bDirectionUp ? icon->fHeight * (- icon->fScale + 1)/2 : icon->fHeight * (icon->fScale - 1)/2), 0., 0.);
		}
	}
	glTranslatef (0., 0., - icon->fHeight * (1+myIcons.fAmplitude));
	if (icon->fOrientation != 0)
	{
		glTranslatef (-icon->fWidth * icon->fScale/2, icon->fHeight * icon->fScale/2, 0.);
		glRotatef (-icon->fOrientation/G_PI*180., 0., 0., 1.);
		glTranslatef (icon->fWidth * icon->fScale/2, -icon->fHeight * icon->fScale/2, 0.);
	}
	if (icon->iRotationX != 0)
		glRotatef (icon->iRotationX, 1., 0., 0.);
	if (icon->iRotationY != 0)
		glRotatef (icon->iRotationY, 0., 1., 0.);
	
	/** //\_____________________ On dessine les indicateur derriere.
	if (icon->bHasIndicator && ! myIndicators.bIndicatorAbove)
	{
		glPushMatrix ();
		_cairo_dock_draw_appli_indicator_opengl (icon, pDock->container.bIsHorizontal, fRatio, pDock->container.bDirectionUp);
		glPopMatrix ();
	}
	gboolean bIsActive = FALSE;
	Window xActiveId = cairo_dock_get_current_active_window ();
	if (xActiveId != 0 && g_pActiveIndicatorBuffer.pSurface != NULL)
	{
		bIsActive = (icon->Xid == xActiveId);
		if (!bIsActive && icon->pSubDock != NULL)
		{
			Icon *subicon;
			GList *ic;
			for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
			{
				subicon = ic->data;
				if (subicon->Xid == xActiveId)
				{
					bIsActive = TRUE;
					break;
				}
			}
		}
	}
	if (bIsActive && ! myIndicators.bActiveIndicatorAbove)
	{
		glPushMatrix ();
		_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, fRatio);
		glPopMatrix ();
	}*/
	
	//\_____________________ On dessine l'icone.
	gboolean bIconHasBeenDrawn = FALSE;
	cairo_dock_notify (CAIRO_DOCK_PRE_RENDER_ICON, icon, pDock, NULL);
	cairo_dock_notify (CAIRO_DOCK_RENDER_ICON, icon, pDock, &bIconHasBeenDrawn, NULL);
	
	/** //\_____________________ On dessine les indicateurs devant.
	if (icon->bHasIndicator && myIndicators.bIndicatorAbove)
	{
		glPushMatrix ();
		glTranslatef (0., 0., icon->fHeight * (1+myIcons.fAmplitude) -1);  // avant-plan
		_cairo_dock_draw_appli_indicator_opengl (icon, pDock->container.bIsHorizontal, fRatio, pDock->container.bDirectionUp);
		glPopMatrix ();
	}
	if (bIsActive && myIndicators.bActiveIndicatorAbove)
	{
		glPushMatrix ();
		glTranslatef (0., 0., icon->fHeight * (1+myIcons.fAmplitude) -1);  // avant-plan
		_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, fRatio);
		glPopMatrix ();
	}
	if (icon->pSubDock != NULL && icon->cClass != NULL && g_pClassIndicatorBuffer.iTexture != 0 && icon->Xid == 0)  // le dernier test est de la paranoia.
	{
		glPushMatrix ();
		glTranslatef (0., 0., icon->fHeight * (1+myIcons.fAmplitude) -1);  // avant-plan
		_cairo_dock_draw_class_indicator_opengl (icon, pDock->container.bIsHorizontal, fRatio, pDock->container.bDirectionUp);
		glPopMatrix ();
	}*/
	
	glPopMatrix ();  // retour juste apres la translation au milieu de l'icone.
	
	//\_____________________ On dessine les infos additionnelles.
	if (icon->iQuickInfoTexture != 0)
	{
		glPushMatrix ();
		glRotatef (-icon->fOrientation/G_PI*180., 0., 0., 1.);
		glTranslatef (0., (- icon->fHeight + icon->iQuickInfoHeight * fRatio) * icon->fScale/2, 0.);
		
		cairo_dock_draw_texture_with_alpha (icon->iQuickInfoTexture,
			icon->iQuickInfoWidth * fRatio * icon->fScale,
			icon->iQuickInfoHeight * fRatio * icon->fScale,
			icon->fAlpha);
		
		glPopMatrix ();
	}
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	glPopMatrix ();  // retour au debut de la fonction.
	if (bUseText && icon->iLabelTexture != 0 && (icon->fScale > 1.01 || myIcons.fAmplitude == 0 || pDock->fMagnitudeMax == 0) && (! myLabels.bLabelForPointedIconOnly || icon->bPointed))  // 1.01 car sin(pi) = 1+epsilon :-/  //  && icon->iAnimationState < CAIRO_DOCK_STATE_CLICKED
	{
		glPushMatrix ();
		if (pDock->container.bIsHorizontal)
			glTranslatef (floor (fX), floor (fY - icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2)), - icon->fHeight * (1+myIcons.fAmplitude));
		else
			glTranslatef (floor (fY + icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2)), floor (fX), - icon->fHeight * (1+myIcons.fAmplitude));
		
		double fOffsetX = 0.;
		if (icon->fDrawX + icon->fWidth * icon->fScale/2 - icon->iTextWidth/2 < 0)
			fOffsetX = icon->iTextWidth/2 - (icon->fDrawX + icon->fWidth * icon->fScale/2);
		else if (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->iTextWidth/2 > pDock->container.iWidth)
			fOffsetX = pDock->container.iWidth - (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->iTextWidth/2);
		if (icon->fOrientation != 0 && ! mySystem.bTextAlwaysHorizontal)
		{
			glTranslatef (-icon->fWidth * icon->fScale/2, icon->fHeight * icon->fScale/2, 0.);
			glRotatef (-icon->fOrientation/G_PI*180., 0., 0., 1.);
			glTranslatef (icon->fWidth * icon->fScale/2, -icon->fHeight * icon->fScale/2, 0.);
		}
		
		double dx = .5 * (icon->iTextWidth & 1);  // on decale la texture pour la coller sur la grille des coordonnees entieres.
		double dy = .5 * (icon->iTextHeight & 1);
		if (! pDock->container.bIsHorizontal && mySystem.bTextAlwaysHorizontal)
		{
			fOffsetX = MIN (0, icon->fDrawY + icon->fWidth * icon->fScale/2 - icon->iTextWidth/2);
			/**glTranslatef (ceil (-icon->fHeight * icon->fScale/2 - (pDock->container.bDirectionUp ? myLabels.iLabelSize : (pDock->container.bUseReflect ? myIcons.fReflectSize : 0.)) + icon->iTextWidth / 2 - myLabels.iconTextDescription.iMargin + 1) + dx,
				floor ((icon->fWidth * icon->fScale + icon->iTextHeight) / 2) + dy,
				0.);*/
			glTranslatef (ceil (- fOffsetX) + dx,
				floor ((icon->fWidth * icon->fScale + icon->iTextHeight) / 2) + dy,
				0.);
		}
		else if (pDock->container.bIsHorizontal)
		{
			glTranslatef (floor (fOffsetX) + dx,
				floor ((pDock->container.bDirectionUp ? 1:-1) * (icon->fHeight * icon->fScale/2 + myLabels.iLabelSize - icon->iTextHeight / 2)) + dy,
				0.);
		}
		else
		{
			glTranslatef (floor ((pDock->container.bDirectionUp ? -.5:.5) * (icon->fHeight * icon->fScale + icon->iTextHeight)) + dx,
				floor (fOffsetX) + dy,
				0.);
			glRotatef (pDock->container.bDirectionUp ? 90 : -90, 0., 0., 1.);
		}
		
		double fMagnitude;
		if (myLabels.bLabelForPointedIconOnly ||pDock->fMagnitudeMax == 0.)
		{
			fMagnitude = fDockMagnitude;  // (icon->fScale - 1) / myIcons.fAmplitude / sin (icon->fPhase);  // sin (phi ) != 0 puisque fScale > 1.
		}
		else
		{
			fMagnitude = (icon->fScale - 1) / myIcons.fAmplitude;  /// il faudrait diviser par pDock->fMagnitudeMax ...
			fMagnitude = pow (fMagnitude, mySystem.fLabelAlphaThreshold);
			///fMagnitude *= (fMagnitude * mySystem.fLabelAlphaThreshold + 1) / (mySystem.fLabelAlphaThreshold + 1);
		}
		
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_alpha ();
		_cairo_dock_apply_texture_at_size_with_alpha (icon->iLabelTexture,
			icon->iTextWidth,
			icon->iTextHeight,
			fMagnitude);
		_cairo_dock_disable_texture ();
		
		glPopMatrix ();
	}
}


void cairo_dock_render_hidden_dock_opengl (CairoDock *pDock)
{
	//g_print ("%s (%d, %x)\n", __func__, pDock->bIsMainDock, g_pVisibleZoneSurface);
	//\_____________________ on dessine la zone de rappel.
	glLoadIdentity ();  // annule l'offset de cachage.
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (pDock->pRenderer->bUseStencil ? GL_STENCIL_BUFFER_BIT : 0));
	cairo_dock_apply_desktop_background_opengl (CAIRO_CONTAINER (pDock));
	
	if (g_pVisibleZoneBuffer.iTexture != 0)
	{
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_over ();
		//_cairo_dock_set_alpha (myBackground.fVisibleZoneAlpha);
		int w = MIN (myAccessibility.iZoneWidth, pDock->container.iWidth);
		int h = MIN (myAccessibility.iZoneHeight, pDock->container.iHeight);
		cd_debug ("%s (%dx%d)", __func__, w, h);
		
		if (pDock->container.bIsHorizontal)
		{
			if (pDock->container.bDirectionUp)
				glTranslatef ((pDock->container.iWidth)/2, h/2, 0.);
			else
				glTranslatef ((pDock->container.iWidth)/2, pDock->container.iHeight - h/2, 0.);
		}
		else
		{
			if (!pDock->container.bDirectionUp)
				glTranslatef (h/2, (pDock->container.iWidth)/2, 0.);
			else
				glTranslatef (pDock->container.iHeight - h/2, (pDock->container.iWidth)/2, 0.);
		}
		
		if (! pDock->container.bIsHorizontal)
			glRotatef (90., 0, 0, 1);
		if (! pDock->container.bDirectionUp && /**myBackground.bReverseVisibleImage*/TRUE)
			glScalef (1., -1., 1.);
		
		_cairo_dock_apply_texture_at_size (g_pVisibleZoneBuffer.iTexture, w, h);
		
		_cairo_dock_disable_texture ();
	}
	
	//\_____________________ on dessine les icones demandant l'attention.
	GList *pFirstDrawnElement = cairo_dock_get_first_drawn_element_linear (pDock->icons);
	if (pFirstDrawnElement == NULL)
		return;
	double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
	
	double y;
	Icon *icon;
	GList *ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;
		if (icon->bIsDemandingAttention || icon->bAlwaysVisible)
		{
			//g_print ("%s : %d (%d)\n", icon->cName, icon->bIsDemandingAttention, icon->Xid);
			y = icon->fDrawY;
			icon->fDrawY = (pDock->container.bDirectionUp ? pDock->container.iHeight - icon->fHeight * icon->fScale : 0.);
			glPushMatrix ();
			icon->fAlpha = pDock->fPostHideOffset * pDock->fPostHideOffset;
			cairo_dock_render_one_icon_opengl (icon, pDock, fDockMagnitude, TRUE);
			glPopMatrix ();
			icon->fDrawY = y;
		}
		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);
}



GLuint cairo_dock_create_texture_from_surface (cairo_surface_t *pImageSurface)
{
	if (! g_bUseOpenGL || pImageSurface == NULL)
		return 0;
	GLuint iTexture = 0;
	int w = cairo_image_surface_get_width (pImageSurface);
	int h = cairo_image_surface_get_height (pImageSurface);
	
	cairo_surface_t *pPowerOfwoSurface = pImageSurface;
	
	int iMaxTextureWidth = 4096, iMaxTextureHeight = 4096;  // il faudrait le recuperer de glInfo ...
	if (! g_openglConfig.bNonPowerOfTwoAvailable)  // cas des vieilles cartes comme la GeForce5.
	{
		double log2_w = log (w) / log (2);
		double log2_h = log (h) / log (2);
		int w_ = MIN (iMaxTextureWidth, pow (2, ceil (log2_w)));
		int h_ = MIN (iMaxTextureHeight, pow (2, ceil (log2_h)));
		cd_debug ("%dx%d --> %dx%d", w, h, w_, h_);
		
		if (w != w_ || h != h_)
		{
			pPowerOfwoSurface = cairo_dock_create_blank_surface (w_, h_);
			cairo_t *pCairoContext = cairo_create (pPowerOfwoSurface);
			cairo_scale (pCairoContext, (double) w_ / w, (double) h_ / h);
			cairo_set_source_surface (pCairoContext, pImageSurface, 0., 0.);
			cairo_paint (pCairoContext);
			cairo_destroy (pCairoContext);
			w = w_;
			h = h_;
		}
	}
	
	glEnable(GL_TEXTURE_2D);
	glGenTextures (1, &iTexture);
	cd_debug ("+ texture %d generee (%x, %dx%d)", iTexture, cairo_image_surface_get_data (pImageSurface), w, h);
	glBindTexture (GL_TEXTURE_2D, iTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (g_bEasterEggs)
		gluBuild2DMipmaps (GL_TEXTURE_2D,
			4,
			w,
			h,
			GL_BGRA,
			GL_UNSIGNED_BYTE,
			cairo_image_surface_get_data (pPowerOfwoSurface));
	else
		glTexImage2D (GL_TEXTURE_2D,
			0,
			4,  // GL_ALPHA / GL_BGRA
			w,
			h,
			0,
			GL_BGRA,  // GL_ALPHA / GL_BGRA
			GL_UNSIGNED_BYTE,
			cairo_image_surface_get_data (pPowerOfwoSurface));
	if (pPowerOfwoSurface != pImageSurface)
		cairo_surface_destroy (pPowerOfwoSurface);
	glDisable(GL_TEXTURE_2D);
	return iTexture;
}

GLuint cairo_dock_load_texture_from_raw_data (const guchar *pTextureRaw, int iWidth, int iHeight)
{
	/*cd_debug ("%dx%d\n", iWidth, iHeight);
	int i;
	guint pixel, alpha, red, green, blue;
	float fAlphaFactor;
	guint *pPixelBuffer = (guint *) pTextureRaw;
	guint *pPixelBuffer2 = g_new (guint, iHeight * iWidth);
	for (i = 0; i < iHeight * iWidth; i ++)
	{
		pixel = (gint) pPixelBuffer[i];
		alpha = (pixel & 0xFF000000) >> 24;
		red = (pixel & 0x00FF0000) >> 16;
		green = (pixel & 0x0000FF00) >> 8;
		blue  = (pixel & 0x000000FF);
		fAlphaFactor = (float) alpha / 255.;
		red *= fAlphaFactor;
		green *= fAlphaFactor;
		blue *= fAlphaFactor;
		pPixelBuffer2[i] = (pixel & 0xFF000000) + (red << 16) + (green << 8) + (blue << 0);
		cd_debug ("\\%o\\%o\\%o\\%o", red, green, blue, alpha);
	}
	pTextureRaw = pPixelBuffer2;*/
	GLuint iTexture = 0;
	
	glEnable (GL_TEXTURE_2D);
	glGenTextures(1, &iTexture);
	glBindTexture(GL_TEXTURE_2D, iTexture);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, iWidth, iHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pTextureRaw);
	glBindTexture (GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	return iTexture;
}

GLuint cairo_dock_create_texture_from_image_full (const gchar *cImageFile, double *fImageWidth, double *fImageHeight)
{
	g_return_val_if_fail (GTK_WIDGET_REALIZED (g_pMainDock->container.pWidget), 0);
	double fWidth=0, fHeight=0;
	if (cImageFile == NULL)
		return 0;
	gchar *cImagePath;
	if (*cImageFile == '/')
		cImagePath = (gchar *)cImageFile;
	else
		cImagePath = cairo_dock_generate_file_path (cImageFile);
	
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (g_pMainDock));
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_image (cImagePath,
		1.,
		0., 0.,
		CAIRO_DOCK_KEEP_RATIO,
		&fWidth,
		&fHeight,
		NULL, NULL);
	//cd_debug ("texture genere (%x, %.2fx%.2f)", pSurface, fWidth, fHeight);
	cairo_destroy (pCairoContext);
	
	if (fImageWidth != NULL)
		*fImageWidth = fWidth;
	if (fImageHeight != NULL)
		*fImageHeight = fHeight;
	GLuint iTexture = cairo_dock_create_texture_from_surface (pSurface);
	cairo_surface_destroy (pSurface);
	if (cImagePath != cImageFile)
		g_free (cImagePath);
	return iTexture;
}


void cairo_dock_update_icon_texture (Icon *pIcon)
{
	if (pIcon != NULL && pIcon->pIconBuffer != NULL)
	{
		glEnable (GL_TEXTURE_2D);
		if (pIcon->iIconTexture == 0)
			glGenTextures (1, &pIcon->iIconTexture);
		int w = cairo_image_surface_get_width (pIcon->pIconBuffer);
		int h = cairo_image_surface_get_height (pIcon->pIconBuffer);
		glBindTexture (GL_TEXTURE_2D, pIcon->iIconTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		glTexImage2D (GL_TEXTURE_2D,
			0,
			4,  // GL_ALPHA / GL_BGRA
			w,
			h,
			0,
			GL_BGRA,  // GL_ALPHA / GL_BGRA
			GL_UNSIGNED_BYTE,
			cairo_image_surface_get_data (pIcon->pIconBuffer));
		glDisable (GL_TEXTURE_2D);
	}
}

void cairo_dock_update_label_texture (Icon *pIcon)
{
	if (pIcon->iLabelTexture != 0)
	{
		_cairo_dock_delete_texture (pIcon->iLabelTexture);
		pIcon->iLabelTexture = 0;
	}
	if (pIcon != NULL && pIcon->pTextBuffer != NULL)
	{
		glEnable (GL_TEXTURE_2D);
		glGenTextures (1, &pIcon->iLabelTexture);
		int w = cairo_image_surface_get_width (pIcon->pTextBuffer);
		int h = cairo_image_surface_get_height (pIcon->pTextBuffer);
		glBindTexture (GL_TEXTURE_2D, pIcon->iLabelTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D,
			0,
			4,  // GL_ALPHA / GL_BGRA
			w,
			h,
			0,
			GL_BGRA,  // GL_ALPHA / GL_BGRA
			GL_UNSIGNED_BYTE,
			cairo_image_surface_get_data (pIcon->pTextBuffer));
		glDisable (GL_TEXTURE_2D);
	}
}

void cairo_dock_update_quick_info_texture (Icon *pIcon)
{
	if (pIcon->iQuickInfoTexture != 0)
	{
		_cairo_dock_delete_texture (pIcon->iQuickInfoTexture);
		pIcon->iQuickInfoTexture = 0;
	}
	if (pIcon != NULL && pIcon->pQuickInfoBuffer != NULL)
	{
		glEnable (GL_TEXTURE_2D);
		glGenTextures (1, &pIcon->iQuickInfoTexture);
		int w = cairo_image_surface_get_width (pIcon->pQuickInfoBuffer);
		int h = cairo_image_surface_get_height (pIcon->pQuickInfoBuffer);
		glBindTexture (GL_TEXTURE_2D, pIcon->iQuickInfoTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D,
			0,
			4,  // GL_ALPHA / GL_BGRA
			w,
			h,
			0,
			GL_BGRA,  // GL_ALPHA / GL_BGRA
			GL_UNSIGNED_BYTE,
			cairo_image_surface_get_data (pIcon->pQuickInfoBuffer));
		glDisable (GL_TEXTURE_2D);
	}
}



void cairo_dock_draw_texture_with_alpha (GLuint iTexture, int iWidth, int iHeight, double fAlpha)
{
	_cairo_dock_enable_texture ();
	if (fAlpha == 1)
		_cairo_dock_set_blend_over ();
	else
		_cairo_dock_set_blend_alpha ();
		//_cairo_dock_set_blend_alpha ();
	
	_cairo_dock_apply_texture_at_size_with_alpha (iTexture, iWidth, iHeight, fAlpha);
	
	_cairo_dock_disable_texture ();
}

void cairo_dock_draw_texture (GLuint iTexture, int iWidth, int iHeight)
{
	cairo_dock_draw_texture_with_alpha (iTexture, iWidth, iHeight, 1.);
}

void cairo_dock_apply_icon_texture_at_current_size (Icon *pIcon, CairoContainer *pContainer)
{
	double fSizeX, fSizeY;
	cairo_dock_get_current_icon_size (pIcon, pContainer, &fSizeX, &fSizeY);
	
	_cairo_dock_apply_texture_at_size (pIcon->iIconTexture, fSizeX, fSizeY);
}

void cairo_dock_draw_icon_texture (Icon *pIcon, CairoContainer *pContainer)
{
	double fSizeX, fSizeY;
	cairo_dock_get_current_icon_size (pIcon, pContainer, &fSizeX, &fSizeY);
	
	cairo_dock_draw_texture_with_alpha (pIcon->iIconTexture,
		fSizeX,
		fSizeY,
		pIcon->fAlpha);
}

static inline void  _draw_icon_bent_backwards (Icon *pIcon, CairoContainer *pContainer, GLuint iOriginalTexture, double f)
{
	cairo_dock_set_perspective_view_for_icon (pIcon, pContainer);
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
	glScalef (1., -1., 1.);
	glTranslatef (0., -iHeight/2, 0.);  // rotation de 50° sur l'axe des X a la base de l'icone.
	glRotatef (-50.*f, 1., 0., 0.);
	glTranslatef (0., iHeight/2, 0.);
	
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_source ();
	glBindTexture (GL_TEXTURE_2D, iOriginalTexture);
	double a=.25, b=.1;
	//double a=.0, b=.0;
	_cairo_dock_apply_current_texture_at_size_with_offset (iWidth*(1+b*f),
		iHeight*(1+a*f),
		0.,
		iHeight*(a/2*f));  // on elargit un peu la texture, car avec l'effet de profondeur elle parait trop petite.
	_cairo_dock_disable_texture ();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0, 0, pContainer->iWidth, pContainer->iHeight);
	glMatrixMode(GL_MODELVIEW);
	cairo_dock_set_ortho_view (pContainer);
}
static gboolean _transition_step (Icon *pIcon, gpointer data)
{
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pDock == NULL)
		return FALSE;
	
	GLuint iOriginalTexture = GPOINTER_TO_INT (data);
	double f = cairo_dock_get_transition_fraction (pIcon);
	if (!pIcon->bIsHidden)
		f = 1 - f;
	
	_draw_icon_bent_backwards (pIcon, CAIRO_CONTAINER (pDock), iOriginalTexture, f);
	return TRUE;
}
static void _free_transition_data (gpointer data)
{
	GLuint iOriginalTexture = GPOINTER_TO_INT (data);
	_cairo_dock_delete_texture (iOriginalTexture);
}
void cairo_dock_draw_hidden_appli_icon (Icon *pIcon, CairoContainer *pContainer, gboolean bStateChanged)
{
	if (bStateChanged)
	{
		cairo_dock_remove_transition_on_icon (pIcon);
		
		int iWidth, iHeight;
		cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
		
		GLuint iOriginalTexture;
		if (pIcon->bIsHidden)
		{
			iOriginalTexture = pIcon->iIconTexture;
			pIcon->iIconTexture = cairo_dock_create_texture_from_surface (pIcon->pIconBuffer);
			/// Using FBOs copies the texture data (pixels) within VRAM only:
			/// - setup & bind FBO
			/// - setup destination texture (using glTexImage() w/ pixels = 0)
			/// - add (blank but sized) destination texture as color attachment
			/// - bind source texture to texture target
			/// - draw quad w/ glTexCoors describing src area, projection/modelview matrices & glVertexes describing dst area
		}
		else
		{
			iOriginalTexture = cairo_dock_create_texture_from_surface (pIcon->pIconBuffer);
		}
		
		cairo_dock_set_transition_on_icon (pIcon, pContainer, NULL,
			(CairoDockTransitionRenderFunc) NULL,
			(CairoDockTransitionGLRenderFunc) _transition_step,
			TRUE,  // slow
			500,  // ms
			TRUE,  // remove when finished
			GINT_TO_POINTER (iOriginalTexture),
			_free_transition_data);
	}
	else if (pIcon->bIsHidden)
	{
		if (!cairo_dock_begin_draw_icon (pIcon, pContainer, 2))
			return ;
		_draw_icon_bent_backwards (pIcon, pContainer, pIcon->iIconTexture, 1.);
		cairo_dock_end_draw_icon (pIcon, pContainer);
	}
}


/// PATH ///

const GLfloat *cairo_dock_generate_rectangle_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, int *iNbPoints)
{
	//static GLfloat pVertexTab[((90/DELTA_ROUND_DEGREE+1)*4+1)*_CAIRO_DOCK_PATH_DIM];
	_cairo_dock_define_static_vertex_tab ((90/DELTA_ROUND_DEGREE+1)*4+1);
	
	double fTotalWidth = fDockWidth + 2 * fRadius;
	double w = fDockWidth / fTotalWidth / 2;
	double h = MAX (0, fFrameHeight - 2 * fRadius) / fFrameHeight / 2;
	double rw = fRadius / fTotalWidth;
	double rh = fRadius / fFrameHeight;
	int i=0, t;
	int iPrecision = DELTA_ROUND_DEGREE;
	for (t = 0;t <= 90;t += iPrecision, i++) // cote haut droit.
	{
		_cairo_dock_set_vertex_xy (i,
			w + rw * cos (t*RADIAN),
			h + rh * sin (t*RADIAN));
		//vx(i) = w + rw * cos (t*RADIAN);
		//vy(i) = h + rh * sin (t*RADIAN);
	}
	for (t = 90;t <= 180;t += iPrecision, i++) // haut gauche.
	{
		_cairo_dock_set_vertex_xy (i,
			-w + rw * cos (t*RADIAN),
			h + rh * sin (t*RADIAN));
		//vx(i) = -w + rw * cos (t*RADIAN);
		//vy(i) = h + rh * sin (t*RADIAN);
	}
	if (bRoundedBottomCorner)
	{
		for (t = 180;t <= 270;t += iPrecision, i++) // bas gauche.
		{
			_cairo_dock_set_vertex_xy (i,
				-w + rw * cos (t*RADIAN),
				-h + rh * sin (t*RADIAN));
			//vx(i) = -w + rw * cos (t*RADIAN);
			//vy(i) = -h + rh * sin (t*RADIAN);
		}
		for (t = 270;t <= 360;t += iPrecision, i++) // bas droit.
		{
			_cairo_dock_set_vertex_xy (i,
				w + rw * cos (t*RADIAN),
				-h + rh * sin (t*RADIAN));
			//vx(i) = w + rw * cos (t*RADIAN);
			//vy(i) = -h + rh * sin (t*RADIAN);
		}
	}
	else
	{
		_cairo_dock_set_vertex_xy (i,
			-w - rw,
			-h - rh);  // bas gauche.
		//vx(i) = -w - rw; // bas gauche.
		//vy(i) = -h - rh;
		i ++;
		_cairo_dock_set_vertex_xy (i,
			w + rw,
			-h - rh);  // bas droit.
		//vx(i) = w + rw; // bas droit.
		//vy(i) = -h - rh;
		i ++;
	}
	_cairo_dock_close_path (i);  // on boucle.
	//vx(i) = w + rw;  // on boucle.
	//vy(i) = h;
	
	*iNbPoints = i+1;
	_cairo_dock_return_vertex_tab ();
}

#define Bezier2(t,p,q,s) ((1-t) * (1-t) * p + 2 * t * (1-t) * q + t * t * s)
void cairo_dock_add_simple_curved_subpath_opengl (GLfloat *pVertexTab, int iNbPts, double x0, double y0, double x1, double y1, double x2, double y2)
{
	double t;
	int i;
	for (i = 0; i < iNbPts; i ++)
	{
		t = 1.*i/iNbPts;  // [0;1[
		_cairo_dock_set_vertex_xy (i,
			Bezier2(t, x0, x1, x2),
			Bezier2(t, y0, y1, y2));
		//vx(i) = Bezier2(t, x0, x1, x2);
		//vy(i) = Bezier2(t, y0, y1, y2);
	}
}
#define NB_PTS_SIMPLE_CURVE 20
GLfloat *cairo_dock_generate_trapeze_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, double fInclination, double *fExtraWidth, int *iNbPoints)
{
	//static GLfloat pVertexTab[((90/DELTA_ROUND_DEGREE+1+20+1)*2+1)*_CAIRO_DOCK_PATH_DIM];
	_cairo_dock_define_static_vertex_tab ((90/DELTA_ROUND_DEGREE+1+NB_PTS_SIMPLE_CURVE+1)*2+1);
	
	double a = atan (fInclination)/G_PI*180.;
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;
	
	*fExtraWidth = fInclination * (fFrameHeight - (bRoundedBottomCorner ? 2 : 1-cosa) * fRadius) + fRadius * (bRoundedBottomCorner ? 1 : sina);
	double fTotalWidth = fDockWidth + 2*(*fExtraWidth);
	double dw = (bRoundedBottomCorner ? fInclination * (fFrameHeight - 2 * fRadius) : *fExtraWidth) / fTotalWidth;
	double w = fDockWidth / fTotalWidth / 2;
	double h = MAX (0, fFrameHeight - 2 * fRadius) / fFrameHeight / 2;
	double rw = fRadius / fTotalWidth;
	double rh = fRadius / fFrameHeight;
	double w_ = w + dw + (bRoundedBottomCorner ? 0 : 0*rw * cosa);
	
	int i=0, t;
	int iPrecision = DELTA_ROUND_DEGREE;
	for (t = a;t <= 90;t += iPrecision, i++) // cote haut droit.
	{
		_cairo_dock_set_vertex_xy (i,
			w + rw * cos (t*RADIAN),
			h + rh * sin (t*RADIAN));
		//vx(i) = w + rw * cos (t*RADIAN);
		//vy(i) = h + rh * sin (t*RADIAN);
	}
	for (t = 90;t <= 180-a;t += iPrecision, i++) // haut gauche.
	{
		_cairo_dock_set_vertex_xy (i,
			-w + rw * cos (t*RADIAN),
			h + rh * sin (t*RADIAN));
		//vx(i) = -w + rw * cos (t*RADIAN);
		//vy(i) = h + rh * sin (t*RADIAN);
	}
	if (bRoundedBottomCorner)
	{
		// OM(t) = sum ([k=0..n] Bn,k(t)*OAk)
		// Bn,k(x) = Cn,k*x^k*(1-x)^(n-k)
		double t = 180-a;
		double x0 = -w_ + rw * cos (t*RADIAN);
		double y0 = -h + rh * sin (t*RADIAN);
		double x1 = x0 - fInclination * rw * (1+sina);
		double y1 = -h - rh;
		double x2 = -w_;
		double y2 = y1;
		for (t=0; t<=1; t+=.05, i++) // bas gauche.
		{
			_cairo_dock_set_vertex_xy (i,
				Bezier2(t, x0, x1, x2),
				Bezier2(t, y0, y1, y2));
			//vx(i) = Bezier2(t, x0, x1, x2);
			//vy(i) = Bezier2(t, y0, y1, y2);
		}
		
		double x3 = x0, y3 = y0;
		x0 = - x2;
		y0 = y2;
		x1 = - x1;
		x2 = - x3;
		y2 = y3;
		for (t=0; t<=1; t+=.05, i++) // bas gauche.
		{
			_cairo_dock_set_vertex_xy (i,
				Bezier2(t, x0, x1, x2),
				Bezier2(t, y0, y1, y2));
			//vx(i) = Bezier2(t, x0, x1, x2);
			//vy(i) = Bezier2(t, y0, y1, y2);
		}
	}
	else
	{
		_cairo_dock_set_vertex_xy (i,
			-w_,
			-h - rh);  // bas gauche.
		//vx(i) = -w_; // bas gauche.
		//vy(i) = -h - rh;
		i ++;
		_cairo_dock_set_vertex_xy (i,
			w_,
			-h - rh);  // bas droit.
		//vx(i) = w_; // bas droit.
		//vy(i) = -h - rh;
		i ++;
	}
	_cairo_dock_close_path (i);  // on boucle.
	//vx(i) = vx(0);  // on boucle.
	//vy(i) = vy(0);
	
	*iNbPoints = i+1;
	_cairo_dock_return_vertex_tab ();
}

// OM(t) = sum ([k=0..n] Bn,k(t)*OAk)
// Bn,k(x) = Cn,k*x^k*(1-x)^(n-k)
#define B0(t) (1-t)*(1-t)*(1-t)
#define B1(t) 3*t*(1-t)*(1-t)
#define B2(t) 3*t*t*(1-t)
#define B3(t) t*t*t
#define Bezier(x0,x1,x2,x3,t) (B0(t)*x0 + B1(t)*x1 + B2(t)*x2 + B3(t)*x3)

#define _get_icon_center_x(icon) (icon->fDrawX + icon->fWidth * icon->fScale/2)
#define _get_icon_center_y(icon) (icon->fDrawY + (bForceConstantSeparator && CAIRO_DOCK_IS_SEPARATOR (icon) ? icon->fHeight * (icon->fScale - .5) : icon->fHeight * icon->fScale/2))
#define _get_icon_center(icon,x,y) do {\
	if (pDock->container.bIsHorizontal) {\
		x = _get_icon_center_x (icon);\
		y = pDock->container.iHeight - _get_icon_center_y (icon); }\
	else {\
		 y = _get_icon_center_x (icon);\
		 x = pDock->container.iWidth - _get_icon_center_y (icon); } } while (0)
#define _calculate_slope(x0,y0,x1,y1,dx,dy) do {\
	dx = x1 - x0;\
	dy = y1 - y0;\
	norme = sqrt (dx*dx + dy*dy);\
	dx /= norme;\
	dy /= norme; } while (0)
#define NB_VERTEX_PER_ICON_PAIR 10
GLfloat *cairo_dock_generate_string_path_opengl (CairoDock *pDock, gboolean bIsLoop, gboolean bForceConstantSeparator, int *iNbPoints)
{
	//static GLfloat pVertexTab[100*NB_VERTEX_PER_ICON_PAIR*3];
	_cairo_dock_define_static_vertex_tab (100*NB_VERTEX_PER_ICON_PAIR);
	
	bForceConstantSeparator = bForceConstantSeparator || myIcons.bConstantSeparatorSize;
	GList *ic, *next_ic, *next2_ic, *pFirstDrawnElement = (pDock->pFirstDrawnElement != NULL ? pDock->pFirstDrawnElement : pDock->icons);
	Icon *pIcon, *pNextIcon, *pNext2Icon;
	double x0,y0, x1,y1, x2,y2;  // centres des icones P0, P1, P2, en coordonnees opengl.
	double norme;  // pour normaliser les pentes.
	double dx, dy;  // direction au niveau de l'icone courante P0.
	double dx_, dy_;  // direction au niveau de l'icone suivante P1.
	double x0_,y0_, x1_,y1_;  // points de controle entre P0 et P1.
	if (pFirstDrawnElement == NULL)
	{
		*iNbPoints = 0;
		_cairo_dock_return_vertex_tab ();
	}
	
	// direction initiale.
	ic = pFirstDrawnElement;
	pIcon = ic->data;
	_get_icon_center (pIcon,x0,y0);
	next_ic = cairo_dock_get_next_element (ic, pDock->icons);
	pNextIcon = next_ic->data;
	_get_icon_center (pNextIcon,x1,y1);
	if (! bIsLoop)
	{
		_calculate_slope (x0,y0, x1,y1, dx,dy);
	}
	else
	{
		next2_ic = cairo_dock_get_previous_element (ic, pDock->icons);  // icone precedente dans la boucle.
		pNext2Icon = next2_ic->data;
		_get_icon_center (pNext2Icon,x2,y2);
		_calculate_slope (x2,y2, x0,y0, dx,dy);
	}
	// point suivant.
	next2_ic = cairo_dock_get_next_element (next_ic, pDock->icons);
	pNext2Icon = next2_ic->data;
	_get_icon_center (pNext2Icon,x2,y2);
	
	// on parcourt les icones.
	double t;
	int i, n=0;
	do
	{
		// l'icone courante, la suivante, et celle d'apres.
		pIcon = ic->data;
		pNextIcon = next_ic->data;
		pNext2Icon = next2_ic->data;
		
		// on va tracer de (x0,y0) a (x1,y1)
		_get_icon_center (pIcon,x0,y0);
		_get_icon_center (pNextIcon,x1,y1);
		_get_icon_center (pNext2Icon,x2,y2);
		
		// la pente au point (x1,y1)
		_calculate_slope (x0,y0, x2,y2, dx_,dy_);
		
		// points de controle.
		norme = sqrt ((x1-x0) * (x1-x0) + (y1-y0) * (y1-y0))/2;  // distance de prolongation suivant la pente.
		x0_ = x0 + dx * norme;
		y0_ = y0 + dy * norme;
		x1_ = x1 - dx_ * norme;
		y1_ = y1 - dy_ * norme;
		
		for (i = 0; i < NB_VERTEX_PER_ICON_PAIR; i ++, n++)
		{
			t = 1.*i/NB_VERTEX_PER_ICON_PAIR;  // [0;1[
			_cairo_dock_set_vertex_xy (n,
				Bezier (x0,x0_,x1_,x1,t),
				Bezier (y0,y0_,y1_,y1,t));
			//vx(n) = Bezier (x0,x0_,x1_,x1,t);
			//vy(n) = Bezier (y0,y0_,y1_,y1,t);
		}
		
		// on decale tout d'un cran.
		ic = next_ic;
		next_ic = next2_ic;
		next2_ic = cairo_dock_get_next_element (next_ic, pDock->icons);
		dx = dx_;
		dy = dy_;
		if (next_ic == pFirstDrawnElement && ! bIsLoop)
			break ;
	}
	while (ic != pFirstDrawnElement && n < 100*NB_VERTEX_PER_ICON_PAIR);
	
	*iNbPoints = n;
	_cairo_dock_return_vertex_tab ();
}


void cairo_dock_draw_frame_background_opengl (GLuint iBackgroundTexture, double fDockWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, const GLfloat *pVertexTab, int iNbVertex, CairoDockTypeHorizontality bHorizontal, gboolean bDirectionUp, double fDecorationsOffsetX)
{
	//\__________________ On mappe la texture dans le cadre.
	glEnable(GL_BLEND); // On active le blend
	
	glPolygonMode(GL_FRONT, GL_FILL);
	
	if (iBackgroundTexture != 0)
	{
		glColor4f(1., 1., 1., 1.); // Couleur a fond
		glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D); // Je veux de la texture
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBindTexture(GL_TEXTURE_2D, iBackgroundTexture); // allez on bind la texture
		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR ); // ok la on selectionne le type de generation des coordonnees de la texture
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		glEnable(GL_TEXTURE_GEN_S); // oui je veux une generation en S
		glEnable(GL_TEXTURE_GEN_T); // Et en T aussi
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		//\__________________ bidouille de la texture.
		glMatrixMode(GL_TEXTURE); // On selectionne la matrice des textures
		glPushMatrix ();
		glLoadIdentity(); // On la reset
		glTranslatef(0.5f - fDecorationsOffsetX * myBackground.fDecorationSpeed / (fDockWidth), 0.5f, 0.);
		glScalef (1., -1., 1.);
		glMatrixMode(GL_MODELVIEW);
	}
	else
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//\__________________ On place le cadre.
	///glLoadIdentity();
	if (bHorizontal)
	{
		glTranslatef ((int) (fDockOffsetX + fDockWidth/2), (int) (fDockOffsetY - fFrameHeight/2), -100);  // (int) -pDock->iMaxIconHeight * (1 + myIcons.fAmplitude) + 1
		glScalef (fDockWidth, fFrameHeight, 1.);
	}
	else
	{
		glTranslatef ((int) (fDockOffsetY - fFrameHeight/2), (int) (fDockOffsetX - fDockWidth/2), -100);
		glScalef (fFrameHeight, fDockWidth, 1.);
	}
	
	if (! bHorizontal)
		glRotatef (bDirectionUp ? 90 : -90, 0., 0., 1.);
	
	if (bHorizontal)
	{
		if (! bDirectionUp)
			glScalef (1., -1., 1.);
	}
	else
	{
		if (bDirectionUp)
			glScalef (-1., 1., 1.);
	}
	
	//\__________________ On dessine le cadre.
	glEnableClientState(GL_VERTEX_ARRAY);
	_cairo_dock_set_vertex_pointer (pVertexTab);
	glDrawArrays(GL_POLYGON, 0, iNbVertex);  // GL_TRIANGLE_FAN
	glDisableClientState(GL_VERTEX_ARRAY);
	
	//\__________________ fini la texture.
	if (iBackgroundTexture != 0)
	{
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_2D); // Plus de texture merci 
		
		glMatrixMode(GL_TEXTURE); // On selectionne la matrice des textures
		glPopMatrix ();
		glMatrixMode(GL_MODELVIEW);
	}
}

void cairo_dock_draw_current_path_opengl (double fLineWidth, double *fLineColor, /*const GLfloat *pVertexTab, */int iNbVertex)
{
	glPolygonMode(GL_FRONT, GL_LINE);
	glEnable (GL_LINE_SMOOTH);
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	glLineWidth(fLineWidth); // Ici on choisit l'epaisseur du contour du polygone 
	if (fLineColor != NULL)
		glColor4f (fLineColor[0], fLineColor[1], fLineColor[2], fLineColor[3]); // Et sa couleur.
	
	glEnableClientState(GL_VERTEX_ARRAY);
	///glVertexPointer(3, GL_FLOAT, 0, pVertexTab);
	glDrawArrays(GL_LINE_STRIP, 0, iNbVertex);
	glDisableClientState(GL_VERTEX_ARRAY);
	
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}


void cairo_dock_draw_string_opengl (CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator)
{
	int iNbVertex;
	GLfloat *pVertexTab = cairo_dock_generate_string_path_opengl (pDock, bIsLoop, bForceConstantSeparator, &iNbVertex);
	if (iNbVertex == 0)
		return;
	//glVertexPointer(_CAIRO_DOCK_PATH_DIM, GL_FLOAT, 0, pVertexTab);
	_cairo_dock_set_vertex_pointer (pVertexTab);
	cairo_dock_draw_current_path_opengl (fStringLineWidth, myIcons.fStringColor, iNbVertex);
}

void cairo_dock_draw_rounded_rectangle_opengl (double fRadius, double fLineWidth, double fFrameWidth, double fFrameHeight, double fOffsetX, double fOffsetY, double *fLineColor)
{
	int iNbVertex = 0;
	const GLfloat *pVertexTab = cairo_dock_generate_rectangle_path (fFrameWidth, fFrameHeight, fRadius, TRUE, &iNbVertex);
	
	if (fLineWidth == 0)
	{
		if (fLineColor != NULL)
			glColor4f (fLineColor[0], fLineColor[1], fLineColor[2], fLineColor[3]);
		cairo_dock_draw_frame_background_opengl (0, fFrameWidth+2*fRadius, fFrameHeight, fOffsetX, fOffsetY, pVertexTab, iNbVertex, CAIRO_DOCK_HORIZONTAL, TRUE, 0.);
	}
	else
	{
		_cairo_dock_set_vertex_pointer (pVertexTab);
		glTranslatef ((int) (fOffsetX + fFrameWidth/2), (int) (fOffsetY - fFrameHeight/2), -1);
		glScalef (fFrameHeight, fFrameWidth, 1.);
		cairo_dock_draw_current_path_opengl (fLineWidth, fLineColor, iNbVertex);
	}
}


  ///////////////
 /// GL PATH ///
///////////////

#define _CD_PATH_DIM 2
#define _cd_gl_path_set_nth_vertex_x(pPath, _x, i) pPath->pVertices[_CD_PATH_DIM*(i)] = _x
#define _cd_gl_path_set_nth_vertex_y(pPath, _y, i) pPath->pVertices[_CD_PATH_DIM*(i)+1] = _y
#define _cd_gl_path_set_vertex_x(pPath, _x) _cd_gl_path_set_nth_vertex_x (pPath, _x, pPath->iCurrentPt)
#define _cd_gl_path_set_vertex_y(pPath, _y) _cd_gl_path_set_nth_vertex_y (pPath, _y, pPath->iCurrentPt)
#define _cd_gl_path_set_current_vertex(pPath, _x, _y) do {\
	_cd_gl_path_set_vertex_x(pPath, _x);\
	_cd_gl_path_set_vertex_y(pPath, _y); } while (0)
#define _cd_gl_path_get_nth_vertex_x(pPath, i) pPath->pVertices[_CAIRO_DOCK_PATH_DIM*(i)]
#define _cd_gl_path_get_nth_vertex_y(pPath, i) pPath->pVertices[_CAIRO_DOCK_PATH_DIM*(i)+1]
#define _cd_gl_path_get_current_vertex_x(pPath) _cd_gl_path_get_nth_vertex_x (pPath, pPath->iCurrentPt-1)
#define _cd_gl_path_get_current_vertex_y(pPath) _cd_gl_path_get_nth_vertex_y (pPath, pPath->iCurrentPt-1)

CairoDockGLPath *cairo_dock_new_gl_path (int iNbVertices, double x0, double y0)
{
	CairoDockGLPath *pPath = g_new0 (CairoDockGLPath, 1);
	pPath->pVertices = g_new0 (GLfloat, (iNbVertices+1) * _CD_PATH_DIM);  // +1 = securite
	pPath->iNbPoints = iNbVertices;
	_cd_gl_path_set_current_vertex (pPath, x0, y0);
	pPath->iCurrentPt ++;
	return pPath;
}

void cairo_dock_free_gl_path (CairoDockGLPath *pPath)
{
	if (!pPath)
		return;
	g_free (pPath->pVertices);
	g_free (pPath);
}

void cairo_dock_gl_path_line_to (CairoDockGLPath *pPath, GLfloat x, GLfloat y)
{
	g_return_if_fail (pPath->iCurrentPt < pPath->iNbPoints);
	_cd_gl_path_set_current_vertex (pPath, x, y);
	pPath->iCurrentPt ++;
}

void cairo_dock_gl_path_rel_line_to (CairoDockGLPath *pPath, GLfloat dx, GLfloat dy)
{
	cairo_dock_gl_path_line_to (pPath,
		_cd_gl_path_get_current_vertex_x (pPath) + dx,
		_cd_gl_path_get_current_vertex_y (pPath) + dy);
}

void cairo_dock_gl_path_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2, GLfloat x3, GLfloat y3)
{
	g_return_if_fail (pPath->iCurrentPt + iNbPoints <= pPath->iNbPoints);
	GLfloat x0, y0;
	x0 = _cd_gl_path_get_current_vertex_x (pPath);
	y0 = _cd_gl_path_get_current_vertex_y (pPath);
	double t;
	int i;
	for (i = 0; i < iNbPoints; i ++)
	{
		t = (double)(i+1)/iNbPoints;  // [0;1]
		_cd_gl_path_set_nth_vertex_x (pPath, Bezier (x0, x1, x2, x3, t), pPath->iCurrentPt + i);
		_cd_gl_path_set_nth_vertex_y (pPath, Bezier (y0, y1, y2, y3, t), pPath->iCurrentPt + i);
	}
	pPath->iCurrentPt += iNbPoints;
}

void cairo_dock_gl_path_rel_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2, GLfloat dx3, GLfloat dy3)
{
	GLfloat x0, y0;
	x0 = _cd_gl_path_get_current_vertex_x (pPath);
	y0 = _cd_gl_path_get_current_vertex_y (pPath);
	cairo_dock_gl_path_curve_to (pPath, iNbPoints, x0 + dx1, y0 + dy1, x0 + dx2, y0 + dy2, x0 + dx3, y0 + dy3);
}

void cairo_dock_gl_path_simple_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	g_return_if_fail (pPath->iCurrentPt + iNbPoints <= pPath->iNbPoints);
	GLfloat x0, y0;
	x0 = _cd_gl_path_get_current_vertex_x (pPath);
	y0 = _cd_gl_path_get_current_vertex_y (pPath);
	double t;
	int i;
	for (i = 0; i < iNbPoints; i ++)
	{
		t = (double)(i+1)/iNbPoints;  // [0;1]
		_cd_gl_path_set_nth_vertex_x (pPath, Bezier2 (x0, x1, x2, t), pPath->iCurrentPt + i);
		_cd_gl_path_set_nth_vertex_y (pPath, Bezier2 (y0, y1, y2, t), pPath->iCurrentPt + i);
	}
	pPath->iCurrentPt += iNbPoints;
}

void cairo_dock_gl_path_rel_simple_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2)
{
	GLfloat x0, y0;
	x0 = _cd_gl_path_get_current_vertex_x (pPath);
	y0 = _cd_gl_path_get_current_vertex_y (pPath);
	cairo_dock_gl_path_simple_curve_to (pPath, iNbPoints, x0 + dx1, y0 + dy1, x0 + dx2, y0 + dy2);
}

void cairo_dock_gl_path_arc_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat xc, GLfloat yc, double cone)
{
	g_return_if_fail (pPath->iCurrentPt + iNbPoints <= pPath->iNbPoints);
	GLfloat x0, y0;
	x0 = _cd_gl_path_get_current_vertex_x (pPath);
	y0 = _cd_gl_path_get_current_vertex_y (pPath);
	double teta0 = atan2 (y0 - yc, x0 - xc);
	double r = sqrt ((x0 - xc) * (x0 - xc) + (y0 - yc) * (y0 - yc));
	double t;
	int i;
	for (i = 0; i < iNbPoints; i ++)
	{
		t = teta0 + (double)(i+1)/iNbPoints * (cone - teta0);  // [teta0, teta0+cone]
		_cd_gl_path_set_nth_vertex_x (pPath, xc + r * cos (t), pPath->iCurrentPt + i);
		_cd_gl_path_set_nth_vertex_y (pPath, xc + r * sin (t), pPath->iCurrentPt + i);
	}
	pPath->iCurrentPt += iNbPoints;
}

void cairo_dock_close_gl_path (CairoDockGLPath *pPath)
{
	g_return_if_fail (pPath->iCurrentPt < pPath->iNbPoints);
	double x0, y0;
	x0 = _cd_gl_path_get_nth_vertex_x (pPath, 0);
	y0 = _cd_gl_path_get_nth_vertex_y (pPath, 0);
	_cd_gl_path_set_current_vertex (pPath, x0, y0);
	pPath->iCurrentPt ++;
}

void cairo_dock_gl_path_move_to (CairoDockGLPath *pPath, double x0, double y0)
{
	pPath->iCurrentPt = 0;
	_cd_gl_path_set_current_vertex (pPath, x0, y0);
	pPath->iCurrentPt ++;
}

void cairo_dock_stroke_gl_path (CairoDockGLPath *pPath, gboolean bFill)
{
	glPolygonMode (GL_FRONT, bFill ? GL_FILL : GL_LINE);
	glEnable (GL_LINE_SMOOTH);
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
	//glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
	
	//glLineWidth(fLineWidth);
	//glColor4f (fLineColor[0], fLineColor[1], fLineColor[2], fLineColor[3]); // Et sa couleur.
	
	glEnableClientState (GL_VERTEX_ARRAY);
	glVertexPointer (_CD_PATH_DIM, GL_FLOAT, 0, pPath->pVertices);
	glDrawArrays (bFill ? GL_POLYGON : GL_LINE_STRIP, 0, pPath->iCurrentPt);  // GL_LINE_LOOP / GL_POLYGON
	glDisableClientState (GL_VERTEX_ARRAY);
	
	glDisable (GL_LINE_SMOOTH);
	glDisable (GL_BLEND);
}



// A utiliser un jour.

typedef struct _CairoAnimatedImage {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	gint iFrameWidth, iFrameHeight;
	gint iNbFrames;
	gint iCurrentFrame;
	} CairoAnimatedImage;

void cairo_dock_load_animated_image (const gchar *cImageFile, int iNbFrames, int iFrameWidth, int iFrameHeight, cairo_t *pSourceContext, CairoAnimatedImage *pAnimatedImage)
{
	pAnimatedImage->iNbFrames = iNbFrames;
	pAnimatedImage->iCurrentFrame = 0;
	
	pAnimatedImage->iFrameWidth = iFrameWidth * iNbFrames;
	pAnimatedImage->iFrameHeight = iFrameHeight;
	pAnimatedImage->pSurface = cairo_dock_create_surface_from_image_simple (cImageFile,
			pAnimatedImage->iFrameWidth,
			pAnimatedImage->iFrameHeight);
	
	if (g_bUseOpenGL && pAnimatedImage->pSurface != NULL)
	{
		pAnimatedImage->iTexture = cairo_dock_create_texture_from_surface (pAnimatedImage->pSurface);
	}
}

CairoAnimatedImage *cairo_dock_create_animated_image (const gchar *cImageFile, int iNbFrames, int iFrameWidth, int iFrameHeight, cairo_t *pSourceContext)
{
	CairoAnimatedImage *pAnimatedImage = g_new0 (CairoAnimatedImage, 1);
	
	cairo_dock_load_animated_image (cImageFile, iNbFrames, iFrameWidth, iFrameHeight, pSourceContext, pAnimatedImage);
	
	return pAnimatedImage;
}

#define cairo_dock_update_animated_image_state(pAnimatedImage) do {\
	(pAnimatedImage)->iCurrentFrame ++;\
	if ((pAnimatedImage)->iCurrentFrame == (pAnimatedImage)->iNbFrames)\
		(pAnimatedImage)->iCurrentFrame = 0; } while (0)

void cairo_dock_update_animated_image_cairo (CairoAnimatedImage *pAnimatedImage, cairo_t *pCairoContext)
{
	cairo_dock_update_animated_image_state (pAnimatedImage);
	cairo_save (pCairoContext);
	cairo_rectangle (pCairoContext, 0., 0., pAnimatedImage->iFrameWidth, pAnimatedImage->iFrameHeight);
	cairo_clip (pCairoContext);
	cairo_set_source_surface (pCairoContext,
		pAnimatedImage->pSurface,
		- pAnimatedImage->iFrameWidth * pAnimatedImage->iCurrentFrame,
		0.);
	cairo_restore (pCairoContext);
}

void cairo_dock_update_animated_image_opengl (CairoAnimatedImage *pAnimatedImage)
{
	cairo_dock_update_animated_image_state (pAnimatedImage);
	_cairo_dock_apply_current_texture_portion_at_size_with_offset (1.*pAnimatedImage->iCurrentFrame/pAnimatedImage->iNbFrames, 0.,
		1. / pAnimatedImage->iNbFrames, 1.,
		pAnimatedImage->iFrameWidth, pAnimatedImage->iFrameHeight,
		0., 0.);
}







typedef struct _CairoDockOpenglPath {
	gint iNbVertices;
	GLfloat *pVertexTab;
	GLfloat *pColorTab;
	gint iCurrentIndex;
	gint iWidthExtent, iHeightExtent;
	} CairoDockOpenglPath;

#define _cairo_dock_ith_vertex_x(pVertexPath, i) (pVertexPath)->pVertexTab[i*_CAIRO_DOCK_PATH_DIM]
#define _cairo_dock_ith_vertex_y(pVertexPath, i) (pVertexPath)->pVertexTab[i*_CAIRO_DOCK_PATH_DIM+1]
#define _cairo_dock_set_ith_vertex_x(pVertexPath, i, x) _cairo_dock_ith_vertex_x(pVertexPath, i) = x
#define _cairo_dock_set_ith_vertex_y(pVertexPath, i, y) _cairo_dock_ith_vertex_y(pVertexPath, i) = y
#define cairo_dock_get_current_vertex_x(pVertexPath) _cairo_dock_ith_vertex_x(pVertexPath, pVertexPath->iCurrentIndex-1)
#define cairo_dock_get_current_vertex_y(pVertexPath) _cairo_dock_ith_vertex_y(pVertexPath, pVertexPath->iCurrentIndex-1)
#define cairo_dock_add_vertex(pVertexPath, x, y) do {\
	_cairo_dock_set_ith_vertex_x (pVertexPath, (pVertexPath)->iCurrentIndex, x);\
	_cairo_dock_set_ith_vertex_y (pVertexPath, (pVertexPath)->iCurrentIndex, y);\
	(pVertexPath)->iCurrentIndex ++; } while (0)

void cairo_dock_add_curved_subpath_opengl (CairoDockOpenglPath *pVertexPath, int iNbPts, double x1, double y1, double x2, double y2, double x3, double y3)
{
	double x0 = cairo_dock_get_current_vertex_x (pVertexPath);  // doit avoir ete initialise.
	double y0 = cairo_dock_get_current_vertex_y (pVertexPath);
	double t;
	int i;
	for (i = 1; i < iNbPts+1; i ++)
	{
		t = 1.*i/iNbPts;  // ]0;1]
		cairo_dock_add_vertex(pVertexPath,
			Bezier (x0,x1,x2,x3,t),
			Bezier (y0,y1,y2,y3,t));
		//vx(i) = Bezier (x0,x1,x2,x3,t);
		//vy(i) = Bezier (y0,y1,y2,y3,t);
	}
}


  ///////////////
 /// GL FONT ///
///////////////
 
GLuint cairo_dock_create_texture_from_text_simple (const gchar *cText, const gchar *cFontDescription, cairo_t* pSourceContext, int *iWidth, int *iHeight)
{
	g_return_val_if_fail (cText != NULL && cFontDescription != NULL, 0);
	
	//\_________________ On ecrit le texte dans un calque Pango.
	PangoLayout *pLayout = pango_cairo_create_layout (pSourceContext);
	
	PangoFontDescription *fd = pango_font_description_from_string (cFontDescription);
	pango_layout_set_font_description (pLayout, fd);
	pango_font_description_free (fd);
	
	pango_layout_set_text (pLayout, cText, -1);
	
	//\_________________ On cree une surface aux dimensions du texte.
	PangoRectangle ink, log;
	pango_layout_get_pixel_extents (pLayout, &ink, &log);
	cairo_surface_t* pNewSurface = cairo_dock_create_blank_surface (
		log.width,
		log.height);
	*iWidth = log.width;
	*iHeight = log.height;
	
	//\_________________ On dessine le texte.
	cairo_t* pCairoContext = cairo_create (pNewSurface);
	cairo_translate (pCairoContext, -log.x, -log.y);
	cairo_set_source_rgb (pCairoContext, 1., 1., 1.);
	cairo_move_to (pCairoContext, 0, 0);
	pango_cairo_show_layout (pCairoContext, pLayout);
	cairo_destroy (pCairoContext);
	
	g_object_unref (pLayout);
	
	//\_________________ On cree la texture.
	GLuint iTexture = cairo_dock_create_texture_from_surface (pNewSurface);
	cairo_surface_destroy (pNewSurface);
	return iTexture;
}


CairoDockGLFont *cairo_dock_load_bitmap_font (const gchar *cFontDescription, int first, int count)
{
	g_return_val_if_fail (cFontDescription != NULL && count > 0, NULL);
	
	GLuint iListBase = glGenLists (count);
	g_return_val_if_fail (iListBase != 0, NULL);
	
	CairoDockGLFont *pFont = g_new0 (CairoDockGLFont, 1);
	pFont->iListBase = iListBase;
	pFont->iNbChars = count;
	pFont->iCharBase = first;
	
	PangoFontDescription *fd = pango_font_description_from_string (cFontDescription);
	PangoFont *font = gdk_gl_font_use_pango_font (fd,
		first,
		count,
		iListBase);
	g_return_val_if_fail (font != NULL, NULL);
	
	PangoFontMetrics* metrics = pango_font_get_metrics (font, NULL);
	pFont->iCharWidth = pango_font_metrics_get_approximate_char_width (metrics) / PANGO_SCALE;
	pFont->iCharHeight = (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics)) / PANGO_SCALE;
	pango_font_metrics_unref(metrics);
	
	pango_font_description_free (fd);
	return pFont;
}

CairoDockGLFont *cairo_dock_load_textured_font (const gchar *cFontDescription, int first, int count)
{
	g_return_val_if_fail (g_pMainDock != NULL && count > 0, NULL);
	if (first < 32)  // 32 = ' '
	{
		count -= (32 - first);
		first = 32;
	}
	gchar *cPool = g_new0 (gchar, 4*count + 1);
	int i, j=0;
	guchar c;
	for (i = 0; i < count; i ++)
	{
		c = first + i;
		if (c > 254)
		{
			count=i;
			break;
		}
		if ((c > 126 && c < 126 + 37) || (c == 173))  // le 173 est un caractere bizarre (sa taille est nulle).
		{
			cPool[j++] = ' ';
		}
		else
		{
			j += MAX (0, sprintf (cPool+j, "%lc", c));  // les caracteres ASCII >128 doivent etre convertis en multi-octets.
		}
	}
	cd_debug ("%s (%d + %d -> '%s')", __func__, first, count, cPool);
	/*iconv_t cd = iconv_open("UTF-8", "ISO-8859-1");
	gchar *outbuf = g_new0 (gchar, count*4+1);
	gchar *outbuf0 = outbuf, *inbuf0 = cPool;
	size_t inbytesleft = count;
	size_t outbytesleft = count*4;
	size_t size = iconv (cd,
		&cPool, &inbytesleft,
		&outbuf, &outbytesleft);
	cd_debug ("%d bytes left, %d bytes written => '%s'\n", inbytesleft, outbytesleft, outbuf0);
	g_free (inbuf0);
	cPool = outbuf0;
	iconv_close (cd);*/
	
	int iWidth, iHeight;
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (g_pMainDock));
	GLuint iTexture = cairo_dock_create_texture_from_text_simple (cPool, cFontDescription, pCairoContext, &iWidth, &iHeight);
	cairo_destroy (pCairoContext);
	g_free (cPool);
	
	CairoDockGLFont *pFont = g_new0 (CairoDockGLFont, 1);
	pFont->iTexture = iTexture;
	pFont->iNbChars = count;
	pFont->iCharBase = first;
	pFont->iNbRows = 1;
	pFont->iNbColumns = count;
	pFont->iCharWidth = (double)iWidth / count;
	pFont->iCharHeight = iHeight;
	
	cd_debug ("%d char / %d pixels => %.3f", count, iWidth, (double)iWidth / count);
	return pFont;
}

CairoDockGLFont *cairo_dock_load_textured_font_from_image (const gchar *cImagePath)
{
	double fImageWidth, fImageHeight;
	GLuint iTexture = cairo_dock_create_texture_from_image_full (cImagePath, &fImageWidth, &fImageHeight);
	g_return_val_if_fail (iTexture != 0, NULL);
	
	CairoDockGLFont *pFont = g_new0 (CairoDockGLFont, 1);
	pFont->iTexture = iTexture;
	pFont->iNbChars = 256;
	pFont->iCharBase = 0;
	pFont->iNbRows = 16;
	pFont->iNbColumns = 16;
	pFont->iCharWidth = fImageWidth / pFont->iNbColumns;
	pFont->iCharHeight = fImageHeight / pFont->iNbRows;
	return pFont;
}

void cairo_dock_free_gl_font (CairoDockGLFont *pFont)
{
	if (pFont == NULL)
		return ;
	if (pFont->iListBase != 0)
		glDeleteLists (pFont->iListBase, pFont->iNbChars);
	if (pFont->iTexture != 0)
		_cairo_dock_delete_texture (pFont->iTexture);
	g_free (pFont);
}


void cairo_dock_get_gl_text_extent (const gchar *cText, CairoDockGLFont *pFont, int *iWidth, int *iHeight)
{
	if (pFont == NULL || cText == NULL)
	{
		*iWidth = 0;
		*iHeight = 0;
		return ;
	}
	int i, w=0, wmax=0, h=pFont->iCharHeight;
	for (i = 0; cText[i] != '\0'; i ++)
	{
		if (cText[i] == '\n')
		{
			h += pFont->iCharHeight + 1;
			wmax = MAX (wmax, w);
			w = 0;
		}
		else
			w += pFont->iCharWidth;
	}
	
	*iWidth = MAX (wmax, w);
	*iHeight = h;
}


void cairo_dock_draw_gl_text (const guchar *cText, CairoDockGLFont *pFont)
{
	int n = strlen (cText);
	if (pFont->iListBase != 0)
	{
		if (pFont->iCharBase == 0 && strchr (cText, '\n') == NULL)  // version optimisee ou on a charge tous les caracteres.
		{
			glDisable (GL_TEXTURE_2D);
			glListBase (pFont->iListBase);
			glCallLists (n, GL_UNSIGNED_BYTE, (unsigned char *)cText);
			glListBase (0);
		}
		else
		{
			int i, j;
			for (i = 0; i < n; i ++)
			{
				if (cText[i] == '\n')
				{
					GLfloat rpos[4];
					glGetFloatv (GL_CURRENT_RASTER_POSITION, rpos);
					glRasterPos2f (rpos[0], rpos[1] + pFont->iCharHeight + 1);
					continue;
				}
				if (cText[i] < pFont->iCharBase || cText[i] >= pFont->iCharBase + pFont->iNbChars)
					continue;
				j = cText[i] - pFont->iCharBase;
				glCallList (pFont->iListBase + j);
			}
		}
	}
	else if (pFont->iTexture != 0)
	{
		_cairo_dock_enable_texture ();
		glBindTexture (GL_TEXTURE_2D, pFont->iTexture);
		double u, v, du=1./pFont->iNbColumns, dv=1./pFont->iNbRows, w=pFont->iCharWidth, h=pFont->iCharHeight, x=.5*w, y=.5*h;
		int i, j;
		for (i = 0; i < n; i ++)
		{
			if (cText[i] == '\n')
			{
				x = .5*pFont->iCharWidth;
				y += pFont->iCharHeight + 1;
				continue;
			}
			if (cText[i] < pFont->iCharBase || cText[i] >= pFont->iCharBase + pFont->iNbChars)
				continue;
			
			j = cText[i] - pFont->iCharBase;
			u = (double) (j%pFont->iNbColumns) / pFont->iNbColumns;
			v = (double) (j/pFont->iNbColumns) / pFont->iNbRows;
			_cairo_dock_apply_current_texture_portion_at_size_with_offset (u, v, du, dv, w, h, x, y);
			x += pFont->iCharWidth;
		}
		_cairo_dock_disable_texture ();
	}
}

void cairo_dock_draw_gl_text_in_area (const guchar *cText, CairoDockGLFont *pFont, int iWidth, int iHeight, gboolean bCentered)
{
	g_return_if_fail (pFont != NULL && cText != NULL);
	if (pFont->iListBase != 0)  // marche po sur du raster.
	{
		cd_warning ("can't resize raster ! use a textured font inside.");
	}
	else
	{
		int n = strlen (cText);
		int w, h;
		cairo_dock_get_gl_text_extent (cText, pFont, &w, &h);
		
		double zx, zy;
		if (fabs ((double)iWidth/w) < fabs ((double)iHeight/h))  // on autorise les dimensions negatives pour pouvoir retourner le texte.
		{
			zx = (double)iWidth/w;
			zy = (iWidth*iHeight > 0 ? zx : -zx);
		}
		else
		{
			zy = (double)iHeight/h;
			zx = (iWidth*iHeight > 0 ? zy : -zy);
		}
		
		glScalef (zx, zy, 1.);
		if (bCentered)
			glTranslatef (-w/2, -h/2, 0.);
		cairo_dock_draw_gl_text (cText, pFont);
	}
}

void cairo_dock_draw_gl_text_at_position (const guchar *cText, CairoDockGLFont *pFont, int x, int y)
{
	g_return_if_fail (pFont != NULL && cText != NULL);
	if (pFont->iListBase != 0)
	{
		glRasterPos2f (x, y);
	}
	else
	{
		glTranslatef (x, y, 0);
	}
	cairo_dock_draw_gl_text (cText, pFont);
}

void cairo_dock_draw_gl_text_at_position_in_area (const guchar *cText, CairoDockGLFont *pFont, int x, int y, int iWidth, int iHeight, gboolean bCentered)
{
	g_return_if_fail (pFont != NULL && cText != NULL);
	if (pFont->iListBase != 0)  // marche po sur du raster.
	{
		cd_warning ("can't resize raster ! use a textured font inside.");
	}
	else
	{
		glTranslatef (x, y, 0);
		cairo_dock_draw_gl_text_in_area (cText, pFont, iWidth, iHeight, bCentered);
	}
}



typedef void (*GLXBindTexImageProc) (Display *display, GLXDrawable drawable, int buffer, int *attribList);
typedef void (*GLXReleaseTexImageProc) (Display *display, GLXDrawable drawable, int buffer);

// Bind redirected window to texture:
GLuint cairo_dock_texture_from_pixmap (Window Xid, Pixmap iBackingPixmap)
{
	if (!g_bEasterEggs)
		return 0;  /// ca ne marche pas. :-(
	
	if (! g_openglConfig.bTextureFromPixmapAvailable)
		return 0;
	
	Display *display = gdk_x11_get_default_xdisplay ();
	XWindowAttributes attrib;
	XGetWindowAttributes (display, Xid, &attrib);
	
	VisualID visualid = XVisualIDFromVisual (attrib.visual);
	
	int nfbconfigs;
	int screen = 0;
	GLXFBConfig *fbconfigs = glXGetFBConfigs (display, screen, &nfbconfigs);
	
	GLfloat top=0., bottom=0.;
	XVisualInfo *visinfo;
	int value;
	int i;
	for (i = 0; i < nfbconfigs; i++)
	{
		visinfo = glXGetVisualFromFBConfig (display, fbconfigs[i]);
		if (!visinfo || visinfo->visualid != visualid)
			continue;
	
		glXGetFBConfigAttrib (display, fbconfigs[i], GLX_DRAWABLE_TYPE, &value);
		if (!(value & GLX_PIXMAP_BIT))
			continue;
	
		glXGetFBConfigAttrib (display, fbconfigs[i],
			GLX_BIND_TO_TEXTURE_TARGETS_EXT,
			&value);
		if (!(value & GLX_TEXTURE_2D_BIT_EXT))
			continue;
		
		glXGetFBConfigAttrib (display, fbconfigs[i],
			GLX_BIND_TO_TEXTURE_RGBA_EXT,
			&value);
		if (value == FALSE)
		{
			glXGetFBConfigAttrib (display, fbconfigs[i],
				GLX_BIND_TO_TEXTURE_RGB_EXT,
				&value);
			if (value == FALSE)
				continue;
		}
		
		glXGetFBConfigAttrib (display, fbconfigs[i],
			GLX_Y_INVERTED_EXT,
			&value);
		if (value == TRUE)
		{
			top = 0.0f;
			bottom = 1.0f;
		}
		else
		{
			top = 1.0f;
			bottom = 0.0f;
		}
		
		break;
	}
	
	if (i == nfbconfigs)
	{
		cd_warning ("No FB Config found");
		return 0;
	}
	
	int pixmapAttribs[5] = { GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
		GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
		None };
	GLXPixmap glxpixmap = glXCreatePixmap (display, fbconfigs[i], iBackingPixmap, pixmapAttribs);
	g_return_val_if_fail (glxpixmap != 0, 0);
	
	GLuint texture;
	glEnable (GL_TEXTURE_2D);
	glGenTextures (1, &texture);
	glBindTexture (GL_TEXTURE_2D, texture);
	
	g_openglConfig.bindTexImage (display, glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
	
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	// draw using iBackingPixmap as texture
	glBegin (GL_QUADS);
	
	glTexCoord2d (0.0f, bottom);
	glVertex2d (0.0f, 0.0f);
	
	glTexCoord2d (0.0f, top);
	glVertex2d (0.0f, attrib.height);
	
	glTexCoord2d (1.0f, top);
	glVertex2d (attrib.width, attrib.height);
	
	glTexCoord2d (1.0f, bottom);
	glVertex2d (attrib.width, 0.0f);
	
	glEnd ();
	glDisable (GL_TEXTURE_2D);
	
	g_openglConfig.releaseTexImage (display, glxpixmap, GLX_FRONT_LEFT_EXT);
	glXDestroyGLXPixmap (display, glxpixmap);
	return texture;
}
