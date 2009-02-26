
#ifndef __CAIRO_DOCK_ANIMATIONS__
#define  __CAIRO_DOCK_ANIMATIONS__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


gboolean cairo_dock_move_up (CairoDock *pDock);

gboolean cairo_dock_move_down (CairoDock *pDock);

void cairo_dock_pop_up (CairoDock *pDock);

gboolean cairo_dock_pop_down (CairoDock *pDock);


gfloat cairo_dock_calculate_magnitude (gint iMagnitudeIndex);

gboolean cairo_dock_grow_up (CairoDock *pDock);

gboolean cairo_dock_shrink_down (CairoDock *pDock);

gboolean cairo_dock_handle_inserting_removing_icons (CairoDock *pDock);

/**
*Definit s'il est utile de lancer l'animation d'un dock (il est inutile de la lancer s'il est manifestement invisible).
*@param pDock le dock a animer.
*/
#define cairo_dock_animation_will_be_visible(pDock) ((pDock)->bInside || (! (pDock)->bAutoHide && (pDock)->iRefCount == 0) || ! (pDock)->bAtBottom)


void cairo_dock_launch_animation (CairoContainer *pContainer);

void cairo_dock_start_shrinking (CairoDock *pDock);

void cairo_dock_start_growing (CairoDock *pDock);

/**
*Lance l'animation de l'icone. Ne fait rien si l'icone ne sera pas animee.
*@param icon l'icone a animer.
*@param pDock le dock contenant l'icone.
*/
void cairo_dock_start_icon_animation (Icon *icon, CairoDock *pDock);

void cairo_dock_request_icon_animation (Icon *pIcon, CairoDock *pDock, const gchar *cAnimation, int iNbRounds);
#define cairo_dock_stop_icon_animation(pIcon) do { \
	if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REMOVE_INSERT) {\
		cairo_dock_notify (CAIRO_DOCK_STOP_ICON, pIcon); \
		pIcon->iAnimationState = CAIRO_DOCK_STATE_REST; } } while (0)

void cairo_dock_mark_icon_animation_as (Icon *pIcon, CairoDockAnimationState iAnimationState);
void cairo_dock_stop_marking_icon_animation_as (Icon *pIcon, CairoDockAnimationState iAnimationState);

#define cairo_dock_mark_icon_as_hovered_by_mouse(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_MOUSE_HOVERED)
#define cairo_dock_stop_marking_icon_as_hovered_by_mouse(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_MOUSE_HOVERED)

#define cairo_dock_mark_icon_as_clicked(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_CLICKED)
#define cairo_dock_stop_marking_icon_as_clicked(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_CLICKED)

#define cairo_dock_mark_icon_as_avoiding_mouse(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_AVOID_MOUSE)
#define cairo_dock_stop_marking_icon_as_avoiding_mouse(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_AVOID_MOUSE)

#define cairo_dock_mark_icon_as_following_mouse(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_FOLLOW_MOUSE)
#define cairo_dock_stop_marking_icon_as_following_mouse(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_FOLLOW_MOUSE)

#define cairo_dock_mark_icon_as_inserting_removing(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_REMOVE_INSERT)
#define cairo_dock_stop_marking_icon_as_inserting_removing(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_REMOVE_INSERT)


void cairo_dock_update_removing_inserting_icon_size_default (Icon *icon);

gboolean cairo_dock_update_inserting_removing_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bContinueAnimation);
gboolean cairo_dock_on_insert_remove_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock);
gboolean cairo_dock_stop_inserting_removing_icon_notification (gpointer pUserData, Icon *pIcon);

G_END_DECLS
#endif
