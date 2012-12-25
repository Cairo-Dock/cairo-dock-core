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


#include <X11/extensions/Xrender.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.fVisibleAppliAlpha
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-overlay.h"
#include "cairo-dock-opengl-path.h"
#include "cairo-dock-draw-opengl.h"

#include "texture-gradation.h"
#define RADIAN (G_PI / 180.0)  // Conversion Radian/Degres
#define DELTA_ROUND_DEGREE 3

extern GLuint g_pGradationTexture[2];

extern CairoContainer *g_pPrimaryContainer;

extern CairoDockImageBuffer g_pVisibleZoneBuffer;

extern gboolean g_bUseOpenGL;
extern CairoDockGLConfig g_openglConfig;

extern gboolean g_bEasterEggs;


void cairo_dock_set_icon_scale (Icon *pIcon, CairoContainer *pContainer, double fZoomFactor)
{
	double fSizeX, fSizeY;
	cairo_dock_get_current_icon_size (pIcon, pContainer, &fSizeX, &fSizeY);
	glScalef (fSizeX * fZoomFactor, fSizeY * fZoomFactor, fSizeY * fZoomFactor);
}

void cairo_dock_set_container_orientation_opengl (CairoContainer *pContainer)
{
	if (pContainer->bIsHorizontal)
	{
		if (! pContainer->bDirectionUp)
		{
			glTranslatef (0., pContainer->iHeight, 0.);
			glScalef (1., -1., 1.);
		}
	}
	else
	{
		glTranslatef (pContainer->iHeight/2, pContainer->iWidth/2, 0.);
		glRotatef (-90., 0., 0., 1.);
		if (pContainer->bDirectionUp)
			glScalef (1., -1., 1.);
		glTranslatef (-pContainer->iWidth/2, -pContainer->iHeight/2, 0.);
	}
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

void cairo_dock_draw_icon_reflect_opengl (Icon *pIcon, CairoDock *pDock)
{
	if (pDock->container.bUseReflect)
	{
		if (pDock->pRenderer->bUseStencil && g_openglConfig.bStencilBufferAvailable)
		{
			glEnable (GL_STENCIL_TEST);
			glStencilFunc (GL_EQUAL, 1, 1);
			glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
		}
		glPushMatrix ();
		double x0, y0, x1, y1;
		double fScale = ((myIconsParam.bConstantSeparatorSize && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon)) ? 1. : pIcon->fScale);
		///double fReflectSize = MIN (myIconsParam.fReflectSize, pIcon->fHeight/pDock->container.fRatio*fScale);
		double fReflectSize = pIcon->fHeight * myIconsParam.fReflectHeightRatio * fScale;
		///double fReflectRatio = fReflectSize * pDock->container.fRatio / pIcon->fHeight / fScale  / pIcon->fHeightFactor;
		double fReflectRatio = myIconsParam.fReflectHeightRatio;
		double fOffsetY = pIcon->fHeight * fScale/2 + fReflectSize/** * pDock->container.fRatio*/ / 2 + pIcon->fDeltaYReflection;
		if (pDock->container.bIsHorizontal)
		{
			if (pDock->container.bDirectionUp)
			{
				glTranslatef (0., - fOffsetY, 0.);
				glScalef (pIcon->fWidth * pIcon->fWidthFactor * fScale, - fReflectSize/** * pDock->container.fRatio*/, 1.);  // taille du reflet et on se retourne.
				x0 = 0.;
				y0 = 1. - fReflectRatio;
				x1 = 1.;
				y1 = 1.;
			}
			else
			{
				glTranslatef (0., fOffsetY, 0.);
				glScalef (pIcon->fWidth * pIcon->fWidthFactor * fScale, fReflectSize/** * pDock->container.fRatio*/, 1.);
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
				glScalef (- fReflectSize/** * pDock->container.fRatio*/, pIcon->fWidth * pIcon->fWidthFactor * fScale, 1.);
				x0 = 1. - fReflectRatio;
				y0 = 0.;
				x1 = 1.;
				y1 = 1.;
			}
			else
			{
				glTranslatef (- fOffsetY, 0., 0.);
				glScalef (fReflectSize/** * pDock->container.fRatio*/, pIcon->fWidth * pIcon->fWidthFactor * fScale, 1.);
				x0 = fReflectRatio;
				y0 = 0.;
				x1 = 0.;
				y1 = 1.;
			}
		}
		
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, pIcon->image.iTexture);
		glEnable(GL_BLEND);
		_cairo_dock_set_blend_alpha ();
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		
		//glBlendColor(1., 1., 1., 1.);  // utile ?
		
		glPolygonMode (GL_FRONT, GL_FILL);
		glColor4f(1., 1., 1., 1.);
		
		glBegin(GL_QUADS);
		
		double fReflectAlpha = myIconsParam.fAlbedo * pIcon->fAlpha;
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
		if (pDock->pRenderer->bUseStencil && g_openglConfig.bStencilBufferAvailable)
		{
			glDisable (GL_STENCIL_TEST);
		}
	}
}

void cairo_dock_draw_icon_opengl (Icon *pIcon, CairoDock *pDock)
{
	//\_____________________ On dessine l'icone.
	double fSizeX, fSizeY;
	cairo_dock_get_current_icon_size (pIcon, CAIRO_CONTAINER (pDock), &fSizeX, &fSizeY);
	
	_cairo_dock_enable_texture ();
	if (pIcon->fAlpha == 1)
		_cairo_dock_set_blend_pbuffer ();
	else
		_cairo_dock_set_blend_alpha ();
	_cairo_dock_apply_texture_at_size_with_alpha (pIcon->image.iTexture, fSizeX, fSizeY, pIcon->fAlpha);
	//if (g_strcmp0 (pIcon->cName, "Calculatrice") == 0)
		//g_print ("%s: %.2f\n", pIcon->cName, pIcon->fAlpha);
	//\_____________________ On dessine son reflet.
	cairo_dock_draw_icon_reflect_opengl (pIcon, pDock);
	
	_cairo_dock_disable_texture ();
}


static inline void _compute_icon_coordinate (Icon *icon, CairoContainer *pContainer, double fDockMagnitude, double *pX, double *pY)
{
	double fX=0, fY=0;
	double fRatio = pContainer->fRatio;
	double fGlideScale;
	if (icon->fGlideOffset != 0)
	{
		double fPhase =  icon->fPhase + icon->fGlideOffset * icon->fWidth / fRatio / myIconsParam.iSinusoidWidth * G_PI;
		if (fPhase < 0)
		{
			fPhase = 0;
		}
		else if (fPhase > G_PI)
		{
			fPhase = G_PI;
		}
		fGlideScale = (1 + fDockMagnitude * myIconsParam.fAmplitude * sin (fPhase)) / icon->fScale;  // c'est un peu hacky ... il faudrait passer l'icone precedente en parametre ...
		if (! pContainer->bDirectionUp)
		{
			if (pContainer->bIsHorizontal)
				fY = (1-fGlideScale)*icon->fHeight*icon->fScale;
			else
				fX = (1-fGlideScale)*icon->fHeight*icon->fScale;
		}
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
	double fMaxScale = cairo_dock_get_icon_max_scale (icon);
	
	if (pContainer->bIsHorizontal)
		glTranslatef ( (fX),  (fY - icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2)), - icon->fHeight * fMaxScale);
	else
		glTranslatef ( (fY + icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2)),  (fX), - icon->fHeight * fMaxScale);
}

void cairo_dock_render_one_icon_opengl (Icon *icon, CairoDock *pDock, double fDockMagnitude, gboolean bUseText)
{
	if (icon->image.iTexture == 0)
		return ;
	double fRatio = pDock->container.fRatio;
	
	if (g_pGradationTexture[pDock->container.bIsHorizontal] == 0)
	{
		//g_pGradationTexture[pDock->container.bIsHorizontal] = cairo_dock_load_local_texture (pDock->container.bIsHorizontal ? "texture-gradation-vert.png" : "texture-gradation-horiz.png", GLDI_SHARE_DATA_DIR);
		g_pGradationTexture[pDock->container.bIsHorizontal] = cairo_dock_create_texture_from_raw_data (gradationTex,
			pDock->container.bIsHorizontal ? 1:48,
			pDock->container.bIsHorizontal ? 48:1);
		cd_debug ("g_pGradationTexture(%d) <- %d", pDock->container.bIsHorizontal, g_pGradationTexture[pDock->container.bIsHorizontal]);
	}
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskbarParam.fVisibleAppliAlpha != 0 && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && !(icon->iBackingPixmap != 0 && icon->bIsHidden))
	{
		double fAlpha = (icon->bIsHidden ? MIN (1 - myTaskbarParam.fVisibleAppliAlpha, 1) : MIN (myTaskbarParam.fVisibleAppliAlpha + 1, 1));
		if (fAlpha != 1)
			icon->fAlpha = fAlpha;  // astuce bidon pour pas multiplier 2 fois.
	}
	
	//\_____________________ On se place au centre de l'icone.
	double fX=0, fY=0;
	_compute_icon_coordinate (icon, CAIRO_CONTAINER (pDock), fDockMagnitude * pDock->fMagnitudeMax, &fX, &fY);
	
	glPushMatrix ();
	if (pDock->container.bIsHorizontal)
		glTranslatef (fX, fY - icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2), - icon->fHeight * icon->fScale);
	else
		glTranslatef (fY + icon->fHeight * icon->fScale * (1 - icon->fGlideScale/2), fX, - icon->fHeight * icon->fScale);
	
	//\_____________________ On positionne l'icone.
	glPushMatrix ();
	if (myIconsParam.bConstantSeparatorSize && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
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
	if (icon->fOrientation != 0)
	{
		glTranslatef (-icon->fWidth * icon->fScale/2, icon->fHeight * icon->fScale/2, 0.);
		glRotatef (-icon->fOrientation/G_PI*180., 0., 0., 1.);
		glTranslatef (icon->fWidth * icon->fScale/2, -icon->fHeight * icon->fScale/2, 0.);
	}
	if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) && myIconsParam.bRevolveSeparator)
	{
		if (pDock->container.bIsHorizontal)
		{
			if (! pDock->container.bDirectionUp)
			{
				glScalef (1., -1., 1.);
			}
		}
		else
		{
			glMatrixMode(GL_TEXTURE);
			glRotatef (-90., 0., 0., 1.);
			glMatrixMode (GL_MODELVIEW);
			if (pDock->container.bDirectionUp)
				glScalef (1., -1., 1.);
		}
	}
	if (icon->iRotationX != 0)
		glRotatef (icon->iRotationX, 1., 0., 0.);
	if (icon->iRotationY != 0)
		glRotatef (icon->iRotationY, 0., 1., 0.);
	
	//\_____________________ On dessine l'icone.
	gboolean bIconHasBeenDrawn = FALSE;
	cairo_dock_notify_on_object (&myIconsMgr, NOTIFICATION_PRE_RENDER_ICON, icon, pDock, NULL);
	cairo_dock_notify_on_object (&myIconsMgr, NOTIFICATION_RENDER_ICON, icon, pDock, &bIconHasBeenDrawn, NULL);
	
	glPopMatrix ();  // retour juste apres la translation au milieu de l'icone.
	
	if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) && myIconsParam.bRevolveSeparator && !pDock->container.bIsHorizontal)
	{
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity ();
		glMatrixMode (GL_MODELVIEW);
	}
	
	//\_____________________ Draw the overlays on top of that.
	cairo_dock_draw_icon_overlays_opengl (icon, fRatio);
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	glPopMatrix ();  // retour au debut de la fonction.
	if (bUseText && icon->label.iTexture != 0 && icon->iHideLabel == 0
	&& (icon->bPointed || (icon->fScale > 1.01 && ! myIconsParam.bLabelForPointedIconOnly)))  // 1.01 car sin(pi) = 1+epsilon :-/  //  && icon->iAnimationState < CAIRO_DOCK_STATE_CLICKED
	{
		glPushMatrix ();
		glLoadIdentity ();
		
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_alpha ();
		
		double fMagnitude;
		if (myIconsParam.bLabelForPointedIconOnly ||pDock->fMagnitudeMax == 0.)
		{
			fMagnitude = fDockMagnitude;  // (icon->fScale - 1) / myIconsParam.fAmplitude / sin (icon->fPhase);  // sin (phi ) != 0 puisque fScale > 1.
		}
		else
		{
			fMagnitude = (icon->fScale - 1) / myIconsParam.fAmplitude;  /// il faudrait diviser par pDock->fMagnitudeMax ...
			fMagnitude = pow (fMagnitude, myIconsParam.fLabelAlphaThreshold);
			///fMagnitude *= (fMagnitude * myIconsParam.fLabelAlphaThreshold + 1) / (myIconsParam.fLabelAlphaThreshold + 1);
		}
		
		double dx = .5 * (icon->label.iWidth & 1);  // on decale la texture pour la coller sur la grille des coordonnees entieres.
		double dy = .5 * (icon->label.iHeight & 1);
		
		if (pDock->container.bIsHorizontal || !myIconsParam.bTextAlwaysHorizontal)
		{
			if (fX + icon->label.iWidth/2 > pDock->container.iWidth)  // l'etiquette deborde a droite.
				fX = pDock->container.iWidth - icon->label.iWidth/2;
			if (fX - icon->label.iWidth/2 < 0)  // l'etiquette deborde a gauche.
				fX = icon->label.iWidth/2;
			
			if (pDock->container.bIsHorizontal)
				glTranslatef (floor (fX) + dx,
					pDock->container.bDirectionUp ? 
						floor (fY + myIconsParam.iLabelSize - icon->label.iHeight / 2) - dy:
						floor (fY - icon->fHeight * icon->fScale - myIconsParam.iLabelSize + icon->label.iHeight / 2) + dy,
					0.);
			else
			{
				glTranslatef (pDock->container.bDirectionUp ? 
						floor (fY - myIconsParam.iLabelSize + icon->label.iHeight / 2) - dy:
						floor (fY + icon->fHeight * icon->fScale + myIconsParam.iLabelSize - icon->label.iHeight / 2) + dy,
					floor (fX) + dx,
					0.);
				glRotatef (pDock->container.bDirectionUp ? 90 : -90, 0., 0., 1.);
			}
			if (icon->fOrientation != 0 && ! myIconsParam.bTextAlwaysHorizontal)
			{
				glTranslatef (-icon->fWidth * icon->fScale/2, icon->fHeight * icon->fScale/2, 0.);
				glRotatef (-icon->fOrientation/G_PI*180., 0., 0., 1.);
				glTranslatef (icon->fWidth * icon->fScale/2, -icon->fHeight * icon->fScale/2, 0.);
			}
			
			_cairo_dock_set_alpha (fMagnitude);
			cairo_dock_apply_image_buffer_texture (&icon->label);
		}
		else  // horizontal label on a vertical dock -> draw them next to the icon, vertically centered (like the Parabolic view)
		{
			if (icon->pSubDock && gldi_container_is_visible (CAIRO_CONTAINER (icon->pSubDock)))
			{
				fMagnitude /= 3;
			}
			
			const int pad = 3;
			int iXStick = (pDock->container.bDirectionUp ? 
				floor (fY - (myDocksParam.iDockLineWidth + myDocksParam.iFrameMargin) * (1 - pDock->fMagnitudeMax) - pad) :  // right border
				floor (fY + icon->fHeight * icon->fScale + (myDocksParam.iDockLineWidth + myDocksParam.iFrameMargin) * (1 - pDock->fMagnitudeMax) + pad));  // left border
			int  iMaxWidth = (pDock->container.bDirectionUp ?
				iXStick :
				pDock->container.iHeight - iXStick);
			
			int w;
			if (icon->label.iWidth > iMaxWidth)
			{
				w = iMaxWidth;
				dx = .5 * (w & 1);
			}
			else
			{
				w = icon->label.iWidth;
			}
			glTranslatef ((pDock->container.bDirectionUp ? 
					floor (iXStick - w/2) + dx :
					floor (iXStick + w/2) + dx),
				floor (fX) + dy,
				0.);
			
			if (icon->label.iWidth > iMaxWidth)  // draw with an alpha gradation on the last part.
			{
				cairo_dock_apply_image_buffer_texture_with_limit (&icon->label, fMagnitude, iMaxWidth);
				/*glBindTexture (GL_TEXTURE_2D, icon->label.iTexture);
				
				double h = icon->label.iHeight;
				double u0 = 0., u1 = (double) iMaxWidth / icon->label.iWidth;
				glBegin(GL_QUAD_STRIP);
				
				double a = .75; // 3/4 plain, 1/4 gradation
				a = (double) (floor ((-.5+a)*w)) / w + .5;
				glColor4f (1., 1., 1., fMagnitude);
				glTexCoord2f(u0, 0); glVertex3f (-.5*w,  .5*h, 0.);  // top left
				glTexCoord2f(u0, 1); glVertex3f (-.5*w, -.5*h, 0.);  // bottom left
				
				glTexCoord2f(u1*a, 0); glVertex3f ((-.5+a)*w,  .5*h, 0.);  // top middle
				glTexCoord2f(u1*a, 1); glVertex3f ((-.5+a)*w, -.5*h, 0.);  // bottom middle
				
				glColor4f (1., 1., 1., 0.);
				
				glTexCoord2f(u1, 0); glVertex3f (.5*w,  .5*h, 0.);  // top right
				glTexCoord2f(u1, 1); glVertex3f (.5*w, -.5*h, 0.);  // bottom right
				
				glEnd();*/
			}
			else
			{
				_cairo_dock_set_alpha (fMagnitude);
				cairo_dock_apply_image_buffer_texture_with_offset (&icon->label, 0, 0);
			}
		}
		/*_cairo_dock_set_alpha (fMagnitude);
		cairo_dock_apply_image_buffer_texture (&icon->label);*/
		_cairo_dock_disable_texture ();
		
		glPopMatrix ();
	}
}


void cairo_dock_render_hidden_dock_opengl (CairoDock *pDock)
{
	//g_print ("%s (%d, %x)\n", __func__, pDock->bIsMainDock, g_pVisibleZoneSurface);
	//\_____________________ on dessine la zone de rappel.
	///glLoadIdentity ();  // annule l'offset de cachage.
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (pDock->pRenderer->bUseStencil && g_openglConfig.bStencilBufferAvailable ? GL_STENCIL_BUFFER_BIT : 0));
	gldi_glx_apply_desktop_background (CAIRO_CONTAINER (pDock));
	
	if (g_pVisibleZoneBuffer.iTexture != 0)
	{
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_over ();
		int w = MIN (myDocksParam.iZoneWidth, pDock->container.iWidth);
		int h = MIN (myDocksParam.iZoneHeight, pDock->container.iHeight);
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
		if (! pDock->container.bDirectionUp)  // reverse image with dock.
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
	double pHiddenBgColor[4];
	const double r = 4; // corner radius of the background
	const double gap = 3;  // gap to the screen
	double dw = (myIconsParam.iIconGap > 2 ? 2 : 0);  // 1px margin around the icons for a better readability (only if icons won't be stuck togather then).
	double w, h;
	_cairo_dock_set_blend_alpha ();
	do
	{
		icon = ic->data;
		if (icon->bIsDemandingAttention || icon->bAlwaysVisible)
		{
			//g_print ("%s : %d (%d)\n", icon->cName, icon->bIsDemandingAttention, icon->Xid);
			y = icon->fDrawY;
			icon->fDrawY = (pDock->container.bDirectionUp ? pDock->container.iHeight - icon->fHeight * icon->fScale - gap : gap);
			
			if (icon->bHasHiddenBg)
			{
				if (icon->pHiddenBgColor)  // custom bg color
					memcpy (pHiddenBgColor, icon->pHiddenBgColor, 4*sizeof (gdouble));
				else  // default bg color
					memcpy (pHiddenBgColor, myDocksParam.fHiddenBg, 4*sizeof (gdouble));
				pHiddenBgColor[3] *= pDock->fPostHideOffset;
				if (pHiddenBgColor[3] != 0)
				{
					_cairo_dock_set_blend_alpha ();
					glPushMatrix ();
					w = icon->fWidth * icon->fScale;
					h = icon->fHeight * icon->fScale;
					if (pDock->container.bIsHorizontal)
					{
						glTranslatef (icon->fDrawX + w/2,
							pDock->container.iHeight - icon->fDrawY - h/2,
							0.);
						cairo_dock_draw_rounded_rectangle_opengl (w - 2*r + dw, h, r, 0, pHiddenBgColor);
					}
					else
					{
						glTranslatef (icon->fDrawY + h/2,
							pDock->container.iWidth - icon->fDrawX - w/2,
							0.);
						cairo_dock_draw_rounded_rectangle_opengl (h - 2*r + dw, w, r, 0, pHiddenBgColor);
					}
					glPopMatrix ();
				}
			}
			
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
	//g_print ("%s (%dx%d)\n", __func__, w, h);
	
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
	
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_source ();
	_cairo_dock_set_alpha (1.);  // full white
	glGenTextures (1, &iTexture);
	//g_print ("+ texture %d generee (%p, %dx%d)\n", iTexture, cairo_image_surface_get_data (pImageSurface), w, h);
	glBindTexture (GL_TEXTURE_2D, iTexture);
	
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MIN_FILTER,
		g_bEasterEggs ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	if (g_bEasterEggs)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	if (g_bEasterEggs)
		gluBuild2DMipmaps (GL_TEXTURE_2D,  /// see for automatic mipmaps generation, or at least how to update the mipmaps...
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
	glDisable(GL_BLEND);
	return iTexture;
}

GLuint cairo_dock_create_texture_from_raw_data (const guchar *pTextureRaw, int iWidth, int iHeight)
{
	/*cd_debug ("%dx%d", iWidth, iHeight);
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
	
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_source ();
	glColor4f (1., 1., 1., 1.);
	glGenTextures(1, &iTexture);
	glBindTexture(GL_TEXTURE_2D, iTexture);
	
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MIN_FILTER,
		g_bEasterEggs ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	if (g_bEasterEggs)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	if (g_bEasterEggs && pTextureRaw)
		gluBuild2DMipmaps (GL_TEXTURE_2D,
			4,
			iWidth,
			iHeight,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			pTextureRaw);
	else
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, iWidth, iHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pTextureRaw);
	glBindTexture (GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	return iTexture;
}

GLuint cairo_dock_create_texture_from_image_full (const gchar *cImageFile, double *fImageWidth, double *fImageHeight)
{
	#if (GTK_MAJOR_VERSION < 3 && GTK_MINOR_VERSION < 20)
	g_return_val_if_fail (GTK_WIDGET_REALIZED (g_pPrimaryContainer->pWidget), 0);
	#else
	g_return_val_if_fail (gtk_widget_get_realized (g_pPrimaryContainer->pWidget), 0);
	#endif
	double fWidth=0, fHeight=0;
	if (cImageFile == NULL)
		return 0;
	gchar *cImagePath;
	if (*cImageFile == '/')
		cImagePath = (gchar *)cImageFile;
	else
		cImagePath = cairo_dock_search_image_s_path (cImageFile);
	
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (g_pPrimaryContainer);
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
	if (pIcon != NULL && pIcon->image.pSurface != NULL)
	{
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_source ();
		_cairo_dock_set_alpha (1.);  // full white
		
		if (pIcon->image.iTexture == 0)
			glGenTextures (1, &pIcon->image.iTexture);
		int w = cairo_image_surface_get_width (pIcon->image.pSurface);
		int h = cairo_image_surface_get_height (pIcon->image.pSurface);
		glBindTexture (GL_TEXTURE_2D, pIcon->image.iTexture);
		
		glTexParameteri (GL_TEXTURE_2D,
			GL_TEXTURE_MIN_FILTER,
			g_bEasterEggs ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		if (g_bEasterEggs)
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		if (g_bEasterEggs)
			gluBuild2DMipmaps (GL_TEXTURE_2D,
				4,
				w,
				h,
				GL_BGRA,
				GL_UNSIGNED_BYTE,
				cairo_image_surface_get_data (pIcon->image.pSurface));
		else
			glTexImage2D (GL_TEXTURE_2D,
				0,
				4,  // GL_ALPHA / GL_BGRA
				w,
				h,
				0,
				GL_BGRA,  // GL_ALPHA / GL_BGRA
				GL_UNSIGNED_BYTE,
				cairo_image_surface_get_data (pIcon->image.pSurface));
		glDisable (GL_TEXTURE_2D);
	}
}



void cairo_dock_draw_texture_with_alpha (GLuint iTexture, int iWidth, int iHeight, double fAlpha)
{
	_cairo_dock_enable_texture ();
	//~ if (fAlpha == 1)
		//~ _cairo_dock_set_blend_over ();
	//~ else
		///_cairo_dock_set_blend_alpha ();
	_cairo_dock_set_blend_over ();  // gives the best result, at least with fAlpha=1
	
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
	
	_cairo_dock_apply_texture_at_size (pIcon->image.iTexture, fSizeX, fSizeY);
}

void cairo_dock_draw_icon_texture (Icon *pIcon, CairoContainer *pContainer)
{
	double fSizeX, fSizeY;
	cairo_dock_get_current_icon_size (pIcon, pContainer, &fSizeX, &fSizeY);
	
	cairo_dock_draw_texture_with_alpha (pIcon->image.iTexture,
		fSizeX,
		fSizeY,
		pIcon->fAlpha);
}

static inline void  _draw_icon_bent_backwards (Icon *pIcon, CairoContainer *pContainer, GLuint iOriginalTexture, double f)
{
	cairo_dock_set_perspective_view_for_icon (pIcon, pContainer);
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
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
		
		GLuint iOriginalTexture;
		if (pIcon->bIsHidden)
		{
			iOriginalTexture = pIcon->image.iTexture;
			pIcon->image.iTexture = cairo_dock_create_texture_from_surface (pIcon->image.pSurface);
			/// Using FBOs copies the texture data (pixels) within VRAM only:
			/// - setup & bind FBO
			/// - setup destination texture (using glTexImage() w/ pixels = 0)
			/// - add (blank but sized) destination texture as color attachment
			/// - bind source texture to texture target
			/// - draw quad w/ glTexCoors describing src area, projection/modelview matrices & glVertexes describing dst area
		}
		else
		{
			iOriginalTexture = cairo_dock_create_texture_from_surface (pIcon->image.pSurface);
		}
		
		cairo_dock_set_transition_on_icon (pIcon, pContainer,
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
		_draw_icon_bent_backwards (pIcon, pContainer, pIcon->image.iTexture, 1.);
		cairo_dock_end_draw_icon (pIcon, pContainer);
	}
}



//typedef void (*GLXBindTexImageProc) (Display *display, GLXDrawable drawable, int buffer, int *attribList);
//typedef void (*GLXReleaseTexImageProc) (Display *display, GLXDrawable drawable, int buffer);

// Bind redirected window to texture:
GLuint cairo_dock_texture_from_pixmap (Window Xid, Pixmap iBackingPixmap)
{
	if (!g_bEasterEggs)
		return 0;  /// ca ne marche pas. :-(
	
	if (!iBackingPixmap || ! g_openglConfig.bTextureFromPixmapAvailable)
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
	
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MIN_FILTER,
		g_bEasterEggs ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	if (g_bEasterEggs)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
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
