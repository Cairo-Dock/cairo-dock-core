
#ifndef __CAIRO_DOCK_ANIMATIONS__
#define  __CAIRO_DOCK_ANIMATIONS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-icons.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-animations.h This class handles the icons and containers animations.
*/

typedef gboolean (*CairoDockTransitionRenderFunc) (gpointer pUserData, cairo_t *pIconContext);
typedef gboolean (*CairoDockTransitionGLRenderFunc) (gpointer pUserData);
struct _CairoDockTransition {
	CairoDockTransitionRenderFunc render;
	CairoDockTransitionGLRenderFunc render_opengl;
	gpointer pUserData;
	gboolean bFastPace;
	gboolean bRemoveWhenFinished;
	gint iDuration;  // en ms.
	gint iElapsedTime;
	gint iCount;
	cairo_t *pIconContext;  // attention a bien detruire la transition
	CairoContainer *pContainer;  // si l'un de ces 2 parametres change !
	} ;

#define CAIRO_DOCK_MIN_SLOW_DELTA_T 90

gboolean cairo_dock_move_up (CairoDock *pDock);

gboolean cairo_dock_move_down (CairoDock *pDock);

void cairo_dock_pop_up (CairoDock *pDock);

gboolean cairo_dock_pop_down (CairoDock *pDock);


gfloat cairo_dock_calculate_magnitude (gint iMagnitudeIndex);

gboolean cairo_dock_grow_up (CairoDock *pDock);

gboolean cairo_dock_shrink_down (CairoDock *pDock);

gboolean cairo_dock_handle_inserting_removing_icons (CairoDock *pDock);

/** Definit s'il est utile de lancer l'animation d'un dock (il est inutile de la lancer s'il est manifestement invisible).
*@param pDock le dock a animer.
*/
#define cairo_dock_animation_will_be_visible(pDock) ((pDock)->bInside || (! (pDock)->bAutoHide && (pDock)->iRefCount == 0) || ! (pDock)->bAtBottom)

#define cairo_dock_container_is_animating(pContainer) ((pContainer)->iSidGLAnimation != 0)

/** Lance l'animation du container. Ne fait rien si l'animation ne sera pas visible (dock cache).
*@param pContainer le container a animer.
*/
void cairo_dock_launch_animation (CairoContainer *pContainer);

void cairo_dock_start_shrinking (CairoDock *pDock);

void cairo_dock_start_growing (CairoDock *pDock);

/** Lance l'animation de l'icone. Ne fait rien si l'icone ne sera pas animee.
*@param icon l'icone a animer.
*@param pDock le dock contenant l'icone.
*/
void cairo_dock_start_icon_animation (Icon *icon, CairoDock *pDock);

/** Lance une animation donnee sur l'icone. Ne fait rien si l'icone ne sera pas animee ou si l'animation n'existe pas
*@param pIcon l'icone a animer.
*@param pDock le dock contenant l'icone.
*@param cAnimation le nom de l'animation.
*@param iNbRounds le nombre de fois que l'animation sera jouee.
*/
void cairo_dock_request_icon_animation (Icon *pIcon, CairoDock *pDock, const gchar *cAnimation, int iNbRounds);
/** Stoppe toute animation sur l'icone, sauf l'animation de disparition/apparition.
*@param pIcon l'icone eventuellement en cours d'animation.
*/
#define cairo_dock_stop_icon_animation(pIcon) do { \
	if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REMOVE_INSERT) {\
		cairo_dock_notify (CAIRO_DOCK_STOP_ICON, pIcon); \
		pIcon->iAnimationState = CAIRO_DOCK_STATE_REST; } } while (0)

/** Renvoie l'intervalle de temps entre 2 etapes de l'animation rapide (en ms).
*@param pContainer le container.
*/
#define cairo_dock_get_animation_delta_t(pContainer) (pContainer)->iAnimationDeltaT
/** Renvoie l'intervalle de temps entre 2 etapes de l'animation lente (en ms).
*@param pContainer le container.
*/
#define cairo_dock_get_slow_animation_delta_t(pContainer) ((int) ceil (1.*CAIRO_DOCK_MIN_SLOW_DELTA_T / (pContainer)->iAnimationDeltaT) * (pContainer)->iAnimationDeltaT)

#define cairo_dock_set_default_animation_delta_t(pContainer) (pContainer)->iAnimationDeltaT = (g_bUseOpenGL ? mySystem.iGLAnimationDeltaT : mySystem.iCairoAnimationDeltaT)

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


void cairo_dock_set_transition_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pIconContext, CairoDockTransitionRenderFunc render_step_cairo, CairoDockTransitionGLRenderFunc render_step_opengl, gboolean bFastPace, gint iDuration, gboolean bRemoveWhenFinished, gpointer pUserData);

void cairo_dock_remove_transition_on_icon (Icon *pIcon);

#define cairo_dock_has_transition(pIcon) ((pIcon)->pTransition != NULL)

#define cairo_dock_get_transition_count(pIcon) (pIcon)->pTransition->iCount

#define cairo_dock_get_transition_elapsed_time(pIcon) (pIcon)->pTransition->iElapsedTime

#define cairo_dock_get_transition_fraction(pIcon) ((pIcon)->pTransition->iDuration ? 1.*(pIcon)->pTransition->iElapsedTime / (pIcon)->pTransition->iDuration : 0)


G_END_DECLS
#endif
