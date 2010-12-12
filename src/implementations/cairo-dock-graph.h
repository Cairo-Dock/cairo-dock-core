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


#ifndef __CAIRO_DOCK_GRAPH2__
#define  __CAIRO_DOCK_GRAPH2__

#include <gtk/gtk.h>
G_BEGIN_DECLS

#include "cairo-dock-struct.h"
#include "cairo-dock-data-renderer-manager.h"

/**
*@file cairo-dock-graph.h This class defines the Graph, which derives from the DataRenderer.
* All you need to know is the attributes that define a Graph, the API to use is the common API for DataRenderer, defined in cairo-dock-data-renderer.h.
*/

/// Types of graph.
typedef enum {
	/// a continuous line.
	CAIRO_DOCK_GRAPH2_LINE=0,
	/// a continuous plain graph.
	CAIRO_DOCK_GRAPH2_PLAIN,
	/// a histogram.
	CAIRO_DOCK_GRAPH2_BAR,
	/// a circle.
	CAIRO_DOCK_GRAPH2_CIRCLE,
	/// a plain circle.
	CAIRO_DOCK_GRAPH2_CIRCLE_PLAIN
	} CairoDockTypeGraph;

typedef struct _CairoGraphAttribute CairoGraphAttribute;
/// Attributes of a Graph.
struct _CairoGraphAttribute {
	/// General attributes of any DataRenderer.
	CairoDataRendererAttribute rendererAttribute;
	/// type of graph
	CairoDockTypeGraph iType;
	/// color of the high values. it's a table of nb_values triplets, each of them representing an rgb color.
	gdouble *fHighColor;
	/// color of the low values. same as fHighColor.
	gdouble *fLowColor;
	/// color of the background.
	gdouble fBackGroundColor[4];
	/// radius of the corners of the background.
	gint iRadius;
	/// TRUE to draw all the values on the same graph.
	gboolean bMixGraphs;
};

typedef struct _CairoDockGraph {
	CairoDataRenderer dataRenderer;
	CairoDockTypeGraph iType;
	gdouble *fHighColor;  // iNbValues triplets (r,v,b).
	gdouble *fLowColor;  // idem.
	cairo_pattern_t **pGradationPatterns;  // iNbValues patterns.
	gdouble fBackGroundColor[4];
	cairo_surface_t *pBackgroundSurface;
	GLuint iBackgroundTexture;
	gint iRadius;
	gdouble fMargin;
	gboolean bMixGraphs;
	} CairoDockGraph;


void cairo_dock_register_data_renderer_graph (void);


G_END_DECLS
#endif
