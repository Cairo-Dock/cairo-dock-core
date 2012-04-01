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
#include "cairo-dock-module-factory.h"  // cairo_dock_reload_module_instance
#include "cairo-dock-log.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.iAppliMaxNameLength
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-dock-manager.h"  // cairo_dock_synchronize_one_sub_dock_orientation
#include "cairo-dock-backends-manager.h"  // cairo_dock_get_icon_container_renderer
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-overlay.h"
#include "cairo-dock-icon-factory.h"

CairoDockImageBuffer g_pIconBackgroundBuffer;

extern CairoDock *g_pMainDock;
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;

const gchar *s_cRendererNames[4] = {NULL, "Emblem", "Stack", "Box"};  // c'est juste pour realiser la transition entre le chiffre en conf, et un nom (limitation du panneau de conf). On garde le numero pour savoir rapidement sur laquelle on set.


inline Icon *cairo_dock_new_icon (void)
{
	Icon *_icon = g_new0 (Icon, 1);
	///cairo_dock_install_notifications_on_object (_icon, NB_NOTIFICATIONS_ICON);
	_icon->object.ref = 1;
	gldi_object_set_manager (GLDI_OBJECT (_icon), GLDI_MANAGER (&myIconsMgr));
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
	
	cairo_surface_destroy (icon->pIconBuffer);
	///cairo_surface_destroy (icon->pReflectionBuffer);
	cairo_surface_destroy (icon->pTextBuffer);
	cairo_surface_destroy (icon->pQuickInfoBuffer);
	
	if (icon->iIconTexture != 0)
		_cairo_dock_delete_texture (icon->iIconTexture);
	if (icon->iLabelTexture != 0)
		_cairo_dock_delete_texture (icon->iLabelTexture);
	if (icon->iQuickInfoTexture != 0)
		_cairo_dock_delete_texture (icon->iQuickInfoTexture);
	cairo_dock_destroy_icon_overlays (icon);
}

  //////////////
 /// LOADER ///
//////////////


void cairo_dock_load_icon_image (Icon *icon, CairoContainer *pContainer)
{
	if (icon->pContainer == NULL)
	{
		//g_print ("/!\\ Icon %s is not inside a container !!!\n", icon->cName);
		return;
	}
	CairoDockModuleInstance *pInstance = icon->pModuleInstance;  // this is the only function where we destroy/create the icon's surface, so we must handle the cairo-context here.
	if (pInstance && pInstance->pDrawContext != NULL)
	{
		cairo_destroy (pInstance->pDrawContext);
		pInstance->pDrawContext = NULL;
	}
	
	//g_print ("%s (%s, %dx%d)\n", __func__, icon->cName, (int)icon->fWidth, (int)icon->fHeight);
	if (icon->fWidth < 0 || icon->fHeight < 0)  // on ne veut pas de surface.
	{
		if (icon->pIconBuffer != NULL)
			cairo_surface_destroy (icon->pIconBuffer);
		icon->pIconBuffer = NULL;
		if (icon->iIconTexture != 0)
			_cairo_dock_delete_texture (icon->iIconTexture);
		icon->iIconTexture = 0;
		/**if (icon->pReflectionBuffer != NULL)
			cairo_surface_destroy (icon->pReflectionBuffer);
		icon->pReflectionBuffer = NULL;*/
		return;
	}
	
	g_return_if_fail (icon->fWidth > 0 & icon->iImageWidth > 0);  // the renderer of the container must have set the size beforehand, when the icon has been inserted into the container.
	
	//\______________ on reset les buffers (on garde la surface/texture actuelle pour les emblemes).
	cairo_surface_t *pPrevSurface = icon->pIconBuffer;
	GLuint iPrevTexture = icon->iIconTexture;
	
	/**if (icon->pReflectionBuffer != NULL)
	{
		cairo_surface_destroy (icon->pReflectionBuffer);
		icon->pReflectionBuffer = NULL;
	}*/
	
	//\______________ on charge la surface/texture.
	if (icon->iface.load_image)
		icon->iface.load_image (icon);
	
	//\______________ Si rien charge, on met une image par defaut.
	if ((icon->pIconBuffer == pPrevSurface || icon->pIconBuffer == NULL) &&
		(icon->iIconTexture == iPrevTexture || icon->iIconTexture == 0))
	{
		gchar *cIconPath = cairo_dock_search_image_s_path (CAIRO_DOCK_DEFAULT_ICON_NAME);
		if (cIconPath == NULL)  // fichier non trouve.
		{
			cIconPath = g_strdup (GLDI_SHARE_DATA_DIR"/icons/"CAIRO_DOCK_DEFAULT_ICON_NAME);
		}
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			icon->iImageWidth,
			icon->iImageHeight);
		g_free (cIconPath);
	}
	
	//\_____________ On met le background de l'icone si necessaire
	if (icon->pIconBuffer != NULL && g_pIconBackgroundBuffer.pSurface != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		cairo_t *pCairoIconBGContext = cairo_create (icon->pIconBuffer);
		cairo_scale(pCairoIconBGContext,
			icon->iImageWidth / g_pIconBackgroundBuffer.iWidth,
			icon->iImageHeight / g_pIconBackgroundBuffer.iHeight);
		cairo_set_source_surface (pCairoIconBGContext,
			g_pIconBackgroundBuffer.pSurface,
			0.,
			0.);
		cairo_set_operator (pCairoIconBGContext, CAIRO_OPERATOR_DEST_OVER);
		cairo_paint (pCairoIconBGContext);
		cairo_destroy (pCairoIconBGContext);
	}
	
	//\______________ le reflet en mode cairo.
	/**if (! g_bUseOpenGL && myIconsParam.fAlbedo > 0 && icon->pIconBuffer != NULL && ! (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && icon->cFileName == NULL))
	{
		cairo_dock_add_reflection_to_icon (icon, pContainer);
	}*/
	
	//\______________ on charge la texture si elle ne l'a pas ete.
	if (g_bUseOpenGL && (icon->iIconTexture == iPrevTexture || icon->iIconTexture == 0))
	{
		icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
	}
	
	//\______________ on libere maintenant les anciennes ressources.
	if (pPrevSurface != NULL)
		cairo_surface_destroy (pPrevSurface);
	if (iPrevTexture != 0)
		_cairo_dock_delete_texture (iPrevTexture);
	
	if (pInstance && icon->pIconBuffer != NULL)
	{
		pInstance->pDrawContext = cairo_create (icon->pIconBuffer);
		if (!pInstance->pDrawContext || cairo_status (pInstance->pDrawContext) != CAIRO_STATUS_SUCCESS)
		{
			cd_warning ("couldn't initialize drawing context, applet won't be able to draw itself !");
			pInstance->pDrawContext = NULL;
		}
	}
}

void cairo_dock_load_icon_text (Icon *icon, CairoDockLabelDescription *pTextDescription)
{
	cairo_surface_destroy (icon->pTextBuffer);
	icon->pTextBuffer = NULL;
	if (icon->iLabelTexture != 0)
	{
		_cairo_dock_delete_texture (icon->iLabelTexture);
		icon->iLabelTexture = 0;
	}
	if (icon->cName == NULL || (pTextDescription->iSize == 0))
		return ;

	gchar *cTruncatedName = NULL;
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskbarParam.iAppliMaxNameLength > 0)
	{
		cTruncatedName = cairo_dock_cut_string (icon->cName, myTaskbarParam.iAppliMaxNameLength);
	}
	
	double fTextXOffset, fTextYOffset;
	cairo_surface_t* pNewSurface = cairo_dock_create_surface_from_text ((cTruncatedName != NULL ? cTruncatedName : icon->cName),
		pTextDescription,
		&icon->iTextWidth, &icon->iTextHeight);
	g_free (cTruncatedName);
	//g_print (" -> %s : (%.2f;%.2f) %dx%d\n", icon->cName, icon->fTextXOffset, icon->fTextYOffset, icon->iTextWidth, icon->iTextHeight);

	icon->pTextBuffer = pNewSurface;
	
	if (g_bUseOpenGL && icon->pTextBuffer != NULL)
	{
		icon->iLabelTexture = cairo_dock_create_texture_from_surface (icon->pTextBuffer);
	}
}

void cairo_dock_load_icon_quickinfo (Icon *icon, CairoDockLabelDescription *pTextDescription)
{
	cairo_surface_destroy (icon->pQuickInfoBuffer);
	icon->pQuickInfoBuffer = NULL;
	if (icon->iQuickInfoTexture != 0)
	{
		_cairo_dock_delete_texture (icon->iQuickInfoTexture);
		icon->iQuickInfoTexture = 0;
	}
	if (icon->cQuickInfo == NULL)
		return ;

	double fMaxScale = cairo_dock_get_icon_max_scale (icon);
	icon->pQuickInfoBuffer = cairo_dock_create_surface_from_text_full (icon->cQuickInfo,
		pTextDescription,
		fMaxScale,
		icon->fWidth * fMaxScale,
		&icon->iQuickInfoWidth, &icon->iQuickInfoHeight);
	
	if (g_bUseOpenGL && icon->pQuickInfoBuffer != NULL)
	{
		icon->iQuickInfoTexture = cairo_dock_create_texture_from_surface (icon->pQuickInfoBuffer);
	}
}


void cairo_dock_load_icon_buffers (Icon *pIcon, CairoContainer *pContainer)
{
	if (pIcon->iSidLoadImage != 0)  // if a load was sheduled, cancel it and do it now (we need to load the applets' buffer before initializing the module).
	{
		//g_print (" load %s immediately\n", pIcon->cName);
		g_source_remove (pIcon->iSidLoadImage);
		pIcon->iSidLoadImage = 0;
	}
	
	cairo_dock_load_icon_image (pIcon, pContainer);

	cairo_dock_load_icon_text (pIcon, &myIconsParam.iconTextDescription);

	cairo_dock_load_icon_quickinfo (pIcon, &myIconsParam.quickInfoTextDescription);
}

static gboolean _load_icon_buffer_idle (Icon *pIcon)
{
	//g_print ("%s (%s; %dx%d; %.2fx%.2f; %x)\n", __func__, pIcon->cName, pIcon->iImageWidth, pIcon->iImageHeight, pIcon->fWidth, pIcon->fHeight, pIcon->pContainer);
	pIcon->iSidLoadImage = 0;
	
	CairoContainer *pContainer = pIcon->pContainer;
	if (pContainer)
	{
		cairo_dock_load_icon_image (pIcon, pContainer);
		
		cairo_dock_load_icon_quickinfo (pIcon, &myIconsParam.quickInfoTextDescription);
		
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
		//g_print ("trigger load for %s (%x)\n", pIcon->cName, pContainer);
		if (!pIcon->pTextBuffer)
			cairo_dock_load_icon_text (pIcon, &myIconsParam.iconTextDescription);  // la vue peut avoir besoin de connaitre la taille du texte.
		pIcon->iSidLoadImage = g_idle_add ((GSourceFunc)_load_icon_buffer_idle, pIcon);
	}
}


void cairo_dock_reload_icon_image (Icon *icon, CairoContainer *pContainer)
{
	/**if (pContainer)
	{
		icon->fWidth /= pContainer->fRatio;
		icon->fHeight /= pContainer->fRatio;
	}*/
	cairo_dock_load_icon_image (icon, pContainer);
	/**if (pContainer)
	{
		icon->fWidth *= pContainer->fRatio;
		icon->fHeight *= pContainer->fRatio;
	}*/
}

/**void cairo_dock_add_reflection_to_icon (Icon *pIcon, CairoContainer *pContainer)
{
	if (g_bUseOpenGL)
		return ;
	g_return_if_fail (pIcon != NULL && pContainer!= NULL);
	
	if (pIcon->pReflectionBuffer != NULL)
	{
		cairo_surface_destroy (pIcon->pReflectionBuffer);
		pIcon->pReflectionBuffer = NULL;
	}
	if (! pContainer->bUseReflect)
		return;
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
	pIcon->pReflectionBuffer = cairo_dock_create_reflection_surface (pIcon->pIconBuffer,
		iWidth,
		iHeight,
		myIconsParam.fReflectHeightRatio * iHeight,
		///myIconsParam.fReflectSize * cairo_dock_get_max_scale (pContainer),
		myIconsParam.fAlbedo,
		pContainer->bIsHorizontal,
		pContainer->bDirectionUp);
}*/


  ///////////////////////
 /// CONTAINER ICONS ///
///////////////////////

void cairo_dock_draw_subdock_content_on_icon (Icon *pIcon, CairoDock *pDock)
{
	g_return_if_fail (pIcon != NULL && pIcon->pSubDock != NULL && (pIcon->pIconBuffer != NULL || pIcon->iIconTexture != 0));
	
	CairoIconContainerRenderer *pRenderer = cairo_dock_get_icon_container_renderer (pIcon->cClass != NULL ? "Stack" : s_cRendererNames[pIcon->iSubdockViewType]);
	if (pRenderer == NULL)
		return;
	cd_debug ("%s (%s)", __func__, pIcon->cName);
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, &w, &h);
	
	cairo_t *pCairoContext = NULL;
	if (pIcon->iIconTexture != 0 && pRenderer->render_opengl)  // dessin opengl
	{
		//\______________ On efface le dessin existant.
		if (! cairo_dock_begin_draw_icon (pIcon, CAIRO_CONTAINER (pDock), 0))
			return ;
		
		_cairo_dock_set_blend_source ();
		if (g_pIconBackgroundBuffer.iTexture != 0)  // on ecrase le dessin existant avec l'image de fond des icones.
		{
			_cairo_dock_enable_texture ();
			_cairo_dock_set_alpha (1.);
			_cairo_dock_apply_texture_at_size (g_pIconBackgroundBuffer.iTexture, w, h);
		}
		else  // sinon on efface juste ce qu'il y'avait.
		{
			glPolygonMode (GL_FRONT, GL_FILL);
			_cairo_dock_set_alpha (0.);
			glBegin(GL_QUADS);
			glVertex3f(-.5*w,  .5*h, 0.);
			glVertex3f( .5*w,  .5*h, 0.);
			glVertex3f( .5*w, -.5*h, 0.);
			glVertex3f(-.5*w, -.5*h, 0.);
			glEnd();
			_cairo_dock_enable_texture ();
			_cairo_dock_set_alpha (1.);
		}
		_cairo_dock_set_blend_alpha ();
		
		//\______________ On dessine les 3 ou 4 premieres icones du sous-dock.
		pRenderer->render_opengl (pIcon, CAIRO_CONTAINER (pDock), w, h);
		
		//\______________ On finit le dessin.
		_cairo_dock_disable_texture ();
		cairo_dock_end_draw_icon (pIcon, CAIRO_CONTAINER (pDock));
	}
	else if (pIcon->pIconBuffer != NULL && pRenderer->render != NULL)  // dessin cairo
	{
		//\______________ On efface le dessin existant.
		pCairoContext = cairo_create (pIcon->pIconBuffer);
		g_return_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
		
		if (g_pIconBackgroundBuffer.pSurface != NULL)  // on ecrase le dessin existant avec l'image de fond des icones.
		{
			cairo_save (pCairoContext);
			cairo_scale(pCairoContext,
				(double) w / g_pIconBackgroundBuffer.iWidth,
				(double) h / g_pIconBackgroundBuffer.iHeight);
			cairo_set_source_surface (pCairoContext,
				g_pIconBackgroundBuffer.pSurface,
				0.,
				0.);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
			cairo_paint (pCairoContext);
			cairo_restore (pCairoContext);
		}
		else  // sinon on efface juste ce qu'il y'avait.
		{
			cairo_dock_erase_cairo_context (pCairoContext);
		}
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
		
		//\______________ On dessine les 3 ou 4 premieres icones du sous-dock.
		pRenderer->render (pIcon, CAIRO_CONTAINER (pDock), w, h, pCairoContext);
		
		//\______________ On finit le dessin.
		if (g_bUseOpenGL)
			cairo_dock_update_icon_texture (pIcon);
		/**else
			cairo_dock_add_reflection_to_icon (pIcon, CAIRO_CONTAINER (pDock));*/
		cairo_destroy (pCairoContext);
	}
}
