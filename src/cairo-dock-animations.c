/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
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

gboolean cairo_dock_move_up (CairoDock *pDock)
{
	int deltaY_possible;
	deltaY_possible = pDock->iWindowPositionY - (pDock->bDirectionUp ? g_iXScreenHeight[pDock->bHorizontalDock] - pDock->iMaxDockHeight - pDock->iGapY : pDock->iGapY);
	//g_print ("%s (%dx%d -> %d)\n", __func__, pDock->iWindowPositionX, pDock->iWindowPositionY, deltaY_possible);
	if ((pDock->bDirectionUp && deltaY_possible > 0) || (! pDock->bDirectionUp && deltaY_possible < 0))  // alors on peut encore monter.
	{
		pDock->iWindowPositionY -= (int) (deltaY_possible * mySystem.fMoveUpSpeed) + (pDock->bDirectionUp ? 1 : -1);
		//g_print ("  move to (%dx%d)\n", g_iWindowPositionX, g_iWindowPositionY);
		if (pDock->bHorizontalDock)
			gtk_window_move (GTK_WINDOW (pDock->pWidget), pDock->iWindowPositionX, pDock->iWindowPositionY);
		else
			gtk_window_move (GTK_WINDOW (pDock->pWidget), pDock->iWindowPositionY, pDock->iWindowPositionX);
		pDock->bAtBottom = FALSE;
		return TRUE;
	}
	else
	{
		pDock->bAtTop = TRUE;
		pDock->iSidMoveUp = 0;
		return FALSE;
	}
}

void cairo_dock_pop_up (CairoDock *pDock)
{
	cd_debug ("%s (%d)", __func__, pDock->bPopped);
	if (! pDock->bPopped && myAccessibility.bPopUp)
	{
		gtk_window_set_keep_above (GTK_WINDOW (pDock->pWidget), TRUE);
		pDock->bPopped = TRUE;
	}
}


gboolean cairo_dock_pop_down (CairoDock *pDock)
{
	cd_debug ("%s (%d)", __func__, pDock->bPopped);
	if (pDock->bIsMainDock && cairo_dock_get_nb_config_panels () != 0)
		return FALSE;
	if (pDock->bPopped && myAccessibility.bPopUp)
	{
		gtk_window_set_keep_below (GTK_WINDOW (pDock->pWidget), TRUE);
		pDock->bPopped = FALSE;
	}
	pDock->iSidPopDown = 0;
	return FALSE;
}

gboolean cairo_dock_move_down (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	if (pDock->iMagnitudeIndex > 0/** || (mySystem.bResetScrollOnLeave && pDock->iScrollOffset != 0)*/)  // on retarde le cachage du dock pour apercevoir les effets.
		return TRUE;
	int deltaY_possible = (pDock->bDirectionUp ? g_iXScreenHeight[pDock->bHorizontalDock] - pDock->iGapY - 0 : pDock->iGapY + 0 - pDock->iMaxDockHeight) - pDock->iWindowPositionY;  // 0 <-> g_iVisibleZoneHeight
	//g_print ("%s (%d)\n", __func__, deltaY_possible);
	if ((pDock->bDirectionUp && deltaY_possible > 8) || (! pDock->bDirectionUp && deltaY_possible < -8))  // alors on peut encore descendre.
	{
		pDock->iWindowPositionY += (int) (deltaY_possible * mySystem.fMoveDownSpeed) + (pDock->bDirectionUp ? 1 : -1);  // 0.33
		//g_print ("pDock->iWindowPositionY <- %d\n", pDock->iWindowPositionY);
		if (pDock->bHorizontalDock)
			gtk_window_move (GTK_WINDOW (pDock->pWidget), pDock->iWindowPositionX, pDock->iWindowPositionY);
		else
			gtk_window_move (GTK_WINDOW (pDock->pWidget), pDock->iWindowPositionY, pDock->iWindowPositionX);
		pDock->bAtTop = FALSE;
		gtk_widget_queue_draw (pDock->pWidget);
		return TRUE;
	}
	else  // on se fixe en bas, et on montre la zone visible.
	{
		cd_debug ("  on se fixe en bas");
		pDock->bAtBottom = TRUE;
		pDock->iSidMoveDown = 0;
		int iNewWidth, iNewHeight;
		cairo_dock_get_window_position_and_geometry_at_balance (pDock, CAIRO_DOCK_MIN_SIZE, &iNewWidth, &iNewHeight);
		if (pDock->bHorizontalDock)
			gdk_window_move_resize (pDock->pWidget->window,
				pDock->iWindowPositionX,
				pDock->iWindowPositionY,
				iNewWidth,
				iNewHeight);
		else
			gdk_window_move_resize (pDock->pWidget->window,
				pDock->iWindowPositionY,
				pDock->iWindowPositionX,
				iNewHeight,
				iNewWidth);

		if (pDock->bAutoHide && pDock->iRefCount == 0)
		{
			//g_print ("on arrete les animations\n");
			Icon *pIcon;
			GList *ic;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				pIcon = ic->data;
				if (pIcon->fPersonnalScale != 0)
				{
					if (pIcon->fPersonnalScale > 0)
						pIcon->fPersonnalScale = 0.05;
					else
						pIcon->fPersonnalScale = - 0.05;
				}
				
				if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REST)  // s'il y'a une animation en cours, on l'arrete.
				{
					cairo_dock_notify (CAIRO_DOCK_STOP_ICON, pIcon);
					pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
				}
			}
			///pDock->iScrollOffset = 0;

			///pDock->calculate_max_dock_size (pDock);  // utilite ?...
			pDock->calculate_icons (pDock);
			pDock->fFoldingFactor = (mySystem.bAnimateOnAutoHide ? 1. : 0.);  // on arme le depliage.

			cairo_dock_allow_entrance (pDock);
		}

		gtk_widget_queue_draw (pDock->pWidget);

		return FALSE;
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
	
	return  tmp;
}

gboolean cairo_dock_grow_up (CairoDock *pDock)
{
	//g_print ("%s (%d ; %2f ; bInside:%d)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->bInside);
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
	
	if (pDock->bHorizontalDock)
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseX, &pDock->iMouseY, NULL);
	else
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseY, &pDock->iMouseX, NULL);
	
	Icon *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	Icon *pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
	if (! pDock->bIsGrowingUp)
		return FALSE;
	
	if (pLastPointedIcon != pPointedIcon && pDock->bInside)
		cairo_dock_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);

	if (pDock->iMagnitudeIndex == CAIRO_DOCK_NB_MAX_ITERATIONS && pDock->fFoldingFactor == 0)  // fin de grossissement et de depliage.
	{
		if (pDock->iRefCount == 0 && pDock->bAutoHide)  // on arrive en fin de l'animation qui montre le dock, les icones sont bien placees a partir de maintenant.
		{
			cairo_dock_set_icons_geometry_for_window_manager (pDock);
			cairo_dock_replace_all_dialogs ();
		}
		return FALSE;
	}
	else
		return TRUE;
}

gboolean cairo_dock_shrink_down (CairoDock *pDock)
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
	if (pDock->bHorizontalDock)  // ce n'est pas le motion_notify qui va nous donner des coordonnees en dehors du dock, et donc le fait d'etre dedans va nous faire interrompre le shrink_down et re-grossir, du coup il faut le faire ici. L'inconvenient, c'est que quand on sort par les cotes, il n'y a soudain plus d'icone pointee, et donc le dock devient tout plat subitement au lieu de le faire doucement. Heureusement j'ai trouve une astuce. ^_^
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseX, &pDock->iMouseY, NULL);
	else
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseY, &pDock->iMouseX, NULL);
	
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
		//g_print ("equilibre atteint (%d)\n", pDock->bInside);
		if (iPrevMagnitudeIndex != 0 || (pDock->fDecorationsOffsetX == 0 && (pDock->fFoldingFactor == 0 || pDock->fFoldingFactor == 1)))  // on vient d'arriver en bas OU l'animation de shrink est finie.
		{
			if (! pDock->bInside)  // on peut etre hors des icones sans etre hors de la fenetre.
			{
				//g_print ("rideau !\n");
				//\__________________ On repasse derriere si on etait devant.
				if (pDock->bPopped)
					cairo_dock_pop_down (pDock);
				
				//\__________________ On se redimensionne en taille normale.
				if (! (pDock->bAutoHide && pDock->iRefCount == 0) && ! pDock->bMenuVisible)
				{
					int iNewWidth, iNewHeight;
					cairo_dock_get_window_position_and_geometry_at_balance (pDock, CAIRO_DOCK_NORMAL_SIZE, &iNewWidth, &iNewHeight);
					if (pDock->bHorizontalDock)
						gdk_window_move_resize (pDock->pWidget->window,
							pDock->iWindowPositionX,
							pDock->iWindowPositionY,
							iNewWidth,
							iNewHeight);
					else
						gdk_window_move_resize (pDock->pWidget->window,
							pDock->iWindowPositionY,
							pDock->iWindowPositionX,
							iNewHeight,
							iNewWidth);
				}
				else if (pDock->bAutoHide && pDock->iRefCount == 0 && pDock->fFoldingFactor != 0)  // si le dock se replie, inutile de rester en taille grande avec une fenetre transparente, ca perturbe.
				{
					int iNewWidth, iNewHeight;
					cairo_dock_get_window_position_and_geometry_at_balance (pDock, CAIRO_DOCK_MIN_SIZE, &iNewWidth, &iNewHeight);
					if (pDock->bHorizontalDock)
						gdk_window_move_resize (pDock->pWidget->window,
							pDock->iWindowPositionX,
							pDock->iWindowPositionY,
							iNewWidth,
							iNewHeight);
					else
						gdk_window_move_resize (pDock->pWidget->window,
							pDock->iWindowPositionY,
							pDock->iWindowPositionX,
							iNewHeight,
							iNewWidth);
				}
				
				//\__________________ On se cache si sous-dock.
				if (pDock->iRefCount > 0)
				{
					//g_print ("on cache ce sous-dock en sortant par lui\n");
					gtk_widget_hide (pDock->pWidget);
					cairo_dock_hide_parent_dock (pDock);
				}

				pDock->bAtBottom = TRUE;
				cairo_dock_hide_dock_like_a_menu ();
			}
			else
			{
				cairo_dock_calculate_dock_icons (pDock);  // relance le grossissement si on est dedans.
				if (! pDock->bIsGrowingUp)
					pDock->bAtBottom = TRUE;
			}
		}
		return (!pDock->bIsGrowingUp && (pDock->fDecorationsOffsetX != 0 || (pDock->fFoldingFactor != 0 && pDock->fFoldingFactor != 1)));
	}
	else
	{
		return (!pDock->bIsGrowingUp);
	}
}


gboolean cairo_dock_handle_inserting_removing_icons (CairoDock *pDock)
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
				g_print ("cette (%s) appli est toujours valide, on la detache juste\n", pIcon->acName);
				cairo_dock_detach_appli (pIcon);
				pIcon->fPersonnalScale = 0.;
			}
			else
			{
				cd_message (" - %s va etre supprimee", pIcon->acName);
				gboolean bIsAppli = CAIRO_DOCK_IS_NORMAL_APPLI (pIcon);  // car apres avoir ete enleve du dock elle n'est plus rien.
				cairo_dock_remove_icon_from_dock (pDock, pIcon);  // enleve le separateur automatique avec.
				
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
			pIcon->fPersonnalScale = 0;  // on n'arrete pas l'animation car elle peut se poursuivre meme apres que l'icone ait atteint sa taille maximale.
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


void cairo_dock_start_icon_animation (Icon *pIcon, CairoDock *pDock)
{
	g_return_if_fail (pIcon != NULL && pDock != NULL);
	cd_message ("%s (%s, %d)", __func__, pIcon->acName, pIcon->iAnimationState);
	
	if (pIcon->iAnimationState != CAIRO_DOCK_STATE_REST && (cairo_dock_animation_will_be_visible (pDock) || pIcon->fPersonnalScale != 0))
	{
		//g_print ("  c'est parti\n");
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}

void cairo_dock_request_icon_animation (Icon *pIcon, CairoDock *pDock, const gchar *cAnimation, int iNbRounds)
{
	cairo_dock_stop_icon_animation (pIcon);
	
	if (cAnimation == NULL || iNbRounds == 0 || pIcon->iAnimationState != CAIRO_DOCK_STATE_REST)
		return ;
	cairo_dock_notify (CAIRO_DOCK_REQUEST_ICON_ANIMATION, pIcon, pDock, cAnimation, iNbRounds);
	cairo_dock_start_icon_animation (pIcon, pDock);
}

static gboolean _cairo_dock_dock_animation_loop (CairoDock *pDock)
{
	//g_print ("%s (%d, %d, %d)\n", __func__, pDock->iRefCount, pDock->bIsShrinkingDown, pDock->bIsGrowingUp);
	gboolean bContinue = FALSE;
	if (pDock->bIsShrinkingDown)
	{
		pDock->bIsShrinkingDown = cairo_dock_shrink_down (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		bContinue |= pDock->bIsShrinkingDown;
	}
	if (pDock->bIsGrowingUp)
	{
		pDock->bIsGrowingUp = cairo_dock_grow_up (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		bContinue |= pDock->bIsGrowingUp;
	}
	//g_print (" => %d, %d\n", pDock->bIsShrinkingDown, pDock->bIsGrowingUp);
	
	gboolean bUpdateSlowAnimation = FALSE;
	pDock->iAnimationStep ++;
	if (pDock->iAnimationStep * pDock->iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pDock->iAnimationStep = 0;
		pDock->bKeepSlowAnimation = FALSE;
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
			pDock->bKeepSlowAnimation |= bIconIsAnimating;
		}
		cairo_dock_notify_on_icon (icon, CAIRO_DOCK_UPDATE_ICON, icon, pDock, &bIconIsAnimating);
		bContinue |= bIconIsAnimating;
		if (! bIconIsAnimating)
			icon->iAnimationState = CAIRO_DOCK_STATE_REST;
	}
	bContinue |= pDock->bKeepSlowAnimation;
	
	if (! cairo_dock_handle_inserting_removing_icons (pDock))
	{
		cd_debug ("ce dock n'a plus de raison d'etre");
		return FALSE;
	}
	
	if (bUpdateSlowAnimation)
	{
		cairo_dock_notify_on_container (pDock, CAIRO_DOCK_UPDATE_DOCK_SLOW, pDock, &pDock->bKeepSlowAnimation);
	}
	cairo_dock_notify_on_container (pDock, CAIRO_DOCK_UPDATE_DOCK, pDock, &bContinue);
	
	if (! bContinue && ! pDock->bKeepSlowAnimation)
	{
		pDock->iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}
static gboolean _cairo_desklet_animation_loop (CairoDesklet *pDesklet)
{
	gboolean bContinue = FALSE;
	
	gboolean bUpdateSlowAnimation = FALSE;
	pDesklet->iAnimationStep ++;
	if (pDesklet->iAnimationStep * pDesklet->iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pDesklet->iAnimationStep = 0;
		pDesklet->bKeepSlowAnimation = FALSE;
	}
	
	if (pDesklet->pIcon != NULL)
	{
		gboolean bIconIsAnimating = FALSE;
		
		if (bUpdateSlowAnimation)
		{
			cairo_dock_notify_on_icon (pDesklet->pIcon, CAIRO_DOCK_UPDATE_ICON_SLOW, pDesklet->pIcon, pDesklet, &bIconIsAnimating);
			pDesklet->bKeepSlowAnimation |= bIconIsAnimating;
		}
		
		cairo_dock_notify_on_icon (pDesklet->pIcon, CAIRO_DOCK_UPDATE_ICON, pDesklet->pIcon, pDesklet, &bIconIsAnimating);
		if (! bIconIsAnimating)
			pDesklet->pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
		else
			bContinue = TRUE;
	}
	
	if (bUpdateSlowAnimation)
	{
		cairo_dock_notify_on_container (pDesklet, CAIRO_DOCK_UPDATE_DESKLET_SLOW, pDesklet, &pDesklet->bKeepSlowAnimation);
	}
	
	cairo_dock_notify_on_container (pDesklet, CAIRO_DOCK_UPDATE_DESKLET, pDesklet, &bContinue);
	
	if (! bContinue && ! pDesklet->bKeepSlowAnimation)
	{
		pDesklet->iSidGLAnimation = 0;
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
		
		cairo_dock_notify (CAIRO_DOCK_UPDATE_ICON, pFlyingContainer->pIcon, pFlyingContainer, &bIconIsAnimating);
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
	//g_print ("%s (%d, %d)\n", __func__, pDock->bIsShrinkingDown, pDock->iSidGLAnimation);
	if (! pDock->bIsShrinkingDown)  // on lance l'animation.
	{
		pDock->bIsGrowingUp = FALSE;
		pDock->bIsShrinkingDown = TRUE;
		
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
}

void cairo_dock_start_growing (CairoDock *pDock)
{
	if (! pDock->bIsGrowingUp)  // on lance l'animation.
	{
		pDock->bIsShrinkingDown = FALSE;
		pDock->bIsGrowingUp = TRUE;
		
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
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
	if (icon->fPersonnalScale > 0)
	{
		icon->fPersonnalScale *= .85;
		if (icon->fPersonnalScale < 0.05)
			icon->fPersonnalScale = 0.05;
	}
	else if (icon->fPersonnalScale < 0)
	{
		icon->fPersonnalScale *= .85;
		if (icon->fPersonnalScale > -0.05)
			icon->fPersonnalScale = -0.05;
	}
}

gboolean cairo_dock_update_inserting_removing_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bContinueAnimation)
{
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
