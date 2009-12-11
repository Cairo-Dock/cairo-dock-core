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

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "cairo-dock-icons.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-container.h"
#include "cairo-dock-flying-container.h"
#include "cairo-dock-load.h"
#include "cairo-dock-animations.h"

extern int g_iXScreenHeight[2];
extern gboolean g_bUseOpenGL;
extern CairoDock *g_pMainDock;


static gboolean _render_fade_out_dock (gpointer pUserData, CairoDock *pDock, cairo_t *pCairoContext)
{
	double fAlpha = (double) pDock->iFadeCounter / mySystem.iFadeOutNbSteps;
	if (pCairoContext != NULL)
	{
		cairo_rectangle (pCairoContext,
			0,
			0,
			pDock->container.bIsHorizontal ? pDock->container.iWidth : pDock->container.iHeight, pDock->container.bIsHorizontal ? pDock->container.iHeight : pDock->container.iWidth);
		cairo_set_line_width (pCairoContext, 0);
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_DEST_OUT);
		cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 1. - fAlpha);
		cairo_fill (pCairoContext);
	}
	else
	{
		glAccum (GL_LOAD, fAlpha);
		glAccum (GL_RETURN, 1.0);
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}
static gboolean _update_fade_out_dock (gpointer pUserData, CairoDock *pDock, gboolean *bContinueAnimation)
{
	pDock->iFadeCounter += (pDock->bFadeInOut ? -1 : 1);  // fade out, puis fade in.
	
	if (pDock->iFadeCounter == 0)
	{
		pDock->bFadeInOut = FALSE;
		gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), TRUE);
		/// si fenetre maximisee, mettre direct iFadeCounter au max...
		if (cairo_dock_search_window_on_our_way (pDock, TRUE, TRUE) != NULL)
			pDock->iFadeCounter = mySystem.iFadeOutNbSteps;
	}
	
	if (pDock->iFadeCounter < mySystem.iFadeOutNbSteps)
	{
		*bContinueAnimation = TRUE;
	}
	else
	{
		cairo_dock_remove_notification_func_on_container (CAIRO_CONTAINER (pDock),
			CAIRO_DOCK_RENDER_DOCK,
			(CairoDockNotificationFunc) _render_fade_out_dock,
			NULL);
		cairo_dock_remove_notification_func_on_container (CAIRO_CONTAINER (pDock),
			CAIRO_DOCK_UPDATE_DOCK,
			(CairoDockNotificationFunc) _update_fade_out_dock,
			NULL);
	}
	
	cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

void cairo_dock_pop_up (CairoDock *pDock)
{
	cd_debug ("%s (%d)", __func__, pDock->bPopped);
	if (! pDock->bPopped && myAccessibility.bPopUp)
	{
		cairo_dock_remove_notification_func_on_container (CAIRO_CONTAINER (pDock),
			CAIRO_DOCK_RENDER_DOCK,
			(CairoDockNotificationFunc) _render_fade_out_dock,
			NULL);
		cairo_dock_remove_notification_func_on_container (CAIRO_CONTAINER (pDock),
			CAIRO_DOCK_UPDATE_DOCK,
			(CairoDockNotificationFunc) _update_fade_out_dock,
			NULL);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		gtk_window_set_keep_above (GTK_WINDOW (pDock->container.pWidget), TRUE);
		pDock->bPopped = TRUE;
	}
}

gboolean cairo_dock_pop_down (CairoDock *pDock)
{
	cd_debug ("%s (%d)", __func__, pDock->bPopped);
	if (pDock->bIsMainDock && cairo_dock_get_nb_dialog_windows () != 0)
		return FALSE;
	if (pDock->bPopped && myAccessibility.bPopUp && ! pDock->container.bInside)
	{
		if (cairo_dock_search_window_on_our_way (pDock, FALSE, FALSE) != NULL)
		{
			pDock->iFadeCounter = mySystem.iFadeOutNbSteps;
			pDock->bFadeInOut = TRUE;
			cairo_dock_register_notification_on_container (CAIRO_CONTAINER (pDock),
				CAIRO_DOCK_RENDER_DOCK,
				(CairoDockNotificationFunc) _render_fade_out_dock,
				CAIRO_DOCK_RUN_AFTER, NULL);
			cairo_dock_register_notification_on_container (CAIRO_CONTAINER (pDock),
				CAIRO_DOCK_UPDATE_DOCK,
				(CairoDockNotificationFunc) _update_fade_out_dock,
				CAIRO_DOCK_RUN_FIRST, NULL);
			cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
		}
		else
		{
			gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), TRUE);
		}
		pDock->bPopped = FALSE;
	}
	pDock->iSidPopDown = 0;
	return FALSE;
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

static gboolean _cairo_dock_grow_up (CairoDock *pDock)
{
	//g_print ("%s (%d ; %2f ; bInside:%d)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->container.bInside);
	if (pDock->bIsShrinkingDown)
		return TRUE;  // on se met en attente de fin d'animation.
	
	pDock->iMagnitudeIndex += mySystem.iGrowUpInterval;
	if (pDock->iMagnitudeIndex > CAIRO_DOCK_NB_MAX_ITERATIONS)
		pDock->iMagnitudeIndex = CAIRO_DOCK_NB_MAX_ITERATIONS;

	//pDock->fFoldingFactor *= sqrt (pDock->fFoldingFactor);
	//if (pDock->fFoldingFactor < 0.03)
	//	pDock->fFoldingFactor = 0;
	if (pDock->fFoldingFactor != 0)
	{
		int iAnimationDeltaT = cairo_dock_get_animation_delta_t (pDock);
		if (iAnimationDeltaT == 0)  // precaution.
		{
			cairo_dock_set_default_animation_delta_t (pDock);
			iAnimationDeltaT = cairo_dock_get_animation_delta_t (pDock);
		}
		pDock->fFoldingFactor -= (double) iAnimationDeltaT / mySystem.iUnfoldingDuration;
		if (pDock->fFoldingFactor < 0)
			pDock->fFoldingFactor = 0;
	}
	
	if (pDock->container.bIsHorizontal)
		gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseX, &pDock->container.iMouseY, NULL);
	else
		gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseY, &pDock->container.iMouseX, NULL);
	
	Icon *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	Icon *pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
	if (! pDock->bIsGrowingUp)
		return FALSE;
	
	if (pLastPointedIcon != pPointedIcon && pDock->container.bInside)
		cairo_dock_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);

	if (pDock->iMagnitudeIndex == CAIRO_DOCK_NB_MAX_ITERATIONS && pDock->fFoldingFactor == 0)  // fin de grossissement et de depliage.
	{
		if (pDock->bWMIconseedsUptade)
		{
			cairo_dock_set_icons_geometry_for_window_manager (pDock);
			pDock->bWMIconseedsUptade = FALSE;
		}
		cairo_dock_replace_all_dialogs ();
		/*if (pDock->iRefCount == 0 && pDock->bAutoHide)  // on arrive en fin de l'animation qui montre le dock, les icones sont bien placees a partir de maintenant.
		{
			cairo_dock_set_icons_geometry_for_window_manager (pDock);  // je pense que c'est redondant de le faire ici et dans le update_dock_size.
			cairo_dock_replace_all_dialogs ();
		}*/
		return FALSE;
	}
	else
		return TRUE;
}

static gboolean _cairo_dock_shrink_down (CairoDock *pDock)
{
	//g_print ("%s (%d, %f, %f)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->fDecorationsOffsetX);
	//\_________________ On fait decroitre la magnitude du dock.
	int iPrevMagnitudeIndex = pDock->iMagnitudeIndex;
	pDock->iMagnitudeIndex -= mySystem.iShrinkDownInterval;
	if (pDock->iMagnitudeIndex < 0)
		pDock->iMagnitudeIndex = 0;
	
	//\_________________ On replie le dock.
	if (pDock->fFoldingFactor != 0 && pDock->fFoldingFactor != 1)
	{
		//pDock->fFoldingFactor = pow (pDock->fFoldingFactor, 2./3);
		//if (pDock->fFoldingFactor > mySystem.fUnfoldAcceleration)
		//	pDock->fFoldingFactor = mySystem.fUnfoldAcceleration;
		int iAnimationDeltaT = cairo_dock_get_animation_delta_t (pDock);
		if (iAnimationDeltaT == 0)  // precaution.
		{
			cairo_dock_set_default_animation_delta_t (pDock);
			iAnimationDeltaT = cairo_dock_get_animation_delta_t (pDock);
		}
		pDock->fFoldingFactor += (double) iAnimationDeltaT / mySystem.iUnfoldingDuration;
		if (pDock->fFoldingFactor > 1)
			pDock->fFoldingFactor = 1;
	}
	
	//\_________________ On remet les decorations a l'equilibre.
	pDock->fDecorationsOffsetX *= .8;
	if (fabs (pDock->fDecorationsOffsetX) < 3)
		pDock->fDecorationsOffsetX = 0.;
	
	//\_________________ On recupere la position de la souris manuellement (car a priori on est hors du dock).
	if (pDock->container.bIsHorizontal)  // ce n'est pas le motion_notify qui va nous donner des coordonnees en dehors du dock, et donc le fait d'etre dedans va nous faire interrompre le shrink_down et re-grossir, du coup il faut le faire ici. L'inconvenient, c'est que quand on sort par les cotes, il n'y a soudain plus d'icone pointee, et donc le dock devient tout plat subitement au lieu de le faire doucement. Heureusement j'ai trouve une astuce. ^_^
		gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseX, &pDock->container.iMouseY, NULL);
	else
		gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseY, &pDock->container.iMouseX, NULL);
	
	//\_________________ On recalcule les icones.
	if (iPrevMagnitudeIndex != 0)
	{
		cairo_dock_calculate_dock_icons (pDock);
		if (! pDock->bIsShrinkingDown)
			return FALSE;
		
		cairo_dock_replace_all_dialogs ();
	}

	if (pDock->iMagnitudeIndex == 0)  // on est arrive en bas.
	{
		//g_print ("equilibre atteint (%d)\n", pDock->container.bInside);
		if (iPrevMagnitudeIndex != 0 || (pDock->fDecorationsOffsetX == 0 && (pDock->fFoldingFactor == 0 || pDock->fFoldingFactor == 1)))  // on vient d'arriver en bas OU l'animation de depliage est finie.
		{
			if (! pDock->container.bInside)  // on peut etre hors des icones sans etre hors de la fenetre.
			{
				//g_print ("rideau !\n");
				//\__________________ On repasse derriere si on etait devant.
				if (pDock->bPopped)
					cairo_dock_pop_down (pDock);
				
				//\__________________ On se redimensionne en taille normale.
				if (! (pDock->bAutoHide && pDock->iRefCount == 0) && ! pDock->bMenuVisible)  // fin de shrink sans auto-hide => taille normale.
				{
					//g_print ("taille normale (%x; %d)\n", pDock->pShapeBitmap , pDock->iInputState);
					if (pDock->pShapeBitmap && pDock->iInputState != CAIRO_DOCK_INPUT_AT_REST)
					{
						//g_print (" input shape\n");
						gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
							NULL,
							0,
							0);
						gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
							pDock->pShapeBitmap,
							0,
							0);
						pDock->iInputState = CAIRO_DOCK_INPUT_AT_REST;
						cairo_dock_replace_all_dialogs ();
					}
				}
				else if (pDock->bAutoHide && pDock->iRefCount == 0 && pDock->fFoldingFactor != 0)  // si le dock se replie, inutile de rester en taille grande avec une fenetre transparente, ca perturbe.
				{
					if (pDock->pHiddenShapeBitmap && pDock->iInputState != CAIRO_DOCK_INPUT_HIDDEN)
					{
						gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
							NULL,
							0,
							0);
						gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
							pDock->pHiddenShapeBitmap,
							0,
							0);
						pDock->iInputState = CAIRO_DOCK_INPUT_HIDDEN;
					}
				}
				
				//\__________________ On se cache si sous-dock.
				if (pDock->iRefCount > 0)
				{
					//g_print ("on cache ce sous-dock en sortant par lui\n");
					gtk_widget_hide (pDock->container.pWidget);
					cairo_dock_hide_parent_dock (pDock);
				}
				cairo_dock_hide_dock_like_a_menu ();
			}
			else
			{
				cairo_dock_calculate_dock_icons (pDock);  // relance le grossissement si on est dedans.
			}
		}
		else if (pDock->fFoldingFactor != 0 && pDock->fFoldingFactor != 1)
		{
			cairo_dock_calculate_dock_icons (pDock);
		}
		return (!pDock->bIsGrowingUp && (pDock->fDecorationsOffsetX != 0 || (pDock->fFoldingFactor != 0 && pDock->fFoldingFactor != 1)));
	}
	else
	{
		return (!pDock->bIsGrowingUp);
	}
}

static gboolean _cairo_dock_hide (CairoDock *pDock)
{
	if (pDock->iMagnitudeIndex > 0)  // on retarde le cachage du dock pour apercevoir les effets.
		return TRUE;
	
	pDock->fHideOffset += (1 - pDock->fHideOffset) * mySystem.fMoveDownSpeed;
	if (pDock->fHideOffset > .99)
	{
		pDock->fHideOffset = 1;
		
		//g_print ("on arrete le cachage\n");
		Icon *pIcon;
		GList *ic;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->fPersonnalScale != 0)  // on accelere l'animation d'apparition/disparition.
			{
				if (pIcon->fPersonnalScale > 0)
					pIcon->fPersonnalScale = 0.05;
				else
					pIcon->fPersonnalScale = - 0.05;
			}
			
			cairo_dock_stop_icon_animation (pIcon);  // s'il y'a une autre animation en cours, on l'arrete.
		}
		
		pDock->pRenderer->calculate_icons (pDock);
		pDock->fFoldingFactor = (mySystem.bAnimateOnAutoHide ? 1. : 0.);  // on arme le depliage.
		
		cairo_dock_allow_entrance (pDock);
		
		cairo_dock_replace_all_dialogs ();
		
		return FALSE;
	}
	return TRUE;
}

static gboolean _cairo_dock_show (CairoDock *pDock)
{
	pDock->fHideOffset -= pDock->fHideOffset * mySystem.fMoveUpSpeed;
	if (pDock->fHideOffset < 0.01)
	{
		pDock->fHideOffset = 0;
		cairo_dock_allow_entrance (pDock);
		return FALSE;
	}
	return TRUE;
}


static gboolean _cairo_dock_handle_inserting_removing_icons (CairoDock *pDock)
{
	gboolean bRecalculateIcons = FALSE;
	GList* ic = pDock->icons, *next_ic;
	Icon *pIcon;
	while (ic != NULL)
	{
		pIcon = ic->data;
		next_ic = ic->next;
		if (pIcon->fPersonnalScale == 0.05)
		{
			gboolean bIsAppli = CAIRO_DOCK_IS_NORMAL_APPLI (pIcon);
			if (bIsAppli && pIcon->iLastCheckTime != -1)  // c'est une icone d'appli non vieille qui disparait, elle s'est probablement cachee => on la detache juste.
			{
				cd_message ("cette (%s) appli est toujours valide, on la detache juste", pIcon->cName);
				cairo_dock_detach_appli (pIcon);
				pIcon->fPersonnalScale = 0.;
			}
			else
			{
				cd_message (" - %s va etre supprimee", pIcon->cName);
				cairo_dock_remove_icon_from_dock (pDock, pIcon);  // enleve le separateur automatique avec; supprime le .desktop et le sous-dock des lanceurs; stoppe les applets; marque le theme.
				
				if (pIcon->cClass != NULL && pDock == cairo_dock_search_dock_from_name (pIcon->cClass))
				{
					gboolean bEmptyClassSubDock = cairo_dock_check_class_subdock_is_empty (pDock, pIcon->cClass);
					if (bEmptyClassSubDock)
						return FALSE;
				}
				
				cairo_dock_update_dock_size (pDock);
				cairo_dock_free_icon (pIcon);
			}
		}
		else if (pIcon->fPersonnalScale == -0.05)
		{
			pIcon->fPersonnalScale = 0;  // cela n'arrete pas l'animation, qui peut se poursuivre meme apres que l'icone ait atteint sa taille maximale.
			bRecalculateIcons = TRUE;
		}
		else if (pIcon->fPersonnalScale != 0)
		{
			bRecalculateIcons = TRUE;
		}
		ic = next_ic;
	}
	
	if (bRecalculateIcons)
		cairo_dock_calculate_dock_icons (pDock);
	return TRUE;
}


static gboolean _cairo_dock_dock_animation_loop (CairoDock *pDock)
{
	//g_print ("%s (%d, %d, %d)\n", __func__, pDock->iRefCount, pDock->bIsShrinkingDown, pDock->bIsGrowingUp);
	gboolean bContinue = FALSE;
	if (pDock->bIsShrinkingDown)
	{
		pDock->bIsShrinkingDown = _cairo_dock_shrink_down (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		bContinue |= pDock->bIsShrinkingDown;
	}
	if (pDock->bIsGrowingUp)
	{
		pDock->bIsGrowingUp = _cairo_dock_grow_up (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		bContinue |= pDock->bIsGrowingUp;
	}
	if (pDock->bIsHiding)
	{
		pDock->bIsHiding = _cairo_dock_hide (pDock);
		///cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		gtk_widget_queue_draw (pDock->container.pWidget);
		bContinue |= pDock->bIsHiding;
	}
	if (pDock->bIsShowing)
	{
		pDock->bIsShowing = _cairo_dock_show (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		bContinue |= pDock->bIsShowing;
	}
	//g_print (" => %d, %d\n", pDock->bIsShrinkingDown, pDock->bIsGrowingUp);
	
	gboolean bUpdateSlowAnimation = FALSE;
	pDock->container.iAnimationStep ++;
	if (pDock->container.iAnimationStep * pDock->container.iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pDock->container.iAnimationStep = 0;
		pDock->container.bKeepSlowAnimation = FALSE;
	}
	double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
	gboolean bIconIsAnimating;
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		
		icon->fDeltaYReflection = 0;
		if (myIcons.fAlphaAtRest != 1)
			icon->fAlpha = fDockMagnitude + myIcons.fAlphaAtRest * (1 - fDockMagnitude);
		
		bIconIsAnimating = FALSE;
		if (bUpdateSlowAnimation)
		{
			cairo_dock_notify_on_icon (icon, CAIRO_DOCK_UPDATE_ICON_SLOW, icon, pDock, &bIconIsAnimating);
			pDock->container.bKeepSlowAnimation |= bIconIsAnimating;
		}
		cairo_dock_notify_on_icon (icon, CAIRO_DOCK_UPDATE_ICON, icon, pDock, &bIconIsAnimating);
		bContinue |= bIconIsAnimating;
		if (! bIconIsAnimating)
			icon->iAnimationState = CAIRO_DOCK_STATE_REST;
	}
	bContinue |= pDock->container.bKeepSlowAnimation;
	
	if (! _cairo_dock_handle_inserting_removing_icons (pDock))
	{
		cd_debug ("ce dock n'a plus de raison d'etre");
		return FALSE;
	}
	
	if (bUpdateSlowAnimation)
	{
		cairo_dock_notify_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_UPDATE_DOCK_SLOW, pDock, &pDock->container.bKeepSlowAnimation);
	}
	cairo_dock_notify_on_container (CAIRO_CONTAINER (pDock), CAIRO_DOCK_UPDATE_DOCK, pDock, &bContinue);
	
	if (! bContinue && ! pDock->container.bKeepSlowAnimation)
	{
		pDock->container.iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}
static gboolean _cairo_desklet_animation_loop (CairoDesklet *pDesklet)
{
	gboolean bContinue = FALSE;
	
	gboolean bUpdateSlowAnimation = FALSE;
	pDesklet->container.iAnimationStep ++;
	if (pDesklet->container.iAnimationStep * pDesklet->container.iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pDesklet->container.iAnimationStep = 0;
		pDesklet->container.bKeepSlowAnimation = FALSE;
	}
	
	if (pDesklet->pIcon != NULL)
	{
		gboolean bIconIsAnimating = FALSE;
		
		if (bUpdateSlowAnimation)
		{
			cairo_dock_notify_on_icon (pDesklet->pIcon, CAIRO_DOCK_UPDATE_ICON_SLOW, pDesklet->pIcon, pDesklet, &bIconIsAnimating);
			pDesklet->container.bKeepSlowAnimation |= bIconIsAnimating;
		}
		
		cairo_dock_notify_on_icon (pDesklet->pIcon, CAIRO_DOCK_UPDATE_ICON, pDesklet->pIcon, pDesklet, &bIconIsAnimating);
		if (! bIconIsAnimating)
			pDesklet->pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
		else
			bContinue = TRUE;
	}
	
	if (bUpdateSlowAnimation)
	{
		cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_UPDATE_DESKLET_SLOW, pDesklet, &pDesklet->container.bKeepSlowAnimation);
	}
	
	cairo_dock_notify_on_container (CAIRO_CONTAINER (pDesklet), CAIRO_DOCK_UPDATE_DESKLET, pDesklet, &bContinue);
	
	if (! bContinue && ! pDesklet->container.bKeepSlowAnimation)
	{
		pDesklet->container.iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}

static gboolean _cairo_flying_container_animation_loop (CairoFlyingContainer *pFlyingContainer)
{
	gboolean bContinue = FALSE;
	
	if (pFlyingContainer->pIcon != NULL)
	{
		gboolean bIconIsAnimating = FALSE;
		
		cairo_dock_notify_on_icon (pFlyingContainer->pIcon, CAIRO_DOCK_UPDATE_ICON, pFlyingContainer->pIcon, pFlyingContainer, &bIconIsAnimating);
		if (! bIconIsAnimating)
			pFlyingContainer->pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
		else
			bContinue = TRUE;
	}
	
	cairo_dock_notify (CAIRO_DOCK_UPDATE_FLYING_CONTAINER, pFlyingContainer, &bContinue);
	
	if (! bContinue)
	{
		cairo_dock_free_flying_container (pFlyingContainer);
		return FALSE;
	}
	else
		return TRUE;
}


static gboolean _cairo_default_container_animation_loop (CairoContainer *pContainer)
{
	gboolean bContinue = FALSE;
	
	gboolean bUpdateSlowAnimation = FALSE;
	pContainer->iAnimationStep ++;
	if (pContainer->iAnimationStep * pContainer->iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pContainer->iAnimationStep = 0;
		pContainer->bKeepSlowAnimation = FALSE;
	}
	
	if (bUpdateSlowAnimation)
	{
		cairo_dock_notify_on_container (pContainer, CAIRO_DOCK_UPDATE_DEFAULT_CONTAINER_SLOW, pContainer, &pContainer->bKeepSlowAnimation);
	}
	
	cairo_dock_notify_on_container (pContainer, CAIRO_DOCK_UPDATE_DEFAULT_CONTAINER, pContainer, &bContinue);
	
	if (! bContinue && ! pContainer->bKeepSlowAnimation)
	{
		pContainer->iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}

void cairo_dock_launch_animation (CairoContainer *pContainer)
{
	if (pContainer->iSidGLAnimation == 0)
	{
		int iAnimationDeltaT = cairo_dock_get_animation_delta_t (pContainer);
		if (iAnimationDeltaT == 0)  // precaution.
		{
			cairo_dock_set_default_animation_delta_t (pContainer);
			iAnimationDeltaT = cairo_dock_get_animation_delta_t (pContainer);
		}
		pContainer->bKeepSlowAnimation = TRUE;
		switch (pContainer->iType)
		{
			case CAIRO_DOCK_TYPE_DOCK :
				pContainer->iSidGLAnimation = g_timeout_add (iAnimationDeltaT, (GSourceFunc)_cairo_dock_dock_animation_loop, pContainer);
			break ;
			case CAIRO_DOCK_TYPE_DESKLET :
				pContainer->iSidGLAnimation = g_timeout_add (iAnimationDeltaT, (GSourceFunc) _cairo_desklet_animation_loop, pContainer);
			break;
			case CAIRO_DOCK_TYPE_FLYING_CONTAINER :
				pContainer->iSidGLAnimation = g_timeout_add (iAnimationDeltaT, (GSourceFunc)_cairo_flying_container_animation_loop, pContainer);
			break ;
			case CAIRO_DOCK_TYPE_DIALOG:
				cd_warning ("Dialogs has no animation capability yet");
			break;
			default :
				pContainer->iSidGLAnimation = g_timeout_add (iAnimationDeltaT, (GSourceFunc)_cairo_default_container_animation_loop, pContainer);
			break ;
		}
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
	//g_print ("%s (%d)\n", __func__, pDock->container.bInside);
	if (! pDock->bIsHiding && ! pDock->container.bInside)  // rien de plus desagreable que le dock qui se cache quand on est dedans.
	{
		pDock->bIsShowing = FALSE;
		pDock->bIsHiding = TRUE;
		
		if (pDock->pHiddenShapeBitmap && pDock->iInputState != CAIRO_DOCK_INPUT_HIDDEN)
		{
			gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
				pDock->pHiddenShapeBitmap,
				0,
				0);
			pDock->iInputState = CAIRO_DOCK_INPUT_HIDDEN;
		}
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}

void cairo_dock_start_showing (CairoDock *pDock)
{
	if (! pDock->bIsShowing)  // on lance l'animation.
	{
		pDock->bIsShowing = TRUE;
		pDock->bIsHiding = FALSE;
		
		if (pDock->pShapeBitmap && pDock->iInputState != CAIRO_DOCK_INPUT_ACTIVE)
		{
			gtk_widget_input_shape_combine_mask (pDock->container.pWidget,
				NULL,
				0,
				0);
			pDock->iInputState = CAIRO_DOCK_INPUT_ACTIVE;
		}
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}


void cairo_dock_start_icon_animation (Icon *pIcon, CairoDock *pDock)
{
	g_return_if_fail (pIcon != NULL && pDock != NULL);
	cd_message ("%s (%s, %d)", __func__, pIcon->cName, pIcon->iAnimationState);
	
	if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REST &&
		(pIcon->fPersonnalScale != 0 || cairo_dock_animation_will_be_visible (pDock)))
	{
		//g_print ("  c'est parti\n");
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}

void cairo_dock_request_icon_animation (Icon *pIcon, CairoDock *pDock, const gchar *cAnimation, int iNbRounds)
{
	cd_debug ("%s (%s, state:%d)", __func__, pIcon->cName, pIcon->iAnimationState);
	if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REST)  // on le fait avant de changer d'animation, pour le cas ou l'icone ne serait plus placee au meme endroit (rebond).
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	cairo_dock_stop_icon_animation (pIcon);
	
	if (cAnimation == NULL || iNbRounds == 0 || pIcon->iAnimationState != CAIRO_DOCK_STATE_REST)
		return ;
	cairo_dock_notify (CAIRO_DOCK_REQUEST_ICON_ANIMATION, pIcon, pDock, cAnimation, iNbRounds);
	cairo_dock_start_icon_animation (pIcon, pDock);
}


void cairo_dock_trigger_icon_removal_from_dock (Icon *pIcon)
{
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pDock != NULL)
	{
		cairo_dock_stop_icon_animation (pIcon);
		if (cairo_dock_animation_will_be_visible (pDock))  // sinon inutile de se taper toute l'animation.
			pIcon->fPersonnalScale = 1.0;
		else
			pIcon->fPersonnalScale = 0.05;
		cairo_dock_notify (CAIRO_DOCK_REMOVE_ICON, pIcon, pDock);
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
	icon->fPersonnalScale *= .85;
	if (icon->fPersonnalScale > 0)
	{
		if (icon->fPersonnalScale < 0.05)
			icon->fPersonnalScale = 0.05;
	}
	else if (icon->fPersonnalScale < 0)
	{
		if (icon->fPersonnalScale > -0.05)
			icon->fPersonnalScale = -0.05;
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
	
	if (pIcon->fPersonnalScale == 0 || ! pIcon->bBeingRemovedByCairo)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	cairo_dock_update_removing_inserting_icon_size_default (pIcon);
	
	if (fabs (pIcon->fPersonnalScale) > 0.05)
	{
		cairo_dock_mark_icon_as_inserting_removing (pIcon);
		*bContinueAnimation = TRUE;
	}
	cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

gboolean cairo_dock_on_insert_remove_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock)
{
	if (pIcon->iAnimationState == CAIRO_DOCK_STATE_REMOVE_INSERT)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	pIcon->bBeingRemovedByCairo = TRUE;
	cairo_dock_mark_icon_as_inserting_removing (pIcon);  // On prend en charge le dessin de l'icone pendant sa phase d'insertion/suppression.
	
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
	
	gboolean bContinue;
	if (CAIRO_CONTAINER_IS_OPENGL (pTransition->pContainer))
	{
		if (pTransition->render_opengl)
		{
			if (! cairo_dock_begin_draw_icon (pIcon, pContainer))
				return CAIRO_DOCK_LET_PASS_NOTIFICATION;
			bContinue = pTransition->render_opengl (pTransition->pUserData);
			cairo_dock_end_draw_icon (pIcon, pContainer);
		}
		else
		{
			cairo_dock_erase_cairo_context (pTransition->pIconContext);
			bContinue = pTransition->render (pTransition->pUserData, pTransition->pIconContext);
			cairo_dock_update_icon_texture (pIcon);
		}
	}
	else if (pTransition->render && pTransition->pIconContext != NULL)
	{
		cairo_dock_erase_cairo_context (pTransition->pIconContext);
		bContinue = pTransition->render (pTransition->pUserData, pTransition->pIconContext);
		if (pContainer->bUseReflect)
			cairo_dock_add_reflection_to_icon (pTransition->pIconContext, pIcon, pContainer);
	}
	else
		bContinue = FALSE;
	
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
void cairo_dock_set_transition_on_icon (Icon *pIcon, CairoContainer *pContainer, cairo_t *pIconContext, CairoDockTransitionRenderFunc render_step_cairo, CairoDockTransitionGLRenderFunc render_step_opengl, gboolean bFastPace, gint iDuration, gboolean bRemoveWhenFinished, gpointer pUserData)
{
	cairo_dock_remove_transition_on_icon (pIcon);
	
	CairoDockTransition *pTransition = g_new0 (CairoDockTransition, 1);
	pTransition->render = render_step_cairo;
	pTransition->render_opengl = render_step_opengl;
	pTransition->bFastPace = bFastPace;
	pTransition->iDuration = iDuration;
	pTransition->bRemoveWhenFinished = bRemoveWhenFinished;
	pTransition->pContainer = pContainer;
	pTransition->pIconContext = pIconContext;
	pTransition->pUserData = pUserData;
	cairo_dock_set_transition (pIcon, pTransition);
	
	cairo_dock_register_notification_on_icon (pIcon, bFastPace ? CAIRO_DOCK_UPDATE_ICON : CAIRO_DOCK_UPDATE_ICON_SLOW,
		(CairoDockNotificationFunc) _cairo_dock_transition_step, CAIRO_DOCK_RUN_AFTER, pUserData);
	
	cairo_dock_launch_animation (pContainer);
}

void cairo_dock_remove_transition_on_icon (Icon *pIcon)
{
	CairoDockTransition *pTransition = cairo_dock_get_transition (pIcon);
	if (pTransition == NULL)
		return ;
	
	cairo_dock_remove_notification_func_on_icon (pIcon, pTransition->bFastPace ? CAIRO_DOCK_UPDATE_ICON : CAIRO_DOCK_UPDATE_ICON_SLOW,
		(CairoDockNotificationFunc) _cairo_dock_transition_step,
		pTransition->pUserData);

	g_free (pTransition);
	cairo_dock_set_transition (pIcon, NULL);
}
