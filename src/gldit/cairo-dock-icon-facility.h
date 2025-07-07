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

#ifndef __CAIRO_DOCK_ICON_FACILITY__
#define  __CAIRO_DOCK_ICON_FACILITY__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-icon-factory.h"  // CairoDockIconGroup
G_BEGIN_DECLS


/**
*@file cairo-dock-icon-facility.h This class provides utility functions on Icons.
*/

/** Say whether an icon is currently being inserted.
*/
#define cairo_dock_icon_is_being_inserted(icon) ((icon)->fInsertRemoveFactor < 0)
/** Say whether an icon is currently being removed.
*/
#define cairo_dock_icon_is_being_removed(icon) ((icon)->fInsertRemoveFactor > 0)
#define cairo_dock_icon_is_being_inserted_or_removed(icon) ((icon)->fInsertRemoveFactor != 0)


#define cairo_dock_get_group_order(iGroup) (iGroup < CAIRO_DOCK_NB_GROUPS ? myIconsParam.tIconTypeOrder[iGroup] : iGroup)
/** Get the group order of an icon. 3 groups are available by default : launchers, applis, and applets, and each group has an order.
*/
#define cairo_dock_get_icon_order(icon) cairo_dock_get_group_order ((icon)->iGroup)

#define cairo_dock_set_icon_container(_pIcon, _pContainer) (_pIcon)->pContainer = CAIRO_CONTAINER (_pContainer)

#define cairo_dock_get_icon_container(_pIcon) (_pIcon)->pContainer

#define cairo_dock_get_icon_max_scale(pIcon) (pIcon->fHeight != 0 && pIcon->pContainer ? (pIcon->pContainer->bIsHorizontal ? pIcon->iAllocatedHeight : pIcon->iAllocatedWidth) / (pIcon->fHeight/pIcon->pContainer->fRatio) : 1.)

#define cairo_dock_set_icon_ignore_quicklist(_pIcon) (_pIcon)->bIgnoreQuicklist = TRUE

#define cairo_dock_icon_set_requested_size(icon, w, h) do { \
	(icon)->iRequestedWidth = w; \
	(icon)->iRequestedHeight = h; \
	(icon)->iRequestedDisplayWidth = 0; \
	(icon)->iRequestedDisplayHeight = 0; } while(0)

#define cairo_dock_icon_set_requested_display_size(icon, w, h) do { \
	(icon)->iRequestedDisplayWidth = w; \
	(icon)->iRequestedDisplayHeight = h; \
	(icon)->iRequestedWidth = 0; \
	(icon)->iRequestedHeight = 0; } while(0)

#define cairo_dock_icon_get_requested_width(icon) (icon)->iRequestedWidth

#define cairo_dock_icon_get_requested_height(icon) (icon)->iRequestedHeight

#define cairo_dock_icon_get_requested_display_width(icon) (icon)->iRequestedDisplayWidth

#define cairo_dock_icon_get_requested_display_height(icon) (icon)->iRequestedDisplayHeight

#define cairo_dock_icon_set_allocated_size(icon, w, h) do { \
	(icon)->iAllocatedWidth = w; \
	(icon)->iAllocatedHeight = h; } while(0)

#define cairo_dock_icon_get_allocated_width(icon) (icon)->iAllocatedWidth

#define cairo_dock_icon_get_allocated_height(icon) (icon)->iAllocatedHeight


void gldi_icon_set_appli (Icon *pIcon, GldiWindowActor *pAppli);
#define gldi_icon_unset_appli(pIcon) gldi_icon_set_appli (pIcon, NULL)

/** Get the type of an icon according to its content (launcher, appli, applet). This can be different from its group.
*@param icon the icon.
*@return the type of the icon.
*/
CairoDockIconGroup cairo_dock_get_icon_type (Icon *icon);

/** Compare 2 icons with the order relation on (group order, icon order).
*@param icon1 an icon.
*@param icon2 another icon.
*@return -1 if icon1 < icon2, 1 if icon1 > icon2, 0 if icon1 = icon2.
*/
int cairo_dock_compare_icons_order (Icon *icon1, Icon *icon2);

/** Compare 2 icons with the order relation on the name (case unsensitive alphabetical order).
*@param icon1 an icon.
*@param icon2 another icon.
*@return -1 if icon1 < icon2, 1 if icon1 > icon2, 0 if icon1 = icon2.
*/
int cairo_dock_compare_icons_name (Icon *icon1, Icon *icon2);

/**
*Compare 2 icons with the order relation on the extension of their URIs (case unsensitive alphabetical order).
*@param icon1 an icon.
*@param icon2 another icon.
*@return -1 if icon1 < icon2, 1 if icon1 > icon2, 0 if icon1 = icon2.
*/
int cairo_dock_compare_icons_extension (Icon *icon1, Icon *icon2);

/** Sort a list with the order relation on (group order, icon order).
*@param pIconList a list of icons.
*@return the sorted list. Elements are the same as the initial list, only their order has changed.
*/
GList *cairo_dock_sort_icons_by_order (GList *pIconList);

/** Sort a list with the alphabetical order on the icons' name.
*@param pIconList a list of icons.
*@return the sorted list. Elements are the same as the initial list, only their order has changed. Icon's orders are updated to reflect the new order.
*/
GList *cairo_dock_sort_icons_by_name (GList *pIconList);

/** Get the first icon of a list of icons.
*@param pIconList a list of icons.
*@return the first icon, or NULL if the list is empty.
*/
Icon *cairo_dock_get_first_icon (GList *pIconList);

/** Get the last icon of a list of icons.
*@param pIconList a list of icons.
*@return the last icon, or NULL if the list is empty.
*/
Icon *cairo_dock_get_last_icon (GList *pIconList);

/** Get the first icon of a given group.
*@param pIconList a list of icons.
*@param iGroup the group of icon.
*@return the first found icon with this group, or NULL if none matches.
*/
Icon *cairo_dock_get_first_icon_of_group (GList *pIconList, CairoDockIconGroup iGroup);
#define cairo_dock_get_first_icon_of_type cairo_dock_get_first_icon_of_group

/** Get the last icon of a given group.
*@param pIconList a list of icons.
*@param iGroup the group of icon.
*@return the last found icon with this group, or NULL if none matches.
*/
Icon *cairo_dock_get_last_icon_of_group (GList *pIconList, CairoDockIconGroup iGroup);

/** Get the first icon whose group has the same order as a given one.
*@param pIconList a list of icons.
*@param iGroup a group of icon.
*@return the first found icon, or NULL if none matches.
*/
Icon* cairo_dock_get_first_icon_of_order (GList *pIconList, CairoDockIconGroup iGroup);

/** Get the last icon whose group has the same order as a given one.
*@param pIconList a list of icons.
*@param iGroup a group of icon.
*@return the last found icon, or NULL if none matches.
*/
Icon* cairo_dock_get_last_icon_of_order (GList *pIconList, CairoDockIconGroup iGroup);

/** Get the currently pointed icon in a list of icons.
*@param pIconList a list of icons.
*@return the icon whose field 'bPointed' is TRUE, or NULL if none is pointed.
*/
Icon *cairo_dock_get_pointed_icon (GList *pIconList);

/** Get the icon next to a given one. The cost is O(n).
*@param pIconList a list of icons.
*@param pIcon an icon in the list.
*@return the icon whose left neighboor is pIcon, or NULL if the list is empty or if pIcon is the last icon.
*/
Icon *cairo_dock_get_next_icon (GList *pIconList, Icon *pIcon);

/** Get the icon previous to a given one. The cost is O(n).
*@param pIconList a list of icons.
*@param pIcon an icon in the list.
*@return the icon whose right neighboor is pIcon, or NULL if the list is empty or if pIcon is the first icon.
*/
Icon *cairo_dock_get_previous_icon (GList *pIconList, Icon *pIcon);

/** Get the next element in a list, looping if necessary..
*@param ic the current element.
*@param list a list.
*@return the next element, or the first element of the list if 'ic' is the last one.
*/
#define cairo_dock_get_next_element(ic, list) (ic == NULL || ic->next == NULL ? list : ic->next)

/** Get the previous element in a list, looping if necessary..
*@param ic the current element.
*@param list a list.
*@return the previous element, or the last element of the list if 'ic' is the first one.
*/
#define cairo_dock_get_previous_element(ic, list) (ic == NULL || ic->prev == NULL ? g_list_last (list) : ic->prev)

/** Search an icon with a given command in a list of icons.
*@param pIconList a list of icons.
*@param cCommand the command.
*@return the first icon whose field 'cCommand' is identical to the given command, or NULL if no icon matches.
*/
Icon *cairo_dock_get_icon_with_command (GList *pIconList, const gchar *cCommand);

/** Search an icon with a given URI in a list of icons.
*@param pIconList a list of icons.
*@param cBaseURI the URI.
*@return the first icon whose field 'cURI' is identical to the given URI, or NULL if no icon matches.
*/
Icon *cairo_dock_get_icon_with_base_uri (GList *pIconList, const gchar *cBaseURI);

/** Search an icon with a given name in a list of icons.
*@param pIconList a list of icons.
*@param cName the name.
*@return the first icon whose field 'cName' is identical to the given name, or NULL if no icon matches.
*/
Icon *cairo_dock_get_icon_with_name (GList *pIconList, const gchar *cName);

/** Search the icon pointing on a given sub-dock in a list of icons.
*@param pIconList a list of icons.
*@param pSubDock a sub-dock.
*@return the first icon whose field 'pSubDock' is equal to the given sub-dock, or NULL if no icon matches.
*/
Icon *cairo_dock_get_icon_with_subdock (GList *pIconList, CairoDock *pSubDock);

Icon *gldi_icons_get_without_dialog (GList *pIconList);

#define gldi_icons_get_any_without_dialog(...) gldi_icons_get_without_dialog (g_pMainDock?g_pMainDock->icons:NULL);

gboolean gldi_icon_has_dialog (Icon *pIcon);

#define cairo_dock_get_last_launcher(pIconList) cairo_dock_get_last_icon_of_group (pIconList, CAIRO_DOCK_LAUNCHER)

/** Get the dimension allocated to the surface/texture of an icon.
@param pIcon the icon.
@param iWidth pointer to the width.
@param iHeight pointer to the height.
*/
void cairo_dock_get_icon_extent (Icon *pIcon, int *iWidth, int *iHeight);

/** Get the current size of an icon as it is seen on the screen (taking into account the zoom and the ratio).
@param pIcon the icon
@param pContainer its container
@param fSizeX pointer to the X size (horizontal)
@param fSizeY pointer to the Y size (vertical)
*/
void cairo_dock_get_current_icon_size (Icon *pIcon, GldiContainer *pContainer, double *fSizeX, double *fSizeY);

/** Get the total zone used by an icon on its container (taking into account reflect, gap to reflect, zoom and stretching).
@param icon the icon
@param pContainer its container
@param pArea a rectangle filled with the zone used by the icon on its container.
*/
void cairo_dock_compute_icon_area (Icon *icon, GldiContainer *pContainer, GdkRectangle *pArea);



void cairo_dock_normalize_icons_order (GList *pIconList, CairoDockIconGroup iGroup);

void cairo_dock_move_icon_after_icon (CairoDock *pDock, Icon *icon1, Icon *icon2);

/** Make an icon static or not. Static icons are not animated when mouse hovers them.
*@param icon an icon.
*@param _bStatic static or not.
*/
#define cairo_dock_set_icon_static(icon, _bStatic) (icon)->bStatic = _bStatic

/** Make an icon always visible, even when the dock is hidden.
*@param icon an icon.
*@param _bAlwaysVisible whether the icon is always visible or not.
*/
#define cairo_dock_set_icon_always_visible(icon, _bAlwaysVisible) (icon)->bAlwaysVisible = _bAlwaysVisible

/** Set the label of an icon. If it has a sub-dock, it is renamed (the name is possibly altered to stay unique). The label buffer is updated too.
*@param pIcon the icon.
*@param cIconName the new label of the icon. You can even pass pIcon->cName.
*/
void gldi_icon_set_name (Icon *pIcon, const gchar *cIconName);

/** Same as above, but takes a printf-like format string.
*@param pIcon the icon.
*@param cIconNameFormat the new label of the icon, in a 'printf' way.
*@param ... data to be inserted into the string.
*/
void gldi_icon_set_name_printf (Icon *pIcon, const gchar *cIconNameFormat, ...) G_GNUC_PRINTF (2, 3);

/** Set the quick-info of an icon. This is a small text (a few characters) that is superimposed on the icon.
*@param pIcon the icon.
*@param cQuickInfo the text of the quick-info. If NULL, will just remove the current the quick-info.
*/
void gldi_icon_set_quick_info (Icon *pIcon, const gchar *cQuickInfo);

/** Same as above, but takes a printf-like format string.
*@param pIcon the icon.
*@param cQuickInfoFormat the text of the quick-info, in a 'printf' way.
*@param ... data to be inserted into the string.
*/
void gldi_icon_set_quick_info_printf (Icon *pIcon, const gchar *cQuickInfoFormat, ...) G_GNUC_PRINTF (2, 3);


#define cairo_dock_listen_for_double_click(pIcon) (pIcon)->iNbDoubleClickListeners ++

#define cairo_dock_stop_listening_for_double_click(pIcon) do {\
	if ((pIcon)->iNbDoubleClickListeners > 0)\
		(pIcon)->iNbDoubleClickListeners --; } while (0)


/** Create a copy of the icon's image, scaled to the desired size, preserving the correct device scale. */
cairo_surface_t *cairo_dock_icon_buffer_to_cairo (Icon *icon, int iWidth, int iHeight);


cairo_t *cairo_dock_begin_draw_icon_cairo (Icon *pIcon, gint iRenderingMode, cairo_t *pCairoContext);

void cairo_dock_end_draw_icon_cairo (Icon *pIcon);

/** Initiate an OpenGL drawing session on an icon's texture.
*@param pIcon the icon on which to draw.
*@param iRenderingMode rendering mode. 0:normal, 1:don't clear the current texture, so that the drawing will be superimposed on it, 2:keep the current icon texture unchanged for all the drawing (the drawing is made on another texture).
*@return TRUE if you can proceed to the drawing, FALSE if an error occured.
*/
gboolean cairo_dock_begin_draw_icon (Icon *pIcon, gint iRenderingMode);
/** Finish an OpenGL drawing session on an icon.
*@param pIcon the icon on which to draw.
*@return TRUE if you can proceed to the drawing, FALSE if an error occured.
*/
void cairo_dock_end_draw_icon (Icon *pIcon);


void gldi_theme_icon_write_container_name_in_conf_file (Icon *pIcon, const gchar *cParentDockName);

void gldi_theme_icon_write_order_in_conf_file (Icon *pIcon, double fOrder);


gboolean gldi_icon_launch_command (Icon *pIcon);

/** Mark an Icon as 'launching'. This states lasts until the corresponding window appears (with a timeout of 15 seconds).
 * Typically used to prevent the program from being started 2 times in a row, or to keep the animation running until the program is started.
 */
#define gldi_icon_mark_as_launching(pIcon) (pIcon)->bIsLaunching = TRUE

#define gldi_icon_stop_marking_as_launching(pIcon) (pIcon)->bIsLaunching = FALSE

/** Tell if an Icon is being launched.
 */
#define gldi_icon_is_launching(pIcon) ((pIcon)->bIsLaunching)


G_END_DECLS
#endif
