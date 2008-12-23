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

#include <gtk/gtkgl.h>
#include <X11/extensions/Xrender.h> 
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <gdk/x11/gdkglx.h>

#include "cairo-dock-load.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-hidden-dock.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-draw-opengl.h"

#include "texture-gradation.h"
#define RADIAN (G_PI / 180.0)  // Conversion Radian/Degres
#define DELTA_ROUND_DEGREE 1

GLuint g_pGradationTexture[2];

extern cairo_surface_t *g_pDesktopBgSurface;

extern int g_iBackgroundTexture;
extern CairoDock *g_pMainDock;

extern GLuint g_iIndicatorTexture;

extern double g_fDropIndicatorWidth, g_fDropIndicatorHeight;

extern double g_fIndicatorWidth, g_fIndicatorHeight;
extern GLuint g_iIndicatorTexture;
extern GLuint g_iActiveIndicatorTexture;
extern GLuint g_pVisibleZoneTexture;
extern cairo_surface_t *g_pIndicatorSurface[2];
extern cairo_surface_t *g_pActiveIndicatorSurface;
extern cairo_surface_t *g_pVisibleZoneSurface;
extern gboolean g_bUseOpenGL;


static void _cairo_dock_draw_appli_indicator_opengl (Icon *icon, gboolean bHorizontalDock, double fRatio, gboolean bDirectionUp)
{
	if (g_iIndicatorTexture == 0 && g_pIndicatorSurface[CAIRO_DOCK_HORIZONTAL] != NULL)
	{
		g_iIndicatorTexture = cairo_dock_create_texture_from_surface (g_pIndicatorSurface[CAIRO_DOCK_HORIZONTAL]);
		g_print ("g_iIndicatorTexture <- %d\n", g_iIndicatorTexture);
	}
	
	//\__________________ On place l'indicateur.
	if (icon->fOrientation != 0)
		glRotatef (icon->fOrientation, 0., 0., 1.);
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
			glScalef (g_fIndicatorWidth * fRatio * icon->fScale, (bDirectionUp ? 1:-1) * g_fIndicatorHeight * fRatio * icon->fScale, 1.);
		}
		else
		{
			if (bDirectionUp)
				fY = - fY;
			glTranslatef (fY, 0., 1.);
			glRotatef (90, 0., 0., 1.);
			glScalef (g_fIndicatorWidth * fRatio * icon->fScale, (bDirectionUp ? 1:-1) * g_fIndicatorHeight * fRatio * icon->fScale, 1.);
		}
	}

	//\__________________ On dessine l'indicateur.
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  /// utile ?
	glDisable(GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT, GL_FILL);
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, g_iIndicatorTexture);
	glColor4f(1., 1., 1., 1.);
	glNormal3f (0., 0., 1.);
	
	glBegin(GL_QUADS);
	glTexCoord2f(0., 0.); glVertex3f(-.5,  .5, 0.);  // Bottom Left Of The Texture and Quad
	glTexCoord2f(1., 0.); glVertex3f( .5,  .5, 0.);  // Bottom Right Of The Texture and Quad
	glTexCoord2f(1., 1.); glVertex3f( .5, -.5, 0.);  // Top Right Of The Texture and Quad
	glTexCoord2f(0., 1.); glVertex3f(-.5, -.5, 0.);  // Top Left Of The Texture and Quad
	glEnd();
	
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
}
static void _cairo_dock_draw_active_window_indicator_opengl (Icon *icon, CairoDock *pDock, double fRatio)
{
	if (g_iActiveIndicatorTexture == 0 && g_pActiveIndicatorSurface != NULL)
	{
		g_iActiveIndicatorTexture = cairo_dock_create_texture_from_surface (g_pActiveIndicatorSurface);
		g_print ("g_iActiveIndicatorTexture <- %d\n", g_iActiveIndicatorTexture);
	}
	
	if (icon->fOrientation != 0)
		glRotatef (icon->fOrientation, 0., 0., 1.);
	
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT, GL_FILL);
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, g_iActiveIndicatorTexture);
	glColor4f(1., 1., 1., 1.);
	glNormal3f (0., 0., 1.);
	
	cairo_dock_set_icon_scale (icon, pDock, 1.);
	glBegin(GL_QUADS);
	glTexCoord2f(0., 0.); glVertex3f(-.5,  .5, 0.);  // Bottom Left Of The Texture and Quad
	glTexCoord2f(1., 0.); glVertex3f( .5,  .5, 0.);  // Bottom Right Of The Texture and Quad
	glTexCoord2f(1., 1.); glVertex3f( .5, -.5, 0.);  // Top Right Of The Texture and Quad
	glTexCoord2f(0., 1.); glVertex3f(-.5, -.5, 0.);  // Top Left Of The Texture and Quad
	glEnd();
	
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
}
void cairo_dock_set_icon_scale (Icon *pIcon, CairoDock *pDock, double fZoomFactor)
{
	if (pDock->bHorizontalDock)
	{
		if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (pIcon))
		{
			glScalef (pIcon->fWidth / 2, pIcon->fHeight / 2, pIcon->fHeight / 2);
		}
		else
		{
			glScalef (pIcon->fWidth * pIcon->fWidthFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor,
				pIcon->fHeight * pIcon->fHeightFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor,
				pIcon->fHeight * pIcon->fHeightFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor);
		}
	}
	else
	{
		if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (pIcon))
		{
			glScalef (pIcon->fHeight / 2, pIcon->fWidth / 2, pIcon->fWidth / 2);
		}
		else
		{
			glScalef (pIcon->fHeight * pIcon->fHeightFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor,
				pIcon->fWidth * pIcon->fWidthFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor,
				pIcon->fWidth * pIcon->fWidthFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor);
		}
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
	glEnable(GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, pIcon->iIconTexture);
	
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
	glDisable(GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT, GL_FILL);
	glColor4f(1., 1., 1., pIcon->fAlpha);
	
	glPushMatrix ();
	cairo_dock_set_icon_scale (pIcon, pDock, 1.);
	glNormal3f(0,0,1);
	glBegin(GL_QUADS);
	glTexCoord2f(0., 0.); glVertex3f(-.5,  .5, 0.);  // Bottom Left Of The Texture and Quad
	glTexCoord2f(1., 0.); glVertex3f( .5,  .5, 0.);  // Bottom Right Of The Texture and Quad
	glTexCoord2f(1., 1.); glVertex3f( .5, -.5, 0.);  // Top Right Of The Texture and Quad
	glTexCoord2f(0., 1.); glVertex3f(-.5, -.5, 0.);  // Top Left Of The Texture and Quad
	glEnd();
	glPopMatrix ();
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	
	if (pDock->bUseReflect)
	{
		glPushMatrix ();
		double x0, y0, x1, y1;
		double fScale = ((myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (pIcon)) ? 1. : pIcon->fScale);
		double fReflectRatio = myIcons.fReflectSize / pIcon->fHeight / fScale  / pIcon->fHeightFactor;
		double fOffsetY = pIcon->fHeight * fScale/2 + myIcons.fReflectSize/2 + pIcon->fDeltaYReflection;
		if (pDock->bHorizontalDock)
		{
			if (pDock->bDirectionUp)
			{
				glTranslatef (0., - fOffsetY, 0.);
				glScalef (pIcon->fWidth * pIcon->fWidthFactor * fScale, - myIcons.fReflectSize, 1.);  // taille du reflet et on se retourne.
				x0 = 0.;
				y0 = 1. - fReflectRatio;
				x1 = 1.;
				y1 = 1.;
			}
			else
			{
				glTranslatef (0., fOffsetY, 0.);
				glScalef (pIcon->fWidth * pIcon->fWidthFactor * fScale, myIcons.fReflectSize, 1.);
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
				glScalef (- myIcons.fReflectSize, pIcon->fWidth * pIcon->fWidthFactor * fScale, 1.);
				x0 = 1. - fReflectRatio;
				y0 = 0.;
				x1 = 1.;
				y1 = 1.;
			}
			else
			{
				glTranslatef (- fOffsetY, 0., 0.);
				glScalef (myIcons.fReflectSize, pIcon->fWidth * pIcon->fWidthFactor * fScale, 1.);
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
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//glBlendFunc (GL_SRC_ALPHA, GL_DST_ALPHA);
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		
		glBlendColor(1., 1., 1., 1.);  // utile ?
		
		glPolygonMode (GL_FRONT, GL_FILL);
		glColor4f(1., 1., 1., 1.);
		
		glBegin(GL_QUADS);
		
		glTexCoord2f (x0, y0);
		glColor4f (1., 1., 1., 0.);
		glVertex3f (-.5, .5, 0.);  // Bottom Left Of The Texture and Quad
		
		glTexCoord2f (x1, y0);
		glColor4f (1., 1., 1., 0.);
		glVertex3f (.5, .5, 0.);  // Bottom Right Of The Texture and Quad
		
		glTexCoord2f (x1, y1);
		glColor4f (1., 1., 1., myIcons.fAlbedo * pIcon->fAlpha);
		glVertex3f (.5, -.5, 0.);  // Top Right Of The Texture and Quad
		
		glTexCoord2f (x0, y1);
		glColor4f (1., 1., 1., myIcons.fAlbedo * pIcon->fAlpha);
		glVertex3f (-.5, -.5, 0.);  // Top Left Of The Texture and Quad
		
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
		g_print ("g_pGradationTexture(%d) <- %d\n", pDock->bHorizontalDock, g_pGradationTexture[pDock->bHorizontalDock]);
	}
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.fVisibleAppliAlpha != 0 && ! CAIRO_DOCK_IS_APPLET (icon))
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
		glRotatef (icon->fOrientation, 0., 0., 1.);
	if (icon->iRotationX != 0)
		glRotatef (icon->iRotationX, 1., 0., 0.);
	if (icon->iRotationY != 0)
		glRotatef (icon->iRotationY, 0., 1., 0.);
	
	//\_____________________ On dessine l'icone.
	/*if (icon->iCount > 0 && icon->iAnimationType == CAIRO_DOCK_PULSE)
	{
		if (icon->fAlpha > 0)
		{
			glPushMatrix ();
			double fScaleFactor = 1 + (1 - icon->fAlpha);
			glScalef (icon->fWidth * icon->fScale / 2 * fScaleFactor, icon->fHeight * icon->fScale / 2 * fScaleFactor, 1.);
			glColor4f(1., 1., 1., icon->fAlpha);
			glEnable (GL_TEXTURE_2D);
			glEnable (GL_BLEND);
			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBindTexture (GL_TEXTURE_2D, icon->iIconTexture);
			glBegin(GL_QUADS);
			glTexCoord2f(0., 0.); glVertex3f(-1., 1,  0.);  // Bottom Left Of The Texture and Quad
			glTexCoord2f(1., 0.); glVertex3f( 1., 1,  0.);  // Bottom Right Of The Texture and Quad
			glTexCoord2f(1., 1.); glVertex3f( 1., -1,  0.);  // Top Right Of The Texture and Quad
			glTexCoord2f(0., 1.); glVertex3f(-1.,  -1,  0.);  // Top Left Of The Texture and Quad
			glEnd();
			glDisable (GL_BLEND);
			glDisable (GL_TEXTURE_2D);
			glPopMatrix ();
		}
		icon->fAlpha = .8;
	}*/
	
	/**GLfloat fWhite[4] = {1., 1., 1., fAlpha};
	GLfloat fBlack[4] = {0., 0., 0., 0.5};
	GLfloat amb[] = {0.5,0.5,0.5,1.0};
	//glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, amb);
	glMaterialfv (GL_BACK,GL_AMBIENT,fBlack);
	glMaterialfv (GL_BACK,GL_DIFFUSE,fBlack);
	glMaterialfv (GL_BACK,GL_SPECULAR,fBlack);
	
	GLfloat dif[] = {0.8,0.8,0.8,1.0};
	GLfloat spec[] = {1.0,1.0,1.0,1.0};
	GLfloat shininess[] = {50.0};
	glMaterialfv(GL_FRONT,GL_DIFFUSE,dif);
	glMaterialfv(GL_FRONT,GL_SPECULAR,spec);
	glMaterialfv(GL_FRONT,GL_SHININESS,shininess);*/
	
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
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	if (bUseText && icon->iLabelTexture != 0 && icon->fScale > 1.01 && (! mySystem.bLabelForPointedIconOnly || icon->bPointed) && icon->iCount == 0)  // 1.01 car sin(pi) = 1+epsilon :-/
	{
		glPushMatrix ();
		
		double fOffsetX = 0.;
		if (icon->fDrawX + icon->fWidth * icon->fScale/2 - icon->iTextWidth/2 < 0)
			fOffsetX = icon->iTextWidth/2 - (icon->fDrawX + icon->fWidth * icon->fScale/2);
		else if (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->iTextWidth/2 > pDock->iCurrentWidth)
			fOffsetX = pDock->iCurrentWidth - (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->iTextWidth/2);
		if (icon->fOrientation != 0 && ! mySystem.bTextAlwaysHorizontal)
		{
			//cairo_rotate (pCairoContext, icon->fOrientation);
			glRotatef (icon->fOrientation, 0., 0., 1.);
		}
		
		if (! pDock->bHorizontalDock && mySystem.bTextAlwaysHorizontal)
		{
			glTranslatef (-icon->fHeight * icon->fScale/2 + icon->iTextWidth / 2,
				(icon->fWidth * icon->fScale + icon->iTextHeight) / 2,
				0.);
		}
		else if (pDock->bHorizontalDock)
		{
			glTranslatef (fOffsetX, (pDock->bDirectionUp ? 1:-1) * (icon->fHeight * icon->fScale/2 + myLabels.iconTextDescription.iSize - icon->iTextHeight / 2), 0.);
		}
		else
		{
			glTranslatef ((pDock->bDirectionUp ? -.5:.5) * (icon->fHeight * icon->fScale + icon->iTextHeight),
				0.,
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
			fMagnitude *= (fMagnitude * mySystem.fLabelAlphaThreshold + 1) / (mySystem.fLabelAlphaThreshold + 1);
		}
		
		glColor4f(1., 1., 1., fMagnitude);
		cairo_dock_draw_texture (icon->iLabelTexture, icon->iTextWidth, icon->iTextHeight);
		
		glPopMatrix ();
	}
	
	//\_____________________ On dessine les infos additionnelles.
	if (icon->iQuickInfoTexture != 0)
	{
		glPushMatrix ();
		glTranslatef (0., (- icon->fHeight + icon->iQuickInfoHeight * fRatio) * icon->fScale/2, 0.);
		
		glColor4f(1., 1., 1., icon->fAlpha);
		cairo_dock_draw_texture (icon->iQuickInfoTexture, icon->iQuickInfoWidth * fRatio * icon->fScale, icon->iQuickInfoHeight * fRatio * icon->fScale);
		
		glPopMatrix ();
	}
}


GLuint cairo_dock_create_texture_from_surface (cairo_surface_t *pImageSurface)
{
	g_return_val_if_fail (pImageSurface != NULL, 0);
	GLuint iTexture = 0;
	int w = cairo_image_surface_get_width (pImageSurface);
	int h = cairo_image_surface_get_height (pImageSurface);
	glEnable(GL_TEXTURE_2D);
	glGenTextures (1, &iTexture);
	g_print ("texture %d generee (%x, %dx%d)\n", iTexture, cairo_image_surface_get_data (pImageSurface), w, h);
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
		cairo_image_surface_get_data (pImageSurface));
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
		glBindTexture (GL_TEXTURE_2D, 0);
	}
}

void cairo_dock_update_label_texture (Icon *pIcon)
{
	if (pIcon->iLabelTexture != 0)
	{
		glDeleteTextures (1, &pIcon->iLabelTexture);
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
		glBindTexture (GL_TEXTURE_2D, 0);
	}
}

void cairo_dock_update_quick_info_texture (Icon *pIcon)
{
	if (pIcon->iQuickInfoTexture != 0)
	{
		glDeleteTextures (1, &pIcon->iQuickInfoTexture);
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
		glBindTexture (GL_TEXTURE_2D, 0);
	}
}


GLuint cairo_dock_create_texture_from_image_full (const gchar *cImagePath, double *fImageWidth, double *fImageHeight)
{
	g_return_val_if_fail (GTK_WIDGET_REALIZED (g_pMainDock->pWidget), 0);
	double fWidth=0, fHeight=0;
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (g_pMainDock));
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_image (cImagePath,
		pCairoContext,
		1.,
		0., 0.,
		CAIRO_DOCK_KEEP_RATIO,
		&fWidth,
		&fHeight,
		NULL, NULL);
	g_print ("texture genere (%x, %.2fx%.2f)\n", pSurface, fWidth, fHeight);
	cairo_destroy (pCairoContext);
	
	if (fImageWidth != NULL)
		*fImageWidth = fWidth;
	if (fImageHeight != NULL)
		*fImageHeight = fHeight;
	GLuint iTexture = cairo_dock_create_texture_from_surface (pSurface);
	cairo_surface_destroy (pSurface);
	return iTexture;
}

GLuint cairo_dock_load_local_texture (const gchar *cImageName, const gchar *cDirPath)
{
	g_return_val_if_fail (GTK_WIDGET_REALIZED (g_pMainDock->pWidget), 0);

	gchar *cTexturePath = g_strdup_printf ("%s/%s", cDirPath, cImageName);
	g_print ("%s\n", cTexturePath);
	GLuint iTexture = cairo_dock_create_texture_from_image (cTexturePath);
	g_free (cTexturePath);
	return iTexture;
}


void cairo_dock_render_background_opengl (CairoDock *pDock)
{
	//g_print ("%s (%d, %x)\n", __func__, pDock->bIsMainDock, g_pVisibleZoneSurface);
	if (g_pVisibleZoneTexture == 0 && g_pVisibleZoneSurface != NULL)
	{
		g_pVisibleZoneTexture = cairo_dock_create_texture_from_surface (g_pVisibleZoneSurface);
		g_print ("g_pVisibleZoneTexture <- %d\n", g_pVisibleZoneTexture);
	}
	if (g_pVisibleZoneTexture == 0)
		return ;
	
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glPolygonMode (GL_FRONT, GL_FILL);
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, g_pVisibleZoneTexture);
	glColor4f(1., 1., 1., 1.);
	glNormal3f (0., 0., 1.);
	
	glLoadIdentity ();
	glTranslatef (pDock->iCurrentWidth/2, pDock->iCurrentHeight/2, 0.);
	
	if (! pDock->bDirectionUp && myHiddenDock.bReverseVisibleImage)
		glScalef (1., -1., 1.);
	if (! pDock->bHorizontalDock)
		glRotatef (-90., 0, 0, 1);
	
	glScalef (pDock->iCurrentWidth, pDock->iCurrentHeight, 0.);
	
	glBegin(GL_QUADS);
	glTexCoord2f(0., 0.); glVertex3f(-.5,  .5, 0.);  // Bottom Left Of The Texture and Quad
	glTexCoord2f(1., 0.); glVertex3f( .5,  .5, 0.);  // Bottom Right Of The Texture and Quad
	glTexCoord2f(1., 1.); glVertex3f( .5, -.5, 0.);  // Top Right Of The Texture and Quad
	glTexCoord2f(0., 1.); glVertex3f(-.5, -.5, 0.);  // Top Left Of The Texture and Quad
	glEnd();
	
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
}


void cairo_dock_draw_texture (GLuint iTexture, int iWidth, int iHeight)
{
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, iTexture);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
	glPolygonMode (GL_FRONT, GL_FILL);
	glNormal3f (0., 0., 1.);
	glScalef (iWidth, iHeight, 1.);
	
	glBegin(GL_QUADS);
	glTexCoord2f(0., 0.); glVertex3f(-.5,  .5, 0.);  // Bottom Left Of The Texture and Quad
	glTexCoord2f(1., 0.); glVertex3f( .5,  .5, 0.);  // Bottom Right Of The Texture and Quad
	glTexCoord2f(1., 1.); glVertex3f( .5, -.5, 0.);  // Top Right Of The Texture and Quad
	glTexCoord2f(0., 1.); glVertex3f(-.5, -.5, 0.);  // Top Left Of The Texture and Quad
	glEnd();
	
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
}


const GLfloat *cairo_dock_generate_rectangle_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, int *iNbPoints)
{
	static GLfloat pVertexTab[((90/DELTA_ROUND_DEGREE+1)*4+1)*3];
	
	double fTotalWidth = fDockWidth + 2 * fRadius;
	double w = fDockWidth / fTotalWidth / 2;
	double h = MAX (0, fFrameHeight - 2 * fRadius) / fFrameHeight / 2;
	double rw = fRadius / fTotalWidth;
	double rh = fRadius / fFrameHeight;
	int i=0, t;
	int iPrecision = DELTA_ROUND_DEGREE;
	for (t = 0;t <= 90;t += iPrecision, i++) // cote haut droit.
	{
		pVertexTab[3*i] = w + rw * cos (t*RADIAN);
		pVertexTab[3*i+1] = h + rh * sin (t*RADIAN);
	}
	for (t = 90;t <= 180;t += iPrecision, i++) // haut gauche.
	{
		pVertexTab[3*i] = -w + rw * cos (t*RADIAN);
		pVertexTab[3*i+1] = h + rh * sin (t*RADIAN);
	}
	if (bRoundedBottomCorner)
	{
		for (t = 180;t <= 270;t += iPrecision, i++) // bas gauche.
		{
			pVertexTab[3*i] = -w + rw * cos (t*RADIAN);
			pVertexTab[3*i+1] = -h + rh * sin (t*RADIAN);
		}
		for (t = 270;t <= 360;t += iPrecision, i++) // bas droit.
		{
			pVertexTab[3*i] = w + rw * cos (t*RADIAN);
			pVertexTab[3*i+1] = -h + rh * sin (t*RADIAN);
		}
	}
	else
	{
		pVertexTab[3*i] = -w - rw; // bas gauche.
		pVertexTab[3*i+1] = -h - rh;
		i ++;
		pVertexTab[3*i] = w + rw; // bas droit.
		pVertexTab[3*i+1] = -h - rh;
		i ++;
	}
	pVertexTab[3*i] = w + rw;  // on boucle.
	pVertexTab[3*i+1] = h;
	
	*iNbPoints = i+1;
	return pVertexTab;
}

#define P(t,p,q,s) (1-t) * (1-t) * p + 2 * t * (1-t) * q + t * t * s;
GLfloat *cairo_dock_generate_trapeze_path (double fDockWidth, double fFrameHeight, double fRadius, gboolean bRoundedBottomCorner, double fInclination, double *fExtraWidth, int *iNbPoints)
{
	static GLfloat pVertexTab[((90/DELTA_ROUND_DEGREE+1)*4+1)*3];
	
	double a = atan (fInclination)/G_PI*180.;
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;
	
	*fExtraWidth = fInclination * (fFrameHeight - (bRoundedBottomCorner ? 2 : 1-cosa) * fRadius) + fRadius * (bRoundedBottomCorner ? 1 : cosa);
	double fTotalWidth = fDockWidth + 2*(*fExtraWidth);
	double dw = fInclination * (fFrameHeight - (bRoundedBottomCorner ? 2 : 1 - sina) * fRadius) / fTotalWidth;
	double w = fDockWidth / fTotalWidth / 2;
	double h = MAX (0, fFrameHeight - 2 * fRadius) / fFrameHeight / 2;
	double rw = fRadius / fTotalWidth;
	double rh = fRadius / fFrameHeight;
	double w_ = w + dw + (bRoundedBottomCorner ? 0 : rw * cosa);
	
	int i=0, t;
	int iPrecision = DELTA_ROUND_DEGREE;
	for (t = a;t <= 90;t += iPrecision, i++) // cote haut droit.
	{
		pVertexTab[3*i] = w + rw * cos (t*RADIAN);
		pVertexTab[3*i+1] = h + rh * sin (t*RADIAN);
	}
	for (t = 90;t <= 180-a;t += iPrecision, i++) // haut gauche.
	{
		pVertexTab[3*i] = -w + rw * cos (t*RADIAN);
		pVertexTab[3*i+1] = h + rh * sin (t*RADIAN);
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
			pVertexTab[3*i] = P(t, x0, x1, x2);
			pVertexTab[3*i+1] = P(t, y0, y1, y2);
		}
		
		double x3 = x0, y3 = y0;
		x0 = - x2;
		y0 = y2;
		x1 = - x1;
		x2 = - x3;
		y2 = y3;
		for (t=0; t<=1; t+=.05, i++) // bas gauche.
		{
			pVertexTab[3*i] = P(t, x0, x1, x2);
			pVertexTab[3*i+1] = P(t, y0, y1, y2);
		}
	}
	else
	{
		pVertexTab[3*i] = -w_; // bas gauche.
		pVertexTab[3*i+1] = -h - rh;
		i ++;
		pVertexTab[3*i] = w_; // bas droit.
		pVertexTab[3*i+1] = -h - rh;
		i ++;
	}
	pVertexTab[3*i] = w + rw * cos (a*RADIAN);  // on boucle.
	pVertexTab[3*i+1] = h + rh * sin (a*RADIAN);
	
	*iNbPoints = i+1;
	return pVertexTab;
}


void cairo_dock_draw_frame_background_opengl (GLuint iBackgroundTexture, double fDockWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, const GLfloat *pVertexTab, int iNbVertex, CairoDockTypeHorizontality bHorizontal, gboolean bDirectionUp)
{
	glEnable(GL_TEXTURE_2D); // Je veux de la texture
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBindTexture(GL_TEXTURE_2D, iBackgroundTexture); // allez on bind la texture
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR ); // ok la on selectionne le type de generation des coordonnees de la texture
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
	glEnable(GL_TEXTURE_GEN_S); // oui je veux une generation en S
	glEnable(GL_TEXTURE_GEN_T); // Et en T aussi
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	glLoadIdentity();
	
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
	
	if (iBackgroundTexture = 0)
		return ;
	
	glMatrixMode(GL_TEXTURE); // On selectionne la matrice des textures
	glPushMatrix ();
	glLoadIdentity(); // On la reset
	glTranslatef(0.5f, 0.5f, 0.);
	glScalef (1., -1., 1.);
	glMatrixMode(GL_MODELVIEW);
	
	glEnable(GL_BLEND); // On active le blend
	//glBlendFunc (GL_ONE, GL_ZERO);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(1., 1., 1., 1.); // Couleur a fond
	///glEnable(GL_POLYGON_OFFSET_FILL);
	///glPolygonOffset (1., 1.);
	glPolygonMode(GL_FRONT, GL_FILL);
	
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
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, pVertexTab);
	glDrawArrays(GL_TRIANGLE_FAN, 0, iNbVertex);
	glDisableClientState(GL_VERTEX_ARRAY);
	
	glDisable(GL_BLEND);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glDisable(GL_TEXTURE_2D); // Plus de texture merci 
	
	glMatrixMode(GL_TEXTURE); // On selectionne la matrice des textures
	glPopMatrix ();
	glMatrixMode(GL_MODELVIEW);
}

void cairo_dock_draw_current_path_opengl (double fLineWidth, double *fLineColor, const GLfloat *pVertexTab, int iNbVertex)
{
	///glEnable(GL_POLYGON_OFFSET_FILL);
	///glPolygonOffset (1., 1.);
	glPolygonMode(GL_FRONT, GL_LINE);
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint (GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glLineWidth(fLineWidth); // Ici on choisi l'epaisseur du contour du polygone 
	glColor4f(fLineColor[0], fLineColor[1], fLineColor[2], fLineColor[3]); // Et sa couleur 
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, pVertexTab);
	glDrawArrays(GL_LINE_LOOP, 0, iNbVertex);
	glDisableClientState(GL_VERTEX_ARRAY);
	
	glDisable(GL_BLEND);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glPolygonMode(GL_FRONT, GL_FILL);
}
