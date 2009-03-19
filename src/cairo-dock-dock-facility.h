
#ifndef __CAIRO_DOCK_FACILITY__
#define  __CAIRO_DOCK_FACILITY__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-accessibility.h"
G_BEGIN_DECLS


/** Teste si le container est un dock.
* @param pContainer le container.
* @return TRUE ssi le container a ete declare comme un dock.
*/
#define CAIRO_DOCK_IS_DOCK(pContainer) (pContainer != NULL && (pContainer)->iType == CAIRO_DOCK_TYPE_DOCK)
/** Caste un container en dock.
* @param pContainer le container.
* @return le dock.
*/
#define CAIRO_DOCK(pContainer) ((CairoDock *)pContainer)

/** Retourne la largeur max autorisee pour un dock.
* @param pDock le dock.
* @return la taille max.
*/
#define cairo_dock_get_max_authorized_dock_width(pDock) (myAccessibility.iMaxAuthorizedWidth == 0 ? g_iScreenWidth[pDock->bHorizontalDock] : MIN (myAccessibility.iMaxAuthorizedWidth, g_iScreenWidth[pDock->bHorizontalDock]))


/**
* Recharge les reflets (cairo) d'un dock. Utile si le dock a changé de position.
* @param pDock un dock.
*/
void cairo_dock_reload_reflects_in_dock (CairoDock *pDock);


/**
* Recalcule la taille maximale du dock, si par exemple une icone a ete enlevee/rajoutee. Met a jour la taille des decorations si necessaire et adapte a la taille max imposee.
* Le dock est deplace de maniere a rester centre sur la meme position, et les coordonnees des icones des applis sont recalculees et renvoyees au WM.
* @param pDock le dock.
*/
void cairo_dock_update_dock_size (CairoDock *pDock);

Icon *cairo_dock_calculate_dock_icons (CairoDock *pDock);


/**
* Demande au WM d'empecher les autres fenetres d'empieter sur l'espace du dock.
* L'espace reserve est pris sur la taille minimale du dock, c'est-a-dire la taille de la zone de rappel si l'auto-hide est active,
* ou la taille du dock au repos sinon.
* @param pDock le dock.
* @param bReserve TRUE pour reserver l'espace, FALSE pour annuler la reservation.
*/
void cairo_dock_reserve_space_for_dock (CairoDock *pDock, gboolean bReserve);

/**
* Met un dock principal a sa taille et a sa place initiale.
* @param pDock le dock.
*/
void cairo_dock_place_root_dock (CairoDock *pDock);
/**
* Borne la position d'un dock a l'interieur de l'ecran.
* @param pDock le dock.
*/
void cairo_dock_prevent_dock_from_out_of_screen (CairoDock *pDock);


void cairo_dock_set_window_position_at_balance (CairoDock *pDock, int iNewWidth, int iNewHeight);

void cairo_dock_get_window_position_and_geometry_at_balance (CairoDock *pDock, CairoDockSizeType iSizeType, int *iNewWidth, int *iNewHeight);

void cairo_dock_set_subdock_position_linear (Icon *pPointedIcon, CairoDock *pParentDock);


void cairo_dock_set_input_shape (CairoDock *pDock);
void cairo_dock_unset_input_shape (CairoDock *pDock);


GList * cairo_dock_calculate_icons_positions_at_rest_linear (GList *pIconList, double fFlatDockWidth, int iXOffset);

double cairo_dock_calculate_max_dock_width (CairoDock *pDock, GList *pFirstDrawnElement, double fFlatDockWidth, double fWidthConstraintFactor, double fExtraWidth);

Icon * cairo_dock_calculate_wave_with_position_linear (GList *pIconList, GList *pFirstDrawnElement, int x_abs, gdouble fMagnitude, double fFlatDockWidth, int iWidth, int iHeight, double fAlign, double fLateralFactor, gboolean bDirectionUp);

Icon *cairo_dock_apply_wave_effect_linear (CairoDock *pDock);
#define cairo_dock_apply_wave_effect cairo_dock_apply_wave_effect_linear
double cairo_dock_get_current_dock_width_linear (CairoDock *pDock);


void cairo_dock_check_if_mouse_inside_linear (CairoDock *pDock);

void cairo_dock_manage_mouse_position (CairoDock *pDock);

void cairo_dock_check_can_drop_linear (CairoDock *pDock);

void cairo_dock_stop_marking_icons (CairoDock *pDock);


void cairo_dock_scroll_dock_icons (CairoDock *pDock, int iScrollAmount);


G_END_DECLS
#endif
