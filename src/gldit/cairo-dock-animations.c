/**
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

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <cairo.h>
#include <gtk/gtk.h>

#include "cairo-dock-icon-facility.h"  // cairo_dock_icon_is_being_inserted_or_removed
#include "cairo-dock-dock-facility.h" // input shapes
#include "cairo-dock-draw.h"  // for transitions
#include "cairo-dock-draw-opengl.h"  // idem
#include "cairo-dock-dialog-manager.h"  // cairo_dock_replace_all_dialogs
#include "cairo-dock-applications-manager.h"  // cairo_dock_search_window_overlapping_dock
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-animations.h"

extern gboolean g_bUseOpenGL;
extern CairoDockGLConfig g_openglConfig;
extern CairoDockHidingEffect *g_pHidingBackend;
extern CairoDockHidingEffect *g_pKeepingBelowBackend;

static gboolean _update_fade_out_dock (gpointer pUserData, CairoDock *pDock, gboolean *bContinueAnimation)
{
	pDock->iFadeCounter += (pDock->bFadeInOut ? 1 : -1);  // fade out, puis fade in.
	
	if (pDock->iFadeCounter >= myBackendsParam.iHideNbSteps)
	{
		pDock->bFadeInOut = FALSE;
		//g_print ("set below\n");
		gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), TRUE);
		// si fenetre maximisee, on met direct iFadeCounter a 0.  // malheureusement X met du temps a faire passer le dock derriere, et ca donne un "sursaut" :-/
		///if (cairo_dock_search_window_covering_dock (pDock, FALSE, FALSE) != NULL)
		///	pDock->iFadeCounter = 0;
	}
	
	//g_print ("pDock->iFadeCounter : %d\n", pDock->iFadeCounter);
	if (pDock->iFadeCounter > 0)
	{
		*bContinueAnimation = TRUE;
	}
	else
	{
		cairo_dock_remove_notification_func_on_object (pDock,
			NOTIFICATION_UPDATE,
			(CairoDockNotificationFunc) _update_fade_out_dock,
			NULL);
	}
	
	cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

void cairo_dock_pop_up (CairoDock *pDock)
{
	//g_print ("%s (%d)\n", __func__, pDock->bIsBelow);
	if (pDock->bIsBelow)
	{
		cairo_dock_remove_notification_func_on_object (pDock,
			NOTIFICATION_UPDATE,
			(CairoDockNotificationFunc) _update_fade_out_dock,
			NULL);
		pDock->iFadeCounter = 0;
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		//g_print ("set above\n");
		gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), FALSE);  // keep above
		pDock->bIsBelow = FALSE;
	}
}

void cairo_dock_pop_down (CairoDock *pDock)
{
	//g_print ("%s (%d, %d)\n", __func__, pDock->bIsBelow, pDock->container.bInside);
	if (! pDock->bIsBelow && pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && ! pDock->container.bInside)
	{
		if (cairo_dock_search_window_overlapping_dock (pDock) != NULL)
		{
			pDock->iFadeCounter = 0;
			pDock->bFadeInOut = TRUE;
			cairo_dock_register_notification_on_object (pDock,
				NOTIFICATION_UPDATE,
				(CairoDockNotificationFunc) _update_fade_out_dock,
				CAIRO_DOCK_RUN_FIRST, NULL);
			if (g_pKeepingBelowBackend != NULL && g_pKeepingBelowBackend->init)
				g_pKeepingBelowBackend->init (pDock);
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
		}
		else
		{
			//g_print ("set below\n");
			gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), TRUE);
		}
		pDock->bIsBelow = TRUE;
	}
}


gfloat cairo_dock_calculate_magnitude (gint iMagnitudeIndex)  // merci a Robrob pour le patch !
{
	gfloat tmp= ((gfloat)iMagnitudeIndex)/CAIRO_DOCK_NB_MAX_ITERATIONS;

	if (tmp>0.5f)
		tmp=1.0f-(1.0f-tmp)*(1.0f-tmp)*(1.0f-tmp)*4.0f;
	else
		tmp=tmp*tmp*tmp*4.0f;

	if (tmp<0.0f)
		tmp=0.0f;

	if (tmp>1.0f)
		tmp=1.0f;
	
	return tmp;
}


void cairo_dock_launch_animation (CairoContainer *pContainer)
{
	if (pContainer->iSidGLAnimation == 0 && pContainer->iface.animation_loop != NULL)
	{
		int iAnimationDeltaT = cairo_dock_get_animation_delta_t (pContainer);
		pContainer->bKeepSlowAnimation = TRUE;
		
		pContainer->iSidGLAnimation = g_timeout_add (iAnimationDeltaT, (GSourceFunc)pContainer->iface.animation_loop, pContainer);
	}
}

void cairo_dock_start_shrinking (CairoDock *pDock)
{
	if (! pDock->bIsShrinkingDown)  // on lance l'animation.
	{
		pDock->bIsGrowingUp = FALSE;
		pDock->bIsShrinkingDown = TRUE;
		
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}

void cairo_dock_start_growing (CairoDock *pDock)
{
	//g_print ("%s (%d)\n", __func__, pDock->bIsGrowingUp);
	if (! pDock->bIsGrowingUp)  // on lance l'animation.
	{
		pDock->bIsShrinkingDown = FALSE;
		pDock->bIsGrowingUp = TRUE;
		
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}

void cairo_dock_start_hiding (CairoDock *pDock)
{
	//g_print ("%s (%d)\n", __func__, pDock->bIsHiding);
	if (! pDock->bIsHiding && ! pDock->container.bInside)  // rien de plus desagreable que le dock qui se cache quand on est dedans.
	{
		pDock->bIsShowing = FALSE;
		pDock->bIsHiding = TRUE;
		
		if (pDock->pHiddenShapeBitmap && pDock->iInputState != CAIRO_DOCK_INPUT_HIDDEN)
		{
			//g_print ("+++ input shape hidden on start hiding\n");
			cairo_dock_set_input_shape_hidden (pDock);
			pDock->iInputState = CAIRO_DOCK_INPUT_HIDDEN;
		}
		
		if (g_pHidingBackend != NULL && g_pHidingBackend->init)
			g_pHidingBackend->init (pDock);
		
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}

void cairo_dock_start_showing (CairoDock *pDock)
{
	//g_print ("%s (%d)\n", __func__, pDock->bIsShowing);
	if (! pDock->bIsShowing)  // on lance l'animation.
	{
		pDock->bIsShowing = TRUE;
		pDock->bIsHiding = FALSE;
		
		if (pDock->pShapeBitmap && pDock->iInputState == CAIRO_DOCK_INPUT_HIDDEN)
		{
			//g_print ("+++ input shape at rest on start showing\n");
			cairo_dock_set_input_shape_at_rest (pDock);
			pDock->iInputState = CAIRO_DOCK_INPUT_AT_REST;
			
			cairo_dock_replace_all_dialogs ();
		}
		
		if (g_pHidingBackend != NULL && g_pHidingBackend->init)
			g_pHidingBackend->init (pDock);
		
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}


void cairo_dock_start_icon_animation (Icon *pIcon, CairoDock *pDock)
{
	g_return_if_fail (pIcon != NULL && pDock != NULL);
	cd_message ("%s (%s, %d)", __func__, pIcon->cName, pIcon->iAnimationState);
	
	if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REST &&
		(cairo_dock_icon_is_being_inserted_or_removed (pIcon) || pIcon->bIsDemandingAttention  || cairo_dock_animation_will_be_visible (pDock)))
	{
		//g_print ("  c'est parti\n");
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}

void cairo_dock_request_icon_animation (Icon *pIcon, CairoContainer *pContainer, const gchar *cAnimation, int iNbRounds)
{
	//g_print ("%s (%s, state:%d)\n", __func__, pIcon->cName, pIcon->iAnimationState);
	CairoDock *pDock;
	if (! CAIRO_DOCK_IS_DOCK (pContainer))  // at the moment, only docks can animate their icons
		return;
	else
		pDock = CAIRO_DOCK (pContainer);
	if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REST)  // on le fait avant de changer d'animation, pour le cas ou l'icone ne serait plus placee au meme endroit (rebond).
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	cairo_dock_stop_icon_animation (pIcon);
	
	if (cAnimation == NULL || iNbRounds == 0 || pIcon->iAnimationState != CAIRO_DOCK_STATE_REST)
		return ;
	cairo_dock_notify_on_object (&myIconsMgr, NOTIFICATION_REQUEST_ICON_ANIMATION, pIcon, pDock, cAnimation, iNbRounds);
	cairo_dock_notify_on_object (pIcon, NOTIFICATION_REQUEST_ICON_ANIMATION, pIcon, pDock, cAnimation, iNbRounds);
	cairo_dock_start_icon_animation (pIcon, pDock);
}

void cairo_dock_request_icon_attention (Icon *pIcon, CairoDock *pDock, const gchar *cAnimation, int iNbRounds)
{
	cairo_dock_stop_icon_animation (pIcon);
	pIcon->bIsDemandingAttention = TRUE;
	
	if (iNbRounds <= 0)
		iNbRounds = 1e6;
	if (cAnimation == NULL || *cAnimation == '\0' || strcmp (cAnimation, "default") == 0)
	{
		if (myTaskbarParam.cAnimationOnDemandsAttention != NULL)
			cAnimation = myTaskbarParam.cAnimationOnDemandsAttention;
		else
			cAnimation = "rotate";
	}
	
	cairo_dock_request_icon_animation (pIcon, CAIRO_CONTAINER (pDock), cAnimation, iNbRounds);
	cairo_dock_mark_icon_as_clicked (pIcon);  // pour eviter qu'un simple survol ne stoppe l'animation.
	
	// on reporte la demande d'attention recursivement vers le bas.
	if (pDock->iRefCount > 0)
	{
		CairoDock *pParentDock = NULL;
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
		if (pPointingIcon != NULL)
		{
			cairo_dock_request_icon_attention (pPointingIcon, pParentDock, cAnimation, iNbRounds);
		}
	}
	else if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && pDock->bIsBelow)
		cairo_dock_pop_up (pDock);
}

void cairo_dock_stop_icon_attention (Icon *pIcon, CairoDock *pDock)
{
	cairo_dock_stop_icon_animation (pIcon);
	//cairo_dock_redraw_icon (pIcon, CAIRO_CONTAINER (pDock));  // a faire avant, lorsque l'icone est encore en mode demande d'attention.
	pIcon->bIsDemandingAttention = FALSE;
	gtk_widget_queue_draw (pDock->container.pWidget);  // redraw all the dock, since the animation of the icon can be larger than the icon itself.
	
	// on stoppe la demande d'attention recursivement vers le bas.
	if (pDock->iRefCount > 0)
	{
		GList *ic;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->bIsDemandingAttention)
				break;
		}
		if (ic == NULL)  // plus aucune animation dans ce dock.
		{
			CairoDock *pParentDock = NULL;
			Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
			if (pPointingIcon != NULL)
			{
				cairo_dock_stop_icon_attention (pPointingIcon, pParentDock);
			}
		}
	}
	else if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && ! pDock->bIsBelow && ! pDock->container.bInside)
	{
		cairo_dock_pop_down (pDock);
	}
}


void cairo_dock_trigger_icon_removal_from_dock (Icon *pIcon)
{
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pDock != NULL)
	{
		cairo_dock_stop_icon_animation (pIcon);
		if (cairo_dock_animation_will_be_visible (pDock))  // sinon inutile de se taper toute l'animation.
			pIcon->fInsertRemoveFactor = 1.0;
		else
			pIcon->fInsertRemoveFactor = 0.05;
		cairo_dock_notify_on_object (&myDocksMgr, NOTIFICATION_REMOVE_ICON, pIcon, pDock);
		cairo_dock_notify_on_object (pDock, NOTIFICATION_REMOVE_ICON, pIcon, pDock);
		cairo_dock_start_icon_animation (pIcon, pDock);
	}
}


void cairo_dock_mark_icon_animation_as (Icon *pIcon, CairoDockAnimationState iAnimationState)
{
	if (pIcon->iAnimationState < iAnimationState)
	{
		pIcon->iAnimationState = iAnimationState;
	}
}
void cairo_dock_stop_marking_icon_animation_as (Icon *pIcon, CairoDockAnimationState iAnimationState)
{
	if (pIcon->iAnimationState == iAnimationState)
	{
		pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
	}
}


void cairo_dock_update_removing_inserting_icon_size_default (Icon *icon)
{
	icon->fInsertRemoveFactor *= .85;
	if (icon->fInsertRemoveFactor > 0)
	{
		if (icon->fInsertRemoveFactor < 0.05)
			icon->fInsertRemoveFactor = 0.05;
	}
	else if (icon->fInsertRemoveFactor < 0)
	{
		if (icon->fInsertRemoveFactor > -0.05)
			icon->fInsertRemoveFactor = -0.05;
	}
}


gboolean cairo_dock_update_inserting_removing_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bContinueAnimation)
{
	if (pIcon->iGlideDirection != 0)
	{
		pIcon->fGlideOffset += pIcon->iGlideDirection * .1;
		if (fabs (pIcon->fGlideOffset) > .99)
		{
			pIcon->fGlideOffset = pIcon->iGlideDirection;
			pIcon->iGlideDirection = 0;
		}
		else if (fabs (pIcon->fGlideOffset) < .01)
		{
			pIcon->fGlideOffset = 0;
			pIcon->iGlideDirection = 0;
		}
		*bContinueAnimation = TRUE;
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	}
	
	if (pIcon->fInsertRemoveFactor == 0 || ! pIcon->bBeingRemovedByCairo)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	cairo_dock_update_removing_inserting_icon_size_default (pIcon);
	
	if (fabs (pIcon->fInsertRemoveFactor) > 0.05)
	{
		cairo_dock_mark_icon_as_inserting_removing (pIcon);
		*bContinueAnimation = TRUE;
	}
	cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_on_insert_remove_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock)
{
	if (pIcon->iAnimationState == CAIRO_DOCK_STATE_REMOVE_INSERT)  // already in insert/remove state
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	cairo_dock_mark_icon_as_inserting_removing (pIcon);  // On prend en charge le dessin de l'icone pendant sa phase d'insertion/suppression.
	
	///if (fabs (pIcon->fInsertRemoveFactor) < .1)  // useless or not needed animation.
	///	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	pIcon->bBeingRemovedByCairo = TRUE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_stop_inserting_removing_icon_notification (gpointer pUserData, Icon *pIcon)
{
	pIcon->fGlideOffset = 0;
	pIcon->iGlideDirection = 0;
	pIcon->bBeingRemovedByCairo = FALSE;
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}



#define cairo_dock_get_transition(pIcon) (pIcon)->pTransition

#define cairo_dock_set_transition(pIcon, transition) (pIcon)->pTransition = transition

static gboolean _cairo_dock_transition_step (gpointer pUserData, Icon *pIcon, CairoContainer *pContainer, gboolean *bContinueAnimation)
{
	CairoDockTransition *pTransition = cairo_dock_get_transition (pIcon);
	if (pTransition == NULL)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	pTransition->iCount ++;
	int iDetlaT = (pTransition->bFastPace ? cairo_dock_get_animation_delta_t (pContainer) : cairo_dock_get_slow_animation_delta_t (pContainer));
	//int iNbSteps = 1.*pTransition->iDuration / iDetlaT;
	pTransition->iElapsedTime += iDetlaT;
	
	if (! pTransition->bRemoveWhenFinished && pTransition->iDuration != 0 && pTransition->iElapsedTime > pTransition->iDuration)  // skip
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	gboolean bContinue = FALSE;
	if (CAIRO_CONTAINER_IS_OPENGL (pTransition->pContainer))
	{
		if (pTransition->render_opengl)
		{
			if (! cairo_dock_begin_draw_icon (pIcon, pContainer, 0))
				return CAIRO_DOCK_LET_PASS_NOTIFICATION;
			bContinue = pTransition->render_opengl (pIcon, pTransition->pUserData);
			cairo_dock_end_draw_icon (pIcon, pContainer);
		}
		else if (pTransition->render != NULL)
		{
			bContinue = pTransition->render (pIcon, pTransition->pUserData);
			cairo_dock_update_icon_texture (pIcon);
		}
	}
	else if (pTransition->render != NULL)
	{
		bContinue = pTransition->render (pIcon, pTransition->pUserData);
		if (pContainer->bUseReflect)
			cairo_dock_add_reflection_to_icon (pIcon, pContainer);
	}
	
	cairo_dock_redraw_icon (pIcon, pContainer);
	
	if (pTransition->iDuration != 0 && pTransition->iElapsedTime >= pTransition->iDuration)
		bContinue = FALSE;
	
	if (! bContinue)
	{
		if (pTransition->bRemoveWhenFinished)
			cairo_dock_remove_transition_on_icon (pIcon);
	}
	else
	{
		*bContinueAnimation = TRUE;
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}
void cairo_dock_set_transition_on_icon (Icon *pIcon, CairoContainer *pContainer, CairoDockTransitionRenderFunc render_step_cairo, CairoDockTransitionGLRenderFunc render_step_opengl, gboolean bFastPace, gint iDuration, gboolean bRemoveWhenFinished, gpointer pUserData, GFreeFunc pFreeUserDataFunc)
{
	cairo_dock_remove_transition_on_icon (pIcon);
	
	CairoDockTransition *pTransition = g_new0 (CairoDockTransition, 1);
	pTransition->render = render_step_cairo;
	pTransition->render_opengl = render_step_opengl;
	pTransition->bFastPace = bFastPace;
	pTransition->iDuration = iDuration;
	pTransition->bRemoveWhenFinished = bRemoveWhenFinished;
	pTransition->pContainer = pContainer;
	pTransition->pUserData = pUserData;
	pTransition->pFreeUserDataFunc = pFreeUserDataFunc;
	cairo_dock_set_transition (pIcon, pTransition);
	
	cairo_dock_register_notification_on_object (pIcon,
		bFastPace ? NOTIFICATION_UPDATE_ICON : NOTIFICATION_UPDATE_ICON_SLOW,
		(CairoDockNotificationFunc) _cairo_dock_transition_step,
		CAIRO_DOCK_RUN_AFTER, pUserData);
	
	cairo_dock_launch_animation (pContainer);
}

void cairo_dock_remove_transition_on_icon (Icon *pIcon)
{
	CairoDockTransition *pTransition = cairo_dock_get_transition (pIcon);
	if (pTransition == NULL)
		return ;
	
	cairo_dock_remove_notification_func_on_object (pIcon,
		pTransition->bFastPace ? NOTIFICATION_UPDATE_ICON : NOTIFICATION_UPDATE_ICON_SLOW,
		(CairoDockNotificationFunc) _cairo_dock_transition_step,
		pTransition->pUserData);
	
	if (pTransition->pFreeUserDataFunc != NULL)
		pTransition->pFreeUserDataFunc (pTransition->pUserData);
	g_free (pTransition);
	cairo_dock_set_transition (pIcon, NULL);
}
