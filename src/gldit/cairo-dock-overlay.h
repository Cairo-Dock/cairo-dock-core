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
#include "cairo-dock-object.h"
G_BEGIN_DECLS

/**
 *@file cairo-dock-overlay.h This class defines Overlays, that are small images superimposed on the icon at a given position.
 * 
 * To add an overlay to an icon, use \ref cairo_dock_add_overlay_from_image or \ref cairo_dock_add_overlay_from_surface.
 * The overlay can then be removed from the icon by simply destroying it with \ref cairo_dock_destroy_overlay
 * 
 * A common feature is to have only 1 overlay at a given position. This can be achieved by passing a non-NULL data to the creation functions. This data will identify all of your overlays.
 * You can then remove an overlay simply from its position with \ref cairo_dock_remove_overlay_at_position, and adding an overlay at a position will automatically remove any previous overlay at this position with the same data.
 * 
 * If you're never going to update nor remove an overlay, you can choose to print it directly onto the icon with \ref cairo_dock_print_overlay_on_icon_from_image or \ref cairo_dock_print_overlay_on_icon_from_surface, which is slightly faster.
 * 
 * Overlays are drawn at 1/2 of the icon size by default, but this can be set up with \ref cairo_dock_set_overlay_scale.
 * If you need to modify an overlay directly, you can get its image buffer with \ref cairo_dock_get_overlay_image_buffer.
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
	/// object
	GldiObject object;
	/// image buffer
	CairoDockImageBuffer image;
	/// position on the icon
	CairoOverlayPosition iPosition;
	/// scale at which to draw the overlay, relatively to the icon (0.5 by default, 1 will cover the whole icon, 0 means to draw at the actual buffer size).
	gdouble fScale;
	/// icon it belongs to.
	Icon *pIcon;
	/// data used to identify an overlay
	gpointer data;
} ;


  ///////////////////
 // OVERLAY CLASS //
///////////////////

/** Add an overlay on an icon from an image.
 *@param pIcon the icon
 *@param cImageFile an image (if it's not a path, it is searched amongst the current theme's images)
 *@param iPosition position where to display the overlay
 *@return the overlay, or NULL if the image couldn't be loaded.
 *@param data data that will be used to look for the overlay in \ref cairo_dock_remove_overlay_at_position; if NULL, then this function can't be used
 */
CairoOverlay *cairo_dock_add_overlay_from_image (Icon *pIcon, const gchar *cImageFile, CairoOverlayPosition iPosition, gpointer data);

/** Add an overlay on an icon from a surface.
 *@param pIcon the icon
 *@param pSurface a cairo surface
 *@param iWidth width of the surface
 *@param iHeight height of the surface
 *@param iPosition position where to display the overlay
 *@param data data that will be used to look for the overlay in \ref cairo_dock_remove_overlay_at_position; if NULL, then this function can't be used
 *@return the overlay.
 */
CairoOverlay *cairo_dock_add_overlay_from_surface (Icon *pIcon, cairo_surface_t *pSurface, int iWidth, int iHeight, CairoOverlayPosition iPosition, gpointer data);

/** Add an overlay on an icon from a texture.
 *@param pIcon the icon
 *@param iTexture a texture
 *@param iPosition position where to display the overlay
 *@param data data that will be used to look for the overlay in \ref cairo_dock_remove_overlay_at_position; if NULL, then this function can't be used
 *@return the overlay.
 */
CairoOverlay *cairo_dock_add_overlay_from_texture (Icon *pIcon, GLuint iTexture, CairoOverlayPosition iPosition, gpointer data);


/** Set the scale of an overlay; by default it's 0.5
 *@param pOverlay the overlay
 *@param fScale the scale
 */
#define cairo_dock_set_overlay_scale(pOverlay, fScale) (pOverlay)->fScale = fScale

/** Get the image buffer of an overlay (only useful if you need to redraw the overlay).
 *@param pOverlay the overlay
 */
#define cairo_dock_get_overlay_image_buffer(pOverlay) (&(pOverlay)->image)


/** Destroy an overlay (it is removed from its icon).
 *@param pOverlay the overlay
 */
void cairo_dock_destroy_overlay (CairoOverlay *pOverlay);

/** Remove an overlay from an icon, given its position and data.
 *@param pIcon the icon
 *@param iPosition the position of the overlay
 *@param data data that was set on the overlay when created; a NULL pointer is not valid.
 */
void cairo_dock_remove_overlay_at_position (Icon *pIcon, CairoOverlayPosition iPosition, gpointer data);


  ///////////////////
 // ICON OVERLAYS //
///////////////////

void cairo_dock_destroy_icon_overlays (Icon *pIcon);

void cairo_dock_draw_icon_overlays_cairo (Icon *pIcon, double fRatio, cairo_t *pCairoContext);

void cairo_dock_draw_icon_overlays_opengl (Icon *pIcon, double fRatio);


  ///////////
 // PRINT //
///////////

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
