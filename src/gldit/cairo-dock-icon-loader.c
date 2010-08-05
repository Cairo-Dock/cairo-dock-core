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

CairoDockImageBuffer g_pIconBackgroundBuffer;

extern CairoDock *g_pMainDock;
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;

const gchar *s_cRendererNames[4] = {NULL, "Emblem", "Stack", "Box"};  // c'est juste pour realiser la transition entre le chiffre en conf, et un nom (limitation du panneau de conf). On garde le numero pour savoir rapidement sur laquelle on set.

  //////////////
 /// LOADER ///
//////////////

gchar *cairo_dock_search_icon_s_path (const gchar *cFileName)
{
	g_return_val_if_fail (cFileName != NULL, NULL);
	
	//\_______________________ cas faciles : l'entree est deja un chemin.
	if (*cFileName == '~')
	{
		return g_strdup_printf ("%s%s", g_getenv ("HOME"), cFileName+1);
	}
	
	if (*cFileName == '/')
	{
		return g_strdup (cFileName);
	}
	
	//\_______________________ On determine si le suffixe et une version sont presents ou non.
	g_return_val_if_fail (myIcons.pDefaultIconDirectory != NULL, NULL);
	
	GString *sIconPath = g_string_new ("");
	const gchar *cSuffixTab[4] = {".svg", ".png", ".xpm", NULL};
	gboolean bAddSuffix=FALSE, bFileFound=FALSE, bHasVersion=FALSE;
	GtkIconInfo* pIconInfo = NULL;
	int i, j;
	gchar *str = strrchr (cFileName, '.');
	bAddSuffix = (str == NULL || ! g_ascii_isalpha (*(str+1)));  // exemple : firefox-3.0
	bHasVersion = (str != NULL && g_ascii_isdigit (*(str+1)) && g_ascii_isdigit (*(str-1)) && str-1 != cFileName);  // doit finir par x.y, x et y ayant autant de chiffres que l'on veut.
	
	//\_______________________ On parcourt les themes disponibles, en testant tous les suffixes connus.
	for (i = 0; i < myIcons.iNbIconPlaces && ! bFileFound; i ++)
	{
		if (myIcons.pDefaultIconDirectory[2*i] != NULL)  // repertoire.
		{
			//g_print ("on recherche %s dans le repertoire %s\n", sIconPath->str, myIcons.pDefaultIconDirectory[2*i]);
			j = 0;
			while (! bFileFound && (cSuffixTab[j] != NULL || ! bAddSuffix))
			{
				g_string_printf (sIconPath, "%s/%s", (gchar *)myIcons.pDefaultIconDirectory[2*i], cFileName);
				if (bAddSuffix)
					g_string_append_printf (sIconPath, "%s", cSuffixTab[j]);
				//g_print ("  -> %s\n", sIconPath->str);
				if ( g_file_test (sIconPath->str, G_FILE_TEST_EXISTS) )
					bFileFound = TRUE;
				j ++;
				if (! bAddSuffix)
					break;
			}
		}
		else  // theme d'icones.
		{
			g_string_assign (sIconPath, cFileName);
			if (! bAddSuffix)  // on vire le suffixe pour chercher tous les formats dans le theme d'icones.
			{
				gchar *str = strrchr (sIconPath->str, '.');
				if (str != NULL)
					*str = '\0';
			}
			//g_print ("on recherche %s dans le theme d'icones\n", sIconPath->str);
			GtkIconTheme *pIconTheme;
			if (myIcons.pDefaultIconDirectory[2*i+1] != NULL)
				pIconTheme = myIcons.pDefaultIconDirectory[2*i+1];
			else
				pIconTheme = gtk_icon_theme_get_default ();
			pIconInfo = gtk_icon_theme_lookup_icon  (GTK_ICON_THEME (pIconTheme),
				sIconPath->str,
				128,
				GTK_ICON_LOOKUP_FORCE_SVG);
			if (pIconInfo != NULL)
			{
				g_string_assign (sIconPath, gtk_icon_info_get_filename (pIconInfo));
				bFileFound = TRUE;
				gtk_icon_info_free (pIconInfo);
			}
		}
	}
	
	//\_______________________ si rien trouve, on cherche sans le numero de version.
	if (! bFileFound && bHasVersion)
	{
		cd_debug ("on cherche sans le numero de version...");
		g_string_assign (sIconPath, cFileName);
		gchar *str = strrchr (sIconPath->str, '.');
		str --;  // on sait que c'est un digit.
		str --;
		while ((g_ascii_isdigit (*str) || *str == '.' || *str == '-') && (str != sIconPath->str))
			str --;
		if (str != sIconPath->str)
		{
			*(str+1) = '\0';
			cd_debug (" on cherche '%s'...\n", sIconPath->str);
			gchar *cPath = cairo_dock_search_icon_s_path (sIconPath->str);
			if (cPath != NULL)
			{
				bFileFound = TRUE;
				g_string_assign (sIconPath, cPath);
				g_free (cPath);
			}
		}
	}
	
	if (! bFileFound)
	{
		g_string_free (sIconPath, TRUE);
		return NULL;
	}
	
	gchar *cIconPath = sIconPath->str;
	g_string_free (sIconPath, FALSE);
	return cIconPath;
}

static void _set_icon_size_generic (CairoContainer *pContainer, Icon *icon)
{
	if (icon->fWidth == 0)
		icon->fWidth = 48;
	if (icon->fHeight == 0)
		icon->fHeight = 48;
}
void cairo_dock_set_icon_size (CairoContainer *pContainer, Icon *icon)
{
	if (! pContainer)
	{
		///pContainer = CAIRO_CONTAINER (g_pMainDock);
		g_print ("icone dans aucun container => pas chargee\n");
		return;
	}
	// taille de l'icone dans le container (hors ratio).
	if (pContainer->iface.set_icon_size)
		pContainer->iface.set_icon_size (pContainer, icon);
	else
		_set_icon_size_generic (pContainer, icon);
	// la taille que devra avoir la texture s'en deduit.
	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	icon->iImageWidth = (pContainer->bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale;
	icon->iImageHeight = (pContainer->bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale;
}

void cairo_dock_load_icon_image (Icon *icon, CairoContainer *pContainer)
{
	if (icon->fWidth < 0 || icon->fHeight < 0)  // on ne veut pas de surface.
	{
		if (icon->pIconBuffer != NULL)
			cairo_surface_destroy (icon->pIconBuffer);
		icon->pIconBuffer = NULL;
		if (icon->iIconTexture != 0)
			_cairo_dock_delete_texture (icon->iIconTexture);
		icon->iIconTexture = 0;
		if (icon->pReflectionBuffer != NULL)
			cairo_surface_destroy (icon->pReflectionBuffer);
		icon->pReflectionBuffer = NULL;
		return;
	}
	
	if (icon->fWidth == 0 || icon->iImageWidth <= 0)
	{
		cairo_dock_set_icon_size (pContainer, icon);
	}
	//g_print ("%s (%.2fx%.2f ; %dx%d)\n", __func__, icon->fWidth, icon->fHeight, icon->iImageWidth, icon->iImageHeight);
	
	//\______________ on reset les buffers (on garde la surface/texture actuelle pour les emblemes).
	cairo_surface_t *pPrevSurface = icon->pIconBuffer;
	GLuint iPrevTexture = icon->iIconTexture;
	
	if (icon->pReflectionBuffer != NULL)
	{
		cairo_surface_destroy (icon->pReflectionBuffer);
		icon->pReflectionBuffer = NULL;
	}
	
	//\______________ on charge la surface/texture.
	if (icon->iface.load_image)
		icon->iface.load_image (icon);
	
	//\______________ Si rien charge, on met une image par defaut.
	if ((icon->pIconBuffer == pPrevSurface || icon->pIconBuffer == NULL) &&
		(icon->iIconTexture == iPrevTexture || icon->iIconTexture == 0))
	{
		gchar *cIconPath = cairo_dock_generate_file_path (CAIRO_DOCK_DEFAULT_ICON_NAME);
		if (cIconPath == NULL || ! g_file_test (cIconPath, G_FILE_TEST_EXISTS))
		{
			g_free (cIconPath);
			cIconPath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_ICON_NAME);
		}
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			icon->iImageWidth,
			icon->iImageHeight);
		g_free (cIconPath);
	}
	cd_debug ("%s (%s) -> %.2fx%.2f", __func__, icon->cName, icon->fWidth, icon->fHeight);
	
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
	if (! g_bUseOpenGL && myIcons.fAlbedo > 0 && icon->pIconBuffer != NULL && ! (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && icon->cFileName == NULL))
	{
		icon->pReflectionBuffer = cairo_dock_create_reflection_surface (icon->pIconBuffer,
			icon->iImageWidth,
			icon->iImageHeight,
			myIcons.fReflectSize * cairo_dock_get_max_scale (pContainer),
			myIcons.fAlbedo,
			pContainer ? pContainer->bIsHorizontal : TRUE,
			pContainer ? pContainer->bDirectionUp : TRUE);
	}
	
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

void cairo_dock_load_icon_quickinfo (Icon *icon, CairoDockLabelDescription *pTextDescription, double fMaxScale)
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


void cairo_dock_load_icon_buffers (Icon *pIcon, CairoContainer *pContainer)
{
	if (pIcon->iSidLoadImage != 0)
	{
		g_source_remove (pIcon->iSidLoadImage);
		pIcon->iSidLoadImage = 0;
	}
	
	cairo_dock_load_icon_image (pIcon, pContainer);

	cairo_dock_load_icon_text (pIcon, &myLabels.iconTextDescription);

	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	cairo_dock_load_icon_quickinfo (pIcon, &myLabels.quickInfoTextDescription, fMaxScale);
}

static gboolean _load_icon_buffer_idle (Icon *pIcon)
{
	//g_print ("%s (%s; %dx%d; %.2fx%.2f; %x)\n", __func__, pIcon->cName, pIcon->iImageWidth, pIcon->iImageHeight, pIcon->fWidth, pIcon->fHeight, pIcon->pContainerForLoad);
	pIcon->iSidLoadImage = 0;
	
	CairoContainer *pContainer = pIcon->pContainerForLoad;
	if (pContainer)
	{
		cairo_dock_load_icon_image (pIcon, pContainer);

		double fMaxScale = cairo_dock_get_max_scale (pContainer);
		cairo_dock_load_icon_quickinfo (pIcon, &myLabels.quickInfoTextDescription, fMaxScale);
		
		cairo_dock_redraw_icon (pIcon, pContainer);
	}
	return FALSE;
}
void cairo_dock_trigger_load_icon_buffers (Icon *pIcon, CairoContainer *pContainer)
{
	cairo_dock_set_icon_size (pContainer, pIcon);
	pIcon->pContainerForLoad = pContainer;
	if (pIcon->iSidLoadImage == 0)
	{
		//g_print ("trigger load for %s (%x)\n", pIcon->cName, pContainer);
		//cairo_dock_load_icon_buffers (pIcon, pContainer);
		cairo_dock_load_icon_text (pIcon, &myLabels.iconTextDescription);  // la vue peut avoir besoin de connaitre la taille du texte.
		pIcon->iSidLoadImage = g_idle_add ((GSourceFunc)_load_icon_buffer_idle, pIcon);
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
			///cairo_dock_set_icon_size (CAIRO_CONTAINER (pDock), icon);
			cairo_dock_trigger_load_icon_buffers (icon, CAIRO_CONTAINER (pDock));  // fait un set_icon_size
			icon->fWidth *= pDock->container.fRatio;
			icon->fHeight *= pDock->container.fRatio;
		}
		
		//g_print (" =size <- %.2fx%.2f\n", icon->fWidth, icon->fHeight);
		fFlatDockWidth += myIcons.iIconGap + icon->fWidth;
		if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
			pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);
	}
	pDock->fFlatDockWidth = (int) fFlatDockWidth;  /// (int) n'est plus tellement necessaire ...
}

void cairo_dock_reload_icon_image (Icon *icon, CairoContainer *pContainer)
{
	if (pContainer)
	{
		icon->fWidth /= pContainer->fRatio;
		icon->fHeight /= pContainer->fRatio;
	}
	cairo_dock_load_icon_image (icon, pContainer);
	if (pContainer)
	{
		icon->fWidth *= pContainer->fRatio;
		icon->fHeight *= pContainer->fRatio;
	}
}

void cairo_dock_add_reflection_to_icon (Icon *pIcon, CairoContainer *pContainer)
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
	cd_debug ("%s (%s)", __func__, pIcon->cName);
	
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
		else
			cairo_dock_add_reflection_to_icon (pIcon, CAIRO_CONTAINER (pDock));
		cairo_destroy (pCairoContext);
	}
}
