/******************************************************************************

This file is a part of the cairo-dock program, 
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cairo-dock-log.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-applet-facility.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-data-renderer.h"

extern gboolean g_bUseOpenGL;

#define cairo_dock_set_data_renderer_on_icon(pIcon, pRenderer) (pIcon)->pDataRenderer = pRenderer
#define cairo_dock_get_icon_data_renderer(pIcon) (pIcon)->pDataRenderer


CairoDataRenderer *cairo_dock_new_data_renderer (const gchar *cRendererName)
{
	CairoDataRendererInitFunc init = cairo_dock_get_data_renderer_entry_point (cRendererName);
	g_return_val_if_fail (init != NULL, NULL);
	
	return init ();
}

CairoDataRenderer *cairo_dock_create_data_renderer (cairo_t *pSourceContext, CairoContainer *pContainer, CairoDataRendererAttribute *pAttribute)
{
	CairoDataRenderer *pRenderer = cairo_dock_new_data_renderer (pAttribute->cModelName);
	if (pRenderer == NULL)
		return NULL;
	
	//\_______________ On alloue la structure des donnees.
	pRenderer->data.iNbValues = pAttribute->iNbValues;
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
				pRenderer->data.pMinMaxValues[2*i] = 1e6;
				pRenderer->data.pMinMaxValues[2*i+1] = -1e6;
			}
		}
		else
		{
			for (i = 0; i < pRenderer->data.iNbValues; i ++)
			{
				pRenderer->data.pMinMaxValues[2*i] = 0;
				pRenderer->data.pMinMaxValues[2*i+1] = 1;
			}
		}
	}
	
	//\_______________ On charge les parametres generaux.
	pRenderer->bUpdateMinMax = pAttribute->bUpdateMinMax;
	pRenderer->bWriteValues = pAttribute->bWriteValues;
	pRenderer->iLatencyTime = pAttribute->iLatencyTime;
	pRenderer->cTitles = pAttribute->cTitles;
	memcpy (pRenderer->fTextColor, pAttribute->fTextColor, sizeof (pRenderer->fTextColor));
	pRenderer->iSmoothAnimationStep = 0;
	pRenderer->format_value = pAttribute->format_value;
	
	return pRenderer;
}

static void _cairo_dock_draw_icon_texture_opengl (CairoDataRenderer *pDataRenderer, Icon *pIcon, CairoContainer *pContainer)
{
	if (! cairo_dock_begin_draw_icon (pIcon, pContainer))
		return ;
	
	pDataRenderer->interface.render_opengl (pDataRenderer);
	
	cairo_dock_end_draw_icon (pIcon, pContainer);
}
static gboolean cairo_dock_update_icon_data_renderer_notification (gpointer pUserData, Icon *pIcon, CairoContainer *pContainer, gboolean *bContinueAnimation)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	if (pRenderer == NULL)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	pRenderer->iSmoothAnimationStep ++;
	int iDeltaT = (int) ceil (90. / pContainer->iAnimationDeltaT) * pContainer->iAnimationDeltaT;
	int iNbIterations = pRenderer->iLatencyTime / iDeltaT;
	if (pRenderer->iSmoothAnimationStep < iNbIterations)
	{
		pRenderer->fLatency = 1. * (iNbIterations - pRenderer->iSmoothAnimationStep) / iNbIterations;
		_cairo_dock_draw_icon_texture_opengl (pRenderer, pIcon, pContainer);
	}
	
	*bContinueAnimation = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

void cairo_dock_add_new_data_renderer_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pSourceContext, CairoDataRendererAttribute *pAttribute)
{
	CairoDataRenderer *pRenderer = cairo_dock_create_data_renderer (pSourceContext, pContainer, pAttribute);
	cairo_dock_set_data_renderer_on_icon (pIcon, pRenderer);
	if (pRenderer == NULL)
		return ;
	
	cairo_dock_get_icon_extent (pIcon, pContainer, &pRenderer->iWidth, &pRenderer->iHeight);
	
	pRenderer->interface.load (pRenderer, pSourceContext, pContainer, pAttribute);
	
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer) && pRenderer->interface.render_opengl)
	{
		cairo_dock_register_notification_on_icon (pIcon, CAIRO_DOCK_UPDATE_ICON_SLOW,
			(CairoDockNotificationFunc) cairo_dock_update_icon_data_renderer_notification,
			CAIRO_DOCK_RUN_AFTER, NULL);
	}
}



void cairo_dock_render_new_data_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pCairoContext, double *pNewValues)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	if (pRenderer == NULL)
		return ;
	
	//\___________________ On met a jour les valeurs du renderer.
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	pData->iCurrentIndex ++;
	if (pData->iCurrentIndex >= pData->iNbValues)
		pData->iCurrentIndex -= pData->iNbValues;
	
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
				pData->pMinMaxValues[2*i] = fNewValue;
		}
		pData->pTabValues[pData->iCurrentIndex][i] = fNewValue;
	}
	
	//\___________________ On met a jour le dessin de l'icone.
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer)&& pRenderer->interface.render_opengl)
	{
		pRenderer->fLatency = 0;
		_cairo_dock_draw_icon_texture_opengl (pRenderer, pIcon, pContainer);
		
		if (pRenderer->iLatencyTime > 0)
		{
			pRenderer->iSmoothAnimationStep = 0;
			cairo_dock_launch_animation (pContainer);
		}
	}
	else
	{
		cairo_save (pCairoContext);
		//\________________ On efface tout.
		cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 0.0);
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
		cairo_paint (pCairoContext);
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
		
		//\________________ On dessine.
		cairo_save (pCairoContext);
		pRenderer->interface.render (pRenderer, pCairoContext);
		cairo_restore (pCairoContext);
		
		if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->bUseReflect)
		{
			double fMaxScale = cairo_dock_get_max_scale (pContainer);
			
			cairo_surface_destroy (pIcon->pReflectionBuffer);
			pIcon->pReflectionBuffer = cairo_dock_create_reflection_surface (pIcon->pIconBuffer,
				pCairoContext,
				(pContainer->bIsHorizontal ? pIcon->fWidth : pIcon->fHeight) * fMaxScale,
				(pContainer->bIsHorizontal ? pIcon->fHeight : pIcon->fWidth) * fMaxScale,
				pContainer->bIsHorizontal,
				fMaxScale,
				pContainer->bDirectionUp);
		}
		
		if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer))
			cairo_dock_update_icon_texture (pIcon);
	}
	
	//\___________________ On met a jour l'info rapide si le renderer n'a pu ecrire les valeurs.
	if (! pRenderer->bCanRenderValueAsText && pRenderer->bWriteValues)  // on prend en charge l'ecriture des valeurs.
	{
		double fValue;
		gchar *cBuffer = g_new0 (gchar, pData->iNbValues * (CAIRO_DOCK_DATA_FORMAT_MAX_LEN+1));
		char *str = cBuffer;
		for (i = 0; i < pData->iNbValues; i ++)
		{
			fValue = cairo_data_renderer_get_normalized_current_value (pRenderer, i);
			cairo_data_renderer_format_value_full (pRenderer, fValue, i, str);
			
			if (i+1 < pData->iNbValues)
			{
				while (*str != '\0')
					str ++;
				*str = '\n';
				str ++;
			}
		}
		double fMaxScale = cairo_dock_get_max_scale (pContainer);
		cairo_dock_set_quick_info (pCairoContext, cBuffer, pIcon, fMaxScale);
		g_free (cBuffer);
	}
	
	cairo_dock_redraw_icon (pIcon, pContainer);
}



void cairo_dock_free_data_renderer (CairoDataRenderer *pRenderer)
{
	if (pRenderer == NULL)
		return ;
	
	g_free (pRenderer->data.pValuesBuffer);
	g_free (pRenderer->data.pTabValues);
	g_free (pRenderer->data.pMinMaxValues);
	
	g_free (pRenderer->fMinMaxValues);
	if (pRenderer->cTitles != NULL)
	{
		int i;
		for (i = 0; i < pRenderer->data.iNbValues; i ++)
			g_free (pRenderer->cTitles[i]);
		g_free (pRenderer->cTitles);
	}
	
	pRenderer->interface.free (pRenderer);
}

void cairo_dock_remove_data_renderer_on_icon (Icon *pIcon)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	
	cairo_dock_remove_notification_func (CAIRO_DOCK_UPDATE_ICON_SLOW, (CairoDockNotificationFunc) cairo_dock_update_icon_data_renderer_notification, NULL);
	
	cairo_dock_free_data_renderer (pRenderer);
	cairo_dock_set_data_renderer_on_icon (pIcon, NULL);
}



void cairo_dock_reload_data_renderer_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pSourceContext, CairoDataRendererAttribute *pAttribute)
{
	CairoDataToRenderer *pData = NULL;
	//\_____________ On recupere les donnees de l'actuel renderer.
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	if (pRenderer != NULL)
	{
		if (pRenderer->data.iNbValues == pAttribute->iNbValues)
		{
			pData = g_memdup (&pRenderer->data, sizeof (CairoDataToRenderer));
			memset (&pRenderer->data, 0, sizeof (CairoDataToRenderer));
			
			if (pRenderer->data.iMemorySize != pAttribute->iMemorySize)  // on redimensionne le tampon des valeurs.
			{
				pData->iMemorySize = MAX (2, pAttribute->iMemorySize);
				gdouble *buf = g_realloc (pData->pValuesBuffer, pData->iMemorySize * pData->iNbValues * sizeof (gdouble));
				g_free (pData->pValuesBuffer);
				pData->pValuesBuffer = buf;
				
				pData->pTabValues = g_new (gdouble *, pData->iMemorySize);
				int i;
				for (i = 0; i < pData->iMemorySize; i ++)
				{
					pData->pTabValues[i] = &pData->pValuesBuffer[i*pData->iNbValues];
				}
			}
		}
		cairo_dock_free_data_renderer (pRenderer);
	}
	
	//\_____________ On en cree un nouveau.
	cairo_dock_add_new_data_renderer_on_icon (pIcon, pContainer, pSourceContext, pAttribute);
	
	//\_____________ On lui remet les valeurs actuelles.
	pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	if (pRenderer != NULL && pData != NULL)
		memcpy (&pRenderer->data, pData, sizeof (CairoDataToRenderer));
	g_free (pData);
}
