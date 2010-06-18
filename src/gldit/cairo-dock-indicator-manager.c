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
#include <stdlib.h>

#include <gtk/gtk.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include <gtk/gtkgl.h>

//#include "../config.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-data-renderer.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-load.h"
#include "cairo-dock-icon-loader.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-indicator-manager.h"

extern CairoDock *g_pMainDock;

static CairoDockImageBuffer s_indicatorBuffer;
static CairoDockImageBuffer s_activeIndicatorBuffer;
static CairoDockImageBuffer s_classIndicatorBuffer;

static gboolean cairo_dock_pre_render_indicator_notification (gpointer pUserData, Icon *icon, CairoDock *pDock, cairo_t *pCairoContext);
static gboolean cairo_dock_render_indicator_notification (gpointer pUserData, Icon *icon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext);


  ///////////////
 /// MANAGER ///
///////////////

void cairo_dock_init_indicator_manager (void)
{
	memset (&s_indicatorBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&s_activeIndicatorBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&s_classIndicatorBuffer, 0, sizeof (CairoDockImageBuffer));
	cairo_dock_register_notification (CAIRO_DOCK_PRE_RENDER_ICON,
		(CairoDockNotificationFunc) cairo_dock_pre_render_indicator_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_RENDER_ICON,
		(CairoDockNotificationFunc) cairo_dock_render_indicator_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
}


static inline void cairo_dock_load_task_indicator (const gchar *cIndicatorImagePath, double fMaxScale, double fIndicatorRatio)
{
	cairo_dock_unload_image_buffer (&s_indicatorBuffer);
	
	double fLauncherWidth = (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : 48);
	double fLauncherHeight = (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : 48);
	double fScale = (myIndicators.bIndicatorOnIcon ? fMaxScale : 1.) * fIndicatorRatio;
	
	cairo_dock_load_image_buffer (&s_indicatorBuffer,
		cIndicatorImagePath,
		fLauncherWidth * fScale,
		fLauncherHeight * fScale,
		CAIRO_DOCK_KEEP_RATIO);
}
static inline void cairo_dock_load_active_window_indicator (const gchar *cImagePath, double fMaxScale, double fCornerRadius, double fLineWidth, double *fActiveColor)
{
	cairo_dock_unload_image_buffer (&s_activeIndicatorBuffer);
	
	int iWidth = MAX (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI]);
	int iHeight = MAX (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI], myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI]);
	if (iWidth == 0)
		iWidth = 48;
	if (iHeight == 0)
		iHeight = 48;
	iWidth *= fMaxScale;
	iHeight *= fMaxScale;
	
	if (cImagePath != NULL)
	{
		cairo_dock_load_image_buffer (&s_activeIndicatorBuffer,
			cImagePath,
			iWidth,
			iHeight,
			CAIRO_DOCK_FILL_SPACE);
	}
	else if (fActiveColor[3] > 0)
	{
		cairo_surface_t *pSurface = cairo_dock_create_blank_surface (iWidth, iHeight);
		cairo_t *pCairoContext = cairo_create (pSurface);
		
		fCornerRadius = MIN (fCornerRadius, (iWidth - fLineWidth) / 2);
		double fFrameWidth = iWidth - (2 * fCornerRadius + fLineWidth);
		double fFrameHeight = iHeight - 2 * fLineWidth;
		double fDockOffsetX = fCornerRadius + fLineWidth/2;
		double fDockOffsetY = fLineWidth/2;
		cairo_dock_draw_frame (pCairoContext, fCornerRadius, fLineWidth, fFrameWidth, fFrameHeight, fDockOffsetX, fDockOffsetY, 1, 0., CAIRO_DOCK_HORIZONTAL, TRUE);
		
		cairo_set_source_rgba (pCairoContext, fActiveColor[0], fActiveColor[1], fActiveColor[2], fActiveColor[3]);
		if (fLineWidth > 0)
		{
			cairo_set_line_width (pCairoContext, fLineWidth);
			cairo_stroke (pCairoContext);
		}
		else
		{
			cairo_fill (pCairoContext);
		}
		cairo_destroy (pCairoContext);
		
		cairo_dock_load_image_buffer_from_surface (&s_activeIndicatorBuffer,
			pSurface,
			iWidth,
			iHeight);
	}
}
static inline void cairo_dock_load_class_indicator (const gchar *cIndicatorImagePath, double fMaxScale)
{
	cairo_dock_unload_image_buffer (&s_classIndicatorBuffer);
	
	double fLauncherWidth = (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : 48);
	double fLauncherHeight = (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : 48);
	
	cairo_dock_load_image_buffer (&s_classIndicatorBuffer,
		cIndicatorImagePath,
		fLauncherWidth/3,
		fLauncherHeight/3,
		CAIRO_DOCK_KEEP_RATIO);
}
void cairo_dock_load_indicator_textures (void)
{
	double fMaxScale = cairo_dock_get_max_scale (g_pMainDock);
	
	cairo_dock_load_task_indicator (myTaskBar.bShowAppli && (myTaskBar.bMixLauncherAppli || myIndicators.bDrawIndicatorOnAppli) ? myIndicators.cIndicatorImagePath : NULL, fMaxScale, myIndicators.fIndicatorRatio);
	
	cairo_dock_load_active_window_indicator (myIndicators.cActiveIndicatorImagePath, fMaxScale, myIndicators.iActiveCornerRadius, myIndicators.iActiveLineWidth, myIndicators.fActiveColor);
	
	cairo_dock_load_class_indicator (myTaskBar.bShowAppli && myTaskBar.bGroupAppliByClass ? myIndicators.cClassIndicatorImagePath : NULL, fMaxScale);
}


void cairo_dock_unload_indicator_textures (void)
{
	cairo_dock_unload_image_buffer (&s_indicatorBuffer);
	cairo_dock_unload_image_buffer (&s_activeIndicatorBuffer);
	cairo_dock_unload_image_buffer (&s_classIndicatorBuffer);
}


static void _set_indicator (Icon *pIcon, CairoContainer *pContainer, gpointer data)
{
	pIcon->bHasIndicator = GPOINTER_TO_INT (data);
}
void cairo_dock_reload_indicators (CairoConfigIndicators *pPrevIndicators, CairoConfigIndicators *pIndicators)
{
	double fMaxScale = cairo_dock_get_max_scale (g_pMainDock);
	
	if (cairo_dock_strings_differ (pPrevIndicators->cIndicatorImagePath, pIndicators->cIndicatorImagePath) ||
		pPrevIndicators->bIndicatorOnIcon != pIndicators->bIndicatorOnIcon ||
		pPrevIndicators->fIndicatorRatio != pIndicators->fIndicatorRatio)
	{
		cairo_dock_load_task_indicator (myTaskBar.bShowAppli && (myTaskBar.bMixLauncherAppli || myIndicators.bDrawIndicatorOnAppli) ? pIndicators->cIndicatorImagePath : NULL, fMaxScale, pIndicators->fIndicatorRatio);
	}
	
	if (cairo_dock_strings_differ (pPrevIndicators->cActiveIndicatorImagePath, pIndicators->cActiveIndicatorImagePath) ||
		pPrevIndicators->iActiveCornerRadius != pIndicators->iActiveCornerRadius ||
		pPrevIndicators->iActiveLineWidth != pIndicators->iActiveLineWidth ||
		cairo_dock_colors_differ (pPrevIndicators->fActiveColor, pIndicators->fActiveColor))
	{
		cairo_dock_load_active_window_indicator (pIndicators->cActiveIndicatorImagePath,
			fMaxScale,
			pIndicators->iActiveCornerRadius,
			pIndicators->iActiveLineWidth,
			pIndicators->fActiveColor);
	}
	
	if (cairo_dock_strings_differ (pPrevIndicators->cClassIndicatorImagePath, pIndicators->cClassIndicatorImagePath) ||
		pPrevIndicators->bUseClassIndic != pIndicators->bUseClassIndic)
	{
		cairo_dock_load_class_indicator (myTaskBar.bShowAppli && myTaskBar.bGroupAppliByClass ? pIndicators->cClassIndicatorImagePath : NULL, fMaxScale);
		
		if (pPrevIndicators->bUseClassIndic != pIndicators->bUseClassIndic && g_pMainDock)  // on recharge les icones pointant sur une classe (qui sont dans le main dock).
		{
			Icon *icon;
			GList *ic;
			for (ic = g_pMainDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if (CAIRO_DOCK_IS_FAKE_LAUNCHER (icon))
				{
					cairo_dock_load_icon_image (icon, CAIRO_CONTAINER (g_pMainDock));
					if (!pIndicators->bUseClassIndic)
						cairo_dock_draw_subdock_content_on_icon (icon, g_pMainDock);
				}
			}
		}
	}
	
	if (cairo_dock_application_manager_is_running () &&
		pPrevIndicators->bDrawIndicatorOnAppli != pIndicators->bDrawIndicatorOnAppli)
	{
		cairo_dock_foreach_applis ((CairoDockForeachIconFunc) _set_indicator, FALSE, GINT_TO_POINTER (pIndicators->bDrawIndicatorOnAppli));
	}
	
	cairo_dock_redraw_root_docks (FALSE);  // tous les docks (main dock et les autres qui peuvent contenir des applets avec un indicateur).
}


  /////////////////
 /// RENDERING ///
/////////////////

static inline double _compute_delta_y (Icon *icon, double py, gboolean bOnIcon, double fRatio, gboolean bUseReflect)
{
	double dy;
	if (bOnIcon)  // decalage vers le haut et zoom avec l'icone.
		dy = py * icon->fHeight * icon->fScale;
	else  // decalage vers le bas sans zoom.
		dy = - py * ((bUseReflect ? myIcons.fReflectSize * fRatio : 0.) + myBackground.iFrameMargin + .5*myBackground.iDockLineWidth);
	return dy;
}

static void _cairo_dock_draw_appli_indicator_opengl (Icon *icon, CairoDock *pDock)
{
	gboolean bIsHorizontal = pDock->container.bIsHorizontal;
	gboolean bDirectionUp = pDock->container.bDirectionUp;
	double fRatio = pDock->container.fRatio;
	if (! myIndicators.bRotateWithDock)
		bDirectionUp = bIsHorizontal = TRUE;
	
	//\__________________ On calcule l'offset et le zoom.
	double w = s_indicatorBuffer.iWidth;
	double h = s_indicatorBuffer.iHeight;
	double fMaxScale = cairo_dock_get_max_scale (pDock);
	double z = (myIndicators.bIndicatorOnIcon ? icon->fScale / fMaxScale : 1.) * fRatio;  // on divise par fMaxScale car l'indicateur est charge a la taille max des icones.
	double fY = _compute_delta_y (icon, myIndicators.fIndicatorDeltaY, myIndicators.bIndicatorOnIcon, fRatio, pDock->container.bUseReflect);
	fY += - icon->fHeight * icon->fScale/2 + h*z/2;  // a 0, le bas de l'indicateur correspond au bas de l'icone.
	
	//\__________________ On place l'indicateur.
	glPushMatrix ();
	if (bIsHorizontal)
	{
		if (! bDirectionUp)
			fY = - fY;
		glTranslatef (0., fY, 0.);
	}
	else
	{
		if (bDirectionUp)
			fY = - fY;
		glTranslatef (fY, 0., 0.);
		glRotatef (90, 0., 0., 1.);
	}
	glScalef (w * z, (bDirectionUp ? 1:-1) * h * z, 1.);
	/**double z = 1 + myIcons.fAmplitude;
	double w = s_indicatorBuffer.iWidth;
	double h = s_indicatorBuffer.iHeight;
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
	}*/
	
	//\__________________ On dessine l'indicateur.
	cairo_dock_draw_texture_with_alpha (s_indicatorBuffer.iTexture, 1., 1., 1.);
	glPopMatrix ();
}
static void _cairo_dock_draw_active_window_indicator_opengl (Icon *icon, CairoDock *pDock, double fRatio)
{
	if (s_activeIndicatorBuffer.iTexture == 0)
		return ;
	glPushMatrix ();
	cairo_dock_set_icon_scale (icon, CAIRO_CONTAINER (pDock), 1.);
	
	_cairo_dock_enable_texture ();
	
	_cairo_dock_set_blend_pbuffer ();  // rend mieux que les 2 autres.
	
	_cairo_dock_apply_texture_at_size_with_alpha (s_activeIndicatorBuffer.iTexture, 1., 1., 1.);
	
	_cairo_dock_disable_texture ();
	glPopMatrix ();
	
}
static void _cairo_dock_draw_class_indicator_opengl (Icon *icon, gboolean bIsHorizontal, double fRatio, gboolean bDirectionUp)
{
	glPushMatrix ();
	if (myIndicators.bZoomClassIndicator)
		fRatio *= icon->fScale;
	double w = s_classIndicatorBuffer.iWidth;
	double h = s_classIndicatorBuffer.iHeight;
	
	if (! bIsHorizontal)
		glRotatef (90., 0., 0., 1.);
	if (! bDirectionUp)
		glScalef (1., -1., 1.);
	glTranslatef (icon->fWidth * icon->fScale/2 - w * fRatio/2,
		icon->fHeight * icon->fScale/2 - h * fRatio/2,
		0.);
	
	cairo_dock_draw_texture_with_alpha (s_classIndicatorBuffer.iTexture,
		w * fRatio,
		h * fRatio,
		1.);
	glPopMatrix ();
}

static void _cairo_dock_draw_appli_indicator (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext)
{
	gboolean bIsHorizontal = pDock->container.bIsHorizontal;
	gboolean bDirectionUp = pDock->container.bDirectionUp;
	double fRatio = pDock->container.fRatio;
	if (! myIndicators.bRotateWithDock)
		bDirectionUp = bIsHorizontal = TRUE;
	
	//\__________________ On calcule l'offset et le zoom.
	double w = s_indicatorBuffer.iWidth;
	double h = s_indicatorBuffer.iHeight;
	double fMaxScale = cairo_dock_get_max_scale (pDock);
	double z = (myIndicators.bIndicatorOnIcon ? icon->fScale / fMaxScale : 1.) * fRatio;  // on divise par fMaxScale car l'indicateur est charge a la taille max des icones.
	double fY = - _compute_delta_y (icon, myIndicators.fIndicatorDeltaY, myIndicators.bIndicatorOnIcon, fRatio, pDock->container.bUseReflect);  // a 0, le bas de l'indicateur correspond au bas de l'icone.
	
	//\__________________ On place l'indicateur.
	cairo_save (pCairoContext);
	if (bIsHorizontal)
	{
		cairo_translate (pCairoContext,
			icon->fWidth * icon->fScale / 2 - w * z/2,
			(bDirectionUp ?
				icon->fHeight * icon->fScale - h * z + fY :
				- fY));
		cairo_scale (pCairoContext,
			z,
			z);
	}
	else
	{
		cairo_translate (pCairoContext,
			(bDirectionUp ?
				icon->fHeight * icon->fScale - h * z + fY :
				- fY),
			icon->fWidth * icon->fScale / 2 - (w * z/2));
		cairo_scale (pCairoContext,
			z,
			z);
	}
	/**double dy = myIndicators.iIndicatorDeltaY / z;
	if (myIndicators.bLinkIndicatorWithIcon)
	{
		w /= z;
		h /= z;
		if (bIsHorizontal)
		{
			cairo_translate (pCairoContext,
				(icon->fWidth - w * fRatio) * icon->fWidthFactor * icon->fScale / 2,
				(bDirectionUp ?
					(icon->fHeight - (h - dy) * fRatio) * icon->fScale :
					- dy * icon->fScale * fRatio));
			cairo_scale (pCairoContext,
				fRatio * icon->fWidthFactor * icon->fScale / z,
				fRatio * icon->fHeightFactor * icon->fScale / z * (bDirectionUp ? 1 : -1));
		}
		else
		{
			cairo_translate (pCairoContext,
				(bDirectionUp ?
					(icon->fHeight - (h - dy) * fRatio) * icon->fScale :
					- dy * icon->fScale * fRatio),
					(icon->fWidth - (bDirectionUp ? 0 : w * fRatio)) * icon->fWidthFactor * icon->fScale / 2);
			cairo_scale (pCairoContext,
				fRatio * icon->fHeightFactor * icon->fScale / z * (bDirectionUp ? 1 : -1),
				fRatio * icon->fWidthFactor * icon->fScale / z);
		}
	}
	else
	{
		if (bIsHorizontal)
		{
			cairo_translate (pCairoContext,
				(icon->fWidth * icon->fScale - w * fRatio) / 2,
				(bDirectionUp ? 
					(icon->fHeight * icon->fScale - (h - dy) * fRatio) :
					- dy * fRatio));
			cairo_scale (pCairoContext,
				fRatio,
				fRatio);
		}
		else
		{
			cairo_translate (pCairoContext,
				(bDirectionUp ? 
					icon->fHeight * icon->fScale - (h - dy) * fRatio : 
					- dy * 1 * fRatio),
				(icon->fWidth * icon->fScale - w * fRatio) / 2);
			cairo_scale (pCairoContext,
				fRatio,
				fRatio);
		}
	}*/
	
	cairo_dock_draw_surface (pCairoContext, s_indicatorBuffer.pSurface, w, h, bDirectionUp, bIsHorizontal, 1.);
	cairo_restore (pCairoContext);
}
static void _cairo_dock_draw_active_window_indicator (cairo_t *pCairoContext, Icon *icon)
{
	cairo_save (pCairoContext);
	cairo_scale (pCairoContext,
		icon->fWidth * icon->fWidthFactor / s_activeIndicatorBuffer.iWidth * icon->fScale,
		icon->fHeight * icon->fHeightFactor / s_activeIndicatorBuffer.iHeight * icon->fScale);
	cairo_set_source_surface (pCairoContext, s_activeIndicatorBuffer.pSurface, 0., 0.);
	cairo_paint (pCairoContext);
	cairo_restore (pCairoContext);
}
static void _cairo_dock_draw_class_indicator (cairo_t *pCairoContext, Icon *icon, gboolean bIsHorizontal, double fRatio, gboolean bDirectionUp)
{
	cairo_save (pCairoContext);
	double w = s_classIndicatorBuffer.iWidth;
	double h = s_classIndicatorBuffer.iHeight;
	if (bIsHorizontal)
	{
		if (bDirectionUp)
			cairo_translate (pCairoContext,
				(icon->fWidth - w * fRatio) * icon->fScale,
				0.);
		else
			cairo_translate (pCairoContext,
				(icon->fWidth - w * fRatio) * icon->fScale,
				(icon->fHeight - h * fRatio) * icon->fScale);
	}
	else
	{
		if (bDirectionUp)
			cairo_translate (pCairoContext,
				0.,
				(icon->fWidth - w * fRatio) * icon->fScale);
		else
			cairo_translate (pCairoContext,
				(icon->fHeight - h * fRatio) * icon->fScale,
				(icon->fWidth - w * fRatio) * icon->fScale);
	}
	cairo_scale (pCairoContext, fRatio * icon->fScale, fRatio * icon->fScale);
	cairo_dock_draw_surface (pCairoContext, s_classIndicatorBuffer.pSurface, w, h, bDirectionUp, bIsHorizontal, 1.);
	cairo_restore (pCairoContext);
}

static inline gboolean _active_indicator_is_visible (Icon *icon)
{
	gboolean bIsActive = FALSE;
	if (icon->Xid)
	{
		Window xActiveId = cairo_dock_get_current_active_window ();
		if (xActiveId != 0)
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
	}
	return bIsActive;
}

static gboolean cairo_dock_pre_render_indicator_notification (gpointer pUserData, Icon *icon, CairoDock *pDock, cairo_t *pCairoContext)
{
	gboolean bIsActive = (myIndicators.bActiveIndicatorAbove ? FALSE : _active_indicator_is_visible (icon));
	
	if (pCairoContext != NULL)
	{
		if (icon->bHasIndicator && ! myIndicators.bIndicatorAbove && s_indicatorBuffer.pSurface != NULL)
		{
			_cairo_dock_draw_appli_indicator (icon, pDock, pCairoContext);
		}
		
		if (bIsActive && s_activeIndicatorBuffer.pSurface != NULL)
		{
			_cairo_dock_draw_active_window_indicator (pCairoContext, icon);
		}
	}
	else
	{
		if (icon->bHasIndicator && ! myIndicators.bIndicatorAbove)
		{
			//glPushMatrix ();
			_cairo_dock_draw_appli_indicator_opengl (icon, pDock);
			//glPopMatrix ();
		}
		
		if (bIsActive)
		{
			//glPushMatrix ();
			_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, pDock->container.fRatio);
			//glPopMatrix ();
		}
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static gboolean cairo_dock_render_indicator_notification (gpointer pUserData, Icon *icon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext)
{
	gboolean bIsActive = (myIndicators.bActiveIndicatorAbove ? _active_indicator_is_visible (icon) : FALSE);
	
	if (pCairoContext != NULL)
	{
		if (bIsActive)
		{
			_cairo_dock_draw_active_window_indicator (pCairoContext, icon);
		}
		if (icon->bHasIndicator && myIndicators.bIndicatorAbove && s_indicatorBuffer.pSurface != NULL)
		{
			_cairo_dock_draw_appli_indicator (icon, pDock, pCairoContext);
		}
		if (icon->pSubDock != NULL && icon->cClass != NULL && s_classIndicatorBuffer.pSurface != NULL && icon->Xid == 0)  // le dernier test est de la paranoia.
		{
			_cairo_dock_draw_class_indicator (pCairoContext, icon, pDock->container.bIsHorizontal, pDock->container.fRatio, pDock->container.bDirectionUp);
		}
	}
	else
	{
		if (icon->bHasIndicator && myIndicators.bIndicatorAbove)
		{
			//glPushMatrix ();
			//glTranslatef (0., 0., icon->fHeight * (1+myIcons.fAmplitude) -1);  // avant-plan
			_cairo_dock_draw_appli_indicator_opengl (icon, pDock);
			//glPopMatrix ();
		}
		if (bIsActive)
		{
			//glPushMatrix ();
			//glTranslatef (0., 0., icon->fHeight * (1+myIcons.fAmplitude) -1);  // avant-plan
			_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, pDock->container.fRatio);
			//glPopMatrix ();
		}
		if (icon->pSubDock != NULL && icon->cClass != NULL && s_classIndicatorBuffer.iTexture != 0 && icon->Xid == 0)  // le dernier test est de la paranoia.
		{
			//glPushMatrix ();
			//glTranslatef (0., 0., icon->fHeight * (1+myIcons.fAmplitude) -1);  // avant-plan
			_cairo_dock_draw_class_indicator_opengl (icon, pDock->container.bIsHorizontal, pDock->container.fRatio, pDock->container.bDirectionUp);
			//glPopMatrix ();
		}
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}
