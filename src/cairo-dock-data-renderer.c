/******************************************************************************

This file is a part of the cairo-dock program, 
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "cairo-dock-log.h"
#include "cairo-dock-renderer-manager.h"

extern gboolean g_bUseOpenGL;

#define cairo_dock_set_data_renderer_on_icon(pIcon, pRenderer) (pIcon)->pDataRenderer = pRenderer
#define cairo_dock_get_icon_data_renderer(pIcon) (pIcon)->pDataRenderer


CairoDataRenderer *cairo_dock_new_data_renderer (const gchar *cRendererName)
{
	CairoDataRendererInitFunc *init = cairo_dock_get_data_renderer_entry_point (cRendererName);
	g_return_val_if_fail (init != NULL, NULL);
	
	return init ();
}

CairoDataRenderer *cairo_dock_create_data_renderer (cairo_t *pSourceContext, CairoDataRendererAttribute *pAttribute)
{
	CairoDataRenderer *pRenderer = cairo_dock_new_data_renderer (pAttribute->cRendererName);
	if (pRenderer != NULL)
		pRenderer->interface.load (pRenderer, pSourceContext, pAttribute);
	
	return pRenderer;
}

void cairo_dock_add_new_data_renderer_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pSourceContext, CairoDataRendererAttribute *pAttribute)
{
	CairoDataRenderer *pRenderer = cairo_dock_create_data_renderer (pSourceContext, pAttribute);
	cairo_dock_set_data_renderer_on_icon (pIcon, pRenderer);
	
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer)&& pDataRenderer->interface.render_opengl)
	{
		cairo_dock_register_notification_on_icon (pIcon, CAIRO_DOCK_UPDATE_ICON_SLOW,
			(CairoDockNotificationFunc) cairo_dock_update_icon_data_renderer_notification,
			CAIRO_DOCK_RUN_AFTER, NULL);
	}
	return pRenderer;
}


static void _cairo_dock_draw_icon_texture_opengl (CairoDataRenderer *pDataRenderer, Icon *pIcon, CairoContainer *pContainer)
{
	/**if (pIcon->vbo)
	{
		// ???
	}
	else if (pIcon->pbuffer)
	{
		//GLXMakeCurrent(pIcon->pbuffer);
		pDataRenderer->interface.render_opengl (pDataRenderer);
		// glCopyTexSubImage2D (pIcon->iTexture);
		// a finir...
	}
	else*/
	{
		GdkGLContext* pGlContext = gtk_widget_get_gl_context (pContainer->pWidget);
		GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (pContainer->pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return ;
		pDataRenderer->interface.render_opengl (pCairoContext, pDataRenderer);
		///glTexCopySubImage2D ();
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
}
gboolean cairo_dock_update_icon_data_renderer_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bContinueAnimation)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	if (pRenderer == NULL)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	_cairo_dock_draw_icon_texture_opengl (pDataRenderer, pIcon, pDock);
	
	*bContinueAnimation = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}



void cairo_dock_render_data_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pCairoContext, double *pNewValues)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	if (pRenderer == NULL)
		return ;
	
	//\___________________ On met a jour les valeurs du renderer.
	CairoDataToRenderer *pData = &pRenderer->data;
	double fNewValue;
	int i;
	for (i = 0; i < pData->iNbValues; i ++)
	{
		fNewValue = pNewValues[i];
		if (pRenderer->bUpdateMinMax)
		{
			if (fNewValue < pData->pMinMaxValues[2*i]
				pData->pMinMaxValues[2*i]= fNewValue;
			if (fNewValue > pData->pMinMaxValues[2*i+1]
				pData->pMinMaxValues[2*i]= fNewValue;
		}
		else
		{
			if (fNewValue < 0)
				fNewValue = 0.;
			else if (fNewValue > 1)
				fNewValue = 1.;
		}
		pData->pTabValues[pData->iCurrentIndex][i] = fNewValue;
	}
	
	pData->iCurrentIndex ++;
	if (pData->iCurrentIndex >= pData->iNbValues)
		pData->iCurrentIndex -= pData->iNbValues;
	
	//\___________________ On met a jour le dessin de l'icone.
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer)&& pDataRenderer->interface.render_opengl)
	{
		_cairo_dock_draw_icon_texture_opengl (pDataRenderer, pIcon, pContainer);
	}
	else
	{
		cairo_save (pCairoContext);
		pDataRenderer->interface.render (pCairoContext, pDataRenderer);
		cairo_restore (pCairoContext);
		
		if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->bUseReflect)
		{
			double fMaxScale = cairo_dock_get_max_scale (pContainer);
			
			cairo_surface_destroy (pIcon->pReflectionBuffer);
			pIcon->pReflectionBuffer = cairo_dock_create_reflection_surface (pIcon->pIconBuffer,
				pSourceContext,
				(pContainer->bIsHorizontal ? pIcon->fWidth : pIcon->fHeight) * fMaxScale,
				(pContainer->bIsHorizontal ? pIcon->fHeight : pIcon->fWidth) * fMaxScale,
				pContainer->bIsHorizontal,
				fMaxScale,
				pContainer->bDirectionUp);
		}
		
		if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer))
			cairo_dock_update_texture (pIcon);
	}
	
	//\___________________ On met a jour l'info rapide si le renderer n'a pu ecrire les valeurs.
	if (! pDataRenderer->bCanRenderValue && pDataRenderer->bWriteValues)  // on prend en charge l'ecriture des valeurs.
	{
		gchar *cBuffer = g_new0 (gchar *, pData->iNbValues * (CAIRO_DOCK_DATA_FORMAT_MAX_LEN+1));
		char *str = cBuffer
		for (i = 0; i < pData->iNbValues; i ++)
		{
			pDataRenderer->get_format_from_value (pData->pTabValues[pData->iCurrentIndex-1][i], str, CAIRO_DOCK_DATA_FORMAT_MAX_LEN);
			if (i+1 < pData->iNbValues)
			{
				while (*str != '\0')
					str ++
				*str = '\n';
				str ++;
			}
		}
		double fMaxScale = cairo_dock_get_max_scale (pContainer);
		cairo_dock_set_quick_info (pCairoContext, cBuffer, pIcon, fMaxScale);*
		g_free (cBuffer);
	}
	
	cairo_dock_redraw_my_icon (pIcon, pContainer);
}



void cairo_dock_free_data_renderer (CairoDataRenderer *pDataRenderer)
{
	if (pDataRenderer == NULL)
		return ;
	pDataRenderer->interface.free (pDataRenderer);
}

void cairo_dock_remove_data_renderer_on_icon (Icon *pIcon)
{
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	cairo_dock_free_data_renderer (pRenderer);
	cairo_dock_set_data_renderer_on_icon (pIcon, NULL);
}



void cairo_dock_reload_data_renderer_on_icon (Icon *pIcon, cairo_t *pSourceContext, CairoDataRendererAttribute *pAttribute)
{
	CairoDataToRenderer *pData = NULL;
	CairoDataRenderer *pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	if (pRenderer != NULL)
	{
		pData = g_memdup (&pRenderer->data, sizeof (CairoDataToRenderer));
		memset (&pRenderer->data, 0, sizeof (CairoDataToRenderer));
		cairo_dock_free_data_renderer (pRenderer);
	}
	
	cairo_dock_add_new_data_renderer_on_icon (pIcon, pSourceContext, pAttribute);
	
	pRenderer = cairo_dock_get_icon_data_renderer (pIcon);
	if (pRenderer != NULL && pData != NULL)
		memcpy (&pRenderer->data, pData, sizeof (CairoDataToRenderer));
	g_free (pData);
	
}
