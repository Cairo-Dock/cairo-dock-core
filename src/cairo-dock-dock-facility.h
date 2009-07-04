
#ifndef __CAIRO_DOCK_FACILITY__
#define  __CAIRO_DOCK_FACILITY__

#include <glib.h>

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
#define cairo_dock_get_max_authorized_dock_width(pDock) (myAccessibility.iMaxAuthorizedWidth == 0 ? g_iScreenWidth[pDock->bHorizontalDock] : MIN (myAccessibility.iMaxAuthorizedWidth, g_iScreenWidth[pDock->bHorizontalDock]))


/*
* Recharge les reflets (cairo) d'un dock. Utile si le dock a changé de position.
* @param pDock un dock.
*/
void cairo_dock_reload_reflects_in_dock (CairoDock *pDock);


/** Compute the maximum size of a dock, and resize it if necessary.
* It takes into account the size limit, and moves the dock so that it stays centered. Also updates the dock's background if necessary, and re-place the appli thumbnails.
*@param pDock the dock.
*/
void cairo_dock_update_dock_size (CairoDock *pDock);

/** Calculate the position of all icons inside a dock, and triggers the enter/leave events according to the position of the mouse.
*@param pDock the dock.
*@return the pointed icon, or NULL if none is pointed.
*/
Icon *cairo_dock_calculate_dock_icons (CairoDock *pDock);


/*
* Demande au WM d'empecher les autres fenetres d'empieter sur l'espace du dock.
* L'espace reserve est pris sur la taille minimale du dock, c'est-a-dire la taille de la zone de rappel si l'auto-hide est active,
* ou la taille du dock au repos sinon.
* @param pDock le dock.
* @param bReserve TRUE pour reserver l'espace, FALSE pour annuler la reservation.
*/
void cairo_dock_reserve_space_for_dock (CairoDock *pDock, gboolean bReserve);

/*
* Met un dock principal a sa taille et a sa place initiale.
*@param pDock le dock.
*/
void cairo_dock_place_root_dock (CairoDock *pDock);
/*
* Borne la position d'un dock a l'interieur de l'ecran.
*@param pDock le dock.
*/
void cairo_dock_prevent_dock_from_out_of_screen (CairoDock *pDock);


void cairo_dock_set_window_position_at_balance (CairoDock *pDock, int iNewWidth, int iNewHeight);

void cairo_dock_get_window_position_and_geometry_at_balance (CairoDock *pDock, CairoDockSizeType iSizeType, int *iNewWidth, int *iNewHeight);

void cairo_dock_set_subdock_position_linear (Icon *pPointedIcon, CairoDock *pParentDock);

/** Pop up a sub-dock.
*@param pPointedIcon icon pointing on the sub-dock.
*@param pParentDock dock containing the icon.
*@param bUpdateBefore TRUE to re-calculate the parent dock before.
*/
void cairo_dock_show_subdock (Icon *pPointedIcon, CairoDock *pParentDock, gboolean bUpdateBefore);


void cairo_dock_set_input_shape (CairoDock *pDock);

/** Calculate the position at rest (when the mouse is outside of the dock and its size is normal) of the icons of a linear dock. 
*@param pIconList a list of icons.
*@param fFlatDockWidth width of all the icons placed next to each other.
*@param iXOffset an offset on the position of the first icon.
*@return the element containing the most left icon.
*/
GList * cairo_dock_calculate_icons_positions_at_rest_linear (GList *pIconList, double fFlatDockWidth, int iXOffset);

double cairo_dock_calculate_max_dock_width (CairoDock *pDock, GList *pFirstDrawnElement, double fFlatDockWidth, double fWidthConstraintFactor, double fExtraWidth);

Icon * cairo_dock_calculate_wave_with_position_linear (GList *pIconList, GList *pFirstDrawnElement, int x_abs, gdouble fMagnitude, double fFlatDockWidth, int iWidth, int iHeight, double fAlign, double fLateralFactor, gboolean bDirectionUp);

/** Apply a wave effect on the icons of a linear dock. It is the famous zoom when the mouse hovers an icon.
*@param pDock a linear dock.
*@return the pointed icon, or NULL if none is pointed.
*/
Icon *cairo_dock_apply_wave_effect_linear (CairoDock *pDock);
#define cairo_dock_apply_wave_effect cairo_dock_apply_wave_effect_linear

/** Get the current width of all the icons of a linear dock. It doesn't take into account any decoration or frame, only the space occupied by the icons.
*@param pDock a linear dock.
*/
double cairo_dock_get_current_dock_width_linear (CairoDock *pDock);

/** Check the position of the mouse inside a linear dock. It can be inside, on the edge, or outside. Update the 'iMousePositionType' field.
*@param pDock a linear dock.
*/
void cairo_dock_check_if_mouse_inside_linear (CairoDock *pDock);

void cairo_dock_manage_mouse_position (CairoDock *pDock);

/** Check if one can drop inside a linear dock.
*Drop is allowed between 2 icons of the launchers group, if the user is dragging something over the dock. Update the 'bCanDrop' field.
*@param pDock a linear dock.
*/
void cairo_dock_check_can_drop_linear (CairoDock *pDock);

void cairo_dock_stop_marking_icons (CairoDock *pDock);


void cairo_dock_scroll_dock_icons (CairoDock *pDock, int iScrollAmount);

/** Get the first icon to be drawn inside a linear dock, so that if you draw from left to right, the pointed icon will be drawn at last.
*@param icons a list of icons of a linear dock.
*@return the element of the list that contains the first icon to draw.
*/
GList *cairo_dock_get_first_drawn_element_linear (GList *icons);


G_END_DECLS
#endif
