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


#ifndef __CAIRO_DOCK_FACILITY__
#define  __CAIRO_DOCK_FACILITY__

#include "cairo-dock-struct.h"
#include "cairo-dock-dock-factory.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-dock-facility.h This class contains functions to manipulate docks.
* Some functions are dedicated to linear docks, that is to say when the icon's position can be defined by 1 coordinate inside a non looped interval; it doesn't mean they have to be drawn on a straight line though, see the Curve view.
*/

/* Retourne la largeur max autorisee pour un dock.
* @param pDock le dock.
* @return la taille max.
*/
#define cairo_dock_get_max_authorized_dock_width gldi_dock_get_screen_width

/* Dis si un dock est etendu ou pas.
* @param pDock le dock.
* @return TRUE ssi le dock doit remplir l'ecran.
*/
#define cairo_dock_is_extended_dock(pDock) ((pDock)->bExtendedMode && (pDock)->iRefCount == 0)

#define cairo_dock_is_hidden(pDock) ((pDock)->iRefCount == 0 && (pDock)->bAutoHide && (pDock)->fHideOffset == 1 && (!g_pHidingBackend || !g_pHidingBackend->bCanDisplayHiddenDock))

#define cairo_dock_get_icon_size(pDock) ((pDock)->iIconSize != 0 ? (pDock)->iIconSize : myIconsParam.iIconWidth)

#define gldi_dock_get_screen_offset_x(pDock) (pDock->container.bIsHorizontal ? cairo_dock_get_screen_position_x (pDock->iNumScreen) : cairo_dock_get_screen_position_y (pDock->iNumScreen))
#define gldi_dock_get_screen_offset_y(pDock) (pDock->container.bIsHorizontal ? cairo_dock_get_screen_position_y (pDock->iNumScreen) : cairo_dock_get_screen_position_x (pDock->iNumScreen))
#define gldi_dock_get_screen_width(pDock) (pDock->container.bIsHorizontal ? cairo_dock_get_screen_width (pDock->iNumScreen) : cairo_dock_get_screen_height (pDock->iNumScreen))
#define gldi_dock_get_screen_height(pDock) (pDock->container.bIsHorizontal ? cairo_dock_get_screen_height (pDock->iNumScreen) : cairo_dock_get_screen_width (pDock->iNumScreen))
#define cairo_dock_dock_get_screen(pDock) (cairo_dock_get_nth_screen (pDock->iNumScreen))


/** Compute the maximum size of a dock, and resize it if necessary.
* It takes into account the size limit, and moves the dock so that it stays centered. Also updates the dock's background if necessary, and re-place the appli thumbnails.
*@param pDock the dock.
*/
void cairo_dock_update_dock_size (CairoDock *pDock);

void cairo_dock_trigger_update_dock_size (CairoDock *pDock);

/** Calculate the position of all icons inside a dock, and triggers the enter/leave events according to the position of the mouse.
*@param pDock the dock.
*@return the pointed icon, or NULL if none is pointed.
*/
Icon *cairo_dock_calculate_dock_icons (CairoDock *pDock);


/* Demande au WM d'empecher les autres fenetres d'empieter sur l'espace du dock.
* L'espace reserve est pris sur la taille minimale du dock, c'est-a-dire la taille de la zone de rappel si l'auto-hide est active,
* ou la taille du dock au repos sinon.
* @param pDock le dock.
* @param bReserve TRUE pour reserver l'espace, FALSE pour annuler la reservation.
*/
void cairo_dock_reserve_space_for_dock (CairoDock *pDock, gboolean bReserve);

/* Borne la position d'un dock a l'interieur de l'ecran.
*@param pDock le dock.
*/
void cairo_dock_prevent_dock_from_out_of_screen (CairoDock *pDock);  // -> dock-manager

/* Calcule la position d'un dock etant donne ses nouvelles dimensions.
*/
void cairo_dock_get_window_position_at_balance (CairoDock *pDock, int iNewWidth, int iNewHeight, int *iNewPositionX, int *iNewPositionY);

/* Met a jour les zones d'input d'un dock.
*/
void cairo_dock_update_input_shape (CairoDock *pDock);

#define cairo_dock_set_input_shape_active(pDock) do {\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), NULL);\
	if (pDock->fMagnitudeMax == 0.)\
		gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), pDock->pShapeBitmap);\
	else if (pDock->pActiveShapeBitmap != NULL)\
		gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), pDock->pActiveShapeBitmap);\
	} while (0)
#define cairo_dock_set_input_shape_at_rest(pDock) do {\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), NULL);\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), pDock->pShapeBitmap);\
	} while (0)
#define cairo_dock_set_input_shape_hidden(pDock) do {\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), NULL);\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), pDock->pHiddenShapeBitmap);\
	} while (0)

/** Pop up a sub-dock.
*@param pPointedIcon icon pointing on the sub-dock.
*@param pParentDock dock containing the icon.
*/
void cairo_dock_show_subdock (Icon *pPointedIcon, CairoDock *pParentDock);

/** Get a list of available docks.
*@param pParentDock excluding this dock if not NULL
*@param pSubDock excluding this dock and its children if not NULL
*@return a list of CairoDock*
*/
GList *cairo_dock_get_available_docks (CairoDock *pParentDock, CairoDock *pSubDock);

/** Get a list of available docks where an user icon can be placed. Its current parent dock is excluded, as well as its sub-dock (if any) and its children.
*@param pIcon the icon
*@return a list of CairoDock*
*/
#define cairo_dock_get_available_docks_for_icon(pIcon) cairo_dock_get_available_docks (CAIRO_DOCK(cairo_dock_get_icon_container(pIcon)), pIcon->pSubDock)


/** Calculate the position at rest (when the mouse is outside of the dock and its size is normal) of the icons of a linear dock.
*@param pIconList a list of icons.
*@param fFlatDockWidth width of all the icons placed next to each other.
*/
void cairo_dock_calculate_icons_positions_at_rest_linear (GList *pIconList, double fFlatDockWidth);

double cairo_dock_calculate_max_dock_width (CairoDock *pDock, double fFlatDockWidth, double fWidthConstraintFactor, double fExtraWidth);

Icon * cairo_dock_calculate_wave_with_position_linear (GList *pIconList, int x_abs, gdouble fMagnitude, double fFlatDockWidth, int iWidth, int iHeight, double fAlign, double fLateralFactor, gboolean bDirectionUp);

/** Apply a wave effect on the icons of a linear dock. It is the famous zoom when the mouse hovers an icon.
*@param pDock a linear dock.
*@return the pointed icon, or NULL if none is pointed.
*/
Icon *cairo_dock_apply_wave_effect_linear (CairoDock *pDock);
#define cairo_dock_apply_wave_effect cairo_dock_apply_wave_effect_linear

/** Get the current width of all the icons of a linear dock. It doesn't take into account any decoration or frame, only the space occupied by the icons.
*@param pDock a linear dock.
* @return the dock's width.
*/
double cairo_dock_get_current_dock_width_linear (CairoDock *pDock);

/** Check the position of the mouse inside a linear dock. It can be inside, on the edge, or outside. Update the 'iMousePositionType' field.
*@param pDock a linear dock.
*/
void cairo_dock_check_if_mouse_inside_linear (CairoDock *pDock);


/** Check if one can drop inside a linear dock.
*Drop is allowed between 2 icons of the launchers group, if the user is dragging something over the dock. Update the 'bCanDrop' field.
*@param pDock a linear dock.
*/
void cairo_dock_check_can_drop_linear (CairoDock *pDock);

void cairo_dock_stop_marking_icons (CairoDock *pDock);

void cairo_dock_set_subdock_position_linear (Icon *pPointedIcon, CairoDock *pParentDock);


/** Get the first icon to be drawn inside a linear dock, so that if you draw from left to right, the pointed icon will be drawn at last.
*@param icons a list of icons of a linear dock.
*@return the element of the list that contains the first icon to draw.
*/
GList *cairo_dock_get_first_drawn_element_linear (GList *icons);


void cairo_dock_trigger_redraw_subdock_content (CairoDock *pDock);
void cairo_dock_trigger_redraw_subdock_content_on_icon (Icon *icon);

void cairo_dock_redraw_subdock_content (CairoDock *pDock);

void cairo_dock_trigger_set_WM_icons_geometry (CairoDock *pDock);


void cairo_dock_resize_icon_in_dock (Icon *pIcon, CairoDock *pDock);


void cairo_dock_load_dock_background (CairoDock *pDock);
void cairo_dock_trigger_load_dock_background (CairoDock *pDock);  // peu utile


void cairo_dock_make_preview (CairoDock *pDock, const gchar *cPreviewPath);

G_END_DECLS
#endif
