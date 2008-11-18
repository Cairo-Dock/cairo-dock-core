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
#include "cairo-dock-load.h"

extern CairoDock *g_pMainDock;
extern double g_fSubDockSizeRatio;
extern gboolean g_bSameHorizontality;

extern gint g_iScreenWidth[2];
extern gint g_iScreenHeight[2];

extern int g_iSinusoidWidth;
extern gint g_iDockLineWidth;
extern gint g_iDockRadius;
extern gint g_iFrameMargin;
extern double g_fAmplitude;
extern int g_iIconGap;
extern double g_fAlbedo;

extern cairo_surface_t *g_pVisibleZoneSurface;
extern gboolean g_bReverseVisibleImage;

extern CairoDockLabelDescription g_iconTextDescription;
extern CairoDockLabelDescription g_quickInfoTextDescription;
extern gboolean g_bTextAlwaysHorizontal;

extern gchar *g_cCurrentThemePath;

extern int g_iDockRadius;
extern int g_iDockLineWidth;

extern gchar *g_cBackgroundImageFile;
extern double g_fBackgroundImageAlpha;
extern cairo_surface_t *g_pBackgroundSurface[2];
extern cairo_surface_t *g_pBackgroundSurfaceFull[2];
extern double g_fBackgroundImageWidth, g_fBackgroundImageHeight;
extern gboolean g_bBackgroundImageRepeat;
extern int g_iNbStripes;
extern double g_fStripesAngle;
extern double g_fStripesWidth;
extern double g_fStripesColorBright[4];
extern double g_fStripesColorDark[4];

extern unsigned int g_iAppliMaxNameLength;

extern int g_tIconAuthorizedWidth[CAIRO_DOCK_NB_TYPES];
extern int g_tIconAuthorizedHeight[CAIRO_DOCK_NB_TYPES];
extern gboolean g_bOverWriteXIcons;
extern gboolean g_bShowThumbnail;

extern cairo_surface_t *g_pDropIndicatorSurface;
extern double g_fDropIndicatorWidth, g_fDropIndicatorHeight;
extern cairo_surface_t *g_pIndicatorSurface[2];
extern gboolean g_bLinkIndicatorWithIcon;
extern double g_fIndicatorWidth, g_fIndicatorHeight;

extern cairo_surface_t *g_pActiveIndicatorSurface;
extern double g_fActiveIndicatorWidth, g_fActiveIndicatorHeight;

extern cairo_surface_t *g_pDesktopBgSurface;

extern gboolean g_bUseOpenGL;
extern GLuint g_iBackgroundTexture;
extern GLuint g_iIndicatorTexture;

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
			/*cairo_surface_t *pNewSurfaceFilled = cairo_surface_create_similar (cairo_get_target (pSourceContext),
				CAIRO_CONTENT_COLOR_ALPHA,
				iDesiredWidth,
				iDesiredHeight);*/
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
	
			cairo_surface_destroy (pNewSurface);
			pNewSurface = pNewSurfaceFilled;
			*fImageWidth = iDesiredWidth;
			*fImageHeight = iDesiredHeight;
		}
		
		if (fAlpha < 1)
		{
			/*cairo_surface_t *pNewSurfaceAlpha = cairo_surface_create_similar (cairo_get_target (pSourceContext),
				CAIRO_CONTENT_COLOR_ALPHA,
				*fImageWidth,
				*fImageHeight);*/
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

cairo_surface_t *cairo_dock_load_image_for_icon (cairo_t *pSourceContext, const gchar *cImageFile, double fImageWidth, double fImageHeight)
{
	double fImageWidth_ = fImageWidth, fImageHeight_ = fImageHeight;
	return cairo_dock_load_image (pSourceContext, cImageFile, &fImageWidth_, &fImageHeight_, 0., 1., FALSE);
}


void cairo_dock_load_reflect_on_icon (Icon *icon, cairo_t *pSourceContext, gdouble fMaxScale, gboolean bHorizontalDock, gboolean bDirectionUp)
{
	if (g_fAlbedo > 0 && icon->pIconBuffer != NULL && ! (CAIRO_DOCK_IS_APPLET (icon) && icon->acFileName == NULL))
	{
		icon->pReflectionBuffer = cairo_dock_create_reflection_surface (icon->pIconBuffer,
			pSourceContext,
			(bHorizontalDock ? icon->fWidth : icon->fHeight) * fMaxScale,
			(bHorizontalDock ? icon->fHeight : icon->fWidth) * fMaxScale,
			bHorizontalDock,
			fMaxScale,
			bDirectionUp);

		icon->pFullIconBuffer = cairo_dock_create_icon_surface_with_reflection (icon->pIconBuffer,
			icon->pReflectionBuffer,
			pSourceContext,
			(bHorizontalDock ? icon->fWidth : icon->fHeight) * fMaxScale,
			(bHorizontalDock ? icon->fHeight : icon->fWidth) * fMaxScale,
			bHorizontalDock,
			fMaxScale,
			bDirectionUp);
	}
}

void cairo_dock_fill_one_icon_buffer (Icon *icon, cairo_t* pSourceContext, gdouble fMaxScale, gboolean bHorizontalDock, gboolean bApplySizeRestriction, gboolean bDirectionUp)
{
	//g_print ("%s (%d, %.2f, %s)\n", __func__, icon->iType, fMaxScale, icon->acFileName);
	cairo_surface_destroy (icon->pIconBuffer);
	icon->pIconBuffer = NULL;
	glDeleteTextures (1, &icon->iIconTexture);
	icon->iIconTexture = 0;
	cairo_surface_destroy (icon->pReflectionBuffer);
	icon->pReflectionBuffer = NULL;
	glDeleteTextures (1, &icon->iReflectionTexture);
	icon->iReflectionTexture = 0;
	cairo_surface_destroy (icon->pFullIconBuffer);
	icon->pFullIconBuffer = NULL;
	glDeleteTextures (1, &icon->iFullIconTexture);
	icon->iFullIconTexture = 0;
	
	if (icon->fWidth < 0 || icon->fHeight < 0)  // on ne veut pas de surface.
		return;
	
	if (CAIRO_DOCK_IS_LAUNCHER (icon) || (CAIRO_DOCK_IS_USER_SEPARATOR (icon) && icon->acFileName != NULL))
	{
		//\_______________________ On recherche une icone.
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->acFileName);
		//g_print (" -> %s\n", cIconPath);

		//\_______________________ On cree la surface cairo a afficher.
		if (cIconPath != NULL && *cIconPath != '\0')
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_image (cIconPath,
				pSourceContext,
				fMaxScale,
				(bApplySizeRestriction ? g_tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : icon->fWidth),
				(bApplySizeRestriction ? g_tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : icon->fHeight),
				CAIRO_DOCK_FILL_SPACE,
				(bHorizontalDock ? &icon->fWidth : &icon->fHeight),
				(bHorizontalDock ? &icon->fHeight : &icon->fWidth),
				NULL, NULL);
		}
		
		g_free (cIconPath);
	}
	else if (CAIRO_DOCK_IS_APPLET (icon))  // c'est l'icône d'une applet.
	{
		//g_print ("  icon->acFileName : %s\n", icon->acFileName);
		icon->pIconBuffer = cairo_dock_create_applet_surface (icon->acFileName, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight, bApplySizeRestriction);
	}
	else if (CAIRO_DOCK_IS_APPLI (icon))  // c'est l'icône d'une appli valide. Dans cet ordre on n'a pas besoin de verifier que c'est NORMAL_APPLI.
	{
		if (g_bOverWriteXIcons && ! cairo_dock_class_is_using_xicon (icon->cClass) && ! (g_bShowThumbnail && icon->bIsHidden))
			icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight);
		if (icon->pIconBuffer == NULL && g_bShowThumbnail && icon->bIsHidden && icon->iBackingPixmap != 0)
			icon->pIconBuffer = cairo_dock_create_surface_from_xpixmap (icon->iBackingPixmap, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight);
		if (icon->pIconBuffer == NULL)
			icon->pIconBuffer = cairo_dock_create_surface_from_xwindow (icon->Xid, pSourceContext, fMaxScale, &icon->fWidth, &icon->fHeight);
	}
	else  // c'est une icone de separation.
	{
		icon->pIconBuffer = cairo_dock_create_separator_surface (pSourceContext, fMaxScale, bHorizontalDock, bDirectionUp, &icon->fWidth, &icon->fHeight);
	}

	if (icon->pIconBuffer == NULL)
	{
		gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_DEFAULT_ICON_NAME);
		CairoDockIconType iType = cairo_dock_get_icon_type  (icon);
		icon->pIconBuffer = cairo_dock_create_surface_from_image (cIconPath,
			pSourceContext,
			fMaxScale,
			(bApplySizeRestriction ? g_tIconAuthorizedWidth[iType] : icon->fWidth),
			(bApplySizeRestriction ? g_tIconAuthorizedHeight[iType] : icon->fHeight),
			CAIRO_DOCK_FILL_SPACE,
			(bHorizontalDock ? &icon->fWidth : &icon->fHeight),
			(bHorizontalDock ? &icon->fHeight : &icon->fWidth),
			NULL, NULL);
		g_free (cIconPath);
	}
	cd_debug ("%s () -> %.2fx%.2f", __func__, icon->fWidth, icon->fHeight);

	if (g_fAlbedo > 0 && icon->pIconBuffer != NULL && ! (CAIRO_DOCK_IS_APPLET (icon) && icon->acFileName == NULL))
	{
		icon->pReflectionBuffer = cairo_dock_create_reflection_surface (icon->pIconBuffer,
			pSourceContext,
			(bHorizontalDock ? icon->fWidth : icon->fHeight) * fMaxScale,
			(bHorizontalDock ? icon->fHeight : icon->fWidth) * fMaxScale,
			bHorizontalDock,
			fMaxScale,
			bDirectionUp);

		icon->pFullIconBuffer = cairo_dock_create_icon_surface_with_reflection (icon->pIconBuffer,
			icon->pReflectionBuffer,
			pSourceContext,
			(bHorizontalDock ? icon->fWidth : icon->fHeight) * fMaxScale,
			(bHorizontalDock ? icon->fHeight : icon->fWidth) * fMaxScale,
			bHorizontalDock,
			fMaxScale,
			bDirectionUp);
	}
	
	if (g_bUseOpenGL && icon->pIconBuffer != NULL)
	{
		GdkGLContext* pGlContext = gtk_widget_get_gl_context (g_pMainDock->pWidget);
		GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (g_pMainDock->pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return ;
		
		icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
		
		if (icon->pReflectionBuffer != NULL)
		{
			icon->iReflectionTexture = cairo_dock_create_texture_from_surface (icon->pReflectionBuffer);
		}
		
		if (icon->pFullIconBuffer != NULL)
		{
			icon->iFullIconTexture = cairo_dock_create_texture_from_surface (icon->pFullIconBuffer);
		}
		
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
}

gchar *cairo_dock_cut_string (const gchar *cString, int iNbCaracters)  // gere l'UTF-8
{
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

void cairo_dock_fill_one_text_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription, gboolean bHorizontalDock, gboolean bDirectionUp)
{
	//g_print ("%s (%s, %d)\n", __func__, cLabelPolice, iLabelSize);
	cairo_surface_destroy (icon->pTextBuffer);
	icon->pTextBuffer = NULL;
	glDeleteTextures (1, &icon->iLabelTexture);
	icon->iLabelTexture = 0;
	if (icon->acName == NULL || (pTextDescription->iSize == 0))
		return ;

	gchar *cTruncatedName = NULL;
	if (CAIRO_DOCK_IS_APPLI (icon) && g_iAppliMaxNameLength > 0)
	{
		cTruncatedName = cairo_dock_cut_string (icon->acName, g_iAppliMaxNameLength);
	}

	cairo_surface_t* pNewSurface = cairo_dock_create_surface_from_text ((cTruncatedName != NULL ? cTruncatedName : icon->acName),
		pSourceContext,
		pTextDescription,
		1.,
		&icon->iTextWidth, &icon->iTextHeight, &icon->fTextXOffset, &icon->fTextYOffset);
	g_free (cTruncatedName);
	//g_print (" -> %s : (%.2f;%.2f) %dx%d\n", icon->acName, icon->fTextXOffset, icon->fTextYOffset, icon->iTextWidth, icon->iTextHeight);

	double fRotationAngle = (bHorizontalDock ? 0 : (bDirectionUp ? -G_PI/2 : G_PI/2));
	cairo_surface_t *pNewSurfaceRotated = cairo_dock_rotate_surface (pNewSurface, pSourceContext, icon->iTextWidth, icon->iTextHeight, fRotationAngle);
	if (pNewSurfaceRotated != NULL)
	{
		cairo_surface_destroy (pNewSurface);
		pNewSurface = pNewSurfaceRotated;
	}

	icon->pTextBuffer = pNewSurface;
	
	if (g_bUseOpenGL && icon->pTextBuffer != NULL)
	{
		GdkGLContext* pGlContext = gtk_widget_get_gl_context (g_pMainDock->pWidget);
		GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (g_pMainDock->pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return ;
		
		icon->iLabelTexture = cairo_dock_create_texture_from_surface (icon->pTextBuffer);
		
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
}

void cairo_dock_fill_one_quick_info_buffer (Icon *icon, cairo_t* pSourceContext, CairoDockLabelDescription *pTextDescription, double fMaxScale)
{
	cairo_surface_destroy (icon->pQuickInfoBuffer);
	icon->pQuickInfoBuffer = NULL;
	glDeleteTextures (1, &icon->iQuickInfoTexture);
	icon->iQuickInfoTexture = 0;
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
		GdkGLContext* pGlContext = gtk_widget_get_gl_context (g_pMainDock->pWidget);
		GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (g_pMainDock->pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return ;
		
		icon->iQuickInfoTexture = cairo_dock_create_texture_from_surface (icon->pQuickInfoBuffer);
		
		
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
}



void cairo_dock_fill_icon_buffers (Icon *icon, cairo_t *pSourceContext, double fMaxScale, gboolean bHorizontalDock, gboolean bApplySizeRestriction, gboolean bDirectionUp)
{
	cairo_dock_fill_one_icon_buffer (icon, pSourceContext, fMaxScale, bHorizontalDock, bApplySizeRestriction, bDirectionUp);

	cairo_dock_fill_one_text_buffer (icon, pSourceContext, &g_iconTextDescription, (g_bTextAlwaysHorizontal ? CAIRO_DOCK_HORIZONTAL : bHorizontalDock), bDirectionUp);

	cairo_dock_fill_one_quick_info_buffer (icon, pSourceContext, &g_quickInfoTextDescription, fMaxScale);
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

	double fFlatDockWidth = - g_iIconGap;
	pDock->iMaxIconHeight = 0;

	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	///double fMaxScale = 1 + g_fAmplitude;

	Icon* icon;
	GList* ic;
	//double fRatio = (pDock->iRefCount == 0 ? 1 : g_fSubDockSizeRatio);
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		
		if (CAIRO_DOCK_IS_APPLET (icon))
		{
			if (bReloadAppletsToo)
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
		fFlatDockWidth += g_iIconGap + icon->fWidth;
		pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);
	}
	pDock->fFlatDockWidth = (int) fFlatDockWidth;  /// (int) n'est plus tellement necessaire ...
	cairo_destroy (pCairoContext);
}


void cairo_dock_load_visible_zone (CairoDock *pDock, gchar *cVisibleZoneImageFile, int iVisibleZoneWidth, int iVisibleZoneHeight, double fVisibleZoneAlpha)
{
	double fVisibleZoneWidth = iVisibleZoneWidth, fVisibleZoneHeight = iVisibleZoneHeight;
	if (g_pVisibleZoneSurface != NULL)
		cairo_surface_destroy (g_pVisibleZoneSurface);
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	if (! g_bUseOpenGL)
	{
		g_pVisibleZoneSurface = cairo_dock_load_image (pCairoContext,
			cVisibleZoneImageFile,
			&fVisibleZoneWidth,
			&fVisibleZoneHeight,
			(pDock->bHorizontalDock ? (! pDock->bDirectionUp && g_bReverseVisibleImage ? G_PI : 0) : (pDock->bDirectionUp ? -G_PI/2 : G_PI/2)),
			fVisibleZoneAlpha,
			FALSE);
	}
	else
	{
		g_pVisibleZoneSurface = cairo_dock_load_image (pCairoContext,
			cVisibleZoneImageFile,
			&fVisibleZoneWidth,
			&fVisibleZoneHeight,
			0.,
			fVisibleZoneAlpha,
			FALSE);
	}
	cairo_destroy (pCairoContext);
}

cairo_surface_t *cairo_dock_load_stripes (cairo_t* pSourceContext, int iStripesWidth, int iStripesHeight, double fRotationAngle)
{
	g_return_val_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	cairo_pattern_t *pStripesPattern;
	double fWidth = (g_iNbStripes > 0 ? 200. : iStripesWidth);
	if (fabs (g_fStripesAngle) != 90)
		pStripesPattern = cairo_pattern_create_linear (0.0f,
			0.0f,
			fWidth,
			fWidth * tan (g_fStripesAngle * G_PI/180.));
	else
		pStripesPattern = cairo_pattern_create_linear (0.0f,
			0.0f,
			0.,
			(g_fStripesAngle == 90) ? iStripesHeight : - iStripesHeight);
	g_return_val_if_fail (cairo_pattern_status (pStripesPattern) == CAIRO_STATUS_SUCCESS, NULL);

	cairo_pattern_set_extend (pStripesPattern, CAIRO_EXTEND_REPEAT);

	if (g_iNbStripes > 0)
	{
		gdouble fStep;
		double fStripesGap = 1. / (g_iNbStripes);  // ecart entre 2 rayures foncees.
		for (fStep = 0.0f; fStep < 1.0f; fStep += fStripesGap)
		{
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep - g_fStripesWidth / 2,
				g_fStripesColorBright[0],
				g_fStripesColorBright[1],
				g_fStripesColorBright[2],
				g_fStripesColorBright[3]);
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep,
				g_fStripesColorDark[0],
				g_fStripesColorDark[1],
				g_fStripesColorDark[2],
				g_fStripesColorDark[3]);
			cairo_pattern_add_color_stop_rgba (pStripesPattern,
				fStep + g_fStripesWidth / 2,
				g_fStripesColorBright[0],
				g_fStripesColorBright[1],
				g_fStripesColorBright[2],
				g_fStripesColorBright[3]);
		}
	}
	else
	{
		cairo_pattern_add_color_stop_rgba (pStripesPattern,
			0.,
			g_fStripesColorDark[0],
			g_fStripesColorDark[1],
			g_fStripesColorDark[2],
			g_fStripesColorDark[3]);
		cairo_pattern_add_color_stop_rgba (pStripesPattern,
			1.,
			g_fStripesColorBright[0],
			g_fStripesColorBright[1],
			g_fStripesColorBright[2],
			g_fStripesColorBright[3]);
	}

	/*cairo_surface_t *pNewSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
		CAIRO_CONTENT_COLOR_ALPHA,
		iStripesWidth,
		iStripesHeight);*/
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
		/*cairo_surface_t *pNewSurfaceRotated = cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			iStripesHeight,
			iStripesWidth);*/
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
		cairo_surface_destroy (g_pBackgroundSurface[0]);
		g_pBackgroundSurface[0] = NULL;
		cairo_surface_destroy (g_pBackgroundSurface[1]);
		g_pBackgroundSurface[1] = NULL;
		cairo_surface_destroy (g_pBackgroundSurfaceFull[0]);
		g_pBackgroundSurfaceFull[0] = NULL;
		cairo_surface_destroy (g_pBackgroundSurfaceFull[1]);
		g_pBackgroundSurfaceFull[1] = NULL;
		cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
		
		if (g_cBackgroundImageFile != NULL)
		{
			if (g_bBackgroundImageRepeat)
			{
				g_fBackgroundImageWidth = MAX (g_fBackgroundImageWidth, 2 * iNewDecorationsWidth);
				g_fBackgroundImageHeight = MAX (g_fBackgroundImageHeight, iNewDecorationsHeight);
				g_pBackgroundSurfaceFull[CAIRO_DOCK_HORIZONTAL] = cairo_dock_load_image (pCairoContext,
					g_cBackgroundImageFile,
					&g_fBackgroundImageWidth,
					&g_fBackgroundImageHeight,
					0,  // (pDock->bHorizontalDock ? 0 : (pDock->bDirectionUp ? -G_PI/2 : G_PI/2)),
					g_fBackgroundImageAlpha,
					g_bBackgroundImageRepeat);
					
				g_pBackgroundSurfaceFull[CAIRO_DOCK_VERTICAL] = cairo_dock_rotate_surface (g_pBackgroundSurfaceFull[CAIRO_DOCK_HORIZONTAL], pCairoContext, g_fBackgroundImageWidth, g_fBackgroundImageHeight, (pDock->bDirectionUp ? -G_PI/2 : G_PI/2));
			}
			else/** if (g_fBackgroundImageWidth == 0 || g_fBackgroundImageHeight == 0)*/
			{
				g_fBackgroundImageWidth = MAX (g_fBackgroundImageWidth, iNewDecorationsWidth);  /// 0
				g_fBackgroundImageHeight = MAX (g_fBackgroundImageHeight, iNewDecorationsHeight);
				g_pBackgroundSurface[CAIRO_DOCK_HORIZONTAL] = cairo_dock_load_image (pCairoContext,
					g_cBackgroundImageFile,
					&g_fBackgroundImageWidth,
					&g_fBackgroundImageHeight,
					0,  // (pDock->bHorizontalDock ? 0 : (pDock->bDirectionUp ? -G_PI/2 : G_PI/2)),
					g_fBackgroundImageAlpha,
					g_bBackgroundImageRepeat);
					
				g_pBackgroundSurface[CAIRO_DOCK_VERTICAL] = cairo_dock_rotate_surface (g_pBackgroundSurface[CAIRO_DOCK_HORIZONTAL], pCairoContext, g_fBackgroundImageWidth, g_fBackgroundImageHeight, (pDock->bDirectionUp ? -G_PI/2 : G_PI/2));
			}
		}
		else
		{
			g_fBackgroundImageWidth = MAX (g_fBackgroundImageWidth, 2 * iNewDecorationsWidth);
			g_fBackgroundImageHeight = MAX (g_fBackgroundImageHeight, iNewDecorationsHeight);
			g_pBackgroundSurfaceFull[CAIRO_DOCK_HORIZONTAL] = cairo_dock_load_stripes (pCairoContext, g_fBackgroundImageWidth, g_fBackgroundImageHeight, 0);  // (pDock->bHorizontalDock ? 0 : (pDock->bDirectionUp ? -G_PI/2 : G_PI/2))
			
			g_pBackgroundSurfaceFull[CAIRO_DOCK_VERTICAL] = cairo_dock_rotate_surface (g_pBackgroundSurfaceFull[CAIRO_DOCK_HORIZONTAL], pCairoContext, g_fBackgroundImageWidth, g_fBackgroundImageHeight, (pDock->bDirectionUp ? -G_PI/2 : G_PI/2));
		}
		
		if (g_bUseOpenGL)
		{
			GdkGLContext* pGlContext = gtk_widget_get_gl_context (g_pMainDock->pWidget);
			GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (g_pMainDock->pWidget);
			if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
				return ;
			
			if (g_iBackgroundTexture != 0)
				glDeleteTextures (1, &g_iBackgroundTexture);
			g_iBackgroundTexture = cairo_dock_create_texture_from_surface (g_pBackgroundSurfaceFull[CAIRO_DOCK_HORIZONTAL] != NULL ? g_pBackgroundSurfaceFull[CAIRO_DOCK_HORIZONTAL] : g_pBackgroundSurface[CAIRO_DOCK_HORIZONTAL]);
			
			gdk_gl_drawable_gl_end (pGlDrawable);
		}
		
		
		cairo_destroy (pCairoContext);
		g_print ("  MaJ des decorations du fond -> %.2fx%.2f\n", g_fBackgroundImageWidth, g_fBackgroundImageHeight);
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


void cairo_dock_load_drop_indicator (gchar *cImagePath, cairo_t* pSourceContext, double fMaxScale)
{
	if (g_pDropIndicatorSurface != NULL)
		cairo_surface_destroy (g_pDropIndicatorSurface);
	g_pDropIndicatorSurface = cairo_dock_create_surface_from_image (cImagePath,
		pSourceContext,
		1.,
		g_tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] * fMaxScale,
		g_tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] * fMaxScale / 2,
		CAIRO_DOCK_KEEP_RATIO,
		&g_fDropIndicatorWidth, &g_fDropIndicatorHeight,
		NULL, NULL);
}


void cairo_dock_load_task_indicator (const gchar *cIndicatorImagePath, cairo_t* pSourceContext, double fMaxScale, double fIndicatorRatio)
{
	cairo_surface_destroy (g_pIndicatorSurface[0]);
	cairo_surface_destroy (g_pIndicatorSurface[1]);
	g_pIndicatorSurface[0] = NULL;
	g_pIndicatorSurface[1] = NULL;
	glDeleteTextures (1, &g_iIndicatorTexture);
	g_iIndicatorTexture = 0;
	if (cIndicatorImagePath != NULL)
	{
		double fLauncherWidth = (g_tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] != 0 ? g_tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER] : 48);
		double fLauncherHeight = (g_tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] != 0 ? g_tIconAuthorizedHeight[CAIRO_DOCK_LAUNCHER] : 48);
		
		double fScale = (g_bLinkIndicatorWithIcon ? 1 + g_fAmplitude : 1);
		g_pIndicatorSurface[CAIRO_DOCK_HORIZONTAL] = cairo_dock_create_surface_from_image (
			cIndicatorImagePath,
			pSourceContext,
			fScale,
			fLauncherWidth * fIndicatorRatio,
			fLauncherHeight * fIndicatorRatio,
			CAIRO_DOCK_KEEP_RATIO,
			&g_fIndicatorWidth,
			&g_fIndicatorHeight,
			NULL, NULL);
		//g_print ("g_pIndicatorSurface : %.2fx%.2f\n", g_fIndicatorWidth, g_fIndicatorHeight);
		if (g_pIndicatorSurface[CAIRO_DOCK_HORIZONTAL] != NULL)
			g_pIndicatorSurface[CAIRO_DOCK_VERTICAL] = cairo_dock_rotate_surface (
				g_pIndicatorSurface[CAIRO_DOCK_HORIZONTAL],
				pSourceContext,
				g_fIndicatorWidth * fScale,
				g_fIndicatorHeight * fScale,
				- G_PI / 2);
		else
			cd_warning ("couldn't load image '%s' for indicators", cIndicatorImagePath);
	}
	/*if (g_bUseOpenGL && g_pIndicatorSurface[CAIRO_DOCK_HORIZONTAL] != NULL)
	{
		GdkGLContext* pGlContext = gtk_widget_get_gl_context (g_pMainDock->pWidget);
		GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (g_pMainDock->pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return ;
		
		g_iIndicatorTexture = cairo_dock_create_texture_from_surface (g_pIndicatorSurface[CAIRO_DOCK_HORIZONTAL]);
		g_print ("g_iIndicatorTexture <- %d\n", g_iIndicatorTexture);
		
		gdk_gl_drawable_gl_end (pGlDrawable);
	}*/ // a mettre dans le realize.
}


void cairo_dock_load_desktop_background_surface (void)  // attention : fonction lourde.
{
	cairo_surface_destroy (g_pDesktopBgSurface);
	g_pDesktopBgSurface = NULL;
	
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
			
			/*g_pDesktopBgSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
				CAIRO_CONTENT_COLOR_ALPHA,
				g_iScreenWidth[CAIRO_DOCK_HORIZONTAL],
				g_iScreenHeight[CAIRO_DOCK_HORIZONTAL]);*/
			g_pDesktopBgSurface = _cairo_dock_create_blank_surface (pSourceContext,
				g_iScreenWidth[CAIRO_DOCK_HORIZONTAL],
				g_iScreenHeight[CAIRO_DOCK_HORIZONTAL]);
			
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
			
			if (fWidth < g_iScreenWidth[CAIRO_DOCK_HORIZONTAL] || fHeight < g_iScreenHeight[CAIRO_DOCK_HORIZONTAL])
			{
				cd_message ("c'est un degrade ou un motif (%dx%d)", (int) fWidth, (int) fHeight);
				/*g_pDesktopBgSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
					CAIRO_CONTENT_COLOR_ALPHA,
					g_iScreenWidth[CAIRO_DOCK_HORIZONTAL],
					g_iScreenHeight[CAIRO_DOCK_HORIZONTAL]);*/
				g_pDesktopBgSurface = _cairo_dock_create_blank_surface (pSourceContext,
					g_iScreenWidth[CAIRO_DOCK_HORIZONTAL],
					g_iScreenHeight[CAIRO_DOCK_HORIZONTAL]);
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
}

void cairo_dock_invalidate_desktop_bg_surface (void)
{
	if (g_pDesktopBgSurface != NULL)
	{
		cairo_surface_destroy (g_pDesktopBgSurface);
		g_pDesktopBgSurface = NULL;
	}
}

cairo_surface_t *cairo_dock_get_desktop_bg_surface (void)
{
	if (g_pDesktopBgSurface == NULL)
		cairo_dock_load_desktop_background_surface ();
	return g_pDesktopBgSurface;
}


void cairo_dock_load_active_window_indicator (cairo_t* pSourceContext, const gchar *cImagePath, double fMaxScale, double fCornerRadius, double fLineWidth, double *fActiveColor)
{
	if (g_pActiveIndicatorSurface != NULL)
		cairo_surface_destroy (g_pActiveIndicatorSurface);
	g_fActiveIndicatorWidth = MAX (g_tIconAuthorizedWidth[CAIRO_DOCK_LAUNCHER], g_tIconAuthorizedWidth[CAIRO_DOCK_APPLI]);
	g_fActiveIndicatorHeight = MAX (g_tIconAuthorizedHeight[CAIRO_DOCK_APPLI], g_tIconAuthorizedHeight[CAIRO_DOCK_APPLI]);
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
		/*g_pActiveIndicatorSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			g_fActiveIndicatorWidth * fMaxScale,
			g_fActiveIndicatorHeight * fMaxScale);*/
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
	else  // invisible.
	{
		g_pActiveIndicatorSurface = NULL;
		return ;
	}
}
