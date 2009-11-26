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

#include <gtk/gtk.h>

#include <cairo.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "cairo-dock-animations.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-default-view.h"

extern GLuint g_iBackgroundTexture;
extern int g_iScreenWidth[2];

void cd_calculate_max_dock_size_default (CairoDock *pDock)
{
	pDock->pFirstDrawnElement = cairo_dock_calculate_icons_positions_at_rest_linear (pDock->icons, pDock->fFlatDockWidth, pDock->iScrollOffset);

	pDock->iDecorationsHeight = pDock->iMaxIconHeight * pDock->container.fRatio + 2 * myBackground.iFrameMargin;

	double fRadius = MIN (myBackground.iDockRadius, (pDock->iDecorationsHeight + myBackground.iDockLineWidth) / 2 - 1);
	double fExtraWidth = myBackground.iDockLineWidth + 2 * (fRadius + myBackground.iFrameMargin);
	pDock->iMaxDockWidth = ceil (cairo_dock_calculate_max_dock_width (pDock, pDock->pFirstDrawnElement, pDock->fFlatDockWidth, 1., fExtraWidth));
	
	if (cairo_dock_is_extended_dock (pDock))  // mode panel etendu.
	{
		if (pDock->iMaxDockWidth < cairo_dock_get_max_authorized_dock_width (pDock))  // alors on etend.
		{
			fExtraWidth += (cairo_dock_get_max_authorized_dock_width (pDock) - pDock->iMaxDockWidth);
			pDock->iMaxDockWidth = ceil (cairo_dock_calculate_max_dock_width (pDock, pDock->pFirstDrawnElement, pDock->fFlatDockWidth, 1., fExtraWidth));  // on pourra optimiser, ce qui nous interesse ici c'est les fXMin/fXMax.
			g_print ("mode etendu : pDock->iMaxDockWidth : %d\n", pDock->iMaxDockWidth);
		}
	}
	
	pDock->iMaxDockHeight = (int) ((1 + myIcons.fAmplitude) * pDock->iMaxIconHeight) + myLabels.iLabelSize + myBackground.iDockLineWidth + myBackground.iFrameMargin;
	//g_print ("myLabels.iLabelSize : %d => %d\n", myLabels.iLabelSize, (int)pDock->iMaxDockHeight);

	pDock->iDecorationsWidth = pDock->iMaxDockWidth;
	pDock->iMinDockHeight = pDock->iMaxIconHeight + 2 * myBackground.iFrameMargin + 2 * myBackground.iDockLineWidth;
	
	/**if (cairo_dock_is_extended_dock (pDock))  // mode panel etendu.
	{
		pDock->iMinDockWidth = cairo_dock_get_max_authorized_dock_width (pDock);
	}
	else
	{
		pDock->iMinDockWidth = pDock->fFlatDockWidth + fExtraWidth;
	}
	
	//g_print ("clic area : %.2f\n", fExtraWidth/2);
	pDock->inputArea.x = (pDock->iMinDockWidth - pDock->fFlatDockWidth) / 2;
	pDock->inputArea.y = 0;
	pDock->inputArea.width = pDock->fFlatDockWidth;
	pDock->inputArea.height = pDock->iMinDockHeight;*/
	
	pDock->iMinLeftMargin = fExtraWidth/2;
	pDock->iMinRightMargin = fExtraWidth/2;
	Icon *pFirstIcon = cairo_dock_get_first_icon (pDock->icons);
	if (pFirstIcon != NULL)
		pDock->iMaxLeftMargin = pFirstIcon->fXMax;
	Icon *pLastIcon = cairo_dock_get_last_icon (pDock->icons);
	if (pLastIcon != NULL)
		pDock->iMaxRightMargin = pDock->iMaxDockWidth - (pLastIcon->fXMin + pLastIcon->fWidth);
	//g_print(" marges min: %d | %d\n marges max: %d | %d\n", pDock->iMinLeftMargin, pDock->iMinRightMargin, pDock->iMaxLeftMargin, pDock->iMaxRightMargin);
	
	pDock->iMinDockWidth = pDock->fFlatDockWidth;
}


void cd_render_default (cairo_t *pCairoContext, CairoDock *pDock)
{
	//\____________________ On trace le cadre.
	double fLineWidth = myBackground.iDockLineWidth;
	double fMargin = myBackground.iFrameMargin;
	double fRadius = (pDock->iDecorationsHeight + fLineWidth - 2 * myBackground.iDockRadius > 0 ? myBackground.iDockRadius : (pDock->iDecorationsHeight + fLineWidth) / 2 - 1);
	double fExtraWidth = 2 * fRadius + fLineWidth;
	double fDockWidth;
	int sens;
	double fDockOffsetX, fDockOffsetY;  // Offset du coin haut gauche du cadre.
	if (cairo_dock_is_extended_dock (pDock))  // mode panel etendu.
	{
		fDockWidth = pDock->container.iWidth - fExtraWidth;
		fDockOffsetX = fExtraWidth / 2;
	}
	else
	{
		fDockWidth = cairo_dock_get_current_dock_width_linear (pDock);
		Icon *pFirstIcon = cairo_dock_get_first_drawn_icon (pDock);
		fDockOffsetX = (pFirstIcon != NULL ? pFirstIcon->fX - fMargin : fExtraWidth / 2);
		if (fDockOffsetX < fExtraWidth / 2)
			fDockOffsetX = fExtraWidth / 2;
		if (fDockOffsetX + fDockWidth + fExtraWidth / 2 > pDock->container.iWidth)
			fDockWidth = pDock->container.iWidth - fDockOffsetX - fExtraWidth / 2;
	}
	if (pDock->container.bDirectionUp)
	{
		sens = 1;
		fDockOffsetY = pDock->container.iHeight - pDock->iDecorationsHeight - 1.5 * fLineWidth;
	}
	else
	{
		sens = -1;
		fDockOffsetY = pDock->iDecorationsHeight + 1.5 * fLineWidth;
	}

	cairo_save (pCairoContext);
	double fDeltaXTrapeze = cairo_dock_draw_frame (pCairoContext, fRadius, fLineWidth, fDockWidth, pDock->iDecorationsHeight, fDockOffsetX, fDockOffsetY, sens, 0., pDock->container.bIsHorizontal);

	//\____________________ On dessine les decorations dedans.
	fDockOffsetY = (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iDecorationsHeight - fLineWidth : fLineWidth);
	cairo_dock_render_decorations_in_frame (pCairoContext, pDock, fDockOffsetY, fDockOffsetX - fDeltaXTrapeze, fDockWidth + 2*fDeltaXTrapeze);

	//\____________________ On dessine le cadre.
	if (fLineWidth > 0)
	{
		cairo_set_line_width (pCairoContext, fLineWidth);
		cairo_set_source_rgba (pCairoContext, myBackground.fLineColor[0], myBackground.fLineColor[1], myBackground.fLineColor[2], myBackground.fLineColor[3]);
		cairo_stroke (pCairoContext);
	}
	else
		cairo_new_path (pCairoContext);
	cairo_restore (pCairoContext);

	//\____________________ On dessine la ficelle qui les joint.
	if (myIcons.iStringLineWidth > 0)
		cairo_dock_draw_string (pCairoContext, pDock, myIcons.iStringLineWidth, FALSE, FALSE);

	//\____________________ On dessine les icones et les etiquettes, en tenant compte de l'ordre pour dessiner celles en arriere-plan avant celles en avant-plan.
	cairo_dock_render_icons_linear (pCairoContext, pDock);
}



void cd_render_optimized_default (cairo_t *pCairoContext, CairoDock *pDock, GdkRectangle *pArea)
{
	//g_print ("%s ((%d;%d) x (%d;%d) / (%dx%d))\n", __func__, pArea->x, pArea->y, pArea->width, pArea->height, pDock->container.iWidth, pDock->container.iHeight);
	double fLineWidth = myBackground.iDockLineWidth;
	double fMargin = myBackground.iFrameMargin;
	int iWidth = pDock->container.iWidth;
	int iHeight = pDock->container.iHeight;

	//\____________________ On dessine les decorations du fond sur la portion de fenetre.
	cairo_save (pCairoContext);

	double fDockOffsetX, fDockOffsetY;
	if (pDock->container.bIsHorizontal)
	{
		fDockOffsetX = pArea->x;
		fDockOffsetY = (pDock->container.bDirectionUp ? iHeight - pDock->iDecorationsHeight - fLineWidth : fLineWidth);
	}
	else
	{
		fDockOffsetX = (pDock->container.bDirectionUp ? iHeight - pDock->iDecorationsHeight - fLineWidth : fLineWidth);
		fDockOffsetY = pArea->y;
	}

	if (pDock->container.bIsHorizontal)
		cairo_rectangle (pCairoContext, fDockOffsetX, fDockOffsetY, pArea->width, pDock->iDecorationsHeight);
	else
		cairo_rectangle (pCairoContext, fDockOffsetX, fDockOffsetY, pDock->iDecorationsHeight, pArea->height);

	fDockOffsetY = (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iDecorationsHeight - fLineWidth : fLineWidth);
	
	double fRadius = MIN (myBackground.iDockRadius, (pDock->iDecorationsHeight + myBackground.iDockLineWidth) / 2 - 1);
	double fOffsetX;
	if (cairo_dock_is_extended_dock (pDock))  // mode panel etendu.
	{
		fOffsetX = fRadius + fLineWidth / 2;
	}
	else
	{
		Icon *pFirstIcon = cairo_dock_get_first_drawn_icon (pDock);
		fOffsetX = (pFirstIcon != NULL ? pFirstIcon->fX - fMargin : fRadius + fLineWidth / 2);
	}
	double fDockWidth = cairo_dock_get_current_dock_width_linear (pDock);
	double fDeltaXTrapeze = fRadius;
	cairo_dock_render_decorations_in_frame (pCairoContext, pDock, fDockOffsetY, fOffsetX - fDeltaXTrapeze, fDockWidth + 2*fDeltaXTrapeze);
	
	//\____________________ On dessine la partie du cadre qui va bien.
	cairo_new_path (pCairoContext);

	if (pDock->container.bIsHorizontal)
	{
		cairo_move_to (pCairoContext, fDockOffsetX, fDockOffsetY - fLineWidth / 2);
		cairo_rel_line_to (pCairoContext, pArea->width, 0);
		cairo_set_line_width (pCairoContext, fLineWidth);
		cairo_set_source_rgba (pCairoContext, myBackground.fLineColor[0], myBackground.fLineColor[1], myBackground.fLineColor[2], myBackground.fLineColor[3]);
		cairo_stroke (pCairoContext);

		cairo_new_path (pCairoContext);
		cairo_move_to (pCairoContext, fDockOffsetX, (pDock->container.bDirectionUp ? iHeight - fLineWidth / 2 : pDock->iDecorationsHeight + 1.5 * fLineWidth));
		cairo_rel_line_to (pCairoContext, pArea->width, 0);
		cairo_set_line_width (pCairoContext, fLineWidth);
		cairo_set_source_rgba (pCairoContext, myBackground.fLineColor[0], myBackground.fLineColor[1], myBackground.fLineColor[2], myBackground.fLineColor[3]);
	}
	else
	{
		cairo_move_to (pCairoContext, fDockOffsetX - fLineWidth / 2, fDockOffsetY);
		cairo_rel_line_to (pCairoContext, 0, pArea->height);
		cairo_set_line_width (pCairoContext, fLineWidth);
		cairo_set_source_rgba (pCairoContext, myBackground.fLineColor[0], myBackground.fLineColor[1], myBackground.fLineColor[2], myBackground.fLineColor[3]);
		cairo_stroke (pCairoContext);

		cairo_new_path (pCairoContext);
		cairo_move_to (pCairoContext, (pDock->container.bDirectionUp ? iHeight - fLineWidth / 2 : pDock->iDecorationsHeight + 1.5 * fLineWidth), fDockOffsetY);
		cairo_rel_line_to (pCairoContext, 0, pArea->height);
		cairo_set_line_width (pCairoContext, fLineWidth);
		cairo_set_source_rgba (pCairoContext, myBackground.fLineColor[0], myBackground.fLineColor[1], myBackground.fLineColor[2], myBackground.fLineColor[3]);
	}
	cairo_stroke (pCairoContext);

	cairo_restore (pCairoContext);

	//\____________________ On dessine les icones impactees.
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);

	GList *pFirstDrawnElement = (pDock->pFirstDrawnElement != NULL ? pDock->pFirstDrawnElement : pDock->icons);
	if (pFirstDrawnElement != NULL)
	{
		double fXMin = (pDock->container.bIsHorizontal ? pArea->x : pArea->y), fXMax = (pDock->container.bIsHorizontal ? pArea->x + pArea->width : pArea->y + pArea->height);
		double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
		double fRatio = pDock->container.fRatio;
		double fXLeft, fXRight;
		
		//g_print ("redraw [%d -> %d]\n", (int) fXMin, (int) fXMax);
		Icon *icon;
		GList *ic = pFirstDrawnElement;
		do
		{
			icon = ic->data;

			fXLeft = icon->fDrawX + icon->fScale + 1;
			fXRight = icon->fDrawX + (icon->fWidth - 1) * icon->fScale * icon->fWidthFactor - 1;

			if (fXLeft < fXMax && fXRight > fXMin)
			{
				cairo_save (pCairoContext);
				//g_print ("dessin optimise de %s [%.2f -> %.2f]\n", icon->cName, fXLeft, fXRight);
				
				icon->fAlpha = 1;
				if (icon->iAnimationState == CAIRO_DOCK_STATE_AVOID_MOUSE)
				{
					icon->fAlpha = 0.7;
				}
				
				cairo_dock_render_one_icon (icon, pDock, pCairoContext, fDockMagnitude, TRUE);
				cairo_restore (pCairoContext);
			}

			ic = cairo_dock_get_next_element (ic, pDock->icons);
		} while (ic != pFirstDrawnElement);
	}
}



void cd_render_opengl_default (CairoDock *pDock)
{
	GLsizei w = pDock->container.iWidth;
	GLsizei h = pDock->container.iHeight;
	
	//\_____________ On definit notre rectangle.
	double fLineWidth = myBackground.iDockLineWidth;
	double fMargin = myBackground.iFrameMargin;
	double fRadius = (pDock->iDecorationsHeight + fLineWidth - 2 * myBackground.iDockRadius > 0 ? myBackground.iDockRadius : (pDock->iDecorationsHeight + fLineWidth) / 2 - 1);
	double fExtraWidth = 2 * fRadius + fLineWidth;
	double fDockWidth;
	double fFrameHeight = pDock->iDecorationsHeight + fLineWidth;
	
	int sens;
	double fDockOffsetX, fDockOffsetY;  // Offset du coin haut gauche du cadre.
	GList *pFirstDrawnElement = (pDock->pFirstDrawnElement != NULL ? pDock->pFirstDrawnElement : pDock->icons);
	if (pFirstDrawnElement == NULL)
		return ;
	if (cairo_dock_is_extended_dock (pDock))  // mode panel etendu.
	{
		fDockWidth = pDock->container.iWidth - fExtraWidth;
		fDockOffsetX = fRadius + fLineWidth / 2;
	}
	else
	{
		fDockWidth = cairo_dock_get_current_dock_width_linear (pDock);
		Icon *pFirstIcon = pFirstDrawnElement->data;
		fDockOffsetX = (pFirstIcon != NULL ? pFirstIcon->fX + 0 - fMargin : fRadius + fLineWidth / 2);
		if (fDockOffsetX - (fRadius + fLineWidth / 2) < 0)
			fDockOffsetX = fRadius + fLineWidth / 2;
		if (fDockOffsetX + fDockWidth + (fRadius + fLineWidth / 2) > pDock->container.iWidth)
			fDockWidth = pDock->container.iWidth - fDockOffsetX - (fRadius + fLineWidth / 2);
	}
	
	if ((pDock->container.bIsHorizontal && ! pDock->container.bDirectionUp) || (! pDock->container.bIsHorizontal && pDock->container.bDirectionUp))
		fDockOffsetY = pDock->container.iHeight - .5 * fLineWidth;
	else
		fDockOffsetY = pDock->iDecorationsHeight + 1.5 * fLineWidth;
	
	double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
	
	//\_____________ On genere les coordonnees du contour.
	int iNbVertex;
	const GLfloat *pVertexTab = cairo_dock_generate_rectangle_path (fDockWidth, fFrameHeight, fRadius, myBackground.bRoundedBottomCorner, &iNbVertex);
	
	if (! pDock->container.bIsHorizontal)
		fDockOffsetX = pDock->container.iWidth - fDockOffsetX + fRadius;
	else
		fDockOffsetX = fDockOffsetX - fRadius;
	
	//\_____________ On trace le fond en texturant par des triangles.
	glPushMatrix ();
	cairo_dock_draw_frame_background_opengl (g_iBackgroundTexture, fDockWidth+2*fRadius, fFrameHeight, fDockOffsetX, fDockOffsetY, pVertexTab, iNbVertex, pDock->container.bIsHorizontal, pDock->container.bDirectionUp, pDock->fDecorationsOffsetX);
	
	//\_____________ On trace le contour.
	if (fLineWidth != 0)
		cairo_dock_draw_current_path_opengl (fLineWidth, myBackground.fLineColor, iNbVertex);
	glPopMatrix ();
	
	//\_____________ On dessine la ficelle.
	if (myIcons.iStringLineWidth > 0)
		cairo_dock_draw_string_opengl (pDock, myIcons.iStringLineWidth, FALSE, FALSE);
	
	
	//\_____________ On dessine les icones.
	/**glEnable (GL_LIGHTING);  // pour indiquer a OpenGL qu'il devra prendre en compte l'eclairage.
	glLightModelf (GL_LIGHT_MODEL_TWO_SIDE, 1.);
	//glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);  // OpenGL doit considerer pour ses calculs d'eclairage que l'oeil est dans la scene (plus realiste).
	GLfloat fGlobalAmbientColor[4] = {.0, .0, .0, 0.};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, fGlobalAmbientColor);  // on definit la couleur de la lampe d'ambiance.
	glEnable (GL_LIGHT0);  // on allume la lampe 0.
	GLfloat fAmbientColor[4] = {.1, .1, .1, 1.};
	//glLightfv (GL_LIGHT0, GL_AMBIENT, fAmbientColor);
	GLfloat fDiffuseColor[4] = {.8, .8, .8, 1.};
	glLightfv (GL_LIGHT0, GL_DIFFUSE, fDiffuseColor);
	GLfloat fSpecularColor[4] = {.9, .9, .9, 1.};
	glLightfv (GL_LIGHT0, GL_SPECULAR, fSpecularColor);
	glLightf (GL_LIGHT0, GL_SPOT_EXPONENT, 10.);
	GLfloat fDirection[4] = {.3, .0, -.8, 0.};  // le dernier 0 <=> direction.
	glLightfv(GL_LIGHT0, GL_POSITION, fDirection);*/
	
	pFirstDrawnElement = cairo_dock_get_first_drawn_element_linear (pDock->icons);
	if (pFirstDrawnElement == NULL)
		return;
	
	glPushMatrix ();
	Icon *icon;
	GList *ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;
		
		glPushMatrix ();
		cairo_dock_render_one_icon_opengl (icon, pDock, fDockMagnitude, TRUE);
		glPopMatrix ();
		
		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);
	glPopMatrix ();
	//glDisable (GL_LIGHTING);
}



static void _cd_calculate_construction_parameters_generic (Icon *icon, CairoDock *pDock)
{
	icon->fDrawX = icon->fX;
	icon->fDrawY = icon->fY;
	icon->fWidthFactor = 1.;
	icon->fHeightFactor = 1.;
	///icon->fDeltaYReflection = 0.;
	icon->fOrientation = 0.;
	//if (icon->fDrawX >= 0 && icon->fDrawX + icon->fWidth * icon->fScale <= pDock->container.iWidth)
	{
		icon->fAlpha = 1;
	}
	/*else
	{
		icon->fAlpha = .25;
	}*/
}
Icon *cd_calculate_icons_default (CairoDock *pDock)
{
	Icon *pPointedIcon = cairo_dock_apply_wave_effect_linear (pDock);
	
	//\____________________ On calcule les position/etirements/alpha des icones.
	Icon* icon;
	GList* ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		_cd_calculate_construction_parameters_generic (icon, pDock);
	}
	
	cairo_dock_check_if_mouse_inside_linear (pDock);
	
	cairo_dock_check_can_drop_linear (pDock);
	
	return pPointedIcon;
}

void cairo_dock_register_default_renderer (void)
{
	CairoDockRenderer *pDefaultRenderer = g_new0 (CairoDockRenderer, 1);
	pDefaultRenderer->cReadmeFilePath = g_strdup_printf ("%s/readme-default-view", CAIRO_DOCK_SHARE_DATA_DIR);
	pDefaultRenderer->cPreviewFilePath = g_strdup_printf ("%s/preview-default.png", CAIRO_DOCK_SHARE_DATA_DIR);
	pDefaultRenderer->compute_size = cd_calculate_max_dock_size_default;
	pDefaultRenderer->calculate_icons = cd_calculate_icons_default;
	pDefaultRenderer->render = cd_render_default;
	pDefaultRenderer->render_optimized = cd_render_optimized_default;
	pDefaultRenderer->render_opengl = cd_render_opengl_default;
	pDefaultRenderer->set_subdock_position = cairo_dock_set_subdock_position_linear;
	pDefaultRenderer->bUseReflect = FALSE;
	pDefaultRenderer->cDisplayedName = gettext (CAIRO_DOCK_DEFAULT_RENDERER_NAME);

	cairo_dock_register_renderer (CAIRO_DOCK_DEFAULT_RENDERER_NAME, pDefaultRenderer);
}
