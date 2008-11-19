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
#include "cairo-dock-draw-opengl.h"
#define RADIAN (G_PI / 180.0)  // Conversion Radian/Degres
#define DELTA_ROUND_DEGREE 1

extern double g_fAmplitude;
extern CairoDockLabelDescription g_iconTextDescription;
extern cairo_surface_t *g_pDesktopBgSurface;

extern int g_iBackgroundTexture;
extern CairoDock *g_pMainDock;

extern double g_fVisibleAppliAlpha;
extern gboolean g_bConstantSeparatorSize;
extern double g_fAlphaAtRest;
extern double g_fReflectSize;
extern gboolean g_bIndicatorAbove;
extern gboolean g_bLabelForPointedIconOnly;
extern gboolean g_bTextAlwaysHorizontal;
extern double g_fLabelAlphaThreshold;
extern GLuint g_iIndicatorTexture;
extern int g_iSinusoidWidth;
extern gboolean g_bReverseVisibleImage;

extern double g_fDropIndicatorWidth, g_fDropIndicatorHeight;
extern GLuint g_iDropIndicatorTexture;

extern gboolean g_bLinkIndicatorWithIcon;
extern double g_fIndicatorWidth, g_fIndicatorHeight;
extern int g_iIndicatorDeltaY;
extern GLuint g_iIndicatorTexture;
extern GLuint g_iActiveIndicatorTexture;
extern GLuint g_pVisibleZoneTexture;
extern cairo_surface_t *g_pIndicatorSurface[2];
extern cairo_surface_t *g_pActiveIndicatorSurface;
extern int g_bActiveIndicatorAbove;
extern cairo_surface_t *g_pVisibleZoneSurface;


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
	if (g_bLinkIndicatorWithIcon)  // il se deforme et rebondit avec l'icone.
	{
		fY = - icon->fHeight * icon->fHeightFactor * icon->fScale/2
			+ (g_fIndicatorHeight - g_iIndicatorDeltaY*(1 + g_fAmplitude)) * fRatio * icon->fScale/2;
		if (! bDirectionUp)
			fY = - fY;
		if (bHorizontalDock)
		{
			glTranslatef (0., fY, 1.);
			glScalef (g_fIndicatorWidth * fRatio * icon->fScale * icon->fWidthFactor, g_fIndicatorHeight * fRatio * icon->fScale * icon->fHeightFactor, 1.);
		}
		else
		{
			glTranslatef (fY, 0., 1.);
			glScalef (g_fIndicatorHeight * fRatio * icon->fScale * icon->fHeightFactor, g_fIndicatorWidth * fRatio * icon->fScale * icon->fWidthFactor, 1.);
		}
		
	}
	else  // il est fixe, en bas de l'icone.
	{
		fY = - icon->fHeight * icon->fScale/2
			+ (g_fIndicatorHeight - g_iIndicatorDeltaY*(1 + g_fAmplitude)) * fRatio/2;
		if (! bDirectionUp)
			fY = - fY;
		if (bHorizontalDock)
		{
			glTranslatef (0., fY, 1.);
			glScalef (g_fIndicatorWidth * fRatio * icon->fScale, g_fIndicatorHeight * fRatio * icon->fScale, 1.);
		}
		else
		{
			glTranslatef (fY, 0., 1.);
			glScalef (g_fIndicatorHeight * fRatio * icon->fScale, g_fIndicatorWidth * fRatio * icon->fScale, 1.);
		}
	}

	//\__________________ On dessine l'indicateur.
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  /// utile ?
	glDisable(GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT, GL_FILL);
	glEnable (GL_TEXTURE);
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, g_iIndicatorTexture);
	glColor4f(1.0f, 1.0f, 1.0f, 1.);
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
	
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT, GL_FILL);
	glEnable (GL_TEXTURE);
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, g_iActiveIndicatorTexture);
	glColor4f(1.0f, 1.0f, 1.0f, 1.);
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
		if (g_bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (pIcon))
		{
			glScalef (pIcon->fWidth / 2, pIcon->fHeight / 2, pIcon->fHeight / 2);
		}
		else
		{
			glScalef (pIcon->fWidth * pIcon->fWidthFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor, pIcon->fHeight * pIcon->fHeightFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor, pIcon->fHeight * pIcon->fHeightFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor);
		}
	}
	else
	{
		if (g_bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (pIcon))
		{
			glScalef (pIcon->fHeight / 2, pIcon->fWidth / 2, pIcon->fWidth / 2);
		}
		else
		{
			glScalef (pIcon->fHeight * pIcon->fHeightFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor, pIcon->fWidth * pIcon->fWidthFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor, pIcon->fWidth * pIcon->fWidthFactor * pIcon->fScale * pIcon->fGlideScale * fZoomFactor);
		}
	}
}

gboolean cairo_dock_render_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered)
{
	if (*bHasBeenRendered)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	glEnable(GL_TEXTURE);
	glEnable(GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, pIcon->iIconTexture);
	
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
	glDisable(GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT, GL_FILL);
	glColor4f(1.0f, 1.0f, 1.0f, 1.);
	
	glPushMatrix ();
	cairo_dock_set_icon_scale (pIcon, pDock, 1.);
	
	glNormal3f(0,0,1);
	
	glBegin(GL_QUADS);
	glTexCoord2f(0., 0.); glVertex3f(-.5,  .5, 0.);  // Bottom Left Of The Texture and Quad
	glTexCoord2f(1., 0.); glVertex3f( .5,  .5, 0.);  // Bottom Right Of The Texture and Quad
	glTexCoord2f(1., 1.); glVertex3f( .5, -.5, 0.);  // Top Right Of The Texture and Quad
	glTexCoord2f(0., 1.); glVertex3f(-.5, -.5, 0.);  // Top Left Of The Texture and Quad
	glEnd();
	
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glPopMatrix ();
	
	*bHasBeenRendered = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

void cairo_dock_render_one_icon_opengl (Icon *icon, CairoDock *pDock, double fRatio, double fDockMagnitude, gboolean bUseText)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && g_fVisibleAppliAlpha != 0 && ! CAIRO_DOCK_IS_APPLET (icon))
	{
		double fAlpha = (icon->bIsHidden ? MIN (1 - g_fVisibleAppliAlpha, 1) : MIN (g_fVisibleAppliAlpha + 1, 1));
		if (fAlpha != 1)
			icon->fAlpha = fAlpha;  // astuce bidon pour pas multiplier 2 fois.
	}
	
	//\_____________________ On se place au centre de l'icone.
	double fX=0, fY=0;
	double fGlideScale;
	if (icon->fGlideOffset != 0)
	{
		double fPhase =  icon->fPhase + icon->fGlideOffset * icon->fWidth / fRatio / g_iSinusoidWidth * G_PI;
		if (fPhase < 0)
		{
			fPhase = 0;
		}
		else if (fPhase > G_PI)
		{
			fPhase = G_PI;
		}
		fGlideScale = (1 + fDockMagnitude * g_fAmplitude * sin (fPhase)) / icon->fScale;  // c'est un peu hacky ... il faudrait passer l'icone precedente en parametre ...
		/*if (bDirectionUp)
			if (bHorizontalDock)
				cairo_translate (pCairoContext, 0., (1-fGlideScale)*icon->fHeight*icon->fScale);
			else
				cairo_translate (pCairoContext, (1-fGlideScale)*icon->fHeight*icon->fScale, 0.);*/
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
		glTranslatef (fX, fY - icon->fHeight * icon->fScale * (1 - fGlideScale/2), -icon->fHeight * (1+g_fAmplitude));
	else
		glTranslatef (fY + icon->fHeight * icon->fScale * (1 - fGlideScale/2), fX, -icon->fHeight * (1+g_fAmplitude));
	glPushMatrix ();
	
	//\_____________________ On positionne l'icone.
	if (g_bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (icon))
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
	if (icon->fOrientation != 0)
		glRotatef (icon->fOrientation, 0., 0., 1.);
	if (icon->iRotationX != 0)
		glRotatef (icon->iRotationX, 1., 0., 0.);
	if (icon->iRotationY != 0)
		glRotatef (icon->iRotationY, 0., 1., 0.);
	
	//\_____________________ On dessine l'indicateur derriere.
	if (icon->bHasIndicator && ! g_bIndicatorAbove /*&& g_iIndicatorTexture != 0*/)
	{
		glPushMatrix ();
		glTranslatef (0., 0., icon->fHeight * (1+g_fAmplitude) -1);
		_cairo_dock_draw_appli_indicator_opengl (icon, pDock->bHorizontalDock, fRatio, pDock->bDirectionUp);
		glPopMatrix ();
	}
	if (icon->Xid != 0 && icon->Xid == cairo_dock_get_current_active_window () && ! g_bActiveIndicatorAbove && g_pActiveIndicatorSurface != NULL)
	{
		glPushMatrix ();
		glTranslatef (0., 0., icon->fHeight * (1+g_fAmplitude) -1);
		_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, fRatio);
		glPopMatrix ();
		/*cairo_save (pCairoContext);
		if (icon->fWidth / fRatio != g_fActiveIndicatorWidth || icon->fHeight / fRatio != g_fActiveIndicatorHeight)
		{
			cairo_scale (pCairoContext, icon->fWidth * icon->fWidthFactor * 1 / fRatio / g_fActiveIndicatorWidth, icon->fHeight * icon->fHeightFactor * 1 / fRatio / g_fActiveIndicatorHeight);
		}
		cairo_set_source_surface (pCairoContext, g_pActiveIndicatorSurface, 0., 0.);
		cairo_paint (pCairoContext);
		cairo_restore (pCairoContext);*/
	}
	
	//\_____________________ Cas de l'animation Pulse.
	double fPreviousAlpha = icon->fAlpha;
	if (icon->iCount > 0 && icon->iAnimationType == CAIRO_DOCK_PULSE)
	{
		if (icon->fAlpha > 0)
		{
			glPushMatrix ();
			double fScaleFactor = 1 + (1 - icon->fAlpha);
			glScalef (icon->fWidth * icon->fScale / 2 * fScaleFactor, icon->fHeight * icon->fScale / 2 * fScaleFactor, 1.);
			///glColor4f(1.0f, 1.0f, 1.0f, icon->fAlpha);
			GLfloat fMaterial[4] = {1., 1., 1., icon->fAlpha};
			glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, fMaterial);  // on definit Les proprietes materielles de l'objet.
			
			glBindTexture (GL_TEXTURE_2D, icon->iIconTexture);
			glBegin(GL_QUADS);
			glTexCoord2f(0., 0.); glVertex3f(-1.0f, 1,  0.);  // Bottom Left Of The Texture and Quad
			glTexCoord2f(1.0f, 0.); glVertex3f( 1.0f, 1,  0.);  // Bottom Right Of The Texture and Quad
			glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, -1,  0.);  // Top Right Of The Texture and Quad
			glTexCoord2f(0., 1.0f); glVertex3f(-1.0f,  -1,  0.);  // Top Left Of The Texture and Quad
			glEnd();
			glPopMatrix ();
			/*cairo_save (pCairoContext);
			double fScaleFactor = 1 + (1 - icon->fAlpha);
			if (bHorizontalDock)
				cairo_translate (pCairoContext, icon->fWidth / fRatio * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2, icon->fHeight / fRatio * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2);
			else
				cairo_translate (pCairoContext, icon->fHeight / fRatio * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2, icon->fWidth / fRatio * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2);
			cairo_scale (pCairoContext, fScaleFactor, fScaleFactor);
			if (icon->pIconBuffer != NULL)
				cairo_set_source_surface (pCairoContext, icon->pIconBuffer, 0.0, 0.0);
			cairo_paint_with_alpha (pCairoContext, icon->fAlpha);
			cairo_restore (pCairoContext);*/
			
		}
		icon->fAlpha = .8;
	}
	
	
	//\_____________________ On dessine l'icone.
	double fAlpha = icon->fAlpha * (fDockMagnitude + g_fAlphaAtRest * (1 - fDockMagnitude));
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
	cairo_dock_notify (CAIRO_DOCK_RENDER_ICON, icon, pDock, &bIconHasBeenDrawn);
	
	glPopMatrix ();  // retour juste apres la translation au milieu de l'icone.
	
	//\_____________________ On dessine les reflets.
	if (pDock->bUseReflect && icon->iReflectionTexture != 0)  // on dessine les reflets.
	{
		glBindTexture (GL_TEXTURE_2D, icon->iReflectionTexture);
		glPushMatrix ();
		
		//\_____________________ Cas de l'animation Pulse.
		if (icon->iCount > 0 && icon->iAnimationType == CAIRO_DOCK_PULSE)
		{
			if (fPreviousAlpha > 0)
			{
				glPushMatrix ();
				double fScaleFactor = 1 + (1 - fPreviousAlpha);
				glScalef (icon->fWidth * icon->fScale / 2 * fScaleFactor, icon->fHeight * icon->fScale / 2 * fScaleFactor, 1.);
				glColor4f(1.0f, 1.0f, 1.0f, fPreviousAlpha);
				glBegin(GL_QUADS);
				glTexCoord2f(0., 0.); glVertex3f(-1.0f, 1,  0.);  // Bottom Left Of The Texture and Quad
				glTexCoord2f(1.0f, 0.); glVertex3f( 1.0f, 1,  0.);  // Bottom Right Of The Texture and Quad
				glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, -1,  0.);  // Top Right Of The Texture and Quad
				glTexCoord2f(0., 1.0f); glVertex3f(-1.0f,  -1,  0.);  // Top Left Of The Texture and Quad
				glEnd ();
				glPopMatrix ();
				/*cairo_save (pCairoContext);
				double fScaleFactor = 1 + (1 - fPreviousAlpha);
				if (bHorizontalDock)
					cairo_translate (pCairoContext, icon->fWidth * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2, icon->fHeight * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2);
				else
					cairo_translate (pCairoContext, icon->fHeight * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2, icon->fWidth * (1 - fScaleFactor) * (1 + g_fAmplitude) / 2);
				cairo_scale (pCairoContext, fScaleFactor, fScaleFactor);
				if (icon->pIconBuffer != NULL)
					cairo_set_source_surface (pCairoContext, icon->pReflectionBuffer, 0.0, 0.0);
				cairo_paint_with_alpha (pCairoContext, fPreviousAlpha);
				cairo_restore (pCairoContext);*/
			}
		}
		
		//\_____________________ On positionne les reflets.
		if (pDock->bHorizontalDock)
		{
			glTranslatef (0., pDock->bDirectionUp ? - icon->fHeight * icon->fScale/2 - g_fReflectSize * icon->fScale/2 : icon->fHeight * icon->fScale/2 + g_fReflectSize * icon->fScale/2, 0.);
		}
		else
		{
			
		}
		
		//\_____________________ On dessine les reflets.
		glColor4f(1.0f, 1.0f, 1.0f, fAlpha);
		glScalef (icon->fWidth * icon->fScale / 2, g_fReflectSize * icon->fScale / 2, 1.);
		glBegin(GL_QUADS);
		glTexCoord2f(0., 0.); glVertex3f(-1.0f, 1,  0.);  // Bottom Left Of The Texture and Quad
		glTexCoord2f(1.0f, 0.); glVertex3f( 1.0f, 1,  0.);  // Bottom Right Of The Texture and Quad
		glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, -1,  0.);  // Top Right Of The Texture and Quad
		glTexCoord2f(0., 1.0f); glVertex3f(-1.0f,  -1,  0.);  // Top Left Of The Texture and Quad
		glEnd();
		glPopMatrix ();  // retour juste apres la translation (fDrawX, fDrawY).
		
		//\_____________________ Cas des reflets dynamiques.
		/*if (g_bDynamicReflection && icon->fScale > 1)
		{
			cairo_pattern_t *pGradationPattern;
			if (bHorizontalDock)
			{
				pGradationPattern = cairo_pattern_create_linear (0.,
					(bDirectionUp ? 0. : g_fReflectSize / fRatio * (1 + g_fAmplitude)),
					0.,
					(bDirectionUp ? g_fReflectSize / fRatio * (1 + g_fAmplitude) / icon->fScale : g_fReflectSize / fRatio * (1 + g_fAmplitude) * (1. - 1./ icon->fScale)));  // de haut en bas.
				g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);
				
				cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					0.,
					0.,
					0.,
					0.,
					1.);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					1.,
					0.,
					0.,
					0.,
					1 - (icon->fScale - 1) / g_fAmplitude);  // astuce pour ne pas avoir a re-creer la surface de la reflection.
			}
			else
			{
				pGradationPattern = cairo_pattern_create_linear ((bDirectionUp ? 0. : g_fReflectSize / fRatio * (1 + g_fAmplitude)),
					0.,
					(bDirectionUp ? g_fReflectSize / fRatio * (1 + g_fAmplitude) / icon->fScale : g_fReflectSize / fRatio * (1 + g_fAmplitude) * (1. - 1./ icon->fScale)),
					0.);
				g_return_if_fail (cairo_pattern_status (pGradationPattern) == CAIRO_STATUS_SUCCESS);
				
				cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					0.,
					0.,
					0.,
					0.,
					1.);
				cairo_pattern_add_color_stop_rgba (pGradationPattern,
					1.,
					0.,
					0.,
					0.,
					1. - (icon->fScale - 1) / g_fAmplitude);  // astuce pour ne pas avoir a re-creer la surface de la reflection.
			}
			cairo_save (pCairoContext);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
			cairo_translate (pCairoContext, 0, 0);
			cairo_mask (pCairoContext, pGradationPattern);
			cairo_restore (pCairoContext);

			cairo_pattern_destroy (pGradationPattern);
		}*/
	}
	
	//\_____________________ On dessine l'indicateur de tache active devant.
	if (icon->bHasIndicator && g_bIndicatorAbove/* && g_iIndicatorTexture != 0*/)
	{
		glPushMatrix ();
		_cairo_dock_draw_appli_indicator_opengl (icon, pDock->bHorizontalDock, fRatio, pDock->bDirectionUp);
		glPopMatrix ();
	}
	if (icon->Xid != 0 && icon->Xid == cairo_dock_get_current_active_window () && g_bActiveIndicatorAbove && g_pActiveIndicatorSurface != NULL)
	{
		glPushMatrix ();
		_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, fRatio);
		glPopMatrix ();
		/*cairo_save (pCairoContext);
		if (icon->fWidth / fRatio != g_fActiveIndicatorWidth || icon->fHeight / fRatio != g_fActiveIndicatorHeight)
		{
			cairo_scale (pCairoContext, icon->fWidth * icon->fWidthFactor * 1 / fRatio / g_fActiveIndicatorWidth, icon->fHeight * icon->fHeightFactor * 1 / fRatio / g_fActiveIndicatorHeight);
		}
		cairo_set_source_surface (pCairoContext, g_pActiveIndicatorSurface, 0., 0.);
		cairo_paint (pCairoContext);
		cairo_restore (pCairoContext);*/
	}
	
	
	//\_____________________ On dessine les etiquettes, avec un alpha proportionnel au facteur d'echelle de leur icone.
	if (bUseText && icon->iLabelTexture != 0 && icon->fScale > 1.01 && (! g_bLabelForPointedIconOnly || icon->bPointed) && icon->iCount == 0)  // 1.01 car sin(pi) = 1+epsilon :-/
	{
		glPushMatrix ();
		
		double fOffsetX = 0.;
		if (icon->fDrawX + icon->fWidth * icon->fScale/2 - icon->iTextWidth/2 < 0)
			fOffsetX = icon->iTextWidth/2 - (icon->fDrawX + icon->fWidth * icon->fScale/2);
		else if (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->iTextWidth/2 > pDock->iCurrentWidth)
			fOffsetX = pDock->iCurrentWidth - (icon->fDrawX + icon->fWidth * icon->fScale/2 + icon->iTextWidth/2);
		if (icon->fOrientation != 0 && ! g_bTextAlwaysHorizontal)
		{
			//cairo_rotate (pCairoContext, icon->fOrientation);
			glRotatef (icon->fOrientation, 0., 0., 1.);
		}
		
		if (! pDock->bHorizontalDock && g_bTextAlwaysHorizontal)
		{
			/*cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				0,
				0);*/
			glTranslatef (0., (pDock->bDirectionUp ? 1:-1)* (icon->fHeight * icon->fScale/2 + icon->iTextHeight / 2), 0.);
			
		}
		else if (pDock->bHorizontalDock)
		{
			glTranslatef (fOffsetX, (pDock->bDirectionUp ? 1:-1) * (icon->fHeight * icon->fScale/2 + g_iconTextDescription.iSize - icon->iTextHeight / 2), 0.);
			/*cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				fOffsetX,
				bDirectionUp ? -g_iconTextDescription.iSize : icon->fHeight * icon->fScale - icon->fTextYOffset);*/
		}
		else
		{
			glTranslatef ((pDock->bDirectionUp ? 1:-1)* (icon->fHeight * icon->fScale/2 + icon->iTextHeight / 2), fOffsetX, 0.);
			/*cairo_set_source_surface (pCairoContext,
				icon->pTextBuffer,
				bDirectionUp ? -g_iconTextDescription.iSize : icon->fHeight * icon->fScale - icon->fTextYOffset,
				fOffsetX);*/
		}
		
		double fMagnitude;
		if (g_bLabelForPointedIconOnly)
		{
			fMagnitude = fDockMagnitude;  // (icon->fScale - 1) / g_fAmplitude / sin (icon->fPhase);  // sin (phi ) != 0 puisque fScale > 1.
		}
		else
		{
			fMagnitude = (icon->fScale - 1) / g_fAmplitude;  /// il faudrait diviser par pDock->fMagnitudeMax ...
			fMagnitude *= (fMagnitude * g_fLabelAlphaThreshold + 1) / (g_fLabelAlphaThreshold + 1);
		}
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f(1.0f, 1.0f, 1.0f, fMagnitude);
		glEnable(GL_TEXTURE);
		glEnable(GL_TEXTURE_2D);
		glBindTexture (GL_TEXTURE_2D, icon->iLabelTexture);
		glScalef (icon->iTextWidth, icon->iTextHeight, 1.);
		glPolygonMode (GL_FRONT, GL_FILL);
		glNormal3f(0,0,1);
		glBegin(GL_QUADS);
		glTexCoord2f(0., 0.); glVertex3f(-.5,  .5, 0.);  // Bottom Left Of The Texture and Quad
		glTexCoord2f(1., 0.); glVertex3f( .5,  .5, 0.);  // Bottom Right Of The Texture and Quad
		glTexCoord2f(1., 1.); glVertex3f( .5, -.5, 0.);  // Top Right Of The Texture and Quad
		glTexCoord2f(0., 1.); glVertex3f(-.5, -.5, 0.);  // Top Left Of The Texture and Quad
		glEnd();
		///glDisable(GL_TEXTURE_2D); // Plus de texture merci 
		///glDisable(GL_TEXTURE);
		glDisable(GL_BLEND);
		glPopMatrix ();
	}
	
	//\_____________________ On dessine les infos additionnelles.
	if (icon->iQuickInfoTexture != 0)
	{
		glPushMatrix ();
		glTranslatef (0., (- icon->fHeight + icon->iQuickInfoHeight * fRatio) * icon->fScale/2, 0.);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f(1.0f, 1.0f, 1.0f, fAlpha);
		glEnable(GL_TEXTURE);
		glEnable(GL_TEXTURE_2D);
		glBindTexture (GL_TEXTURE_2D, icon->iQuickInfoTexture);
		glScalef (icon->iQuickInfoWidth * fRatio * icon->fScale, icon->iQuickInfoHeight * fRatio * icon->fScale, 1.);
		glPolygonMode (GL_FRONT, GL_FILL);
		glNormal3f(0,0,1);
		glBegin(GL_QUADS);
		glTexCoord2f(0., 0.); glVertex3f(-.5,  .5, 0.);  // Bottom Left Of The Texture and Quad
		glTexCoord2f(1., 0.); glVertex3f( .5,  .5, 0.);  // Bottom Right Of The Texture and Quad
		glTexCoord2f(1., 1.); glVertex3f( .5, -.5, 0.);  // Top Right Of The Texture and Quad
		glTexCoord2f(0., 1.); glVertex3f(-.5, -.5, 0.);  // Top Left Of The Texture and Quad
		glEnd();
		///glDisable(GL_TEXTURE_2D); // Plus de texture merci 
		///glDisable(GL_TEXTURE);
		glDisable(GL_BLEND);
		glPopMatrix ();
		/*cairo_translate (pCairoContext,
			//-icon->fQuickInfoXOffset + icon->fWidth / 2,
			//icon->fHeight - icon->fQuickInfoYOffset);
			(- icon->iQuickInfoWidth * fRatio + icon->fWidthFactor * icon->fWidth) / 2 * icon->fScale,
			(icon->fHeight - icon->iQuickInfoHeight * fRatio) * icon->fScale);
		
		cairo_scale (pCairoContext,
			fRatio * icon->fScale / (1 + g_fAmplitude) * 1,
			fRatio * icon->fScale / (1 + g_fAmplitude) * 1);
		
		cairo_set_source_surface (pCairoContext,
			icon->pQuickInfoBuffer,
			0,
			0);
		if (fAlpha == 1)
			cairo_paint (pCairoContext);
		else
			cairo_paint_with_alpha (pCairoContext, fAlpha);*/
	}
}


GLuint cairo_dock_create_texture_from_surface (cairo_surface_t *pImageSurface)
{
	GLuint iTexture = 0;
	int w = cairo_image_surface_get_width (pImageSurface);
	int h = cairo_image_surface_get_height (pImageSurface);
	glEnable(GL_TEXTURE);
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


GLuint cairo_dock_load_texture_from_raw_data (const guchar *pTextureRaw)
{
	GLuint iTexture = 0;
	
	glEnable (GL_TEXTURE);
	glEnable (GL_TEXTURE_2D);
	glGenTextures(1, &iTexture);
	glBindTexture(GL_TEXTURE_2D, iTexture);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, pTextureRaw);
	glBindTexture (GL_TEXTURE_2D, 0);
	
	return iTexture;
}


void cairo_dock_update_icon_texture (Icon *pIcon)
{
	if (pIcon != NULL && pIcon->pIconBuffer != NULL)
	{
		glEnable (GL_TEXTURE);
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
		glEnable (GL_TEXTURE);
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
		glEnable (GL_TEXTURE);
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
        g_print (cTexturePath);
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
	
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDisable(GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT, GL_FILL);
	glEnable (GL_TEXTURE);
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, g_pVisibleZoneTexture);
	glColor4f(1.0f, 1.0f, 1.0f, 1.);
	glNormal3f (0., 0., 1.);
	
	glTranslatef (pDock->iCurrentWidth/2, pDock->iCurrentHeight/2, 0.);
	
	double fRotation = (pDock->bHorizontalDock ? (! pDock->bDirectionUp && g_bReverseVisibleImage ? 180 : 0) : (pDock->bDirectionUp ? -90 : 90));
	if (fRotation != 0)
		glRotatef (fRotation, 0, 0, 1);
	
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
	
	glEnable (GL_TEXTURE);
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, iTexture);
	
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
	double fInclinaisonCadre = 0.;
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
	double fInclinaisonCadre = 0.;
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
		for (t = 180-a;t <= 270;t += iPrecision, i++) // bas gauche.
		{ 
			pVertexTab[3*i] = -w + rw * cos (t*RADIAN);
			pVertexTab[3*i+1] = -h + rh * sin (t*RADIAN);
		}
		for (t = 270;t <= 360+a;t += iPrecision, i++) // bas droit. 
		{ 
			pVertexTab[3*i] = w + rw * cos (t*RADIAN);
			pVertexTab[3*i+1] = -h + rh * sin (t*RADIAN);
		}
		pVertexTab[3*i] = w + rw;  // on boucle.
		pVertexTab[3*i+1] = h;
	}
	else
	{
		pVertexTab[3*i] = -w - rw * cosa - dw; // bas gauche.
		pVertexTab[3*i+1] = -h - rh;
		i ++;
		pVertexTab[3*i] = w + rw * cosa + dw; // bas droit.
		pVertexTab[3*i+1] = -h - rh;
		i ++;
	}
	
	*iNbPoints = i;
	return pVertexTab;
}


void cairo_dock_draw_frame_background_opengl (GLuint iBackgroundTexture, double fDockWidth, double fFrameHeight, double fDockOffsetX, double fDockOffsetY, const GLfloat *pVertexTab, int iNbVertex)
{
	glEnable(GL_TEXTURE_2D); // Je veux de la texture
	glBindTexture(GL_TEXTURE_2D, iBackgroundTexture); // allez on bind la texture
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR ); // ok la on selectionne le type de generation des coordonnees de la texture
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
	glEnable(GL_TEXTURE_GEN_S); // oui je veux une generation en S
	glEnable(GL_TEXTURE_GEN_T); // Et en T aussi
	
	glLoadIdentity();
	glTranslatef ((int) (fDockOffsetX + fDockWidth/2), (int) (fDockOffsetY - fFrameHeight/2), -1);  // (int) -pDock->iMaxIconHeight * (1 + g_fAmplitude) + 1
	
	glScalef (fDockWidth, fFrameHeight, 1.);
	
	glMatrixMode(GL_TEXTURE); // On selectionne la matrice des textures
	glPushMatrix ();
	glLoadIdentity(); // On la reset
	glTranslatef(0.5f, 0.5f, 0.);
	glScalef (1., -1., 1.);
	glMatrixMode(GL_MODELVIEW);
	
	glEnable(GL_BLEND); // On active le blend
	glBlendFunc (GL_SRC_ALPHA, 1.); // Transparence avec le canal alpha
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // Couleur a fond
	///glEnable(GL_POLYGON_OFFSET_FILL);
	///glPolygonOffset (1., 1.);
	glPolygonMode(GL_FRONT, GL_FILL);
	
	glBegin (GL_TRIANGLE_FAN);
	int i;
	for (i = 0; i <= iNbVertex; i++) // La on affiche un polygone plein texture
	{
		glVertex3fv (&pVertexTab[3*i]);
	}
	glEnd();
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
	glBegin(GL_LINE_LOOP);
	int i;
	for (i = 0; i < iNbVertex; i++) // Et on affiche le contour 
	{
		glVertex3fv (&pVertexTab[3*i]);
	}
	glEnd();
	glDisable(GL_BLEND);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glPolygonMode(GL_FRONT, GL_FILL);
}
