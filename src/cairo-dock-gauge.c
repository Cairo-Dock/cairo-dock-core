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
#include <cairo-dock-gauge.h>

extern double g_fAmplitude;
extern gchar *g_cCairoDockDataDir;
extern gboolean g_bUseOpenGL;

static GHashTable *s_pGaugeTable = NULL;

void cairo_dock_list_available_gauges (void)
{
	if (s_pGaugeTable != NULL)
		return ;
	
	GError *erreur = NULL;
	gchar *cGaugeShareDir = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_GAUGES_DIR);
	s_pGaugeTable = cairo_dock_list_themes (cGaugeShareDir, s_pGaugeTable, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	g_free (cGaugeShareDir);
	
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s/%s", g_cCairoDockDataDir, CAIRO_DOCK_EXTRAS_DIR, CAIRO_DOCK_GAUGES_DIR);
	s_pGaugeTable = cairo_dock_list_themes (cGaugeUserDir, s_pGaugeTable, NULL);  // le repertoire peut ne pas exister, donc on ignore l'erreur.
	g_free (cGaugeUserDir);
}

void cairo_dock_update_conf_file_with_gauges (GKeyFile *pOpenedKeyFile, gchar *cConfFile, gchar *cGroupName, gchar *cKeyName)
{
	//g_print ("%s (%s, %s, %s)\n", __func__, cConfFile, cGroupName, cKeyName);
	cairo_dock_update_conf_file_with_hash_table (pOpenedKeyFile, cConfFile, s_pGaugeTable, cGroupName, cKeyName, NULL, (GHFunc) cairo_dock_write_one_theme_name, TRUE, FALSE);
}

void cairo_dock_invalidate_gauges_list (void)
{
	if (s_pGaugeTable == NULL)
		return ;
	g_hash_table_destroy (s_pGaugeTable);
	s_pGaugeTable = NULL;
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
	GaugeImage *pGaugeImage = NULL;
	
	xmlInitParser ();
	xmlDocPtr pGaugeTheme;
	xmlNodePtr pGaugeMainNode;
	
	gchar *cXmlFile = g_strdup_printf("%s/theme.xml",cThemePath);
	cairo_dock_xml_open_file(cXmlFile, "gauge",&pGaugeTheme,&pGaugeMainNode);
	g_free (cXmlFile);
	
	if(pGaugeTheme != NULL)
	{
		pGauge = g_new0 (Gauge, 1);
		pGauge->sizeX = iWidth;
		pGauge->sizeY = iHeight;
		
		xmlNodePtr pGaugeNode;
		xmlAttrPtr ap;
		xmlChar *cAttribute, *cNodeContent;
		
		for (pGaugeNode = pGaugeMainNode->xmlChildrenNode; pGaugeNode != NULL; pGaugeNode = pGaugeNode->next)
		{
			if (xmlStrcmp (pGaugeNode->name, (const xmlChar *) "name") == 0)
			{
				pGauge->themeName = xmlNodeGetContent(pGaugeNode);
				cd_debug("gauge : Nom du theme(%s)",pGauge->themeName);
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
				GaugeIndicator *pGaugeIndicator = g_new0 (GaugeIndicator, 1);
				pGaugeIndicator->direction = 1;
				
				cd_debug ("gauge : On charge un indicateur");
				xmlNodePtr pGaugeSubNode;
				for (pGaugeSubNode = pGaugeNode->xmlChildrenNode; pGaugeSubNode != NULL; pGaugeSubNode = pGaugeSubNode->next)
				{
					cNodeContent = xmlNodeGetContent (pGaugeSubNode);
					if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posX") == 0)
						pGaugeIndicator->posX = atof (cNodeContent);
					else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "direction") == 0)
						pGaugeIndicator->direction = atof (cNodeContent);
					else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posY") == 0)
						pGaugeIndicator->posY = atof (cNodeContent);
					else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStart") == 0)
						pGaugeIndicator->posStart = atof (cNodeContent);
					else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posStop") == 0)
						pGaugeIndicator->posStop = atof (cNodeContent);
					else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "file") == 0)
					{
						cd_debug("gauge : On charge un fichier(%s)",cNodeContent);
						ap = xmlHasProp(pGaugeSubNode, "key");
						cAttribute = xmlNodeGetContent(ap->xmlChildrenNode);
						if (strcmp (cAttribute, "needle") == 0)
						{
							g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
							pGaugeImage = cairo_dock_init_gauge_image(sImagePath->str);
							
							pGaugeIndicator->imageNeedle = g_list_append (pGaugeIndicator->imageNeedle, pGaugeImage);
						}
						else if (strcmp (cAttribute,"image") == 0)
						{
							g_string_printf (sImagePath, "%s/%s", cThemePath, cNodeContent);
							pGaugeImage = cairo_dock_init_gauge_image(sImagePath->str);
							cairo_dock_load_gauge_image(pSourceContext, pGaugeImage, iWidth, iHeight);
							
							pGaugeIndicator->nbImage++;
							
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
	}
	
	xmlCleanupParser ();
	
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
	//g_print ("%s (%x; %x)\n", __func__, pSourceContext, pGauge);
	g_return_if_fail (pGauge != NULL && pGauge->indicatorList != NULL && pContainer != NULL && pSourceContext != NULL);
	g_return_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS);
	//cd_debug("gauge : %s ()",__func__);
	
	GaugeImage *pGaugeImage;
	
	cairo_save (pSourceContext);
	cairo_set_source_rgba (pSourceContext, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (pSourceContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pSourceContext);
	cairo_set_operator (pSourceContext, CAIRO_OPERATOR_OVER);
	
	// On affiche le fond
	if(pGauge->imageBackground != NULL)
	{
		pGaugeImage = pGauge->imageBackground;
		cairo_set_source_surface (pSourceContext, pGaugeImage->cairoSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
	
	// On represente l'indicateur de chaque valeur.
	GList *pIndicatorElement;
	GList *pValueList;
	double *pValue;
	pIndicatorElement = pGauge->indicatorList;
	for(pValueList = valueList; pValueList != NULL; pValueList = pValueList->next)
	{
		pValue = pValueList->data;
		if(*pValue > 1) *pValue = 1;
		else if(*pValue < 0) *pValue = 0;
		
		draw_cd_Gauge_image(pSourceContext, pIndicatorElement->data, *pValue);
		
		if(pIndicatorElement->next != NULL) pIndicatorElement = pIndicatorElement->next;
	}
	
	// On represente chaque valeur par son aiguille.
	pIndicatorElement = pGauge->indicatorList;
	for(pValueList = valueList; pValueList != NULL; pValueList = pValueList->next)
	{
		pValue = pValueList->data;
		if(*pValue > 1) *pValue = 1;
		else if(*pValue < 0) *pValue = 0;
		
		draw_cd_Gauge_needle(pSourceContext, pGauge, pIndicatorElement->data, *pValue);
		
		if(pIndicatorElement->next != NULL) pIndicatorElement = pIndicatorElement->next;
	}
	
	// On affiche le fond
	if(pGauge->imageForeground != NULL)
	{
		pGaugeImage = pGauge->imageForeground;
		cairo_set_source_surface (pSourceContext, pGaugeImage->cairoSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
	cairo_restore (pSourceContext);
	
	// On cree le reflet.
	if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->bUseReflect)
	{
		double fMaxScale = cairo_dock_get_max_scale (pContainer);
		
		cairo_surface_destroy (pIcon->pReflectionBuffer);  // pour aller plus vite on decide de ne creer que le minimum.
		pIcon->pReflectionBuffer = NULL;
		if (pIcon->pFullIconBuffer != NULL)
		{
			cairo_surface_destroy (pIcon->pFullIconBuffer);
			pIcon->pFullIconBuffer = NULL;
		}
		
		pIcon->pReflectionBuffer = cairo_dock_create_reflection_surface (pIcon->pIconBuffer,
			pSourceContext,
			(pContainer->bIsHorizontal ? pIcon->fWidth : pIcon->fHeight) * fMaxScale,
			(pContainer->bIsHorizontal ? pIcon->fHeight : pIcon->fWidth) * fMaxScale,
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
		pGaugeImage = pGaugeIndicator->imageNeedle->data;
		
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
	imageWidthZone = 1 / ((double) pGaugeIndicator->nbImage - 1);
	trueImage = imageWidthZone * (pGaugeIndicator->nbImage - 1) * (fValue * (pGaugeIndicator->nbImage - 1) + 0.5);
	//cd_debug("gauge : La bonne image est : %d / %d (%d)",trueImage,pGaugeIndicator->nbImage,imageWidthZone);
	
	pGaugeImage = g_list_nth_data (pGaugeIndicator->imageList, trueImage);
	if (pGaugeImage != NULL)
	{
		cairo_set_source_surface (pSourceContext, pGaugeImage->cairoSurface, 0.0f, 0.0f);
		cairo_paint (pSourceContext);
	}
}
/*void draw_cd_Gauge_image(cairo_t *pSourceContext, Gauge *pGauge, GaugeIndicator *pGaugeIndicator, double fValue)
{
	cd_debug("gauge : %s\n",__func__);
	
	if(pGaugeIndicator->imageList != NULL)
	{
		GaugeImage *pGaugeImage;
		int trueImage;
		double imageWidthZone;
		
		//Equation donnant la bonne image.
		imageWidthZone = 1 / ((double) pGaugeIndicator->nbImage - 1);
		trueImage = imageWidthZone * (pGaugeIndicator->nbImage - 1) * (fValue * (pGaugeIndicator->nbImage - 1) + 0.5);
		cd_debug("gauge : La bonne image est : %d / %d (%d)",trueImage,pGaugeIndicator->nbImage,imageWidthZone);
		
		//On charge l'image correspondante à la valeur
		int i = 0;
		GList *pElement;
		for(pElement = pGaugeIndicator->imageList; pElement != NULL; pElement = pElement->next)
		{
			if(i > trueImage) break;
			else if(i == trueImage)
			{
				cd_debug("gauge : On a trouver l'image %d",i);
				pGaugeImage = pElement->data;
			}
			else pGaugeImage = NULL;
			i++;
		}
		
		if(pGaugeImage != NULL)
		{
			cairo_set_source_surface (pSourceContext, pGaugeImage->cairoSurface, 0.0f, 0.0f);
		}
	}
}*/


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
	
	for (pElement = pGaugeIndicator->imageNeedle; pElement != NULL; pElement = pElement->next)
	{
		cairo_dock_free_gauge_image(pElement->data);
	}
	g_list_free (pGaugeIndicator->imageNeedle);
	
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
	if (s_pGaugeTable == NULL)
		cairo_dock_list_available_gauges ();
	
	const gchar *cGaugePath = NULL;
	gchar *cChosenThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	if (cChosenThemeName != NULL)
		cGaugePath = g_hash_table_lookup (s_pGaugeTable, cChosenThemeName);
	g_free (cChosenThemeName);
	
	cd_debug("Theme de la jauge : %s",cGaugePath);
	cairo_dock_update_conf_file_with_gauges (pKeyFile, cAppletConfFilePath, cGroupName, cKeyName);
	
	return cGaugePath;
	/**gchar *cThemePath = cairo_dock_manage_themes_for_applet (CAIRO_DOCK_SHARE_DATA_DIR, "gauges", cAppletConfFilePath, pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName);
	cd_debug("Clés du theme : [%s] %s",cGroupName,cKeyName);
	cd_debug("Theme de la jauge : %s",cThemePath);
	cd_debug("Dossier des jauges : %s/gauges",CAIRO_DOCK_SHARE_DATA_DIR);
	return cThemePath;*/
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
