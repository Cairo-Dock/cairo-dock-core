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
#include "cairo-dock-dock-manager.h"  // gldi_docks_redraw_all_root
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-container.h"
#include "cairo-dock-applications-manager.h"  // cairo_dock_foreach_appli_icon
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_get_icon_container
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-data-renderer.h"
#include "cairo-dock-style-manager.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.bShowAppli
#include "cairo-dock-windows-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-indicator-manager.h"

// public (manager, config, data)
CairoIndicatorsParam myIndicatorsParam;
GldiManager myIndicatorsMgr;

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
	if (! myIndicatorsParam.bRotateWithDock)
		bDirectionUp = bIsHorizontal = TRUE;
	
	//\__________________ On calcule l'offset et le zoom.
	double w = s_indicatorBuffer.iWidth;
	double h = s_indicatorBuffer.iHeight;
	///double fMaxScale = cairo_dock_get_max_scale (pDock);
	///double z = (myIndicatorsParam.bIndicatorOnIcon ? icon->fScale / fMaxScale : 1.) * fRatio;  // on divise par fMaxScale car l'indicateur est charge a la taille max des icones.
	double z = icon->fWidth / w * (myIndicatorsParam.bIndicatorOnIcon ? icon->fScale : 1.) * myIndicatorsParam.fIndicatorRatio;
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
static void _cairo_dock_draw_active_window_indicator_opengl (Icon *icon, CairoDock *pDock, G_GNUC_UNUSED double fRatio)
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
	double w = icon->fWidth/3 * fRatio;
	double h = icon->fHeight/3 * fRatio;
	
	if (! bIsHorizontal)
		glRotatef (90., 0., 0., 1.);
	if (! bDirectionUp)
		glScalef (1., -1., 1.);
	glTranslatef (icon->fWidth * icon->fScale/2 - w/2,  // top-right corner, 1/3 of the icon
		icon->fHeight * icon->fScale/2 - h/2,
		0.);
	
	cairo_dock_draw_texture_with_alpha (s_classIndicatorBuffer.iTexture,
		w,
		h,
		1.);
	glPopMatrix ();
}

static void _cairo_dock_draw_appli_indicator (Icon *icon, CairoDock *pDock, cairo_t *pCairoContext)
{
	gboolean bIsHorizontal = pDock->container.bIsHorizontal;
	gboolean bDirectionUp = pDock->container.bDirectionUp;
	if (! myIndicatorsParam.bRotateWithDock)
		bDirectionUp = bIsHorizontal = TRUE;
	
	//\__________________ On calcule l'offset et le zoom.
	double w = s_indicatorBuffer.iWidth;
	double h = s_indicatorBuffer.iHeight;
	///double fMaxScale = cairo_dock_get_max_scale (pDock);
	///double z = (myIndicatorsParam.bIndicatorOnIcon ? icon->fScale / fMaxScale : 1.) * fRatio;  // on divise par fMaxScale car l'indicateur est charge a la taille max des icones.
	double z = icon->fWidth / w * (myIndicatorsParam.bIndicatorOnIcon ? icon->fScale : 1.) * myIndicatorsParam.fIndicatorRatio;
	double fY = - _compute_delta_y (icon, myIndicatorsParam.fIndicatorDeltaY, myIndicatorsParam.bIndicatorOnIcon, pDock->container.bUseReflect);  // a 0, le bas de l'indicateur correspond au bas de l'icone.
	
	//\__________________ On place l'indicateur.
	cairo_save (pCairoContext);
	if (bIsHorizontal)
	{
		cairo_translate (pCairoContext,
			icon->fWidth * icon->fScale / 2 - w * z/2,
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
			icon->fWidth * icon->fScale / 2 - (w * z/2));
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
	if (myIndicatorsParam.bZoomClassIndicator)
		fRatio *= icon->fScale;
	double w = s_classIndicatorBuffer.iWidth;
	double h = s_classIndicatorBuffer.iHeight;
	if (bIsHorizontal)  // draw it in the top-right corner, at 1/3 of the icon.
	{
		if (bDirectionUp)
			cairo_translate (pCairoContext,
				icon->fWidth * (icon->fScale - fRatio/3),
				0.);
		else
			cairo_translate (pCairoContext,
				icon->fWidth * (icon->fScale - fRatio/3),
				icon->fHeight * (icon->fScale - fRatio/3));
	}
	else
	{
		if (bDirectionUp)
			cairo_translate (pCairoContext,
				0.,
				icon->fWidth * (icon->fScale - fRatio/3));
		else
			cairo_translate (pCairoContext,
				icon->fHeight * (icon->fScale - fRatio/3),
				icon->fWidth * (icon->fScale - fRatio/3));
	}
	cairo_scale (pCairoContext, icon->fWidth/3 * fRatio / w, icon->fHeight/3 * fRatio / h);
	cairo_dock_draw_surface (pCairoContext, s_classIndicatorBuffer.pSurface, w, h, bDirectionUp, bIsHorizontal, 1.);
	cairo_restore (pCairoContext);
}

static inline gboolean _active_indicator_is_visible (Icon *icon)
{
	gboolean bIsActive = FALSE;
	if (icon->pAppli)
	{
		GldiWindowActor *pAppli = gldi_windows_get_active ();
		if (pAppli != NULL)
		{
			bIsActive = (icon->pAppli == pAppli);
			if (!bIsActive && icon->pSubDock != NULL)
			{
				Icon *subicon;
				GList *ic;
				for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
				{
					subicon = ic->data;
					if (subicon->pAppli == pAppli)
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

static gboolean cairo_dock_pre_render_indicator_notification (G_GNUC_UNUSED gpointer pUserData, Icon *icon, CairoDock *pDock, cairo_t *pCairoContext)
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
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean cairo_dock_render_indicator_notification (G_GNUC_UNUSED gpointer pUserData, Icon *icon, CairoDock *pDock, G_GNUC_UNUSED gboolean *bHasBeenRendered, cairo_t *pCairoContext)
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
		if (icon->pSubDock != NULL && icon->cClass != NULL && s_classIndicatorBuffer.pSurface != NULL && icon->pAppli == NULL)  // le dernier test est de la paranoia.
		{
			_cairo_dock_draw_class_indicator (pCairoContext, icon, pDock->container.bIsHorizontal, pDock->container.fRatio, pDock->container.bDirectionUp);
		}
	}
	else
	{
		if (icon->bHasIndicator && myIndicatorsParam.bIndicatorAbove)
		{
			glPushMatrix ();
//			glLoadIdentity();
//			cairo_dock_translate_on_icon_opengl (icon, CAIRO_CONTAINER (pDock), 1.);
			_cairo_dock_draw_appli_indicator_opengl (icon, pDock);
			glPopMatrix ();
		}
		if (bIsActive)
		{
			_cairo_dock_draw_active_window_indicator_opengl (icon, pDock, pDock->container.fRatio);
		}
		if (icon->pSubDock != NULL && icon->cClass != NULL && s_classIndicatorBuffer.iTexture != 0 && icon->pAppli == NULL)  // le dernier test est de la paranoia.
		{
			_cairo_dock_draw_class_indicator_opengl (icon, pDock->container.bIsHorizontal, pDock->container.fRatio, pDock->container.bDirectionUp);
		}
	}
	return GLDI_NOTIFICATION_LET_PASS;
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
		cIndicatorImageName = NULL;
	}
	if (pIndicators->cIndicatorImagePath == NULL)
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
	pIndicators->bActiveFillFrame = (cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active frame", &bFlushConfFileNeeded, 0, NULL, NULL) == 0);
	int iIndicType = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active style", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 in case the key doesn't exist yet
	if (iIndicType == -1)  // old param < 3.4
	{
		iIndicType = g_key_file_get_integer (pKeyFile, "Indicators", "active indic type", NULL);
		
		if (iIndicType == 0)
		{
			gchar *cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "active indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
			if (cIndicatorImageName != NULL)  // ensure the image exists
			{
				pIndicators->cActiveIndicatorImagePath = cairo_dock_search_image_s_path (cIndicatorImageName);
				g_free (cIndicatorImageName);
			}
		}
		if (pIndicators->cActiveIndicatorImagePath != NULL)
			iIndicType = 1;
		else
			iIndicType = 2;
		g_key_file_set_integer (pKeyFile, "Indicators", "active style", iIndicType);
		
		int iActiveLineWidth = g_key_file_get_integer (pKeyFile, "Indicators", "active line width", NULL);
		pIndicators->bActiveFillFrame = (iActiveLineWidth == 0);
		g_key_file_set_integer (pKeyFile, "Indicators", "active frame", pIndicators->bActiveFillFrame ? 0:1);
		if (iActiveLineWidth == 0)
			g_key_file_set_integer (pKeyFile, "Indicators", "active line width", 2);
	}
	if (iIndicType == 1)
	{
		gchar *cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "active indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
		if (cIndicatorImageName != NULL && pIndicators->cActiveIndicatorImagePath == NULL)  // ensure the image exists
			pIndicators->cActiveIndicatorImagePath = cairo_dock_search_image_s_path (cIndicatorImageName);
		g_free (cIndicatorImageName);
		if (pIndicators->cActiveIndicatorImagePath == NULL)
			iIndicType = 0;
	}
	if (iIndicType == 0)
	{
		pIndicators->bActiveUseDefaultColors = TRUE;
	}
	else if (iIndicType == 2)
	{
		GldiColor couleur_active = {{0., 0.4, 0.8, 0.5}};
		cairo_dock_get_color_key_value (pKeyFile, "Indicators", "active color", &bFlushConfFileNeeded, &pIndicators->fActiveColor, &couleur_active, "Icons", NULL);
		pIndicators->iActiveLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active line width", &bFlushConfFileNeeded, 3, "Icons", NULL);
		pIndicators->iActiveCornerRadius = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active corner radius", &bFlushConfFileNeeded, 6, "Icons", NULL);
	}
	
	/**
	int iIndicType = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active indic type", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 pour pouvoir intercepter le cas ou la cle n'existe pas.
	
	if (iIndicType == 0 || iIndicType == -1)  // image or new key => get the image
	{
		cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "active indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
		if (cIndicatorImageName != NULL)  // ensure the image exists
			pIndicators->cActiveIndicatorImagePath = cairo_dock_search_image_s_path (cIndicatorImageName);
		if (iIndicType == -1)  // new key
		{
			iIndicType = (pIndicators->cActiveIndicatorImagePath != NULL ? 0 : 1);  // if a valid image was defined before, use this option.
			g_key_file_set_integer (pKeyFile, "Indicators", "active indic type", iIndicType);
		}
		g_free (cIndicatorImageName);
		cIndicatorImageName = NULL;
	}
	
	if (pIndicators->cActiveIndicatorImagePath == NULL)  // 'frame' option, or no image defined, or nonexistent image => load the frame
	{
		GldiColor couleur_active = {{0., 0.4, 0.8, 0.5}};
		cairo_dock_get_color_key_value (pKeyFile, "Indicators", "active color", &bFlushConfFileNeeded, &pIndicators->fActiveColor, &couleur_active, "Icons", NULL);
		pIndicators->iActiveLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active line width", &bFlushConfFileNeeded, 3, "Icons", NULL);
		pIndicators->iActiveCornerRadius = cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "active corner radius", &bFlushConfFileNeeded, 6, "Icons", NULL);
	}  // donc ici si on choisit le mode "image" sans en definir une, le alpha de la couleur reste a 0 => aucun indicateur
	*/
	pIndicators->bActiveIndicatorAbove = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "active frame position", &bFlushConfFileNeeded, FALSE, "Icons", NULL);
	
	//\__________________ On recupere l'indicateur de classe groupee.
	pIndicators->bUseClassIndic = (cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "use class indic", &bFlushConfFileNeeded, 0, NULL, NULL) == 0);
	if (pIndicators->bUseClassIndic)
	{
		cIndicatorImageName = cairo_dock_get_string_key_value (pKeyFile, "Indicators", "class indicator", &bFlushConfFileNeeded, NULL, NULL, NULL);
		if (cIndicatorImageName != NULL)
		{
			pIndicators->cClassIndicatorImagePath = cairo_dock_search_image_s_path (cIndicatorImageName);
			g_free (cIndicatorImageName);
			cIndicatorImageName = NULL;
		}
		else
		{
			pIndicators->cClassIndicatorImagePath = g_strdup (GLDI_SHARE_DATA_DIR"/icons/default-class-indicator.svg");
		}
		pIndicators->bZoomClassIndicator = cairo_dock_get_boolean_key_value (pKeyFile, "Indicators", "zoom class", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	}
	
	//\__________________ Progress bar.
	pIndicators->bBarUseDefaultColors = (cairo_dock_get_integer_key_value (pKeyFile, "Indicators", "bar_colors", &bFlushConfFileNeeded, 1, NULL, NULL) == 0);
	GldiColor start_color = {{.53, .53, .53, .85}};  // grey
	cairo_dock_get_color_key_value (pKeyFile, "Indicators", "bar_color_start", &bFlushConfFileNeeded, &pIndicators->fBarColorStart, &start_color, NULL, NULL);
	GldiColor stop_color= {{.87, .87, .87, .85}};  // grey (lighter)
	cairo_dock_get_color_key_value (pKeyFile, "Indicators", "bar_color_stop", &bFlushConfFileNeeded, &pIndicators->fBarColorStop, &stop_color, NULL, NULL);
	GldiColor outline_color = {{1, 1, 1, .85}};  // white
	cairo_dock_get_color_key_value (pKeyFile, "Indicators", "bar_color_outline", &bFlushConfFileNeeded, &pIndicators->fBarColorOutline, &outline_color, NULL, NULL);
	if (g_key_file_has_key (pKeyFile, "Indicators", "bar_outline", NULL))  // old param < 3.4
	{
		if (! g_key_file_get_boolean (pKeyFile, "Indicators", "bar_outline", NULL))
			pIndicators->fBarColorOutline.rgba.alpha = 0.;
	}
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
static inline void _load_active_window_indicator (const gchar *cImagePath, double fMaxScale, double fCornerRadius, double fLineWidth, GldiColor *fActiveColor, gboolean bDefaultValues, gboolean bFillFrame)
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
	else
	{
		cairo_surface_t *pSurface = cairo_dock_create_blank_surface (iWidth, iHeight);
		cairo_t *pCairoContext = cairo_create (pSurface);
		
		if (bDefaultValues)
		{
			fCornerRadius = myStyleParam.iCornerRadius;
			fLineWidth = 2*myStyleParam.iLineWidth;  // *2 or it will be too light
		}
		if (bFillFrame)
			fLineWidth = 0;
		fCornerRadius = MIN (fCornerRadius, (iWidth - fLineWidth) / 2);
		double fFrameWidth = iWidth - (2 * fCornerRadius + fLineWidth);
		double fFrameHeight = iHeight - 2 * fLineWidth;
		double fDockOffsetX = fCornerRadius + fLineWidth/2;
		double fDockOffsetY = fLineWidth/2;
		cairo_dock_draw_frame (pCairoContext, fCornerRadius, fLineWidth, fFrameWidth, fFrameHeight, fDockOffsetX, fDockOffsetY, 1, 0., CAIRO_DOCK_HORIZONTAL, TRUE);
		
		if (bDefaultValues)
		{
			if (bFillFrame)
				gldi_style_colors_set_selected_bg_color (pCairoContext);
			else
			{
				gldi_style_colors_set_child_color (pCairoContext);  // for a line, the selected color is often too close to the bg color
				cairo_set_line_width (pCairoContext, fLineWidth);
			}
		}
		else
		{
			gldi_color_set_cairo (pCairoContext, fActiveColor);
			if (! bFillFrame)
				cairo_set_line_width (pCairoContext, fLineWidth);
		}
		if (bFillFrame)
		{
			cairo_fill (pCairoContext);
		}
		else
		{
			cairo_stroke (pCairoContext);
		}
		cairo_destroy (pCairoContext);
		
		cairo_dock_load_image_buffer_from_surface (&s_activeIndicatorBuffer,
			pSurface,
			iWidth,
			iHeight);
	}
}
static inline void _load_class_indicator (const gchar *cIndicatorImagePath)
{
	cairo_dock_unload_image_buffer (&s_classIndicatorBuffer);
	
	int iLauncherWidth = myIconsParam.iIconWidth;
	int iLauncherHeight = myIconsParam.iIconHeight;
	
	cairo_dock_load_image_buffer (&s_classIndicatorBuffer,
		cIndicatorImagePath,
		iLauncherWidth/3,  // will be drawn at 1/3 of the icon, with no zoom.
		iLauncherHeight/3,
		CAIRO_DOCK_KEEP_RATIO);
}
static void load (void)
{
	double fMaxScale = 1 + myIconsParam.fAmplitude;
	
	_load_task_indicator (myTaskbarParam.bShowAppli && (myTaskbarParam.bMixLauncherAppli || myIndicatorsParam.bDrawIndicatorOnAppli) ? myIndicatorsParam.cIndicatorImagePath : NULL, fMaxScale, myIndicatorsParam.fIndicatorRatio);
	
	_load_active_window_indicator (myIndicatorsParam.cActiveIndicatorImagePath, fMaxScale, myIndicatorsParam.iActiveCornerRadius, myIndicatorsParam.iActiveLineWidth, &myIndicatorsParam.fActiveColor, myIndicatorsParam.bActiveUseDefaultColors, myIndicatorsParam.bActiveFillFrame);
	
	_load_class_indicator (myTaskbarParam.bShowAppli && myTaskbarParam.bGroupAppliByClass ? myIndicatorsParam.cClassIndicatorImagePath : NULL);
}


  //////////////
 /// RELOAD ///
//////////////

static void _set_indicator (Icon *pIcon, gpointer data)
{
	pIcon->bHasIndicator = GPOINTER_TO_INT (data);
}
static void _reload_progress_bar (Icon *pIcon, G_GNUC_UNUSED gpointer data)
{
	if (cairo_dock_get_icon_data_renderer (pIcon) != NULL)
	{
		GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
		cairo_dock_reload_data_renderer_on_icon (pIcon, pContainer);
	}
}
static void _reload_multi_appli (Icon *icon, G_GNUC_UNUSED gpointer data)
{
	if (CAIRO_DOCK_IS_MULTI_APPLI (icon))
	{
		GldiContainer *pContainer = cairo_dock_get_icon_container (icon);
		cairo_dock_load_icon_image (icon, pContainer);
		if (!myIndicatorsParam.bUseClassIndic)
			cairo_dock_draw_subdock_content_on_icon (icon, CAIRO_DOCK (pContainer));
	}
}
static void reload (CairoIndicatorsParam *pPrevIndicators, CairoIndicatorsParam *pIndicators)
{
	double fMaxScale = 1 + myIconsParam.fAmplitude;
	
	if (g_strcmp0 (pPrevIndicators->cIndicatorImagePath, pIndicators->cIndicatorImagePath) != 0
	|| pPrevIndicators->bIndicatorOnIcon != pIndicators->bIndicatorOnIcon
	|| pPrevIndicators->fIndicatorRatio != pIndicators->fIndicatorRatio)
	{
		_load_task_indicator (myTaskbarParam.bShowAppli && (myTaskbarParam.bMixLauncherAppli || myIndicatorsParam.bDrawIndicatorOnAppli) ? pIndicators->cIndicatorImagePath : NULL, fMaxScale, pIndicators->fIndicatorRatio);
	}
	
	if (g_strcmp0 (pPrevIndicators->cActiveIndicatorImagePath, pIndicators->cActiveIndicatorImagePath) != 0
	|| pPrevIndicators->iActiveCornerRadius != pIndicators->iActiveCornerRadius
	|| pPrevIndicators->iActiveLineWidth != pIndicators->iActiveLineWidth
	|| gldi_color_compare (&pPrevIndicators->fActiveColor, &pIndicators->fActiveColor)
	|| pPrevIndicators->bActiveUseDefaultColors != pIndicators->bActiveUseDefaultColors
	|| pPrevIndicators->bActiveFillFrame != pIndicators->bActiveFillFrame)
	{
		_load_active_window_indicator (pIndicators->cActiveIndicatorImagePath,
			fMaxScale,
			pIndicators->iActiveCornerRadius,
			pIndicators->iActiveLineWidth,
			&pIndicators->fActiveColor,
			pIndicators->bActiveUseDefaultColors,
			pIndicators->bActiveFillFrame);
	}
	
	if (g_strcmp0 (pPrevIndicators->cClassIndicatorImagePath, pIndicators->cClassIndicatorImagePath) != 0
	|| pPrevIndicators->bUseClassIndic != pIndicators->bUseClassIndic)
	{
		_load_class_indicator (myTaskbarParam.bShowAppli && myTaskbarParam.bGroupAppliByClass ? pIndicators->cClassIndicatorImagePath : NULL);
		
		if (pPrevIndicators->bUseClassIndic != pIndicators->bUseClassIndic)  // on recharge les icones pointant sur une classe (qui sont dans le main dock).
		{
			gldi_icons_foreach_in_docks ((GldiIconFunc)_reload_multi_appli, NULL);
		}
	}
	
	if (pPrevIndicators->bDrawIndicatorOnAppli != pIndicators->bDrawIndicatorOnAppli)
	{
		cairo_dock_foreach_appli_icon ((GldiIconFunc) _set_indicator, GINT_TO_POINTER (pIndicators->bDrawIndicatorOnAppli));
	}
	
	if (pPrevIndicators->bBarUseDefaultColors != pIndicators->bBarUseDefaultColors
	|| gldi_color_compare (&pPrevIndicators->fBarColorStart, &pIndicators->fBarColorStart)
	|| gldi_color_compare (&pPrevIndicators->fBarColorStop, &pIndicators->fBarColorStop)
	|| gldi_color_compare (&pPrevIndicators->fBarColorOutline, &pIndicators->fBarColorOutline)
	|| pPrevIndicators->iBarThickness != pIndicators->iBarThickness)
	{
		gldi_icons_foreach ((GldiIconFunc) _reload_progress_bar, NULL);
	}
	
	gldi_docks_redraw_all_root ();
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

static gboolean on_style_changed (G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Indic: style changed to %d", myIndicatorsParam.bBarUseDefaultColors);
	if (myIndicatorsParam.bBarUseDefaultColors)  // reload progress bars
	{
		cd_debug ("reload indicators...");
		gldi_icons_foreach ((GldiIconFunc) _reload_progress_bar, NULL);
	}
	if (myIndicatorsParam.bActiveUseDefaultColors)  // reload active indicator
	{
		cd_debug ("reload active indicator...");
		double fMaxScale = 1 + myIconsParam.fAmplitude;
		_load_active_window_indicator (myIndicatorsParam.cActiveIndicatorImagePath, fMaxScale, myIndicatorsParam.iActiveCornerRadius, myIndicatorsParam.iActiveLineWidth, &myIndicatorsParam.fActiveColor, myIndicatorsParam.bActiveUseDefaultColors, myIndicatorsParam.bActiveFillFrame);
	}
	return GLDI_NOTIFICATION_LET_PASS;
}

static void init (void)
{
	gldi_object_register_notification (&myIconObjectMgr,
		NOTIFICATION_PRE_RENDER_ICON,
		(GldiNotificationFunc) cairo_dock_pre_render_indicator_notification,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myIconObjectMgr,
		NOTIFICATION_RENDER_ICON,
		(GldiNotificationFunc) cairo_dock_render_indicator_notification,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myStyleMgr,
		NOTIFICATION_STYLE_CHANGED,
		(GldiNotificationFunc) on_style_changed,
		GLDI_RUN_AFTER, NULL);
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_indicators_manager (void)
{
	// Manager
	memset (&myIndicatorsMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myIndicatorsMgr), &myManagerObjectMgr, NULL);
	myIndicatorsMgr.cModuleName  = "Indicators";
	// interface
	myIndicatorsMgr.init         = init;
	myIndicatorsMgr.load         = load;
	myIndicatorsMgr.unload       = unload;
	myIndicatorsMgr.reload       = (GldiManagerReloadFunc)reload;
	myIndicatorsMgr.get_config   = (GldiManagerGetConfigFunc)get_config;
	myIndicatorsMgr.reset_config = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myIndicatorsParam, 0, sizeof (CairoIndicatorsParam));
	myIndicatorsMgr.pConfig = (GldiManagerConfigPtr)&myIndicatorsParam;
	myIndicatorsMgr.iSizeOfConfig = sizeof (CairoIndicatorsParam);
	// data
	memset (&s_indicatorBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&s_activeIndicatorBuffer, 0, sizeof (CairoDockImageBuffer));
	memset (&s_classIndicatorBuffer, 0, sizeof (CairoDockImageBuffer));
	myIndicatorsMgr.pData = (GldiManagerDataPtr)NULL;
	myIndicatorsMgr.iSizeOfData = 0;
	// signals
	gldi_object_install_notifications (&myIndicatorsMgr, NB_NOTIFICATIONS_INDICATORS);
}
