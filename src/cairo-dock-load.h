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


#ifndef __CAIRO_DOCK_LOAD__
#define  __CAIRO_DOCK_LOAD__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-icon-loader.h"
#include "cairo-dock-surface-factory.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-load.h This class defines a simple image loader interface.
* It also handles the main image buffers of the dock.
* Use \ref cairo_dock_create_image_buffer to create an immage buffer from a file, or \ref cairo_dock_load_image_buffer to load an image into an existing immage buffer.
* Use \ref cairo_dock_free_image_buffer to destroy it or \ref cairo_dock_unload_image_buffer to unload and reset it to 0.
* If you just want to load an image into a mere cairo_surface, use the functions of the surface-factory.
*/

/// Definition of a Desktop Background Buffer. It has a reference count so that it can be shared across all the lib.
struct _CairoDockDesktopBackground {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	guint iSidDestroyBg;
	gint iRefCount;
	} ;

/// Definition of an Image Buffer. It provides an unified interface for a cairo/opengl image buffer.
struct _CairoDockImageBuffer {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	gint iWidth;
	gint iHeight;
	gdouble fZoomX;
	gdouble fZoomY;
	} ;

void cairo_dock_free_label_description (CairoDockLabelDescription *pTextDescription);
void cairo_dock_copy_label_description (CairoDockLabelDescription *pDestTextDescription, CairoDockLabelDescription *pOrigTextDescription);
CairoDockLabelDescription *cairo_dock_duplicate_label_description (CairoDockLabelDescription *pOrigTextDescription);

/** Say if 2 strings differ, taking into account NULL strings.
*/
#define cairo_dock_strings_differ(s1, s2) ((s1 && ! s2) || (! s1 && s2) || (s1 && s2 && strcmp (s1, s2)))
/** Say if 2 RGBA colors differ.
*/
#define cairo_dock_colors_rvb_differ(c1, c2) ((c1[0] != c2[0]) || (c1[1] != c2[1]) || (c1[2] != c2[2]))
/** Say if 2 RGB colors differ.
*/
#define cairo_dock_colors_differ(c1, c2) (cairo_dock_colors_rvb_differ (c1, c2) || (c1[3] != c2[3]))

/** Generate a complete path from a file name. '~' is handled, and files are supposed to be in the root folder of the current theme.
*@param cImageFile a file name or path. If it's already a path, it will just be duplicated.
*@return the path of the file.
*/
gchar *cairo_dock_generate_file_path (const gchar *cImageFile);


/** Load an image into an ImageBuffer with a given transparency. If the image is given by its sole name, it is taken in the root folder of the current theme.
*@param pImage an ImageBuffer.
*@param cImageFile name of a file
*@param iWidth width it should be loaded.
*@param iHeight height it should be loaded.
*@param iLoadModifier modifier
*@param fAlpha transparency (1:fully opaque)
*/
void cairo_dock_load_image_buffer_full (CairoDockImageBuffer *pImage, const gchar *cImageFile, int iWidth, int iHeight, CairoDockLoadImageModifier iLoadModifier, double fAlpha);
/** Load an image into an ImageBuffer. If the image is given by its sole name, it is taken in the root folder of the current theme.
*@param pImage an ImageBuffer.
*@param cImageFile name of a file
*@param iWidth width it should be loaded. The resulting width can be different depending on the modifier.
*@param iHeight height it should be loaded. The resulting width can be different depending on the modifier.
*@param iLoadModifier modifier
*/
#define cairo_dock_load_image_buffer(pImage, cImageFile, iWidth, iHeight, iLoadModifier) cairo_dock_load_image_buffer_full (pImage, cImageFile, iWidth, iHeight, iLoadModifier, 1.)
/** Load a surface into an ImageBuffer.
*@param pImage an ImageBuffer.
*@param pSurface a cairo surface
*@param iWidth width of the surface
*@param iHeight height of the surface
*/
void cairo_dock_load_image_buffer_from_surface (CairoDockImageBuffer *pImage, cairo_surface_t *pSurface, int iWidth, int iHeight);
/** Create and load an image into an ImageBuffer. If the image is given by its sole name, it is taken in the root folder of the current theme.
*@param cImageFile name of a file
*@param iWidth width it should be loaded.
*@param iHeight height it should be loaded.
*@param iLoadModifier modifier
*@return a newly allocated ImageBuffer.
*/
CairoDockImageBuffer *cairo_dock_create_image_buffer (const gchar *cImageFile, int iWidth, int iHeight, CairoDockLoadImageModifier iLoadModifier);

/** Reset an ImageBuffer's ressources. It can be used to load another image then.
*@param pImage an ImageBuffer.
*/
void cairo_dock_unload_image_buffer (CairoDockImageBuffer *pImage);
/** Reset and free an ImageBuffer.
*@param pImage an ImageBuffer.
*/
void cairo_dock_free_image_buffer (CairoDockImageBuffer *pImage);


///void cairo_dock_load_visible_zone (CairoDock *pDock, gchar *cVisibleZoneImageFile, int iVisibleZoneWidth, int iVisibleZoneHeight, double fVisibleZoneAlpha);

void cairo_dock_update_background_decorations_if_necessary (CairoDock *pDock, int iNewDecorationsWidth, int iNewDecorationsHeight);

void cairo_dock_load_background_decorations (CairoDock *pDock);


CairoDockDesktopBackground *cairo_dock_get_desktop_background (gboolean bWithTextureToo);

void cairo_dock_destroy_desktop_background (CairoDockDesktopBackground *pDesktopBg);

cairo_surface_t *cairo_dock_get_desktop_bg_surface (CairoDockDesktopBackground *pDesktopBg);

GLuint cairo_dock_get_desktop_bg_texture (CairoDockDesktopBackground *pDesktopBg);

void cairo_dock_reload_desktop_background (void);


void cairo_dock_unload_additionnal_textures (void);


G_END_DECLS
#endif
