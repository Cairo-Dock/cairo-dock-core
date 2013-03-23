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

#include "gldi-config.h"
#include "cairo-dock-draw.h"  // cairo_dock_erase_cairo_context
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-module-factory.h"  // CairoDockModuleInstance
#include "cairo-dock-log.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.iAppliMaxNameLength
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-dock-manager.h"  // cairo_dock_synchronize_one_sub_dock_orientation
#include "cairo-dock-backends-manager.h"  // cairo_dock_get_icon_container_renderer
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-data-renderer.h"
#include "cairo-dock-overlay.h"
#include "cairo-dock-icon-factory.h"

extern CairoDockImageBuffer g_pIconBackgroundBuffer;

extern gboolean g_bUseOpenGL;

const gchar *s_cRendererNames[4] = {NULL, "Emblem", "Stack", "Box"};  // c'est juste pour realiser la transition entre le chiffre en conf, et un nom (limitation du panneau de conf). On garde le numero pour savoir rapidement sur laquelle on set.


inline Icon *cairo_dock_new_icon (void)
{
	Icon *_icon = gldi_object_new (Icon, &myIconsMgr);
	return _icon;
}


void cairo_dock_free_icon_buffers (Icon *icon)
{
	if (icon == NULL)
		return ;
	
	g_free (icon->cDesktopFileName);
	g_free (icon->cFileName);
	g_free (icon->cName);
	g_free (icon->cInitialName);
	g_free (icon->cCommand);
	g_free (icon->cWorkingDirectory);
	g_free (icon->cBaseURI);
	g_free (icon->cParentDockName);  // on ne liberera pas le sous-dock ici sous peine de se mordre la queue, donc il faut l'avoir fait avant.
	g_free (icon->cClass);
	g_free (icon->cWmClass);
	g_free (icon->cQuickInfo);
	g_free (icon->cLastAttentionDemand);
	g_free (icon->pHiddenBgColor);
	if (icon->pMimeTypes)
		g_strfreev (icon->pMimeTypes);
	
	cairo_dock_unload_image_buffer (&icon->image);
	
	cairo_dock_unload_image_buffer (&icon->label);
	
	cairo_dock_destroy_icon_overlays (icon);
}

  //////////////
 /// LOADER ///
//////////////

gboolean cairo_dock_apply_icon_background_opengl (Icon *icon)
{
	if (cairo_dock_begin_draw_icon (icon, icon->pContainer, 1))  // 1 => don't clear current image
	{
		_cairo_dock_enable_texture ();
		glBlendFunc (GL_ONE_MINUS_DST_ALPHA, GL_ONE);  // dest_over = src * (1 - dst.a) + dst
		_cairo_dock_set_alpha (1.);
		_cairo_dock_apply_texture_at_size (g_pIconBackgroundBuffer.iTexture,
			icon->image.iWidth,
			icon->image.iHeight);
		cairo_dock_end_draw_icon (icon, icon->pContainer);
		return TRUE;
	}
	return FALSE;
}

void cairo_dock_load_icon_image (Icon *icon, G_GNUC_UNUSED CairoContainer *pContainer)
{
	if (icon->pContainer == NULL)
	{
		cd_warning ("/!\\ Icon %s is not inside a container !!!", icon->cName);  // it's ok if this happens, but it should be rare, and I'd like to know when, so be noisy.
		return;
	}
	CairoDockModuleInstance *pInstance = icon->pModuleInstance;  // this is the only function where we destroy/create the icon's surface, so we must handle the cairo-context here.
	if (pInstance && pInstance->pDrawContext != NULL)
	{
		cairo_destroy (pInstance->pDrawContext);
		pInstance->pDrawContext = NULL;
	}
	
	//g_print ("%s (%s, %dx%d)\n", __func__, icon->cName, (int)icon->fWidth, (int)icon->fHeight);
	// the renderer of the container must have set the size beforehand, when the icon has been inserted into the container.
	if (cairo_dock_icon_get_allocated_width (icon) <= 0 || cairo_dock_icon_get_allocated_height (icon) <= 0)  // we don't want a surface/texture.
	{
		cairo_dock_unload_image_buffer (&icon->image);
		return;
	}
	g_return_if_fail (icon->fWidth > 0); // should never happen; if it does, it's an error, so be noisy.
	
	//\______________ keep the current buffer on the icon so that the 'load' can use it (for instance, applis may draw emblems).
	cairo_surface_t *pPrevSurface = icon->image.pSurface;
	GLuint iPrevTexture = icon->image.iTexture;
	
	//\______________ load the image buffer (surface + texture).
	if (icon->iface.load_image)
		icon->iface.load_image (icon);
	
	//\______________ if nothing has changed or no image was loaded, set a default image.
	if ((icon->image.pSurface == pPrevSurface || icon->image.pSurface == NULL)
	&& (icon->image.iTexture == iPrevTexture || icon->image.iTexture == 0))
	{
		gchar *cIconPath = cairo_dock_search_image_s_path (CAIRO_DOCK_DEFAULT_ICON_NAME);
		if (cIconPath == NULL)  // fichier non trouve.
		{
			cIconPath = g_strdup (GLDI_SHARE_DATA_DIR"/icons/"CAIRO_DOCK_DEFAULT_ICON_NAME);
		}
		int w = cairo_dock_icon_get_allocated_width (icon);
		int h = cairo_dock_icon_get_allocated_height (icon);
		cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconPath,
			w,
			h);
		cairo_dock_load_image_buffer_from_surface (&icon->image, pSurface, w, h);
		g_free (cIconPath);
	}
	
	//\_____________ set the background if needed.
	icon->bNeedApplyBackground = FALSE;
	if (g_pIconBackgroundBuffer.pSurface != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		if (icon->image.iTexture != 0 && g_pIconBackgroundBuffer.iTexture != 0)
		{
			if (! cairo_dock_apply_icon_background_opengl (icon))  // couldn't draw on the texture
			{
				icon->bDamaged = FALSE;  // it's not a big deal, since we can draw under the existing image easily; so we don't need to damage the icon (it's expensive especially if it's an applet).
				icon->bNeedApplyBackground = TRUE;  // just postpone it until drawing is possible.
			}
		}
		else if (icon->image.pSurface != NULL)
		{
			cairo_t *pCairoIconBGContext = cairo_create (icon->image.pSurface);
			cairo_set_operator (pCairoIconBGContext, CAIRO_OPERATOR_DEST_OVER);
			cairo_dock_apply_image_buffer_surface_at_size (&g_pIconBackgroundBuffer, pCairoIconBGContext,
				icon->image.iWidth, icon->image.iHeight,
				0, 0, 1);
			cairo_destroy (pCairoIconBGContext);
		}
	}
	
	//\______________ free the previous buffers.
	if (pPrevSurface != NULL)
		cairo_surface_destroy (pPrevSurface);
	if (iPrevTexture != 0)
		_cairo_dock_delete_texture (iPrevTexture);
	
	if (pInstance && icon->image.pSurface != NULL)
	{
		pInstance->pDrawContext = cairo_create (icon->image.pSurface);
		if (!pInstance->pDrawContext || cairo_status (pInstance->pDrawContext) != CAIRO_STATUS_SUCCESS)
		{
			cd_warning ("couldn't initialize drawing context, applet won't be able to draw itself !");
			pInstance->pDrawContext = NULL;
		}
	}
}

void cairo_dock_load_icon_text (Icon *icon)
{
	cairo_dock_unload_image_buffer (&icon->label);
	
	if (icon->cName == NULL || (myIconsParam.iconTextDescription.iSize == 0))
		return ;

	gchar *cTruncatedName = NULL;
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskbarParam.iAppliMaxNameLength > 0)
	{
		cTruncatedName = cairo_dock_cut_string (icon->cName, myTaskbarParam.iAppliMaxNameLength);
	}
	
	int iWidth, iHeight;
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_text ((cTruncatedName != NULL ? cTruncatedName : icon->cName),
		&myIconsParam.iconTextDescription,
		&iWidth,
		&iHeight);
	cairo_dock_load_image_buffer_from_surface (&icon->label, pSurface, iWidth, iHeight);
	g_free (cTruncatedName);
}

void cairo_dock_load_icon_quickinfo (Icon *icon)
{
	if (icon->cQuickInfo == NULL)  // no more quick-info -> remove any previous one
	{
		cairo_dock_remove_overlay_at_position (icon, CAIRO_OVERLAY_BOTTOM, (gpointer)"quick-info");
	}
	else  // add an overlay at the bottom with the text surface; any previous "quick-info" overlay will be removed.
	{
		int iWidth, iHeight;
		cairo_dock_get_icon_extent (icon, &iWidth, &iHeight);
		double fMaxScale = cairo_dock_get_icon_max_scale (icon);
		if (iHeight / (myIconsParam.quickInfoTextDescription.iSize * fMaxScale) > 5)  // if the icon is very height (the text occupies less than 20% of the icon)
			fMaxScale = MIN ((double)iHeight / (myIconsParam.quickInfoTextDescription.iSize * 5), MAX (1., 16./myIconsParam.quickInfoTextDescription.iSize) * fMaxScale);  // let's make it use 20% of the icon's height, limited to 16px
		int w, h;
		cairo_surface_t *pSurface = cairo_dock_create_surface_from_text_full (icon->cQuickInfo,
			&myIconsParam.quickInfoTextDescription,
			fMaxScale,
			iWidth,  // limit the text to the width of the icon
			&w, &h);
		CairoOverlay *pOverlay = cairo_dock_add_overlay_from_surface (icon, pSurface, w, h, CAIRO_OVERLAY_BOTTOM, (gpointer)"quick-info");  // the constant string "quick-info" is used as a unique identifier for all quick-infos; the surface is taken by the overlay.
		if (pOverlay)
			cairo_dock_set_overlay_scale (pOverlay, 0);
	}
}


void cairo_dock_load_icon_buffers (Icon *pIcon, CairoContainer *pContainer)
{
	gboolean bLoadText = TRUE;
	if (pIcon->iSidLoadImage != 0)  // if a load was sheduled, cancel it and do it now (we need to load the applets' buffer before initializing the module).
	{
		//g_print (" load %s immediately\n", pIcon->cName);
		g_source_remove (pIcon->iSidLoadImage);
		pIcon->iSidLoadImage = 0;
		bLoadText = FALSE;  // has been done in cairo_dock_trigger_load_icon_buffers(), the only function to schedule the image loading.
	}
	
	if (cairo_dock_icon_get_allocated_width (pIcon) > 0)
	{
		cairo_dock_load_icon_image (pIcon, pContainer);

		if (bLoadText)
			cairo_dock_load_icon_text (pIcon);

		cairo_dock_load_icon_quickinfo (pIcon);
	}
}

static gboolean _load_icon_buffer_idle (Icon *pIcon)
{
	//g_print ("%s (%s; %dx%d; %.2fx%.2f; %x)\n", __func__, pIcon->cName, pIcon->iAllocatedWidth, pIcon->iAllocatedHeight, pIcon->fWidth, pIcon->fHeight, pIcon->pContainer);
	pIcon->iSidLoadImage = 0;
	
	CairoContainer *pContainer = pIcon->pContainer;
	if (pContainer)
	{
		cairo_dock_load_icon_image (pIcon, pContainer);
		
		if (cairo_dock_get_icon_data_renderer (pIcon) != NULL)
			cairo_dock_refresh_data_renderer (pIcon, pContainer);
		
		cairo_dock_load_icon_quickinfo (pIcon);
		
		cairo_dock_redraw_icon (pIcon, pContainer);
		//g_print ("icon-factory: do 1 main loop iteration\n");
		//gtk_main_iteration_do (FALSE);  /// "unforseen consequences" : if _redraw_subdock_content_idle is planned just after, the container-icon stays blank in opengl only. couldn't figure why exactly :-/
	}
	return FALSE;
}
void cairo_dock_trigger_load_icon_buffers (Icon *pIcon)
{
	if (pIcon->iSidLoadImage == 0)
	{
		//g_print ("trigger load for %s\n", pIcon->cName);
		cairo_dock_load_icon_text (pIcon);  // la vue peut avoir besoin de connaitre la taille du texte.
		pIcon->iSidLoadImage = g_idle_add ((GSourceFunc)_load_icon_buffer_idle, pIcon);
	}
}



  ///////////////////////
 /// CONTAINER ICONS ///
///////////////////////

void cairo_dock_draw_subdock_content_on_icon (Icon *pIcon, CairoDock *pDock)
{
	g_return_if_fail (pIcon != NULL && pIcon->pSubDock != NULL && (pIcon->image.pSurface != NULL || pIcon->image.iTexture != 0));
	
	CairoIconContainerRenderer *pRenderer = cairo_dock_get_icon_container_renderer (pIcon->cClass != NULL ? "Stack" : s_cRendererNames[pIcon->iSubdockViewType]);
	if (pRenderer == NULL)
		return;
	cd_debug ("%s (%s)", __func__, pIcon->cName);
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	
	if (pIcon->image.iTexture != 0 && pRenderer->render_opengl)  // dessin opengl
	{
		//\______________ On efface le dessin existant.
		if (! cairo_dock_begin_draw_icon (pIcon, CAIRO_CONTAINER (pDock), 0))  // 0 <=> erase the current texture.
			return ;
		
		_cairo_dock_set_blend_alpha ();
		_cairo_dock_set_alpha (1.);
		_cairo_dock_enable_texture ();
		
		//\______________ On dessine les 3 ou 4 premieres icones du sous-dock.
		pRenderer->render_opengl (pIcon, CAIRO_CONTAINER (pDock), w, h);
		
		//\______________ On finit le dessin.
		_cairo_dock_disable_texture ();
		cairo_dock_end_draw_icon (pIcon, CAIRO_CONTAINER (pDock));
	}
	else if (pIcon->image.pSurface != NULL && pRenderer->render != NULL)  // dessin cairo
	{
		//\______________ On efface le dessin existant.
		cairo_t *pCairoContext = cairo_dock_begin_draw_icon_cairo (pIcon, 0, NULL);  // 0 <=> erase
		g_return_if_fail (pCairoContext != NULL);
		
		//\______________ On dessine les 3 ou 4 premieres icones du sous-dock.
		pRenderer->render (pIcon, CAIRO_CONTAINER (pDock), w, h, pCairoContext);
		
		//\______________ On finit le dessin.
		cairo_dock_end_draw_icon_cairo (pIcon);
		cairo_destroy (pCairoContext);
	}
}
