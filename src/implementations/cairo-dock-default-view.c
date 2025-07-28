
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
#include <stdlib.h>

#include <gtk/gtk.h>
#include <cairo.h>

#include "gldi-config.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"  // cairo_dock_calculate_magnitude
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-opengl-path.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-object.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-style-manager.h"
#include "cairo-dock-desktop-manager.h"  // gldi_dock_get_screen_width
#include "cairo-dock-default-view.h"


static void cd_calculate_max_dock_size_default (CairoDock *pDock)
{
	cairo_dock_calculate_icons_positions_at_rest_linear (pDock->icons, pDock->fFlatDockWidth);

	pDock->iDecorationsHeight = pDock->iMaxIconHeight * pDock->container.fRatio + 2 * myDocksParam.iFrameMargin;
	double fLineWidth = (myDocksParam.bUseDefaultColors ? myStyleParam.iLineWidth : myDocksParam.iDockLineWidth);
	double fRadius = (myDocksParam.bUseDefaultColors ? myStyleParam.iCornerRadius : myDocksParam.iDockRadius);
	if (pDock->iDecorationsHeight + fLineWidth - 2 * fRadius < 0)
		fRadius = (pDock->iDecorationsHeight + fLineWidth) / 2 - 1;
	double fExtraWidth = fLineWidth + 2 * (fRadius + myDocksParam.iFrameMargin);
	int iMaxDockWidth = ceil (cairo_dock_calculate_max_dock_width (pDock, pDock->fFlatDockWidth, 1., fExtraWidth));
	pDock->iOffsetForExtend = 0;
	pDock->iMaxDockWidth = iMaxDockWidth;
	
	///if (cairo_dock_is_extended_dock (pDock))  // mode panel etendu.
	if (pDock->iRefCount == 0)
	{
		if (pDock->iMaxDockWidth < cairo_dock_get_max_authorized_dock_width (pDock))  // alors on etend.
		{
			if (pDock->fAlign != .5)
			{
				pDock->iOffsetForExtend = (cairo_dock_get_max_authorized_dock_width (pDock) - pDock->iMaxDockWidth) / 2;
				cd_debug ("iOffsetForExtend : %d; iMaxDockWidth : %d; fExtraWidth : %d", pDock->iOffsetForExtend, pDock->iMaxDockWidth, (int) fExtraWidth);
			}
			fExtraWidth += (cairo_dock_get_max_authorized_dock_width (pDock) - pDock->iMaxDockWidth);
			pDock->iMaxDockWidth = ceil (cairo_dock_calculate_max_dock_width (pDock, pDock->fFlatDockWidth, 1., fExtraWidth));  // on pourra optimiser, ce qui nous interesse ici c'est les fXMin/fXMax.
			//g_print ("mode etendu : pDock->iMaxDockWidth : %d\n", pDock->iMaxDockWidth);
		}
	}
	
	pDock->iMaxDockHeight = (int) ceil ((1 + myIconsParam.fAmplitude) * pDock->iMaxIconHeight * pDock->container.fRatio) + fLineWidth + myDocksParam.iFrameMargin + (pDock->container.bIsHorizontal ? myIconsParam.iLabelSize : 0);
	//g_print ("myIconsParam.iLabelSize : %d => %d\n", myIconsParam.iLabelSize, (int)pDock->iMaxDockHeight);

	pDock->iDecorationsWidth = pDock->iMaxDockWidth;
	pDock->iMinDockHeight = pDock->iMaxIconHeight * pDock->container.fRatio + 2 * myDocksParam.iFrameMargin + 2 * fLineWidth;
	
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
	
	///if (pDock->fAlign != .5/** && cairo_dock_is_extended_dock (pDock)*/)
	///	pDock->iMinDockWidth = pDock->iMaxDockWidth;
	///else
		pDock->iMinDockWidth = MAX (1, pDock->fFlatDockWidth);  // fFlatDockWidth peut etre meme negatif avec un dock vide.
	
	///pDock->iActiveWidth = pDock->iMaxDockWidth;
	pDock->iActiveWidth = iMaxDockWidth;
	pDock->iActiveHeight = pDock->iMaxDockHeight;
	if (! pDock->container.bIsHorizontal)
		pDock->iMaxDockHeight += 8*myIconsParam.iLabelSize;  // vertical dock, add some padding to draw the labels.	
}


static void _draw_flat_separator (Icon *icon, G_GNUC_UNUSED CairoDock *pDock, cairo_t *pCairoContext, G_GNUC_UNUSED double fDockMagnitude)
{
	double fSizeX = icon->fWidth * icon->fScale, fSizeY = icon->fHeight;
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	
	if (myIconsParam.bSeparatorUseDefaultColors)
		gldi_style_colors_set_separator_color (pCairoContext);
	else
		gldi_color_set_cairo (pCairoContext, &myIconsParam.fSeparatorColor);
	cairo_move_to (pCairoContext, icon->fDrawX + .5 * fSizeX, icon->fDrawY + (pDock->container.bDirectionUp ? icon->fHeight * icon->fScale - fSizeY : 0));
	cairo_set_line_width (pCairoContext, myDocksParam.iDockLineWidth);
	cairo_rel_line_to (pCairoContext, 0., fSizeY);
	cairo_stroke (pCairoContext);
}

static void _draw_physical_separator (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext, G_GNUC_UNUSED double fDockMagnitude)
{
	double fSizeX = icon->fWidth * icon->fScale;
	cairo_set_line_width (pCairoContext, myDocksParam.iDockLineWidth);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_DEST_OUT);
	cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 1.0);
	
	if (pDock->container.bIsHorizontal)
	{
		cairo_translate (pCairoContext, icon->fDrawX, 0.);
		cairo_rectangle (pCairoContext, 0., 0., fSizeX, pDock->container.iHeight);
		cairo_fill (pCairoContext);
		
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
		if (myDocksParam.bUseDefaultColors)
			gldi_style_colors_set_line_color (pCairoContext);
		else
			gldi_color_set_cairo (pCairoContext, &myDocksParam.fLineColor);
		
		cairo_move_to (pCairoContext,
			-.5*myDocksParam.iDockLineWidth,
			pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iDecorationsHeight - myDocksParam.iDockLineWidth : 0.);  // coin haut gauche.
		cairo_rel_line_to (pCairoContext, 0., pDock->iDecorationsHeight + myDocksParam.iDockLineWidth);
		cairo_stroke (pCairoContext);
		
		cairo_move_to (pCairoContext,
			fSizeX + .5*myDocksParam.iDockLineWidth,
			pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iDecorationsHeight - myDocksParam.iDockLineWidth : 0.);  // coin haut droit.
		cairo_rel_line_to (pCairoContext, 0., pDock->iDecorationsHeight + myDocksParam.iDockLineWidth);
		cairo_stroke (pCairoContext);
	}
	else
	{
		cairo_translate (pCairoContext, 0., icon->fDrawX);
		cairo_rectangle (pCairoContext, 0., 0., pDock->container.iHeight, fSizeX);
		cairo_fill (pCairoContext);
		
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
		if (myDocksParam.bUseDefaultColors)
			gldi_style_colors_set_line_color (pCairoContext);
		else
			gldi_color_set_cairo (pCairoContext, &myDocksParam.fLineColor);
		
		cairo_move_to (pCairoContext,
			pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iDecorationsHeight - myDocksParam.iDockLineWidth : 0.,
			-.5*myDocksParam.iDockLineWidth);  // coin haut gauche.
		cairo_rel_line_to (pCairoContext, pDock->iDecorationsHeight + myDocksParam.iDockLineWidth, 0.);
		cairo_stroke (pCairoContext);
		
		cairo_move_to (pCairoContext,
			pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iDecorationsHeight - myDocksParam.iDockLineWidth : 0.,
			fSizeX + .5*myDocksParam.iDockLineWidth);  // coin haut droit.
		cairo_rel_line_to (pCairoContext, pDock->iDecorationsHeight + myDocksParam.iDockLineWidth, 0.);
		cairo_stroke (pCairoContext);
	}
}

static void _cairo_dock_draw_separator (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext, double fDockMagnitude)
{
	if (myIconsParam.iSeparatorType == CAIRO_DOCK_FLAT_SEPARATOR)
		_draw_flat_separator (icon, pDock, pCairoContext, fDockMagnitude);
	else
		_draw_physical_separator (icon, pDock, pCairoContext, fDockMagnitude);
}

static void cd_render_default (cairo_t *pCairoContext, CairoDock *pDock)
{
	//\____________________ On trace le cadre.
	double fLineWidth = (myDocksParam.bUseDefaultColors ? myStyleParam.iLineWidth : myDocksParam.iDockLineWidth);
	double fMargin = myDocksParam.iFrameMargin;
	double fRadius = (myDocksParam.bUseDefaultColors ? myStyleParam.iCornerRadius : myDocksParam.iDockRadius);
	if (pDock->iDecorationsHeight + fLineWidth - 2 * fRadius < 0)
		fRadius = (pDock->iDecorationsHeight + fLineWidth) / 2 - 1;
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
		Icon *pFirstIcon = cairo_dock_get_first_icon (pDock->icons);
		fDockOffsetX = (pFirstIcon != NULL ? pFirstIcon->fX - fMargin : fExtraWidth / 2);
		if (fDockOffsetX < fExtraWidth / 2)
			fDockOffsetX = fExtraWidth / 2;
		if (fDockOffsetX + fDockWidth + fExtraWidth / 2 > pDock->container.iWidth)
			fDockWidth = pDock->container.iWidth - fDockOffsetX - fExtraWidth / 2;
		fDockOffsetX += (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2);
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
	double fDeltaXTrapeze = cairo_dock_draw_frame (pCairoContext, fRadius, fLineWidth, fDockWidth, pDock->iDecorationsHeight, fDockOffsetX, fDockOffsetY, sens, 0., pDock->container.bIsHorizontal, myDocksParam.bRoundedBottomCorner);

	//\____________________ On dessine les decorations dedans.
	fDockOffsetY = (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iDecorationsHeight - fLineWidth : fLineWidth);
	cairo_dock_render_decorations_in_frame (pCairoContext, pDock, fDockOffsetY, fDockOffsetX - fDeltaXTrapeze, fDockWidth + 2*fDeltaXTrapeze);

	//\____________________ On dessine le cadre.
	if (fLineWidth > 0)
	{
		cairo_set_line_width (pCairoContext, fLineWidth);
		if (myDocksParam.bUseDefaultColors)
			gldi_style_colors_set_line_color (pCairoContext);
		else
			gldi_color_set_cairo (pCairoContext, &myDocksParam.fLineColor);
		cairo_stroke (pCairoContext);
	}
	else
		cairo_new_path (pCairoContext);
	cairo_restore (pCairoContext);

	//\____________________ On dessine la ficelle qui les joint.
	if (myIconsParam.iStringLineWidth > 0)
		cairo_dock_draw_string (pCairoContext, pDock, myIconsParam.iStringLineWidth, FALSE, FALSE);

	//\____________________ On dessine les icones et les etiquettes, en tenant compte de l'ordre pour dessiner celles en arriere-plan avant celles en avant-plan.
	GList *pFirstDrawnElement = cairo_dock_get_first_drawn_element_linear (pDock->icons);
	if (pFirstDrawnElement == NULL)
		return;
	
	double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);  // * pDock->fMagnitudeMax
	Icon *icon;
	GList *ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;

		cairo_save (pCairoContext);
		if (myIconsParam.iSeparatorType != CAIRO_DOCK_NORMAL_SEPARATOR && icon->cFileName == NULL && GLDI_OBJECT_IS_SEPARATOR_ICON (icon))
			_cairo_dock_draw_separator (icon, pDock, pCairoContext, fDockMagnitude);
		else
			cairo_dock_render_one_icon (icon, pDock, pCairoContext, fDockMagnitude, TRUE);
		cairo_restore (pCairoContext);

		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);
}



static void cd_render_optimized_default (cairo_t *pCairoContext, CairoDock *pDock, GdkRectangle *pArea)
{
	//g_print ("%s ((%d;%d) x (%d;%d) / (%dx%d))\n", __func__, pArea->x, pArea->y, pArea->width, pArea->height, pDock->container.iWidth, pDock->container.iHeight);
	double fLineWidth = myDocksParam.iDockLineWidth;
	double fMargin = myDocksParam.iFrameMargin;
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
	
	double fRadius = MIN (myDocksParam.iDockRadius, (pDock->iDecorationsHeight + myDocksParam.iDockLineWidth) / 2 - 1);
	double fOffsetX;
	if (cairo_dock_is_extended_dock (pDock))  // mode panel etendu.
	{
		fOffsetX = fRadius + fLineWidth / 2;
	}
	else
	{
		Icon *pFirstIcon = cairo_dock_get_first_icon (pDock->icons);
		fOffsetX = (pFirstIcon != NULL ? pFirstIcon->fX - fMargin : fRadius + fLineWidth / 2);
	}
	double fDockWidth = cairo_dock_get_current_dock_width_linear (pDock);
	double fDeltaXTrapeze = fRadius;
	cairo_dock_render_decorations_in_frame (pCairoContext, pDock, fDockOffsetY, fOffsetX - fDeltaXTrapeze, fDockWidth + 2*fDeltaXTrapeze);
	
	//\____________________ On dessine la partie du cadre qui va bien.
	if (myDocksParam.bUseDefaultColors)
		gldi_style_colors_set_line_color (pCairoContext);
	else
		gldi_color_set_cairo (pCairoContext, &myDocksParam.fLineColor);
	cairo_new_path (pCairoContext);

	if (pDock->container.bIsHorizontal)
	{
		cairo_move_to (pCairoContext, fDockOffsetX, fDockOffsetY - fLineWidth / 2);
		cairo_rel_line_to (pCairoContext, pArea->width, 0);
		cairo_set_line_width (pCairoContext, fLineWidth);
		cairo_stroke (pCairoContext);

		cairo_new_path (pCairoContext);
		cairo_move_to (pCairoContext, fDockOffsetX, (pDock->container.bDirectionUp ? iHeight - fLineWidth / 2 : pDock->iDecorationsHeight + 1.5 * fLineWidth));
		cairo_rel_line_to (pCairoContext, pArea->width, 0);
		cairo_set_line_width (pCairoContext, fLineWidth);
	}
	else
	{
		cairo_move_to (pCairoContext, fDockOffsetX - fLineWidth / 2, fDockOffsetY);
		cairo_rel_line_to (pCairoContext, 0, pArea->height);
		cairo_set_line_width (pCairoContext, fLineWidth);
		cairo_stroke (pCairoContext);

		cairo_new_path (pCairoContext);
		cairo_move_to (pCairoContext, (pDock->container.bDirectionUp ? iHeight - fLineWidth / 2 : pDock->iDecorationsHeight + 1.5 * fLineWidth), fDockOffsetY);
		cairo_rel_line_to (pCairoContext, 0, pArea->height);
		cairo_set_line_width (pCairoContext, fLineWidth);
	}
	cairo_stroke (pCairoContext);

	cairo_restore (pCairoContext);

	//\____________________ On dessine les icones impactees.
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);

	GList *pFirstDrawnElement = pDock->icons;
	if (pFirstDrawnElement != NULL)
	{
		double fXMin = (pDock->container.bIsHorizontal ? pArea->x : pArea->y), fXMax = (pDock->container.bIsHorizontal ? pArea->x + pArea->width : pArea->y + pArea->height);
		double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
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
				
				if (myIconsParam.iSeparatorType != CAIRO_DOCK_NORMAL_SEPARATOR && icon->cFileName == NULL && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
					_cairo_dock_draw_separator (icon, pDock, pCairoContext, fDockMagnitude);
				else
					cairo_dock_render_one_icon (icon, pDock, pCairoContext, fDockMagnitude, TRUE);
				cairo_restore (pCairoContext);
			}

			ic = cairo_dock_get_next_element (ic, pDock->icons);
		} while (ic != pFirstDrawnElement);
	}
}



static void _draw_flat_separator_opengl (Icon *icon, CairoDock *pDock, G_GNUC_UNUSED double fDockMagnitude)
{
	double fSizeX = icon->fWidth * icon->fScale, fSizeY = icon->fHeight;
	if (myIconsParam.bSeparatorUseDefaultColors)
		gldi_style_colors_set_separator_color (NULL);
	else
		gldi_color_set_opengl (&myIconsParam.fSeparatorColor);
	glPolygonMode (GL_FRONT, GL_LINE);
	glEnable (GL_LINE_SMOOTH);
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable (GL_BLEND);
	_cairo_dock_set_blend_alpha ();
	glLineWidth(myDocksParam.iDockLineWidth);
	
	if (pDock->container.bIsHorizontal)
	{
		glTranslatef (icon->fDrawX + fSizeX/2, pDock->container.iHeight - icon->fDrawY - icon->fHeight * icon->fScale + fSizeY/2, 0.);  // must be before the glBegin()
		glBegin(GL_LINES);
		glVertex3f(0., -.5*fSizeY, 0.);
		glVertex3f(0.,  .5*fSizeY, 0.);
	}
	else
	{
		glTranslatef (icon->fDrawY + fSizeY/2, pDock->container.iWidth - (icon->fDrawX + fSizeX/2), 0.);
		glBegin(GL_LINES);
		glVertex3f(-.5*fSizeY, 0., 0.);
		glVertex3f( .5*fSizeY, 0., 0.);
	}
	glEnd();
	glDisable (GL_LINE_SMOOTH);
	glDisable (GL_BLEND);
}

static void _draw_physical_separator_opengl (Icon *icon, CairoDock *pDock, G_GNUC_UNUSED double fDockMagnitude)
{
	double fSizeX = icon->fWidth * icon->fScale;
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ZERO);
	glColor4f (0., 0., 0., 0.);
	
	glPolygonMode (GL_FRONT, GL_FILL);
	glLineWidth (myDocksParam.iDockLineWidth);
	
	if (pDock->container.bIsHorizontal)
	{
		glTranslatef (icon->fDrawX, 0., 0.);
		glBegin(GL_QUADS);
		glVertex3f(0., 0., 0.);
		glVertex3f(fSizeX, 0., 0.);
		glVertex3f(fSizeX, pDock->container.iHeight, 0.);
		glVertex3f(0., pDock->container.iHeight, 0.);
		glEnd();
		
		_cairo_dock_set_blend_alpha ();
		if (myDocksParam.bUseDefaultColors)
			gldi_style_colors_set_line_color (NULL);
		else
			gldi_color_set_opengl (&myDocksParam.fLineColor);
		glPolygonMode(GL_FRONT, GL_LINE);
		
		if (! pDock->container.bDirectionUp)
			glTranslatef (0., pDock->container.iHeight - pDock->iDecorationsHeight - myDocksParam.iDockLineWidth, 0.);
		
		glBegin(GL_LINES);
		glVertex3f(-.5*myDocksParam.iDockLineWidth, 0., 0.);
		glVertex3f(-.5*myDocksParam.iDockLineWidth, pDock->iDecorationsHeight + myDocksParam.iDockLineWidth, 0.);
		glEnd();
		
		glBegin(GL_LINES);
		glVertex3f(fSizeX + .5*myDocksParam.iDockLineWidth, 0., 0.);
		glVertex3f(fSizeX + .5*myDocksParam.iDockLineWidth, pDock->iDecorationsHeight + myDocksParam.iDockLineWidth, 0.);
		glEnd();
	}
	else
	{
		glTranslatef (0., pDock->container.iWidth - icon->fDrawX - fSizeX, 0.);
		glBegin(GL_QUADS);
		glVertex3f(0., 0., 0.);
		glVertex3f(0., fSizeX, 0.);
		glVertex3f(pDock->container.iHeight, fSizeX, 0.);
		glVertex3f(pDock->container.iHeight, 0., 0.);
		glEnd();
		
		_cairo_dock_set_blend_alpha ();
		if (myDocksParam.bUseDefaultColors)
			gldi_style_colors_set_line_color (NULL);
		else
			gldi_color_set_opengl (&myDocksParam.fLineColor);
		glPolygonMode(GL_FRONT, GL_LINE);
		
		if (pDock->container.bDirectionUp)
			glTranslatef (pDock->container.iHeight - pDock->iDecorationsHeight - myDocksParam.iDockLineWidth, 0., 0.);
		
		glBegin(GL_LINES);
		glVertex3f(0., -.5*myDocksParam.iDockLineWidth, 0.);
		glVertex3f(pDock->iDecorationsHeight + myDocksParam.iDockLineWidth, -.5*myDocksParam.iDockLineWidth, 0.);
		glEnd();
		
		glBegin(GL_LINES);
		glVertex3f(0., fSizeX + .5*myDocksParam.iDockLineWidth, 0.);
		glVertex3f(pDock->iDecorationsHeight + myDocksParam.iDockLineWidth, fSizeX + .5*myDocksParam.iDockLineWidth, 0.);
		glEnd();
	}
	glDisable (GL_BLEND);
}

static void _cairo_dock_draw_separator_opengl (Icon *icon, CairoDock *pDock, double fDockMagnitude)
{
	if (myIconsParam.iSeparatorType == CAIRO_DOCK_FLAT_SEPARATOR)
		_draw_flat_separator_opengl (icon, pDock, fDockMagnitude);
	else
		_draw_physical_separator_opengl (icon, pDock, fDockMagnitude);
}

static void cd_render_opengl_default (CairoDock *pDock)
{
	//\_____________ On definit notre rectangle.
	double fLineWidth = (myDocksParam.bUseDefaultColors ? myStyleParam.iLineWidth : myDocksParam.iDockLineWidth);
	double fMargin = myDocksParam.iFrameMargin;
	double fRadius = (myDocksParam.bUseDefaultColors ? myStyleParam.iCornerRadius : myDocksParam.iDockRadius);
	if (pDock->iDecorationsHeight + fLineWidth - 2 * fRadius < 0)
		fRadius = (pDock->iDecorationsHeight + fLineWidth) / 2 - 1;
	double fExtraWidth = 2 * fRadius + fLineWidth;
	double fDockWidth;
	double fFrameHeight = pDock->iDecorationsHeight + fLineWidth;
	
	double fDockOffsetX, fDockOffsetY;  // Offset du coin haut gauche du cadre.
	GList *pFirstDrawnElement = pDock->icons;
	if (pFirstDrawnElement == NULL)
		return ;
	if (cairo_dock_is_extended_dock (pDock))  // mode panel etendu.
	{
		fDockWidth = pDock->container.iWidth - fExtraWidth;
		fDockOffsetX = fLineWidth / 2;
	}
	else
	{
		fDockWidth = cairo_dock_get_current_dock_width_linear (pDock);
		Icon *pFirstIcon = pFirstDrawnElement->data;
		fDockOffsetX = (pFirstIcon != NULL ? pFirstIcon->fX + 0 - fMargin - fRadius : fLineWidth / 2);
		if (fDockOffsetX - fLineWidth/2 < 0)
			fDockOffsetX = fLineWidth / 2;
		if (fDockOffsetX + fDockWidth + (2*fRadius + fLineWidth) > pDock->container.iWidth)
			fDockWidth = pDock->container.iWidth - fDockOffsetX - (2*fRadius + fLineWidth);
		fDockOffsetX += (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2);
	}
	
	fDockOffsetY = pDock->iDecorationsHeight + 1.5 * fLineWidth;
	
	fDockOffsetX = floor (fDockOffsetX);  // round values so that left and right edges are pixel-aligned
	fDockWidth = floor (fDockWidth);
	
	double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
	
	//\_____________ On genere les coordonnees du contour.
	const CairoDockGLPath *pFramePath = cairo_dock_generate_rectangle_path (fDockWidth, fFrameHeight, fRadius, myDocksParam.bRoundedBottomCorner);
	
	//\_____________ On remplit avec le fond.
	glPushMatrix ();
	cairo_dock_set_container_orientation_opengl (CAIRO_CONTAINER (pDock));
	glTranslatef (fDockOffsetX + (fDockWidth+2*fRadius)/2 + .5,  // +.5 so that left and right edges are pixel-aligned
		fDockOffsetY - fFrameHeight/2,
		0.);
	
	/*int i;
	for (i = 0; i < pFramePath->iCurrentPt; i++)
	{
		pFramePath->pVertices[2*i] /= fDockWidth + fExtraWidth;
		pFramePath->pVertices[2*i+1] /= fFrameHeight;
	}
	glScalef (fDockWidth + fExtraWidth, fFrameHeight, 1.);
	cairo_dock_gl_path_set_extent (pFramePath, 1, 1);*/
	cairo_dock_fill_gl_path (pFramePath, pDock->backgroundBuffer.iTexture);
	
	//\_____________ On trace le contour.
	if (fLineWidth != 0)
	{
		glLineWidth (fLineWidth);
		if (myDocksParam.bUseDefaultColors)
			gldi_style_colors_set_line_color (NULL);
		else
			gldi_color_set_opengl (&myDocksParam.fLineColor);
		_cairo_dock_set_blend_alpha ();
		cairo_dock_stroke_gl_path (pFramePath, TRUE);
	}
	glPopMatrix ();
	
	//\_____________ On dessine la ficelle.
	if (myIconsParam.iStringLineWidth > 0)
		cairo_dock_draw_string_opengl (pDock, myIconsParam.iStringLineWidth, FALSE, myIconsParam.bConstantSeparatorSize);
	
	
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
	
	Icon *icon;
	GList *ic = pFirstDrawnElement;
	do
	{
		icon = ic->data;
		
		glPushMatrix ();
		if (myIconsParam.iSeparatorType != CAIRO_DOCK_NORMAL_SEPARATOR && icon->cFileName == NULL && GLDI_OBJECT_IS_SEPARATOR_ICON (icon))
			_cairo_dock_draw_separator_opengl (icon, pDock, fDockMagnitude);
		else
			cairo_dock_render_one_icon_opengl (icon, pDock, fDockMagnitude, TRUE);
		glPopMatrix ();
		
		ic = cairo_dock_get_next_element (ic, pDock->icons);
	} while (ic != pFirstDrawnElement);
	//glDisable (GL_LIGHTING);
}



static void _cd_calculate_construction_parameters_generic (Icon *icon, CairoDock *pDock)
{
	icon->fDrawX = icon->fX + (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2);
	icon->fDrawY = icon->fY;
	icon->fWidthFactor = 1.;
	icon->fHeightFactor = 1.;
	///icon->fDeltaYReflection = 0.;
	icon->fOrientation = 0.;
	icon->fAlpha = 1;
}
static Icon *cd_calculate_icons_default (CairoDock *pDock)
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
	pDefaultRenderer->cReadmeFilePath = g_strdup_printf ("%s/readme-default-view", GLDI_SHARE_DATA_DIR);
	pDefaultRenderer->cPreviewFilePath = g_strdup_printf ("%s/images/preview-default.png", GLDI_SHARE_DATA_DIR);
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
