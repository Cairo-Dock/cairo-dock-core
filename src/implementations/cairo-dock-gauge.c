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

/** Exemple of a gauge with text inside :
 * 
 * <gauge>
	<name>Screenlets Vista'ish Default</name>
	<author>(conversion by Nochka85)</author>
	<rank>2</rank>
	<file key="background">back.svg</file>
	<file key="foreground">dialdot.svg</file>
	<indicator>
		<posX>-0,35</posX>
		<posY>-0,10</posY>
		<posStart>-130</posStart>
		<posStop>130</posStop>
		<file key="needle">dial.svg</file>
		<offset_x>0</offset_x>
		<width>40</width>
		<height>8</height>
		<text_zone>
			<x_center>-0,34</x_center>
			<y_center>-0,47</y_center>
			<width>0,18</width>
			<height>0,10</height>
			<red>1.0</red>
			<green>1,0</green>
			<blue>1,0</blue>
		</text_zone>
		<logo_zone>
			<x_center>-0,32</x_center>
			<y_center>0,00</y_center>
			<width>0,10</width>
			<height>0,10</height>
			<alpha>1.0</alpha>
		</logo_zone>
	</indicator>
	<indicator>
		<posX>0,48</posX>
		<posY>0,27</posY>
		<direction>1</direction>
		<posStart>-128</posStart>
		<posStop>128</posStop>
		<file key="needle">dial2.svg</file>
		<offset_x>0</offset_x>
		<width>30</width>
		<height>8</height>
		<text_zone>
			<x_center>0,50</x_center>
			<y_center>-0,00</y_center>
			<width>0,18</width>
			<height>0,08</height>
			<red>1.0</red>
			<green>1,0</green>
			<blue>1,0</blue>
		</text_zone>
		<logo_zone>
			<x_center>0,50</x_center>
			<y_center>0,30</y_center>
			<width>0,10</width>
			<height>0,10</height>
			<alpha>1.0</alpha>
		</logo_zone>
	</indicator>
</gauge>
 * 
 * */

#include <string.h>
#include <math.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "../config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-opengl-font.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-gauge.h"

#define CAIRO_DOCK_GAUGES_DIR "gauges"

extern gchar *g_cExtrasDirPath;
extern gboolean g_bUseOpenGL;
extern gboolean g_bEasterEggs;

  ////////////////////////////////////////////
 /////////////// LOAD GAUGE /////////////////
////////////////////////////////////////////
/*void cairo_dock_xml_open_file (const gchar *filePath, const gchar *mainNodeName,xmlDocPtr *myXmlDoc,xmlNodePtr *myXmlNode)
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
}*/

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
static void _cairo_dock_load_gauge_image (GaugeImage *pGaugeImage, int iWidth, int iHeight)
{
	cd_message ("%s (%dx%d)", __func__, iWidth, iHeight);
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
static void _cairo_dock_load_gauge_needle (GaugeIndicator *pGaugeIndicator, int iWidth, int iHeight)
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
static gboolean _cairo_dock_load_gauge_theme (Gauge *pGauge, const gchar *cThemePath)
{
	cd_debug ("%s (%s)", __func__, cThemePath);
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	if (iWidth == 0 || iHeight == 0)
		return FALSE;
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	
	g_return_val_if_fail (cThemePath != NULL, FALSE);
	xmlInitParser ();
	xmlDocPtr pGaugeTheme;
	xmlNodePtr pGaugeMainNode;
	gchar *cXmlFile = g_strdup_printf("%s/theme.xml",cThemePath);
	//cairo_dock_xml_open_file (cXmlFile, "gauge",&pGaugeTheme,&pGaugeMainNode);
	pGaugeTheme = cairo_dock_open_xml_file (cXmlFile, "gauge", &pGaugeMainNode, NULL);
	g_free (cXmlFile);
	g_return_val_if_fail (pGaugeTheme != NULL && pGaugeMainNode != NULL, FALSE);
	
	xmlAttrPtr ap;
	xmlChar *cAttribute, *cNodeContent, *cTextNodeContent;
	GString *sImagePath = g_string_new ("");
	GaugeImage *pGaugeImage;
	GaugeIndicator *pGaugeIndicator = NULL;
	xmlNodePtr pGaugeNode;
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
			CAIRO_DATA_RENDERER (pGauge)->iRank = atoi (cNodeContent);
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
				_cairo_dock_load_gauge_image (pGauge->pImageBackground, iWidth, iHeight);
			}
			else if (xmlStrcmp (cAttribute, "foreground") == 0)
			{
				pGauge->pImageForeground = _cairo_dock_new_gauge_image (sImagePath->str);
				_cairo_dock_load_gauge_image (pGauge->pImageForeground, iWidth, iHeight);
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
					pGaugeIndicator->posX = _str2double (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "posY") == 0)
					pGaugeIndicator->posY = _str2double (cNodeContent);
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "text_zone") == 0)
				{
					xmlNodePtr pTextSubNode;
					for (pTextSubNode = pGaugeSubNode->children; pTextSubNode != NULL; pTextSubNode = pTextSubNode->next)
					{
						cTextNodeContent = xmlNodeGetContent (pTextSubNode);
						if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "x_center") == 0)
							pGaugeIndicator->textZone.fX = _str2double (cTextNodeContent);
						else if(xmlStrcmp (pTextSubNode->name, (const xmlChar *) "y_center") == 0)
							pGaugeIndicator->textZone.fY = _str2double (cTextNodeContent);
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
					}
				}
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "logo_zone") == 0)
				{
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
				else if(xmlStrcmp (pGaugeSubNode->name, (const xmlChar *) "file") == 0)
				{
					cd_debug("gauge : On charge un fichier (%s)",cNodeContent);
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
								_cairo_dock_load_gauge_image (&pGaugeIndicator->pImageList[pGaugeIndicator->iNbImageLoaded], iWidth, iHeight);
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
					_cairo_dock_load_gauge_needle (pGaugeIndicator, iWidth, iHeight);
			}
			pGauge->pIndicatorList = g_list_append (pGauge->pIndicatorList, pGaugeIndicator);
		}
	}
	cairo_dock_close_xml_file (pGaugeTheme);
	g_string_free (sImagePath, TRUE);
	
	g_return_val_if_fail (CAIRO_DATA_RENDERER (pGauge)->iRank != 0 && pGaugeIndicator != NULL, FALSE);
	CAIRO_DATA_RENDERER (pGauge)->bCanRenderValueAsText = (pGaugeIndicator->textZone.fWidth != 0 && pGaugeIndicator->textZone.fHeight != 0);
	
	return TRUE;
}
static void cairo_dock_load_gauge (Gauge *pGauge, CairoContainer *pContainer, CairoGaugeAttribute *pAttribute)
{
	_cairo_dock_load_gauge_theme (pGauge, pAttribute->cThemePath);
	
	/// charger les emblemes...
	
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
		_cairo_dock_load_gauge_image (pGaugeImage, iWidth, iHeight);
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
		
		if (/**pRenderer->bWriteValues && */pIndicator->textZone.fWidth != 0 && pIndicator->textZone.fHeight != 0)  // cet indicateur a un emplacement pour le texte de la valeur.
		{
			cairo_data_renderer_format_value (pRenderer, fValue, i);
			//g_print (" >>>%s\n", pRenderer->cFormatBuffer);
			cairo_save (pCairoContext);
			cairo_set_source_rgb (pCairoContext, pIndicator->textZone.pColor[0], pIndicator->textZone.pColor[1], pIndicator->textZone.pColor[2]);
			
			PangoLayout *pLayout = pango_cairo_create_layout (pCairoContext);
			PangoFontDescription *fd = pango_font_description_from_string ("Monospace 12");
			pango_layout_set_font_description (pLayout, fd);
			
			PangoRectangle ink, log;
			pango_layout_set_text (pLayout, pRenderer->cFormatBuffer, -1);
			pango_layout_get_pixel_extents (pLayout, &ink, &log);
			double fZoom = MIN (pIndicator->textZone.fWidth * pRenderer->iWidth / (log.width), pIndicator->textZone.fHeight * pRenderer->iHeight / log.height);
			
			cairo_move_to (pCairoContext,
				floor ((1. + pIndicator->textZone.fX) * pRenderer->iWidth/2 - log.width*fZoom/2),
				floor ((1. - pIndicator->textZone.fY) * pRenderer->iHeight/2 - log.height*fZoom/2));
			cairo_scale (pCairoContext,
				fZoom,
				fZoom);
			pango_cairo_show_layout (pCairoContext, pLayout);
			cairo_restore (pCairoContext);
		}
	}
	
	//\________________ On affiche l'avant-plan.
	if(pGauge->pImageForeground != NULL)
	{
		pGaugeImage = pGauge->pImageForeground;
		cairo_set_source_surface (pCairoContext, pGaugeImage->pSurface, 0.0f, 0.0f);
		cairo_paint (pCairoContext);
	}
}
void cairo_dock_render_gauge (Gauge *pGauge, cairo_t *pCairoContext)
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
static void _draw_gauge_needle_opengl (Gauge *pGauge, GaugeIndicator *pGaugeIndicator, double fValue)
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
	GaugeIndicator *pIndicator;
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
		
		if (/**pRenderer->bWriteValues && */pIndicator->textZone.fWidth != 0 && pIndicator->textZone.fHeight != 0)  // cet indicateur a un emplacement pour le texte de la valeur.
		{
			cairo_data_renderer_format_value (pRenderer, fValue, i);
			
			CairoDockGLFont *pFont = cairo_dock_get_default_data_renderer_font ();
			glColor3f (pIndicator->textZone.pColor[0], pIndicator->textZone.pColor[1], pIndicator->textZone.pColor[2]);
			glPushMatrix ();
			
			cairo_dock_draw_gl_text_at_position_in_area (pRenderer->cFormatBuffer,
				pFont,
				floor (pIndicator->textZone.fX * pRenderer->iWidth/2),
				floor (pIndicator->textZone.fY * pRenderer->iHeight/2),
				pIndicator->textZone.fWidth * pRenderer->iWidth,
				pIndicator->textZone.fHeight * pRenderer->iHeight,
				TRUE);
			
			glPopMatrix ();
			_cairo_dock_enable_texture ();
			glColor3f (1.0, 1.0, 1.0);
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
static void cairo_dock_render_gauge_opengl (Gauge *pGauge)
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
static void cairo_dock_reload_gauge (Gauge *pGauge)
{
	//g_print ("%s (%dx%d)\n", __func__, iWidth, iHeight);
	g_return_if_fail (pGauge != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	int iWidth = pGauge->dataRenderer.iWidth, iHeight = pGauge->dataRenderer.iHeight;
	if (pGauge->pImageBackground != NULL)
	{
		_cairo_dock_load_gauge_image (pGauge->pImageBackground, iWidth, iHeight);
	}
	
	if (pGauge->pImageForeground != NULL)
	{
		_cairo_dock_load_gauge_image (pGauge->pImageForeground, iWidth, iHeight);
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
			_cairo_dock_load_gauge_image (pGaugeImage, iWidth, iHeight);
		}
		if (g_bUseOpenGL && pGaugeIndicator->pImageNeedle)
		{
			_cairo_dock_load_gauge_needle (pGaugeIndicator, iWidth, iHeight);
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
static void cairo_dock_free_gauge (Gauge *pGauge)
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


  //////////////////////////////////////////
 /////////////// RENDERER /////////////////
//////////////////////////////////////////
Gauge *cairo_dock_new_gauge (void)
{
	Gauge *pGauge = g_new0 (Gauge, 1);
	pGauge->dataRenderer.interface.new				= (CairoDataRendererNewFunc) cairo_dock_new_gauge;
	pGauge->dataRenderer.interface.load				= (CairoDataRendererLoadFunc) cairo_dock_load_gauge;
	pGauge->dataRenderer.interface.render			= (CairoDataRendererRenderFunc) cairo_dock_render_gauge;
	pGauge->dataRenderer.interface.render_opengl		= (CairoDataRendererRenderOpenGLFunc) cairo_dock_render_gauge_opengl;
	pGauge->dataRenderer.interface.reload			= (CairoDataRendererReloadFunc) cairo_dock_reload_gauge;
	pGauge->dataRenderer.interface.free				= (CairoDataRendererFreeFunc) cairo_dock_free_gauge;
	return pGauge;
}


  /////////////////////////////////////////////////
 /////////////// LIST OF THEMES  /////////////////
/////////////////////////////////////////////////
GHashTable *cairo_dock_list_available_gauges (void)
{
	gchar *cGaugeShareDir = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s", g_cExtrasDirPath, CAIRO_DOCK_GAUGES_DIR);
	GHashTable *pGaugeTable = cairo_dock_list_themes (cGaugeShareDir, cGaugeUserDir, CAIRO_DOCK_GAUGES_DIR);
	
	g_free (cGaugeShareDir);
	g_free (cGaugeUserDir);
	return pGaugeTable;
}


gchar *cairo_dock_get_gauge_theme_path (const gchar *cThemeName, CairoDockThemeType iType)  // utile pour DBus aussi.
{
	const gchar *cGaugeShareDir = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_GAUGES_DIR;
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s", g_cExtrasDirPath, CAIRO_DOCK_GAUGES_DIR);
	gchar *cGaugePath = cairo_dock_get_theme_path (cThemeName, cGaugeShareDir, cGaugeUserDir, CAIRO_DOCK_GAUGES_DIR, iType);
	g_free (cGaugeUserDir);
	return cGaugePath;
}

gchar *cairo_dock_get_theme_path_for_gauge (const gchar *cAppletConfFilePath, GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultThemeName)
{
	gchar *cChosenThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	if (cChosenThemeName == NULL)
		cChosenThemeName = g_strdup ("Turbo-night-fuel");
	
	CairoDockThemeType iType = cairo_dock_extract_theme_type_from_name (cChosenThemeName);
	gchar *cGaugePath = cairo_dock_get_gauge_theme_path (cChosenThemeName, iType);
	
	if (cGaugePath == NULL)  // theme introuvable.
		cGaugePath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_GAUGES_DIR"/Turbo-night-fuel");
	
	if (iType != CAIRO_DOCK_ANY_THEME)
	{
		g_key_file_set_string (pKeyFile, cGroupName, cKeyName, cChosenThemeName);
		cairo_dock_write_keys_to_file (pKeyFile, cAppletConfFilePath);
	}
	cd_debug ("Theme de la jauge : %s", cGaugePath);
	g_free (cChosenThemeName);
	return cGaugePath;
}


/// deprecated ... to be added in the DataRenderer API.
void cairo_dock_add_watermark_on_gauge (Gauge *pGauge, gchar *cImagePath, double fAlpha)
{
	g_return_if_fail (pGauge != NULL && cImagePath != NULL);
	
	CairoDataRenderer *pRenderer = CAIRO_DATA_RENDERER (pGauge);
	cairo_surface_t *pWatermarkSurface = cairo_dock_create_surface_for_icon (cImagePath, pRenderer->iWidth/2, pRenderer->iHeight/2);
	
	if (pGauge->pImageBackground == NULL)
	{
		pGauge->pImageBackground = g_new0 (GaugeImage, 1);
		pGauge->pImageBackground->sizeX = pRenderer->iWidth;
		pGauge->pImageBackground->sizeY = pRenderer->iHeight;
		
		pGauge->pImageBackground->pSurface = cairo_dock_create_blank_surface (
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
