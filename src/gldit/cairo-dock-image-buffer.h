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


#ifndef __CAIRO_DOCK_IMAGE_BUFFER__
#define  __CAIRO_DOCK_IMAGE_BUFFER__

#include <glib.h>
#include <sys/time.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-surface-factory.h"  // CairoDockLoadImageModifier
G_BEGIN_DECLS

/**
*@file cairo-dock-image-buffer.h This class defines a generic image API that works for both Cairo and OpenGL.
* It allows to easily load and display images, without having to care the rendering mode.
* It supports animated images (an animated image is made of several frames, ordered side by side from left to right).
* 
* Use \ref cairo_dock_create_image_buffer to create an image buffer from a file, or \ref cairo_dock_load_image_buffer to load an image into an existing image buffer.
* Use \ref cairo_dock_free_image_buffer to destroy it or \ref cairo_dock_unload_image_buffer to unload and reset it to 0.
* 
* Use \ref cairo_dock_apply_image_buffer_surface or \ref cairo_dock_apply_image_buffer_texture to display the image.
*/


/// Definition of an Image Buffer. It provides an unified interface for a cairo/opengl image buffer.
struct _CairoDockImageBuffer {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	gint iWidth;
	gint iHeight;
	gdouble fZoomX;
	gdouble fZoomY;
	gint iNbFrames;  // nb frames in the case of an animated image.
	gdouble iCurrentFrame; // current frame, the decimal part indicates we are between 2 frames.
	gdouble fDeltaFrame;  // duration of 1 frame
	struct timeval time;  // time the current frame has been set
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

/** Find the path of an image. '~' is handled, as well as the 'images' folder of the current theme. Use \ref cairo_dock_search_icon_s_path to search theme icons.
*@param cImageFile a file name or path. If it's already a path, it will just be duplicated.
*@return the path of the file, or NULL if it has not been found.
*/
gchar *cairo_dock_search_image_s_path (const gchar *cImageFile);
#define cairo_dock_generate_file_path cairo_dock_search_image_s_path


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

void cairo_dock_load_image_buffer_from_texture (CairoDockImageBuffer *pImage, GLuint iTexture, int iWidth, int iHeight);

/** Create and load an image into an ImageBuffer. If the image is given by its sole name, it is taken in the root folder of the current theme.
*@param cImageFile name of a file
*@param iWidth width it should be loaded.
*@param iHeight height it should be loaded.
*@param iLoadModifier modifier
*@return a newly allocated ImageBuffer.
*/
CairoDockImageBuffer *cairo_dock_create_image_buffer (const gchar *cImageFile, int iWidth, int iHeight, CairoDockLoadImageModifier iLoadModifier);

#define cairo_dock_image_buffer_is_animated(pImage) ((pImage) && (pImage)->iNbFrames > 0)

void cairo_dock_image_buffer_next_frame (CairoDockImageBuffer *pImage);

#define cairo_dock_image_buffer_set_timelength(pImage, fTimeLength) (pImage)->fDeltaFrame = ((pImage)->iNbFrames != 0 ? (double)fTimeLength / (pImage)->iNbFrames : 1)

/** Reset an ImageBuffer's ressources. It can be used to load another image then.
*@param pImage an ImageBuffer.
*/
void cairo_dock_unload_image_buffer (CairoDockImageBuffer *pImage);
/** Reset and free an ImageBuffer.
*@param pImage an ImageBuffer.
*/
void cairo_dock_free_image_buffer (CairoDockImageBuffer *pImage);


/** Draw an ImageBuffer with an offset on a Cairo context, at the size it was loaded.
*@param pImage an ImageBuffer.
*@param pCairoContext the current cairo context.
*@param x horizontal offset.
*@param y vertical offset.
*@param fAlpha transparency (in [0;1])
*/
void cairo_dock_apply_image_buffer_surface_with_offset (CairoDockImageBuffer *pImage, cairo_t *pCairoContext, double x, double y, double fAlpha);

/** Draw an ImageBuffer on a cairo context.
*@param pImage an ImageBuffer.
*@param pCairoContext the current cairo context.
*/
#define cairo_dock_apply_image_buffer_surface(pImage, pCairoContext) cairo_dock_apply_image_buffer_surface_with_offset (pImage, pCairoContext, 0., 0., 1.)

/** Draw an ImageBuffer with an offset on the current OpenGL context, at the size it was loaded.
*@param pImage an ImageBuffer.
*@param x horizontal offset.
*@param y vertical offset.
*/
void cairo_dock_apply_image_buffer_texture_with_offset (CairoDockImageBuffer *pImage, double x, double y);

/** Draw an ImageBuffer on the current OpenGL context.
*@param pImage an ImageBuffer.
*/
#define cairo_dock_apply_image_buffer_texture(pImage) cairo_dock_apply_image_buffer_texture_with_offset (pImage, 0., 0.)

/** Draw an ImageBuffer with an offset on a Cairo context, at a given size.
*@param pImage an ImageBuffer.
*@param pCairoContext the current cairo context.
*@param w requested width
*@param h requested height
*@param x horizontal offset.
*@param y vertical offset.
*@param fAlpha transparency (in [0;1])
*/
void cairo_dock_apply_image_buffer_surface_at_size (CairoDockImageBuffer *pImage, cairo_t *pCairoContext, int w, int h, double x, double y, double fAlpha);

/** Draw an ImageBuffer on the current OpenGL context at a given size.
*@param pImage an ImageBuffer.
*@param w requested width
*@param h requested height
*@param x horizontal offset.
*@param y vertical offset.
*/
void cairo_dock_apply_image_buffer_texture_at_size (CairoDockImageBuffer *pImage, int w, int h, double x, double y);


void cairo_dock_apply_image_buffer_surface_with_offset_and_limit (CairoDockImageBuffer *pImage, cairo_t *pCairoContext, double x, double y, double fAlpha, int iMaxWidth);

void cairo_dock_apply_image_buffer_texture_with_limit (CairoDockImageBuffer *pImage, double fAlpha, int iMaxWidth);


G_END_DECLS
#endif
