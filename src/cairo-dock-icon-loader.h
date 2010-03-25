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


#ifndef __CAIRO_DOCK_ICON_LOADER__
#define  __CAIRO_DOCK_ICON_LOADER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-surface-factory.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-icon-loader.h This class loads the icons and manages the associated ressources.
*/

/* Cree la surface de reflection d'une icone (pour cairo).
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*/
void cairo_dock_add_reflection_to_icon (Icon *pIcon, CairoContainer *pContainer);

/**Fill the image buffer (surface & texture) of a given icon, according to its type. Set its size accordingly, and fills the reflection buffer for cairo.
*@param icon the icon.
*@param fMaxScale maximum zoom.
*@param bIsHorizontal TRUE if the icon will be in a horizontal container (needed for the cairo reflect).
*@param bDirectionUp TRUE if the icon will be in a up container (needed for the cairo reflect).
*/
void cairo_dock_fill_one_icon_buffer (Icon *icon, gdouble fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp);

/**Cut an UTF-8 or ASCII string to n characters, and add '...' to the end in cas it was effectively cut. If n is negative, it will remove the last |n| characters. It manages correctly UTF-8 strings.
*@param cString the string.
*@param iNbCaracters the maximum number of characters wished, or the number of characters to remove if negative.
*@return the newly allocated string.
*/
gchar *cairo_dock_cut_string (const gchar *cString, int iNbCaracters);

/**Fill the label buffer (surface & texture) of a given icon, according to a text description.
*@param icon the icon.
*@param pTextDescription desctiption of the text rendering.
*/
void cairo_dock_fill_one_text_buffer (Icon *icon, CairoDockLabelDescription *pTextDescription);

/**Fill the quick-info buffer (surface & texture) of a given icon, according to a text description.
*@param icon the icon.
*@param pTextDescription desctiption of the text rendering.
*@param fMaxScale maximum zoom.
*/
void cairo_dock_fill_one_quick_info_buffer (Icon *icon, CairoDockLabelDescription *pTextDescription, double fMaxScale);

void cairo_dock_fill_icon_buffers (Icon *icon, double fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp);
#define cairo_dock_fill_icon_buffers_for_desklet(pIcon) cairo_dock_fill_icon_buffers (pIcon, 1, CAIRO_DOCK_HORIZONTAL, TRUE)
#define cairo_dock_fill_icon_buffers_for_dock(pIcon, pDock) cairo_dock_fill_icon_buffers (pIcon, 1 + myIcons.fAmplitude, pDock->container.bIsHorizontal, pDock->container.bDirectionUp)

/** Fill all the buffers (surfaces & textures) of a given icon, according to its type. Set its size accordingly, and fills the reflection buffer for cairo. Label and quick-info are loaded with the current global text description.
*@param pIcon the icon.
*@param pContainer its container.
*/
void cairo_dock_load_one_icon_from_scratch (Icon *pIcon, CairoContainer *pContainer);

void cairo_dock_reload_buffers_in_dock (gchar *cDockName, CairoDock *pDock, gpointer data);
#define cairo_dock_load_buffers_in_one_dock(pDock) cairo_dock_reload_buffers_in_dock (NULL, pDock, GINT_TO_POINTER (TRUE))

void cairo_dock_reload_one_icon_buffer_in_dock (Icon *icon, CairoDock *pDock);



void cairo_dock_load_icons_background_surface (const gchar *cImagePath, double fMaxScale);

/**void cairo_dock_load_task_indicator (const gchar *cIndicatorImagePath, double fMaxScale, double fIndicatorRatio);

void cairo_dock_load_active_window_indicator (const gchar *cImagePath, double fMaxScale, double fCornerRadius, double fLineWidth, double *fActiveColor);

void cairo_dock_load_class_indicator (const gchar *cIndicatorImagePath, double fMaxScale);*/


void cairo_dock_init_icon_manager (void);

void cairo_dock_load_icon_textures (void);

void cairo_dock_unload_icon_textures (void);


void cairo_dock_draw_subdock_content_on_icon (Icon *pIcon, CairoDock *pDock);



typedef void (*CairoIconContainerLoadFunc) (void);
typedef void (*CairoIconContainerUnloadFunc) (void);
typedef void (*CairoIconContainerRenderFunc) (Icon *pIcon, int w, int h, cairo_t *pCairoContext);
typedef void (*CairoIconContainerRenderOpenGLFunc) (Icon *pIcon, int w, int h);

struct _CairoIconContainerRendererInterface {
	CairoIconContainerLoadFunc load;
	CairoIconContainerUnloadFunc unload;
	CairoIconContainerRenderFunc render;
	CairoIconContainerRenderOpenGLFunc render_opengl;
};


G_END_DECLS
#endif
