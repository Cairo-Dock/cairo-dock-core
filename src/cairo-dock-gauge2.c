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
#include <cairo-dock-log.h>
#include <cairo-dock-applet-facility.h>
#include <cairo-dock-draw.h>
#include <cairo-dock-draw-opengl.h>
#include <cairo-dock-themes-manager.h>
#include <cairo-dock-surface-factory.h>
#include <cairo-dock-dock-factory.h>
#include <cairo-dock-keyfile-utilities.h>
#include <cairo-dock-config.h>
#include <cairo-dock-renderer-manager.h>
#include <cairo-dock-internal-icons.h>
#include <cairo-dock-gui-factory.h>
#include <cairo-dock-gauge2.h>

extern gchar *g_cCairoDockDataDir;
extern gboolean g_bUseOpenGL;

  ////////////////////////////////////////////
 /////////////// LOAD GAUGE /////////////////
////////////////////////////////////////////
void cairo_dock_xml_open_file2 (const gchar *filePath, const gchar *mainNodeName,xmlDocPtr *myXmlDoc,xmlNodePtr *myXmlNode)
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

static void _cairo_dock_init_gauge_image (const gchar *cImagePath, GaugeImage2 *pGaugeImage2)
{
	// chargement du fichier.
	pGaugeImage2->pSvgHandle = rsvg_handle_new_from_file (cImagePath, NULL);
	
	//On récupère la taille de l'image.
	RsvgDimensionData SizeInfo;
	rsvg_handle_get_dimensions (pGaugeImage2->pSvgHandle, &SizeInfo);
	pGaugeImage2->sizeX = SizeInfo.width;
	pGaugeImage2->sizeY = SizeInfo.height;
}
static GaugeImage2 *_cairo_dock_new_gauge_image (const gchar *cImagePath)
{
	cd_debug ("%s (%s)", __func__, cImagePath);
	GaugeImage2 *pGaugeImage2 = g_new0 (GaugeImage2, 1);
	
	_cairo_dock_init_gauge_image (cImagePath, pGaugeImage2);
	
	return pGaugeImage2;
}
static void _cairo_dock_load_gauge_image (cairo_t *pSourceContext, GaugeImage2 *pGaugeImage2, int iWidth, int iHeight)
{
	cd_message ("%s (%dx%d)", __func__, iWidth, iHeight);
	if (pGaugeImage2->pSurface != NULL)
		cairo_surface_destroy (pGaugeImage2->pSurface);
	if (pGaugeImage2->iTexture != 0)
		glDeleteTextures (1, &pGaugeImage2->iTexture);
	
	if (pGaugeImage2->pSvgHandle != NULL)
	{
		pGaugeImage2->pSurface = _cairo_dock_create_blank_surface (pSourceContext,
			iWidth,
			iHeight);
		
		cairo_t* pDrawingContext = cairo_create (pGaugeImage2->pSurface);
		
		cairo_scale (pDrawingContext,
			(double) iWidth / (double) pGaugeImage2->sizeX,
			(double) iHeight / (double) pGaugeImage2->sizeY);
		
		rsvg_handle_render_cairo (pGaugeImage2->pSvgHandle, pDrawingContext);
		cairo_destroy (pDrawingContext);
		
		if (g_bUseOpenGL)
		{
			pGaugeImage2->iTexture = cairo_dock_create_texture_from_surface (pGaugeImage2->pSurface);
		}
	}
	else
	{
		pGaugeImage2->pSurface = NULL;
		pGaugeImage2->iTexture = 0;
	}
}
static void cairo_dock_load_gauge_needle (cairo_t *pSourceContext, GaugeIndicator2 *pGaugeIndicator2, int iWidth, int iHeight)
{
	cd_message ("%s (%dx%d)", __func__, iWidth, iHeight);
	GaugeImage2 *pGaugeImage2 = pGaugeIndicator2->pImageNeedle;
	g_return_if_fail (pGaugeImage2 != NULL);
	
	if (pGaugeImage2->pSurface != NULL)
		cairo_surface_destroy (pGaugeImage2->pSurface);
	if (pGaugeImage2->iTexture != 0)
		glDeleteTextures (1, &pGaugeImage2->iTexture);
	
	if (pGaugeImage2->pSvgHandle != NULL)
	{
		int iSize = MIN (iWidth, iHeight);
		pGaugeIndicator2->fNeedleScale = (double)iSize / (double) pGaugeImage2->sizeX;  // car l'aiguille est a l'horizontale dans le fichier svg.
		pGaugeIndicator2->iNeedleWidth = (double) pGaugeIndicator2->iNeedleRealWidth * pGaugeIndicator2->fNeedleScale;
		pGaugeIndicator2->iNeedleHeight = (double) pGaugeIndicator2->iNeedleRealHeight * pGaugeIndicator2->fNeedleScale;
		
		cairo_surface_t *pNeedleSurface = _cairo_dock_create_blank_surface (pSourceContext, pGaugeIndicator2->iNeedleWidth, pGaugeIndicator2->iNeedleHeight);
		g_return_if_fail (cairo_surface_status (pNeedleSurface) == CAIRO_STATUS_SUCCESS);
		
		cairo_t* pDrawingContext = cairo_create (pNeedleSurface);
		g_return_if_fail (cairo_status (pDrawingContext) == CAIRO_STATUS_SUCCESS);
		
		cairo_scale (pDrawingContext, pGaugeIndicator2->fNeedleScale, pGaugeIndicator2->fNeedleScale);
		cairo_translate (pDrawingContext, pGaugeIndicator2->iNeedleOffsetX, pGaugeIndicator2->iNeedleOffsetY);
		rsvg_handle_render_cairo (pGaugeImage2->pSvgHandle, pDrawingContext);
		cairo_destroy (pDrawingContext);
		
		pGaugeImage2->iTexture = cairo_dock_create_texture_from_surface (pNeedleSurface);
		cairo_surface_destroy (pNeedleSurface);
	}
	else
	{
		pGaugeImage2->pSurface = NULL;
		pGaugeImage2->iTexture = 0;
	}
}
static gboolean cairo_dock_load_gauge_theme (Gauge2 *pGauge, cairo_t *pSourceContext, const gchar *cThemePath)
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
	cairo_dock_xml_open_file2 (cXmlFile, "gauge",&pGaugeTheme,&pGaugeMainNode);
	g_free (cXmlFile);
	g_return_val_if_fail (pGaugeTheme != NULL, FALSE);
	
	xmlAttrPtr ap;
	xmlChar *cAttribute, *cNodeContent, *cTextNodeContent;
	GString *sImagePath = g_string_new ("");
	GaugeImage2 *pGaugeImage2;
	GaugeIndicator2 *pGaugeIndicator2 = NULL;
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

			pGaugeIndicator2 = g_new0 (GaugeIndicator2, 1);
			pGaugeIndicator2->direction = 1;
			
			cd_debug ("gauge : On charge un indicateur");
			xmlNodePtr pGaugeSubNode;
			for (pGaugeSubNode = pGaugeNode->xmlChildrenNode; pGaugeSubNode != NULL; pGaugeSubNode = pGaugeSubNode->next)
			{
				cNodeContent = xmlNodeGetContent (pGaugeSubNode);
				if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posX") == 0)
					pGaugeIndicator2->posX = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posY") == 0)
					pGaugeIndicator2->posY = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "text") == 0)
				{
					xmlNodePtr pTextSubNode;
					for (pTextSubNode = pGaugeSubNode->xmlChildrenNode; pTextSubNode != NULL; pTextSubNode = pTextSubNode->next)
					{
						cTextNodeContent = xmlNodeGetContent (pTextSubNode);
						if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textX") == 0)
							pGaugeIndicator2->textX = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textY") == 0)
							pGaugeIndicator2->textY = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textWidth") == 0)
							pGaugeIndicator2->textWidth = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textHeight") == 0)
							pGaugeIndicator2->textHeight = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textColorR") == 0)
							pGaugeIndicator2->textColor[0] = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textColorG") == 0)
							pGaugeIndicator2->textColor[1] = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textColorB") == 0)
							pGaugeIndicator2->textColor[2] = atof (cNodeContent);
					}
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "direction") == 0)
					pGaugeIndicator2->direction = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStart") == 0)
					pGaugeIndicator2->posStart = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStop") == 0)
					pGaugeIndicator2->posStop = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "nb images") == 0)
					pGaugeIndicator2->iNbImages = atoi (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "offset x") == 0)
				{
					pGaugeIndicator2->iNeedleOffsetX = atoi (cNodeContent);
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "width") == 0)
				{
					pGaugeIndicator2->iNeedleRealWidth = atoi (cNodeContent);
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "height") == 0)
				{
					pGaugeIndicator2->iNeedleRealHeight = atoi (cNodeContent);
					pGaugeIndicator2->iNeedleOffsetY = .5 * pGaugeIndicator2->iNeedleRealHeight;
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "file") == 0)
				{
					cd_debug("gauge : On charge un fichier (%s)",cNodeContent);
					ap = xmlHasProp(pGaugeSubNode, "key");
					cAttribute = xmlNodeGetContent(ap->xmlChildrenNode);
					if (strcmp (cAttribute, "needle") == 0 && pGaugeIndicator2->pImageNeedle == NULL)
					{
						g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
						pGaugeIndicator2->pImageNeedle = _cairo_dock_new_gauge_image (sImagePath->str);
					}
					else if (strcmp (cAttribute,"image") == 0)
					{
						if (pGaugeIndicator2->iNbImages == 0)
						{
							pGaugeIndicator2->iNbImages = 1;
							xmlNodePtr node;
							for (node = pGaugeSubNode->next; node != NULL; node = node->next)
							{
								if (xmlStrcmp (node->name, (const xmlChar *) "file") == 0)
									pGaugeIndicator2->iNbImages ++;
							}
						}
						if (pGaugeIndicator2->pImageList == NULL)
							pGaugeIndicator2->pImageList = g_new0 (GaugeImage2, pGaugeIndicator2->iNbImages);
						
						if (pGaugeIndicator2->iNbImageLoaded < pGaugeIndicator2->iNbImages)
						{
							g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
							_cairo_dock_init_gauge_image (sImagePath->str, &pGaugeIndicator2->pImageList[pGaugeIndicator2->iNbImageLoaded]);
							pGaugeIndicator2->iNbImageLoaded ++;
						}
					}
					xmlFree (cAttribute);
				}
				xmlFree (cNodeContent);
			}
			if (pGaugeIndicator2->pImageNeedle != NULL)  // taille par defaut de l'aiguille si non fournie.
			{
				if (pGaugeIndicator2->iNeedleRealHeight == 0)
				{
					pGaugeIndicator2->iNeedleRealHeight = .12*pGaugeIndicator2->pImageNeedle->sizeY;  // 12px utiles sur les 100
					pGaugeIndicator2->iNeedleOffsetY = pGaugeIndicator2->iNeedleRealHeight/2;
				}
				if (pGaugeIndicator2->iNeedleRealWidth == 0)
				{
					pGaugeIndicator2->iNeedleRealWidth = pGaugeIndicator2->pImageNeedle->sizeY;  // 100px utiles sur les 100
					pGaugeIndicator2->iNeedleOffsetX = 10;
				}
			}
			pGauge->pIndicatorList = g_list_append (pGauge->pIndicatorList, pGaugeIndicator2);
		}
	}
	xmlFreeDoc (pGaugeTheme);
	xmlCleanupParser ();
	g_string_free (sImagePath, TRUE);
	
	g_return_val_if_fail (CAIRO_DATA_RENDERER (pGauge)->iRank != 0 && pGaugeIndicator2 != NULL, FALSE);
	CAIRO_DATA_RENDERER (pGauge)->bCanRenderValueAsText = (pGaugeIndicator2->textWidth != 0 && pGaugeIndicator2->textHeight != 0);
	
	return TRUE;
}
void cairo_dock_load_gauge2 (Gauge2 *pGauge, cairo_t *pSourceContext, CairoContainer *pContainer, CairoGaugeAttribute *pAttribute)
{
	gboolean bLoadOK = cairo_dock_load_gauge_theme (pGauge, pSourceContext, pAttribute->cThemePath);
}

  ////////////////////////////////////////////
 /////////////// DRAW GAUGE /////////////////
////////////////////////////////////////////
static void _draw_gauge_needle (cairo_t *pSourceContext, Gauge2 *pGauge, GaugeIndicator2 *pGaugeIndicator2, double fValue)
{
	GaugeImage2 *pGaugeImage2 = pGaugeIndicator2->pImageNeedle;
	if(pGaugeImage2 != NULL)
	{
		double fAngle = (pGaugeIndicator2->posStart + fValue * (pGaugeIndicator2->posStop - pGaugeIndicator2->posStart)) * G_PI / 180.;
		if (pGaugeIndicator2->direction < 0)
			fAngle = - fAngle;
		
		double fHalfX = pGauge->pImageBackground->sizeX / 2.0f * (1 + pGaugeIndicator2->posX);
		double fHalfY = pGauge->pImageBackground->sizeY / 2.0f * (1 - pGaugeIndicator2->posY);
		
		cairo_save (pSourceContext);
		
		cairo_scale (pSourceContext,
			(double) CAIRO_DATA_RENDERER (pGauge)->iWidth / (double) pGaugeImage2->sizeX,
			(double) CAIRO_DATA_RENDERER (pGauge)->iHeight / (double) pGaugeImage2->sizeY);
		cairo_translate (pSourceContext, fHalfX, fHalfY);
		cairo_rotate (pSourceContext, -G_PI/2 + fAngle);
		
		rsvg_handle_render_cairo (pGaugeImage2->pSvgHandle, pSourceContext);
		
		cairo_restore (pSourceContext);
	}
}
static void _draw_gauge_image (cairo_t *pSourceContext, Gauge2 *pGauge, GaugeIndicator2 *pGaugeIndicator2, double fValue)
{
	int iNumImage = fValue * (pGaugeIndicator2->iNbImages - 1) + 0.5;
	g_return_if_fail (iNumImage < pGaugeIndicator2->iNbImages);
	
	GaugeImage2 *pGaugeImage2 = &pGaugeIndicator2->pImageList[iNumImage];
	if (pGaugeImage2->pSurface == NULL)
	{
		int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
		_cairo_dock_load_gauge_image (pSourceContext, pGaugeImage2, iWidth, iHeight);
	}
	
	if (pGaugeImage2->pSurface != NULL)
	{
		cairo_set_source_surface (pSourceContext, pGaugeImage2->pSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
}
static void cairo_dock_draw_one_gauge (cairo_t *pSourceContext, Gauge2 *pGauge, int iDataOffset)
{
	GaugeImage2 *pGaugeImage2;
	//\________________ On affiche le fond.
	if(pGauge->pImageBackground != NULL)
	{
		pGaugeImage2 = pGauge->pImageBackground;
		cairo_set_source_surface (pSourceContext, pGaugeImage2->pSurface, 0.0f, 0.0f);
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
		fValue = cairo_data_renderer_get_normalized_current_value_with_latency (pRenderer, i);
		
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
			g_print (" >>>%s\n", pRenderer->cFormatBuffer);
			cairo_save (pSourceContext);
			cairo_set_source_rgb (pSourceContext, pIndicator->textColor[0], pIndicator->textColor[1], pIndicator->textColor[2]);
			cairo_set_line_width (pSourceContext, 20.);
			
			cairo_text_extents_t textExtents;
			cairo_text_extents (pSourceContext, pRenderer->cFormatBuffer, &textExtents);
			cairo_move_to (pSourceContext,
				pIndicator->textX * pRenderer->iWidth - textExtents.width / 2,
				pIndicator->textY * pRenderer->iHeight + textExtents.height / 2);
			cairo_show_text (pSourceContext, pRenderer->cFormatBuffer);
			cairo_restore (pSourceContext);
		}
	}
	
	//\________________ On affiche l'avant-plan.
	if(pGauge->pImageForeground != NULL)
	{
		pGaugeImage2 = pGauge->pImageForeground;
		cairo_set_source_surface (pSourceContext, pGaugeImage2->pSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
}
void cairo_dock_render_gauge2 (Gauge2 *pGauge, cairo_t *pCairoContext)
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
			else if (i = 2)
			{
				cairo_translate (pCairoContext, 2 * pRenderer->iWidth / 3, 0.);
				cairo_scale (pCairoContext, 1./3, 1./3);
			}
			else if (i = 3)
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
static void _draw_gauge_image_opengl (Gauge2 *pGauge, GaugeIndicator2 *pGaugeIndicator2, double fValue)
{
	int iNumImage = fValue * (pGaugeIndicator2->iNbImages - 1) + 0.5;
	g_return_if_fail (iNumImage < pGaugeIndicator2->iNbImages);
	
	GaugeImage2 *pGaugeImage2 = &pGaugeIndicator2->pImageList[iNumImage];
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	if (pGaugeImage2->iTexture == 0)
	{
		_cairo_dock_load_gauge_image (NULL, pGaugeImage2, iWidth, iHeight);  // pas besoin d'un cairo_context pour creer une cairo_image_surface.
	}
	
	if (pGaugeImage2->iTexture != 0)
	{
		cairo_dock_apply_texture_at_size (pGaugeImage2->iTexture, iWidth, iHeight);
	}
}
static void _draw_gauge_needle_opengl (Gauge2 *pGauge, GaugeIndicator2 *pGaugeIndicator2, double fValue)
{
	GaugeImage2 *pGaugeImage2 = pGaugeIndicator2->pImageNeedle;
	g_return_if_fail (pGaugeImage2 != NULL);
	
	if (pGaugeImage2->iTexture == 0)
	{
		int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
		_cairo_dock_load_gauge_image (NULL, pGaugeImage2, iWidth, iHeight);  // pas besoin d'un cairo_context pour creer une cairo_image_surface.
	}
	
	if(pGaugeImage2->iTexture != 0)
	{
		double fAngle = (pGaugeIndicator2->posStart + fValue * (pGaugeIndicator2->posStop - pGaugeIndicator2->posStart));
		if (pGaugeIndicator2->direction < 0)
			fAngle = - fAngle;
		
		double fHalfX = pGauge->pImageBackground->sizeX / 2.0f * (0 + pGaugeIndicator2->posX);
		double fHalfY = pGauge->pImageBackground->sizeY / 2.0f * (0 + pGaugeIndicator2->posY);
		
		glPushMatrix ();
		
		glTranslatef (fHalfX, fHalfY, 0.);
		glRotatef (fAngle + 90., 0., 0., 1.);
		glTranslatef (pGaugeIndicator2->iNeedleWidth/2 - pGaugeIndicator2->fNeedleScale * pGaugeIndicator2->iNeedleOffsetX, 0., 0.);
		glScalef (pGaugeIndicator2->iNeedleWidth, pGaugeIndicator2->iNeedleHeight, 1.);
		cairo_dock_apply_texture (pGaugeImage2->iTexture);
		
		glPopMatrix ();
	}
}
static void cairo_dock_draw_one_gauge_opengl (Gauge2 *pGauge, int iDataOffset)
{
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	GaugeImage2 *pGaugeImage2;
	
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // ne me demandez pas pourquoi...
	
	glEnable (GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
	glPolygonMode (GL_FRONT, GL_FILL);
	
	//\________________ On affiche le fond.
	if(pGauge->pImageBackground != NULL)
	{
		pGaugeImage2 = pGauge->pImageBackground;
		glPushMatrix ();
		glScalef (iWidth, iHeight, 1.);
		cairo_dock_apply_texture (pGaugeImage2->iTexture);
		glPopMatrix ();
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
			g_print (" >>>%s\n", pRenderer->cFormatBuffer);
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
		pGaugeImage2 = pGauge->pImageForeground;
		glPushMatrix ();
		glScalef (iWidth, iHeight, 1.);
		cairo_dock_apply_texture (pGaugeImage2->iTexture);
		glPopMatrix ();
	}
	
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
}
void cairo_dock_render_gauge_opengl2 (Gauge2 *pGauge)
{
	g_return_if_fail (pGauge != NULL && pGauge->pIndicatorList != NULL);
	
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
				glScalef (2./3, 2./3, 1.);
			}
			else if (i == 1)
			{
				glTranslatef (2 * pRenderer->iWidth / 3, 2 * pRenderer->iHeight / 3, 0.);
				glScalef (1./3, 1./3, 1.);
			}
			else if (i = 2)
			{
				glTranslatef (2 * pRenderer->iWidth / 3, 0., 0.);
				glScalef (1./3, 1./3, 1.);
			}
			else if (i = 3)
			{
				glTranslatef (0., 2 * pRenderer->iHeight / 3, 0.);
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
}


  //////////////////////////////////////////////
 /////////////// RELOAD GAUGE /////////////////
//////////////////////////////////////////////
void cairo_dock_reload_gauge2 (cairo_t *pSourceContext, Gauge2 *pGauge, int iWidth, int iHeight)
{
	//g_print ("%s (%dx%d)\n", __func__, iWidth, iHeight);
	g_return_if_fail (pGauge != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	pRenderer->iWidth = iWidth;
	pRenderer->iHeight = iHeight;
	
	if (pGauge->pImageBackground != NULL)
	{
		_cairo_dock_load_gauge_image (pSourceContext, pGauge->pImageBackground, iWidth, iHeight);
	}
	
	if (pGauge->pImageForeground != NULL)
	{
		_cairo_dock_load_gauge_image (pSourceContext, pGauge->pImageForeground, iWidth, iHeight);
	}
	
	GaugeIndicator2 *pGaugeIndicator2;
	GaugeImage2 *pGaugeImage2;
	int i;
	GList *pElement, *pElement2;
	for (pElement = pGauge->pIndicatorList; pElement != NULL; pElement = pElement->next)
	{
		pGaugeIndicator2 = pElement->data;
		for (i = 0; i < pGaugeIndicator2->iNbImages; i ++)
		{
			pGaugeImage2 = &pGaugeIndicator2->pImageList[i];
			_cairo_dock_load_gauge_image (pSourceContext, pGaugeImage2, iWidth, iHeight);
		}
	}
	
	if (g_bUseOpenGL)
	{
		pGaugeImage2 = pGaugeIndicator2->pImageNeedle;
		_cairo_dock_load_gauge_image (pSourceContext, pGaugeImage2, iWidth, iHeight);
	}
}

  ////////////////////////////////////////////
 /////////////// FREE GAUGE /////////////////
////////////////////////////////////////////
static void _cairo_dock_free_gauge_image(GaugeImage2 *pGaugeImage2, gboolean bFree)
{
	if (pGaugeImage2 == NULL)
		return ;
	cd_debug("");

	if(pGaugeImage2->pSvgHandle != NULL)
		rsvg_handle_free (pGaugeImage2->pSvgHandle);
	if(pGaugeImage2->pSurface != NULL)
		cairo_surface_destroy (pGaugeImage2->pSurface);
	if (pGaugeImage2->iTexture != 0)
		glDeleteTextures (1, &pGaugeImage2->iTexture);
	
	if (bFree)
		g_free (pGaugeImage2);
}
static void _cairo_dock_free_gauge_indicator(GaugeIndicator2 *pGaugeIndicator2)
{
	if (pGaugeIndicator2 == NULL)
		return ;
	cd_debug("");
	
	int i;
	for (i = 0; i < pGaugeIndicator2->iNbImages; i ++)
	{
		_cairo_dock_free_gauge_image (&pGaugeIndicator2->pImageList[i], FALSE);
	}
	g_free (pGaugeIndicator2->pImageList);
	
	_cairo_dock_free_gauge_image (pGaugeIndicator2->pImageNeedle, TRUE);
	
	g_free (pGaugeIndicator2);
}
void cairo_dock_free_gauge2 (Gauge2 *pGauge)
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


void cairo_dock_add_watermark_on_gauge2 (cairo_t *pSourceContext, Gauge2 *pGauge, gchar *cImagePath, double fAlpha)
{
	g_return_if_fail (pGauge != NULL && cImagePath != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	cairo_surface_t *pWatermarkSurface = cairo_dock_create_surface_for_icon (cImagePath, pSourceContext, pRenderer->iWidth/2, pRenderer->iHeight/2);
	
	if (pGauge->pImageBackground == NULL)
	{
		pGauge->pImageBackground = g_new0 (GaugeImage2, 1);
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
Gauge2 *cairo_dock_new_gauge (void)
{
	Gauge2 *pGauge = g_new0 (Gauge2, 1);
	pGauge->dataRenderer.interface.new				= cairo_dock_new_gauge;
	pGauge->dataRenderer.interface.load				= cairo_dock_load_gauge2;
	pGauge->dataRenderer.interface.render			= cairo_dock_render_gauge2;
	pGauge->dataRenderer.interface.render_opengl	= cairo_dock_render_gauge_opengl2;
	pGauge->dataRenderer.interface.free				= cairo_dock_free_gauge2;
	return pGauge;
}

  /////////////////////////////////////////////////
 /////////////// LIST OF THEMES  /////////////////
/////////////////////////////////////////////////
/*GHashTable *cairo_dock_list_available_gauges (void)
{
	gchar *cGaugeShareDir = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, CAIRO_DOCK_GAUGES_DIR);
	GHashTable *pGaugeTable = cairo_dock_list_themes (cGaugeShareDir, cGaugeUserDir, CAIRO_DOCK_GAUGES_DIR);
	
	g_free (cGaugeShareDir);
	g_free (cGaugeUserDir);
	return pGaugeTable;
}

const gchar *cairo_dock_get_gauge_key_value(gchar *cAppletConfFilePath, GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName)
{
	gchar *cChosenThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	gchar *cGaugeShareDir = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugePath = cairo_dock_get_theme_path (cChosenThemeName, cGaugeShareDir, cGaugeUserDir, CAIRO_DOCK_GAUGES_DIR);
	g_free (cGaugeShareDir);
	g_free (cGaugeUserDir);
	
	cd_debug ("Theme de la jauge : %s", cGaugePath);
	return cGaugePath;
}
*/