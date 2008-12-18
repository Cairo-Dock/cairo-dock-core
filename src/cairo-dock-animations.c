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
#include "cairo-dock-callbacks.h"
#include "cairo-dock-draw.h"
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
#include "cairo-dock-animations.h"

extern int g_iScreenHeight[2];

extern int g_tNbIterInOneRound[CAIRO_DOCK_NB_ANIMATIONS];

extern gboolean g_bEasterEggs;

extern CairoDock *g_pMainDock;

extern gboolean g_bUseOpenGL;
extern gdouble g_iGLAnimationDeltaT;
extern gdouble g_iCairoAnimationDeltaT;

gboolean cairo_dock_move_up (CairoDock *pDock)
{
	int deltaY_possible;
	deltaY_possible = pDock->iWindowPositionY - (pDock->bDirectionUp ? g_iScreenHeight[pDock->bHorizontalDock] - pDock->iMaxDockHeight - pDock->iGapY : pDock->iGapY);
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

gboolean cairo_dock_pop_up (CairoDock *pDock)
{
	cd_debug ("%s (%d)", __func__, pDock->bPopped);
	if (! pDock->bPopped && myAccessibility.bPopUp)
		gtk_window_set_keep_above (GTK_WINDOW (pDock->pWidget), TRUE);
	
	pDock->iSidPopUp = 0;
	pDock->bPopped = TRUE;
	return FALSE;
}


gboolean cairo_dock_pop_down (CairoDock *pDock)
{
	cd_debug ("%s (%d)", __func__, pDock->bPopped);
	if (pDock->bIsMainDock && cairo_dock_get_nb_config_panels () != 0)
		return FALSE;
	if (pDock->bPopped && myAccessibility.bPopUp)
		gtk_window_set_keep_below (GTK_WINDOW (pDock->pWidget), TRUE);
	
	pDock->iSidPopDown = 0;
	pDock->bPopped = FALSE;
	return FALSE;
}

gboolean cairo_dock_move_down (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	if (pDock->iMagnitudeIndex > 0 || (mySystem.bResetScrollOnLeave && pDock->iScrollOffset != 0))  // on retarde le cachage du dock pour apercevoir les effets.
		return TRUE;
	int deltaY_possible = (pDock->bDirectionUp ? g_iScreenHeight[pDock->bHorizontalDock] - pDock->iGapY - 0 : pDock->iGapY + 0 - pDock->iMaxDockHeight) - pDock->iWindowPositionY;  // 0 <-> g_iVisibleZoneHeight
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
			Icon *pBouncingIcon = cairo_dock_get_bouncing_icon (pDock->icons);
			if (pBouncingIcon != NULL)  // s'il y'a une icone en cours d'animation, on l'arrete.
			{
				pBouncingIcon->iCount = 0;
			}
			Icon *pRemovingIcon = cairo_dock_get_removing_or_inserting_icon (pDock->icons);
			if (pRemovingIcon != NULL)  // idem.
			{
				if (pRemovingIcon->fPersonnalScale > 0)
					pRemovingIcon->fPersonnalScale = 0.05;
				else
					pRemovingIcon->fPersonnalScale = - 0.05;
				//g_print ("fPersonnalScale <- %f\n", pRemovingIcon->fPersonnalScale);
			}
			pDock->iScrollOffset = 0;

			pDock->calculate_max_dock_size (pDock);
			pDock->fFoldingFactor = (mySystem.bAnimateOnAutoHide ? mySystem.fUnfoldAcceleration : 0);

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
	//g_print ("%s (%d ; %f ; %d)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->bInside);
	if (pDock->bIsShrinkingDown)
		return TRUE;  // on se met en attente de fin d'animation.

	pDock->iMagnitudeIndex += mySystem.iGrowUpInterval;
	if (pDock->iMagnitudeIndex > CAIRO_DOCK_NB_MAX_ITERATIONS)
		pDock->iMagnitudeIndex = CAIRO_DOCK_NB_MAX_ITERATIONS;

	pDock->fFoldingFactor *= sqrt (pDock->fFoldingFactor);
	if (pDock->fFoldingFactor < 0.03)
		pDock->fFoldingFactor = 0;

	if (pDock->bHorizontalDock)
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseX, &pDock->iMouseY, NULL);
	else
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseY, &pDock->iMouseX, NULL);
	
	Icon *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	Icon *pPointedIcon = pDock->calculate_icons (pDock);
	//gtk_widget_queue_draw (pDock->pWidget);
	
	if (pLastPointedIcon != pPointedIcon)
		cairo_dock_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);

	if (pDock->bIsGrowingUp && pDock->iMagnitudeIndex == CAIRO_DOCK_NB_MAX_ITERATIONS && pDock->fFoldingFactor == 0)  // fin de grossissement.
	{
		pDock->iMagnitudeIndex = CAIRO_DOCK_NB_MAX_ITERATIONS;
		pDock->bIsGrowingUp = 0;
		if (pDock->iRefCount == 0 && pDock->bAutoHide)  // on arrive en fin de l'animation qui montre le dock, les icones sont bien placees a partir de maintenant.
		{
			cairo_dock_set_icons_geometry_for_window_manager (pDock);
			cairo_dock_replace_all_dialogs ();
		}
		if (g_bEasterEggs)
			cairo_dock_unset_input_shape (pDock);
		return FALSE;
	}
	else
		return TRUE;
}

gboolean cairo_dock_shrink_down (CairoDock *pDock)
{
	//g_print ("%s (%d)\n", __func__, pDock->iMagnitudeIndex);
	int iPrevMagnitudeIndex = pDock->iMagnitudeIndex;
	pDock->iMagnitudeIndex -= mySystem.iShrinkDownInterval;
	if (pDock->iMagnitudeIndex < 0)
		pDock->iMagnitudeIndex = 0;
	//g_print ("pDock->fFoldingFactor : %f\n", pDock->fFoldingFactor);
	if (pDock->fFoldingFactor != 0 && (! mySystem.bResetScrollOnLeave || pDock->iScrollOffset == 0))
	{
		pDock->fFoldingFactor = pow (pDock->fFoldingFactor, 2./3);
		if (pDock->fFoldingFactor > mySystem.fUnfoldAcceleration)
			pDock->fFoldingFactor = mySystem.fUnfoldAcceleration;
	}
	pDock->fDecorationsOffsetX *= .8;
	//g_print ("fDecorationsOffsetX <- %.2f\n", pDock->fDecorationsOffsetX);

	if (pDock->bHorizontalDock)  // ce n'est pas le motion_notify qui va nous donner des coordonnees en dehors du dock, et donc le fait d'etre dedans va nous faire interrompre le shrink_down et re-grossir, du coup il faut le faire ici. L'inconvenient, c'est que quand on sort par les cotes, il n'y a soudain plus d'icone pointee, et donc le dock devient tout plat subitement au lieu de le faire doucement. Heureusement j'ai trouve une astuce. ^_^
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseX, &pDock->iMouseY, NULL);
	else
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseY, &pDock->iMouseX, NULL);

	if (pDock->iScrollOffset != 0 && mySystem.bResetScrollOnLeave)
	{
		//g_print ("iScrollOffset : %d\n", pDock->iScrollOffset);
		if (pDock->iScrollOffset < pDock->fFlatDockWidth / 2)
		{
			//pDock->iScrollOffset = pDock->iScrollOffset * mySystem.fScrollAcceleration;
			pDock->iScrollOffset -= MAX (2, ceil (pDock->iScrollOffset * (1 - mySystem.fScrollAcceleration)));
			if (pDock->iScrollOffset < 0)
				pDock->iScrollOffset = 0;
		}
		else
		{
			pDock->iScrollOffset += MAX (2, ceil ((pDock->fFlatDockWidth - pDock->iScrollOffset) * (1 - mySystem.fScrollAcceleration)));
			if (pDock->iScrollOffset > pDock->fFlatDockWidth)
				pDock->iScrollOffset = 0;
		}
		pDock->calculate_max_dock_size (pDock);
	}

	pDock->calculate_icons (pDock);
	//gtk_widget_queue_draw (pDock->pWidget);
	
	if (g_bEasterEggs && iPrevMagnitudeIndex != 0 && pDock->iMagnitudeIndex == 0 && pDock->bInside)  // on arrive en fin de retrecissement.
		cairo_dock_set_input_shape (pDock);
	
	if (! pDock->bInside)
		cairo_dock_replace_all_dialogs ();

	if (pDock->iMagnitudeIndex == 0)
	{
		if (pDock->bPopped && ! pDock->bInside)
			cairo_dock_pop_down (pDock);
		
		Icon *pBouncingIcon = cairo_dock_get_bouncing_icon (pDock->icons);
		Icon *pRemovingIcon = cairo_dock_get_removing_or_inserting_icon (pDock->icons);

		if (pBouncingIcon == NULL && pRemovingIcon == NULL && (! mySystem.bResetScrollOnLeave || pDock->iScrollOffset == 0))  // plus aucune animation en cours.
		{
			if (! (pDock->bAutoHide && pDock->iRefCount == 0) && ! pDock->bInside && ! pDock->bMenuVisible)
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

			pDock->calculate_icons (pDock);  // relance le grossissement si on est dedans.
			if (! pDock->bInside && pDock->iRefCount > 0)
			{
				//g_print ("on cache ce sous-dock en sortant par lui\n");
				gtk_widget_hide (pDock->pWidget);
				cairo_dock_hide_parent_dock (pDock);
			}

			//pDock->iSidShrinkDown = 0;
			pDock->bIsShrinkingDown = FALSE;
			cairo_dock_hide_dock_like_a_menu ();
			return pDock->bIsGrowingUp;
		}

		//\______________ Au moins une icone est en cours d'animation suite a un clique, on continue le 'shrink_down'.
		if (pRemovingIcon != NULL)
		{
			cd_debug ("au moins 1 icone en cours d'insertion/suppression (%f)", pRemovingIcon->fPersonnalScale);
			if (pRemovingIcon->fPersonnalScale == 0.05)
			{
				cd_debug ("  va etre supprimee");
				gboolean bIsAppli = CAIRO_DOCK_IS_NORMAL_APPLI (pRemovingIcon);  // car apres avoir ete enleve du dock elle n'est plus rien.
				cairo_dock_remove_icon_from_dock (pDock, pRemovingIcon);
				
				if (! g_bEasterEggs)
				{
					if (bIsAppli && pRemovingIcon->cClass != NULL && pDock == cairo_dock_search_dock_from_name (pRemovingIcon->cClass) && pDock->icons == NULL)  // il n'y a plus aucune icone de cette classe.
					{
						cd_message ("le sous-dock de la classe %s n'a plus d'element et va etre detruit", pRemovingIcon->cClass);
						cairo_dock_destroy_dock (pDock, pRemovingIcon->cClass, NULL, NULL);
					}
					else
					{
						cairo_dock_update_dock_size (pDock);
					}
				}
				else
				{
					if (bIsAppli && pRemovingIcon->cClass != NULL && pDock == cairo_dock_search_dock_from_name (pRemovingIcon->cClass))
					{
						if (pDock->icons == NULL)  // ne devrait plus arriver.
						{
							cd_message ("le sous-dock de la classe %s n'a plus d'element et va etre detruit", pRemovingIcon->cClass);
							
							CairoDock *pFakeParentDock = NULL;
							Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
							
							cairo_dock_destroy_dock (pDock, pRemovingIcon->cClass, NULL, NULL);
							pFakeClassIcon->pSubDock = NULL;
							
							cairo_dock_remove_icon_from_dock (pFakeParentDock, pFakeClassIcon);
							cairo_dock_free_icon (pFakeClassIcon);
							cairo_dock_update_dock_size (pFakeParentDock);
						}
						else if (pDock->icons->next == NULL)
						{
							cd_message ("le sous-dock de la classe %s n'a plus que 1 element et va etre vide puis detruit", pRemovingIcon->cClass);
							
							CairoDock *pFakeParentDock = NULL;
							Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
							
							Icon *pLastClassIcon = pDock->icons->data;
							pLastClassIcon->fOrder = pFakeClassIcon->fOrder;
							
							cairo_dock_destroy_dock (pDock, pRemovingIcon->cClass, pFakeParentDock, pFakeClassIcon->cParentDockName);
							pFakeClassIcon->pSubDock = NULL;
							
							cairo_dock_remove_icon_from_dock (pFakeParentDock, pFakeClassIcon);
							cairo_dock_free_icon (pFakeClassIcon);
							
							cairo_dock_redraw_my_icon (pLastClassIcon, CAIRO_CONTAINER (pFakeParentDock));  // on suppose que les tailles des 2 icones sont identiques.
						}
						else
						{
							cairo_dock_update_dock_size (pDock);
						}
					}
					else
					{
						cairo_dock_update_dock_size (pDock);
					}
				}
				cairo_dock_free_icon (pRemovingIcon);
			}
			else if (pRemovingIcon->fPersonnalScale == -0.05)
			{
				//g_print ("  fin\n");
				pRemovingIcon->fPersonnalScale = 0;
			}
		}
		return TRUE;
	}
	else
		return TRUE;
}


void cairo_dock_arm_animation (Icon *icon, CairoDockAnimationType iAnimationType, int iNbRounds)
{
	g_return_if_fail (icon != NULL);
	if (icon->iAnimationState > CAIRO_DOCK_STATE_CLICKED)
		return ;
	CairoDockIconType iType = cairo_dock_get_icon_type (icon);
	if (iAnimationType == -1)
		icon->iAnimationType = myIcons.tAnimationType[iType];
	else
		icon->iAnimationType = iAnimationType;

	if (icon->iAnimationType == CAIRO_DOCK_RANDOM)
		icon->iAnimationType = g_random_int_range (0, CAIRO_DOCK_NB_ANIMATIONS-1);  // [a;b[

	if (iNbRounds == -1)
		iNbRounds = myIcons.tNbAnimationRounds[iType];
	icon->iCount = MAX (0, g_tNbIterInOneRound[icon->iAnimationType] * iNbRounds - 1);
}

void cairo_dock_arm_animation_by_type (Icon *icon, CairoDockIconType iType)
{
	cairo_dock_arm_animation (icon, myIcons.tAnimationType[iType], myIcons.tNbAnimationRounds[iType]);
}

void cairo_dock_start_animation (Icon *icon, CairoDock *pDock)
{
	if (pDock == NULL)
	{
		icon->iCount = 0;
		return ;
	}
	cd_message ("%s (%s, %d)", __func__, icon->acName, icon->iAnimationType);
	if ((icon->iCount > 0 && icon->iAnimationType < CAIRO_DOCK_RANDOM) || icon->fPersonnalScale != 0)
	{
		icon->iAnimationState = CAIRO_DOCK_STATE_CLICKED;
		if (pDock->bIsGrowingUp)
		{
			pDock->fFoldingFactor = 0;  /// il ne revient pas a 0 tout seul ?...
			pDock->bIsGrowingUp = FALSE;
		}
		cairo_dock_start_shrinking (pDock);
		/*if (pDock->iSidGrowUp != 0)
		{
			pDock->fFoldingFactor = 0;
			g_source_remove (pDock->iSidGrowUp);
			pDock->iSidGrowUp = 0;
		}
		if (pDock->iSidShrinkDown == 0)
			pDock->iSidShrinkDown = g_timeout_add (50, (GSourceFunc) cairo_dock_shrink_down, (gpointer) pDock);  // fera diminuer de taille les icones, et rebondir/tourner/clignoter celle qui est animee.*/
	}
}


static gboolean _cairo_dock_gl_animation (CairoDock *pDock)
{
	//g_print ("%s (%d)\n", __func__, pDock->iMagnitudeIndex);
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fDeltaYReflection = 0;
		cairo_dock_manage_animations (icon, pDock);
	}
	
	gboolean bIconIsAnimating, bContinue = FALSE;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		bIconIsAnimating = FALSE;
		cairo_dock_notify (CAIRO_DOCK_UPDATE_ICON, icon, pDock, &bIconIsAnimating);
		bContinue |= bIconIsAnimating;
		if (! bIconIsAnimating)
			icon->iAnimationState = CAIRO_DOCK_STATE_REST;
	}

	cairo_dock_notify (CAIRO_DOCK_UPDATE_DOCK, pDock, &bContinue);
	
	if (pDock->bIsGrowingUp)
	{
		bContinue |= cairo_dock_grow_up (pDock);
		double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
		icon->fAlpha *= fDockMagnitude + myIcons.fAlphaAtRest * (1 - fDockMagnitude);
		gtk_widget_queue_draw (pDock->pWidget);
	}
	else if (pDock->bIsShrinkingDown)
	{
		bContinue |= cairo_dock_shrink_down (pDock);
		double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
		icon->fAlpha *= fDockMagnitude + myIcons.fAlphaAtRest * (1 - fDockMagnitude);
		gtk_widget_queue_draw (pDock->pWidget);
	}
	else if (g_bUseOpenGL && pDock->render_opengl)
		gtk_widget_queue_draw (pDock->pWidget);
	
	if (! bContinue)
	{
		pDock->iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}

void cairo_dock_launch_animation (CairoDock *pDock)
{
	if (pDock->iSidGLAnimation == 0)
	{
		if (pDock->render_opengl != NULL)
			pDock->iSidGLAnimation = g_timeout_add (g_iGLAnimationDeltaT, (GSourceFunc)_cairo_dock_gl_animation, pDock);
		else
			pDock->iSidGLAnimation = g_timeout_add (g_iCairoAnimationDeltaT, (GSourceFunc)_cairo_dock_gl_animation, pDock);
	}
}

void cairo_dock_start_shrinking (CairoDock *pDock)
{
	if (! pDock->bIsShrinkingDown)  // on lance l'animation.
	{
		pDock->bIsGrowingUp = FALSE;
		pDock->bIsShrinkingDown = TRUE;
		
		cairo_dock_launch_animation (pDock);
	}
}

void cairo_dock_start_growing (CairoDock *pDock)
{
	if (! pDock->bIsGrowingUp)  // on lance l'animation.
	{
		pDock->bIsShrinkingDown = FALSE;
		pDock->bIsGrowingUp = TRUE;
		
		cairo_dock_launch_animation (pDock);
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




gboolean cairo_dock_update_inserting_removing_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bContinueAnimation)
{
	if (pIcon->fPersonnalScale == 0)
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
	
	cairo_dock_update_removing_inserting_icon_size_default (pIcon);
	if (fabs (pIcon->fPersonnalScale) > 0.05)
		cairo_dock_mark_icon_as_inserting_removing (pIcon);
	return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
}

gboolean cairo_dock_on_insert_remove_icon_notification (gpointer pUserData, Icon *pIcon, CairoDock *pDock)
{
	cairo_dock_mark_icon_as_inserting_removing (pIcon);  // On prend en charge le dessin de l'icone pendant sa phase d'insertion/suppression.
	return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
}
