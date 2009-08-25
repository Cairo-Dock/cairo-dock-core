/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
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
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-container.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-load.h"

extern CairoDock *g_pMainDock;

extern gint g_iXScreenWidth[2];  // change tous les g_iScreen par g_iXScreen le 28/07/2009
extern gint g_iXScreenHeight[2];

extern cairo_surface_t *g_pVisibleZoneSurface;

extern gchar *g_cCurrentThemePath;

extern cairo_surface_t *g_pBackgroundSurface;
extern cairo_surface_t *g_pBackgroundSurfaceFull;
extern double g_fBackgroundImageWidth, g_fBackgroundImageHeight;

extern cairo_surface_t *g_pIndicatorSurface;
extern double g_fIndicatorWidth, g_fIndicatorHeight;

extern cairo_surface_t *g_pActiveIndicatorSurface;
extern double g_fActiveIndicatorWidth, g_fActiveIndicatorHeight;

extern cairo_surface_t *g_pClassIndicatorSurface;
extern double g_fClassIndicatorWidth, g_fClassIndicatorHeight;

extern cairo_surface_t *g_pIconBackgroundImageSurface;
extern double g_iIconBackgroundImageWidth, g_iIconBackgroundImageHeight;

extern cairo_surface_t *g_pDesktopBgSurface;
extern GLuint g_pGradationTexture[2];

extern gboolean g_bUseOpenGL;
extern GLuint g_iBackgroundTexture;
extern GLuint g_iVisibleZoneTexture;
extern GLuint g_iIndicatorTexture;
extern GLuint g_iActiveIndicatorTexture;
extern GLuint g_iDesktopBgTexture;
extern GLuint g_iClassIndicatorTexture;

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

cairo_surface_t *cairo_dock_load_image (cairo_t *pSourceContext, const gchar *cImageFile, double *fImageWidth, double *fImageHeight, double fRotationAngle, double fAlpha, gboolean bReapeatAsPattern)
{
	g_return_val_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	cairo_surface_t *pNewSurface = NULL;

	if (cImageFile != NULL)
	{
		gchar *cImagePath = cairo_dock_generate_file_path (cImageFile);

		int iDesiredWidth = (int) (*fImageWidth), iDesiredHeight = (int) (*fImageHeight);
		pNewSurface = cairo_dock_create_surface_from_image (cImagePath,
			pSourceContext,
			1.,
			bReapeatAsPattern ? 0 : iDesiredWidth,  // pas de contrainte sur
			bReapeatAsPattern ? 0 : iDesiredHeight,  // la taille du motif initialement.
			CAIRO_DOCK_FILL_SPACE,
			fImageWidth,
			fImageHeight,
			NULL, NULL);
		
		if (bReapeatAsPattern)
		{
			cairo_surface_t *pNewSurfaceFilled = _cairo_dock_create_blank_surface (pSourceContext,
				iDesiredWidth,
				iDesiredHeight);
			cairo_t *pCairoContext = cairo_create (pNewSurfaceFilled);
	
			cairo_pattern_t* pPattern = cairo_pattern_create_for_surface (pNewSurface);
			g_return_val_if_fail (cairo_pattern_status (pPattern) == CAIRO_STATUS_SUCCESS, NULL);
			cairo_pattern_set_extend (pPattern, CAIRO_EXTEND_REPEAT);
	
			cairo_set_source (pCairoContext, pPattern);
			cairo_paint (pCairoContext);
			cairo_destroy (pCairoContext);
			cairo_pattern_destroy (pPattern);
			
			cairo_surface_destroy (pNewSurface);
			pNewSurface = pNewSurfaceFilled;
			*fImageWidth = iDesiredWidth;
			*fImageHeight = iDesiredHeight;
		}
		
		if (fAlpha < 1)
		{
			cairo_surface_t *pNewSurfaceAlpha = _cairo_dock_create_blank_surface (pSourceContext,
				*fImageWidth,
				*fImageHeight);
			cairo_t *pCairoContext = cairo_create (pNewSurfaceAlpha);
	
			cairo_set_source_surface (pCairoContext, pNewSurface, 0, 0);
			cairo_paint_with_alpha (pCairoContext, fAlpha);
			cairo_destroy (pCairoContext);
	
			cairo_surface_destroy (pNewSurface);
			pNewSurface = pNewSurfaceAlpha;
		}
		
		if (fRotationAngle != 0)
		{
			cairo_surface_t *pNewSurfaceRotated = cairo_dock_rotate_surface (pNewSurface,
				pSourceContext,
				*fImageWidth,
				*fImageHeight,
				fRotationAngle);
			cairo_surface_destroy (pNewSurface);
			pNewSurface = pNewSurfaceRotated;
		}
		
		g_free (cImagePath);
	}
	
	return pNewSurface;
}


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
		pContainer->bIsHorizontal,
		cairo_dock_get_max_scale (pContainer),
		pContainer->bDirectionUp);
}


void cairo_dock_fill_one_icon_buffer (Icon *icon, cairo_t* pSourceContext, gdouble fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp)
{
	//g_print ("%s (%d, %.2f, %s)\n", __func__, icon->iType, fMaxScale, icon->acFileName);
	if (icon->pIconBuffer != NULL)
	{
		cairo_surface_destroy (icon->pIconBuffer);
		icon->pIconBuffer = NULL;
	}
	if (icon->iIconTexture != 0)
	{
		_cairo_dock_delete_texture (icon->iIconTexture);
		icon->iIconTexture = 0;
	}
	if (icon->pReflectionBuffer != NULL)
	{
		cairo_surface_destroy (icon->pReflectionBuffer);
		icon->pReflectionBuffer = NULL;
	}
	
	if (icon->fWidth < 0 || icon->fHeight < 0)  // on ne veut pas de surface.
		return;
	
	if (CAIRO_DOCK_IS_LAUNCHER (icon) || (CAIRO_DOCK_IS_USER_SEPARATOR (icon) && icon->acFileName != NULL))
	{
		/// A FAIRE : verifier qu'on peut enlever le test sur fMaxScale ...
		if (icon->fWidth == 0 || fMaxScale != 1.)
			icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER];
		if (icon->fHeight == 0 || fMaxScale != 1.)
			icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER];
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->acFileName);
		
		if (cIconPath != NULL && *cIconPath != '\0')  // c'est un lanceur classique.
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_image (cIconPath,
				pSourceContext,
				fMaxScale,
				icon->fWidth/*myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER]*/,
				icon->fHeight/*myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER]*/,
				CAIRO_DOCK_FILL_SPACE,
				(bHorizontalDock ? &icon->fWidth : &icon->fHeight),
				(bHorizontalDock ? &icon->fHeight : &icon->fWidth),
				NULL, NULL);
		}
		else if (icon->pSubDock != NULL && icon->cClass != NULL)  // c'est un epouvantail
		{
			//g_print ("c'est un epouvantail\n");
			icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass,
				pSourceContext,
				fMaxScale,
				(bHorizontalDock ? &icon->fWidth : &icon->fHeight),
				(bHorizontalDock ? &icon->fHeight : &icon->fWidth));
			if (icon->pIconBuffer == NULL)
			{
				GList *pApplis = cairo_dock_list_existing_appli_with_class (icon->cClass);
				if (pApplis != NULL)
				{
					Icon *pOneIcon = (Icon *) (g_list_last (pApplis)->data);  // on prend le dernier car les applis sont inserees a l'envers, et on veut avoir celle qui etait deja present dans le dock (pour 2 raison : continuite, et la nouvelle (en 1ere position) n'est pas forcement deja dans un dock, ce qui fausse le ratio).
					icon->pIconBuffer = cairo_dock_duplicate_inhibator_surface_for_appli (pSourceContext,
						pOneIcon,
						fMaxScale,
						(bHorizontalDock ? &icon->fWidth : &icon->fHeight),
						(bHorizontalDock ? &icon->fHeight : &icon->fWidth));
				}
			}
		}
		g_free (cIconPath);
	}
	else if (CAIRO_DOCK_IS_APPLET (icon))  // c'est l'icône d'une applet.
	{
		//g_print ("  icon->acFileName : %s\n", icon->acFileName);
		icon->pIconBuffer = cairo_dock_create_applet_surface (icon->acFileName,
			pSourceContext,
			fMaxScale,
			(bHorizontalDock ? &icon->fWidth : &icon->fHeight),
			(bHorizontalDock ? &icon->fHeight : &icon->fWidth));
	}
	else if (CAIRO_DOCK_IS_APPLI (icon))  // c'est l'icône d'une appli valide. Dans cet ordre on n'a pas besoin de verifier que c'est NORMAL_APPLI.
	{
		icon->fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI];
		icon->fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI];
		if (myTaskBar.bShowThumbnail && icon->bIsHidden && icon->iBackingPixmap != 0)
			icon->pIconBuffer = cairo_dock_create_surface_from_xpixmap (icon->iBackingPixmap, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight);
		if (icon->pIconBuffer == NULL && myTaskBar.bOverWriteXIcons && ! cairo_dock_class_is_using_xicon (icon->cClass))
			icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight);
		if (icon->pIconBuffer == NULL)
			icon->pIconBuffer = cairo_dock_create_surface_from_xwindow (icon->Xid, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight);
		if (icon->pIconBuffer == NULL)  // certaines applis comme xterm ne definissent pas d'icone, on en met une par defaut.
		{
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
				(bHorizontalDock ? &icon->fWidth : &icon->fHeight),
				(bHorizontalDock ? &icon->fHeight : &icon->fWidth),
				NULL, NULL);
			g_free (cIconPath);
		}
	}
	else  // c'est une icone de separation.
	{
		icon->pIconBuffer = cairo_dock_create_separator_surface (pSourceContext, fMaxScale, bHorizontalDock, bDirectionUp, &icon->fWidth, &icon->fHeight);
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
			(bHorizontalDock ? &icon->fWidth : &icon->fHeight),
			(bHorizontalDock ? &icon->fHeight : &icon->fWidth),
			NULL, NULL);
		g_free (cIconPath);
	}
	cd_debug ("%s () -> %.2fx%.2f", __func__, icon->fWidth, icon->fHeight);
	
	//\_____________ On met le background de l'icone si necessaire
	if (icon->pIconBuffer != NULL &&
		g_pIconBackgroundImageSurface != NULL &&
		(CAIRO_DOCK_IS_NORMAL_LAUNCHER(icon) || CAIRO_DOCK_IS_APPLI(icon) || (myIcons.bBgForApplets && CAIRO_DOCK_IS_APPLET(icon))))
	{
		cd_message (">>> %s prendra un fond d'icone", icon->acName);

		cairo_t *pCairoIconBGContext = cairo_create (icon->pIconBuffer);
		cairo_scale(pCairoIconBGContext,
			icon->fWidth / g_iIconBackgroundImageWidth,
			icon->fHeight / g_iIconBackgroundImageHeight);
		cairo_set_source_surface (pCairoIconBGContext,
			g_pIconBackgroundImageSurface,
			0.,
			0.);
		cairo_set_operator (pCairoIconBGContext, CAIRO_OPERATOR_DEST_OVER);
		cairo_paint (pCairoIconBGContext);
		cairo_destroy (pCairoIconBGContext);
	}

	if (! g_bUseOpenGL && myIcons.fAlbedo > 0 && icon->pIconBuffer != NULL && ! (CAIRO_DOCK_IS_APPLET (icon) && icon->acFileName == NULL))
	{
		icon->pReflectionBuffer = cairo_dock_create_reflection_surface (icon->pIconBuffer,
			pSourceContext,
			(bHorizontalDock ? icon->fWidth : icon->fHeight) * fMaxScale,
			(bHorizontalDock ? icon->fHeight : icon->fWidth) * fMaxScale,
			bHorizontalDock,
			fMaxScale,
			bDirectionUp);
	}
	
	if (g_bUseOpenGL && icon->pIconBuffer != NULL)
	{
		icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
	}
}


gchar *cairo_dock_cut_string (const gchar *cString, guint iNbCaracters)  // gere l'UTF-8
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
	if (g_utf8_validate (cUtf8Name, -1, &cEndValidChain))
	{
		if (g_utf8_strlen (cUtf8Name, -1) > iNbCaracters)
		{
			cTruncatedName = g_new0 (gchar, 8 * (iNbCaracters + 4));  // 8 octets par caractere.
			g_utf8_strncpy (cTruncatedName, cUtf8Name, iNbCaracters);

			gchar *cTruncature = g_utf8_offset_to_pointer (cTruncatedName, iNbCaracters);
			*cTruncature = '.';
			*(cTruncature+1) = '.';
			*(cTruncature+2) = '.';
		}
	}
	else
	{
		if (strlen (cString) > iNbCaracters)
		{
			cTruncatedName = g_new0 (gchar, iNbCaracters + 4);
			strncpy (cTruncatedName, cString, iNbCaracters);

			cTruncatedName[iNbCaracters] = '.';
			cTruncatedName[iNbCaracters+1] = '.';
			cTruncatedName[iNbCaracters+2] = '.';
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
	if (icon->acName == NULL || (pTextDescription->iSize == 0))
		return ;

	gchar *cTruncatedName = NULL;
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.iAppliMaxNameLength > 0)
	{
		cTruncatedName = cairo_dock_cut_string (icon->acName, myTaskBar.iAppliMaxNameLength);
	}
	
	cairo_surface_t* pNewSurface = cairo_dock_create_surface_from_text_full ((cTruncatedName != NULL ? cTruncatedName : icon->acName),
		pSourceContext,
		pTextDescription,
		1., 0,
		&icon->iTextWidth, &icon->iTextHeight, &icon->fTextXOffset, &icon->fTextYOffset);
	g_free (cTruncatedName);
	//g_print (" -> %s : (%.2f;%.2f) %dx%d\n", icon->acName, icon->fTextXOffset, icon->fTextYOffset, icon->iTextWidth, icon->iTextHeight);

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

	icon->pQuickInfoBuffer = cairo_dock_create_surface_from_text_full (icon->cQuickInfo,
		pSourceContext,
		pTextDescription,
		fMaxScale,
		icon->fWidth * fMaxScale,
		&icon->iQuickInfoWidth, &icon->iQuickInfoHeight, &icon->fQuickInfoXOffset, &icon->fQuickInfoYOffset);
	
	if (g_bUseOpenGL && icon->pQuickInfoBuffer != NULL)
	{
		icon->iQuickInfoTexture = cairo_dock_create_texture_from_surface (icon->pQuickInfoBuffer);
	}
}



void cairo_dock_fill_icon_buffers (Icon *icon, cairo_t *pSourceContext, double fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp)
{
	cairo_dock_fill_one_icon_buffer (icon, pSourceContext, fMaxScale, bHorizontalDock, bDirectionUp);

	cairo_dock_fill_one_text_buffer (icon, pSourceContext, &myLabels.iconTextDescription);

	cairo_dock_fill_one_quick_info_buffer (icon, pSourceContext, &myLabels.quickInfoTextDescription, fMaxScale);
}

void cairo_dock_load_one_icon_from_scratch (Icon *pIcon, CairoContainer *pContainer)
{
	g_return_if_fail (pIcon != NULL);
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pContainer));
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

	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));

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
			icon->fWidth /= pDock->fRatio;
			icon->fHeight /= pDock->fRatio;
			cairo_dock_fill_icon_buffers_for_dock (icon, pCairoContext, pDock);
			icon->fWidth *= pDock->fRatio;
			icon->fHeight *= pDock->fRatio;
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
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	cairo_dock_reload_one_icon_buffer_in_dock_full (icon, pDock, pCairoContext);
	cairo_destroy (pCairoContext);
}



void cairo_dock_load_visible_zone (CairoDock *pDock, gchar *cVisibleZoneImageFile, int iVisibleZoneWidth, int iVisibleZoneHeight, double fVisibleZoneAlpha)
{
	double fVisibleZoneWidth = iVisibleZoneWidth, fVisibleZoneHeight = iVisibleZoneHeight;
	if (g_pVisibleZoneSurface != NULL)
		cairo_surface_destroy (g_pVisibleZoneSurface);
	if (g_iVisibleZoneTexture != 0)
	{
		_cairo_dock_delete_texture (g_iVisibleZoneTexture);
		g_iVisibleZoneTexture = 0;
	}
	if (cVisibleZoneImageFile != NULL)
	{
		cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
		g_pVisibleZoneSurface = cairo_dock_load_image (pCairoContext,
			cVisibleZoneImageFile,
			&fVisibleZoneWidth,
			&fVisibleZoneHeight,
			0.,
			fVisibleZoneAlpha,
			FALSE);
		cairo_destroy (pCairoContext);
	}
	else
		g_pVisibleZoneSurface = NULL;
}

cairo_surface_t *cairo_dock_load_stripes (cairo_t* pSourceContext, int iStripesWidth, int iStripesHeight, double fRotationAngle)
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

	if (fRotationAngle != 0)
	{
		cairo_surface_t *pNewSurfaceRotated = _cairo_dock_create_blank_surface (pSourceContext,
			iStripesHeight,
			iStripesWidth);
		pImageContext = cairo_create (pNewSurfaceRotated);

		if (fRotationAngle < 0)
		{
			cairo_move_to (pImageContext, iStripesWidth, 0);
			cairo_rotate (pImageContext, fRotationAngle);
			cairo_translate (pImageContext, - iStripesWidth, 0);
		}
		else
		{
			cairo_move_to (pImageContext, 0, 0);
			cairo_rotate (pImageContext, fRotationAngle);
			cairo_translate (pImageContext, 0, - iStripesHeight);
		}
		cairo_set_source_surface (pImageContext, pNewSurface, 0, 0);

		cairo_paint (pImageContext);
		cairo_surface_destroy (pNewSurface);
		cairo_destroy (pImageContext);
		pNewSurface = pNewSurfaceRotated;
	}

	return pNewSurface;
}



void cairo_dock_update_background_decorations_if_necessary (CairoDock *pDock, int iNewDecorationsWidth, int iNewDecorationsHeight)
{
	//g_print ("%s (%dx%d) [%.2fx%.2f]\n", __func__, iNewDecorationsWidth, iNewDecorationsHeight, g_fBackgroundImageWidth, g_fBackgroundImageHeight);
	if (2 * iNewDecorationsWidth > g_fBackgroundImageWidth || iNewDecorationsHeight > g_fBackgroundImageHeight)
	{
		if (g_pBackgroundSurface != NULL)
		{
			cairo_surface_destroy (g_pBackgroundSurface);
			g_pBackgroundSurface = NULL;
		}
		if (g_pBackgroundSurfaceFull != NULL)
		{
			cairo_surface_destroy (g_pBackgroundSurfaceFull);
			g_pBackgroundSurfaceFull = NULL;
		}
		cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
		
		if (myBackground.cBackgroundImageFile != NULL)
		{
			if (myBackground.bBackgroundImageRepeat)
			{
				g_fBackgroundImageWidth = MAX (g_fBackgroundImageWidth, 2 * iNewDecorationsWidth);
				g_fBackgroundImageHeight = MAX (g_fBackgroundImageHeight, iNewDecorationsHeight);
				g_pBackgroundSurfaceFull = cairo_dock_load_image (pCairoContext,
					myBackground.cBackgroundImageFile,
					&g_fBackgroundImageWidth,
					&g_fBackgroundImageHeight,
					0.,
					myBackground.fBackgroundImageAlpha,
					myBackground.bBackgroundImageRepeat);
			}
			else/** if (g_fBackgroundImageWidth == 0 || g_fBackgroundImageHeight == 0)*/
			{
				g_fBackgroundImageWidth = MAX (g_fBackgroundImageWidth, iNewDecorationsWidth);  /// 0
				g_fBackgroundImageHeight = MAX (g_fBackgroundImageHeight, iNewDecorationsHeight);
				g_pBackgroundSurface = cairo_dock_load_image (pCairoContext,
					myBackground.cBackgroundImageFile,
					&g_fBackgroundImageWidth,
					&g_fBackgroundImageHeight,
					0.,
					myBackground.fBackgroundImageAlpha,
					myBackground.bBackgroundImageRepeat);
			}
		}
		else
		{
			g_fBackgroundImageWidth = MAX (g_fBackgroundImageWidth, 2 * iNewDecorationsWidth);
			g_fBackgroundImageHeight = MAX (g_fBackgroundImageHeight, iNewDecorationsHeight);
			g_pBackgroundSurfaceFull = cairo_dock_load_stripes (pCairoContext,
				g_fBackgroundImageWidth,
				g_fBackgroundImageHeight,
				0.);
		}
		
		if (g_bUseOpenGL)
		{
			if (g_iBackgroundTexture != 0)
				_cairo_dock_delete_texture (g_iBackgroundTexture);
			g_iBackgroundTexture = cairo_dock_create_texture_from_surface (g_pBackgroundSurfaceFull != NULL ? g_pBackgroundSurfaceFull : g_pBackgroundSurface);
		}
		
		cairo_destroy (pCairoContext);
		cd_debug ("  MaJ des decorations du fond -> %.2fx%.2f", g_fBackgroundImageWidth, g_fBackgroundImageHeight);
	}
}


void cairo_dock_load_background_decorations (CairoDock *pDock)
{
	int iWidth, iHeight;
	cairo_dock_search_max_decorations_size (&iWidth, &iHeight);
	
	g_fBackgroundImageWidth = 0;
	g_fBackgroundImageHeight = 0;
	cairo_dock_update_background_decorations_if_necessary (pDock, iWidth, iHeight);
}


void cairo_dock_load_icons_background_surface (const gchar *cImagePath, cairo_t* pSourceContext, double fMaxScale)
{
	if (g_pIconBackgroundImageSurface != NULL)
	{
		cairo_surface_destroy (g_pIconBackgroundImageSurface);
		g_pIconBackgroundImageSurface = NULL;
	}

	// On creait deux surfaces: une de la taille des launcher et une de la taille des appli.
	if( cImagePath != NULL )
	{
		int iSize = MAX (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI]);
		if (iSize == 0)
			iSize = 48;
		g_iIconBackgroundImageWidth = g_iIconBackgroundImageHeight = 0;

		g_pIconBackgroundImageSurface = cairo_dock_create_surface_from_image (cImagePath,
			pSourceContext,
			fMaxScale,
			iSize,   /* largeur avant creation [IN] */
			iSize, /* hauteur avant creation [IN] */
			CAIRO_DOCK_FILL_SPACE,
			&g_iIconBackgroundImageWidth, &g_iIconBackgroundImageHeight,  /* largeur et hauteur apres creation [OUT] */
			NULL, NULL); /* zoom applique [OUT] */
	}
}



void cairo_dock_load_desktop_background_surface (void)  // attention : fonction lourde.
{
	cairo_dock_invalidate_desktop_bg_surface ();
	
	Pixmap iRootPixmapID = cairo_dock_get_window_background_pixmap (cairo_dock_get_root_id ());
	g_return_if_fail (iRootPixmapID != 0);
	
	GdkPixbuf *pBgPixbuf = cairo_dock_get_pixbuf_from_pixmap (iRootPixmapID, FALSE);  // on n'y ajoute pas de transparence.
	if (pBgPixbuf != NULL)
	{
		cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (g_pMainDock));
		
		if (gdk_pixbuf_get_height (pBgPixbuf) == 1 && gdk_pixbuf_get_width(pBgPixbuf) == 1)  // couleur unie.
		{
			guchar *pixels = gdk_pixbuf_get_pixels (pBgPixbuf);
			cd_message ("c'est une couleur unie (%.2f, %.2f, %.2f)", (double) pixels[0] / 255, (double) pixels[1] / 255, (double) pixels[2] / 255);
			
			g_pDesktopBgSurface = _cairo_dock_create_blank_surface (pSourceContext,
				g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL],
				g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
			
			cairo_t *pCairoContext = cairo_create (g_pDesktopBgSurface);
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
			cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (g_pMainDock));
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
				cd_message ("c'est un degrade ou un motif (%dx%d)", (int) fWidth, (int) fHeight);
				g_pDesktopBgSurface = _cairo_dock_create_blank_surface (pSourceContext,
					g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL],
					g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
				cairo_t *pCairoContext = cairo_create (g_pDesktopBgSurface);
				
				cairo_pattern_t *pPattern = cairo_pattern_create_for_surface (pBgSurface);
				g_return_if_fail (cairo_pattern_status (pPattern) == CAIRO_STATUS_SUCCESS);
				cairo_pattern_set_extend (pPattern, CAIRO_EXTEND_REPEAT);
				
				cairo_set_source (pCairoContext, pPattern);
				cairo_paint (pCairoContext);
				
				cairo_destroy (pCairoContext);
				cairo_pattern_destroy (pPattern);
				cairo_surface_destroy (pBgSurface);
			}
			else
			{
				cd_message ("c'est un fond d'ecran de taille %dx%d", (int) fWidth, (int) fHeight);
				g_pDesktopBgSurface = pBgSurface;
			}
			
			g_object_unref (pBgPixbuf);
		}
		
		cairo_destroy (pSourceContext);
	}
	if (g_pDesktopBgSurface != NULL && g_bUseOpenGL)
	{
		g_iDesktopBgTexture = cairo_dock_create_texture_from_surface (g_pDesktopBgSurface);
	}
}

void cairo_dock_invalidate_desktop_bg_surface (void)
{
	if (g_pDesktopBgSurface != NULL)
	{
		cairo_surface_destroy (g_pDesktopBgSurface);
		g_pDesktopBgSurface = NULL;
	}
	if (g_iDesktopBgTexture != 0)
	{
		_cairo_dock_delete_texture (g_iDesktopBgTexture);
		g_iDesktopBgTexture = 0;
	}
}

cairo_surface_t *cairo_dock_get_desktop_bg_surface (void)
{
	if (g_pDesktopBgSurface == NULL)
		cairo_dock_load_desktop_background_surface ();
	return g_pDesktopBgSurface;
}



void cairo_dock_load_task_indicator (const gchar *cIndicatorImagePath, cairo_t* pSourceContext, double fMaxScale, double fIndicatorRatio)
{
	if (g_pIndicatorSurface != NULL)
	{
		cairo_surface_destroy (g_pIndicatorSurface);
		g_pIndicatorSurface = NULL;
	}
	if (g_iIndicatorTexture != 0)
	{
		_cairo_dock_delete_texture (g_iIndicatorTexture);
		g_iIndicatorTexture = 0;
	}
	if (cIndicatorImagePath != NULL)
	{
		double fLauncherWidth = (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : 48);
		double fLauncherHeight = (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : 48);
		
		double fScale = (myIndicators.bLinkIndicatorWithIcon ? fMaxScale : 1);
		g_pIndicatorSurface = cairo_dock_create_surface_from_image (
			cIndicatorImagePath,
			pSourceContext,
			fScale,
			fLauncherWidth * fIndicatorRatio,
			fLauncherHeight * fIndicatorRatio,
			CAIRO_DOCK_KEEP_RATIO,
			&g_fIndicatorWidth,
			&g_fIndicatorHeight,
			NULL, NULL);
		cd_debug ("g_pIndicatorSurface : %.2fx%.2f", g_fIndicatorWidth, g_fIndicatorHeight);
	}
}

void cairo_dock_load_active_window_indicator (cairo_t* pSourceContext, const gchar *cImagePath, double fMaxScale, double fCornerRadius, double fLineWidth, double *fActiveColor)
{
	if (g_pActiveIndicatorSurface != NULL)
	{
		cairo_surface_destroy (g_pActiveIndicatorSurface);
		g_pActiveIndicatorSurface = NULL;
	}
	if (g_iActiveIndicatorTexture != 0)
	{
		_cairo_dock_delete_texture (g_iActiveIndicatorTexture);
		g_iActiveIndicatorTexture = 0;
	}
	g_fActiveIndicatorWidth = MAX (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI]);
	g_fActiveIndicatorHeight = MAX (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI], myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI]);
	if (g_fActiveIndicatorWidth == 0)
		g_fActiveIndicatorWidth = 48;
	if (g_fActiveIndicatorHeight == 0)
		g_fActiveIndicatorHeight = 48;
	
	if (cImagePath != NULL)
	{
		g_pActiveIndicatorSurface = cairo_dock_create_surface_for_icon (cImagePath,
			pSourceContext,
			g_fActiveIndicatorWidth * fMaxScale,
			g_fActiveIndicatorHeight * fMaxScale);
	}
	else if (fActiveColor[3] > 0)
	{
		g_pActiveIndicatorSurface = _cairo_dock_create_blank_surface (pSourceContext,
			g_fActiveIndicatorWidth * fMaxScale,
			g_fActiveIndicatorHeight * fMaxScale);
		cairo_t *pCairoContext = cairo_create (g_pActiveIndicatorSurface);
		
		fCornerRadius = MIN (fCornerRadius, (g_fActiveIndicatorWidth * fMaxScale - fLineWidth) / 2);
		double fFrameWidth = g_fActiveIndicatorWidth * fMaxScale - (2 * fCornerRadius + fLineWidth);
		double fFrameHeight = g_fActiveIndicatorHeight * fMaxScale - 2 * fLineWidth;
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
	}
}

void cairo_dock_load_class_indicator (const gchar *cIndicatorImagePath, cairo_t* pSourceContext, double fMaxScale)
{
	if (g_pClassIndicatorSurface != NULL)
	{
		cairo_surface_destroy (g_pClassIndicatorSurface);
		g_pClassIndicatorSurface = NULL;
	}
	if (g_iClassIndicatorTexture != 0)
	{
		_cairo_dock_delete_texture (g_iClassIndicatorTexture);
		g_iClassIndicatorTexture = 0;
	}
	if (cIndicatorImagePath != NULL)
	{
		double fLauncherWidth = (myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : 48);
		double fLauncherHeight = (myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != 0 ? myIcons.tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : 48);
		
		double fScale = (myIndicators.bLinkIndicatorWithIcon ? fMaxScale : 1.);
		fScale = 1;
		g_pClassIndicatorSurface = cairo_dock_create_surface_from_image (
			cIndicatorImagePath,
			pSourceContext,
			fScale,
			fLauncherWidth/3,
			fLauncherHeight/3,
			CAIRO_DOCK_KEEP_RATIO,
			&g_fClassIndicatorWidth,
			&g_fClassIndicatorHeight,
			NULL, NULL);
	}
}


void cairo_dock_unload_additionnal_textures (void)
{
	if (g_iBackgroundTexture != 0)
	{
		_cairo_dock_delete_texture (g_iBackgroundTexture);
		g_iBackgroundTexture = 0;
	}
	if (g_iVisibleZoneTexture != 0)
	{
		_cairo_dock_delete_texture (g_iVisibleZoneTexture);
		g_iVisibleZoneTexture = 0;
	}
	if (g_iIndicatorTexture != 0)
	{
		_cairo_dock_delete_texture (g_iIndicatorTexture);
		g_iIndicatorTexture = 0;
	}
	if (g_iActiveIndicatorTexture != 0)
	{
		_cairo_dock_delete_texture (g_iActiveIndicatorTexture);
		g_iActiveIndicatorTexture = 0;
	}
	if (g_iClassIndicatorTexture != 0)
	{
		_cairo_dock_delete_texture (g_iClassIndicatorTexture);
		g_iClassIndicatorTexture = 0;
	}
	cairo_dock_unload_desklet_buttons_texture ();
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
	cairo_dock_invalidate_desktop_bg_surface ();
	cairo_dock_destroy_icon_pbuffer ();
}
