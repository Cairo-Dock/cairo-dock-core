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

#ifndef __CAIRO_DOCK_OVERLAY__
#define  __CAIRO_DOCK_OVERLAY__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
 *@file cairo-dock-overlay.h This class defines Overlays, that are small images superimposed on the icon at a given position.
 * You can either print the overlay directly on the icon's surface/texture, or add it (i nthis case it is drawn separately, and can be removed without modifying the icon's surface/texture, and will remain even if you erase the icon's surface/texture).
 * Only one overlay can be added at a given position.
 * 
 * To add an overlay to an icon, use \ref cairo_dock_add_overlay_from_image or \ref cairo_dock_add_overlay_from_surface.
 * To remove an overlay, use \ref cairo_dock_remove_overlay_at_position.
 * If you need to modify an overlay directly, you can get its image buffer with \ref cairo_dock_get_overlay_buffer_at_position.
 * If you're never going to update nor remove an overlay, you can choose to print it directly onto the icon with \ref cairo_dock_print_overlay_on_icon_from_image or \ref cairo_dock_print_overlay_on_icon_from_surface, which is slightly optimized.
 */

/// Available position of an overlay on an icon.
typedef enum {
	CAIRO_OVERLAY_UPPER_LEFT,
	CAIRO_OVERLAY_LOWER_RIGHT,
	CAIRO_OVERLAY_LOWER_LEFT,
	CAIRO_OVERLAY_UPPER_RIGHT,
	CAIRO_OVERLAY_MIDDLE,
	CAIRO_OVERLAY_BOTTOM,
	CAIRO_OVERLAY_TOP,
	CAIRO_OVERLAY_RIGHT,
	CAIRO_OVERLAY_LEFT,
	CAIRO_OVERLAY_NB_POSITIONS,
} CairoOverlayPosition;


/// Definition of an Icon Overlay.
struct _CairoOverlay {
	/// image buffer
	CairoDockImageBuffer image;
	/// position on the icon
	CairoOverlayPosition iPosition;
	/// scale at which to draw the overlay, relatively to the icon (0.5 by default, 1 will cover the whole icon, 0 means to draw at the actual buffer size).
	gdouble fScale;
} ;


  ///////////////////
 // CREATE / FREE //
///////////////////

CairoOverlay *cairo_dock_create_overlay_from_image (Icon *pIcon, const gchar *cImageFile);

CairoOverlay *cairo_dock_create_overlay_from_surface (Icon *pIcon, cairo_surface_t *pSurface, int iWidth, int iHeight);

CairoOverlay *cairo_dock_create_overlay_from_texture (Icon *pIcon, GLuint iTexture, int iWidth, int iHeight);

void cairo_dock_free_overlay (CairoOverlay *pOverlay);

#define cairo_dock_set_overlay_scale(pOverlay, fScale) (pOverlay)->fScale = fScale

  //////////////////
 // ADD / REMOVE //
//////////////////

/** Add an overlay on an icon.
 *@param pIcon the icon
 *@param pOverlay the overlay
 *@param iPosition position where to display the overlay
 */
void cairo_dock_add_overlay_to_icon (Icon *pIcon, CairoOverlay *pOverlay, CairoOverlayPosition iPosition);

/** Add an overlay on an icon from an image.
 *@param pIcon the icon
 *@param cImageFile an image (if it's not a path, it is searched amongst the current theme's images)
 *@param iPosition position where to display the overlay
 *@return TRUE if the overlay has been successfuly added.
 */
gboolean cairo_dock_add_overlay_from_image (Icon *pIcon, const gchar *cImageFile, CairoOverlayPosition iPosition);

/** Add an overlay on an icon from a surface.
 *@param pIcon the icon
 *@param pSurface a cairo surface
 *@param iWidth width of the surface
 *@param iHeight height of the surface
 *@param iPosition position where to display the overlay
 */
void cairo_dock_add_overlay_from_surface (Icon *pIcon, cairo_surface_t *pSurface, int iWidth, int iHeight, CairoOverlayPosition iPosition);

/** Add an overlay on an icon from a texture.
 *@param pIcon the icon
 *@param iTexture a texture
 *@param iPosition position where to display the overlay
 */
void cairo_dock_add_overlay_from_texture (Icon *pIcon, GLuint iTexture, CairoOverlayPosition iPosition);

/** Remove an overlay on an icon, given its position (there is only one overlay at a given position).
 *@param pIcon the icon
 *@param iPosition position of the overlay
 */
void cairo_dock_remove_overlay_at_position (Icon *pIcon, CairoOverlayPosition iPosition);

/** Get the image buffer of an overlay, given its position (there is only one overlay at a given position).
 *@param pIcon the icon
 *@param iPosition position of the overlay
 *@return the image-buffer of the overlay, or NULL if there is no overlay at this position.
 */
CairoDockImageBuffer *cairo_dock_get_overlay_buffer_at_position (Icon *pIcon, CairoOverlayPosition iPosition);


void cairo_dock_destroy_icon_overlays (Icon *pIcon);

  //////////
 // DRAW //
//////////

void cairo_dock_draw_icon_overlays_cairo (Icon *pIcon, double fRatio, cairo_t *pCairoContext);

void cairo_dock_draw_icon_overlays_opengl (Icon *pIcon, double fRatio);

  ///////////
 // PRINT //
///////////

void cairo_dock_print_overlay_on_icon (Icon *pIcon, CairoContainer *pContainer, CairoOverlay *pOverlay, CairoOverlayPosition iPosition);

/** Print an overlay onto an icon from an image at a given position. You can't remove/modify the overlay then. The overlay will be displayed until you modify the icon directly (for instance by setting a new image).
 *@param pIcon the icon
 *@param pContainer container of the icon
 *@param cImageFile an image (if it's not a path, it is searched amongst the current theme's images)
 *@param iPosition position where to display the overlay
 *@return TRUE if the overlay has been successfuly printed.
 */
gboolean cairo_dock_print_overlay_on_icon_from_image (Icon *pIcon, CairoContainer *pContainer, const gchar *cImageFile, CairoOverlayPosition iPosition);

/** Print an overlay onto an icon from a surface at a given position. You can't remove/modify the overlay then. The overlay will be displayed until you modify the icon directly (for instance by setting a new image).
 *@param pIcon the icon
 *@param pContainer container of the icon
 *@param pSurface a cairo surface
 *@param iWidth width of the surface
 *@param iHeight height of the surface
 *@param iPosition position where to display the overlay
 *@return TRUE if the overlay has been successfuly printed.
 */
void cairo_dock_print_overlay_on_icon_from_surface (Icon *pIcon, CairoContainer *pContainer, cairo_surface_t *pSurface, int iWidth, int iHeight, CairoOverlayPosition iPosition);

void cairo_dock_print_overlay_on_icon_from_texture (Icon *pIcon, CairoContainer *pContainer, GLuint iTexture, CairoOverlayPosition iPosition);


G_END_DECLS
#endif
