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

#include "cairo-dock-draw.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-application-factory.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-applet-factory.h"
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
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-data-renderer.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-load.h"

extern CairoDock *g_pMainDock;

extern gint g_iXScreenWidth[2];  // change tous les g_iScreen par g_iXScreen le 28/07/2009
extern gint g_iXScreenHeight[2];

extern gchar *g_cCurrentThemePath;

extern CairoDockImageBuffer g_pDockBackgroundBuffer;
extern CairoDockImageBuffer g_pVisibleZoneBuffer;
extern CairoDockImageBuffer g_pIndicatorBuffer;
extern CairoDockImageBuffer g_pActiveIndicatorBuffer;
extern CairoDockImageBuffer g_pClassIndicatorBuffer;
extern CairoDockImageBuffer g_pIconBackgroundBuffer;
extern CairoDockImageBuffer g_pBoxAboveBuffer;
extern CairoDockImageBuffer g_pBoxBelowBuffer;

extern GLuint g_pGradationTexture[2];

extern gboolean g_bUseOpenGL;

static CairoDockDesktopBackground *s_pDesktopBg = NULL;
static cairo_t *s_pSourceContext = NULL;

void cairo_dock_free_label_description (CairoDockLabelDescription *pTextDescription)
{
	if (pTextDescription == NULL)
		return ;
	g_free (pTextDescription->cFont);
	g_free (pTextDescription);
}

void cairo_dock_copy_label_description (CairoDockLabelDescription *pDestTextDescription, CairoDockLabelDescription *pOrigTextDescription)
{
	g_return_if_fail (pOrigTextDescription != NULL && pDestTextDescription != NULL);
	memcpy (pDestTextDescription, pOrigTextDescription, sizeof (CairoDockLabelDescription));
	pDestTextDescription->cFont = g_strdup (pOrigTextDescription->cFont);
}

CairoDockLabelDescription *cairo_dock_duplicate_label_description (CairoDockLabelDescription *pOrigTextDescription)
{
	g_return_val_if_fail (pOrigTextDescription != NULL, NULL);
	CairoDockLabelDescription *pTextDescription = g_memdup (pOrigTextDescription, sizeof (CairoDockLabelDescription));
	pTextDescription->cFont = g_strdup (pOrigTextDescription->cFont);
	return pTextDescription;
}

gchar *cairo_dock_generate_file_path (const gchar *cImageFile)
{
	g_return_val_if_fail (cImageFile != NULL, NULL);
	gchar *cImagePath;
	if (*cImageFile == '~')
	{
		cImagePath = g_strdup_printf ("%s%s", getenv("HOME"), cImageFile + 1);
	}
	else if (*cImageFile == '/')
	{
		cImagePath = g_strdup (cImageFile);
	}
	else
	{
		cImagePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, cImageFile);
	}
	return cImagePath;
}


#define _get_source_context(...) \
	__extension__ ({\
	if (s_pSourceContext == NULL) {\
		if (g_pMainDock == NULL) return;\
		s_pSourceContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (g_pMainDock)); }\
	s_pSourceContext; })
#define _destroy_source_context(...) do {\
	if (s_pSourceContext != NULL) {\
		cairo_destroy (s_pSourceContext);\
		s_pSourceContext = NULL; } } while (0)

void cairo_dock_load_image_buffer_full (CairoDockImageBuffer *pImage, const gchar *cImageFile, int iWidth, int iHeight, CairoDockLoadImageModifier iLoadModifier, double fAlpha)
{
	if (cImageFile == NULL)
		return;
	cairo_t *pSourceContext = _get_source_context ();
	gchar *cImagePath = cairo_dock_generate_file_path (cImageFile);
	double w, h;
	pImage->pSurface = cairo_dock_create_surface_from_image (
		cImagePath,
		pSourceContext,
		1.,
		iWidth,
		iHeight,
		iLoadModifier,
		&w,
		&h,
		&pImage->fZoomX,
		&pImage->fZoomY);
	pImage->iWidth = w;
	pImage->iHeight = h;
	
	if (fAlpha < 1)
	{
		cairo_surface_t *pNewSurfaceAlpha = _cairo_dock_create_blank_surface (pSourceContext,
			w,
			h);
		cairo_t *pCairoContext = cairo_create (pNewSurfaceAlpha);

		cairo_set_source_surface (pCairoContext, pImage->pSurface, 0, 0);
		cairo_paint_with_alpha (pCairoContext, fAlpha);
		cairo_destroy (pCairoContext);

		cairo_surface_destroy (pImage->pSurface);
		pImage->pSurface = pNewSurfaceAlpha;
	}
	
	if (g_bUseOpenGL)
		pImage->iTexture = cairo_dock_create_texture_from_surface (pImage->pSurface);
	
	g_free (cImagePath);
}

void cairo_dock_load_image_buffer_from_surface (CairoDockImageBuffer *pImage, cairo_surface_t *pSurface, int iWidth, int iHeight)
{
	pImage->pSurface = pSurface;
	pImage->iWidth = iWidth;
	pImage->iHeight = iHeight;
	pImage->fZoomX = 1.;
	pImage->fZoomY = 1.;
	if (g_bUseOpenGL)
		pImage->iTexture = cairo_dock_create_texture_from_surface (pImage->pSurface);
}

CairoDockImageBuffer *cairo_dock_create_image_buffer (const gchar *cImageFile, int iWidth, int iHeight, CairoDockLoadImageModifier iLoadModifier)
{
	CairoDockImageBuffer *pImage = g_new0 (CairoDockImageBuffer, 1);
	
	cairo_dock_load_image_buffer (pImage, cImageFile, iWidth, iHeight, iLoadModifier);
	
	return pImage;
}

void cairo_dock_unload_image_buffer (CairoDockImageBuffer *pImage)
{
	if (pImage->pSurface != NULL)
	{
		cairo_surface_destroy (pImage->pSurface);
	}
	if (pImage->iTexture != 0)
	{
		_cairo_dock_delete_texture (pImage->iTexture);
	}
	memset (pImage, 0, sizeof (CairoDockImageBuffer));
}

void cairo_dock_free_image_buffer (CairoDockImageBuffer *pImage)
{
	if (pImage == NULL)
		return;
	cairo_dock_unload_image_buffer (pImage);
	g_free (pImage);
}



  //////////////////
 /// ICON LOADER ///
//////////////////

void cairo_dock_add_reflection_to_icon (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer)
{
	if (g_bUseOpenGL)
		return ;
	g_return_if_fail (pIcon != NULL && pContainer!= NULL);
	if (pIcon->pReflectionBuffer != NULL)
		cairo_surface_destroy (pIcon->pReflectionBuffer);
	
	int iWidth, iHeight;
	cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
	pIcon->pReflectionBuffer = cairo_dock_create_reflection_surface (pIcon->pIconBuffer,
		pSourceContext,
		iWidth,
		iHeight,
		myIcons.fReflectSize * cairo_dock_get_max_scale (pContainer),
		myIcons.fAlbedo,
		pContainer->bIsHorizontal,
		pContainer->bDirectionUp);
}


void cairo_dock_fill_one_icon_buffer (Icon *icon, cairo_t* pSourceContext, gdouble fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp)
{
	//g_print ("%s (%d, %.2f, %s)\n", __func__, icon->iType, fMaxScale, icon->cFileName);
	if (cairo_dock_icon_is_being_removed (icon))  // si la fenetre est en train de se faire degager du dock, pas la peine de mettre a jour son icone. /// A voir pour les icones d'appli ...
	{
		cd_debug ("skip icon reload for %s", icon->cName);
		return;
	}
	// on garde la surface/texture actuelle pour les emblemes.
	cairo_surface_t *pPrevSurface = icon->pIconBuffer;
	icon->pIconBuffer = NULL;
	GLuint iPrevTexture = icon->iIconTexture;
	icon->iIconTexture = 0;
	
	if (icon->pReflectionBuffer != NULL)
	{
		cairo_surface_destroy (icon->pReflectionBuffer);
		icon->pReflectionBuffer = NULL;
	}
	
	if (icon->fWidth < 0 || icon->fHeight < 0)  // on ne veut pas de surface.
	{
		if (iPrevTexture != 0)
			_cairo_dock_delete_texture (iPrevTexture);
		if (pPrevSurface != NULL)
			cairo_surface_destroy (pPrevSurface);
		return;
	}
	
	if (CAIRO_DOCK_IS_LAUNCHER (icon)/** || (CAIRO_DOCK_IS_USER_SEPARATOR (icon) && icon->cFileName != NULL)*/)  // si c'est un lanceur /*ou un separateur avec une icone.*/
	{
		/// A FAIRE : verifier qu'on peut enlever le test sur fMaxScale ...
		if (icon->fWidth == 0 || fMaxScale != 1.)
			icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
		if (icon->fHeight == 0 || fMaxScale != 1.)
			icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER];
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName);
		
		if (icon->pSubDock != NULL && icon->iSubdockViewType != 0)  // icone de sous-dock, on le redessinera lorsque les icones du sous-dock auront ete chargees.
		{
			icon->pIconBuffer = _cairo_dock_create_blank_surface (pSourceContext, icon->fWidth * fMaxScale, icon->fHeight * fMaxScale);
		}
		else if (cIconPath != NULL && *cIconPath != '\0')  // c'est un lanceur classique.
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_image (cIconPath,
				pSourceContext,
				fMaxScale,
				icon->fWidth/*myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER]*/,
				icon->fHeight/*myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER]*/,
				CAIRO_DOCK_FILL_SPACE,
				(bIsHorizontal ? &icon->fWidth : &icon->fHeight),
				(bIsHorizontal ? &icon->fHeight : &icon->fWidth),
				NULL, NULL);
		}
		else if (icon->pSubDock != NULL && icon->cClass != NULL)  // c'est un epouvantail
		{
			//g_print ("c'est un epouvantail\n");
			icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass,
				pSourceContext,
				fMaxScale,
				(bIsHorizontal ? &icon->fWidth : &icon->fHeight),
				(bIsHorizontal ? &icon->fHeight : &icon->fWidth));
			if (icon->pIconBuffer == NULL)  // aucun inhibiteur ou aucune image correspondant a cette classe, on cherche a copier une des icones d'appli de cette classe.
			{
				const GList *pApplis = cairo_dock_list_existing_appli_with_class (icon->cClass);
				if (pApplis != NULL)
				{
					Icon *pOneIcon = (Icon *) (g_list_last ((GList*)pApplis)->data);  // on prend le dernier car les applis sont inserees a l'envers, et on veut avoir celle qui etait deja present dans le dock (pour 2 raison : continuite, et la nouvelle (en 1ere position) n'est pas forcement deja dans un dock, ce qui fausse le ratio).
					icon->pIconBuffer = cairo_dock_duplicate_inhibator_surface_for_appli (pSourceContext,
						pOneIcon,
						fMaxScale,
						(bIsHorizontal ? &icon->fWidth : &icon->fHeight),
						(bIsHorizontal ? &icon->fHeight : &icon->fWidth));
				}
			}
		}
		g_free (cIconPath);
	}
	else if (CAIRO_DOCK_IS_APPLET (icon))  // c'est l'icone d'une applet.
	{
		//g_print ("  icon->cFileName : %s\n", icon->cFileName);
		icon->pIconBuffer = cairo_dock_create_applet_surface (icon->cFileName,
			pSourceContext,
			fMaxScale,
			(bIsHorizontal ? &icon->fWidth : &icon->fHeight),
			(bIsHorizontal ? &icon->fHeight : &icon->fWidth));
	}
	else if (CAIRO_DOCK_IS_APPLI (icon))  // c'est l'icone d'une appli valide. Dans cet ordre on n'a pas besoin de verifier que c'est NORMAL_APPLI.
	{
		icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI];
		icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI];
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
				icon->pIconBuffer = cairo_dock_create_surface_from_xpixmap (icon->iBackingPixmap, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight);
				if (g_bUseOpenGL)
					icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
			}
			// on affiche l'image precedente en embleme.
			if (icon->iIconTexture != 0 && iPrevTexture != 0)
			{
				CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
				CairoEmblem *e = cairo_dock_make_emblem_from_texture (iPrevTexture,icon, CAIRO_CONTAINER (pParentDock));
				cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
				cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
				g_free (e);  // on n'utilise pas cairo_dock_free_emblem pour ne pas detruire la texture avec.
			}
			else if (icon->pIconBuffer != NULL && pPrevSurface != NULL)
			{
				CairoDock *pParentDock = NULL;  // cairo_dock_search_dock_from_name (icon->cParentDockName);
				CairoEmblem *e = cairo_dock_make_emblem_from_surface (pPrevSurface, 0, 0, icon, CAIRO_CONTAINER (pParentDock));
				cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
				cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
				g_free (e);  // meme remarque.
			}
		}
		if (icon->pIconBuffer == NULL && myTaskBar.bOverWriteXIcons && ! cairo_dock_class_is_using_xicon (icon->cClass))
			icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight);
		if (icon->pIconBuffer == NULL)
			icon->pIconBuffer = cairo_dock_create_surface_from_xwindow (icon->Xid, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight);
		if (icon->pIconBuffer == NULL)  // certaines applis comme xterm ne definissent pas d'icone, on en met une par defaut.
		{
			g_print ("%s (%ld) doesn't define any icon, we set the default one.\n", icon->cName, icon->Xid);
			gchar *cIconPath = cairo_dock_generate_file_path (CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
			if (cIconPath == NULL || ! g_file_test (cIconPath, G_FILE_TEST_EXISTS))
			{
				g_free (cIconPath);
				cIconPath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
			}
			icon->pIconBuffer = cairo_dock_create_surface_from_image (cIconPath,
				pSourceContext,
				fMaxScale,
				myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI],
				myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI],
				CAIRO_DOCK_FILL_SPACE,
				(bIsHorizontal ? &icon->fWidth : &icon->fHeight),
				(bIsHorizontal ? &icon->fHeight : &icon->fWidth),
				NULL, NULL);
			g_free (cIconPath);
		}
	}
	else  // c'est une icone de separation.
	{
		if (CAIRO_DOCK_IS_USER_SEPARATOR (icon) && icon->cFileName != NULL)
		{
			/// A FAIRE : verifier qu'on peut enlever le test sur fMaxScale ...
			if (icon->fWidth == 0 || fMaxScale != 1.)
				icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12];
			if (icon->fHeight == 0 || fMaxScale != 1.)
				icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12];
			gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName);
			
			if (cIconPath != NULL && *cIconPath != '\0')
			{
				icon->pIconBuffer = cairo_dock_create_surface_from_image (cIconPath,
					pSourceContext,
					fMaxScale,
					icon->fWidth,
					icon->fHeight,
					CAIRO_DOCK_FILL_SPACE,
					(bIsHorizontal ? &icon->fWidth : &icon->fHeight),
					(bIsHorizontal ? &icon->fHeight : &icon->fWidth),
					NULL, NULL);
			}
			g_free (cIconPath);
		}
		else
		{
			icon->pIconBuffer = cairo_dock_create_separator_surface (pSourceContext, fMaxScale, bIsHorizontal, bDirectionUp, &icon->fWidth, &icon->fHeight);
		}
	}

	if (icon->pIconBuffer == NULL)
	{
		gchar *cIconPath = cairo_dock_generate_file_path (CAIRO_DOCK_DEFAULT_ICON_NAME);
		if (cIconPath == NULL || ! g_file_test (cIconPath, G_FILE_TEST_EXISTS))
		{
			g_free (cIconPath);
			cIconPath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_ICON_NAME);
		}
		CairoDockIconType iType = cairo_dock_get_icon_type  (icon);
		/// A FAIRE : verifier qu'on peut enlever le test sur fMaxScale ...
		if (icon->fWidth == 0 || fMaxScale != 1.)
			icon->fWidth = myIcons.tIconAuthorizedWidth[iType];
		if (icon->fHeight == 0 || fMaxScale != 1.)
			icon->fHeight = myIcons.tIconAuthorizedHeight[iType];
		icon->pIconBuffer = cairo_dock_create_surface_from_image (cIconPath,
			pSourceContext,
			fMaxScale,
			icon->fWidth,
			icon->fHeight,
			CAIRO_DOCK_FILL_SPACE,
			(bIsHorizontal ? &icon->fWidth : &icon->fHeight),
			(bIsHorizontal ? &icon->fHeight : &icon->fWidth),
			NULL, NULL);
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
			icon->fWidth / (g_pIconBackgroundBuffer.iWidth / fMaxScale),
			icon->fHeight / (g_pIconBackgroundBuffer.iHeight / fMaxScale));
		cairo_set_source_surface (pCairoIconBGContext,
			g_pIconBackgroundBuffer.pSurface,
			0.,
			0.);
		cairo_set_operator (pCairoIconBGContext, CAIRO_OPERATOR_DEST_OVER);
		cairo_paint (pCairoIconBGContext);
		cairo_destroy (pCairoIconBGContext);
	}

	if (! g_bUseOpenGL && myIcons.fAlbedo > 0 && icon->pIconBuffer != NULL && ! (CAIRO_DOCK_IS_APPLET (icon) && icon->cFileName == NULL))
	{
		icon->pReflectionBuffer = cairo_dock_create_reflection_surface (icon->pIconBuffer,
			pSourceContext,
			(bIsHorizontal ? icon->fWidth : icon->fHeight) * fMaxScale,
			(bIsHorizontal ? icon->fHeight : icon->fWidth) * fMaxScale,
			myIcons.fReflectSize * fMaxScale,
			myIcons.fAlbedo,
			bIsHorizontal,
			bDirectionUp);
	}
	
	if (g_bUseOpenGL && icon->pIconBuffer != NULL && icon->iIconTexture == 0)
	{
		icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
	}
	//g_print ("%s (%s, %x, %.2fx%.2f) ->%d\n", __func__, icon->cName, icon->pIconBuffer, icon->fWidth, icon->fHeight, icon->iIconTexture);
	if (iPrevTexture != 0)
		_cairo_dock_delete_texture (iPrevTexture);
	if (pPrevSurface != NULL)
		cairo_surface_destroy (pPrevSurface);
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

void cairo_dock_fill_one_text_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription)
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
		pSourceContext,
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

void cairo_dock_fill_one_quick_info_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription, double fMaxScale)
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
		pSourceContext,
		pTextDescription,
		fMaxScale,
		icon->fWidth * fMaxScale,
		&icon->iQuickInfoWidth, &icon->iQuickInfoHeight, NULL, NULL);
	
	if (g_bUseOpenGL && icon->pQuickInfoBuffer != NULL)
	{
		icon->iQuickInfoTexture = cairo_dock_create_texture_from_surface (icon->pQuickInfoBuffer);
	}
}



void cairo_dock_fill_icon_buffers (Icon *icon, cairo_t *pSourceContext, double fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp)
{
	cairo_dock_fill_one_icon_buffer (icon, pSourceContext, fMaxScale, bIsHorizontal, bDirectionUp);

	cairo_dock_fill_one_text_buffer (icon, pSourceContext, &myLabels.iconTextDescription);

	cairo_dock_fill_one_quick_info_buffer (icon, pSourceContext, &myLabels.quickInfoTextDescription, fMaxScale);
}

void cairo_dock_load_one_icon_from_scratch (Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (pIcon != NULL);
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pContainer));
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		cairo_dock_fill_icon_buffers_for_dock (pIcon, pCairoContext, pDock);
	}
	else
	{
		cairo_dock_fill_icon_buffers_for_desklet (pIcon, pCairoContext);
	}
	cairo_destroy (pCairoContext);
}

void cairo_dock_reload_buffers_in_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	gboolean bReloadAppletsToo = GPOINTER_TO_INT (data);
	cd_message ("%s (%s, %d)", __func__, cDockName, bReloadAppletsToo);

	double fFlatDockWidth = - myIcons.iIconGap;
	pDock->iMaxIconHeight = 0;

	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDock));

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
			cairo_dock_fill_icon_buffers_for_dock (icon, pCairoContext, pDock);
			icon->fWidth *= pDock->container.fRatio;
			icon->fHeight *= pDock->container.fRatio;
		}
		
		//g_print (" =size <- %.2fx%.2f\n", icon->fWidth, icon->fHeight);
		fFlatDockWidth += myIcons.iIconGap + icon->fWidth;
		pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);
	}
	pDock->fFlatDockWidth = (int) fFlatDockWidth;  /// (int) n'est plus tellement necessaire ...
	cairo_destroy (pCairoContext);
}

void cairo_dock_reload_one_icon_buffer_in_dock (Icon *icon, CairoDock *pDock)
{
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDock));
	cairo_dock_reload_one_icon_buffer_in_dock_full (icon, pDock, pCairoContext);
	cairo_destroy (pCairoContext);
}


  ///////////////////////
 /// DOCK BACKGROUND ///
///////////////////////

void cairo_dock_load_visible_zone (CairoDock *pDock, gchar *cVisibleZoneImageFile, int iVisibleZoneWidth, int iVisibleZoneHeight, double fVisibleZoneAlpha)
{
	cairo_dock_unload_image_buffer (&g_pVisibleZoneBuffer);
	
	cairo_dock_load_image_buffer (&g_pVisibleZoneBuffer,
		cVisibleZoneImageFile,
		iVisibleZoneWidth,
		iVisibleZoneHeight,
		CAIRO_DOCK_FILL_SPACE);
}


static cairo_surface_t *_cairo_dock_make_stripes_background (cairo_t* pSourceContext, int iStripesWidth, int iStripesHeight)
{
	g_return_val_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	cairo_pattern_t *pStripesPattern;
	double fWidth = (myBackground.iNbStripes > 0 ? 200. : iStripesWidth);
	if (fabs (myBackground.fStripesAngle) != 90)
		pStripesPattern = cairo_pattern_create_linear (0.0f,
			0.0f,
			fWidth,
			fWidth * tan (myBackground.fStripesAngle * G_PI/180.));
	else
		pStripesPattern = cairo_pattern_create_linear (0.0f,
			0.0f,
			0.,
			(myBackground.fStripesAngle == 90) ? iStripesHeight : - iStripesHeight);
	g_return_val_if_fail (cairo_pattern_status (pStripesPattern) == CAIRO_STATUS_SUCCESS, NULL);

	cairo_pattern_set_extend (pStripesPattern, CAIRO_EXTEND_REPEAT);

	if (myBackground.iNbStripes > 0)
	{
		gdouble fStep;
		double fStripesGap = 1. / (myBackground.iNbStripes);  // ecart entre 2 rayures foncees.
		for (fStep = 0.0f; fStep < 1.0f; fStep += fStripesGap)
		{
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep - myBackground.fStripesWidth / 2,
				myBackground.fStripesColorBright[0],
				myBackground.fStripesColorBright[1],
				myBackground.fStripesColorBright[2],
				myBackground.fStripesColorBright[3]);
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep,
				myBackground.fStripesColorDark[0],
				myBackground.fStripesColorDark[1],
				myBackground.fStripesColorDark[2],
				myBackground.fStripesColorDark[3]);
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep + myBackground.fStripesWidth / 2,
				myBackground.fStripesColorBright[0],
				myBackground.fStripesColorBright[1],
				myBackground.fStripesColorBright[2],
				myBackground.fStripesColorBright[3]);
		}
	}
	else
	{
		cairo_pattern_add_color_stop_rgba (pStripesPattern,
			0.,
			myBackground.fStripesColorDark[0],
			myBackground.fStripesColorDark[1],
			myBackground.fStripesColorDark[2],
			myBackground.fStripesColorDark[3]);
		cairo_pattern_add_color_stop_rgba (pStripesPattern,
			1.,
			myBackground.fStripesColorBright[0],
			myBackground.fStripesColorBright[1],
			myBackground.fStripesColorBright[2],
			myBackground.fStripesColorBright[3]);
	}

	cairo_surface_t *pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
		iStripesWidth,
		iStripesHeight);
	cairo_t *pImageContext = cairo_create (pNewSurface);
	cairo_set_source (pImageContext, pStripesPattern);
	cairo_paint (pImageContext);

	cairo_pattern_destroy (pStripesPattern);
	cairo_destroy (pImageContext);
	
	return pNewSurface;
}
void cairo_dock_update_background_decorations_if_necessary (CairoDock *pDock, int iNewDecorationsWidth, int iNewDecorationsHeight)
{
	//g_print ("%s (%dx%d) [%.2fx%.2f]\n", __func__, iNewDecorationsWidth, iNewDecorationsHeight, g_fBackgroundImageWidth, g_fBackgroundImageHeight);
	int k = (myBackground.fDecorationSpeed || myBackground.bDecorationsFollowMouse ? 2 : 1);
	if (k * iNewDecorationsWidth > g_pDockBackgroundBuffer.iWidth || iNewDecorationsHeight > g_pDockBackgroundBuffer.iHeight)
	{
		cairo_dock_unload_image_buffer (&g_pDockBackgroundBuffer);
		cairo_t *pCairoContext = _get_source_context ();
		int iWidth, iHeight;
		if (myBackground.cBackgroundImageFile != NULL)
		{
			if (myBackground.bBackgroundImageRepeat)
			{
				iWidth = MAX (g_pDockBackgroundBuffer.iWidth, k * iNewDecorationsWidth);
				iHeight = MAX (g_pDockBackgroundBuffer.iHeight, iNewDecorationsHeight);
				cairo_surface_t *pBgSurface = cairo_dock_create_surface_from_pattern (myBackground.cBackgroundImageFile,
					pCairoContext,
					iWidth,
					iHeight,
					myBackground.fBackgroundImageAlpha);
				cairo_dock_load_image_buffer_from_surface (&g_pDockBackgroundBuffer,
					pBgSurface,
					iWidth,
					iHeight);
			}
			else
			{
				iWidth = MAX (g_pDockBackgroundBuffer.iWidth, iNewDecorationsWidth);
				iHeight = MAX (g_pDockBackgroundBuffer.iHeight, iNewDecorationsHeight);
				cairo_dock_load_image_buffer_full (&g_pDockBackgroundBuffer,
					myBackground.cBackgroundImageFile,
					iWidth,
					iHeight,
					CAIRO_DOCK_FILL_SPACE,
					myBackground.fBackgroundImageAlpha);
			}
		}
		else
		{
			iWidth = MAX (g_pDockBackgroundBuffer.iWidth, k * iNewDecorationsWidth);
			iHeight = MAX (g_pDockBackgroundBuffer.iHeight, iNewDecorationsHeight);
			cairo_surface_t *pBgSurface = _cairo_dock_make_stripes_background (pCairoContext,
				iWidth,
				iHeight);
			cairo_dock_load_image_buffer_from_surface (&g_pDockBackgroundBuffer,
				pBgSurface,
				iWidth,
				iHeight);
		}
		//cd_debug ("  MaJ des decorations du fond -> %dx%d", iWidth, iHeight);
	}
}

void cairo_dock_load_background_decorations (CairoDock *pDock)
{
	int iWidth, iHeight;
	cairo_dock_search_max_decorations_size (&iWidth, &iHeight);
	
	g_pDockBackgroundBuffer.iWidth = 0;
	g_pDockBackgroundBuffer.iHeight = 0;
	cairo_dock_update_background_decorations_if_necessary (pDock, iWidth, iHeight);
}


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


void cairo_dock_load_box_surface (double fMaxScale)
{
	cairo_dock_unload_image_buffer (&g_pBoxAboveBuffer);
	cairo_dock_unload_image_buffer (&g_pBoxBelowBuffer);
	
	int iSize = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
	if (iSize == 0)
		iSize = 48;
	iSize *= fMaxScale;
	
	gchar *cUserPath = cairo_dock_generate_file_path ("box-front.png");
	if (! g_file_test (cUserPath, G_FILE_TEST_EXISTS))
	{
		g_free (cUserPath);
		cUserPath = NULL;
	}
	cairo_dock_load_image_buffer (&g_pBoxAboveBuffer,
		cUserPath ? cUserPath : CAIRO_DOCK_SHARE_DATA_DIR"/box-front.png",
		iSize,
		iSize,
		CAIRO_DOCK_FILL_SPACE);
	
	cUserPath = cairo_dock_generate_file_path ("box-back.png");
	if (! g_file_test (cUserPath, G_FILE_TEST_EXISTS))
	{
		g_free (cUserPath);
		cUserPath = NULL;
	}
	cairo_dock_load_image_buffer (&g_pBoxBelowBuffer,
		CAIRO_DOCK_SHARE_DATA_DIR"/box-back.png",
		iSize,
		iSize,
		CAIRO_DOCK_FILL_SPACE);
}



  //////////////////
 /// DESKTOP BG ///
//////////////////

static cairo_surface_t *_cairo_dock_create_surface_from_desktop_bg (void)  // attention : fonction lourde.
{
	cd_debug ("%s ()", __func__);
	Pixmap iRootPixmapID = cairo_dock_get_window_background_pixmap (cairo_dock_get_root_id ());
	g_return_val_if_fail (iRootPixmapID != 0, NULL);
	
	cairo_surface_t *pDesktopBgSurface = NULL;
	GdkPixbuf *pBgPixbuf = cairo_dock_get_pixbuf_from_pixmap (iRootPixmapID, FALSE);  // FALSE <=> on n'y ajoute pas de transparence.
	if (pBgPixbuf != NULL)
	{
		cairo_t *pSourceContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (g_pMainDock));
		
		if (gdk_pixbuf_get_height (pBgPixbuf) == 1 && gdk_pixbuf_get_width (pBgPixbuf) == 1)  // couleur unie.
		{
			guchar *pixels = gdk_pixbuf_get_pixels (pBgPixbuf);
			cd_debug ("c'est une couleur unie (%.2f, %.2f, %.2f)", (double) pixels[0] / 255, (double) pixels[1] / 255, (double) pixels[2] / 255);
			
			pDesktopBgSurface = _cairo_dock_create_blank_surface (pSourceContext,
				g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL],
				g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
			
			cairo_t *pCairoContext = cairo_create (pDesktopBgSurface);
			cairo_set_source_rgb (pCairoContext,
				(double) pixels[0] / 255,
				(double) pixels[1] / 255,
				(double) pixels[2] / 255);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
			cairo_paint (pCairoContext);
			cairo_destroy (pCairoContext);
		}
		else
		{
			cairo_t *pSourceContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (g_pMainDock));
			double fWidth, fHeight;
			cairo_surface_t *pBgSurface = cairo_dock_create_surface_from_pixbuf (pBgPixbuf,
				pSourceContext,
				1,
				0,
				0,
				FALSE,
				&fWidth,
				&fHeight,
				NULL, NULL);
			
			if (fWidth < g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] || fHeight < g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL])
			{
				cd_debug ("c'est un degrade ou un motif (%dx%d)", (int) fWidth, (int) fHeight);
				pDesktopBgSurface = _cairo_dock_create_blank_surface (pSourceContext,
					g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL],
					g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
				cairo_t *pCairoContext = cairo_create (pDesktopBgSurface);
				
				cairo_pattern_t *pPattern = cairo_pattern_create_for_surface (pBgSurface);
				g_return_val_if_fail (cairo_pattern_status (pPattern) == CAIRO_STATUS_SUCCESS, NULL);
				cairo_pattern_set_extend (pPattern, CAIRO_EXTEND_REPEAT);
				
				cairo_set_source (pCairoContext, pPattern);
				cairo_paint (pCairoContext);
				
				cairo_destroy (pCairoContext);
				cairo_pattern_destroy (pPattern);
				cairo_surface_destroy (pBgSurface);
			}
			else
			{
				cd_debug ("c'est un fond d'ecran de taille %dx%d", (int) fWidth, (int) fHeight);
				pDesktopBgSurface = pBgSurface;
			}
		}
		
		g_object_unref (pBgPixbuf);
		cairo_destroy (pSourceContext);
	}
	return pDesktopBgSurface;
}

CairoDockDesktopBackground *cairo_dock_get_desktop_background (gboolean bWithTextureToo)
{
	cd_message ("%s (%d, %d)", __func__, bWithTextureToo, s_pDesktopBg?s_pDesktopBg->iRefCount:-1);
	if (s_pDesktopBg == NULL)
	{
		s_pDesktopBg = g_new0 (CairoDockDesktopBackground, 1);
	}
	if (s_pDesktopBg->iRefCount == 0)
	{
		s_pDesktopBg->pSurface = _cairo_dock_create_surface_from_desktop_bg ();
	}
	if (s_pDesktopBg->iTexture == 0 && bWithTextureToo)
	{
		s_pDesktopBg->iTexture = cairo_dock_create_texture_from_surface (s_pDesktopBg->pSurface);
	}
	
	s_pDesktopBg->iRefCount ++;
	if (s_pDesktopBg->iSidDestroyBg != 0)
	{
		g_source_remove (s_pDesktopBg->iSidDestroyBg);
		s_pDesktopBg->iSidDestroyBg = 0;
	}
	return s_pDesktopBg;
}

static gboolean _destroy_bg (CairoDockDesktopBackground *pDesktopBg)
{
	cd_message ("%s ()", __func__);
	g_return_val_if_fail (pDesktopBg != NULL, 0);
	if (pDesktopBg->pSurface != NULL)
	{
		cairo_surface_destroy (pDesktopBg->pSurface);
		pDesktopBg->pSurface = NULL;
	}
	if (pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (pDesktopBg->iTexture);
		pDesktopBg->iTexture = 0;
	}
	pDesktopBg->iSidDestroyBg = 0;
	return FALSE;
}
void cairo_dock_destroy_desktop_background (CairoDockDesktopBackground *pDesktopBg)
{
	g_return_if_fail (pDesktopBg != NULL);
	pDesktopBg->iRefCount --;
	if (pDesktopBg->iRefCount == 0 && pDesktopBg->iSidDestroyBg == 0)
	{
		pDesktopBg->iSidDestroyBg = g_timeout_add_seconds (3, (GSourceFunc)_destroy_bg, pDesktopBg);
	}
}

cairo_surface_t *cairo_dock_get_desktop_bg_surface (CairoDockDesktopBackground *pDesktopBg)
{
	g_return_val_if_fail (pDesktopBg != NULL, NULL);
	return pDesktopBg->pSurface;
}

GLuint cairo_dock_get_desktop_bg_texture (CairoDockDesktopBackground *pDesktopBg)
{
	g_return_val_if_fail (pDesktopBg != NULL, 0);
	return pDesktopBg->iTexture;
}

void cairo_dock_reload_desktop_background (void)
{
	cd_message ("%s ()", __func__);
	if (s_pDesktopBg == NULL)  // rien a recharger.
		return ;
	if (s_pDesktopBg->pSurface == NULL && s_pDesktopBg->iTexture == 0)  // rien a recharger.
		return ;
	
	if (s_pDesktopBg->pSurface != NULL)
		cairo_surface_destroy (s_pDesktopBg->pSurface);
	s_pDesktopBg->pSurface = _cairo_dock_create_surface_from_desktop_bg ();
	
	if (s_pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (s_pDesktopBg->iTexture);
		s_pDesktopBg->iTexture = cairo_dock_create_texture_from_surface (s_pDesktopBg->pSurface);
	}
}


  //////////////////
 /// INDICATORS ///
//////////////////

void cairo_dock_load_task_indicator (const gchar *cIndicatorImagePath, double fMaxScale, double fIndicatorRatio)
{
	cairo_dock_unload_image_buffer (&g_pIndicatorBuffer);
	
	double fLauncherWidth = (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : 48);
	double fLauncherHeight = (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : 48);
	double fScale = (myIndicators.bLinkIndicatorWithIcon ? fMaxScale : 1.) * fIndicatorRatio;
	
	cairo_dock_load_image_buffer (&g_pIndicatorBuffer,
		cIndicatorImagePath,
		fLauncherWidth * fScale,
		fLauncherHeight * fScale,
		CAIRO_DOCK_KEEP_RATIO);
}

void cairo_dock_load_active_window_indicator (const gchar *cImagePath, double fMaxScale, double fCornerRadius, double fLineWidth, double *fActiveColor)
{
	cairo_dock_unload_image_buffer (&g_pActiveIndicatorBuffer);
	
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
		cairo_dock_load_image_buffer (&g_pActiveIndicatorBuffer,
			cImagePath,
			iWidth,
			iHeight,
			CAIRO_DOCK_FILL_SPACE);
	}
	else if (fActiveColor[3] > 0)
	{
		cairo_t *pSourceContext = _get_source_context ();
		cairo_surface_t *pSurface = _cairo_dock_create_blank_surface (pSourceContext,
			iWidth,
			iHeight);
		cairo_t *pCairoContext = cairo_create (pSurface);
		
		fCornerRadius = MIN (fCornerRadius, (iWidth - fLineWidth) / 2);
		double fFrameWidth = iWidth - (2 * fCornerRadius + fLineWidth);
		double fFrameHeight = iHeight - 2 * fLineWidth;
		double fDockOffsetX = fCornerRadius + fLineWidth/2;
		double fDockOffsetY = fLineWidth/2;
		cairo_dock_draw_frame (pCairoContext, fCornerRadius, fLineWidth, fFrameWidth, fFrameHeight, fDockOffsetX, fDockOffsetY, 1, 0., CAIRO_DOCK_HORIZONTAL);
		
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
		
		cairo_dock_load_image_buffer_from_surface (&g_pActiveIndicatorBuffer,
			pSurface,
			iWidth,
			iHeight);
	}
}

void cairo_dock_load_class_indicator (const gchar *cIndicatorImagePath, double fMaxScale)
{
	cairo_dock_unload_image_buffer (&g_pClassIndicatorBuffer);
	
	double fLauncherWidth = (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : 48);
	double fLauncherHeight = (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : 48);
	
	cairo_dock_load_image_buffer (&g_pClassIndicatorBuffer,
		cIndicatorImagePath,
		fLauncherWidth/3,
		fLauncherHeight/3,
		CAIRO_DOCK_KEEP_RATIO);
}


void cairo_dock_unload_additionnal_textures (void)
{
	cd_debug ("");
	cairo_dock_unload_image_buffer (&g_pDockBackgroundBuffer);
	cairo_dock_unload_image_buffer (&g_pVisibleZoneBuffer);
	cairo_dock_unload_image_buffer (&g_pIconBackgroundBuffer);
	cairo_dock_unload_image_buffer (&g_pIndicatorBuffer);
	cairo_dock_unload_image_buffer (&g_pActiveIndicatorBuffer);
	cairo_dock_unload_image_buffer (&g_pClassIndicatorBuffer);
	cairo_dock_unload_image_buffer (&g_pBoxAboveBuffer);
	cairo_dock_unload_image_buffer (&g_pBoxBelowBuffer);
	cairo_dock_unload_desklet_buttons ();
	cairo_dock_unload_dialog_buttons ();
	if (g_pGradationTexture[0] != 0)
	{
		_cairo_dock_delete_texture (g_pGradationTexture[0]);
		g_pGradationTexture[0] = 0;
	}
	if (g_pGradationTexture[1] != 0)
	{
		_cairo_dock_delete_texture (g_pGradationTexture[1]);
		g_pGradationTexture[1] = 0;
	}
	if (s_pDesktopBg != NULL && s_pDesktopBg->iTexture != 0)
	{
		_cairo_dock_delete_texture (s_pDesktopBg->iTexture);
		s_pDesktopBg->iTexture = 0;
	}
	///cairo_dock_destroy_icon_pbuffer ();
	cairo_dock_destroy_icon_fbo ();
	cairo_dock_unload_default_data_renderer_font ();
	cairo_dock_unload_flying_container_textures ();
	_destroy_source_context ();
}
