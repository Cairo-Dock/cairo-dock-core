/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

#include "cairo-dock-load.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-container.h"
#include "cairo-dock-draw-opengl.h"

#include "texture-gradation.h"
#define RADIAN (G_PI / 180.0)  // Conversion Radian/Degres
#define DELTA_ROUND_DEGREE 3

GLuint g_pGradationTexture[2];

extern cairo_surface_t *g_pDesktopBgSurface;

extern int g_iXScreenWidth[2];
extern int g_iXScreenHeight[2];
extern CairoDock *g_pMainDock;

extern double g_fIndicatorWidth, g_fIndicatorHeight;
extern GLuint g_iIndicatorTexture;
extern GLuint g_iActiveIndicatorTexture;
extern GLuint g_iClassIndicatorTexture;
extern GLuint g_iVisibleZoneTexture;
extern GLuint g_iDesktopBgTexture;
extern cairo_surface_t *g_pIndicatorSurface;
extern cairo_surface_t *g_pActiveIndicatorSurface;
extern cairo_surface_t *g_pClassIndicatorSurface;
extern double g_fClassIndicatorWidth, g_fClassIndicatorHeight;
extern cairo_surface_t *g_pVisibleZoneSurface;
extern gboolean g_bUseOpenGL;
extern gboolean g_bIndirectRendering;
extern GdkGLConfig* g_pGlConfig;


static void _cairo_dock_draw_appli_indicator_opengl (Icon *icon, gboolean bHorizontalDock, double fRatio, gboolean bDirectionUp)
{
	if (g_iIndicatorTexture == 0 && g_pIndicatorSurface != NULL)
	{
		g_iIndicatorTexture = cairo_dock_create_texture_from_surface (g_pIndicatorSurface);
		cd_debug ("g_iIndicatorTexture <- %d", g_iIndicatorTexture);
	}
	
	if (! myIndicators.bRotateWithDock)
		bDirectionUp = bHorizontalDock = TRUE;
	
	//\__________________ On place l'indicateur.
	if (icon->fOrientation != 0)
	{
		glTranslatef (-icon->fWidth * icon->fScale/2, icon->fHeight * icon->fScale/2, 0.);
		glRotatef (-icon->fOrientation/G_PI*180., 0., 0., 1.);
		glTranslatef (icon->fWidth * icon->fScale/2, -icon->fHeight * icon->fScale/2, 0.);
	}
	double fY;
	if (myIndicators.bLinkIndicatorWithIcon)  // il se deforme et rebondit avec l'icone.
	{
		fY = - icon->fHeight * icon->fScale/2
			+ (g_fIndicatorHeight/2 - myIndicators.iIndicatorDeltaY/(1 + myIcons.fAmplitude)) * fRatio * icon->fScale;
		
		if (bHorizontalDock)
		{
			if (! bDirectionUp)
				fY = - fY;
			glTranslatef (0., fY, 0.);
			glScalef (g_fIndicatorWidth * fRatio * icon->fScale * icon->fWidthFactor, (bDirectionUp ? 1:-1) * g_fIndicatorHeight * fRatio * icon->fScale * icon->fHeightFactor, 1.);
		}
		else
		{
			if (bDirectionUp)
				fY = - fY;
			glTranslatef (fY, 0., 0.);
			glRotatef (90, 0., 0., 1.);
			glScalef (g_fIndicatorWidth * fRatio * icon->fScale * icon->fWidthFactor, (bDirectionUp ? 1:-1) * g_fIndicatorHeight * fRatio * icon->fScale * icon->fHeightFactor, 1.);
		}
		
	}
	else  // il est fixe, en bas de l'icone.
	{
		fY = - icon->fHeight * icon->fScale/2
			+ (g_fIndicatorHeight/2 - myIndicators.iIndicatorDeltaY/(1 + myIcons.fAmplitude)) * fRatio;
		if (bHorizontalDock)
		{
			if (! bDirectionUp)
				fY = - fY;
			glTranslatef (0., fY, 1.);
			glScalef (g_fIndicatorWidth * fRatio * 1., (bDirectionUp ? 1:-1) * g_fIndicatorHeight * fRatio * 1., 1.);
		}
		else
		{
			if (bDirectionUp)
				fY = - fY;
			glTranslatef (fY, 0., 1.);
			glRotatef (90, 0., 0., 1.);
			glScalef (g_fIndicatorWidth * fRatio * 1., (bDirectionUp ? 1:-1) * g_fIndicatorHeight * fRatio * 1., 1.);
		}
	}

	//\__________________ On dessine l'indicateur.
	cairo_dock_draw_texture_with_alpha (g_iIndicatorTexture, 1., 1., 1.);
}
static void _cairo_dock_draw_active_window_indicator_opengl (Icon *icon, CairoDock *pDock, double fRatio)
{
	if (g_iActiveIndicatorTexture == 0 && g_pActiveIndicatorSurface != NULL)
	{
		g_iActiveIndicatorTexture = cairo_dock_create_texture_from_surface (g_pActiveIndicatorSurface);
		cd_debug ("g_iActiveIndicatorTexture <- %d", g_iActiveIndicatorTexture);
	}
	
	if (icon->fOrientation != 0)
	{
		glTranslatef (-icon->fWidth * icon->fScale/2, icon->fHeight * icon->fScale/2, 0.);
		glRotatef (-icon->fOrientation/G_PI*180., 0., 0., 1.);
		glTranslatef (icon->fWidth * icon->fScale/2, -icon->fHeight * icon->fScale/2, 0.);
	}
	
	cairo_dock_set_icon_scale (icon, CAIRO_CONTAINER (pDock), 1.);
	cairo_dock_draw_texture_with_alpha (g_iActiveIndicatorTexture, 1., 1., 1.);
}
static void _cairo_dock_draw_class_indicator_opengl (Icon *icon, gboolean bHorizontalDock, double fRatio, gboolean bDirectionUp)
{
	if (g_iClassIndicatorTexture == 0 && g_pClassIndicatorSurface != NULL)
	{
		g_iClassIndicatorTexture = cairo_dock_create_texture_from_surface (g_pClassIndicatorSurface);
		cd_debug ("g_iClassIndicatorTexture <- %d", g_iClassIndicatorTexture);
	}
	
	if (icon->fOrientation != 0)
	{
		glTranslatef (-icon->fWidth * icon->fScale/2, icon->fHeight * icon->fScale/2, 0.);
		glRotatef (-icon->fOrientation/G_PI*180., 0., 0., 1.);
		glTranslatef (icon->fWidth * icon->fScale/2, -icon->fHeight * icon->fScale/2, 0.);
	}
	
	if (myIndicators.bZoomClassIndicator)
		fRatio *= icon->fScale;
	
	if (! bHorizontalDock)
		glRotatef (90., 0., 0., 1.);
	if (! bDirectionUp)
		glScalef (1., -1., 1.);
	glTranslatef (icon->fWidth * icon->fScale/2 - g_fClassIndicatorWidth * fRatio/2,
		icon->fHeight * icon->fScale/2 - g_fClassIndicatorHeight * fRatio/2,
		0.);
	
	cairo_dock_draw_texture_with_alpha (g_iClassIndicatorTexture,
		g_fClassIndicatorWidth * fRatio,
		g_fClassIndicatorHeight * fRatio,
		1.);
}

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
gboolean cairo_dock_render_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext)
{
	if (pCairoContext != NULL)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	if (*bHasBeenRendered)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	if (pIcon->iIconTexture == 0)
	{
		*bHasBeenRendered = TRUE;
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	}
	
	cairo_dock_draw_icon_texture (pIcon, CAIRO_CONTAINER (pDock));
	
	if (pDock->bUseReflect)
	{
		if (pDock->bUseStencil)
		{
			glEnable (GL_STENCIL_TEST);
			glStencilFunc (GL_EQUAL, 1, 1);
			glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
		}
		glPushMatrix ();
		double x0, y0, x1, y1;
		double fScale = ((myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (pIcon)) ? 1. : pIcon->fScale);
		double fReflectSize = MIN (myIcons.fReflectSize, pIcon->fHeight/pDock->fRatio*fScale);
		double fReflectRatio = fReflectSize * pDock->fRatio / pIcon->fHeight / fScale  / pIcon->fHeightFactor;
		double fOffsetY = pIcon->fHeight * fScale/2 + fReflectSize * pDock->fRatio/2 + pIcon->fDeltaYReflection;
		if (pDock->bHorizontalDock)
		{
			if (pDock->bDirectionUp)
			{
				glTranslatef (0., - fOffsetY, 0.);
				glScalef (pIcon->fWidth * pIcon->fWidthFactor * fScale, - fReflectSize * pDock->fRatio, 1.);  // taille du reflet et on se retourne.
				x0 = 0.;
				y0 = 1. - fReflectRatio;
				x1 = 1.;
				y1 = 1.;
			}
			else
			{
				glTranslatef (0., fOffsetY, 0.);
				glScalef (pIcon->fWidth * pIcon->fWidthFactor * fScale, fReflectSize * pDock->fRatio, 1.);
				x0 = 0.;
				y0 = fReflectRatio;
				x1 = 1.;
				y1 = 0.;
			}
		}
		else
		{
			if (pDock->bDirectionUp)
			{
				glTranslatef (fOffsetY, 0., 0.);
				glScalef (- fReflectSize * pDock->fRatio, pIcon->fWidth * pIcon->fWidthFactor * fScale, 1.);
				x0 = 1. - fReflectRatio;
				y0 = 0.;
				x1 = 1.;
				y1 = 1.;
			}
			else
			{
				glTranslatef (- fOffsetY, 0., 0.);
				glScalef (fReflectSize * pDock->fRatio, pIcon->fWidth * pIcon->fWidthFactor * fScale, 1.);
				x0 = fReflectRatio;
				y0 = 0.;
				x1 = 0.;
				y1 = 1.;
			}
		}
		/**glActiveTexture (GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_pGradationTexture[pDock->bHorizontalDock]);
		glActiveTexture (GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, pIcon->iIconTexture);
		cairo_dock_combine_argb_argb ();*/
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, pIcon->iIconTexture);
		glEnable(GL_BLEND);
		///glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		_cairo_dock_set_blend_alpha ();
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		
		//glBlendColor(1., 1., 1., 1.);  // utile ?
		
		glPolygonMode (GL_FRONT, GL_FILL);
		glColor4f(1., 1., 1., 1.);
		
		glBegin(GL_QUADS);
		
		double fReflectAlpha = myIcons.fAlbedo * pIcon->fAlpha;
		if (pDock->bHorizontalDock)
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
		
		/**glActiveTexture(GL_TEXTURE0_ARB); // Go pour le multitexturing 1ere passe
		glEnable(GL_TEXTURE_2D); // On active le texturing sur cette passe
		glBindTexture(GL_TEXTURE_2D, pIcon->iIconTexture);
		glColor4f(1., 1., 1., 1.);  // transparence du reflet.
		glEnable(GL_BLEND);
		glBlendFunc (1, 0);
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		
		glActiveTexture(GL_TEXTURE1_ARB); // Go pour le texturing 2eme passe
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, g_pGradationTexture[pDock->bHorizontalDock]);
		glColor4f(1., 1., 1., myIcons.fAlbedo * pIcon->fAlpha);  // transparence du reflet.
		glEnable(GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // Le mode de combinaison des textures
		//glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE);  // multiplier les alpha.
		
		glBegin(GL_QUADS);
		glNormal3f(0,0,1);
		glMultiTexCoord2fARB (GL_TEXTURE0_ARB, x0, y0);
		glMultiTexCoord2fARB (GL_TEXTURE1_ARB, 0., 0.);
		glVertex3f (-0.5, .5, 0.);  // Bottom Left Of The Texture and Quad
		
		glMultiTexCoord2fARB (GL_TEXTURE0_ARB, x1, y0);
		glMultiTexCoord2fARB (GL_TEXTURE1_ARB, 1., 0.);
		glVertex3f ( 0.5, .5, 0.);  // Bottom Right Of The Texture and Quad
		
		glMultiTexCoord2fARB (GL_TEXTURE0_ARB, x1, y1);
		glMultiTexCoord2fARB (GL_TEXTURE1_ARB, 1., 1.);
		glVertex3f ( 0.5, -.5, 0.);  // Top Right Of The Texture and Quad
		
		glMultiTexCoord2fARB (GL_TEXTURE0_ARB, x0, y1);
		glMultiTexCoord2fARB (GL_TEXTURE1_ARB, 0., 1.);
		glVertex3f (-0.5, -.5, 0.);  // Top Left Of The Texture and Quad
		glEnd();*/
		
		/*glActiveTexture(GL_TEXTURE1_ARB);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glActiveTexture(GL_TEXTURE0_ARB);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);*/
		
		glPopMatrix ();
		if (pDock->bUseStencil)
		{
			glDisable (GL_STENCIL_TEST);
		}
	}
	
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	
	*bHasBeenRendered = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

void cairo_dock_render_one_icon_opengl (Icon *icon, CairoDock *pDock, double fDockMagnitude, gboolean bUseText)
{
	if (icon->iIconTexture == 0)
		return ;
	double fRatio = pDock->fRatio;
	
	if (g_pGradationTexture[pDock->bHorizontalDock] == 0)
	{
		//g_pGradationTexture[pDock->bHorizontalDock] = cairo_dock_load_local_texture (pDock->bHorizontalDock ? "texture-gradation-vert.png" : "texture-gradation-horiz.png", CAIRO_DOCK_SHARE_DATA_DIR);
		g_pGradationTexture[pDock->bHorizontalDock] = cairo_dock_load_texture_from_raw_data (gradationTex,
			pDock->bHorizontalDock ? 1:48,
			pDock->bHorizontalDock ? 48:1);
		cd_debug ("g_pGradationTexture(%d) <- %d", pDock->bHorizontalDock, g_pGradationTexture[pDock->bHorizontalDock]);
	}
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.fVisibleAppliAlpha != 0 && ! CAIRO_DOCK_IS_APPLET (icon) && !(icon->iBackingPixmap != 0 && icon->bIsHidden))
	{
		double fAlpha = (icon->bIsHidden ? MIN (1 - myTaskBar.fVisibleAppliAlpha, 1) : MIN (myTaskBar.fVisibleAppliAlpha + 1, 1));
		if (fAlpha != 1)
			icon->fAlpha = fAlpha;  // astuce bidon pour pas multiplier 2 fois.
	}
	
	//\_____________________ On se place au centre de l'icone.
	double fX=0, fY=0;
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
		if (! pDock->bDirectionUp)
			if (pDock->bHorizontalDock)
				fY = (1-fGlideScale)*icon->fHeight*icon->fScale;
			else
				fX = (1-fGlideScale)*icon->fHeight*icon->fScale;
	}
	else
		fGlideScale = 1;
	icon->fGlideScale = fGlideScale;
	
	if (pDock->bHorizontalDock)
	{
		fY += pDock->iCurrentHeight - icon->fDrawY;  // ordonnee du haut de l'icone.
		fX += icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1);  // abscisse du milieu de l'icone.
	}
	else
	{
		fY += icon->fDrawY;  // ordonnee du haut de l'icone.
		fX +=  pDock->iCurrentWidth - (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->fGlideOffset * icon->fWidth * icon->fScale * (icon->fGlideOffset < 0 ? fGlideScale : 1));
	}
	
	glLoadIdentity ();
	if (pDock->bHorizontalDock)
		glTranslatef (fX, fY - icon->fHeight * icon->fScale * (1 - fGlideScale/2), - icon->fHeight * (1+myIcons.fAmplitude));
	else
		glTranslatef (fY + icon->fHeight * icon->fScale * (1 - fGlideScale/2), fX, - icon->fHeight * (1+myIcons.fAmplitude));
	glPushMatrix ();
	
	//\_____________________ On dessine l'indicateur derriere.
	if (icon->bHasIndicator && ! myIndicators.bIndicatorAbove /*&& g_iIndicatorTexture != 0*/)
	{
		glPushMatrix ();
		_cairo_dock_draw_appli_indicator_opengl (icon, pDock->bHorizontalDock, fRatio, pDock->bDirectionUp);
		glPopMatrix ();
	}
	if (icon->Xid != 0 && icon->Xid == cairo_dock_get_current_active_window () && ! myIndicators.bActiveIndicatorAbove && g_pActiveIndicatorSurface != NULL)
	{
		glPushMatrix ();
		_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, fRatio);
		glPopMatrix ();
	}
	
	//\_____________________ On positionne l'icone.
	if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
	{
		if (pDock->bHorizontalDock)
		{
			glTranslatef (0., (pDock->bDirectionUp ? icon->fHeight * (- icon->fScale + 1)/2 : icon->fHeight * (icon->fScale - 1)/2), 0.);
		}
		else
		{
			glTranslatef ((!pDock->bDirectionUp ? icon->fHeight * (- icon->fScale + 1)/2 : icon->fHeight * (icon->fScale - 1)/2), 0., 0.);
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
	
	//\_____________________ On dessine l'icone.
	gboolean bIconHasBeenDrawn = FALSE;
	cairo_dock_notify (CAIRO_DOCK_PRE_RENDER_ICON, icon, pDock);
	cairo_dock_notify (CAIRO_DOCK_RENDER_ICON, icon, pDock, &bIconHasBeenDrawn, NULL);
	
	glPopMatrix ();  // retour juste apres la translation au milieu de l'icone.
	/*if (pDock->bUseReflect)
	{
		glPushMatrix ();
		glTranslatef (0., -icon->fHeight * icon->fScale/2, 1.);
		glScalef (1., -1, 1.);
 		bIconHasBeenDrawn=FALSE;
 		cairo_dock_notify (CAIRO_DOCK_PRE_RENDER_ICON, icon, pDock);
 		cairo_dock_notify (CAIRO_DOCK_RENDER_ICON, icon, pDock, &bIconHasBeenDrawn);
		
		glPopMatrix ();
	}*/
	
	//\_____________________ On dessine l'indicateur devant.
	if (icon->bHasIndicator && myIndicators.bIndicatorAbove/* && g_iIndicatorTexture != 0*/)
	{
		glPushMatrix ();
		glTranslatef (0., 0., icon->fHeight * (1+myIcons.fAmplitude) -1);  // avant-plan
		_cairo_dock_draw_appli_indicator_opengl (icon, pDock->bHorizontalDock, fRatio, pDock->bDirectionUp);
		glPopMatrix ();
	}
	if (icon->Xid != 0 && icon->Xid == cairo_dock_get_current_active_window () && myIndicators.bActiveIndicatorAbove && g_pActiveIndicatorSurface != NULL)
	{
		glPushMatrix ();
		glTranslatef (0., 0., icon->fHeight * (1+myIcons.fAmplitude) -1);  // avant-plan
		_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, fRatio);
		glPopMatrix ();
	}
	if (icon->pSubDock != NULL && icon->cClass != NULL && g_pClassIndicatorSurface != NULL && icon->Xid == 0)  // le dernier test est de la paranoia.
	{
		glPushMatrix ();
		glTranslatef (0., 0., icon->fHeight * (1+myIcons.fAmplitude) -1);  // avant-plan
		_cairo_dock_draw_class_indicator_opengl (icon, pDock->bHorizontalDock, fRatio, pDock->bDirectionUp);
		glPopMatrix ();
	}
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	if (bUseText && icon->iLabelTexture != 0 && icon->fScale > 1.01 && (! mySystem.bLabelForPointedIconOnly || icon->bPointed))  // 1.01 car sin(pi) = 1+epsilon :-/  //  && icon->iAnimationState < CAIRO_DOCK_STATE_CLICKED
	{
		glPushMatrix ();
		
		double fOffsetX = 0.;
		if (icon->fDrawX + icon->fWidth * icon->fScale/2 - icon->iTextWidth/2 < 0)
			fOffsetX = icon->iTextWidth/2 - (icon->fDrawX + icon->fWidth * icon->fScale/2);
		else if (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->iTextWidth/2 > pDock->iCurrentWidth)
			fOffsetX = pDock->iCurrentWidth - (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->iTextWidth/2);
		if (icon->fOrientation != 0 && ! mySystem.bTextAlwaysHorizontal)
		{
			glTranslatef (-icon->fWidth * icon->fScale/2, icon->fHeight * icon->fScale/2, 0.);
			glRotatef (-icon->fOrientation/G_PI*180., 0., 0., 1.);
			glTranslatef (icon->fWidth * icon->fScale/2, -icon->fHeight * icon->fScale/2, 0.);
		}
		
		if (! pDock->bHorizontalDock && mySystem.bTextAlwaysHorizontal)
		{
			glTranslatef (-icon->fHeight * icon->fScale/2 - (pDock->bDirectionUp ? myLabels.iLabelSize : (pDock->bUseReflect ? myIcons.fReflectSize : 0.)) + icon->iTextWidth / 2 - myLabels.iconTextDescription.iMargin + 1,
				(icon->fWidth * icon->fScale + icon->iTextHeight) / 2,
				0.);
		}
		else if (pDock->bHorizontalDock)
		{
			glTranslatef (fOffsetX, (pDock->bDirectionUp ? 1:-1) * (icon->fHeight * icon->fScale/2 + myLabels.iLabelSize - icon->iTextHeight / 2), 0.);
		}
		else
		{
			glTranslatef ((pDock->bDirectionUp ? -.5:.5) * (icon->fHeight * icon->fScale + icon->iTextHeight),
				fOffsetX,
				0.);
			glRotatef (pDock->bDirectionUp ? 90 : -90, 0., 0., 1.);
		}
		
		double fMagnitude;
		if (mySystem.bLabelForPointedIconOnly)
		{
			fMagnitude = fDockMagnitude;  // (icon->fScale - 1) / myIcons.fAmplitude / sin (icon->fPhase);  // sin (phi ) != 0 puisque fScale > 1.
		}
		else
		{
			fMagnitude = (icon->fScale - 1) / myIcons.fAmplitude;  /// il faudrait diviser par pDock->fMagnitudeMax ...
			fMagnitude = pow (fMagnitude, mySystem.fLabelAlphaThreshold);
			///fMagnitude *= (fMagnitude * mySystem.fLabelAlphaThreshold + 1) / (mySystem.fLabelAlphaThreshold + 1);
		}
		
		cairo_dock_draw_texture_with_alpha (icon->iLabelTexture,
			icon->iTextWidth,
			icon->iTextHeight,
			fMagnitude);
		
		glPopMatrix ();
	}
	
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
}


void cairo_dock_render_hidden_dock_opengl (CairoDock *pDock)
{
	//g_print ("%s (%d, %x)\n", __func__, pDock->bIsMainDock, g_pVisibleZoneSurface);
	if (g_iVisibleZoneTexture == 0 && g_pVisibleZoneSurface != NULL)
	{
		g_iVisibleZoneTexture = cairo_dock_create_texture_from_surface (g_pVisibleZoneSurface);
		cd_debug ("g_iVisibleZoneTexture <- %d", g_iVisibleZoneTexture);
	}
	
	glLoadIdentity ();
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (pDock->bUseStencil ? GL_STENCIL_BUFFER_BIT : 0));
	cairo_dock_apply_desktop_background (CAIRO_CONTAINER (pDock));
	
	if (g_iVisibleZoneTexture == 0)
		return ;
	
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_over ();
	_cairo_dock_set_alpha (1.);
	
	glLoadIdentity ();
	glTranslatef (pDock->iCurrentWidth/2, pDock->iCurrentHeight/2, 0.);
	
	if (! pDock->bDirectionUp && myBackground.bReverseVisibleImage)
		glScalef (1., -1., 1.);
	if (! pDock->bHorizontalDock)
		glRotatef (-90., 0, 0, 1);
	
	_cairo_dock_apply_texture_at_size (g_iVisibleZoneTexture, pDock->iCurrentWidth, pDock->iCurrentHeight);
	
	_cairo_dock_disable_texture ();
}



static GLboolean _check_extension (const char *extName)
{
	/*
	** Search for extName in the extensions string.  Use of strstr()
	** is not sufficient because extension names can be prefixes of
	** other extension names.  Could use strtok() but the constant
	** string returned by glGetString can be in read-only memory.
	*/
	char *p = (char *) glGetString (GL_EXTENSIONS);

	char *end;
	int extNameLen;

	extNameLen = strlen(extName);
	end = p + strlen(p);

	while (p < end)
	{
		int n = strcspn(p, " ");
		if ((extNameLen == n) && (strncmp(extName, p, n) == 0))
		{
			return GL_TRUE;
		}
		p += (n + 1);
	}
	return GL_FALSE;
}
GLuint cairo_dock_create_texture_from_surface (cairo_surface_t *pImageSurface)
{
	static gint iNonPowerOfTwoAvailable = -1;
	if (! g_bUseOpenGL || pImageSurface == NULL)
		return 0;
	GLuint iTexture = 0;
	int w = cairo_image_surface_get_width (pImageSurface);
	int h = cairo_image_surface_get_height (pImageSurface);
	
	cairo_surface_t *pPowerOfwoSurface = pImageSurface;
	
	if (iNonPowerOfTwoAvailable == -1)
	{
		iNonPowerOfTwoAvailable = _check_extension ("GL_ARB_texture_non_power_of_two");
		cd_message ("non power of two available : %d", iNonPowerOfTwoAvailable);
	}
	int iMaxTextureWidth = 4096, iMaxTextureHeight = 4096;  // il faudrait le recuperer de glInfo ...
	if (! iNonPowerOfTwoAvailable)  // cas des vieilles cartes comme la GeForce5.
	{
		double log2_w = log (w) / log (2);
		double log2_h = log (h) / log (2);
		int w_ = MIN (iMaxTextureWidth, pow (2, ceil (log2_w)));
		int h_ = MIN (iMaxTextureHeight, pow (2, ceil (log2_h)));
		cd_debug ("%dx%d --> %dx%d", w, h, w_, h_);
		
		if (w != w_ || h != h_)
		{
			pPowerOfwoSurface = _cairo_dock_create_blank_surface (NULL, w_, h_);
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
	/*g_print ("%dx%d\n", iWidth, iHeight);
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
		g_print ("\\%o\\%o\\%o\\%o", red, green, blue, alpha);
	}
	pTextureRaw = pPixelBuffer2;
	*/
	
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
	g_return_val_if_fail (GTK_WIDGET_REALIZED (g_pMainDock->pWidget), 0);
	double fWidth=0, fHeight=0;
	if (cImageFile == NULL)
		return 0;
	gchar *cImagePath;
	if (*cImageFile == '/')
		cImagePath = (gchar *)cImageFile;
	else
		cImagePath = cairo_dock_generate_file_path (cImageFile);
	
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (g_pMainDock));
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_image (cImagePath,
		pCairoContext,
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

#define P(t,p,q,s) ((1-t) * (1-t) * p + 2 * t * (1-t) * q + t * t * s)
void cairo_dock_add_simple_curved_subpath_opengl (GLfloat *pVertexTab, int iNbPts, double x0, double y0, double x1, double y1, double x2, double y2)
{
	double t;
	int i;
	for (i = 0; i < iNbPts; i ++)
	{
		t = 1.*i/iNbPts;  // [0;1[
		_cairo_dock_set_vertex_xy (i,
			P(t, x0, x1, x2),
			P(t, y0, y1, y2));
		//vx(i) = P(t, x0, x1, x2);
		//vy(i) = P(t, y0, y1, y2);
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
				P(t, x0, x1, x2),
				P(t, y0, y1, y2));
			//vx(i) = P(t, x0, x1, x2);
			//vy(i) = P(t, y0, y1, y2);
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
				P(t, x0, x1, x2),
				P(t, y0, y1, y2));
			//vx(i) = P(t, x0, x1, x2);
			//vy(i) = P(t, y0, y1, y2);
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
	if (pDock->bHorizontalDock) {\
		x = _get_icon_center_x (icon);\
		y = pDock->iCurrentHeight - _get_icon_center_y (icon); }\
	else {\
		 y = _get_icon_center_x (icon);\
		 x = pDock->iCurrentWidth - _get_icon_center_y (icon); } } while (0)
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
		glTranslatef(0.5f - fDecorationsOffsetX * mySystem.fStripesSpeedFactor / (fDockWidth), 0.5f, 0.);
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

GLXPbuffer cairo_dock_create_pbuffer (int iWidth, int iHeight, GLXContext *pContext)
{
	Display *XDisplay = gdk_x11_get_default_xdisplay ();
	
	GLXFBConfig *pFBConfigs; 
	XRenderPictFormat *pPictFormat = NULL;
	int visAttribs[] = {
		GLX_DRAWABLE_TYPE, 	GLX_PBUFFER_BIT | GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, 		GLX_RGBA_BIT,
		GLX_RED_SIZE, 		1,
		GLX_GREEN_SIZE, 		1,
		GLX_BLUE_SIZE, 		1,
		GLX_ALPHA_SIZE, 		1,
		GLX_DEPTH_SIZE, 		1,
		None};
	
	XVisualInfo *pVisInfo = NULL;
	int i, iNumOfFBConfigs = 0;
	pFBConfigs = glXChooseFBConfig (XDisplay,
		DefaultScreen (XDisplay),
		visAttribs,
		&iNumOfFBConfigs);
	if (iNumOfFBConfigs == 0)
	{
		cd_warning ("No suitable visual could be found for pbuffer\n this might affect the drawing of applets that inside a dock");
		*pContext = 0;
		return 0;
	}
	cd_debug (" -> %d FBConfig(s) pour le pbuffer", iNumOfFBConfigs);
	
	
	int pbufAttribs [] = {
		GLX_PBUFFER_WIDTH, iWidth,
		GLX_PBUFFER_HEIGHT, iHeight,
		GLX_LARGEST_PBUFFER, True,
		None};
	GLXPbuffer pbuffer = glXCreatePbuffer (XDisplay, pFBConfigs[0], pbufAttribs);
	
	
	pVisInfo = glXGetVisualFromFBConfig (XDisplay, pFBConfigs[0]);
	
	GdkGLContext *pGlContext = gtk_widget_get_gl_context (g_pMainDock->pWidget);
	GLXContext mainContext = GDK_GL_CONTEXT_GLXCONTEXT (pGlContext);
	*pContext = glXCreateContext (XDisplay, pVisInfo, mainContext, ! g_bIndirectRendering);
	
	XFree (pVisInfo);
	XFree (pFBConfigs);
	
	return pbuffer;
}

static GLXPbuffer s_iconPbuffer = 0;
static GLXContext s_iconContext = 0;
int s_iIconPbufferWidth = 0, s_iIconPbufferHeight = 0;
void cairo_dock_create_icon_pbuffer (void)
{
	if (!g_bUseOpenGL)
		return ;
	int iWidth = 0, iHeight = 0;
	int i;
	for (i = 0; i < CAIRO_DOCK_NB_TYPES; i += 2)
	{
		iWidth = MAX (iWidth, myIcons.tIconAuthorizedWidth[i]);
		iHeight = MAX (iHeight, myIcons.tIconAuthorizedHeight[i]);
	}
	if (iWidth == 0)
		iWidth = 48;
	if (iHeight == 0)
		iHeight = 48;
	iWidth *= (1 + myIcons.fAmplitude);
	iHeight *= (1 + myIcons.fAmplitude);
	
	cd_debug ("%s (%dx%d)", __func__, iWidth, iHeight);
	if (s_iIconPbufferWidth != iWidth || s_iIconPbufferHeight != iHeight)
	{
		Display *XDisplay = gdk_x11_get_default_xdisplay ();
		if (s_iconPbuffer != 0)
		{
			glXDestroyPbuffer (XDisplay, s_iconPbuffer);
			glXDestroyContext (XDisplay, s_iconContext);
			s_iconContext = 0;
		}
		s_iconPbuffer = cairo_dock_create_pbuffer (iWidth, iHeight, &s_iconContext);
		s_iIconPbufferWidth = iWidth;
		s_iIconPbufferHeight = iHeight;
		
		g_print ("if your drivers are crappy, we'll know it immediately ...");
		if (s_iconPbuffer != 0 && s_iconContext != 0 && glXMakeCurrent (XDisplay, s_iconPbuffer, s_iconContext))
		{
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, iWidth, 0, iHeight, 0.0, 500.0);
			glMatrixMode (GL_MODELVIEW);
			
			glLoadIdentity();
			gluLookAt (0., 0., 3.,
				0., 0., 0.,
				0.0f, 1.0f, 0.0f);
			
			glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
			glClearDepth (1.0f);
		}
		g_print (" ok, they seem fine enough.\n");
	}
}

void cairo_dock_destroy_icon_pbuffer (void)
{
	Display *XDisplay = gdk_x11_get_default_xdisplay ();
	if (s_iconPbuffer != 0)
	{
		glXDestroyPbuffer (XDisplay, s_iconPbuffer);
		s_iconPbuffer = 0;
	}
	if (s_iconContext != 0)
	{
		glXDestroyContext (XDisplay, s_iconContext);
		s_iconContext = 0;
	}
	s_iIconPbufferWidth = 0;
	s_iIconPbufferHeight = 0;
}

gboolean cairo_dock_begin_draw_icon (Icon *pIcon, CairoContainer *pContainer)
{
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		GdkGLContext *pGlContext = gtk_widget_get_gl_context (pContainer->pWidget);
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pContainer->pWidget);
		if (! gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return FALSE;
		
		cairo_dock_set_ortho_view (pContainer->iWidth, pContainer->iHeight);
	}
	else if (s_iconContext != 0)
	{
		Display *XDisplay = gdk_x11_get_default_xdisplay ();
		if (! glXMakeCurrent (XDisplay, s_iconPbuffer, s_iconContext))
			return FALSE;
		glLoadIdentity ();
		glTranslatef (s_iIconPbufferWidth/2, s_iIconPbufferHeight/2, - s_iIconPbufferHeight/2);
	}
	else
		return FALSE;
	
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glColor4f(1., 1., 1., 1.);
	
	glScalef (1., -1., 1.);
	
	return TRUE;
}

void cairo_dock_end_draw_icon (Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (pIcon->iIconTexture != 0);
	// taille de la texture
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
	
	// copie dans notre texture
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, pIcon->iIconTexture);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_BLEND);
	glBlendFunc (GL_ZERO, GL_ONE);
	glColor4f(1., 1., 1., 1.);
	
	int x,y;
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		x = (pContainer->iWidth - iWidth)/2;
		y = (pContainer->iHeight - iHeight)/2;
	}
	else
	{
		x = (s_iIconPbufferWidth - iWidth)/2;
		y = (s_iIconPbufferHeight - iHeight)/2;
	}
	
	glCopyTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, x, y, iWidth, iHeight, 0);  // target, num mipmap, format, x,y, w,h, border.
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	
	//end
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		cairo_dock_set_perspective_view (pContainer->iWidth, pContainer->iHeight);
		
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pContainer->pWidget);
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
}



void cairo_dock_set_perspective_view (int iWidth, int iHeight)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, 1.0*(GLfloat)iWidth/(GLfloat)iHeight, 1., 4*iHeight);
	glMatrixMode (GL_MODELVIEW);
	
	glLoadIdentity ();
	gluLookAt (0., 0., 3.,
		0., 0., 0.,
		0.0f, 1.0f, 0.0f);
	glTranslatef (0., 0., -iHeight*(sqrt(3)/2) - 1);
}

void cairo_dock_set_ortho_view (int iWidth, int iHeight)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, iWidth, 0, iHeight, 0.0, 500.0);
	glMatrixMode (GL_MODELVIEW);
	
	glLoadIdentity ();
	gluLookAt (0., 0., 3.,
		0., 0., 0.,
		0.0f, 1.0f, 0.0f);
	glTranslatef (iWidth/2, iHeight/2, - iHeight/2);
}

GdkGLConfig *cairo_dock_get_opengl_config (gboolean bForceOpenGL, gboolean *bHasBeenForced)  // taken from a MacSlow's exemple.
{
	GdkGLConfig *pGlConfig = NULL;
	
	Display *XDisplay = gdk_x11_get_default_xdisplay ();
	
	GLXFBConfig *pFBConfigs; 
	XRenderPictFormat *pPictFormat = NULL;
	int doubleBufferAttributes[] = {
		GLX_DRAWABLE_TYPE, 	GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, 		GLX_RGBA_BIT,
		GLX_DOUBLEBUFFER, 	True,
		GLX_RED_SIZE, 		1,
		GLX_GREEN_SIZE, 		1,
		GLX_BLUE_SIZE, 		1,
		GLX_ALPHA_SIZE, 		1,
		GLX_DEPTH_SIZE, 		1,
		GLX_STENCIL_SIZE, 	1,
		None};
	
	
	XVisualInfo *pVisInfo = NULL;
	int i, iNumOfFBConfigs = 0;
	cd_debug ("cherchons les configs ...");
	pFBConfigs = glXChooseFBConfig (XDisplay,
		DefaultScreen (XDisplay),
		doubleBufferAttributes,
		&iNumOfFBConfigs);
	
	cd_debug (" -> %d FBConfig(s)", iNumOfFBConfigs);
	for (i = 0; i < iNumOfFBConfigs; i++)
	{
		pVisInfo = glXGetVisualFromFBConfig (XDisplay, pFBConfigs[i]);
		if (!pVisInfo)
		{
			cd_warning ("this FBConfig has no visual.");
			continue;
		}
		
		pPictFormat = XRenderFindVisualFormat (XDisplay, pVisInfo->visual);
		if (!pPictFormat)
		{
			cd_warning ("this visual has an unknown format.");
			XFree (pVisInfo);
			pVisInfo = NULL;
			continue;
		}
		
		if (pPictFormat->direct.alphaMask > 0)
		{
			cd_message ("Strike, found a GLX visual with alpha-support !");
			*bHasBeenForced = FALSE;
			break;
		}

		XFree (pVisInfo);
		pVisInfo = NULL;
	}
	if (pFBConfigs)
		XFree (pFBConfigs);
	
	if (pVisInfo == NULL)
	{
		cd_warning ("couldn't find an appropriate visual ourself, trying something else, this may not work with some drivers ...");
		pGlConfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGB |
			GDK_GL_MODE_ALPHA |
			GDK_GL_MODE_DEPTH |
			GDK_GL_MODE_DOUBLE |
			GDK_GL_MODE_STENCIL);
		
		if (pGlConfig == NULL)
		{
			cd_warning ("no luck, trying without double-buffer and stencil ...");
			pGlConfig = gdk_gl_config_new_by_mode (GDK_GL_MODE_RGB |
				GDK_GL_MODE_ALPHA |
				GDK_GL_MODE_DEPTH);
		}
		if (pGlConfig != NULL)
			return pGlConfig;
	}
	
	if (pVisInfo == NULL && bForceOpenGL)
	{
		cd_warning ("we could not get an ARGB-visual, trying to get an RGB one (fake transparency will be used in return) ...");
		*bHasBeenForced = TRUE;
		doubleBufferAttributes[13] = 0;
		pFBConfigs = glXChooseFBConfig (XDisplay,
			DefaultScreen (XDisplay),
			doubleBufferAttributes,
			&iNumOfFBConfigs);
		cd_message ("got %d FBConfig(s) this time", iNumOfFBConfigs);
		for (i = 0; i < iNumOfFBConfigs; i++)
		{
			pVisInfo = glXGetVisualFromFBConfig (XDisplay, pFBConfigs[i]);
			if (!pVisInfo)
			{
				cd_warning ("this FBConfig has no visual.");
				XFree (pVisInfo);
				pVisInfo = NULL;
			}
			else
				break;
		}
		if (pFBConfigs)
			XFree (pFBConfigs);
		
		if (pVisInfo == NULL)
		{
			cd_warning ("still no visual, this is the last chance");
			pVisInfo = glXChooseVisual (XDisplay,
				DefaultScreen (XDisplay),
				doubleBufferAttributes);
		}
	}
	if (pVisInfo != NULL)
	{
		cd_message ("ok, got a visual");
		pGlConfig = gdk_x11_gl_config_new_from_visualid (pVisInfo->visualid);
		XFree (pVisInfo);
	}
	else
	{
		cd_warning ("couldn't find a suitable GLX Visual, OpenGL can't be used.\n (sorry to say that, but your graphic card and/or its driver is crappy)");
	}
	
	return pGlConfig;
}

void cairo_dock_apply_desktop_background (CairoContainer *pContainer)
{
	if (! mySystem.bUseFakeTransparency || g_iDesktopBgTexture == 0)
		return ;
	
	glPolygonMode (GL_FRONT, GL_FILL);
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, g_iDesktopBgTexture);
	glColor4f(1., 1., 1., 1.);
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ZERO);  /// utile ?
	
	glBegin(GL_QUADS);
	glTexCoord2f (1.*(pContainer->iWindowPositionX + 0.)/g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL],
	1.*(pContainer->iWindowPositionY + 0.)/g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
	glVertex3f (0., pContainer->iHeight, 0.);  // Top Left.
	
	glTexCoord2f (1.*(pContainer->iWindowPositionX + pContainer->iWidth)/g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL], 1.*(pContainer->iWindowPositionY + 0.)/g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
	glVertex3f (pContainer->iWidth, pContainer->iHeight, 0.);  // Top Right
	
	glTexCoord2f (1.*(pContainer->iWindowPositionX + pContainer->iWidth)/g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL], 1.*(pContainer->iWindowPositionY + pContainer->iHeight)/g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
	glVertex3f (pContainer->iWidth, 0., 0.);  // Bottom Right
	
	glTexCoord2f (1.*(pContainer->iWindowPositionX + 0.)/g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL], 1.*(pContainer->iWindowPositionY + pContainer->iHeight)/g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
	glVertex3f (0., 0., 0.);  // Bottom Left
	glEnd();
	
	glDisable (GL_TEXTURE_2D);
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
			pSourceContext,
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
