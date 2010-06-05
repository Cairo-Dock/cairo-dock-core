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


#ifndef __CAIRO_DOCK_DATA_RENDERER__
#define  __CAIRO_DOCK_DATA_RENDERER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-themes-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-data-renderer.h This class defines the Data Renderer structure and API.
* A CairoDataRenderer is a generic way to render onto an icon a set of values defined by : {yk = f(tk)}, k=0..n where yk is a vector.
* Data Renderers are binded to an icon, and initialized with a set of attributes, that derives from a CairoDataRendererAttribute, with the function /ref cairo_dock_add_new_data_renderer_on_icon.
* You update a Data Renderer with /ref cairo_dock_render_new_data_on_icon, providing it the correct number of values.
* If you want to change any parameter of a Data Renderer, use /ref cairo_dock_reload_data_renderer_on_icon, which keeps the history.
* To remove the Data Renderer from an icon, use /ref cairo_dock_remove_data_renderer_on_icon.
*/

//
// Structures
//

struct _CairoDataToRenderer {
	gint iNbValues;
	gint iMemorySize;
	gdouble *pValuesBuffer;
	gdouble **pTabValues;
	gdouble *pMinMaxValues;
	gint iCurrentIndex;
};

#define CAIRO_DOCK_DATA_FORMAT_MAX_LEN 20
/// Prototype of a function used to format the values in a short readable format (to be displayed as quick-info).
typedef void (*CairoDockGetValueFormatFunc) (double fValue, int iNumValue, gchar *cFormatBuffer, int iBufferLength, gpointer data);
/// Generic DataRenderer attributes structure. The attributes of any implementation of a DataRenderer will derive from this class.
struct _CairoDataRendererAttribute {
	const gchar *cModelName;
	gint iNbValues;
	gint iMemorySize;
	gdouble *pMinMaxValues;
	gboolean bUpdateMinMax;
	gboolean bWriteValues;
	gint iLatencyTime;
	CairoDockGetValueFormatFunc format_value;
	gpointer pFormatData;
	gchar **cEmblems;
	gchar **cTitles;
	gdouble fTextColor[3];
};

typedef CairoDataRenderer * (*CairoDataRendererNewFunc) (void);
typedef void (*CairoDataRendererLoadFunc) (CairoDataRenderer *pDataRenderer, CairoContainer *pContainer, CairoDataRendererAttribute *pAttribute);
typedef void (*CairoDataRendererRenderFunc) (CairoDataRenderer *pDataRenderer, cairo_t *pCairoContext);
typedef void (*CairoDataRendererRenderOpenGLFunc) (CairoDataRenderer *pDataRenderer);
typedef void (*CairoDataRendererReloadFunc) (CairoDataRenderer *pDataRenderer);
typedef void (*CairoDataRendererFreeFunc) (CairoDataRenderer *pDataRenderer);
/// Interface of a DataRenderer.
struct _CairoDataRendererInterface {
	CairoDataRendererNewFunc new;
	CairoDataRendererLoadFunc load;
	CairoDataRendererRenderFunc render;
	CairoDataRendererRenderOpenGLFunc render_opengl;
	CairoDataRendererReloadFunc reload;
	CairoDataRendererFreeFunc free;
};


struct _CairoDataRendererEmblem {
	gdouble fX, fY;
	gdouble fWidth, fHeight;
	gdouble fAlpha;
	cairo_surface_t *pSurface;
	GLuint iTexture;
	};

struct _CairoDataRendererTextZone {
	gdouble fX, fY;
	gdouble fWidth, fHeight;
	gdouble pColor[3];
	};


/// Generic DataRenderer. Any implementation of a DataRenderer will derive from this class.
struct _CairoDataRenderer {
	// fill at init by the high level renderer.
	/// interface of the Data Renderer.
	CairoDataRendererInterface interface;
	// fill at load time independantly of the renderer type.
	/// internal data to be drawn by the renderer.it
	CairoDataToRenderer data;
	/// size of the drawing area.
	gint iWidth, iHeight;  // taille du contexte de dessin.
	/// specific function to format the values as text.
	CairoDockGetValueFormatFunc format_value;
	/// buffer for the text.
	gchar cFormatBuffer[CAIRO_DOCK_DATA_FORMAT_MAX_LEN+1];
	/// data passed to the format fonction.
	gpointer pFormatData;
	/// TRUE <=> the Data Renderer should dynamically update the range of the values.
	gboolean bUpdateMinMax;
	/// TRUE <=> the Data Renderer should write the values as text itself.
	gboolean bWriteValues;
	/// the time it will take to update to the new value, with a smooth animation (require openGL capacity)
	gint iLatencyTime;
	// fill at load time by the high level renderer.
	/// the rank of the renderer, eg the number of values it can display at once (for exemple, 1 for a bar, 2 for a dual-gauge)
	gint iRank;  // nbre de valeurs que peut afficher 1 unite (en general : gauge:1/2, graph:1/2, bar:1)
	/// set to TRUE <=> the renderer can draw the values as text itself.
	gboolean bCanRenderValueAsText;
	// dynamic.
	/// the animation counter for the smooth movement.
	gint iSmoothAnimationStep;
	/// latency due to the smooth movement (0 means the displayed value is the current one, 1 the previous)
	gdouble fLatency;
	/// an optionnal list of tiltes to be displayed on the Data Renderer next to each value. Same size as the set of values.
	gchar **cTitles;
	/// color of the titles.
	gdouble fTextColor[3];
	/// an optionnal list of emblems to be displayed on the Data Renderer next to each value. Same size as the set of values.
	gchar **cEmblems;
	/// an optionnal list of emblems to be displayed on the Data Renderer to indicate the nature of each value. Same size as the set of values.
	CairoDataRendererEmblem *pEmblems;
	/// an optionnal list of titles to be displayed on the Data Renderer to indicate the nature of each value. Same size as the set of values.
	CairoDataRendererTextZone *pTextZones;
};


///
/// Renderer manipulation
///
/** Get the default GLX font for Data Renderer. It can render strings of digits from 0 to 9. Don't destroy it.
*@return the default GLX font*/
CairoDockGLFont *cairo_dock_get_default_data_renderer_font (void);

void cairo_dock_unload_default_data_renderer_font (void);


/**Add a Data Renderer on an icon (usually the icon of an applet). A Data Renderer is a view that will be used to display a set a values on the icon.
*@param pIcon the icon
*@param pContainer the icon's container
*@param pAttribute attributes defining the Renderer*/
void cairo_dock_add_new_data_renderer_on_icon (Icon *pIcon, CairoContainer *pContainer, CairoDataRendererAttribute *pAttribute);

/**Draw the current values associated with the Renderer on the icon.
*@param pIcon the icon
*@param pContainer the icon's container
*@param pCairoContext a drawing context on the icon
*@param pNewValues a set a new values (must be of the size defined on the creation of the Renderer)*/
void cairo_dock_render_new_data_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pCairoContext, double *pNewValues);

/**Remove the Data Renderer of an icon. All the allocated ressources will be freed.
*@param pIcon the icon*/
void cairo_dock_remove_data_renderer_on_icon (Icon *pIcon);

/**Reload the Data Renderer of an icon. If no attributes are provided, it simply reload it with its current attributes. History is kept.
*@param pIcon the icon
*@param pContainer the icon's container
*@param pAttribute new attributes defining the Renderer, or NULL to keep the current ones*/
void cairo_dock_reload_data_renderer_on_icon (Icon *pIcon, CairoContainer *pContainer, CairoDataRendererAttribute *pAttribute);


/** Resize the history of a DataRenderer of an icon, that is to say change the number of previous values that are remembered by the DataRenderer.
*@param pIcon the icon
*@param iNewMemorySize the new size of history*/
void cairo_dock_resize_data_renderer_history (Icon *pIcon, int iNewMemorySize);

/** Redraw the DataRenderer of an icon, with the current values.
*@param pIcon the icon
*@param pContainer the icon's container
*@param pCairoContext a drawing context on the icon*/
void cairo_dock_refresh_data_renderer (Icon *pIcon, CairoContainer *pContainer, cairo_t *pCairoContext);


///
/// Structure Access
///
/**Get the elementary part of a Data Renderer
*@param r a high level data renderer
*@return a CairoDataRenderer* */
#define CAIRO_DATA_RENDERER(r) (&(r)->dataRenderer)
/**Get the data of a Data Renderer
*@param pRenderer a data renderer
*@return a CairoDataToRenderer* */
#define cairo_data_renderer_get_data(pRenderer) (&(pRenderer)->data);
/**Get the elementary part of a Data Renderer Attribute
*@param pAttr a high level data renderer attribute
*@return a CairoDataRendererAttribute* */
#define CAIRO_DATA_RENDERER_ATTRIBUTE(pAttr) ((CairoDataRendererAttribute *) pAttr)


/*#define cairo_data_renderer_set_attribute(pRendererAttribute, cAttributeName, ) g_datalist_get_data (pRendererAttribute->pExtraProperties)
#define cairo_data_renderer_get_attribute(pRendererAttribute, cAttributeName) g_datalist_get_data (pRendererAttribute->pExtraProperties)*/

///
/// Data Access
///
/**Get the lower range of the i-th value.
*@param pRenderer a data renderer
*@param i the number of the value
*@return a double*/
#define cairo_data_renderer_get_min_value(pRenderer, i) (pRenderer)->data.pMinMaxValues[2*i]
/**Get the upper range of the i-th value.
*@param pRenderer a data renderer
*@param i the number of the value
*@return a double*/
#define cairo_data_renderer_get_max_value(pRenderer, i) (pRenderer)->data.pMinMaxValues[2*i+1]
/**Get the i-th value at the time t.
*@param pRenderer a data renderer
*@param i the number of the value
*@param t the time (in number of steps)
*@return a double*/
#define cairo_data_renderer_get_value(pRenderer, i, t) pRenderer->data.pTabValues[pRenderer->data.iCurrentIndex+t > pRenderer->data.iMemorySize ? pRenderer->data.iCurrentIndex+t-pRenderer->data.iMemorySize : pRenderer->data.iCurrentIndex+t < 0 ? pRenderer->data.iCurrentIndex+t+pRenderer->data.iMemorySize : pRenderer->data.iCurrentIndex+t][i]
/**Get the current i-th value.
*@param pRenderer a data renderer
*@param i the number of the value
*@return a double*/
#define cairo_data_renderer_get_current_value(pRenderer, i) pRenderer->data.pTabValues[pRenderer->data.iCurrentIndex][i]
/**Get the previous i-th value.
*@param pRenderer a data renderer
*@param i the number of the value
*@return a double*/
#define cairo_data_renderer_get_previous_value(pRenderer, i) cairo_data_renderer_get_value (pRenderer, i, -1)
/**Get the normalized i-th value (between 0 and 1) at the time t.
*@param pRenderer a data renderer
*@param i the number of the value
*@param t the time (in number of steps)
*@return a double in [0,1]*/
#define cairo_data_renderer_get_normalized_value(pRenderer, i, t) MAX (0, MIN (1, (cairo_data_renderer_get_value (pRenderer, i, t) - cairo_data_renderer_get_min_value (pRenderer, i)) / (cairo_data_renderer_get_max_value (pRenderer, i) - cairo_data_renderer_get_min_value (pRenderer, i))))
/**Get the normalized current i-th value (between 0 and 1).
*@param pRenderer a data renderer
*@param i the number of the value
*@return a double in [0,1]*/
#define cairo_data_renderer_get_normalized_current_value(pRenderer, i) cairo_data_renderer_get_normalized_value(pRenderer, i, 0)
/**Get the normalized previous i-th value (between 0 and 1).
*@param pRenderer a data renderer
*@param i the number of the value
*@return a double in [0,1]*/
#define cairo_data_renderer_get_normalized_previous_value(pRenderer, i) cairo_data_renderer_get_normalized_value(pRenderer, i, -1)
/**Get the normalized current i-th value (between 0 and 1), taking into account the latency of the smooth movement.
*@param pRenderer a data renderer
*@param i the number of the value
*@return a double in [0,1]*/
#define cairo_data_renderer_get_normalized_current_value_with_latency(pRenderer, i) (pRenderer->fLatency == 0 ? cairo_data_renderer_get_normalized_current_value (pRenderer, i) : cairo_data_renderer_get_normalized_current_value (pRenderer, i) * (1 - pRenderer->fLatency) + cairo_data_renderer_get_normalized_previous_value (pRenderer, i) * pRenderer->fLatency);

///
/// Data Format
///
/**Write a value in a readable text format.
*@param pRenderer a data renderer
*@param fValue the normalized value
*@param i the number of the value
*@param cBuffer a buffer where to write*/
#define cairo_data_renderer_format_value_full(pRenderer, fValue, i, cBuffer) do {\
	if (pRenderer->format_value != NULL)\
		(pRenderer)->format_value (fValue, i, cBuffer, CAIRO_DOCK_DATA_FORMAT_MAX_LEN, (pRenderer)->pFormatData);\
	else\
		snprintf (cBuffer, CAIRO_DOCK_DATA_FORMAT_MAX_LEN, fValue < .0995 ? "%.1f" : (fValue < 1 ? " %.0f" : "%.0f"), fValue * 100.); } while (0)
/**Write a value in a readable text format in the renderer text buffer.
*@param pRenderer a data renderer
*@param fValue the normalized value
*@param i the number of the value*/
#define cairo_data_renderer_format_value(pRenderer, fValue, i) cairo_data_renderer_format_value_full (pRenderer, fValue, i, (pRenderer)->cFormatBuffer)

#define cairo_data_renderer_can_write_value(pRenderer) (pRenderer)->bCanRenderValueAsText


GHashTable *cairo_dock_list_available_themes_for_data_renderer (const gchar *cRendererName);

gchar *cairo_dock_get_data_renderer_theme_path (const gchar *cRendererName, const gchar *cThemeName, CairoDockThemeType iType);

gchar *cairo_dock_get_theme_path_for_data_renderer (const gchar *cRendererName, const gchar *cAppletConfFilePath, GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultThemeName);


void cairo_dock_register_built_in_data_renderers (void);


G_END_DECLS
#endif
