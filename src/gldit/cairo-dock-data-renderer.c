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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-opengl-font.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-container.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-load.h"
#include "cairo-dock-config.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-gauge.h"
#include "cairo-dock-graph.h"
#include "cairo-dock-data-renderer.h"

extern gboolean g_bUseOpenGL;
extern CairoContainer *g_pPrimaryContainer;
extern gchar *g_cExtrasDirPath;

#define cairo_dock_set_data_renderer_on_icon(pIcon, pRenderer) (pIcon)->pDataRenderer = pRenderer
#define CD_MIN_TEXT_WITH 24

static CairoDockGLFont *s_pFont = NULL;

#define _init_data_renderer_font(...) s_pFont = cairo_dock_load_textured_font ("Monospace Bold 12", 0, 184)  // on va jusqu'a ø

CairoDockGLFont *cairo_dock_get_default_data_renderer_font (void)
{
	if (s_pFont == NULL)
		_init_data_renderer_font ();
	return s_pFont;
}

void cairo_dock_unload_default_data_renderer_font (void)
{
	cairo_dock_free_gl_font (s_pFont);
	s_pFont = NULL;
}


CairoDataRenderer *cairo_dock_new_data_renderer (const gchar *cRendererName)
{
	CairoDockDataRendererRecord *pRecord = cairo_dock_get_data_renderer_record (cRendererName);
	g_return_val_if_fail (pRecord != NULL && pRecord->iStructSize != 0, NULL);
	
	if (g_pPrimaryContainer && s_pFont == NULL)
	{
		_init_data_renderer_font ();
	}
	
	CairoDataRenderer *pRenderer = g_malloc0 (pRecord->iStructSize);
	memcpy (&pRenderer->interface, &pRecord->interface, sizeof (CairoDataRendererInterface));
	return pRenderer;
}

static void _cairo_dock_init_data_renderer (CairoDataRenderer *pRenderer, CairoContainer *pContainer, CairoDataRendererAttribute *pAttribute)
{
	//\_______________ On alloue la structure des donnees.
	pRenderer->data.iNbValues = MAX (1, pAttribute->iNbValues);
	pRenderer->data.iMemorySize = MAX (2, pAttribute->iMemorySize);  // au moins la derniere valeur et la nouvelle.
	pRenderer->data.pValuesBuffer = g_new0 (gdouble, pRenderer->data.iNbValues * pRenderer->data.iMemorySize);
	pRenderer->data.pTabValues = g_new (gdouble *, pRenderer->data.iMemorySize);
	int i;
	for (i = 0; i < pRenderer->data.iMemorySize; i ++)
	{
		pRenderer->data.pTabValues[i] = &pRenderer->data.pValuesBuffer[i*pRenderer->data.iNbValues];
	}
	pRenderer->data.iCurrentIndex = -1;
	pRenderer->data.pMinMaxValues = g_new (gdouble, 2 * pRenderer->data.iNbValues);
	if (pAttribute->pMinMaxValues != NULL)
	{
		memcpy (pRenderer->data.pMinMaxValues, pAttribute->pMinMaxValues, 2 * pRenderer->data.iNbValues * sizeof (gdouble));
	}
	else
	{
		if (pAttribute->bUpdateMinMax)
		{
			for (i = 0; i < pRenderer->data.iNbValues; i ++)
			{
				pRenderer->data.pMinMaxValues[2*i] = 1.e6;
				pRenderer->data.pMinMaxValues[2*i+1] = -1.e6;
			}
		}
		else
		{
			for (i = 0; i < pRenderer->data.iNbValues; i ++)
			{
				pRenderer->data.pMinMaxValues[2*i] = 0.;
				pRenderer->data.pMinMaxValues[2*i+1] = 1.;
			}
		}
	}
	
	if (pAttribute->cEmblems != NULL)
	{
		pRenderer->pEmblems = g_new0 (CairoDataRendererEmblem, pRenderer->data.iNbValues);
		int i;
		for (i = 0; i < pRenderer->data.iNbValues; i ++)
		{
			pRenderer->pEmblems[i].cImagePath = g_strdup (pAttribute->cEmblems[i]);
			pRenderer->pEmblems[i].param.fAlpha = 1.;
		}
	}
	if (pAttribute->cLabels != NULL)
	{
		pRenderer->pLabels = g_new0 (CairoDataRendererText, pRenderer->data.iNbValues);
		int i;
		for (i = 0; i < pRenderer->data.iNbValues; i ++)
		{
			pRenderer->pLabels[i].cText = g_strdup (pAttribute->cLabels[i]);
			pRenderer->pLabels[i].param.pColor[3] = 1.;
		}
	}
	pRenderer->pValuesText = g_new0 (CairoDataRendererTextParam, pRenderer->data.iNbValues);
	
	//\_______________ On charge les parametres generaux.
	pRenderer->bUpdateMinMax = pAttribute->bUpdateMinMax;
	pRenderer->bWriteValues = pAttribute->bWriteValues;
	pRenderer->iLatencyTime = pAttribute->iLatencyTime;
	pRenderer->iSmoothAnimationStep = 0;
	pRenderer->format_value = pAttribute->format_value;
	pRenderer->pFormatData = pAttribute->pFormatData;
}

static void _cairo_dock_render_to_texture (CairoDataRenderer *pRenderer, Icon *pIcon, CairoContainer *pContainer)
{
	if (! cairo_dock_begin_draw_icon (pIcon, pContainer, 0))
		return ;
	
	//\________________ On dessine.
	glPushMatrix ();
	if (!pContainer->bIsHorizontal && pRenderer->bRotateWithContainer)
	{
		glTranslatef (pRenderer->iWidth/2, pRenderer->iHeight/2, 0.);
		glRotatef (-90., 0., 0., 1.);
		glTranslatef (-pRenderer->iHeight/2, -pRenderer->iWidth/2, 0.);
	}
	
	glPushMatrix ();
	pRenderer->interface.render_opengl (pRenderer);
	glPopMatrix ();
	
	//\________________ On dessine les overlays.
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	if (pRenderer->pEmblems != NULL)
	{
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_over ();
		
		CairoDataRendererEmblem *pEmblem;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pEmblem = &pRenderer->pEmblems[i];
			if (pEmblem->iTexture != 0)
			{
				glBindTexture (GL_TEXTURE_2D, pEmblem->iTexture);
				_cairo_dock_set_alpha (pEmblem->param.fAlpha);
				_cairo_dock_apply_current_texture_at_size_with_offset (
					pEmblem->param.fWidth * pRenderer->iWidth,
					pEmblem->param.fHeight * pRenderer->iHeight,
					pEmblem->param.fX * pRenderer->iWidth,
					pEmblem->param.fY * pRenderer->iHeight);
			}
		}
		_cairo_dock_disable_texture ();
	}
	
	if (pRenderer->pLabels != NULL)
	{
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_over ();
		_cairo_dock_set_alpha (1.);
		
		CairoDataRendererText *pLabel;
		double f;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pLabel = &pRenderer->pLabels[i];
			if (pLabel->iTexture != 0)
			{
				glPushMatrix ();
				f = MIN (pLabel->param.fWidth * pRenderer->iWidth / pLabel->iTextWidth, pLabel->param.fHeight * pRenderer->iHeight / pLabel->iTextHeight);  // on garde le ratio du texte.
				glBindTexture (GL_TEXTURE_2D, pLabel->iTexture);
				_cairo_dock_set_alpha (pLabel->param.pColor[3]);
				_cairo_dock_apply_current_texture_at_size_with_offset (
					pLabel->iTextWidth * f,
					pLabel->iTextHeight * f,
					pLabel->param.fX * pRenderer->iWidth,
					pLabel->param.fY * pRenderer->iHeight);
				glPopMatrix ();
			}
		}
		_cairo_dock_disable_texture ();
	}
	
	if (pRenderer->bWriteValues && pRenderer->bCanRenderValueAsText)
	{
		CairoDataRendererTextParam *pText;
		int w, h, dw, dh;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pText = &pRenderer->pValuesText[i];
			if (pText->fWidth == 0 || pText->fHeight == 0)
				continue;
			
			cairo_data_renderer_format_value (pRenderer, i);
			
			CairoDockGLFont *pFont = cairo_dock_get_default_data_renderer_font ();
			glColor3f (pText->pColor[0], pText->pColor[1], pText->pColor[2]);
			glPushMatrix ();
			
			w = pText->fWidth * pRenderer->iWidth;
			h = pText->fHeight * pRenderer->iHeight;
			dw = w & 1;
			dh = h & 1;
			cairo_dock_draw_gl_text_at_position_in_area (pRenderer->cFormatBuffer,
				pFont,
				floor (pText->fX * pRenderer->iWidth) + .5*dw,
				floor (pText->fY * pRenderer->iHeight) + .5*dh,
				w,
				h,
				TRUE);
			
			glPopMatrix ();
			glColor3f (1.0, 1.0, 1.0);
		}
	}
	glPopMatrix ();
	
	cairo_dock_end_draw_icon (pIcon, pContainer);
}
static void _cairo_dock_render_to_context (CairoDataRenderer *pRenderer, Icon *pIcon, CairoContainer *pContainer, cairo_t *pCairoContext)
{
	//\________________ On efface tout.
	cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pCairoContext);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	
	//\________________ On dessine.
	cairo_save (pCairoContext);
	if (!pContainer->bIsHorizontal && pRenderer->bRotateWithContainer)
	{
		cairo_translate (pCairoContext, pRenderer->iWidth/2, pRenderer->iHeight/2);
		cairo_rotate (pCairoContext, G_PI/2);
		cairo_translate (pCairoContext, -pRenderer->iHeight/2, -pRenderer->iWidth/2);
	}
	
	cairo_save (pCairoContext);
	pRenderer->interface.render (pRenderer, pCairoContext);
	cairo_restore (pCairoContext);
	
	//\________________ On dessine les overlays.
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	if (pRenderer->pEmblems != NULL)
	{
		CairoDataRendererEmblem *pEmblem;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pEmblem = &pRenderer->pEmblems[i];
			if (pEmblem->pSurface != NULL)
			{
				cairo_set_source_surface (pCairoContext,
					pEmblem->pSurface,
					(.5 + pEmblem->param.fX - pEmblem->param.fWidth/2) * pRenderer->iWidth,
					(.5 - pEmblem->param.fY - pEmblem->param.fHeight/2) * pRenderer->iHeight);
				cairo_paint_with_alpha (pCairoContext, pEmblem->param.fAlpha);
			}
		}
	}
	
	if (pRenderer->pLabels != NULL)
	{
		CairoDataRendererText *pLabel;
		double f;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pLabel = &pRenderer->pLabels[i];
			if (pLabel->pSurface != NULL)
			{
				cairo_save (pCairoContext);
				f = MIN (pLabel->param.fWidth * pRenderer->iWidth / pLabel->iTextWidth, pLabel->param.fHeight * pRenderer->iHeight / pLabel->iTextHeight);  // on garde le ratio du texte.
				cairo_scale (pCairoContext,
					f,
					f);
				cairo_set_source_surface (pCairoContext,
					pLabel->pSurface,
					(.5 + pLabel->param.fX) * pRenderer->iWidth/f - pLabel->iTextWidth /2,
					(.5 - pLabel->param.fY) * pRenderer->iHeight/f - pLabel->iTextHeight /2);
				cairo_paint_with_alpha (pCairoContext, pLabel->param.pColor[3]);
				cairo_restore (pCairoContext);
			}
		}
	}
	
	if (pRenderer->bWriteValues && pRenderer->bCanRenderValueAsText)
	{
		CairoDataRendererTextParam *pText;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pText = &pRenderer->pValuesText[i];
			if (pText->fWidth == 0 || pText->fHeight == 0)
				continue;
			
			cairo_data_renderer_format_value (pRenderer, i);
			
			cairo_save (pCairoContext);
			cairo_set_source_rgb (pCairoContext, pText->pColor[0], pText->pColor[1], pText->pColor[2]);
			
			PangoLayout *pLayout = pango_cairo_create_layout (pCairoContext);
			PangoFontDescription *fd = pango_font_description_from_string ("Monospace 12");
			pango_layout_set_font_description (pLayout, fd);
			
			PangoRectangle ink, log;
			pango_layout_set_text (pLayout, pRenderer->cFormatBuffer, -1);
			pango_layout_get_pixel_extents (pLayout, &ink, &log);
			double fZoom = MIN (pText->fWidth * pRenderer->iWidth / (log.width), pText->fHeight * pRenderer->iHeight / log.height);
			
			cairo_move_to (pCairoContext,
				floor ((.5 + pText->fX) * pRenderer->iWidth - log.width*fZoom/2),
				floor ((.5 - pText->fY) * pRenderer->iHeight - log.height*fZoom/2));
			cairo_scale (pCairoContext,
				fZoom,
				fZoom);
			pango_cairo_show_layout (pCairoContext, pLayout);
			g_object_unref (pLayout);
			cairo_restore (pCairoContext);
		}
	}
	cairo_restore (pCairoContext);
	
	if (pContainer->bUseReflect)
	{
		cairo_dock_add_reflection_to_icon (pIcon, pContainer);
	}
	
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer))
		cairo_dock_update_icon_texture (pIcon);
}

static gboolean cairo_dock_update_icon_data_renderer_notification (gpointer pUserData, Icon *pIcon, CairoContainer *pContainer, gboolean *bContinueAnimation)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	if (pRenderer == NULL)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	if (pRenderer->iSmoothAnimationStep > 0)
	{
		pRenderer->iSmoothAnimationStep --;
		int iDeltaT = cairo_dock_get_slow_animation_delta_t (pContainer);
		int iNbIterations = pRenderer->iLatencyTime / iDeltaT;
		
		pRenderer->fLatency = (double) pRenderer->iSmoothAnimationStep / iNbIterations;
		_cairo_dock_render_to_texture (pRenderer, pIcon, pContainer);
		cairo_dock_redraw_icon (pIcon, pContainer);
		
		if (pRenderer->iSmoothAnimationStep < iNbIterations)
			*bContinueAnimation = TRUE;
	}
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static void _cairo_dock_finish_load_data_renderer (CairoDataRenderer *pRenderer, gboolean bLoadTextures)
{
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	//\___________________ On charge les emblemes si l'implementation les a valides.
	if (pRenderer->pEmblems != NULL)
	{
		CairoDataRendererEmblem *pEmblem;
		cairo_surface_t *pSurface;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pEmblem = &pRenderer->pEmblems[i];
			if (pEmblem->pSurface != NULL)
			{
				cairo_surface_destroy (pEmblem->pSurface);
				pEmblem->pSurface = NULL;
			}
			if (pEmblem->iTexture != 0)
			{
				_cairo_dock_delete_texture (pEmblem->iTexture);
				pEmblem->iTexture = 0;
			}
			if (pEmblem->param.fWidth != 0 && pEmblem->param.fHeight != 0 && pEmblem->cImagePath != NULL)
			{
				pSurface = cairo_dock_create_surface_from_image_simple (pEmblem->cImagePath,
					pEmblem->param.fWidth * pRenderer->iWidth,
					pEmblem->param.fHeight * pRenderer->iHeight);
				if (bLoadTextures)
				{
					pEmblem->iTexture = cairo_dock_create_texture_from_surface (pSurface);
					cairo_surface_destroy (pSurface);
				}
				else
					pEmblem->pSurface = pSurface;
			}
		}
	}
	
	//\___________________ On charge les labels si l'implementation les a valides.
	if (pRenderer->pLabels != NULL)
	{
		CairoDockLabelDescription textDescription;
		cairo_dock_copy_label_description (&textDescription, &myLabels.quickInfoTextDescription);
		
		CairoDataRendererText *pLabel;
		cairo_surface_t *pSurface;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pLabel = &pRenderer->pLabels[i];
			if (pLabel->pSurface != NULL)
			{
				cairo_surface_destroy (pLabel->pSurface);
				pLabel->pSurface = NULL;
			}
			if (pLabel->iTexture != 0)
			{
				_cairo_dock_delete_texture (pLabel->iTexture);
				pLabel->iTexture = 0;
			}
			if (pLabel->param.fWidth != 0 && pLabel->param.fHeight != 0 && pLabel->cText != NULL)
			{
				textDescription.fBackgroundColor[3] = 0.;
				textDescription.fColorStart[0] = pLabel->param.pColor[0];
				textDescription.fColorStart[1] = pLabel->param.pColor[1];
				textDescription.fColorStart[2] = pLabel->param.pColor[2];
				textDescription.fColorStop[0] = pLabel->param.pColor[0];
				textDescription.fColorStop[1] = pLabel->param.pColor[1];
				textDescription.fColorStop[2] = pLabel->param.pColor[2];
				pSurface = cairo_dock_create_surface_from_text (pLabel->cText,
					&textDescription,
					&pLabel->iTextWidth, &pLabel->iTextHeight);
				if (bLoadTextures)
				{
					pLabel->iTexture = cairo_dock_create_texture_from_surface (pSurface);
					cairo_surface_destroy (pSurface);
				}
				else
					pLabel->pSurface = pSurface;
			}
		}
	}
	
	//\___________________ On regarde si le texte dessine sur l'icone sera suffisamment lisible.
	if (pRenderer->pValuesText != NULL)
	{
		CairoDataRendererTextParam *pText = &pRenderer->pValuesText[0];
		g_print ("+++++++pText->fWidth * pRenderer->iWidth : %.2f\n", pText->fWidth * pRenderer->iWidth);
		pRenderer->bCanRenderValueAsText = (pText->fWidth * pRenderer->iWidth >= CD_MIN_TEXT_WITH);
	}
}

void cairo_dock_add_new_data_renderer_on_icon (Icon *pIcon, CairoContainer *pContainer, CairoDataRendererAttribute *pAttribute)
{
	//\___________________ On affecte un nouveau DataRenderer a l'icone.
	CairoDataRenderer *pRenderer = cairo_dock_new_data_renderer (pAttribute->cModelName);
	
	cairo_dock_set_data_renderer_on_icon (pIcon, pRenderer);
	if (pRenderer == NULL)
		return ;
	
	_cairo_dock_init_data_renderer (pRenderer, pContainer, pAttribute);
	
	cairo_dock_get_icon_extent (pIcon, pContainer, &pRenderer->iWidth, &pRenderer->iHeight);
	
	gboolean bLoadTextures = FALSE;
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer) && pRenderer->interface.render_opengl)
	{
		bLoadTextures = TRUE;
		cairo_dock_register_notification_on_icon (pIcon, CAIRO_DOCK_UPDATE_ICON_SLOW,
			(CairoDockNotificationFunc) cairo_dock_update_icon_data_renderer_notification,
			CAIRO_DOCK_RUN_AFTER, NULL);  // pour l'affichage fluide.
	}
	
	//\___________________ On le charge.
	pRenderer->interface.load (pRenderer, pContainer, pAttribute);
	
	//\___________________ On charge les overlays si l'implementation les a valides.
	_cairo_dock_finish_load_data_renderer (pRenderer, bLoadTextures);
}



void cairo_dock_render_new_data_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pCairoContext, double *pNewValues)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	g_return_if_fail (pRenderer != NULL);
	
	//\___________________ On met a jour les valeurs du renderer.
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	pData->iCurrentIndex ++;
	if (pData->iCurrentIndex >= pData->iMemorySize)
		pData->iCurrentIndex -= pData->iMemorySize;
	double fNewValue;
	int i;
	for (i = 0; i < pData->iNbValues; i ++)
	{
		fNewValue = pNewValues[i];
		if (pRenderer->bUpdateMinMax)
		{
			if (fNewValue < pData->pMinMaxValues[2*i])
				pData->pMinMaxValues[2*i] = fNewValue;
			if (fNewValue > pData->pMinMaxValues[2*i+1])
				pData->pMinMaxValues[2*i+1] = MAX (fNewValue, pData->pMinMaxValues[2*i]+.1);
		}
		pData->pTabValues[pData->iCurrentIndex][i] = fNewValue;
	}
	
	//\___________________ On met a jour le dessin de l'icone.
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer) && pRenderer->interface.render_opengl)
	{
		if (pRenderer->iLatencyTime > 0)
		{
			int iDeltaT = cairo_dock_get_slow_animation_delta_t (pContainer);
			int iNbIterations = pRenderer->iLatencyTime / iDeltaT;
			pRenderer->iSmoothAnimationStep = iNbIterations;
			cairo_dock_launch_animation (pContainer);
		}
		else
		{
			pRenderer->fLatency = 0;
			_cairo_dock_render_to_texture (pRenderer, pIcon, pContainer);
		}
	}
	else
	{
		_cairo_dock_render_to_context (pRenderer, pIcon, pContainer, pCairoContext);
	}
	
	//\___________________ On met a jour l'info rapide si le renderer n'a pu ecrire les valeurs.
	if (! pRenderer->bCanRenderValueAsText && pRenderer->bWriteValues)  // on prend en charge l'ecriture des valeurs.
	{
		double fValue;
		gchar *cBuffer = g_new0 (gchar, pData->iNbValues * (CAIRO_DOCK_DATA_FORMAT_MAX_LEN+1));
		char *str = cBuffer;
		for (i = 0; i < pData->iNbValues; i ++)
		{
			cairo_data_renderer_format_value_full (pRenderer, i, str);
			
			if (i+1 < pData->iNbValues)
			{
				while (*str != '\0')
					str ++;
				*str = '\n';
				str ++;
			}
		}
		cairo_dock_set_quick_info (pIcon, pContainer, cBuffer);
		g_free (cBuffer);
	}
	else
		cairo_dock_set_quick_info (pIcon, pContainer, NULL);
	
	cairo_dock_redraw_icon (pIcon, pContainer);
}



void cairo_dock_free_data_renderer (CairoDataRenderer *pRenderer)
{
	if (pRenderer == NULL)
		return ;
	
	if (pRenderer->interface.unload)
		pRenderer->interface.unload (pRenderer);
	
	g_free (pRenderer->data.pValuesBuffer);
	g_free (pRenderer->data.pTabValues);
	g_free (pRenderer->data.pMinMaxValues);
	
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	if (pRenderer->pEmblems != NULL)
	{
		CairoDataRendererEmblem *pEmblem;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pEmblem = &pRenderer->pEmblems[i];
			if (pEmblem->pSurface != NULL)
				cairo_surface_destroy (pEmblem->pSurface);
			if (pEmblem->iTexture != 0)
				_cairo_dock_delete_texture (pEmblem->iTexture);
		}
		g_free (pRenderer->pEmblems);
	}
	
	if (pRenderer->pLabels != NULL)
	{
		CairoDataRendererText *pText;
		int i;
		for (i = 0; i < iNbValues; i ++)
		{
			pText = &pRenderer->pLabels[i];
			if (pText->pSurface != NULL)
				cairo_surface_destroy (pText->pSurface);
			if (pText->iTexture != 0)
				_cairo_dock_delete_texture (pText->iTexture);
		}
		g_free (pRenderer->pLabels);
	}
	
	g_free (pRenderer->pValuesText);
	
	g_free (pRenderer);
}

void cairo_dock_remove_data_renderer_on_icon (Icon *pIcon)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	
	cairo_dock_remove_notification_func_on_icon (pIcon, CAIRO_DOCK_UPDATE_ICON_SLOW, (CairoDockNotificationFunc) cairo_dock_update_icon_data_renderer_notification, NULL);
	
	cairo_dock_free_data_renderer (pRenderer);
	cairo_dock_set_data_renderer_on_icon (pIcon, NULL);
}


void cairo_dock_reload_data_renderer_on_icon (Icon *pIcon, CairoContainer *pContainer, CairoDataRendererAttribute *pAttribute)
{
	//\_____________ On recupere les donnees de l'actuel renderer.
	CairoDataToRenderer *pData = NULL;
	CairoDataRenderer *pOldRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	g_return_if_fail (pOldRenderer != NULL || pAttribute != NULL);
	
	if (pAttribute == NULL)  // rien ne change dans les parametres du data-renderer, on se contente de le recharger a la taille de l'icone.
	{
		g_return_if_fail (pOldRenderer->interface.reload != NULL);
		cairo_dock_get_icon_extent (pIcon, pContainer, &pOldRenderer->iWidth, &pOldRenderer->iHeight);
		pOldRenderer->interface.reload (pOldRenderer);
		
		gboolean bLoadTextures = (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer) && pOldRenderer->interface.render_opengl);
		_cairo_dock_finish_load_data_renderer (pOldRenderer, bLoadTextures);
	}
	else  // on recree le data-renderer avec les nouveaux attributs.
	{
		pAttribute->iNbValues = MAX (1, pAttribute->iNbValues);
		//\_____________ On recupere les donnees courantes.
		if (pOldRenderer && cairo_data_renderer_get_nb_values (pOldRenderer) == pAttribute->iNbValues)
		{
			pData = g_memdup (&pOldRenderer->data, sizeof (CairoDataToRenderer));
			memset (&pOldRenderer->data, 0, sizeof (CairoDataToRenderer));
			
			pAttribute->iMemorySize = MAX (2, pAttribute->iMemorySize);
			if (pData->iMemorySize != pAttribute->iMemorySize)  // on redimensionne le tampon des valeurs.
			{
				int iOldMemorySize = pData->iMemorySize;
				pData->iMemorySize = pAttribute->iMemorySize;
				pData->pValuesBuffer = g_realloc (pData->pValuesBuffer, pData->iMemorySize * pData->iNbValues * sizeof (gdouble));
				if (pData->iMemorySize > iOldMemorySize)
				{
					memset (&pData->pValuesBuffer[iOldMemorySize * pData->iNbValues], 0, (pData->iMemorySize - iOldMemorySize) * pData->iNbValues * sizeof (gdouble));
				}
				
				g_free (pData->pTabValues);
				pData->pTabValues = g_new (gdouble *, pData->iMemorySize);
				int i;
				for (i = 0; i < pData->iMemorySize; i ++)
				{
					pData->pTabValues[i] = &pData->pValuesBuffer[i*pData->iNbValues];
				}
				if (pData->iCurrentIndex >= pData->iMemorySize)
					pData->iCurrentIndex = pData->iMemorySize - 1;
			}
		}
		
		//\_____________ On supprime l'ancien.
		cairo_dock_remove_data_renderer_on_icon (pIcon);
		
		//\_____________ On en cree un nouveau.
		cairo_dock_add_new_data_renderer_on_icon (pIcon, pContainer, pAttribute);
		
		//\_____________ On lui remet les valeurs actuelles.
		CairoDataRenderer *pNewRenderer = cairo_dock_get_icon_data_renderer (pIcon);
		if (pNewRenderer != NULL && pData != NULL)
			memcpy (&pNewRenderer->data, pData, sizeof (CairoDataToRenderer));
		g_free (pData);
	}
}


void cairo_dock_resize_data_renderer_history (Icon *pIcon, int iNewMemorySize)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	g_return_if_fail (pRenderer != NULL);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	
	iNewMemorySize = MAX (2, iNewMemorySize);
	//g_print ("iMemorySize : %d -> %d\n", pData->iMemorySize, iNewMemorySize);
	if (pData->iMemorySize == iNewMemorySize)
		return ;
	
	int iOldMemorySize = pData->iMemorySize;
	pData->iMemorySize = iNewMemorySize;
	pData->pValuesBuffer = g_realloc (pData->pValuesBuffer, pData->iMemorySize * pData->iNbValues * sizeof (gdouble));
	if (iNewMemorySize > iOldMemorySize)
	{
		memset (&pData->pValuesBuffer[iOldMemorySize * pData->iNbValues], 0, (iNewMemorySize - iOldMemorySize) * pData->iNbValues * sizeof (gdouble));
	}
	
	g_free (pData->pTabValues);
	pData->pTabValues = g_new (gdouble *, pData->iMemorySize);
	int i;
	for (i = 0; i < pData->iMemorySize; i ++)
	{
		pData->pTabValues[i] = &pData->pValuesBuffer[i*pData->iNbValues];
	}
	if (pData->iCurrentIndex >= pData->iMemorySize)
		pData->iCurrentIndex = pData->iMemorySize - 1;
}

void cairo_dock_refresh_data_renderer (Icon *pIcon, CairoContainer *pContainer, cairo_t *pCairoContext)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	g_return_if_fail (pRenderer != NULL);
	
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer) && pRenderer->interface.render_opengl)
	{
		_cairo_dock_render_to_texture (pRenderer, pIcon, pContainer);
	}
	else
	{
		_cairo_dock_render_to_context (pRenderer, pIcon, pContainer, pCairoContext);
	}
}



  /////////////////////////////////////////////////
 /////////////// LIST OF THEMES  /////////////////
/////////////////////////////////////////////////
GHashTable *cairo_dock_list_available_themes_for_data_renderer (const gchar *cRendererName)
{
	CairoDockDataRendererRecord *pRecord = cairo_dock_get_data_renderer_record (cRendererName);
	g_return_val_if_fail (pRecord != NULL, NULL);
	
	if (pRecord->cThemeDirName == NULL)
		return NULL;
	gchar *cGaugeShareDir = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, pRecord->cThemeDirName);
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s", g_cExtrasDirPath, pRecord->cThemeDirName);
	GHashTable *pGaugeTable = cairo_dock_list_packages (cGaugeShareDir, cGaugeUserDir, pRecord->cThemeDirName);
	
	g_free (cGaugeShareDir);
	g_free (cGaugeUserDir);
	return pGaugeTable;
}


gchar *cairo_dock_get_data_renderer_theme_path (const gchar *cRendererName, const gchar *cThemeName, CairoDockPackageType iType)  // utile pour DBus aussi.
{
	CairoDockDataRendererRecord *pRecord = cairo_dock_get_data_renderer_record (cRendererName);
	g_return_val_if_fail (pRecord != NULL, NULL);
	
	if (pRecord->cThemeDirName == NULL)
		return NULL;
	
	const gchar *cGaugeShareDir = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, pRecord->cThemeDirName);
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s", g_cExtrasDirPath, pRecord->cThemeDirName);
	gchar *cGaugePath = cairo_dock_get_package_path (cThemeName, cGaugeShareDir, cGaugeUserDir, pRecord->cThemeDirName, iType);
	g_free (cGaugeUserDir);
	return cGaugePath;
}

gchar *cairo_dock_get_package_path_for_data_renderer (const gchar *cRendererName, const gchar *cAppletConfFilePath, GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultThemeName)
{
	CairoDockDataRendererRecord *pRecord = cairo_dock_get_data_renderer_record (cRendererName);
	g_return_val_if_fail (pRecord != NULL, NULL);
	
	gchar *cChosenThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	if (cChosenThemeName == NULL)
		cChosenThemeName = g_strdup (pRecord->cDefaultTheme);
	
	CairoDockPackageType iType = cairo_dock_extract_package_type_from_name (cChosenThemeName);
	gchar *cGaugePath = cairo_dock_get_data_renderer_theme_path (cRendererName, cChosenThemeName, iType);
	
	if (cGaugePath == NULL)  // theme introuvable.
		cGaugePath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, pRecord->cThemeDirName, pRecord->cDefaultTheme);
	
	if (iType != CAIRO_DOCK_ANY_PACKAGE)
	{
		g_key_file_set_string (pKeyFile, cGroupName, cKeyName, cChosenThemeName);
		cairo_dock_write_keys_to_file (pKeyFile, cAppletConfFilePath);
	}
	cd_debug ("Theme de la jauge : %s", cGaugePath);
	g_free (cChosenThemeName);
	return cGaugePath;
}


void cairo_dock_register_built_in_data_renderers (void)
{
	cairo_dock_register_data_renderer_graph ();
	cairo_dock_register_data_renderer_gauge ();
}
