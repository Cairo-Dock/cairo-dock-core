/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Necropotame (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <string.h>
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
#include <cairo-dock-gauge.h>

extern gchar *g_cCairoDockDataDir;
extern gboolean g_bUseOpenGL;

GHashTable *cairo_dock_list_available_gauges (void)
{
	gchar *cGaugeShareDir = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, CAIRO_DOCK_GAUGES_DIR);
	GHashTable *pGaugeTable = cairo_dock_list_themes (cGaugeShareDir, cGaugeUserDir, CAIRO_DOCK_GAUGES_DIR);
	
	g_free (cGaugeShareDir);
	g_free (cGaugeUserDir);
	return pGaugeTable;
}



void cairo_dock_xml_open_file (gchar *filePath, const gchar *mainNodeName,xmlDocPtr *myXmlDoc,xmlNodePtr *myXmlNode)
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

Gauge *cairo_dock_load_gauge(cairo_t *pSourceContext, const gchar *cThemePath, int iWidth, int iHeight)
{
	cd_debug ("%s (%dx%d, %s)", __func__, iWidth, iHeight, cThemePath);
	if (iWidth == 0 || iHeight == 0)
		return NULL;
	g_return_val_if_fail (cThemePath != NULL, NULL);
	Gauge *pGauge = NULL;
	
	GString *sImagePath = g_string_new ("");
	GaugeImage *pGaugeImage;
	
	xmlInitParser ();
	xmlDocPtr pGaugeTheme;
	xmlNodePtr pGaugeMainNode;
	
	gchar *cXmlFile = g_strdup_printf("%s/theme.xml",cThemePath);
	cairo_dock_xml_open_file(cXmlFile, "gauge",&pGaugeTheme,&pGaugeMainNode);
	g_free (cXmlFile);
	
	g_return_val_if_fail (pGaugeTheme != NULL, NULL);
	
	pGauge = g_new0 (Gauge, 1);
	pGauge->sizeX = iWidth;
	pGauge->sizeY = iHeight;
	
	xmlNodePtr pGaugeNode;
	xmlAttrPtr ap;
	xmlChar *cAttribute, *cNodeContent, *cTextNodeContent;
	
	for (pGaugeNode = pGaugeMainNode->xmlChildrenNode; pGaugeNode != NULL; pGaugeNode = pGaugeNode->next)
	{
		if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "name") == 0)
		{
			pGauge->themeName = xmlNodeGetContent(pGaugeNode);
			cd_debug("gauge : Nom du theme(%s)",pGauge->themeName);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "rank") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			pGauge->iRank = atoi (cNodeContent);
			xmlFree (cNodeContent);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "file") == 0)
		{
			cNodeContent = xmlNodeGetContent (pGaugeNode);
			g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
			ap = xmlHasProp (pGaugeNode, "key");
			cAttribute = xmlNodeGetContent(ap->xmlChildrenNode);
			if (xmlStrcmp (cAttribute,"background") == 0)
			{
				pGauge->imageBackground = cairo_dock_init_gauge_image (sImagePath->str);
				cairo_dock_load_gauge_image (pSourceContext, pGauge->imageBackground, iWidth, iHeight);
			}
			else if (xmlStrcmp (cAttribute, "foreground") == 0)
			{
				pGauge->imageForeground = cairo_dock_init_gauge_image (sImagePath->str);
				cairo_dock_load_gauge_image (pSourceContext, pGauge->imageForeground, iWidth, iHeight);
			}
			xmlFree (cNodeContent);
			xmlFree (cAttribute);
		}
		else if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "indicator") == 0)
		{
			if (pGauge->iRank == 0)
			{
				pGauge->iRank = 1;
				xmlNodePtr node;
				for (node = pGaugeNode->next; node != NULL; node = node->next)
				{
					if (xmlStrcmp (node->name, (const xmlChar *) "indicator") == 0)
						pGauge->iRank ++;
				}
			}

			GaugeIndicator *pGaugeIndicator = g_new0 (GaugeIndicator, 1);
			pGaugeIndicator->direction = 1;
			
			cd_debug ("gauge : On charge un indicateur");
			xmlNodePtr pGaugeSubNode;
			for (pGaugeSubNode = pGaugeNode->xmlChildrenNode; pGaugeSubNode != NULL; pGaugeSubNode = pGaugeSubNode->next)
			{
				cNodeContent = xmlNodeGetContent (pGaugeSubNode);
				if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posX") == 0)
					pGaugeIndicator->posX = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posY") == 0)
					pGaugeIndicator->posY = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "text") == 0)
				{
					xmlNodePtr pTextSubNode;
					for (pTextSubNode = pGaugeSubNode->xmlChildrenNode; pTextSubNode != NULL; pTextSubNode = pTextSubNode->next)
					{
						cTextNodeContent = xmlNodeGetContent (pTextSubNode);
						if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textX") == 0)
							pGaugeIndicator->textX = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textY") == 0)
							pGaugeIndicator->textY = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textWidth") == 0)
							pGaugeIndicator->textWidth = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textHeight") == 0)
							pGaugeIndicator->textHeight = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textColorR") == 0)
							pGaugeIndicator->textColor[0] = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textColorG") == 0)
							pGaugeIndicator->textColor[1] = atof (cNodeContent);
						else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "textColorB") == 0)
							pGaugeIndicator->textColor[2] = atof (cNodeContent);
					}
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "direction") == 0)
					pGaugeIndicator->direction = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStart") == 0)
					pGaugeIndicator->posStart = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStop") == 0)
					pGaugeIndicator->posStop = atof (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "nb images") == 0)
					pGaugeIndicator->nbImage = atoi (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "file") == 0)
				{
					cd_debug("gauge : On charge un fichier(%s)",cNodeContent);
					ap = xmlHasProp(pGaugeSubNode, "key");
					cAttribute = xmlNodeGetContent(ap->xmlChildrenNode);
					if (strcmp (cAttribute, "needle") == 0 && pGaugeIndicator->imageNeedle == NULL)
					{
						g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
						pGaugeImage = cairo_dock_init_gauge_image(sImagePath->str);
						
						pGaugeIndicator->imageNeedle = pGaugeImage;
					}
					else if (strcmp (cAttribute,"image") == 0)
					{
						if (pGaugeIndicator->nbImage == 0)
						{
							pGaugeIndicator->nbImage = 1;
							xmlNodePtr node;
							for (node = pGaugeSubNode->next; node != NULL; node = node->next)
							{
								if (xmlStrcmp (node->name, (const xmlChar *) "file") == 0)
									pGaugeIndicator->nbImage ++;
							}
						}
						g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
						pGaugeImage = cairo_dock_init_gauge_image(sImagePath->str);
						cairo_dock_load_gauge_image(pSourceContext, pGaugeImage, iWidth, iHeight);
						
						//pGaugeIndicator->nbImage++;
						
						pGaugeIndicator->imageList = g_list_append (pGaugeIndicator->imageList, pGaugeImage);
					}
					xmlFree (cAttribute);
				}
				xmlFree (cNodeContent);
			}
			pGauge->indicatorList = g_list_append(pGauge->indicatorList,pGaugeIndicator);
		}
	}
	xmlFreeDoc (pGaugeTheme);
	
	
	g_return_val_if_fail (pGauge->iRank != 0, NULL);
	pGauge->pCurrentValues = g_new0 (double, pGauge->iRank);
	pGauge->pOldValues = g_new0 (double, pGauge->iRank);

	xmlCleanupParser ();
	g_string_free (sImagePath, TRUE);
	
	return pGauge;
}


GaugeImage *cairo_dock_init_gauge_image (gchar *cImagePath)
{
	cd_debug ("%s (%s)", __func__, cImagePath);
	GaugeImage *pGaugeImage = g_new0 (GaugeImage, 1);
	
	pGaugeImage->svgNeedle = rsvg_handle_new_from_file (cImagePath, NULL);
	
	//On récupère la taille de l'image
	RsvgDimensionData SizeInfo;
	rsvg_handle_get_dimensions (pGaugeImage->svgNeedle, &SizeInfo);
	pGaugeImage->sizeX = SizeInfo.width;
	pGaugeImage->sizeY = SizeInfo.height;
	
	return pGaugeImage;
}

void cairo_dock_load_gauge_image (cairo_t *pSourceContext, GaugeImage *pGaugeImage, int iWidth, int iHeight)
{
	cd_message ("%s (%dx%d)", __func__, iWidth, iHeight);
	if (pGaugeImage->cairoSurface != NULL)
		cairo_surface_destroy (pGaugeImage->cairoSurface);
	
	if (pGaugeImage->svgNeedle != NULL)
	{
		pGaugeImage->cairoSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			iWidth,
			iHeight);
		
		cairo_t* pDrawingContext = cairo_create (pGaugeImage->cairoSurface);
		
		cairo_scale (pDrawingContext,
			(double) iWidth / (double) pGaugeImage->sizeX,
			(double) iHeight / (double) pGaugeImage->sizeY);
		
		rsvg_handle_render_cairo (pGaugeImage->svgNeedle, pDrawingContext);
		cairo_destroy (pDrawingContext);
	}
	else
		pGaugeImage->cairoSurface = NULL;
}



void cairo_dock_render_gauge(cairo_t *pSourceContext, CairoContainer *pContainer, Icon *pIcon, Gauge *pGauge, double fValue)
{
	GList *pList = NULL;
	pList = g_list_prepend (pList, &fValue);
	
	cairo_dock_render_gauge_multi_value(pSourceContext,pContainer,pIcon,pGauge,pList);
	
	g_list_free (pList);
}

void cairo_dock_render_gauge_multi_value(cairo_t *pSourceContext, CairoContainer *pContainer, Icon *pIcon, Gauge *pGauge, GList *valueList)
{
	g_return_if_fail (pGauge != NULL && pGauge->indicatorList != NULL && pContainer != NULL && pSourceContext != NULL);
	g_return_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS);
	
	//\________________ On efface tout.
	cairo_save (pSourceContext);
	cairo_set_source_rgba (pSourceContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pSourceContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pSourceContext);
	cairo_set_operator (pSourceContext, CAIRO_OPERATOR_OVER);
	
	GaugeImage *pGaugeImage;
	//\________________ On affiche le fond.
	if(pGauge->imageBackground != NULL)
	{
		pGaugeImage = pGauge->imageBackground;
		cairo_set_source_surface (pSourceContext, pGaugeImage->cairoSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
	
	//\________________ On represente l'indicateur de chaque valeur.
	GList *pIndicatorElement;
	GList *pValueList;
	double *pValue;
	GaugeIndicator *pIndicator;
	pIndicatorElement = pGauge->indicatorList;
	for (pValueList = valueList; pValueList != NULL && pIndicatorElement != NULL; pValueList = pValueList->next, pIndicatorElement = pIndicatorElement->next)
	{
		pValue = pValueList->data;
		pIndicator = pIndicatorElement->data;
		
		if(*pValue > 1) *pValue = 1;
		else if(*pValue < 0) *pValue = 0;
		
		if (pIndicator->imageNeedle != NULL)  // c'est une aiguille.
		{
			draw_cd_Gauge_needle(pSourceContext, pGauge, pIndicatorElement->data, *pValue);
		}
		else  // c'est une image.
		{
			draw_cd_Gauge_image(pSourceContext, pIndicator, *pValue);
		}

		if (pIndicator->textWidth != 0 && pIndicator->textHeight != 0)  // cet indicateur a un emplacement pour le texte de la valeur.
		{
			gchar *cValueFormat = g_strdup_printf (*pValue < .1 ? "%.1f%%" : "%.0f", *pValue * 100);
			g_print ("%s\n", cValueFormat);
			cairo_save (pSourceContext);
			cairo_set_source_rgb (pSourceContext, pIndicator->textColor[0], pIndicator->textColor[1], pIndicator->textColor[2]);
			cairo_set_line_width (pSourceContext, 20.);
			
			cairo_text_extents_t textExtents;
			cairo_text_extents (pSourceContext, cValueFormat, &textExtents);
			cairo_move_to (pSourceContext,
				pIndicator->textX * pGauge->sizeX - textExtents.width / 2,
				pIndicator->textY * pGauge->sizeY + textExtents.height / 2);
			cairo_show_text (pSourceContext, cValueFormat);
			cairo_restore (pSourceContext);
			g_free (cValueFormat);
		}
	}
	
	//\________________ On affiche le fond
	if(pGauge->imageForeground != NULL)
	{
		pGaugeImage = pGauge->imageForeground;
		cairo_set_source_surface (pSourceContext, pGaugeImage->cairoSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
	
	cairo_restore (pSourceContext);
	
	//\________________ On cree le reflet.
	if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->bUseReflect)
	{
		double fMaxScale = cairo_dock_get_max_scale (pContainer);
		
		cairo_surface_destroy (pIcon->pReflectionBuffer);
		pIcon->pReflectionBuffer = cairo_dock_create_reflection_surface (pIcon->pIconBuffer,
			pSourceContext,
			(pContainer->bIsHorizontal ? pIcon->fWidth : pIcon->fHeight) * fMaxScale / pContainer->fRatio,
			(pContainer->bIsHorizontal ? pIcon->fHeight : pIcon->fWidth) * fMaxScale / pContainer->fRatio,
			pContainer->bIsHorizontal,
			fMaxScale,
			pContainer->bDirectionUp);
	}
	
	if (CAIRO_DOCK_CONTAINER_IS_OPENGL (pContainer))
		cairo_dock_update_icon_texture (pIcon);
	
	cairo_dock_redraw_my_icon (pIcon, pContainer);
}

void draw_cd_Gauge_needle(cairo_t *pSourceContext, Gauge *pGauge, GaugeIndicator *pGaugeIndicator, double fValue)
{
	//cd_debug("gauge : %s\n",__func__);
	
	if(pGaugeIndicator->imageNeedle != NULL)
	{
		GaugeImage *pGaugeImage;
		double fHalfX;
		double fHalfY;
		double physicValue = ((fValue * (pGaugeIndicator->posStop - pGaugeIndicator->posStart)) + pGaugeIndicator->posStart) / 360;
		
		int direction = pGaugeIndicator->direction >= 0 ? 1 : -1;
		//cd_debug("gauge : Direction(%i)",direction);
		
		//On affiche l'aiguille
		pGaugeImage = pGaugeIndicator->imageNeedle;
		
		fHalfX = pGauge->imageBackground->sizeX / 2.0f * (1 + pGaugeIndicator->posX);
		fHalfY = pGauge->imageBackground->sizeY / 2.0f * (1 - pGaugeIndicator->posY);
		
		cairo_save (pSourceContext);
		
		cairo_scale (pSourceContext,
			(double) pGauge->sizeX / (double) pGaugeImage->sizeX,
			(double) pGauge->sizeY / (double) pGaugeImage->sizeY);
		
		cairo_translate (pSourceContext, fHalfX, fHalfY);
		cairo_rotate (pSourceContext, -G_PI/2.0f + G_PI*physicValue*direction*2.0f);
		
		rsvg_handle_render_cairo (pGaugeImage->svgNeedle, pSourceContext);
		
		cairo_restore (pSourceContext);
	}
}

void draw_cd_Gauge_image(cairo_t *pSourceContext, GaugeIndicator *pGaugeIndicator, double fValue)
{
	GaugeImage *pGaugeImage;
	int trueImage;
	double imageWidthZone;
	
	//Equation donnant la bonne image.
	trueImage = fValue * (pGaugeIndicator->nbImage - 1) + 0.5;
	//cd_debug("gauge : La bonne image est : %d / %d (%d)",trueImage,pGaugeIndicator->nbImage,imageWidthZone);
	
	pGaugeImage = g_list_nth_data (pGaugeIndicator->imageList, trueImage);
	if (pGaugeImage != NULL)
	{
		cairo_set_source_surface (pSourceContext, pGaugeImage->cairoSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
}



void cairo_dock_reload_gauge (cairo_t *pSourceContext, Gauge *pGauge, int iWidth, int iHeight)
{
	//g_print ("%s (%dx%d)\n", __func__, iWidth, iHeight);
	g_return_if_fail (pGauge != NULL);
	
	pGauge->sizeX = iWidth;
	pGauge->sizeY = iHeight;
	
	if (pGauge->imageBackground != NULL)
	{
		cairo_dock_load_gauge_image (pSourceContext, pGauge->imageBackground, iWidth, iHeight);
	}
	
	if (pGauge->imageForeground != NULL)
	{
		cairo_dock_load_gauge_image (pSourceContext, pGauge->imageForeground, iWidth, iHeight);
	}
	
	GaugeIndicator *pGaugeIndicator;
	GaugeImage *pGaugeImage;
	GList *pElement, *pElement2;
	for (pElement = pGauge->indicatorList; pElement != NULL; pElement = pElement->next)
	{
		pGaugeIndicator = pElement->data;
		for (pElement2 = pGaugeIndicator->imageList; pElement2 != NULL; pElement2 = pElement2->next)
		{
			pGaugeImage = pElement2->data;
			cairo_dock_load_gauge_image (pSourceContext, pGaugeImage, iWidth, iHeight);  // si c'est une image on la recharge, pour une aiguille SVG c'est inutile.
		}
	}
}



static void cairo_dock_free_gauge_image(GaugeImage *pGaugeImage)
{
	cd_debug("");
	if (pGaugeImage == NULL)
		return ;
	
	if(pGaugeImage->svgNeedle != NULL)
	{
		rsvg_handle_free (pGaugeImage->svgNeedle);
	}
	if(pGaugeImage->cairoSurface != NULL)
	{
		cairo_surface_destroy (pGaugeImage->cairoSurface);
	}
	
	g_free (pGaugeImage);
}

static void cairo_dock_free_gauge_indicator(GaugeIndicator *pGaugeIndicator)
{
	cd_debug("");
	if (pGaugeIndicator == NULL)
		return ;
	
	GList *pElement;
	for (pElement = pGaugeIndicator->imageList; pElement != NULL; pElement = pElement->next)
	{
		cairo_dock_free_gauge_image(pElement->data);
	}
	g_list_free (pGaugeIndicator->imageList);
	
	cairo_dock_free_gauge_image(pGaugeIndicator->imageNeedle);
	
	g_free (pGaugeIndicator);
}

void cairo_dock_free_gauge(Gauge *pGauge)
{
	cd_debug("");
	
	if(pGauge != NULL)
	{
		if(pGauge->themeName != NULL) g_free(pGauge->themeName);
		
		if(pGauge->imageBackground != NULL) cairo_dock_free_gauge_image(pGauge->imageBackground);
		if(pGauge->imageForeground != NULL) cairo_dock_free_gauge_image(pGauge->imageForeground);
		
		GList *pElement;
		for (pElement = pGauge->indicatorList; pElement != NULL; pElement = pElement->next)
		{
			cairo_dock_free_gauge_indicator(pElement->data);
		}
		g_list_free (pGauge->indicatorList);
		
		g_free (pGauge);
	}
}

const gchar *cairo_dock_get_gauge_key_value(gchar *cAppletConfFilePath, GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName)
{
	gchar *cChosenThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	gchar *cGaugeShareDir = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugePath = cairo_dock_get_theme_path (cChosenThemeName, cGaugeShareDir, cGaugeUserDir, CAIRO_DOCK_GAUGES_DIR);
	g_free (cGaugeShareDir);
	g_free (cGaugeUserDir);
	
	/*if (s_pGaugeTable == NULL)
		cairo_dock_list_available_gauges ();
	
	const gchar *cGaugePath = NULL;
	if (cChosenThemeName != NULL)
	{
		CairoDockTheme *pTheme = g_hash_table_lookup (s_pGaugeTable, cChosenThemeName);
		if (pTheme != NULL)
			cGaugePath = pTheme->cThemePath;
		g_free (cChosenThemeName);
	}*/
	
	cd_debug("Theme de la jauge : %s",cGaugePath);
	
	return cGaugePath;
}

void cairo_dock_add_watermark_on_gauge (cairo_t *pSourceContext, Gauge *pGauge, gchar *cImagePath, double fAlpha)
{
	g_return_if_fail (pGauge != NULL && cImagePath != NULL);
	
	cairo_surface_t *pWatermarkSurface = cairo_dock_create_surface_for_icon (cImagePath, pSourceContext, pGauge->sizeX/2, pGauge->sizeY/2);
	
	if (pGauge->imageBackground == NULL)
	{
		pGauge->imageBackground = g_new0 (GaugeImage, 1);
		pGauge->imageBackground->sizeX = pGauge->sizeX;
		pGauge->imageBackground->sizeY = pGauge->sizeY;
		
		pGauge->imageBackground->cairoSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			pGauge->sizeX,
			pGauge->sizeY);
	}
	
	cairo_t *pCairoContext = cairo_create (pGauge->imageBackground->cairoSurface);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
	
	cairo_set_source_surface (pCairoContext, pWatermarkSurface, pGauge->sizeX/4, pGauge->sizeY/4);
	cairo_paint_with_alpha (pCairoContext, fAlpha);
	
	cairo_destroy (pCairoContext);
	
	cairo_surface_destroy (pWatermarkSurface);
}



gboolean cairo_dock_gauge_can_draw_text_value (Gauge *pGauge)
{
	if (pGauge->indicatorList == NULL)
		return FALSE;
	GaugeIndicator *pIndicator = pGauge->indicatorList->data;
	return (pIndicator->textWidth != 0 && pIndicator->textHeight != 0);
}
