
#ifndef __CAIRO_DOCK_ANIMATIONS__
#define  __CAIRO_DOCK_ANIMATIONS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-icons.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-animations.h This class handles the icons and containers animations.
* Each container has a rendering loop. An iteration of this loop is separated in 2 phases : the update of each element of the container and of the container itself, and the redraw of each element and of the container itself.
* The loop has 2 possible frequencies : fast (~33Hz) and slow (~10Hz), to optimize the CPU load.
* To be called on each iteration of the loop, you register to the CAIRO_DOCK_UPDATE_X or CAIRO_DOCK_UPDATE_X_SLOW, and CAIRO_DOCK_RENDER_X, where X is either ICON, DOCK, DESKLET, DIALOG or FLYING_CONTAINER. 
*/

/// callback to render the icon with libcairo at each step of the Transition.
typedef gboolean (*CairoDockTransitionRenderFunc) (gpointer pUserData, cairo_t *pIconContext);
/// callback to render the icon with OpenGL at each step of the Transition.
typedef gboolean (*CairoDockTransitionGLRenderFunc) (gpointer pUserData);

/// Transitions are an easy way to set an animation on an Icon to make it change from a state to another.
struct _CairoDockTransition {
	/// the cairo rendering function.
	CairoDockTransitionRenderFunc render;
	/// the openGL rendering function (can be NULL, in which case the texture mapping from the cairo drawing is done automatically).
	CairoDockTransitionGLRenderFunc render_opengl;
	/// data passed to the rendering functions.
	gpointer pUserData;
	/// TRUE <=> the transition will be in the fast loop (high frequency refresh).
	gboolean bFastPace;
	/// TRUE <=> the transition will be destroyed and removed from the icon when finished.
	gboolean bRemoveWhenFinished;
	/// duration if the transition, in ms. Can be 0 for an endless transition.
	gint iDuration;  // en ms.
	/// elapsed time since the beginning of the transition, in ms.
	gint iElapsedTime;
	/// number of setps since the beginning of the transition, in ms.
	gint iCount;
	/// cairo context on the Icon.
	cairo_t *pIconContext;  // attention a bien detruire la transition
	/// Container of the Icon.
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

/** Say if it's usefull to launch an animation on a Dock (indeed, it's useless to launch it if it's invisible).
*@param pDock the dock to animate.
*/
#define cairo_dock_animation_will_be_visible(pDock) ((pDock)->bInside || (! (pDock)->bAutoHide && (pDock)->iRefCount == 0) || ! (pDock)->bAtBottom)

#define cairo_dock_container_is_animating(pContainer) ((pContainer)->iSidGLAnimation != 0)

/** Launch the animation of a Container. Do nothing if the container is invisible (hidden dock).
*@param pContainer the container to animate.
*/
void cairo_dock_launch_animation (CairoContainer *pContainer);

void cairo_dock_start_shrinking (CairoDock *pDock);

void cairo_dock_start_growing (CairoDock *pDock);

/** Launch the animation of an Icon. Do nothing if the icon will not be animated.
*@param icon the icon to animate.
*@param pDock the dock containing the icon.
*/
void cairo_dock_start_icon_animation (Icon *icon, CairoDock *pDock);

/** Launch a given animation on an Icon. Do nothing if the icon will not be animated or if the animation doesn't exist.
*@param pIcon the icon to animate.
*@param pDock the dock containing the icon.
*@param cAnimation name of the animation.
*@param iNbRounds number of rounds the animation will be played.
*/
void cairo_dock_request_icon_animation (Icon *pIcon, CairoDock *pDock, const gchar *cAnimation, int iNbRounds);

/** Stop any animation on an Icon, except the disappearance/appearance animation.
*@param pIcon the icon.
*/
#define cairo_dock_stop_icon_animation(pIcon) do { \
	if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REMOVE_INSERT) {\
		cairo_dock_notify (CAIRO_DOCK_STOP_ICON, pIcon); \
		pIcon->iAnimationState = CAIRO_DOCK_STATE_REST; } } while (0)

/** Get the interval of time between 2 iterations of the fast loop (in ms).
*@param pContainer the container.
*/
#define cairo_dock_get_animation_delta_t(pContainer) (pContainer)->iAnimationDeltaT
/** Get the interval of time between 2 iterations of the slow loop (in ms).
*@param pContainer the container.
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


/** Set a Transition on an Icon.
*@param pIcon the icon.
*@param pContainer the Container of the Icon. It will be shared with the transition.
*@param pIconContext a cairo context on the Icon. It will be shared with the transition.
*@param render_step_cairo the cairo rendering function.
*@param render_step_opengl the openGL rendering function (can be NULL, in which case the texture mapping from the cairo drawing is done automatically).
*@param bFastPace TRUE for a high frequency refresh (this uses of course more CPU).
*@param iDuration duration if the transition, in ms. Can be 0 for an endless transition, in which case you can stop the transition with #cairo_dock_remove_transition_on_icon.
*@param bRemoveWhenFinished TRUE to destroy and remove the transition when it is finished.
*@param pUserData data passed to the rendering functions.
*/
void cairo_dock_set_transition_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pIconContext, CairoDockTransitionRenderFunc render_step_cairo, CairoDockTransitionGLRenderFunc render_step_opengl, gboolean bFastPace, gint iDuration, gboolean bRemoveWhenFinished, gpointer pUserData);

/** Stop and remove the Transition of an Icon.
*@param pIcon the icon.
*/
void cairo_dock_remove_transition_on_icon (Icon *pIcon);

/** Say if an Icon has a Transition.
*@param pIcon the icon.
*@return TRUE if the icon has a Transition.
*/
#define cairo_dock_has_transition(pIcon) ((pIcon)->pTransition != NULL)

/** Get the the elpased number of steps since the beginning of the transition.
*@param pIcon the icon.
*@return the elpased number of steps.
*/
#define cairo_dock_get_transition_count(pIcon) (pIcon)->pTransition->iCount

/** Get the elapsed time (in ms) since the beginning of the transition.
*@param pIcon the icon.
*@return the elapsed time.
*/
#define cairo_dock_get_transition_elapsed_time(pIcon) (pIcon)->pTransition->iElapsedTime

/** Get the percentage of the elapsed time (between 0 and 1) since the beginning of the transition, if the transition has a fixed duration (otherwise 0).
*@param pIcon the icon.
*@return the elapsed time in [0,1].
*/
#define cairo_dock_get_transition_fraction(pIcon) ((pIcon)->pTransition->iDuration ? 1.*(pIcon)->pTransition->iElapsedTime / (pIcon)->pTransition->iDuration : 0)


G_END_DECLS
#endif
