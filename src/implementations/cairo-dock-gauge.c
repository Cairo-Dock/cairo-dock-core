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

/** See the "data/gauges" folder for some exemples */

#include <string.h>
#include <math.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "gldi-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-opengl-font.h"
#include "cairo-dock-packages.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-gauge.h"


typedef struct {
	RsvgHandle *pSvgHandle;
	cairo_surface_t *pSurface;
	gint sizeX;
	gint sizeY;
	GLuint iTexture;
} GaugeImage;

// Effect applied on Indicator image.
typedef enum {
	CD_GAUGE_EFFECT_NONE=0,
	CD_GAUGE_EFFECT_CROP,
	CD_GAUGE_EFFECT_STRETCH,
	CD_GAUGE_EFFECT_ZOOM,
	CD_GAUGE_EFFECT_FADE,
	CD_GAUGE_NB_EFFECTS
	} GaugeIndicatorEffect; 

typedef struct {
	// needle
	gdouble posX, posY;
	gdouble posStart, posStop;
	gdouble direction;
	gint iNeedleRealWidth, iNeedleRealHeight;
	gdouble iNeedleOffsetX, iNeedleOffsetY;
	gdouble fNeedleScale;
	gint iNeedleWidth, iNeedleHeight;
	GaugeImage *pImageNeedle;
	// image list
	gint iNbImages;
	gint iNbImageLoaded;
	GaugeImage *pImageList;
	// value text zone
	CairoDataRendererTextParam textZone;
	// logo zone
	CairoDataRendererEmblemParam emblem;
	// label text zone
	CairoDataRendererTextParam labelZone;
	// indicator image effect
	GaugeIndicatorEffect iEffect;
} GaugeIndicator;

// Multi gauge position options.
typedef enum {
	CD_GAUGE_MULTI_DISPLAY_SCATTERED=0,
	CD_GAUGE_MULTI_DISPLAY_SHARED,
	CD_GAUGE_NB_MULTI_DISPLAY
	} GaugeMultiDisplay; 

typedef struct {
	CairoDataRenderer dataRenderer;
	gchar *cThemeName;
	GaugeImage *pImageBackground;
	GaugeImage *pImageForeground;
	GList *pIndicatorList;
	GaugeMultiDisplay iMultiDisplay;
} Gauge;


extern gboolean g_bUseOpenGL;

  ////////////////////////////////////////////
 /////////////// LOAD GAUGE /////////////////
////////////////////////////////////////////

static double _str2double (gchar *s)
{
	gchar *str = strchr (s, ',');
	if (str)
		*str = '.';
	return g_ascii_strtod (s, NULL);
}

static void _cairo_dock_init_gauge_image (const gchar *cImagePath, GaugeImage *pGaugeImage)
{
	// chargement du fichier.
	pGaugeImage->pSvgHandle = rsvg_handle_new_from_file (cImagePath, NULL);
	g_return_if_fail (pGaugeImage->pSvgHandle != NULL);
	
	//On récupère la taille de l'image.
	RsvgDimensionData SizeInfo;
	rsvg_handle_get_dimensions (pGaugeImage->pSvgHandle, &SizeInfo);
	pGaugeImage->sizeX = SizeInfo.width;
	pGaugeImage->sizeY = SizeInfo.height;
}
static GaugeImage *_cairo_dock_new_gauge_image (const gchar *cImagePath)
{
	GaugeImage *pGaugeImage = g_new0 (GaugeImage, 1);
	
	_cairo_dock_init_gauge_image (cImagePath, pGaugeImage);
	
	return pGaugeImage;
}
static void _load_image (GaugeImage *pGaugeImage, int iWidth, int iHeight)
{
	if (pGaugeImage->pSurface != NULL)
		cairo_surface_destroy (pGaugeImage->pSurface);
	if (pGaugeImage->iTexture != 0)
		_cairo_dock_delete_texture (pGaugeImage->iTexture);
	
	if (pGaugeImage->pSvgHandle != NULL)
	{
		pGaugeImage->pSurface = cairo_dock_create_blank_surface (
			iWidth,
			iHeight);
		
		cairo_t* pDrawingContext = cairo_create (pGaugeImage->pSurface);
		
		cairo_scale (pDrawingContext,
			(double) iWidth / (double) pGaugeImage->sizeX,
			(double) iHeight / (double) pGaugeImage->sizeY);
		
		rsvg_handle_render_cairo (pGaugeImage->pSvgHandle, pDrawingContext);
		cairo_destroy (pDrawingContext);
		
		if (g_bUseOpenGL)
		{
			pGaugeImage->iTexture = cairo_dock_create_texture_from_surface (pGaugeImage->pSurface);
		}
	}
	else
	{
		pGaugeImage->pSurface = NULL;
		pGaugeImage->iTexture = 0;
	}
}
static void _load_needle (GaugeIndicator *pGaugeIndicator, int iWidth, int iHeight)
{
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	g_return_if_fail (pGaugeImage != NULL);
	
	if (pGaugeImage->pSurface != NULL)
		cairo_surface_destroy (pGaugeImage->pSurface);
	if (pGaugeImage->iTexture != 0)
		_cairo_dock_delete_texture (pGaugeImage->iTexture);
	
	if (pGaugeImage->pSvgHandle != NULL)
	{
		int iSize = MIN (iWidth, iHeight);
		//g_print ("size : applet : %d, image : %d\n", iSize, pGaugeImage->sizeX);
		pGaugeIndicator->fNeedleScale = (double)iSize / (double) pGaugeImage->sizeX;  // car l'aiguille est a l'horizontale dans le fichier svg.
		pGaugeIndicator->iNeedleWidth = (double) pGaugeIndicator->iNeedleRealWidth * pGaugeIndicator->fNeedleScale;
		pGaugeIndicator->iNeedleHeight = (double) pGaugeIndicator->iNeedleRealHeight * pGaugeIndicator->fNeedleScale;
		//g_print (" + needle %dx%d\n", pGaugeIndicator->iNeedleWidth, pGaugeIndicator->iNeedleHeight);
		
		cairo_surface_t *pNeedleSurface = cairo_dock_create_blank_surface ( pGaugeIndicator->iNeedleWidth, pGaugeIndicator->iNeedleHeight);
		g_return_if_fail (cairo_surface_status (pNeedleSurface) == CAIRO_STATUS_SUCCESS);
		
		cairo_t* pDrawingContext = cairo_create (pNeedleSurface);
		g_return_if_fail (cairo_status (pDrawingContext) == CAIRO_STATUS_SUCCESS);
		
		cairo_scale (pDrawingContext, pGaugeIndicator->fNeedleScale, pGaugeIndicator->fNeedleScale);
		cairo_translate (pDrawingContext, pGaugeIndicator->iNeedleOffsetX, pGaugeIndicator->iNeedleOffsetY);
		rsvg_handle_render_cairo (pGaugeImage->pSvgHandle, pDrawingContext);
		cairo_destroy (pDrawingContext);
		
		pGaugeImage->iTexture = cairo_dock_create_texture_from_surface (pNeedleSurface);
		cairo_surface_destroy (pNeedleSurface);
	}
	else
	{
		pGaugeImage->pSurface = NULL;
		pGaugeImage->iTexture = 0;
	}
}
static gboolean _load_theme (Gauge *pGauge, const gchar *cThemePath)
{
	cd_message ("%s (%s)", __func__, cThemePath);
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	if (iWidth == 0 || iHeight == 0)
		return FALSE;
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	
	g_return_val_if_fail (cThemePath != NULL, FALSE);
	xmlInitParser ();
	xmlDocPtr pGaugeTheme;
	xmlNodePtr pGaugeMainNode;
	gchar *cXmlFile = g_strdup_printf ("%s/theme.xml", cThemePath);
	pGaugeTheme = cairo_dock_open_xml_file (cXmlFile, "gauge", &pGaugeMainNode, NULL);
	g_free (cXmlFile);
	g_return_val_if_fail (pGaugeTheme != NULL && pGaugeMainNode != NULL, FALSE);
	
	xmlAttrPtr ap;
	xmlChar *cAttribute, *cNodeContent, *cTextNodeContent;
	GString *sImagePath = g_string_new ("");
	GaugeImage *pGaugeImage;
	GaugeIndicator *pGaugeIndicator = NULL;
	xmlNodePtr pGaugeNode;
	double ratio_xy = 1.;
	double ratio_text = 2.;
	int i;
	for (pGaugeNode = pGaugeMainNode->children, i = 0; pGaugeNode != NULL; pGaugeNode = pGaugeNode->next, i ++)
	{
		if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "name") == 0)
		{
			pGauge->cThemeName = xmlNodeGetContent(pGaugeNode);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "rank") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			pRenderer->iRank = atoi (cNodeContent);
			xmlFree (cNodeContent);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "version") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			int iVersion = atoi (cNodeContent);
			if (iVersion == 2)
			{
				ratio_text = 1.;
				ratio_xy = 2.;
			}
			xmlFree (cNodeContent);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "file") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
			ap = xmlHasProp (pGaugeNode, "key");
			cAttribute = xmlNodeGetContent(ap->children);
			if (xmlStrcmp (cAttribute, "background") == 0)
			{
				pGauge->pImageBackground = _cairo_dock_new_gauge_image (sImagePath->str);
				_load_image (pGauge->pImageBackground, iWidth, iHeight);
			}
			else if (xmlStrcmp (cAttribute, "foreground") == 0)
			{
				pGauge->pImageForeground = _cairo_dock_new_gauge_image (sImagePath->str);
				_load_image (pGauge->pImageForeground, iWidth, iHeight);
			}
			xmlFree (cNodeContent);
			xmlFree (cAttribute);
		}
		else if(xmlStrcmp (pGaugeNode->name, (const xmlChar *) "multi_display") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			pGauge->iMultiDisplay = atoi (cNodeContent);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "indicator") == 0)
		{
			if (pRenderer->iRank == 0)
			{
				pRenderer->iRank = 1;
				xmlNodePtr node;
				for (node = pGaugeNode->next; node != NULL; node = node->next)
				{
					if (xmlStrcmp (node->name, (const xmlChar *) "indicator") == 0)
						pRenderer->iRank ++;
				}
			}
			
			pGaugeIndicator = g_new0 (GaugeIndicator, 1);
			pGaugeIndicator->direction = 1;
			
			cd_debug ("gauge : On charge un indicateur");
			xmlNodePtr pGaugeSubNode;
			for (pGaugeSubNode = pGaugeNode->children; pGaugeSubNode != NULL; pGaugeSubNode = pGaugeSubNode->next)
			{
				if (xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "text") == 0)
					continue;
				//g_print ("+ %s\n", pGaugeSubNode->name);
				cNodeContent = xmlNodeGetContent (pGaugeSubNode);
				if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posX") == 0)
					pGaugeIndicator->posX = _str2double (cNodeContent) * ratio_xy;
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posY") == 0)
					pGaugeIndicator->posY = _str2double (cNodeContent) * ratio_xy;
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "text_zone") == 0)
				{
					pGaugeIndicator->textZone.pColor[3] = 1.;
					xmlNodePtr pTextSubNode;
					for (pTextSubNode = pGaugeSubNode->children; pTextSubNode != NULL; pTextSubNode = pTextSubNode->next)
					{
						cTextNodeContent = xmlNodeGetContent (pTextSubNode);
						if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "x_center") == 0)
							pGaugeIndicator->textZone.fX = _str2double (cTextNodeContent)/ratio_text;
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "y_center") == 0)
							pGaugeIndicator->textZone.fY = _str2double (cTextNodeContent)/ratio_text;
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "width") == 0)
							pGaugeIndicator->textZone.fWidth = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "height") == 0)
							pGaugeIndicator->textZone.fHeight = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "red") == 0)
							pGaugeIndicator->textZone.pColor[0] = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "green") == 0)
							pGaugeIndicator->textZone.pColor[1] = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "blue") == 0)
							pGaugeIndicator->textZone.pColor[2] = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "alpha") == 0)
							pGaugeIndicator->textZone.pColor[3] = _str2double (cTextNodeContent);
					}
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "label_zone") == 0)
				{
					xmlNodePtr pTextSubNode;
					pGaugeIndicator->labelZone.pColor[3] = 1.;
					for (pTextSubNode = pGaugeSubNode->children; pTextSubNode != NULL; pTextSubNode = pTextSubNode->next)
					{
						cTextNodeContent = xmlNodeGetContent (pTextSubNode);
						if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "x_center") == 0)
							pGaugeIndicator->labelZone.fX = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "y_center") == 0)
							pGaugeIndicator->labelZone.fY = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "width") == 0)
							pGaugeIndicator->labelZone.fWidth = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "height") == 0)
							pGaugeIndicator->labelZone.fHeight = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "red") == 0)
							pGaugeIndicator->labelZone.pColor[0] = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "green") == 0)
							pGaugeIndicator->labelZone.pColor[1] = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "blue") == 0)
							pGaugeIndicator->labelZone.pColor[2] = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "alpha") == 0)
							pGaugeIndicator->labelZone.pColor[3] = _str2double (cTextNodeContent);
					}
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "logo_zone") == 0)
				{
					pGaugeIndicator->emblem.fAlpha = 1.;
					xmlNodePtr pLogoSubNode;
					for (pLogoSubNode = pGaugeSubNode->children; pLogoSubNode != NULL; pLogoSubNode = pLogoSubNode->next)
					{
						cTextNodeContent = xmlNodeGetContent (pLogoSubNode);
						if(xmlStrcmp (pLogoSubNode->name, (const xmlChar *) "x_center") == 0)
							pGaugeIndicator->emblem.fX = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pLogoSubNode->name, (const xmlChar *) "y_center") == 0)
							pGaugeIndicator->emblem.fY = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pLogoSubNode->name, (const xmlChar *) "width") == 0)
							pGaugeIndicator->emblem.fWidth = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pLogoSubNode->name, (const xmlChar *) "height") == 0)
							pGaugeIndicator->emblem.fHeight = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pLogoSubNode->name, (const xmlChar *) "alpha") == 0)
							pGaugeIndicator->emblem.fAlpha = _str2double (cTextNodeContent);
					}
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "direction") == 0)
					pGaugeIndicator->direction = _str2double (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStart") == 0)
					pGaugeIndicator->posStart = _str2double (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStop") == 0)
					pGaugeIndicator->posStop = _str2double (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "nb images") == 0)
					pGaugeIndicator->iNbImages = atoi (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "offset_x") == 0)
				{
					pGaugeIndicator->iNeedleOffsetX = atoi (cNodeContent);
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "width") == 0)
				{
					pGaugeIndicator->iNeedleRealWidth = atoi (cNodeContent);
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "height") == 0)
				{
					pGaugeIndicator->iNeedleRealHeight = atoi (cNodeContent);
					pGaugeIndicator->iNeedleOffsetY = .5 * pGaugeIndicator->iNeedleRealHeight;
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "effect") == 0)
					pGaugeIndicator->iEffect = atoi (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "file") == 0)
				{
					ap = xmlHasProp(pGaugeSubNode, "key");
					cAttribute = xmlNodeGetContent(ap->children);
					if (strcmp (cAttribute, "needle") == 0 && pGaugeIndicator->pImageNeedle == NULL)
					{
						g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
						pGaugeIndicator->pImageNeedle = _cairo_dock_new_gauge_image (sImagePath->str);
					}
					else if (strcmp (cAttribute,"image") == 0)
					{
						if (pGaugeIndicator->iNbImages == 0)
						{
							pGaugeIndicator->iNbImages = 1;
							xmlNodePtr node;
							for (node = pGaugeSubNode->next; node != NULL; node = node->next)
							{
								if (xmlStrcmp (node->name, (const xmlChar *) "file") == 0)
									pGaugeIndicator->iNbImages ++;
							}
						}
						if (pGaugeIndicator->pImageList == NULL)
							pGaugeIndicator->pImageList = g_new0 (GaugeImage, pGaugeIndicator->iNbImages);
						
						if (pGaugeIndicator->iNbImageLoaded < pGaugeIndicator->iNbImages)
						{
							g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
							_cairo_dock_init_gauge_image (sImagePath->str, &pGaugeIndicator->pImageList[pGaugeIndicator->iNbImageLoaded]);
							if (g_bUseOpenGL)  // il faut les charger maintenant, car on ne peut pas le faire durant le dessin sur le pbuffer (le chargement n'est pas effectif tout de suite et ca donne des textures blanches).
								_load_image (&pGaugeIndicator->pImageList[pGaugeIndicator->iNbImageLoaded], iWidth, iHeight);
							pGaugeIndicator->iNbImageLoaded ++;
						}
					}
					xmlFree (cAttribute);
				}
				xmlFree (cNodeContent);
			}
			if (pGaugeIndicator->pImageNeedle != NULL)  // on gere le cas d'une aiguille dont la taille est non fournie.
			{
				if (pGaugeIndicator->iNeedleRealHeight == 0)
				{
					pGaugeIndicator->iNeedleRealHeight = .12*pGaugeIndicator->pImageNeedle->sizeY;  // 12px utiles sur les 100
					pGaugeIndicator->iNeedleOffsetY = pGaugeIndicator->iNeedleRealHeight/2;
				}
				if (pGaugeIndicator->iNeedleRealWidth == 0)
				{
					pGaugeIndicator->iNeedleRealWidth = pGaugeIndicator->pImageNeedle->sizeY;  // 100px utiles sur les 100
					pGaugeIndicator->iNeedleOffsetX = 10;
				}
				if (g_bUseOpenGL)  // meme remarque.
					_load_needle (pGaugeIndicator, iWidth, iHeight);
			}
			pGauge->pIndicatorList = g_list_append (pGauge->pIndicatorList, pGaugeIndicator);
		}
	}
	cairo_dock_close_xml_file (pGaugeTheme);
	g_string_free (sImagePath, TRUE);
	
	g_return_val_if_fail (pRenderer->iRank != 0 && pGaugeIndicator != NULL, FALSE);
	
	return TRUE;
}
static void load (Gauge *pGauge, CairoContainer *pContainer, CairoGaugeAttribute *pAttribute)
{
	// on charge le theme defini en attribut.
	gboolean bThemeLoaded = _load_theme (pGauge, pAttribute->cThemePath);
	if (!bThemeLoaded)
		return;
	
	// on complete le data-renderer.
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	int iNbValues = cairo_data_renderer_get_nb_values (pRenderer);
	CairoDataRendererTextParam *pValuesText;
	CairoDataRendererEmblem *pEmblem;
	CairoDataRendererText *pLabel;
	GaugeIndicator *pGaugeIndicator;
	GList *il = pGauge->pIndicatorList;
	int i;
	for (i = 0; i < iNbValues; i ++)
	{
		pGaugeIndicator = il->data;
		il = il->next;
		if (il == NULL)
			il = pGauge->pIndicatorList;
		
		if (pRenderer->pValuesText)
		{
			pValuesText = &pRenderer->pValuesText[i];
			memcpy (pValuesText, &pGaugeIndicator->textZone, sizeof (CairoDataRendererTextParam));
		}
		if (pRenderer->pEmblems)
		{
			pEmblem = &pRenderer->pEmblems[i];
			memcpy (pEmblem, &pGaugeIndicator->emblem, sizeof (CairoDataRendererEmblemParam));
		}
		if (pRenderer->pLabels)
		{
			pLabel = &pRenderer->pLabels[i];
			memcpy (pLabel, &pGaugeIndicator->labelZone, sizeof (CairoDataRendererTextParam));
		}
	}
}

  ////////////////////////////////////////////
 ////////////// RENDER GAUGE ////////////////
////////////////////////////////////////////
static void _draw_gauge_needle (cairo_t *pCairoContext, Gauge *pGauge, GaugeIndicator *pGaugeIndicator, double fValue)
{
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	if(pGaugeImage != NULL)
	{
		double fAngle = (pGaugeIndicator->posStart + fValue * (pGaugeIndicator->posStop - pGaugeIndicator->posStart)) * G_PI / 180.;
		if (pGaugeIndicator->direction < 0)
			fAngle = - fAngle;
		
		double fHalfX = pGauge->pImageBackground->sizeX / 2.0f * (1 + pGaugeIndicator->posX);
		double fHalfY = pGauge->pImageBackground->sizeY / 2.0f * (1 - pGaugeIndicator->posY);
		
		cairo_save (pCairoContext);
		
		cairo_scale (pCairoContext,
			(double) CAIRO_DATA_RENDERER (pGauge)->iWidth / (double) pGaugeImage->sizeX,
			(double) CAIRO_DATA_RENDERER (pGauge)->iHeight / (double) pGaugeImage->sizeY);
		cairo_translate (pCairoContext, fHalfX, fHalfY);
		cairo_rotate (pCairoContext, -G_PI/2 + fAngle);
		
		rsvg_handle_render_cairo (pGaugeImage->pSvgHandle, pCairoContext);
		
		cairo_restore (pCairoContext);
	}
}
static void _draw_gauge_image (cairo_t *pCairoContext, Gauge *pGauge, GaugeIndicator *pGaugeIndicator, double fValue)
{
	int iNumImage = fValue * (pGaugeIndicator->iNbImages - 1) + 0.5;
	g_return_if_fail (iNumImage < pGaugeIndicator->iNbImages);
	
	GaugeImage *pGaugeImage = &pGaugeIndicator->pImageList[iNumImage];
	if (pGaugeImage->pSurface == NULL)
	{
		int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
		_load_image (pGaugeImage, iWidth, iHeight);
	}
	
	if (pGaugeImage->pSurface != NULL)
	{
		cairo_set_source_surface (pCairoContext, pGaugeImage->pSurface, 0.0f, 0.0f);
		cairo_paint (pCairoContext);
	}
}
static void cairo_dock_draw_one_gauge (cairo_t *pCairoContext, Gauge *pGauge, int iDataOffset)
{
	GaugeImage *pGaugeImage;
	//\________________ On affiche le fond.
	if(pGauge->pImageBackground != NULL)
	{
		pGaugeImage = pGauge->pImageBackground;
		cairo_set_source_surface (pCairoContext, pGaugeImage->pSurface, 0.0f, 0.0f);
		cairo_paint (pCairoContext);
	}
	
	//\________________ On represente l'indicateur de chaque valeur.
	GList *pIndicatorElement;
	GList *pValueList;
	double fValue;
	GaugeIndicator *pIndicator;
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int i;
	for (i = iDataOffset, pIndicatorElement = pGauge->pIndicatorList; i < pData->iNbValues && pIndicatorElement != NULL; i++, pIndicatorElement = pIndicatorElement->next)
	{
		pIndicator = pIndicatorElement->data;
		fValue = cairo_data_renderer_get_normalized_current_value (pRenderer, i);
		
		if (pIndicator->pImageNeedle != NULL)  // c'est une aiguille.
		{
			_draw_gauge_needle (pCairoContext, pGauge, pIndicator, fValue);
		}
		else  // c'est une image.
		{
			_draw_gauge_image (pCairoContext, pGauge, pIndicator, fValue);
		}
	}
	
	//\________________ On affiche l'avant-plan.
	if(pGauge->pImageForeground != NULL)
	{
		pGaugeImage = pGauge->pImageForeground;
		cairo_set_source_surface (pCairoContext, pGaugeImage->pSurface, 0.0f, 0.0f);
		cairo_paint (pCairoContext);
	}
	
	//\________________ On affiche les overlays.
	for (i = iDataOffset, pIndicatorElement = pGauge->pIndicatorList; i < pData->iNbValues && pIndicatorElement != NULL; i++, pIndicatorElement = pIndicatorElement->next)
	{
		cairo_dock_render_overlays_to_context (pRenderer, i, pCairoContext);
	}
}
void render (Gauge *pGauge, cairo_t *pCairoContext)
{
	g_return_if_fail (pGauge != NULL && pGauge->pIndicatorList != NULL);
	g_return_if_fail (pCairoContext != NULL && cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int iNbDrawings = (int) ceil (1. * pData->iNbValues / pRenderer->iRank);
	int i, iDataOffset = 0;
	for (i = 0; i < iNbDrawings; i ++)
	{
		if (iNbDrawings > 1)  // on va dessiner la jauges plusieurs fois, la 1ere en grand et les autres en petit autour.
		{
			cairo_save (pCairoContext);
			if (i == 0)
			{
				cairo_scale (pCairoContext, 2./3, 2./3);
			}
			else if (i == 1)
			{
				cairo_translate (pCairoContext, 2 * pRenderer->iWidth / 3, 2 * pRenderer->iHeight / 3);
				cairo_scale (pCairoContext, 1./3, 1./3);
			}
			else if (i == 2)
			{
				cairo_translate (pCairoContext, 2 * pRenderer->iWidth / 3, 0.);
				cairo_scale (pCairoContext, 1./3, 1./3);
			}
			else if (i == 3)
			{
				cairo_translate (pCairoContext, 0., 2 * pRenderer->iHeight / 3);
				cairo_scale (pCairoContext, 1./3, 1./3);
			}
			else  // 5 valeurs faut pas pousser non plus.
				break ;
		}
		
		cairo_dock_draw_one_gauge (pCairoContext, pGauge, iDataOffset);
		
		if (iNbDrawings > 1)
			cairo_restore (pCairoContext);
		
		iDataOffset += pRenderer->iRank;
	}
}

  ///////////////////////////////////////////////
 /////////////// RENDER OPENGL /////////////////
///////////////////////////////////////////////
static void _draw_gauge_image_opengl (Gauge *pGauge, GaugeIndicator *pGaugeIndicator, double fValue)
{
	int iNumImage = fValue * (pGaugeIndicator->iNbImages - 1) + 0.5;
	g_return_if_fail (iNumImage < pGaugeIndicator->iNbImages);
	
	GaugeImage *pGaugeImage = &pGaugeIndicator->pImageList[iNumImage];
	int iWidth, iHeight;
	cairo_data_renderer_get_size (CAIRO_DATA_RENDERER (pGauge), &iWidth, &iHeight);
	
	if (pGaugeImage->iTexture != 0)
	{
		glBindTexture (GL_TEXTURE_2D, pGaugeImage->iTexture);\
		switch (pGaugeIndicator->iEffect)
		{
			case CD_GAUGE_EFFECT_CROP :
				_cairo_dock_apply_current_texture_at_size_crop (pGaugeImage->iTexture, iWidth, iHeight, fValue);
			break;

			case CD_GAUGE_EFFECT_STRETCH :
				_cairo_dock_apply_current_texture_portion_at_size_with_offset (0. , 0., 1., 1., iWidth, iHeight * fValue, 0., iHeight * (fValue - 1)/2);
			break;

			case CD_GAUGE_EFFECT_ZOOM :
				_cairo_dock_apply_current_texture_portion_at_size_with_offset (0. , 0., 1., 1., iWidth * fValue, iHeight * fValue, 0., 0.);
			break;

			case CD_GAUGE_EFFECT_FADE :
				_cairo_dock_set_alpha(fValue); // no break, we need the default texture draw

			default :
				_cairo_dock_apply_current_texture_at_size (iWidth, iHeight);
			break;
		}
	}
}
static void _draw_gauge_needle_opengl (Gauge *pGauge, GaugeIndicator *pGaugeIndicator, double fValue)
{
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	g_return_if_fail (pGaugeImage != NULL);
	
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	if(pGaugeImage->iTexture != 0)
	{
		double fAngle = (pGaugeIndicator->posStart + fValue * (pGaugeIndicator->posStop - pGaugeIndicator->posStart));
		if (pGaugeIndicator->direction < 0)
			fAngle = - fAngle;
		double fHalfX = iWidth / 2.0f * (0 + pGaugeIndicator->posX);
		double fHalfY = iHeight / 2.0f * (0 + pGaugeIndicator->posY);
		
		glPushMatrix ();
		
		glTranslatef (fHalfX, fHalfY, 0.);
		glRotatef (90. - fAngle, 0., 0., 1.);
		glTranslatef (pGaugeIndicator->iNeedleWidth/2 - pGaugeIndicator->fNeedleScale * pGaugeIndicator->iNeedleOffsetX, 0., 0.);
		_cairo_dock_apply_texture_at_size (pGaugeImage->iTexture, pGaugeIndicator->iNeedleWidth, pGaugeIndicator->iNeedleHeight);
		
		glPopMatrix ();
	}
}
static void cairo_dock_draw_one_gauge_opengl (Gauge *pGauge, int iDataOffset)
{
	_cairo_dock_enable_texture ();  // on le fait ici car l'affichage des overlays le change.
	_cairo_dock_set_blend_pbuffer ();  // ceci reste un mystere...
	_cairo_dock_set_alpha (1.);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int iWidth, iHeight;
	cairo_data_renderer_get_size (pRenderer, &iWidth, &iHeight);
	GaugeImage *pGaugeImage;
	
	//\________________ On affiche le fond.
	if(pGauge->pImageBackground != NULL)
	{
		pGaugeImage = pGauge->pImageBackground;
		if (pGaugeImage->iTexture != 0)
			_cairo_dock_apply_texture_at_size (pGaugeImage->iTexture, iWidth, iHeight);
	}
	
	//\________________ On represente l'indicateur de chaque valeur.
	GList *pIndicatorElement;
	GList *pValueList;
	double fValue;
	GaugeIndicator *pIndicator;
	int i;
	for (i = iDataOffset, pIndicatorElement = pGauge->pIndicatorList; i < pData->iNbValues && pIndicatorElement != NULL; i++, pIndicatorElement = pIndicatorElement->next)
	{
		pIndicator = pIndicatorElement->data;
		fValue = cairo_data_renderer_get_normalized_current_value_with_latency (pRenderer, i);
		
		if (pIndicator->pImageNeedle != NULL)  // c'est une aiguille.
		{
			_draw_gauge_needle_opengl (pGauge, pIndicator, fValue);
		}
		else  // c'est une image.
		{
			_draw_gauge_image_opengl (pGauge, pIndicator, fValue);
		}
	}
	
	//\________________ On affiche l'avant-plan.
	if(pGauge->pImageForeground != NULL)
	{
		pGaugeImage = pGauge->pImageForeground;
		if (pGaugeImage->iTexture != 0)
			_cairo_dock_apply_texture_at_size (pGaugeImage->iTexture, iWidth, iHeight);
	}
	
	//\________________ On affiche les overlays.
	for (i = iDataOffset, pIndicatorElement = pGauge->pIndicatorList; i < pData->iNbValues && pIndicatorElement != NULL; i++, pIndicatorElement = pIndicatorElement->next)
	{
		cairo_dock_render_overlays_to_texture (pRenderer, i);
	}
}
static void render_opengl (Gauge *pGauge)
{
	g_return_if_fail (pGauge != NULL && pGauge->pIndicatorList != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int iNbDrawings = (int) ceil (1. * pData->iNbValues / pRenderer->iRank);
	int i, iDataOffset = 0;
	float ratio = (float) 1 / iNbDrawings;
	int iWidth, iHeight;
	cairo_data_renderer_get_size (pRenderer, &iWidth, &iHeight);
	gboolean bDisplay = TRUE;
	for (i = 0; i < iNbDrawings; i ++)
	{
		if (iNbDrawings > 1)  // on va dessiner la jauges plusieurs fois, la 1ere en grand et les autres en petit autour.
		{
			glPushMatrix ();
			switch (pGauge->iMultiDisplay)
			{
				case CD_GAUGE_MULTI_DISPLAY_SHARED :
					/** box positions : 
					* change axis to left border : -w / 2
					* move to box #i : w * i / n
					* move 1/2 box left : -w / 2n 
					* =w(-0.5 +i/n -1/2n)
					*/
					glTranslatef (iWidth * (i * ratio - 0.5 + ratio / 2), 0., 0.);
					glScalef (ratio, 1., 1.);
					break;
	
				case CD_GAUGE_MULTI_DISPLAY_SCATTERED :
					if (i == 0)  /// tester avec 1/2, 1/2
					{
						glTranslatef (-iWidth / 6, iHeight / 6, 0.);
						glScalef (2./3, 2./3, 1.);
					}
					else if (i == 1)
					{
						glTranslatef (iWidth / 3, - iHeight / 3, 0.);
						glScalef (1./3, 1./3, 1.);
					}
					else if (i == 2)
					{
						glTranslatef (iWidth / 3, iHeight / 3, 0.);
						glScalef (1./3, 1./3, 1.);
					}
					else if (i == 3)
					{
						glTranslatef (-iWidth / 3, -iHeight / 3, 0.);
						glScalef (1./3, 1./3, 1.);
					}
					else  // 5 valeurs faut pas pousser non plus.
						bDisplay = FALSE;
			}
		}
		
		if (bDisplay)
			cairo_dock_draw_one_gauge_opengl (pGauge, iDataOffset);
		
		if (iNbDrawings > 1)
			glPopMatrix ();
		
		iDataOffset += pRenderer->iRank;
	}
	_cairo_dock_disable_texture ();
}


  //////////////////////////////////////////////
 /////////////// RELOAD GAUGE /////////////////
//////////////////////////////////////////////
static void reload (Gauge *pGauge)
{
	//g_print ("%s (%dx%d)\n", __func__, iWidth, iHeight);
	g_return_if_fail (pGauge != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	if (pGauge->pImageBackground != NULL)
	{
		_load_image (pGauge->pImageBackground, iWidth, iHeight);
	}
	
	if (pGauge->pImageForeground != NULL)
	{
		_load_image (pGauge->pImageForeground, iWidth, iHeight);
	}
	
	GaugeIndicator *pGaugeIndicator;
	GaugeImage *pGaugeImage;
	int i, j;
	GList *pElement, *pElement2;
	for (pElement = pGauge->pIndicatorList; pElement != NULL; pElement = pElement->next)
	{
		pGaugeIndicator = pElement->data;
		for (i = 0; i < pGaugeIndicator->iNbImages; i ++)
		{
			pGaugeImage = &pGaugeIndicator->pImageList[i];
			_load_image (pGaugeImage, iWidth, iHeight);
		}
		if (g_bUseOpenGL && pGaugeIndicator->pImageNeedle)
		{
			_load_needle (pGaugeIndicator, iWidth, iHeight);
		}
	}
}

  ////////////////////////////////////////////
 /////////////// FREE GAUGE /////////////////
////////////////////////////////////////////
static void _cairo_dock_free_gauge_image(GaugeImage *pGaugeImage, gboolean bFree)
{
	if (pGaugeImage == NULL)
		return ;
	cd_debug("");

	if(pGaugeImage->pSvgHandle != NULL)
		rsvg_handle_free (pGaugeImage->pSvgHandle);
	if(pGaugeImage->pSurface != NULL)
		cairo_surface_destroy (pGaugeImage->pSurface);
	if (pGaugeImage->iTexture != 0)
		_cairo_dock_delete_texture (pGaugeImage->iTexture);
	
	if (bFree)
		g_free (pGaugeImage);
}
static void _cairo_dock_free_gauge_indicator(GaugeIndicator *pGaugeIndicator)
{
	if (pGaugeIndicator == NULL)
		return ;
	cd_debug("");
	
	int i;
	for (i = 0; i < pGaugeIndicator->iNbImages; i ++)
	{
		_cairo_dock_free_gauge_image (&pGaugeIndicator->pImageList[i], FALSE);
	}
	g_free (pGaugeIndicator->pImageList);
	
	_cairo_dock_free_gauge_image (pGaugeIndicator->pImageNeedle, TRUE);
	
	g_free (pGaugeIndicator);
}
static void unload (Gauge *pGauge)
{
	cd_debug("");
	_cairo_dock_free_gauge_image(pGauge->pImageBackground, TRUE);
	_cairo_dock_free_gauge_image(pGauge->pImageForeground, TRUE);
	
	GList *pElement;
	for (pElement = pGauge->pIndicatorList; pElement != NULL; pElement = pElement->next)
	{
		_cairo_dock_free_gauge_indicator (pElement->data);
	}
	g_list_free (pGauge->pIndicatorList);
}


  //////////////////////////////////////////
 /////////////// RENDERER /////////////////
//////////////////////////////////////////
void cairo_dock_register_data_renderer_gauge (void)
{
	CairoDockDataRendererRecord *pRecord = g_new0 (CairoDockDataRendererRecord, 1);
	pRecord->interface.load			= (CairoDataRendererLoadFunc) load;
	pRecord->interface.render		= (CairoDataRendererRenderFunc) render;
	pRecord->interface.render_opengl	= (CairoDataRendererRenderOpenGLFunc) render_opengl;
	pRecord->interface.reload		= (CairoDataRendererReloadFunc) reload;
	pRecord->interface.unload		= (CairoDataRendererUnloadFunc) unload;
	pRecord->iStructSize			= sizeof (Gauge);
	pRecord->cThemeDirName 			= "gauges";
	pRecord->cDistantThemeDirName 	= "gauges2";
	pRecord->cDefaultTheme 			= "Turbo-night-fuel";
	
	cairo_dock_register_data_renderer ("gauge", pRecord);
}
