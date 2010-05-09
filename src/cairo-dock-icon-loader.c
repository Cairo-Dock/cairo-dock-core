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

#include "../config.h"
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
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-icon-loader.h"

#define CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME "default-icon-appli.svg"

extern CairoDock *g_pMainDock;

extern gchar *g_cCurrentThemePath;

extern CairoDockImageBuffer g_pIconBackgroundBuffer;

extern gboolean g_bUseOpenGL;

static void cairo_dock_load_launcher (Icon *icon, int iWidth, int iHeight);
static void cairo_dock_load_appli (Icon *icon, int iWidth, int iHeight);
static void cairo_dock_load_applet (Icon *icon, int iWidth, int iHeight);
static void cairo_dock_load_separator (Icon *icon, int iWidth, int iHeight);

const gchar *s_cRendererNames[4] = {NULL, "Emblem", "Stack", "Box"};  // c'est juste pour realiser la transition entre le chiffre en conf, et un nom (limitation du panneau de conf).

  //////////////
 /// LOADER ///
//////////////

void cairo_dock_fill_one_icon_buffer (Icon *icon, gdouble fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp)
{
	//g_print ("%s (%s, %d, %.2f, %s)\n", __func__, icon->cName, icon->iType, fMaxScale, icon->cFileName);
	if (cairo_dock_icon_is_being_removed (icon))  // si la fenetre est en train de se faire degager du dock, pas la peine de mettre a jour son icone. /// A voir pour les icones d'appli ...
	{
		cd_debug ("skip icon reload for %s", icon->cName);
		return;
	}
	
	//\______________ on reset les buffers (on garde la surface/texture actuelle pour les emblemes).
	cairo_surface_t *pPrevSurface = icon->pIconBuffer;
	GLuint iPrevTexture = icon->iIconTexture;
	
	if (icon->pReflectionBuffer != NULL)
	{
		cairo_surface_destroy (icon->pReflectionBuffer);
		icon->pReflectionBuffer = NULL;
	}
	
	if (icon->fWidth < 0 || icon->fHeight < 0)  // on ne veut pas de surface.
	{
		if (pPrevSurface != NULL)
			cairo_surface_destroy (pPrevSurface);
		icon->pIconBuffer = NULL;
		if (iPrevTexture != 0)
			_cairo_dock_delete_texture (iPrevTexture);
		icon->iIconTexture = 0;
		return;
	}
	
	int w, h;
	
	//\______________ on charge la surface/texture de l'icone.
	if (CAIRO_DOCK_IS_LAUNCHER (icon))  // c'est un lanceur.
	{
		if (CAIRO_DOCK_IS_FAKE_LAUNCHER (icon))
		{
			icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI];
			icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI];
		}
		else
		{
			icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
			icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER];
		}
		
		w = (bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale;
		h = (bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale;
		cairo_dock_load_launcher (icon, w, h);
	}
	else if (CAIRO_DOCK_IS_APPLET (icon))  // c'est l'icone d'une applet.
	{
		//g_print ("  icon->cFileName : %s\n", icon->cFileName);
		if (icon->fWidth == 0)
			icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLET];
		if (icon->fHeight == 0)
			icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLET];
		
		w = (bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale;
		h = (bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale;
		cairo_dock_load_applet (icon, w, h);
	}
	else if (CAIRO_DOCK_IS_APPLI (icon))  // c'est l'icone d'une appli valide. Dans cet ordre on n'a pas besoin de verifier que c'est NORMAL_APPLI.
	{
		icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI];
		icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI];
		
		w = (bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale;
		h = (bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale;
		cairo_dock_load_appli (icon, w, h);
	}
	else  // c'est une icone de separation.
	{
		icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12];
		icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12];
		
		w = (bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale;
		h = (bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale;
		cairo_dock_load_separator (icon, w, h);
	}

	//\______________ Si rien charge, on met une image par defaut.
	if (icon->pIconBuffer == pPrevSurface || icon->pIconBuffer == NULL)
	{
		gchar *cIconPath = cairo_dock_generate_file_path (CAIRO_DOCK_DEFAULT_ICON_NAME);
		if (cIconPath == NULL || ! g_file_test (cIconPath, G_FILE_TEST_EXISTS))
		{
			g_free (cIconPath);
			cIconPath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_ICON_NAME);
		}
		CairoDockIconType iType = cairo_dock_get_icon_type  (icon);
		/// A FAIRE : verifier qu'on peut enlever le test sur fMaxScale ...
		icon->fWidth = myIcons.tIconAuthorizedWidth[iType];
		icon->fHeight = myIcons.tIconAuthorizedHeight[iType];
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			w,
			h);
		g_free (cIconPath);
	}
	cd_debug ("%s (%s) -> %.2fx%.2f", __func__, icon->cName, icon->fWidth, icon->fHeight);
	
	//\_____________ On met le background de l'icone si necessaire
	if (icon->pIconBuffer != NULL &&
		g_pIconBackgroundBuffer.pSurface != NULL &&
		(! CAIRO_DOCK_IS_SEPARATOR (icon)/* && (myIcons.bBgForApplets || ! CAIRO_DOCK_IS_APPLET(pIcon))*/))
	{
		cairo_t *pCairoIconBGContext = cairo_create (icon->pIconBuffer);
		cairo_scale(pCairoIconBGContext,
			w / g_pIconBackgroundBuffer.iWidth,
			h / g_pIconBackgroundBuffer.iHeight);
		cairo_set_source_surface (pCairoIconBGContext,
			g_pIconBackgroundBuffer.pSurface,
			0.,
			0.);
		cairo_set_operator (pCairoIconBGContext, CAIRO_OPERATOR_DEST_OVER);
		cairo_paint (pCairoIconBGContext);
		cairo_destroy (pCairoIconBGContext);
	}
	
	//\______________ le reflet en mode cairo.
	if (! g_bUseOpenGL && myIcons.fAlbedo > 0 && icon->pIconBuffer != NULL && ! (CAIRO_DOCK_IS_APPLET (icon) && icon->cFileName == NULL))
	{
		icon->pReflectionBuffer = cairo_dock_create_reflection_surface (icon->pIconBuffer,
			w,
			h,
			myIcons.fReflectSize * fMaxScale,
			myIcons.fAlbedo,
			bIsHorizontal,
			bDirectionUp);
	}
	
	//\______________ on charge la texture si elle ne l'a pas ete.
	if (g_bUseOpenGL && (icon->iIconTexture == iPrevTexture || icon->iIconTexture == 0))
	{
		icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
	}
	
	if (pPrevSurface != NULL)
		cairo_surface_destroy (pPrevSurface);
	if (iPrevTexture != 0)
		_cairo_dock_delete_texture (iPrevTexture);
	//g_print ("%s (%s, %x, %.1fx%.1f) ->%d\n", __func__, icon->cName, icon->pIconBuffer, icon->fWidth, icon->fHeight, icon->iIconTexture);
}


gchar *cairo_dock_cut_string (const gchar *cString, int iNbCaracters)  // gere l'UTF-8
{
	g_return_val_if_fail (cString != NULL, NULL);
	gchar *cTruncatedName = NULL;
	gsize bytes_read, bytes_written;
	GError *erreur = NULL;
	gchar *cUtf8Name = g_locale_to_utf8 (cString,
		-1,
		&bytes_read,
		&bytes_written,
		&erreur);  // inutile sur Ubuntu, qui est nativement UTF8, mais sur les autres on ne sait pas.
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	if (cUtf8Name == NULL)  // une erreur s'est produite, on tente avec la chaine brute.
		cUtf8Name = g_strdup (cString);
	
	const gchar *cEndValidChain = NULL;
	int iStringLength;
	if (g_utf8_validate (cUtf8Name, -1, &cEndValidChain))
	{
		iStringLength = g_utf8_strlen (cUtf8Name, -1);
		int iNbChars = -1;
		if (iNbCaracters < 0)
		{
			iNbChars = MAX (0, iStringLength + iNbCaracters);
		}
		else if (iStringLength > iNbCaracters)
		{
			iNbChars = iNbCaracters;
		}
		
		if (iNbChars != -1)
		{
			cTruncatedName = g_new0 (gchar, 8 * (iNbChars + 4));  // 8 octets par caractere.
			if (iNbChars != 0)
				g_utf8_strncpy (cTruncatedName, cUtf8Name, iNbChars);
			
			gchar *cTruncature = g_utf8_offset_to_pointer (cTruncatedName, iNbChars);
			*cTruncature = '.';
			*(cTruncature+1) = '.';
			*(cTruncature+2) = '.';
		}
	}
	else
	{
		iStringLength = strlen (cString);
		int iNbChars = -1;
		if (iNbCaracters < 0)
		{
			iNbChars = MAX (0, iStringLength + iNbCaracters);
		}
		else if (iStringLength > iNbCaracters)
		{
			iNbChars = iNbCaracters;
		}
		
		if (iNbChars != -1)
		{
			cTruncatedName = g_new0 (gchar, iNbCaracters + 4);
			if (iNbChars != 0)
				strncpy (cTruncatedName, cString, iNbChars);
			
			cTruncatedName[iNbChars] = '.';
			cTruncatedName[iNbChars+1] = '.';
			cTruncatedName[iNbChars+2] = '.';
		}
	}
	if (cTruncatedName == NULL)
		cTruncatedName = cUtf8Name;
	else
		g_free (cUtf8Name);
	//g_print (" -> etiquette : %s\n", cTruncatedName);
	return cTruncatedName;
}

void cairo_dock_fill_one_text_buffer (Icon *icon, CairoDockLabelDescription *pTextDescription)
{
	//g_print ("%s (%s, %d)\n", __func__, cLabelPolice, iLabelSize);
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
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.iAppliMaxNameLength > 0)
	{
		cTruncatedName = cairo_dock_cut_string (icon->cName, myTaskBar.iAppliMaxNameLength);
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

void cairo_dock_fill_one_quick_info_buffer (Icon *icon, CairoDockLabelDescription *pTextDescription, double fMaxScale)
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

	double fQuickInfoXOffset, fQuickInfoYOffset;
	icon->pQuickInfoBuffer = cairo_dock_create_surface_from_text_full (icon->cQuickInfo,
		pTextDescription,
		fMaxScale,
		icon->fWidth * fMaxScale,
		&icon->iQuickInfoWidth, &icon->iQuickInfoHeight, NULL, NULL);
	
	if (g_bUseOpenGL && icon->pQuickInfoBuffer != NULL)
	{
		icon->iQuickInfoTexture = cairo_dock_create_texture_from_surface (icon->pQuickInfoBuffer);
	}
}



void cairo_dock_fill_icon_buffers (Icon *icon, double fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp)
{
	cairo_dock_fill_one_icon_buffer (icon, fMaxScale, bIsHorizontal, bDirectionUp);

	cairo_dock_fill_one_text_buffer (icon, &myLabels.iconTextDescription);

	cairo_dock_fill_one_quick_info_buffer (icon, &myLabels.quickInfoTextDescription, fMaxScale);
}

void cairo_dock_load_one_icon_from_scratch (Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (pIcon != NULL);
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		cairo_dock_fill_icon_buffers_for_dock (pIcon, pDock);
	}
	else
	{
		cairo_dock_fill_icon_buffers_for_desklet (pIcon);
	}
}

void cairo_dock_reload_buffers_in_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	gboolean bReloadAppletsToo = GPOINTER_TO_INT (data);
	cd_message ("%s (%s, %d)", __func__, cDockName, bReloadAppletsToo);

	double fFlatDockWidth = - myIcons.iIconGap;
	pDock->iMaxIconHeight = 0;
	Icon* icon;
	GList* ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		
		if (CAIRO_DOCK_IS_APPLET (icon))
		{
			if (bReloadAppletsToo)  /// modif du 23/05/2009 : utiliser la taille avec ratio ici. les applets doivent faire attention a utiliser la fonction get_icon_extent().
				cairo_dock_reload_module_instance (icon->pModuleInstance, FALSE);
		}
		else
		{
			icon->fWidth /= pDock->container.fRatio;
			icon->fHeight /= pDock->container.fRatio;
			cairo_dock_fill_icon_buffers_for_dock (icon, pDock);
			icon->fWidth *= pDock->container.fRatio;
			icon->fHeight *= pDock->container.fRatio;
		}
		
		//g_print (" =size <- %.2fx%.2f\n", icon->fWidth, icon->fHeight);
		fFlatDockWidth += myIcons.iIconGap + icon->fWidth;
		pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);
	}
	pDock->fFlatDockWidth = (int) fFlatDockWidth;  /// (int) n'est plus tellement necessaire ...
}

void cairo_dock_reload_one_icon_buffer_in_dock (Icon *icon, CairoDock *pDock)
{
	icon->fWidth /= pDock->container.fRatio;
	icon->fHeight /= pDock->container.fRatio;
	cairo_dock_fill_icon_buffers_for_dock (icon, pDock);
	icon->fWidth *= pDock->container.fRatio;
	icon->fHeight *= pDock->container.fRatio;
}


void cairo_dock_add_reflection_to_icon (Icon *pIcon, CairoContainer *pContainer)
{
	if (g_bUseOpenGL)
		return ;
	g_return_if_fail (pIcon != NULL && pContainer!= NULL);
	if (pIcon->pReflectionBuffer != NULL)
		cairo_surface_destroy (pIcon->pReflectionBuffer);
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
	pIcon->pReflectionBuffer = cairo_dock_create_reflection_surface (pIcon->pIconBuffer,
		iWidth,
		iHeight,
		myIcons.fReflectSize * cairo_dock_get_max_scale (pContainer),
		myIcons.fAlbedo,
		pContainer->bIsHorizontal,
		pContainer->bDirectionUp);
}


  ///////////////
 /// MANAGER ///
///////////////

void cairo_dock_load_icons_background_surface (const gchar *cImagePath, double fMaxScale)
{
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	
	int iSize = MAX (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI]);
	if (iSize == 0)
		iSize = 48;
	iSize *= fMaxScale;
	cairo_dock_load_image_buffer (&g_pIconBackgroundBuffer,
		cImagePath,
		iSize,
		iSize,
		CAIRO_DOCK_FILL_SPACE);
}

void cairo_dock_init_icon_manager (void)
{
	memset (&g_pIconBackgroundBuffer, 0, sizeof (CairoDockImageBuffer));
}

static void _load_renderer (const gchar *cRenderername, CairoIconContainerRenderer *pRenderer, gpointer data)
{
	if (pRenderer && pRenderer->load)
		pRenderer->load ();
}
void cairo_dock_load_icon_textures (void)
{
	double fMaxScale = cairo_dock_get_max_scale (g_pMainDock);
	
	cairo_dock_load_icons_background_surface (myIcons.cBackgroundImagePath, fMaxScale);
	
	cairo_dock_foreach_icon_container_renderer ((GHFunc)_load_renderer, NULL);
}

static void _unload_renderer (const gchar *cRenderername, CairoIconContainerRenderer *pRenderer, gpointer data)
{
	if (pRenderer && pRenderer->unload)
		pRenderer->unload ();
}
void cairo_dock_unload_icon_textures (void)
{
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	
	cairo_dock_foreach_icon_container_renderer ((GHFunc)_unload_renderer, NULL);
}


  ///////////////////////
 /// CONTAINER ICONS ///
///////////////////////

void cairo_dock_draw_subdock_content_on_icon (Icon *pIcon, CairoDock *pDock)
{
	g_return_if_fail (pIcon != NULL && pIcon->pSubDock != NULL && (pIcon->pIconBuffer != NULL || pIcon->iIconTexture != 0));
	
	CairoIconContainerRenderer *pRenderer = cairo_dock_get_icon_container_renderer (pIcon->cClass != NULL ? "Stack" : s_cRendererNames[pIcon->iSubdockViewType]);
	if (pRenderer == NULL)
		return;
	cd_debug ("%s (%s)\n", __func__, pIcon->cName);
	
	int w, h;
	cairo_dock_get_icon_extent (pIcon, CAIRO_CONTAINER (pDock), &w, &h);
	
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
		pRenderer->render_opengl (pIcon, w, h);
		
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
		pRenderer->render (pIcon, w, h, pCairoContext);
		
		//\______________ On finit le dessin.
		if (g_bUseOpenGL)
			cairo_dock_update_icon_texture (pIcon);
		else
			cairo_dock_add_reflection_to_icon (pIcon, CAIRO_CONTAINER (pDock));
		cairo_destroy (pCairoContext);
	}
}



  //////////////////////
 /// IMPLEMENTATION ///
//////////////////////

static void cairo_dock_load_launcher (Icon *icon, int iWidth, int iHeight)
{
	if (icon->pSubDock != NULL && (icon->iSubdockViewType != 0 || (icon->cClass != NULL && !myIndicators.bUseClassIndic)))  // icone de sous-dock avec un rendu specifique, on le redessinera lorsque les icones du sous-dock auront ete chargees.
	{
		icon->pIconBuffer = cairo_dock_create_blank_surface (iWidth, iHeight);
	}
	else if (icon->cFileName)  // icone possedant une image, on affiche celle-ci.
	{
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName);
		if (cIconPath != NULL && *cIconPath != '\0')
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			iWidth,
			iHeight);
		g_free (cIconPath);
	}
	else if (icon->pSubDock != NULL && icon->cClass != NULL && icon->cDesktopFileName == NULL)  // icone pointant sur une classe (epouvantail).
	{
		cd_debug ("c'est un epouvantail\n");
		icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass,
			iWidth,
			iHeight);
		if (icon->pIconBuffer == NULL)  // aucun inhibiteur ou aucune image correspondant a cette classe, on cherche a copier une des icones d'appli de cette classe.
		{
			const GList *pApplis = cairo_dock_list_existing_appli_with_class (icon->cClass);
			if (pApplis != NULL)
			{
				Icon *pOneIcon = (Icon *) (g_list_last ((GList*)pApplis)->data);  // on prend le dernier car les applis sont inserees a l'envers, et on veut avoir celle qui etait deja present dans le dock (pour 2 raison : continuite, et la nouvelle (en 1ere position) n'est pas forcement deja dans un dock, ce qui fausse le ratio).
				icon->pIconBuffer = cairo_dock_duplicate_inhibator_surface_for_appli (pOneIcon,
					iWidth,
					iHeight);
			}
		}
	}
}

static void cairo_dock_load_appli (Icon *icon, int iWidth, int iHeight)
{
	cairo_surface_t *pPrevSurface = icon->pIconBuffer;
	GLuint iPrevTexture = icon->iIconTexture;
	icon->pIconBuffer = NULL;
	icon->iIconTexture = 0;
	//g_print ("%s (%dx%d / %ld)\n", __func__, iWidth, iHeight, icon->iBackingPixmap);
	if (myTaskBar.iMinimizedWindowRenderType == 1 && icon->bIsHidden && icon->iBackingPixmap != 0)
	{
		// on cree la miniature.
		if (g_bUseOpenGL)
		{
			icon->iIconTexture = cairo_dock_texture_from_pixmap (icon->Xid, icon->iBackingPixmap);
			//g_print ("opengl thumbnail : %d\n", icon->iIconTexture);
		}
		if (icon->iIconTexture == 0)
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_xpixmap (icon->iBackingPixmap,
				iWidth,
				iHeight);
			if (g_bUseOpenGL)
				icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
		}
		// on affiche l'image precedente en embleme.
		if (icon->iIconTexture != 0 && iPrevTexture != 0)
		{
			CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			if (pParentDock)  // le dessin de l'embleme utilise cairo_dock_get_icon_extent()
			{
				icon->fWidth *= pParentDock->container.fRatio;
				icon->fHeight *= pParentDock->container.fRatio;
			}
			CairoEmblem *e = cairo_dock_make_emblem_from_texture (iPrevTexture,icon, CAIRO_CONTAINER (pParentDock));
			cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
			cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
			g_free (e);  // on n'utilise pas cairo_dock_free_emblem pour ne pas detruire la texture avec.
			if (pParentDock)
			{
				icon->fWidth /= pParentDock->container.fRatio;
				icon->fHeight /= pParentDock->container.fRatio;
			}
		}
		else if (icon->pIconBuffer != NULL && pPrevSurface != NULL)
		{
			CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			if (pParentDock)  // le dessin de l'embleme utilise cairo_dock_get_icon_extent()
			{
				icon->fWidth *= pParentDock->container.fRatio;
				icon->fHeight *= pParentDock->container.fRatio;
			}
			CairoEmblem *e = cairo_dock_make_emblem_from_surface (pPrevSurface, 0, 0, icon, CAIRO_CONTAINER (pParentDock));
			cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
			cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
			g_free (e);  // meme remarque.
			if (pParentDock)
			{
				icon->fWidth /= pParentDock->container.fRatio;
				icon->fHeight /= pParentDock->container.fRatio;
			}
		}
	}
	if (icon->pIconBuffer == NULL && myTaskBar.bOverWriteXIcons && ! cairo_dock_class_is_using_xicon (icon->cClass))
		icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass, iWidth, iHeight);
	if (icon->pIconBuffer == NULL)
		icon->pIconBuffer = cairo_dock_create_surface_from_xwindow (icon->Xid, iWidth, iHeight);
	if (icon->pIconBuffer == NULL)  // certaines applis comme xterm ne definissent pas d'icone, on en met une par defaut.
	{
		cd_debug ("%s (%ld) doesn't define any icon, we set the default one.\n", icon->cName, icon->Xid);
		gchar *cIconPath = cairo_dock_generate_file_path (CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
		if (cIconPath == NULL || ! g_file_test (cIconPath, G_FILE_TEST_EXISTS))
		{
			g_free (cIconPath);
			cIconPath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
		}
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			iWidth,
			iHeight);
		g_free (cIconPath);
	}
}

static void cairo_dock_load_applet (Icon *icon, int iWidth, int iHeight)
{
	icon->pIconBuffer = cairo_dock_create_applet_surface (icon->cFileName,
		iWidth,
		iHeight);
}

static void cairo_dock_load_separator (Icon *icon, int iWidth, int iHeight)
{
	if (CAIRO_DOCK_IS_USER_SEPARATOR (icon) && icon->cFileName != NULL)
	{
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName);
		if (cIconPath != NULL && *cIconPath != '\0')
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
				iWidth,
				iHeight);
		}
		g_free (cIconPath);
	}
	else
	{
		icon->pIconBuffer = cairo_dock_create_separator_surface (
			iWidth,
			iHeight);
	}
}
