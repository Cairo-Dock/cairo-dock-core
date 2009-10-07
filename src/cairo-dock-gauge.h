/*
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


#ifndef __CAIRO_DOCK_GAUGE2__
#define  __CAIRO_DOCK_GAUGE2__

#include "cairo-dock-struct.h"
#include <cairo-dock-data-renderer.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
G_BEGIN_DECLS

/**
*@file cairo-dock-gauge.h This class defines the Gauge, which derives from the DataRenderer.
* All you need to know is the attributes that define a Gauge, the API to use is the common API for DataRenderer, i ncairo-dock-data-renderer.h.
*/ 

typedef struct {
	RsvgHandle *pSvgHandle;
	cairo_surface_t *pSurface;
	gint sizeX;
	gint sizeY;
	GLuint iTexture;
} GaugeImage;

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
	// text zone
	gdouble textX, textY;
	gdouble textWidth, textHeight;
	gdouble textColor[3];
} GaugeIndicator;

typedef struct {
	CairoDataRenderer dataRenderer;
	gchar *cThemeName;
	GaugeImage *pImageBackground;
	GaugeImage *pImageForeground;
	GList *pIndicatorList;
} Gauge;

/// Attributes of a Gauge.
typedef struct _CairoGaugeAttribute CairoGaugeAttribute;
struct _CairoGaugeAttribute {
	/// General attributes of any DataRenderer.
	CairoDataRendererAttribute rendererAttribute;
	/// path to a gauge theme.
	gchar *cThemePath;
};


Gauge *cairo_dock_new_gauge (void);


GHashTable *cairo_dock_list_available_gauges (void);

gchar *cairo_dock_get_gauge_theme_path (const gchar *cThemeName);

gchar *cairo_dock_get_gauge_key_value (gchar *cAppletConfFilePath, GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName);


G_END_DECLS
#endif
