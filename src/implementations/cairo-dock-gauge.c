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
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-gauge.h"


typedef struct {
	CairoDockImageBuffer image;
	gchar *cImagePath;
} GaugeImage;

typedef enum {
	CD_GAUGE_TYPE_UNKNOWN=0,
	CD_GAUGE_TYPE_NEEDLE,
	CD_GAUGE_TYPE_IMAGES,
	CD_NB_GAUGE_TYPES
} GaugeType;

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
	// images list
	GaugeIndicatorEffect iEffect;
	gint iNbImages;
	gint iNbImageLoaded;
	GaugeImage *pImageList;
	GaugeImage *pImageUndef;
	// value text zone
	CairoDataRendererTextParam textZone;
	// logo zone
	CairoDataRendererEmblemParam emblem;
	// label text zone
	CairoDataRendererTextParam labelZone;
} GaugeIndicator;

// Multi gauge position options.
typedef enum {
	CD_GAUGE_MULTI_DISPLAY_SCATTERED=0,
	CD_GAUGE_MULTI_DISPLAY_SHARED,
	CD_GAUGE_NB_MULTI_DISPLAY
	} GaugeMultiDisplay;

typedef struct {
	CairoDataRenderer dataRenderer;
	GaugeImage *pImageBackground;
	GaugeImage *pImageForeground;
	GList *pIndicatorList;
	GaugeMultiDisplay iMultiDisplay;
} Gauge;


extern gboolean g_bUseOpenGL;

  ////////////////////////////////////////////
 /////////////// LOAD GAUGE /////////////////
////////////////////////////////////////////

static double _str2double (xmlChar *s)
{
	gchar *str = strchr ((char *) s, ',');
	if (str)
		*str = '.';
	return g_ascii_strtod ((char *) s, NULL);
}

static void _load_gauge_image (GaugeImage *pGaugeImage, const gchar *cThemePath, const xmlChar *cImageName, int iWidth, int iHeight)
{
	pGaugeImage->cImagePath = g_strdup_printf ("%s/%s", cThemePath, (gchar *) cImageName);
	cairo_dock_load_image_buffer (&pGaugeImage->image, pGaugeImage->cImagePath, iWidth, iHeight, 0);
}

static GaugeImage *_new_gauge_image (const gchar *cThemePath, const xmlChar *cImageName, int iWidth, int iHeight)
{
	GaugeImage *pGaugeImage = g_new0 (GaugeImage, 1);
	_load_gauge_image (pGaugeImage, cThemePath, cImageName, iWidth, iHeight);
	return pGaugeImage;
}

static void _reload_gauge_image (GaugeImage *pGaugeImage, int iWidth, int iHeight)
{
	cairo_dock_unload_image_buffer (&pGaugeImage->image);
	
	if (pGaugeImage->cImagePath)
	{
		cairo_dock_load_image_buffer (&pGaugeImage->image, pGaugeImage->cImagePath, iWidth, iHeight, 0);
	}
}

static void __load_needle (GaugeIndicator *pGaugeIndicator, int iWidth, int iHeight)
{
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	
	// load the SVG file.
	RsvgHandle *pSvgHandle = rsvg_handle_new_from_file (pGaugeImage->cImagePath, NULL);
	g_return_if_fail (pSvgHandle != NULL);
	
	// get the SVG dimensions.
	RsvgDimensionData SizeInfo;
	rsvg_handle_get_dimensions (pSvgHandle, &SizeInfo);
	int sizeX = SizeInfo.width;
	int sizeY = SizeInfo.height;
	
	// guess the needle size and offset if not specified.
	if (pGaugeIndicator->iNeedleRealHeight == 0)
	{
		pGaugeIndicator->iNeedleRealHeight = .12*sizeY;  // 12px utiles sur les 100
		pGaugeIndicator->iNeedleOffsetY = pGaugeIndicator->iNeedleRealHeight/2;
	}
	if (pGaugeIndicator->iNeedleRealWidth == 0)
	{
		pGaugeIndicator->iNeedleRealWidth = sizeX;  // 100px utiles sur les 100
		pGaugeIndicator->iNeedleOffsetX = 10;
	}
	
	int iSize = MIN (iWidth, iHeight);
	pGaugeIndicator->fNeedleScale = (double)iSize / (double) sizeX;  // car l'aiguille est a l'horizontale dans le fichier svg.
	pGaugeIndicator->iNeedleWidth = (double) pGaugeIndicator->iNeedleRealWidth * pGaugeIndicator->fNeedleScale;
	pGaugeIndicator->iNeedleHeight = (double) pGaugeIndicator->iNeedleRealHeight * pGaugeIndicator->fNeedleScale;
	
	// make a cairo surface.
	cairo_surface_t *pNeedleSurface = cairo_dock_create_blank_surface (pGaugeIndicator->iNeedleWidth, pGaugeIndicator->iNeedleHeight);
	g_return_if_fail (cairo_surface_status (pNeedleSurface) == CAIRO_STATUS_SUCCESS);
	
	cairo_t* pDrawingContext = cairo_create (pNeedleSurface);
	g_return_if_fail (cairo_status (pDrawingContext) == CAIRO_STATUS_SUCCESS);
	
	cairo_scale (pDrawingContext, pGaugeIndicator->fNeedleScale, pGaugeIndicator->fNeedleScale);
	cairo_translate (pDrawingContext, pGaugeIndicator->iNeedleOffsetX, pGaugeIndicator->iNeedleOffsetY);
	rsvg_handle_render_cairo (pSvgHandle, pDrawingContext);
	
	cairo_destroy (pDrawingContext);
	g_object_unref (pSvgHandle);
	
	// load it into an image buffer.
	cairo_dock_load_image_buffer_from_surface (&pGaugeImage->image, pNeedleSurface, iWidth, iHeight);
}

static void _reload_gauge_needle (GaugeIndicator *pGaugeIndicator, int iWidth, int iHeight)
{
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	
	if (pGaugeImage != NULL)
	{
		cairo_dock_unload_image_buffer (&pGaugeImage->image);
		if (pGaugeImage->cImagePath)
		{
			__load_needle (pGaugeIndicator, iWidth, iHeight);
		}
	}
}

static void _load_gauge_needle (GaugeIndicator *pGaugeIndicator, const gchar *cThemePath, const gchar *cImageName, int iWidth, int iHeight)
{
	if (!cImageName)
		return;
	
	GaugeImage *pGaugeImage = g_new0 (GaugeImage, 1);
	pGaugeImage->cImagePath = g_strdup_printf ("%s/%s", cThemePath, cImageName);
	pGaugeIndicator->pImageNeedle = pGaugeImage;
	
	__load_needle (pGaugeIndicator, iWidth, iHeight);
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
	
	xmlChar *cAttribute, *cNodeContent, *cTextNodeContent, *cTypeAttr;
	GString *sImagePath = g_string_new ("");
	gchar *cNeedleImage;
	GaugeType iType;  // type of the current indicator.
	gboolean next;
	GaugeIndicator *pGaugeIndicator = NULL;
	xmlNodePtr pGaugeNode;
	double ratio_xy = 1.;
	double ratio_text = 2.;
	int i;
	for (pGaugeNode = pGaugeMainNode->children, i = 0; pGaugeNode != NULL; pGaugeNode = pGaugeNode->next, i ++)
	{
		if (xmlStrcmp (pGaugeNode->name, BAD_CAST "rank") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			pRenderer->iRank = atoi ((char *) cNodeContent);
			xmlFree (cNodeContent);
		}
		else if (xmlStrcmp (pGaugeNode->name, BAD_CAST "version") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			int iVersion = atoi ((char *) cNodeContent);
			if (iVersion >= 2)
			{
				ratio_text = 1.;
				ratio_xy = 2.;
			}
			xmlFree (cNodeContent);
		}
		else if (xmlStrcmp (pGaugeNode->name, BAD_CAST "file") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			cAttribute = xmlGetProp (pGaugeNode, BAD_CAST "type");
			if (!cAttribute)
				cAttribute = xmlGetProp (pGaugeNode, BAD_CAST "key");
			if (cAttribute != NULL)
			{
				if (xmlStrcmp (cAttribute, BAD_CAST "background") == 0)
				{
					pGauge->pImageBackground = _new_gauge_image (cThemePath, cNodeContent, iWidth, iHeight);
				}
				else if (xmlStrcmp (cAttribute, BAD_CAST "foreground") == 0)
				{
					pGauge->pImageForeground = _new_gauge_image (cThemePath, cNodeContent, iWidth, iHeight);
				}
				xmlFree (cAttribute);
			}
			xmlFree (cNodeContent);
		}
		else if(xmlStrcmp (pGaugeNode->name, BAD_CAST "multi_display") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			pGauge->iMultiDisplay = atoi ((char *) cNodeContent);
		}
		else if (xmlStrcmp (pGaugeNode->name, BAD_CAST "indicator") == 0)
		{
			// count the number of indicators.
			if (pRenderer->iRank == 0)  // first indicator.
			{
				pRenderer->iRank = 1;
				xmlNodePtr node;
				for (node = pGaugeNode->next; node != NULL; node = node->next)
				{
					if (xmlStrcmp (node->name, BAD_CAST "indicator") == 0)
						pRenderer->iRank ++;
				}
			}
			
			// get the type if the indicator.
			iType = CD_GAUGE_TYPE_UNKNOWN;  // until we know the indicator type, it can be any one.
			cAttribute = xmlGetProp (pGaugeNode, BAD_CAST "type");
			if (cAttribute != NULL)
			{
				if (strcmp ((char *) cAttribute, "needle") == 0)
				{
					iType = CD_GAUGE_TYPE_NEEDLE;
				}
				else if (strcmp ((char *) cAttribute, "images") == 0)
				{
					iType = CD_GAUGE_TYPE_IMAGES;
				}
				else  // wrong attribute, skip this indicator.
				{
					pRenderer->iRank --;
					continue;
				}
				xmlFree (cAttribute);
			}
			
			// load the indicators.
			pGaugeIndicator = g_new0 (GaugeIndicator, 1);
			pGaugeIndicator->direction = 1;
			
			cd_debug ("gauge : On charge un indicateur");
			cNeedleImage = NULL;
			xmlNodePtr pIndicatorNode;
			for (pIndicatorNode = pGaugeNode->children; pIndicatorNode != NULL; pIndicatorNode = pIndicatorNode->next)
			{
				if (xmlStrcmp (pIndicatorNode->name, BAD_CAST "text") == 0)  // xml noise.
					continue;
				cNodeContent = xmlNodeGetContent (pIndicatorNode);
				
				next = FALSE;
				if (iType != CD_GAUGE_TYPE_IMAGES)  // needle or unknown
				{
					if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "posX") == 0)
						pGaugeIndicator->posX = _str2double (cNodeContent) * ratio_xy;
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "posY") == 0)
						pGaugeIndicator->posY = _str2double (cNodeContent) * ratio_xy;
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "direction") == 0)
						pGaugeIndicator->direction = _str2double (cNodeContent);
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "posStart") == 0)
						pGaugeIndicator->posStart = _str2double (cNodeContent);
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "posStop") == 0)
						pGaugeIndicator->posStop = _str2double (cNodeContent);
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "nb images") == 0)
						pGaugeIndicator->iNbImages = atoi ((char *) cNodeContent);
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "offset_x") == 0)
					{
						pGaugeIndicator->iNeedleOffsetX = atoi ((char *) cNodeContent);
					}
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "width") == 0)
					{
						pGaugeIndicator->iNeedleRealWidth = atoi ((char *) cNodeContent);
					}
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "height") == 0)
					{
						pGaugeIndicator->iNeedleRealHeight = atoi ((char *) cNodeContent);
						pGaugeIndicator->iNeedleOffsetY = .5 * pGaugeIndicator->iNeedleRealHeight;
					}
					else
						next = TRUE;
				}
				if (iType != CD_GAUGE_TYPE_NEEDLE)  // image or unknown
				{
					if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "effect") == 0)
					{
						pGaugeIndicator->iEffect = atoi ((char *) cNodeContent);
					}
					else
						next = TRUE;
				}
				if (next)  // other parameters
				{
					if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "file") == 0)
					{
						// if the type is still unknown, try to get it here (version <= 2)
						if (iType == CD_GAUGE_TYPE_UNKNOWN)
						{
							cAttribute = xmlGetProp (pIndicatorNode, BAD_CAST "key");
							if (cAttribute && strcmp ((char *) cAttribute, "needle") == 0)
								iType = CD_GAUGE_TYPE_NEEDLE;
							else
								iType = CD_GAUGE_TYPE_IMAGES;
							xmlFree (cAttribute);
						}
						// get/load the image(s).
						if (iType == CD_GAUGE_TYPE_NEEDLE)
						{
							cNeedleImage = (gchar *) cNodeContent;  // just remember the image name, we'll load it in the end.
							cNodeContent = NULL;
						}
						else  // load the images.
						{
							cAttribute = xmlGetProp (pIndicatorNode, BAD_CAST "type");
							if (cAttribute && strcmp ((char *) cAttribute, "undef-value") == 0)
							{
								pGaugeIndicator->pImageUndef = _new_gauge_image (cThemePath, cNodeContent, iWidth, iHeight);
							}
							else
							{
								// count the number of images
								if (pGaugeIndicator->iNbImages == 0)
								{
									pGaugeIndicator->iNbImages = 1;
									xmlNodePtr node;
									for (node = pIndicatorNode->next; node != NULL; node = node->next)
									{
										if (xmlStrcmp (node->name, BAD_CAST "file") == 0)
										{
											cTypeAttr = xmlGetProp (node, BAD_CAST "type");
											if (!cTypeAttr || strcmp ((char *) cTypeAttr, "undef-value") != 0)
												pGaugeIndicator->iNbImages ++;
											xmlFree (cTypeAttr);
										}
									}
								}
								if (pGaugeIndicator->pImageList == NULL)
									pGaugeIndicator->pImageList = g_new0 (GaugeImage, pGaugeIndicator->iNbImages);
								
								// load the image.
								if (pGaugeIndicator->iNbImageLoaded < pGaugeIndicator->iNbImages)
								{
									_load_gauge_image (&pGaugeIndicator->pImageList[pGaugeIndicator->iNbImageLoaded],
										cThemePath, cNodeContent,
										iWidth, iHeight);
									pGaugeIndicator->iNbImageLoaded ++;
								}
							}
							xmlFree (cAttribute);
						}
					}
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "text_zone") == 0)
					{
						pGaugeIndicator->textZone.pColor[3] = 1.;
						xmlNodePtr pTextSubNode;
						for (pTextSubNode = pIndicatorNode->children; pTextSubNode != NULL; pTextSubNode = pTextSubNode->next)
						{
							cTextNodeContent = xmlNodeGetContent (pTextSubNode);
							if(xmlStrcmp (pTextSubNode->name, BAD_CAST "x_center") == 0)
								pGaugeIndicator->textZone.fX = _str2double (cTextNodeContent)/ratio_text;
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "y_center") == 0)
								pGaugeIndicator->textZone.fY = _str2double (cTextNodeContent)/ratio_text;
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "width") == 0)
								pGaugeIndicator->textZone.fWidth = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "height") == 0)
								pGaugeIndicator->textZone.fHeight = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "red") == 0)
								pGaugeIndicator->textZone.pColor[0] = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "green") == 0)
								pGaugeIndicator->textZone.pColor[1] = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "blue") == 0)
								pGaugeIndicator->textZone.pColor[2] = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "alpha") == 0)
								pGaugeIndicator->textZone.pColor[3] = _str2double (cTextNodeContent);
						}
					}
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "label_zone") == 0)
					{
						xmlNodePtr pTextSubNode;
						pGaugeIndicator->labelZone.pColor[3] = 1.;
						for (pTextSubNode = pIndicatorNode->children; pTextSubNode != NULL; pTextSubNode = pTextSubNode->next)
						{
							cTextNodeContent = xmlNodeGetContent (pTextSubNode);
							if(xmlStrcmp (pTextSubNode->name, BAD_CAST "x_center") == 0)
								pGaugeIndicator->labelZone.fX = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "y_center") == 0)
								pGaugeIndicator->labelZone.fY = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "width") == 0)
								pGaugeIndicator->labelZone.fWidth = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "height") == 0)
								pGaugeIndicator->labelZone.fHeight = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "red") == 0)
								pGaugeIndicator->labelZone.pColor[0] = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "green") == 0)
								pGaugeIndicator->labelZone.pColor[1] = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "blue") == 0)
								pGaugeIndicator->labelZone.pColor[2] = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pTextSubNode->name, BAD_CAST "alpha") == 0)
								pGaugeIndicator->labelZone.pColor[3] = _str2double (cTextNodeContent);
						}
					}
					else if(xmlStrcmp (pIndicatorNode->name, BAD_CAST "logo_zone") == 0)
					{
						pGaugeIndicator->emblem.fAlpha = 1.;
						xmlNodePtr pLogoSubNode;
						for (pLogoSubNode = pIndicatorNode->children; pLogoSubNode != NULL; pLogoSubNode = pLogoSubNode->next)
						{
							cTextNodeContent = xmlNodeGetContent (pLogoSubNode);
							if(xmlStrcmp (pLogoSubNode->name, BAD_CAST "x_center") == 0)
								pGaugeIndicator->emblem.fX = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pLogoSubNode->name, BAD_CAST "y_center") == 0)
								pGaugeIndicator->emblem.fY = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pLogoSubNode->name, BAD_CAST "width") == 0)
								pGaugeIndicator->emblem.fWidth = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pLogoSubNode->name, BAD_CAST "height") == 0)
								pGaugeIndicator->emblem.fHeight = _str2double (cTextNodeContent);
							else if(xmlStrcmp (pLogoSubNode->name, BAD_CAST "alpha") == 0)
								pGaugeIndicator->emblem.fAlpha = _str2double (cTextNodeContent);
						}
					}
				}
				xmlFree (cNodeContent);
			}
			
			// in the case of a needle, load it now, since we need to know the dimensions beforehand.
			if (cNeedleImage != NULL)
			{
				_load_gauge_needle (pGaugeIndicator, cThemePath, cNeedleImage, iWidth, iHeight);
				xmlFree (cNeedleImage);
			}
			pGauge->pIndicatorList = g_list_append (pGauge->pIndicatorList, pGaugeIndicator);
		}
	}
	cairo_dock_close_xml_file (pGaugeTheme);
	g_string_free (sImagePath, TRUE);
	
	g_return_val_if_fail (pRenderer->iRank != 0 && pGaugeIndicator != NULL, FALSE);
	
	return TRUE;
}
static void load (Gauge *pGauge, G_GNUC_UNUSED Icon *pIcon, CairoGaugeAttribute *pAttribute)
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
	for (il = pGauge->pIndicatorList, i = 0; il != NULL && i < iNbValues; il = il->next, i ++)
	{
		pGaugeIndicator = il->data;
		
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
	if (fValue <= CAIRO_DATA_RENDERER_UNDEF_VALUE+1)
		return;
	
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	if (pGaugeImage != NULL)
	{
		double fAngle = (pGaugeIndicator->posStart + fValue * (pGaugeIndicator->posStop - pGaugeIndicator->posStart)) * G_PI / 180.;
		if (pGaugeIndicator->direction < 0)
			fAngle = - fAngle;
		
		double fHalfX = CAIRO_DATA_RENDERER (pGauge)->iWidth / 2.0f * (1 + pGaugeIndicator->posX);
		double fHalfY = CAIRO_DATA_RENDERER (pGauge)->iHeight / 2.0f * (1 - pGaugeIndicator->posY);
		
		cairo_save (pCairoContext);
		
		cairo_translate (pCairoContext, fHalfX, fHalfY);
		cairo_rotate (pCairoContext, -G_PI/2 + fAngle);
		
		cairo_set_source_surface (pCairoContext, pGaugeImage->image.pSurface, -pGaugeIndicator->iNeedleOffsetX, -pGaugeIndicator->iNeedleOffsetY);
		cairo_paint (pCairoContext);
		
		
		cairo_restore (pCairoContext);
	}
}
static GaugeImage *_get_nth_image (Gauge *pGauge, GaugeIndicator *pGaugeIndicator, double fValue)
{
	GaugeImage *pGaugeImage;
	if (fValue <= CAIRO_DATA_RENDERER_UNDEF_VALUE+1)
	{
		pGaugeImage = pGaugeIndicator->pImageUndef;
		if (pGaugeIndicator->pImageUndef == NULL && pGauge->pImageBackground == NULL)  // the theme doesn't define an "undef" image, and there is no bg image => to avoid having an empty icon, we draw the 0-th image.
			pGaugeImage = &pGaugeIndicator->pImageList[0];
	}
	else
	{
		int iNumImage = fValue * (pGaugeIndicator->iNbImages - 1) + 0.5;
		if (iNumImage < 0)
			iNumImage = 0;
		if (iNumImage > pGaugeIndicator->iNbImages - 1)
			iNumImage = pGaugeIndicator->iNbImages - 1;
		pGaugeImage = &pGaugeIndicator->pImageList[iNumImage];
	}
	return pGaugeImage;
}
static void _draw_gauge_image (cairo_t *pCairoContext, Gauge *pGauge, GaugeIndicator *pGaugeIndicator, double fValue)
{
	GaugeImage *pGaugeImage = _get_nth_image (pGauge, pGaugeIndicator, fValue);
	
	if (pGaugeImage && pGaugeImage->image.pSurface != NULL)
	{
		cairo_set_source_surface (pCairoContext, pGaugeImage->image.pSurface, 0.0f, 0.0f);
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
		cairo_set_source_surface (pCairoContext, pGaugeImage->image.pSurface, 0.0f, 0.0f);
		cairo_paint (pCairoContext);
	}
	
	//\________________ On represente l'indicateur de chaque valeur.
	GList *pIndicatorElement;
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
		cairo_set_source_surface (pCairoContext, pGaugeImage->image.pSurface, 0.0f, 0.0f);
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
	GaugeImage *pGaugeImage = _get_nth_image (pGauge, pGaugeIndicator, fValue);
	
	int iWidth, iHeight;
	cairo_data_renderer_get_size (CAIRO_DATA_RENDERER (pGauge), &iWidth, &iHeight);
	
	if (pGaugeImage && pGaugeImage->image.iTexture != 0)
	{
		glBindTexture (GL_TEXTURE_2D, pGaugeImage->image.iTexture);\
		switch (pGaugeIndicator->iEffect)
		{
			case CD_GAUGE_EFFECT_CROP :
				_cairo_dock_apply_current_texture_at_size_crop (pGaugeImage->image.iTexture, iWidth, iHeight, fValue);
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
	if (fValue <= CAIRO_DATA_RENDERER_UNDEF_VALUE+1)
		return;
	
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	g_return_if_fail (pGaugeImage != NULL);
	
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	if(pGaugeImage->image.iTexture != 0)
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
		_cairo_dock_apply_texture_at_size (pGaugeImage->image.iTexture, pGaugeIndicator->iNeedleWidth, pGaugeIndicator->iNeedleHeight);
		
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
		if (pGaugeImage->image.iTexture != 0)
			_cairo_dock_apply_texture_at_size (pGaugeImage->image.iTexture, iWidth, iHeight);
	}
	
	//\________________ On represente l'indicateur de chaque valeur.
	GList *pIndicatorElement;
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
		if (pGaugeImage->image.iTexture != 0)
			_cairo_dock_apply_texture_at_size (pGaugeImage->image.iTexture, iWidth, iHeight);
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

				case CD_GAUGE_NB_MULTI_DISPLAY :
					break;
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
	
	int iWidth, iHeight;
	cairo_data_renderer_get_size (CAIRO_DATA_RENDERER (pGauge), &iWidth, &iHeight);
	
	if (pGauge->pImageBackground)
		_reload_gauge_image (pGauge->pImageBackground, iWidth, iHeight);
	
	if (pGauge->pImageForeground)
		_reload_gauge_image (pGauge->pImageForeground, iWidth, iHeight);
	
	GaugeIndicator *pGaugeIndicator;
	int i;
	GList *pElement;
	for (pElement = pGauge->pIndicatorList; pElement != NULL; pElement = pElement->next)
	{
		pGaugeIndicator = pElement->data;
		for (i = 0; i < pGaugeIndicator->iNbImages; i ++)
		{
			_reload_gauge_image (&pGaugeIndicator->pImageList[i], iWidth, iHeight);
		}
		if (pGaugeIndicator->pImageNeedle)
		{
			_reload_gauge_needle (pGaugeIndicator, iWidth, iHeight);
		}
	}
}

  ////////////////////////////////////////////
 /////////////// FREE GAUGE /////////////////
////////////////////////////////////////////
static void _cairo_dock_free_gauge_image (GaugeImage *pGaugeImage, gboolean bFree)
{
	if (pGaugeImage == NULL)
		return ;

	cairo_dock_unload_image_buffer (&pGaugeImage->image);
	g_free (pGaugeImage->cImagePath);
	
	if (bFree)
		g_free (pGaugeImage);
}
static void _cairo_dock_free_gauge_indicator(GaugeIndicator *pGaugeIndicator)
{
	if (pGaugeIndicator == NULL)
		return ;
	
	int i;
	for (i = 0; i < pGaugeIndicator->iNbImages; i ++)
	{
		_cairo_dock_free_gauge_image (&pGaugeIndicator->pImageList[i], FALSE);
	}
	g_free (pGaugeIndicator->pImageList);
	
	_cairo_dock_free_gauge_image (pGaugeIndicator->pImageUndef, TRUE);
	
	_cairo_dock_free_gauge_image (pGaugeIndicator->pImageNeedle, TRUE);
	
	g_free (pGaugeIndicator);
}
static void unload (Gauge *pGauge)
{
	cd_debug("");
	
	_cairo_dock_free_gauge_image(pGauge->pImageBackground, TRUE);
	_cairo_dock_free_gauge_image(pGauge->pImageForeground, TRUE);
	
	g_list_foreach (pGauge->pIndicatorList, (GFunc)_cairo_dock_free_gauge_indicator, NULL);
	g_list_free (pGauge->pIndicatorList);
}


  //////////////////////////////////////////
 /////////////// RENDERER /////////////////
//////////////////////////////////////////
void cairo_dock_register_data_renderer_gauge (void)
{
	// create a new record
	CairoDockDataRendererRecord *pRecord = g_new0 (CairoDockDataRendererRecord, 1);
	
	// fill the properties we need
	pRecord->interface.load          = (CairoDataRendererLoadFunc) load;
	pRecord->interface.render        = (CairoDataRendererRenderFunc) render;
	pRecord->interface.render_opengl = (CairoDataRendererRenderOpenGLFunc) render_opengl;
	pRecord->interface.reload        = (CairoDataRendererReloadFunc) reload;
	pRecord->interface.unload        = (CairoDataRendererUnloadFunc) unload;
	pRecord->iStructSize             = sizeof (Gauge);
	pRecord->cThemeDirName           = "gauges";
	pRecord->cDistantThemeDirName    = "gauges3";
	pRecord->cDefaultTheme           = "Turbo-night-fuel";
	
	// register
	cairo_dock_register_data_renderer ("gauge", pRecord);
}
