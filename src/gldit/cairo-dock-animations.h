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

#ifndef __CAIRO_DOCK_ANIMATIONS__
#define  __CAIRO_DOCK_ANIMATIONS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-icon-factory.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-animations.h This class handles the icons and containers animations.
* Each container has a rendering loop. An iteration of this loop is separated in 2 phases : the update of each element of the container and of the container itself, and the redraw of each element and of the container itself.
* The loop has 2 possible frequencies : fast (~33Hz) and slow (~10Hz), to optimize the CPU load according to the needs of the animation.
* To be called on each iteration of the loop, you register to the CAIRO_DOCK_UPDATE_X or CAIRO_DOCK_UPDATE_X_SLOW, where X is either ICON, DOCK, DESKLET, DIALOG or FLYING_CONTAINER.
* If you need to draw things directly on the container, you register to CAIRO_DOCK_RENDER_X, where X is either ICON, DOCK, DESKLET, DIALOG or FLYING_CONTAINER.
*/

/// callback to render the icon with libcairo at each step of the Transition.
typedef gboolean (*CairoDockTransitionRenderFunc) (Icon *pIcon, gpointer pUserData);
/// callback to render the icon with OpenGL at each step of the Transition.
typedef gboolean (*CairoDockTransitionGLRenderFunc) (Icon *pIcon, gpointer pUserData);

/// Transitions are an easy way to set an animation on an Icon to make it change from a state to another.
struct _CairoDockTransition {
	/// the cairo rendering function.
	CairoDockTransitionRenderFunc render;
	/// the openGL rendering function (can be NULL, in which case the texture mapping from the cairo drawing is done automatically).
	CairoDockTransitionGLRenderFunc render_opengl;
	/// data passed to the rendering functions.
	gpointer pUserData;
	/// function called to destroy the data when the transition is deleted.
	GFreeFunc pFreeUserDataFunc;
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
	/// Container of the Icon.
	GldiContainer *pContainer;  // keep it up-to-date!
	};

/// Definition of a Hiding Effect backend (used to provide an animation when the docks hides/shows itself).
struct _CairoDockHidingEffect {
	/// translated name of the effect
	const gchar *cDisplayedName;
	/// whether the backend can display the dock even when it's hidden
	gboolean bCanDisplayHiddenDock;
	/// function called before the icons are drawn (cairo)
	void (*pre_render) (CairoDock *pDock, double fOffset, cairo_t *pCairoContext);
	/// function called before the icons are drawn (opengl)
	void (*pre_render_opengl) (CairoDock *pDock, double fOffset);
	/// function called afer the icons are drawn (cairo)
	void (*post_render) (CairoDock *pDock, double fOffset, cairo_t *pCairoContext);
	/// function called afer the icons are drawn (opengl)
	void (*post_render_opengl) (CairoDock *pDock, double fOffset);
	/// function called when the animation is started.
	void (*init) (CairoDock *pDock);
	};
	
#define CAIRO_DOCK_MIN_SLOW_DELTA_T 90

/** Say if a container is currently animated.
*@param pContainer a Container
*/
#define cairo_dock_container_is_animating(pContainer) (CAIRO_CONTAINER(pContainer)->iSidGLAnimation != 0)

/** Say if it's usefull to launch an animation on a Dock (indeed, it's useless to launch it if it will be invisible).
*@param pDock the Dock to animate.
*/
#define cairo_dock_animation_will_be_visible(pDock) ( \
((pDock)->iRefCount != 0 && gldi_container_is_visible (CAIRO_CONTAINER(pDock))) \
|| ((pDock)->iRefCount == 0 && (! (pDock)->bAutoHide || CAIRO_CONTAINER(pDock)->bInside || (pDock)->fHideOffset < 1)) \
)


/** Pop up a Dock above other windows, if it is in mode "keep below other windows"; otherwise do nothing.
*@param pDock the dock.
*/
void cairo_dock_pop_up (CairoDock *pDock);

/** Pop down a Dock below other windows, if it is in mode "keep below other windows"; otherwise do nothing.
*@param pDock the dock.
*/
void cairo_dock_pop_down (CairoDock *pDock);


gfloat cairo_dock_calculate_magnitude (gint iMagnitudeIndex);

/** Launch the animation of a Container.
*@param pContainer the container to animate.
*/
void cairo_dock_launch_animation (GldiContainer *pContainer);

void cairo_dock_start_shrinking (CairoDock *pDock);

void cairo_dock_start_growing (CairoDock *pDock);

void cairo_dock_start_hiding (CairoDock *pDock);

void cairo_dock_start_showing (CairoDock *pDock);

/** Start the animation of an Icon. Do nothing if the icon is at rest or if the animation won't be visible.
*@param icon the icon to animate.
*/
void gldi_icon_start_animation (Icon *icon);

/** Launch a given animation on an Icon. Do nothing if the icon will not be animated or if the animation doesn't exist.
*@param pIcon the icon to animate.
*@param cAnimation name of the animation.
*@param iNbRounds number of rounds the animation will be played.
*/
void gldi_icon_request_animation (Icon *pIcon, const gchar *cAnimation, int iNbRounds);

/** Stop any animation on an Icon, except the disappearance/appearance animation.
*@param pIcon the icon.
*/
#define gldi_icon_stop_animation(pIcon) do { \
	if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REMOVE_INSERT && pIcon->iAnimationState != CAIRO_DOCK_STATE_REST) {\
		gldi_object_notify (pIcon, NOTIFICATION_STOP_ICON, pIcon); \
		pIcon->iAnimationState = CAIRO_DOCK_STATE_REST; } } while (0)

void gldi_icon_request_attention (Icon *pIcon, const gchar *cAnimation, int iNbRounds);

void gldi_icon_stop_attention (Icon *pIcon);

/** Trigger the removal of an Icon from its Dock. The icon will effectively be removed at the end of the animation.
*If the icon is not inside a dock, nothing happens.
*@param pIcon the icon to remove
*/
void cairo_dock_trigger_icon_removal_from_dock (Icon *pIcon);

/** Get the interval of time between 2 iterations of the fast loop (in ms).
*@param pContainer the container.
*/
#define cairo_dock_get_animation_delta_t(pContainer) CAIRO_CONTAINER(pContainer)->iAnimationDeltaT
/** Get the interval of time between 2 iterations of the slow loop (in ms).
*@param pContainer the container.
*/
#define cairo_dock_get_slow_animation_delta_t(pContainer) ((int) ceil (1.*CAIRO_DOCK_MIN_SLOW_DELTA_T / CAIRO_CONTAINER(pContainer)->iAnimationDeltaT) * CAIRO_CONTAINER(pContainer)->iAnimationDeltaT)

void cairo_dock_mark_icon_animation_as (Icon *pIcon, CairoDockAnimationState iAnimationState);
void cairo_dock_stop_marking_icon_animation_as (Icon *pIcon, CairoDockAnimationState iAnimationState);

#define cairo_dock_mark_icon_as_hovered_by_mouse(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_MOUSE_HOVERED)
#define cairo_dock_stop_marking_icon_as_hovered_by_mouse(pIcon) cairo_dock_stop_marking_icon_animation_as (pIcon, CAIRO_DOCK_STATE_MOUSE_HOVERED)

#define cairo_dock_mark_icon_as_clicked(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_CLICKED)
#define cairo_dock_stop_marking_icon_as_clicked(pIcon) cairo_dock_stop_marking_icon_animation_as (pIcon, CAIRO_DOCK_STATE_CLICKED)

#define cairo_dock_mark_icon_as_avoiding_mouse(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_AVOID_MOUSE)
#define cairo_dock_stop_marking_icon_as_avoiding_mouse(pIcon) cairo_dock_stop_marking_icon_animation_as (pIcon, CAIRO_DOCK_STATE_AVOID_MOUSE)

#define cairo_dock_mark_icon_as_following_mouse(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_FOLLOW_MOUSE)
#define cairo_dock_stop_marking_icon_as_following_mouse(pIcon) cairo_dock_stop_marking_icon_animation_as (pIcon, CAIRO_DOCK_STATE_FOLLOW_MOUSE)

#define cairo_dock_mark_icon_as_inserting_removing(pIcon) cairo_dock_mark_icon_animation_as (pIcon, CAIRO_DOCK_STATE_REMOVE_INSERT)
#define cairo_dock_stop_marking_icon_as_inserting_removing(pIcon) cairo_dock_stop_marking_icon_animation_as (pIcon, CAIRO_DOCK_STATE_REMOVE_INSERT)


/** Set a Transition on an Icon.
*@param pIcon the icon.
*@param pContainer the Container of the Icon. It will be shared with the transition.
*@param render_step_cairo the cairo rendering function.
*@param render_step_opengl the openGL rendering function (can be NULL, in which case the texture mapping from the cairo drawing is done automatically).
*@param bFastPace TRUE for a high frequency refresh (this uses of course more CPU).
*@param iDuration duration if the transition, in ms. Can be 0 for an endless transition, in which case you can stop the transition with #cairo_dock_remove_transition_on_icon.
*@param bRemoveWhenFinished TRUE to destroy and remove the transition when it is finished.
*@param pUserData data passed to the rendering functions.
*@param pFreeUserDataFunc function called to free the user data when the transition is destroyed (optionnal).
*/
void cairo_dock_set_transition_on_icon (Icon *pIcon, GldiContainer *pContainer, CairoDockTransitionRenderFunc render_step_cairo, CairoDockTransitionGLRenderFunc render_step_opengl, gboolean bFastPace, gint iDuration, gboolean bRemoveWhenFinished, gpointer pUserData, GFreeFunc pFreeUserDataFunc);

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
