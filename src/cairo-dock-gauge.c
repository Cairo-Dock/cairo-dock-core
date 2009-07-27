/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Necropotame (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>
#include <math.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

//#include <cairo-dock-struct.h>
#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gauge.h"

extern gchar *g_cCairoDockDataDir;
extern gboolean g_bUseOpenGL;

  ////////////////////////////////////////////
 /////////////// LOAD GAUGE /////////////////
////////////////////////////////////////////
void cairo_dock_xml_open_file (const gchar *filePath, const gchar *mainNodeName,xmlDocPtr *myXmlDoc,xmlNodePtr *myXmlNode)
{
	xmlDocPtr doc = xmlParseFile (filePath);
	if (doc == NULL)
	{
		cd_warning ("Impossible de lire le fichier XML.");
		*myXmlDoc = NULL;
		*myXmlNode = NULL;
		return ;
	}
	
	xmlNodePtr node = xmlDocGetRootElement (doc);
	if (node == NULL || xmlStrcmp (node->name, (const xmlChar *) mainNodeName) != 0)
	{
		cd_warning ("Le format du fichier XML n'est pas valide.");
		*myXmlDoc = NULL;
		*myXmlNode = NULL;
		return ;
	}
	
	*myXmlDoc = doc;
	*myXmlNode = node;
}

static void _cairo_dock_init_gauge_image (const gchar *cImagePath, GaugeImage *pGaugeImage)
{
	// chargement du fichier.
	pGaugeImage->pSvgHandle = rsvg_handle_new_from_file (cImagePath, NULL);
	
	//On récupère la taille de l'image.
	RsvgDimensionData SizeInfo;
	rsvg_handle_get_dimensions (pGaugeImage->pSvgHandle, &SizeInfo);
	pGaugeImage->sizeX = SizeInfo.width;
	pGaugeImage->sizeY = SizeInfo.height;
}
static GaugeImage *_cairo_dock_new_gauge_image (const gchar *cImagePath)
{
	cd_debug ("%s (%s)", __func__, cImagePath);
	GaugeImage *pGaugeImage = g_new0 (GaugeImage, 1);
	
	_cairo_dock_init_gauge_image (cImagePath, pGaugeImage);
	
	return pGaugeImage;
}
static void _cairo_dock_load_gauge_image (cairo_t *pSourceContext, GaugeImage *pGaugeImage, int iWidth, int iHeight)
{
	cd_message ("%s (%dx%d)", __func__, iWidth, iHeight);
	if (pGaugeImage->pSurface != NULL)
		cairo_surface_destroy (pGaugeImage->pSurface);
	if (pGaugeImage->iTexture != 0)
		_cairo_dock_delete_texture (pGaugeImage->iTexture);
	
	if (pGaugeImage->pSvgHandle != NULL)
	{
		pGaugeImage->pSurface = _cairo_dock_create_blank_surface (pSourceContext,
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
static void _cairo_dock_load_gauge_needle (cairo_t *pSourceContext, GaugeIndicator2 *pGaugeIndicator, int iWidth, int iHeight)
{
	cd_message ("%s (%dx%d)", __func__, iWidth, iHeight);
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	g_return_if_fail (pGaugeImage != NULL);
	
	if (pGaugeImage->pSurface != NULL)
		cairo_surface_destroy (pGaugeImage->pSurface);
	if (pGaugeImage->iTexture != 0)
		_cairo_dock_delete_texture (pGaugeImage->iTexture);
	
	if (pGaugeImage->pSvgHandle != NULL)
	{
		int iSize = MIN (iWidth, iHeight);
		g_print ("size : applet : %d, image : %d\n", iSize, pGaugeImage->sizeX);
		pGaugeIndicator->fNeedleScale = (double)iSize / (double) pGaugeImage->sizeX;  // car l'aiguille est a l'horizontale dans le fichier svg.
		pGaugeIndicator->iNeedleWidth = (double) pGaugeIndicator->iNeedleRealWidth * pGaugeIndicator->fNeedleScale;
		pGaugeIndicator->iNeedleHeight = (double) pGaugeIndicator->iNeedleRealHeight * pGaugeIndicator->fNeedleScale;
		g_print (" + needle %dx%d\n", pGaugeIndicator->iNeedleWidth, pGaugeIndicator->iNeedleHeight);
		
		cairo_surface_t *pNeedleSurface = _cairo_dock_create_blank_surface (pSourceContext, pGaugeIndicator->iNeedleWidth, pGaugeIndicator->iNeedleHeight);
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
static gboolean cairo_dock_load_gauge_theme (Gauge *pGauge, cairo_t *pSourceContext, const gchar *cThemePath)
{
	cd_debug ("%s (%s)", __func__, cThemePath);
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	if (iWidth == 0 || iHeight == 0)
		return FALSE;
	
	g_return_val_if_fail (cThemePath != NULL, FALSE);
	xmlInitParser ();
	xmlDocPtr pGaugeTheme;
	xmlNodePtr pGaugeMainNode;
	gchar *cXmlFile = g_strdup_printf("%s/theme.xml",cThemePath);
	cairo_dock_xml_open_file (cXmlFile, "gauge",&pGaugeTheme,&pGaugeMainNode);
	g_free (cXmlFile);
	g_return_val_if_fail (pGaugeTheme != NULL, FALSE);
	
	xmlAttrPtr ap;
	xmlChar *cAttribute, *cNodeContent, *cTextNodeContent;
	GString *sImagePath = g_string_new ("");
	GaugeImage *pGaugeImage;
	GaugeIndicator2 *pGaugeIndicator = NULL;
	xmlNodePtr pGaugeNode;
	for (pGaugeNode = pGaugeMainNode->xmlChildrenNode; pGaugeNode != NULL; pGaugeNode = pGaugeNode->next)
	{
		if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "name") == 0)
		{
			pGauge->cThemeName = xmlNodeGetContent(pGaugeNode);
			cd_debug("gauge : Nom du theme(%s)",pGauge->pIndicatorList);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "rank") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			CAIRO_DATA_RENDERER (pGauge)->iRank = atoi (cNodeContent);
			xmlFree (cNodeContent);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "file") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
			ap = xmlHasProp (pGaugeNode, "key");
			cAttribute = xmlNodeGetContent(ap->xmlChildrenNode);
			if (xmlStrcmp (cAttribute, "background") == 0)
			{
				pGauge->pImageBackground = _cairo_dock_new_gauge_image (sImagePath->str);
				_cairo_dock_load_gauge_image (pSourceContext, pGauge->pImageBackground, iWidth, iHeight);
			}
			else if (xmlStrcmp (cAttribute, "foreground") == 0)
			{
				pGauge->pImageForeground = _cairo_dock_new_gauge_image (sImagePath->str);
				_cairo_dock_load_gauge_image (pSourceContext, pGauge->pImageForeground, iWidth, iHeight);
			}
			xmlFree (cNodeContent);
			xmlFree (cAttribute);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "indicator") == 0)
		{
			if (CAIRO_DATA_RENDERER (pGauge)->iRank == 0)
			{
				CAIRO_DATA_RENDERER (pGauge)->iRank = 1;
				xmlNodePtr node;
				for (node = pGaugeNode->next; node != NULL; node = node->next)
				{
					if (xmlStrcmp (node->name, (const xmlChar *) "indicator") == 0)
						CAIRO_DATA_RENDERER (pGauge)->iRank ++;
				}
			}

			pGaugeIndicator = g_new0 (GaugeIndicator2, 1);
			pGaugeIndicator->direction = 1;
			
			cd_debug ("gauge : On charge un indicateur");
			xmlNodePtr pGaugeSubNode;
			for (pGaugeSubNode = pGaugeNode->xmlChildrenNode; pGaugeSubNode != NULL; pGaugeSubNode = pGaugeSubNode->next)
			{
				if (xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "text") == 0)
					continue;
				//g_print ("+ %s\n", pGaugeSubNode->name);
				cNodeContent = xmlNodeGetContent (pGaugeSubNode);
				if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posX") == 0)
					pGaugeIndicator->posX = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posY") == 0)
					pGaugeIndicator->posY = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "text_zone") == 0)
				{
					xmlNodePtr pTextSubNode;
					for (pTextSubNode = pGaugeSubNode->xmlChildrenNode; pTextSubNode != NULL; pTextSubNode = pTextSubNode->next)
					{
						if (xmlStrcmp (pTextSubNode->name, (const xmlChar *) "text") == 0)
							continue;
						//g_print ("  + %s\n", pTextSubNode->name);
						cTextNodeContent = xmlNodeGetContent (pTextSubNode);
						//g_print ("     -> '%s'\n", cTextNodeContent);
						if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "textX") == 0)
							pGaugeIndicator->textX = atof (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "textY") == 0)
							pGaugeIndicator->textY = atof (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "textWidth") == 0)
							pGaugeIndicator->textWidth = atof (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "textHeight") == 0)
							pGaugeIndicator->textHeight = atof (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "textColorR") == 0)
							pGaugeIndicator->textColor[0] = atof (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "textColorG") == 0)
							pGaugeIndicator->textColor[1] = atof (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "textColorB") == 0)
							pGaugeIndicator->textColor[2] = atof (cTextNodeContent);
					}
					g_print ("pGaugeIndicator->textWidth:%.2f\n", pGaugeIndicator->textWidth);
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "direction") == 0)
					pGaugeIndicator->direction = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStart") == 0)
					pGaugeIndicator->posStart = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStop") == 0)
					pGaugeIndicator->posStop = atof (cNodeContent);
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
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "file") == 0)
				{
					cd_debug("gauge : On charge un fichier (%s)",cNodeContent);
					ap = xmlHasProp(pGaugeSubNode, "key");
					cAttribute = xmlNodeGetContent(ap->xmlChildrenNode);
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
								_cairo_dock_load_gauge_image (pSourceContext, &pGaugeIndicator->pImageList[pGaugeIndicator->iNbImageLoaded], iWidth, iHeight);
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
					_cairo_dock_load_gauge_needle (pSourceContext, pGaugeIndicator, iWidth, iHeight);
			}
			pGauge->pIndicatorList = g_list_append (pGauge->pIndicatorList, pGaugeIndicator);
		}
	}
	xmlFreeDoc (pGaugeTheme);
	xmlCleanupParser ();
	g_string_free (sImagePath, TRUE);
	
	g_return_val_if_fail (CAIRO_DATA_RENDERER (pGauge)->iRank != 0 && pGaugeIndicator != NULL, FALSE);
	CAIRO_DATA_RENDERER (pGauge)->bCanRenderValueAsText = (pGaugeIndicator->textWidth != 0 && pGaugeIndicator->textHeight != 0);
	
	return TRUE;
}
void cairo_dock_load_gauge (Gauge *pGauge, cairo_t *pSourceContext, CairoContainer *pContainer, CairoGaugeAttribute *pAttribute)
{
	cairo_dock_load_gauge_theme (pGauge, pSourceContext, pAttribute->cThemePath);
}

  ////////////////////////////////////////////
 /////////////// DRAW GAUGE /////////////////
////////////////////////////////////////////
static void _draw_gauge_needle (cairo_t *pSourceContext, Gauge *pGauge, GaugeIndicator2 *pGaugeIndicator, double fValue)
{
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	if(pGaugeImage != NULL)
	{
		double fAngle = (pGaugeIndicator->posStart + fValue * (pGaugeIndicator->posStop - pGaugeIndicator->posStart)) * G_PI / 180.;
		if (pGaugeIndicator->direction < 0)
			fAngle = - fAngle;
		
		double fHalfX = pGauge->pImageBackground->sizeX / 2.0f * (1 + pGaugeIndicator->posX);
		double fHalfY = pGauge->pImageBackground->sizeY / 2.0f * (1 - pGaugeIndicator->posY);
		
		cairo_save (pSourceContext);
		
		cairo_scale (pSourceContext,
			(double) CAIRO_DATA_RENDERER (pGauge)->iWidth / (double) pGaugeImage->sizeX,
			(double) CAIRO_DATA_RENDERER (pGauge)->iHeight / (double) pGaugeImage->sizeY);
		cairo_translate (pSourceContext, fHalfX, fHalfY);
		cairo_rotate (pSourceContext, -G_PI/2 + fAngle);
		
		rsvg_handle_render_cairo (pGaugeImage->pSvgHandle, pSourceContext);
		
		cairo_restore (pSourceContext);
	}
}
static void _draw_gauge_image (cairo_t *pSourceContext, Gauge *pGauge, GaugeIndicator2 *pGaugeIndicator, double fValue)
{
	int iNumImage = fValue * (pGaugeIndicator->iNbImages - 1) + 0.5;
	g_return_if_fail (iNumImage < pGaugeIndicator->iNbImages);
	
	GaugeImage *pGaugeImage = &pGaugeIndicator->pImageList[iNumImage];
	if (pGaugeImage->pSurface == NULL)
	{
		int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
		_cairo_dock_load_gauge_image (pSourceContext, pGaugeImage, iWidth, iHeight);
	}
	
	if (pGaugeImage->pSurface != NULL)
	{
		cairo_set_source_surface (pSourceContext, pGaugeImage->pSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
}
static void cairo_dock_draw_one_gauge (cairo_t *pSourceContext, Gauge *pGauge, int iDataOffset)
{
	GaugeImage *pGaugeImage;
	//\________________ On affiche le fond.
	if(pGauge->pImageBackground != NULL)
	{
		pGaugeImage = pGauge->pImageBackground;
		cairo_set_source_surface (pSourceContext, pGaugeImage->pSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
	
	//\________________ On represente l'indicateur de chaque valeur.
	GList *pIndicatorElement;
	GList *pValueList;
	double fValue;
	GaugeIndicator2 *pIndicator;
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int i;
	for (i = iDataOffset, pIndicatorElement = pGauge->pIndicatorList; i < pData->iNbValues && pIndicatorElement != NULL; i++, pIndicatorElement = pIndicatorElement->next)
	{
		pIndicator = pIndicatorElement->data;
		fValue = cairo_data_renderer_get_normalized_current_value (pRenderer, i);
		
		if (pIndicator->pImageNeedle != NULL)  // c'est une aiguille.
		{
			_draw_gauge_needle (pSourceContext, pGauge, pIndicator, fValue);
		}
		else  // c'est une image.
		{
			_draw_gauge_image (pSourceContext, pGauge, pIndicator, fValue);
		}

		if (pIndicator->textWidth != 0 && pIndicator->textHeight != 0)  // cet indicateur a un emplacement pour le texte de la valeur.
		{
			cairo_data_renderer_format_value (pRenderer, fValue, i);
			//g_print (" >>>%s\n", pRenderer->cFormatBuffer);
			cairo_save (pSourceContext);
			cairo_set_source_rgb (pSourceContext, pIndicator->textColor[0], pIndicator->textColor[1], pIndicator->textColor[2]);
			cairo_set_line_width (pSourceContext, 20);
			
			cairo_text_extents_t textExtents;
			cairo_text_extents (pSourceContext, pRenderer->cFormatBuffer, &textExtents);
			double fZoom = MIN (pIndicator->textWidth * pRenderer->iWidth / textExtents.width, pIndicator->textHeight * pRenderer->iHeight / textExtents.height);
			cairo_move_to (pSourceContext,
				(1. + pIndicator->textX) * pRenderer->iWidth/2 - textExtents.width / 2,
				(1. - pIndicator->textY) * pRenderer->iHeight/2 + textExtents.height);
			cairo_scale (pSourceContext,
				fZoom,
				fZoom);
			cairo_show_text (pSourceContext, pRenderer->cFormatBuffer);
			cairo_restore (pSourceContext);
		}
	}
	
	//\________________ On affiche l'avant-plan.
	if(pGauge->pImageForeground != NULL)
	{
		pGaugeImage = pGauge->pImageForeground;
		cairo_set_source_surface (pSourceContext, pGaugeImage->pSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
}
void cairo_dock_render_gauge (Gauge *pGauge, cairo_t *pCairoContext)
{
	g_return_if_fail (pGauge != NULL && pGauge->pIndicatorList != NULL && pCairoContext != NULL);
	g_return_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
	
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
static void _draw_gauge_image_opengl (Gauge *pGauge, GaugeIndicator2 *pGaugeIndicator, double fValue)
{
	int iNumImage = fValue * (pGaugeIndicator->iNbImages - 1) + 0.5;
	g_return_if_fail (iNumImage < pGaugeIndicator->iNbImages);
	
	GaugeImage *pGaugeImage = &pGaugeIndicator->pImageList[iNumImage];
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	/*if (pGaugeImage->iTexture == 0)
	{
		_cairo_dock_load_gauge_image (NULL, pGaugeImage, iWidth, iHeight);  // pas besoin d'un cairo_context pour creer une cairo_image_surface.
		return ;
	}*/
	
	if (pGaugeImage->iTexture != 0)
	{
		_cairo_dock_apply_texture_at_size (pGaugeImage->iTexture, iWidth, iHeight);
	}
}
static void _draw_gauge_needle_opengl (Gauge *pGauge, GaugeIndicator2 *pGaugeIndicator, double fValue)
{
	GaugeImage *pGaugeImage = pGaugeIndicator->pImageNeedle;
	g_return_if_fail (pGaugeImage != NULL);
	
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	/*if (pGaugeImage->iTexture == 0)
	{
		_cairo_dock_load_gauge_needle (NULL, pGaugeIndicator, iWidth, iHeight);  // pas besoin d'un cairo_context pour creer une cairo_image_surface.
		return ;
	}*/
	
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
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
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
	GaugeIndicator2 *pIndicator;
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
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

		if (pIndicator->textWidth != 0 && pIndicator->textHeight != 0)  // cet indicateur a un emplacement pour le texte de la valeur.
		{
			cairo_data_renderer_format_value (pRenderer, fValue, i);
			//g_print (" >>>%s\n", pRenderer->cFormatBuffer);
			/*cairo_save (pSourceContext);
			cairo_set_source_rgb (pSourceContext, pIndicator->textColor[0], pIndicator->textColor[1], pIndicator->textColor[2]);
			cairo_set_line_width (pSourceContext, 20.);
			
			cairo_text_extents_t textExtents;
			cairo_text_extents (pSourceContext, pRenderer->cFormatBuffer, &textExtents);
			cairo_move_to (pSourceContext,
				pIndicator->textX * pRenderer->iWidth - textExtents.width / 2,
				pIndicator->textY * pRenderer->iHeight + textExtents.height / 2);
			cairo_show_text (pSourceContext, pRenderer->cFormatBuffer);
			cairo_restore (pSourceContext);*/
		}
	}
	
	//\________________ On affiche l'avant-plan.
	if(pGauge->pImageForeground != NULL)
	{
		pGaugeImage = pGauge->pImageForeground;
		if (pGaugeImage->iTexture != 0)
			_cairo_dock_apply_texture_at_size (pGaugeImage->iTexture, iWidth, iHeight);
	}
}
void cairo_dock_render_gauge_opengl (Gauge *pGauge)
{
	g_return_if_fail (pGauge != NULL && pGauge->pIndicatorList != NULL);
	
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_pbuffer ();  // ceci reste un mystere...
	_cairo_dock_set_alpha (1.);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	CairoDataToRenderer *pData = cairo_data_renderer_get_data (pRenderer);
	int iNbDrawings = (int) ceil (1. * pData->iNbValues / pRenderer->iRank);
	int i, iDataOffset = 0;
	for (i = 0; i < iNbDrawings; i ++)
	{
		if (iNbDrawings > 1)  // on va dessiner la jauges plusieurs fois, la 1ere en grand et les autres en petit autour.
		{
			glPushMatrix ();
			if (i == 0)
			{
				glTranslatef (-pRenderer->iWidth / 6, pRenderer->iHeight / 6, 0.);
				glScalef (2./3, 2./3, 1.);
			}
			else if (i == 1)
			{
				glTranslatef (pRenderer->iWidth / 3, - pRenderer->iHeight / 3, 0.);
				glScalef (1./3, 1./3, 1.);
			}
			else if (i == 2)
			{
				glTranslatef (pRenderer->iWidth / 3, pRenderer->iHeight / 3, 0.);
				glScalef (1./3, 1./3, 1.);
			}
			else if (i == 3)
			{
				glTranslatef (-pRenderer->iWidth / 3, -pRenderer->iHeight / 3, 0.);
				glScalef (1./3, 1./3, 1.);
			}
			else  // 5 valeurs faut pas pousser non plus.
				break ;
		}
		
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
void cairo_dock_reload_gauge (Gauge *pGauge, cairo_t *pSourceContext)
{
	//g_print ("%s (%dx%d)\n", __func__, iWidth, iHeight);
	g_return_if_fail (pGauge != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	if (pGauge->pImageBackground != NULL)
	{
		_cairo_dock_load_gauge_image (pSourceContext, pGauge->pImageBackground, iWidth, iHeight);
	}
	
	if (pGauge->pImageForeground != NULL)
	{
		_cairo_dock_load_gauge_image (pSourceContext, pGauge->pImageForeground, iWidth, iHeight);
	}
	
	GaugeIndicator2 *pGaugeIndicator;
	GaugeImage *pGaugeImage;
	int i;
	GList *pElement, *pElement2;
	for (pElement = pGauge->pIndicatorList; pElement != NULL; pElement = pElement->next)
	{
		pGaugeIndicator = pElement->data;
		for (i = 0; i < pGaugeIndicator->iNbImages; i ++)
		{
			pGaugeImage = &pGaugeIndicator->pImageList[i];
			_cairo_dock_load_gauge_image (pSourceContext, pGaugeImage, iWidth, iHeight);
		}
		if (g_bUseOpenGL && pGaugeIndicator->pImageNeedle)
		{
			_cairo_dock_load_gauge_needle (pSourceContext, pGaugeIndicator, iWidth, iHeight);
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
static void _cairo_dock_free_gauge_indicator(GaugeIndicator2 *pGaugeIndicator)
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
void cairo_dock_free_gauge (Gauge *pGauge)
{
	cd_debug("");
	if(pGauge == NULL)
		return ;
	
	_cairo_dock_free_gauge_image(pGauge->pImageBackground, TRUE);
	_cairo_dock_free_gauge_image(pGauge->pImageForeground, TRUE);
	
	GList *pElement;
	for (pElement = pGauge->pIndicatorList; pElement != NULL; pElement = pElement->next)
	{
		_cairo_dock_free_gauge_indicator (pElement->data);
	}
	g_list_free (pGauge->pIndicatorList);
	
	g_free (pGauge);
}


void cairo_dock_add_watermark_on_gauge (cairo_t *pSourceContext, Gauge *pGauge, gchar *cImagePath, double fAlpha)
{
	g_return_if_fail (pGauge != NULL && cImagePath != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	cairo_surface_t *pWatermarkSurface = cairo_dock_create_surface_for_icon (cImagePath, pSourceContext, pRenderer->iWidth/2, pRenderer->iHeight/2);
	
	if (pGauge->pImageBackground == NULL)
	{
		pGauge->pImageBackground = g_new0 (GaugeImage, 1);
		pGauge->pImageBackground->sizeX = pRenderer->iWidth;
		pGauge->pImageBackground->sizeY = pRenderer->iHeight;
		
		pGauge->pImageBackground->pSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			pRenderer->iWidth,
			pRenderer->iHeight);
	}
	
	cairo_t *pCairoContext = cairo_create (pGauge->pImageBackground->pSurface);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	
	cairo_set_source_surface (pCairoContext, pWatermarkSurface, pRenderer->iWidth/4, pRenderer->iHeight/4);
	cairo_paint_with_alpha (pCairoContext, fAlpha);
	
	cairo_destroy (pCairoContext);
	
	cairo_surface_destroy (pWatermarkSurface);
}

  //////////////////////////////////////////
 /////////////// RENDERER /////////////////
//////////////////////////////////////////
Gauge *cairo_dock_new_gauge (void)
{
	Gauge *pGauge = g_new0 (Gauge, 1);
	pGauge->dataRenderer.interface.new				= (CairoDataRendererNewFunc) cairo_dock_new_gauge;
	pGauge->dataRenderer.interface.load				= (CairoDataRendererLoadFunc) cairo_dock_load_gauge;
	pGauge->dataRenderer.interface.render			= (CairoDataRendererRenderFunc) cairo_dock_render_gauge;
	pGauge->dataRenderer.interface.render_opengl	= (CairoDataRendererRenderOpenGLFunc) cairo_dock_render_gauge_opengl;
	pGauge->dataRenderer.interface.reload			= (CairoDataRendererReloadFunc) cairo_dock_reload_gauge;
	pGauge->dataRenderer.interface.free				= (CairoDataRendererFreeFunc) cairo_dock_free_gauge;
	return pGauge;
}

  /////////////////////////////////////////////////
 /////////////// LIST OF THEMES  /////////////////
/////////////////////////////////////////////////
GHashTable *cairo_dock_list_available_gauges (void)
{
	gchar *cGaugeShareDir = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, CAIRO_DOCK_GAUGES_DIR);
	GHashTable *pGaugeTable = cairo_dock_list_themes (cGaugeShareDir, cGaugeUserDir, CAIRO_DOCK_GAUGES_DIR);
	
	g_free (cGaugeShareDir);
	g_free (cGaugeUserDir);
	return pGaugeTable;
}

gchar *cairo_dock_get_gauge_key_value(gchar *cAppletConfFilePath, GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName)
{
	gchar *cChosenThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	const gchar *cGaugeShareDir = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_GAUGES_DIR;
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR"/"CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugePath = cairo_dock_get_theme_path (cChosenThemeName, cGaugeShareDir, cGaugeUserDir, CAIRO_DOCK_GAUGES_DIR);
	g_free (cGaugeUserDir);
	
	cd_debug ("Theme de la jauge : %s", cGaugePath);
	return cGaugePath;
}

