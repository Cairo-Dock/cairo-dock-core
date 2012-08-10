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

#include "gldi-config.h"  // GLDI_SHARE_DATA_DIR
#include "cairo-dock-draw.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"  // cairo_dock_redraw_root_docks
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-container.h"
#include "cairo-dock-applications-manager.h"  // cairo_dock_foreach_applis
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-data-renderer.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.bShowAppli
#define _MANAGER_DEF_
#include "cairo-dock-indicator-manager.h"

// public (manager, config, data)
CairoIndicatorsParam myIndicatorsParam;
CairoIndicatorsManager myIndicatorsMgr;

// dependancies
extern CairoDock *g_pMainDock;

// private
static CairoDockImageBuffer s_indicatorBuffer;
static CairoDockImageBuffer s_activeIndicatorBuffer;
static CairoDockImageBuffer s_classIndicatorBuffer;

static gboolean cairo_dock_pre_render_indicator_notification (gpointer pUserData, Icon *icon, CairoDock *pDock, cairo_t *pCairoContext);
static gboolean cairo_dock_render_indicator_notification (gpointer pUserData, Icon *icon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext);


  /////////////////
 /// RENDERING ///
/////////////////

static inline double _compute_delta_y (Icon *icon, double py, gboolean bOnIcon, gboolean bUseReflect)
{
	double dy;
	if (bOnIcon)  // decalage vers le haut et zoom avec l'icone.
		dy = py * icon->fHeight * icon->fScale + icon->fDeltaYReflection / 2;
	else  // decalage vers le bas sans zoom.
		dy = - py * ((bUseReflect ? /**myIconsParam.fReflectSize * fRatio*/icon->fHeight * myIconsParam.fReflectHeightRatio : 0.) + myDocksParam.iFrameMargin + .5*myDocksParam.iDockLineWidth);
	return dy;
}

static void _cairo_dock_draw_appli_indicator_opengl (Icon *icon, CairoDock *pDock)
{
	gboolean bIsHorizontal = pDock->container.bIsHorizontal;
	gboolean bDirectionUp = pDock->container.bDirectionUp;
	double fRatio = pDock->container.fRatio;
	if (! myIndicatorsParam.bRotateWithDock)
		bDirectionUp = bIsHorizontal = TRUE;
	
	//\__________________ On calcule l'offset et le zoom.
	double w = s_indicatorBuffer.iWidth;
	double h = s_indicatorBuffer.iHeight;
	///double fMaxScale = cairo_dock_get_max_scale (pDock);
	///double z = (myIndicatorsParam.bIndicatorOnIcon ? icon->fScale / fMaxScale : 1.) * fRatio;  // on divise par fMaxScale car l'indicateur est charge a la taille max des icones.
	double z = icon->fWidth / w * (myIndicatorsParam.bIndicatorOnIcon ? icon->fScale : 1.);
	double fY = _compute_delta_y (icon, myIndicatorsParam.fIndicatorDeltaY, myIndicatorsParam.bIndicatorOnIcon, pDock->container.bUseReflect);
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
	if (myIndicatorsParam.bZoomClassIndicator)
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
	if (! myIndicatorsParam.bRotateWithDock)
		bDirectionUp = bIsHorizontal = TRUE;
	
	//\__________________ On calcule l'offset et le zoom.
	double w = s_indicatorBuffer.iWidth;
	double h = s_indicatorBuffer.iHeight;
	///double fMaxScale = cairo_dock_get_max_scale (pDock);
	///double z = (myIndicatorsParam.bIndicatorOnIcon ? icon->fScale / fMaxScale : 1.) * fRatio;  // on divise par fMaxScale car l'indicateur est charge a la taille max des icones.
	double z = icon->fWidth / w * (myIndicatorsParam.bIndicatorOnIcon ? icon->fScale : 1.);
	double fY = - _compute_delta_y (icon, myIndicatorsParam.fIndicatorDeltaY, myIndicatorsParam.bIndicatorOnIcon, pDock->container.bUseReflect);  // a 0, le bas de l'indicateur correspond au bas de l'icone.
	
	//\__________________ On place l'indicateur.
	cairo_save (pCairoContext);
	if (bIsHorizontal)
	{
		cairo_translate (pCairoContext,
			icon->fWidth * icon->fWidthFactor * icon->fScale / 2 - w * z/2,
			(bDirectionUp ?
				icon->fHeight * icon->fHeightFactor * icon->fScale - h * z + fY :
				- fY));
		cairo_scale (pCairoContext,
			z,
			z);
	}
	else
	{
		cairo_translate (pCairoContext,
			(bDirectionUp ?
				icon->fHeight * icon->fHeightFactor * icon->fScale - h * z + fY :
				- fY),
			icon->fWidth * icon->fWidthFactor * icon->fScale / 2 - (w * z/2));
		cairo_scale (pCairoContext,
			z,
			z);
	}
	
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
	gboolean bIsActive = (myIndicatorsParam.bActiveIndicatorAbove ? FALSE : _active_indicator_is_visible (icon));
	
	if (pCairoContext != NULL)
	{
		if (icon->bHasIndicator && ! myIndicatorsParam.bIndicatorAbove && s_indicatorBuffer.pSurface != NULL)
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
		if (icon->bHasIndicator && ! myIndicatorsParam.bIndicatorAbove)
		{
			_cairo_dock_draw_appli_indicator_opengl (icon, pDock);
		}
		
		if (bIsActive)
		{
			_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, pDock->container.fRatio);
		}
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static gboolean cairo_dock_render_indicator_notification (gpointer pUserData, Icon *icon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext)
{
	gboolean bIsActive = (myIndicatorsParam.bActiveIndicatorAbove ? _active_indicator_is_visible (icon) : FALSE);
	
	if (pCairoContext != NULL)
	{
		if (bIsActive)
		{
			_cairo_dock_draw_active_window_indicator (pCairoContext, icon);
		}
		if (icon->bHasIndicator && myIndicatorsParam.bIndicatorAbove && s_indicatorBuffer.pSurface != NULL)
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
		if (icon->bHasIndicator && myIndicatorsParam.bIndicatorAbove)
		{
			glPushMatrix ();
			glLoadIdentity();
			cairo_dock_translate_on_icon_opengl (icon, CAIRO_CONTAINER (pDock), 1.);
			_cairo_dock_draw_appli_indicator_opengl (icon, pDock);
			glPopMatrix ();
		}
		if (bIsActive)
		{
			_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, pDock->container.fRatio);
		}
		if (icon->pSubDock != NULL && icon->cClass != NULL && s_classIndicatorBuffer.iTexture != 0 && icon->Xid == 0)  // le dernier test est de la paranoia.
		{
			_cairo_dock_draw_class_indicator_opengl (icon, pDock->container.bIsHorizontal, pDock->container.fRatio, pDock->container.bDirectionUp);
		}
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoIndicatorsParam *pIndicators)
{
	gboolean bFlushConfFileNeeded = FALSE;
	gchar *cIndicatorImageName;
	
	//\__________________ On recupere l'indicateur d'appli lancee.
	cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "indicator image", &bFlushConfFileNeeded, NULL, "Icons", NULL);
	if (cIndicatorImageName != NULL)
	{
		pIndicators->cIndicatorImagePath = cairo_dock_search_image_s_path (cIndicatorImageName);
		g_free (cIndicatorImageName);
	}
	else
		pIndicators->cIndicatorImagePath = g_strdup (GLDI_SHARE_DATA_DIR"/icons/default-indicator.png");
	
	pIndicators->bIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "indicator above", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	
	pIndicators->fIndicatorRatio = cairo_dock_get_double_key_value (pKeyFile, "Indicators", "indicator ratio", &bFlushConfFileNeeded, 1., "Icons", NULL);
	
	pIndicators->bIndicatorOnIcon = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "indicator on icon", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	pIndicators->fIndicatorDeltaY = cairo_dock_get_double_key_value (pKeyFile, "Indicators", "indicator offset", &bFlushConfFileNeeded, 11, NULL, NULL);
	if (pIndicators->fIndicatorDeltaY > 10)  // nouvelle option.
	{
		double iIndicatorDeltaY = g_key_file_get_integer (pKeyFile, "Indicators", "indicator deltaY", NULL);
		double z = g_key_file_get_double (pKeyFile, "Icons", "zoom max", NULL);
		if (z != 0)
			iIndicatorDeltaY /= z;
		pIndicators->bIndicatorOnIcon = g_key_file_get_boolean (pKeyFile, "Indicators", "link indicator", NULL);
		if (iIndicatorDeltaY > 6)  // en general cela signifie que l'indicateur est sur le dock.
			pIndicators->bIndicatorOnIcon = FALSE;
		else if (iIndicatorDeltaY < 3)  // en general cela signifie que l'indicateur est sur l'icone.
			pIndicators->bIndicatorOnIcon = TRUE;  // sinon on garde le comportement d'avant.
		
		int w, hi=0;  // on va se baser sur la taille des lanceurs.
		cairo_dock_get_size_key_value_helper (pKeyFile, "Icons", "launcher ", bFlushConfFileNeeded, w, hi);  
		if (hi < 1)
			hi = 48;
		if (pIndicators->bIndicatorOnIcon)  // decalage vers le haut et zoom avec l'icone.
		{
			// on la recupere comme ca car on n'est pas forcement encore passe dans le groupe "Icons".
			pIndicators->fIndicatorDeltaY = (double)iIndicatorDeltaY / hi;
			//g_print ("icones : %d, deltaY : %d\n", hi, (int)iIndicatorDeltaY);
		}
		else  // decalage vers le bas sans zoom.
		{
			double hr, hb, l;
			hr = hi * g_key_file_get_double (pKeyFile, "Icons", "field depth", NULL);
			hb = g_key_file_get_integer (pKeyFile, "Background", "frame margin", NULL);
			l = g_key_file_get_integer (pKeyFile, "Background", "line width", NULL);
			pIndicators->fIndicatorDeltaY = (double)iIndicatorDeltaY / (hr + hb + l/2);
		}
		//g_print ("recuperation de l'indicateur : %.3f, %d\n", pIndicators->fIndicatorDeltaY, pIndicators->bIndicatorOnIcon);
		g_key_file_set_double (pKeyFile, "Indicators", "indicator offset", pIndicators->fIndicatorDeltaY);
		g_key_file_set_boolean (pKeyFile, "Indicators", "indicator on icon", pIndicators->bIndicatorOnIcon);
	}
	
	pIndicators->bRotateWithDock = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "rotate indicator", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	pIndicators->bDrawIndicatorOnAppli = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "indic on appli", &bFlushConfFileNeeded, FALSE, "TaskBar", NULL);
	
	//\__________________ On recupere l'indicateur de fenetre active.
	int iIndicType = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active indic type", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 pour pouvoir intercepter le cas ou la cle n'existe pas.
	
	cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "active indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
	if (iIndicType == -1)  // nouvelle cle.
	{
		iIndicType = (cIndicatorImageName != NULL ? 0 : 1);
		g_key_file_set_integer (pKeyFile, "Indicators", "active indic type", iIndicType);
	}
	else
	{
		if (iIndicType != 0)  // pas d'image.
		{
			g_free (cIndicatorImageName);
			cIndicatorImageName = NULL;
		}
	}
	
	if (cIndicatorImageName != NULL)
	{
		pIndicators->cActiveIndicatorImagePath = cairo_dock_search_image_s_path (cIndicatorImageName);
		g_free (cIndicatorImageName);
	}
	else
		pIndicators->cActiveIndicatorImagePath = NULL;
	
	if (iIndicType == 1)  // cadre
	{
		double couleur_active[4] = {0., 0.4, 0.8, 0.5};
		cairo_dock_get_double_list_key_value (pKeyFile, "Indicators", "active color", &bFlushConfFileNeeded, pIndicators->fActiveColor, 4, couleur_active, "Icons", NULL);
		pIndicators->iActiveLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active line width", &bFlushConfFileNeeded, 3, "Icons", NULL);
		pIndicators->iActiveCornerRadius = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active corner radius", &bFlushConfFileNeeded, 6, "Icons", NULL);
	}  // donc ici si on choisit le mode "image" sans en definir une, le alpha de la couleur reste a 0 => aucun indicateur
	
	pIndicators->bActiveIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "active frame position", &bFlushConfFileNeeded, TRUE, "Icons", NULL);
	
	//\__________________ On recupere l'indicateur de classe groupee.
	pIndicators->bUseClassIndic = (cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "use class indic", &bFlushConfFileNeeded, 0, NULL, NULL) == 0);
	if (pIndicators->bUseClassIndic)
	{
		cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "class indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
		if (cIndicatorImageName != NULL)
		{
			pIndicators->cClassIndicatorImagePath = cairo_dock_search_image_s_path (cIndicatorImageName);
			g_free (cIndicatorImageName);
		}
		else
		{
			pIndicators->cClassIndicatorImagePath = g_strdup (GLDI_SHARE_DATA_DIR"/icons/default-class-indicator.svg");
		}
		pIndicators->bZoomClassIndicator = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "zoom class", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	}
	
	//\__________________ Progress bar.
	double start_color[4] = {.53, .53, .53, .85};  // grey
	cairo_dock_get_double_list_key_value (pKeyFile, "Indicators", "bar_color_start", &bFlushConfFileNeeded, pIndicators->fBarColorStart, 4, start_color, NULL, NULL);
	double stop_color[4] = {.87, .87, .87, .85};  // grey (lighter)
	cairo_dock_get_double_list_key_value (pKeyFile, "Indicators", "bar_color_stop", &bFlushConfFileNeeded, pIndicators->fBarColorStop, 4, stop_color, NULL, NULL);
	pIndicators->bBarHasOutline = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "bar_outline", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	double outline_color[4] = {1, 1, 1, .85};  // white
	cairo_dock_get_double_list_key_value (pKeyFile, "Indicators", "bar_color_outline", &bFlushConfFileNeeded, pIndicators->fBarColorOutline, 4, outline_color, NULL, NULL);
	pIndicators->iBarThickness = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "bar_thickness", &bFlushConfFileNeeded, 4, NULL, NULL);
	
	return bFlushConfFileNeeded;
}


  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (CairoIndicatorsParam *pIndicators)
{
	g_free (pIndicators->cIndicatorImagePath);
	g_free (pIndicators->cActiveIndicatorImagePath);
	g_free (pIndicators->cClassIndicatorImagePath);
}


  ////////////
 /// LOAD ///
////////////

static inline void _load_task_indicator (const gchar *cIndicatorImagePath, double fMaxScale, double fIndicatorRatio)
{
	cairo_dock_unload_image_buffer (&s_indicatorBuffer);
	
	double fLauncherWidth = myIconsParam.iIconWidth;
	double fLauncherHeight = myIconsParam.iIconHeight;
	double fScale = (myIndicatorsParam.bIndicatorOnIcon ? fMaxScale : 1.) * fIndicatorRatio;
	
	cairo_dock_load_image_buffer (&s_indicatorBuffer,
		cIndicatorImagePath,
		fLauncherWidth * fScale,
		fLauncherHeight * fScale,
		CAIRO_DOCK_KEEP_RATIO);
}
static inline void _load_active_window_indicator (const gchar *cImagePath, double fMaxScale, double fCornerRadius, double fLineWidth, double *fActiveColor)
{
	cairo_dock_unload_image_buffer (&s_activeIndicatorBuffer);
	
	int iWidth = myIconsParam.iIconWidth;
	int iHeight = myIconsParam.iIconHeight;
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
static inline void _load_class_indicator (const gchar *cIndicatorImagePath, double fMaxScale)
{
	cairo_dock_unload_image_buffer (&s_classIndicatorBuffer);
	
	double fLauncherWidth = myIconsParam.iIconWidth;
	double fLauncherHeight = myIconsParam.iIconHeight;
	
	cairo_dock_load_image_buffer (&s_classIndicatorBuffer,
		cIndicatorImagePath,
		fLauncherWidth/3,
		fLauncherHeight/3,
		CAIRO_DOCK_KEEP_RATIO);
}
static void load (void)
{
	double fMaxScale = cairo_dock_get_max_scale (g_pMainDock);
	
	_load_task_indicator (myTaskbarParam.bShowAppli && (myTaskbarParam.bMixLauncherAppli || myIndicatorsParam.bDrawIndicatorOnAppli) ? myIndicatorsParam.cIndicatorImagePath : NULL, fMaxScale, myIndicatorsParam.fIndicatorRatio);
	
	_load_active_window_indicator (myIndicatorsParam.cActiveIndicatorImagePath, fMaxScale, myIndicatorsParam.iActiveCornerRadius, myIndicatorsParam.iActiveLineWidth, myIndicatorsParam.fActiveColor);
	
	_load_class_indicator (myTaskbarParam.bShowAppli && myTaskbarParam.bGroupAppliByClass ? myIndicatorsParam.cClassIndicatorImagePath : NULL, fMaxScale);
}


  //////////////
 /// RELOAD ///
//////////////

static void _set_indicator (Icon *pIcon, CairoContainer *pContainer, gpointer data)
{
	pIcon->bHasIndicator = GPOINTER_TO_INT (data);
}
static void _reload_progress_bar (Icon *pIcon, CairoContainer *pContainer, gpointer data)
{
	if (cairo_dock_get_icon_data_renderer (pIcon) != NULL)
	{
		cairo_dock_reload_data_renderer_on_icon (pIcon, pContainer);
	}
}
static void reload (CairoIndicatorsParam *pPrevIndicators, CairoIndicatorsParam *pIndicators)
{
	double fMaxScale = cairo_dock_get_max_scale (g_pMainDock);
	
	if (cairo_dock_strings_differ (pPrevIndicators->cIndicatorImagePath, pIndicators->cIndicatorImagePath) ||
		pPrevIndicators->bIndicatorOnIcon != pIndicators->bIndicatorOnIcon ||
		pPrevIndicators->fIndicatorRatio != pIndicators->fIndicatorRatio)
	{
		_load_task_indicator (myTaskbarParam.bShowAppli && (myTaskbarParam.bMixLauncherAppli || myIndicatorsParam.bDrawIndicatorOnAppli) ? pIndicators->cIndicatorImagePath : NULL, fMaxScale, pIndicators->fIndicatorRatio);
	}
	
	if (cairo_dock_strings_differ (pPrevIndicators->cActiveIndicatorImagePath, pIndicators->cActiveIndicatorImagePath) ||
		pPrevIndicators->iActiveCornerRadius != pIndicators->iActiveCornerRadius ||
		pPrevIndicators->iActiveLineWidth != pIndicators->iActiveLineWidth ||
		cairo_dock_colors_differ (pPrevIndicators->fActiveColor, pIndicators->fActiveColor))
	{
		_load_active_window_indicator (pIndicators->cActiveIndicatorImagePath,
			fMaxScale,
			pIndicators->iActiveCornerRadius,
			pIndicators->iActiveLineWidth,
			pIndicators->fActiveColor);
	}
	
	if (cairo_dock_strings_differ (pPrevIndicators->cClassIndicatorImagePath, pIndicators->cClassIndicatorImagePath) ||
		pPrevIndicators->bUseClassIndic != pIndicators->bUseClassIndic)
	{
		_load_class_indicator (myTaskbarParam.bShowAppli && myTaskbarParam.bGroupAppliByClass ? pIndicators->cClassIndicatorImagePath : NULL, fMaxScale);
		
		if (pPrevIndicators->bUseClassIndic != pIndicators->bUseClassIndic && g_pMainDock)  // on recharge les icones pointant sur une classe (qui sont dans le main dock).
		{
			/// il faudrait le faire pour les lanceurs de tous les docks ...
			Icon *icon;
			GList *ic;
			for (ic = g_pMainDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if (CAIRO_DOCK_IS_MULTI_APPLI (icon))
				{
					cairo_dock_load_icon_image (icon, CAIRO_CONTAINER (g_pMainDock));
					if (!pIndicators->bUseClassIndic)
						cairo_dock_draw_subdock_content_on_icon (icon, g_pMainDock);
				}
			}
		}
	}
	
	if (pPrevIndicators->bDrawIndicatorOnAppli != pIndicators->bDrawIndicatorOnAppli)
	{
		cairo_dock_foreach_applis ((CairoDockForeachIconFunc) _set_indicator, FALSE, GINT_TO_POINTER (pIndicators->bDrawIndicatorOnAppli));
	}
	
	if (pPrevIndicators->fBarColorStart[0] != pIndicators->fBarColorStart[0]
	|| pPrevIndicators->fBarColorStart[1] != pIndicators->fBarColorStart[1]
	|| pPrevIndicators->fBarColorStart[2] != pIndicators->fBarColorStart[2]
	|| pPrevIndicators->fBarColorStart[3] != pIndicators->fBarColorStart[3]
	|| pPrevIndicators->fBarColorStop[0] != pIndicators->fBarColorStop[0]
	|| pPrevIndicators->fBarColorStop[1] != pIndicators->fBarColorStop[1]
	|| pPrevIndicators->fBarColorStop[2] != pIndicators->fBarColorStop[2]
	|| pPrevIndicators->fBarColorStop[3] != pIndicators->fBarColorStop[3]
	|| pPrevIndicators->iBarThickness != pIndicators->iBarThickness
	|| pPrevIndicators->bBarHasOutline != pIndicators->bBarHasOutline
	|| pPrevIndicators->fBarColorOutline[0] != pIndicators->fBarColorOutline[0]
	|| pPrevIndicators->fBarColorOutline[1] != pIndicators->fBarColorOutline[1]
	|| pPrevIndicators->fBarColorOutline[2] != pIndicators->fBarColorOutline[2]
	|| pPrevIndicators->fBarColorOutline[3] != pIndicators->fBarColorOutline[3])
	{
		cairo_dock_foreach_icons ((CairoDockForeachIconFunc) _reload_progress_bar, NULL);
	}
	
	cairo_dock_redraw_root_docks (FALSE);  // tous les docks (main dock et les autres qui peuvent contenir des applets avec un indicateur).
}


  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	cairo_dock_unload_image_buffer (&s_indicatorBuffer);
	cairo_dock_unload_image_buffer (&s_activeIndicatorBuffer);
	cairo_dock_unload_image_buffer (&s_classIndicatorBuffer);
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	cairo_dock_register_notification_on_object (&myIconsMgr,
		NOTIFICATION_PRE_RENDER_ICON,
		(CairoDockNotificationFunc) cairo_dock_pre_render_indicator_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification_on_object (&myIconsMgr,
		NOTIFICATION_RENDER_ICON,
		(CairoDockNotificationFunc) cairo_dock_render_indicator_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_indicators_manager (void)
{
	// Manager
	memset (&myIndicatorsMgr, 0, sizeof (CairoIndicatorsManager));
	myIndicatorsMgr.mgr.cModuleName 	= "Indicators";
	myIndicatorsMgr.mgr.init 			= init;
	myIndicatorsMgr.mgr.load 			= load;
	myIndicatorsMgr.mgr.unload 			= unload;
	myIndicatorsMgr.mgr.reload 			= (GldiManagerReloadFunc)reload;
	myIndicatorsMgr.mgr.get_config 		= (GldiManagerGetConfigFunc)get_config;
	myIndicatorsMgr.mgr.reset_config 	= (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myIndicatorsParam, 0, sizeof (CairoIndicatorsParam));
	myIndicatorsMgr.mgr.pConfig = (GldiManagerConfigPtr)&myIndicatorsParam;
	myIndicatorsMgr.mgr.iSizeOfConfig = sizeof (CairoIndicatorsParam);
	// data
	memset (&s_indicatorBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&s_activeIndicatorBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&s_classIndicatorBuffer, 0, sizeof (CairoDockImageBuffer));
	myIndicatorsMgr.mgr.pData = (GldiManagerDataPtr)NULL;
	myIndicatorsMgr.mgr.iSizeOfData = 0;
	// signals
	cairo_dock_install_notifications_on_object (&myIndicatorsMgr, NB_NOTIFICATIONS_INDICATORS);
	// register
	gldi_register_manager (GLDI_MANAGER(&myIndicatorsMgr));
}
