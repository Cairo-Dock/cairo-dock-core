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
extern gboolean g_bEasterEggs;
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

			pDock->calculate_max_dock_size (pDock);  // utilite ?
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
	//g_print ("%s (%d ; %2f ; bInside:%d)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->bInside);
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
	Icon *pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
	if (! pDock->bIsGrowingUp)
		return FALSE;
	
	if (pLastPointedIcon != pPointedIcon && pDock->bInside)
		cairo_dock_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);

	if (pDock->iMagnitudeIndex == CAIRO_DOCK_NB_MAX_ITERATIONS && pDock->fFoldingFactor == 0)  // fin de grossissement.
	{
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
	//g_print ("%s (%d, %f, %f)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->fDecorationsOffsetX);
	//\_________________ On fait decroitre la magnitude du dock.
	int iPrevMagnitudeIndex = pDock->iMagnitudeIndex;
	pDock->iMagnitudeIndex -= mySystem.iShrinkDownInterval;
	if (pDock->iMagnitudeIndex < 0)
		pDock->iMagnitudeIndex = 0;
	
	//\_________________ On replie le dock.
	if (pDock->fFoldingFactor != 0/** && (! mySystem.bResetScrollOnLeave || pDock->iScrollOffset == 0)*/)
	{
		pDock->fFoldingFactor = pow (pDock->fFoldingFactor, 2./3);
		if (pDock->fFoldingFactor > mySystem.fUnfoldAcceleration)
			pDock->fFoldingFactor = mySystem.fUnfoldAcceleration;
	}
	
	//\_________________ On remet les decorations a l'equilibre.
	pDock->fDecorationsOffsetX *= .8;
	if (fabs (pDock->fDecorationsOffsetX) < 3)
		pDock->fDecorationsOffsetX = 0.;
	
	//\_________________ On remet les icones a l'equilibre.
	/**if (pDock->iScrollOffset != 0 && mySystem.bResetScrollOnLeave)
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
	}*/
	
	//\_________________ On recupere la position de la souris pour le cas ou on est hors du dock.
	if (pDock->bHorizontalDock)  // ce n'est pas le motion_notify qui va nous donner des coordonnees en dehors du dock, et donc le fait d'etre dedans va nous faire interrompre le shrink_down et re-grossir, du coup il faut le faire ici. L'inconvenient, c'est que quand on sort par les cotes, il n'y a soudain plus d'icone pointee, et donc le dock devient tout plat subitement au lieu de le faire doucement. Heureusement j'ai trouve une astuce. ^_^
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseX, &pDock->iMouseY, NULL);
	else
		gdk_window_get_pointer (pDock->pWidget->window, &pDock->iMouseY, &pDock->iMouseX, NULL);
	
	//\_________________ On recalcule les icones.
	cairo_dock_calculate_dock_icons (pDock);
	if (! pDock->bIsShrinkingDown)
		return FALSE;
	
	if (g_bEasterEggs && iPrevMagnitudeIndex != 0 && pDock->iMagnitudeIndex == 0 && pDock->bInside)  // on arrive en fin de retrecissement.
		cairo_dock_set_input_shape (pDock);
	
	if (! pDock->bInside)
		cairo_dock_replace_all_dialogs ();

	if (/**(pDock->iScrollOffset == 0 || ! mySystem.bResetScrollOnLeave) &&*/
		(pDock->iMagnitudeIndex == 0) &&
		(pDock->fDecorationsOffsetX == 0) &&
		(pDock->fFoldingFactor == 0 || pDock->fFoldingFactor == mySystem.fUnfoldAcceleration))
	{
		//g_print ("equilibre atteint (%d)\n", pDock->bInside);
		if (! pDock->bInside)
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
		return FALSE;
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
			cd_message (" - %s va etre supprimee", pIcon->acName);
			gboolean bIsAppli = CAIRO_DOCK_IS_NORMAL_APPLI (pIcon);  // car apres avoir ete enleve du dock elle n'est plus rien.
			cairo_dock_remove_icon_from_dock (pDock, pIcon);
			
			/*if (! g_bEasterEggs)
			{
				if (pIcon->cClass != NULL && pDock == cairo_dock_search_dock_from_name (pIcon->cClass) && pDock->icons == NULL)  // il n'y a plus aucune icone de cette classe.
				{
					cd_message ("   le sous-dock de la classe %s n'a plus d'element et va etre detruit", pIcon->cClass);
					cairo_dock_destroy_dock (pDock, pIcon->cClass, NULL, NULL);
					return FALSE;
				}
				else
				{
					cairo_dock_update_dock_size (pDock);
				}
			}
			else*/
			{
				if (pIcon->cClass != NULL && pDock == cairo_dock_search_dock_from_name (pIcon->cClass))
				{
					gboolean bEmptyClassSubDock = cairo_dock_check_class_subdock_is_empty (pDock, pIcon->cClass);
					if (bEmptyClassSubDock)
						return FALSE;
					/*if (pDock->icons == NULL)  // ne devrait plus arriver.
					{
						cd_warning ("   le sous-dock de la classe %s n'a plus d'element !\nil va etre detruit", pIcon->cClass);
						CairoDock *pFakeParentDock = NULL;
						Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
						cairo_dock_destroy_dock (pDock, pIcon->cClass, NULL, NULL);
						pFakeClassIcon->pSubDock = NULL;
						cairo_dock_remove_icon_from_dock (pFakeParentDock, pFakeClassIcon);
						cairo_dock_free_icon (pFakeClassIcon);
						cairo_dock_update_dock_size (pFakeParentDock);
						return FALSE;
					}
					else if (pDock->icons->next == NULL)
					{
						g_print ("   le sous-dock de la classe %s n'a plus que 1 element et va etre vide puis detruit\n", pIcon->cClass);
						Icon *pLastClassIcon = pDock->icons->data;
						
						CairoDock *pFakeParentDock = NULL;
						Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
						g_return_val_if_fail (pFakeClassIcon != NULL, TRUE);
						if (CAIRO_DOCK_IS_NORMAL_LAUNCHER (pFakeClassIcon) || CAIRO_DOCK_IS_APPLET (pFakeClassIcon))
						{
							cairo_dock_detach_icon_from_dock (pLastClassIcon, pDock, FALSE);
							g_free (pLastClassIcon->cParentDockName);
							pLastClassIcon->cParentDockName = NULL;
							
							cairo_dock_destroy_dock (pDock, pIcon->cClass, NULL, NULL);
							pFakeClassIcon->pSubDock = NULL;
							g_print ("sanity check : pFakeClassIcon->Xid : %d\n", pFakeClassIcon->Xid);
							cairo_dock_insert_appli_in_dock (pLastClassIcon, g_pMainDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
						}
						else  // le sous-dock est donc pointe par un inhibiteur.
						{
							g_print ("trouve l'icone en papier (%x;%x)\n", pFakeClassIcon, pFakeParentDock);
							cairo_dock_detach_icon_from_dock (pLastClassIcon, pDock, FALSE);
							g_free (pLastClassIcon->cParentDockName);
							pLastClassIcon->cParentDockName = g_strdup (pFakeClassIcon->cParentDockName);
							pLastClassIcon->fOrder = pFakeClassIcon->fOrder;
							
							g_print (" on detruit le sous-dock...\n");
							cairo_dock_destroy_dock (pDock, pIcon->cClass, NULL, NULL);
							pFakeClassIcon->pSubDock = NULL;
							
							g_print (" et l'icone de paille\n");
							cairo_dock_remove_icon_from_dock (pFakeParentDock, pFakeClassIcon);
							cairo_dock_free_icon (pFakeClassIcon);
							
							g_print (" puis on re-insere l'appli restante\n");
							cairo_dock_insert_icon_in_dock (pLastClassIcon, pFakeParentDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, FALSE);
							cairo_dock_redraw_icon (pLastClassIcon, CAIRO_CONTAINER (pFakeParentDock));  // on suppose que les tailles des 2 icones sont identiques.
						}
						return FALSE;
					}
					else
					{
						cairo_dock_update_dock_size (pDock);
					}*/
				}
				cairo_dock_update_dock_size (pDock);
			}
			cairo_dock_free_icon (pIcon);
		}
		else if (pIcon->fPersonnalScale == -0.05)
		{
			pIcon->fPersonnalScale = 0;
			cairo_dock_notify (CAIRO_DOCK_STOP_ICON, pIcon);
			pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
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
	if (pDock->bIsShrinkingDown)
	{
		pDock->bIsShrinkingDown = cairo_dock_shrink_down (pDock);
		//g_print ("pDock->bIsShrinkingDown <- %d\n", pDock->bIsShrinkingDown);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	}
	if (pDock->bIsGrowingUp)
	{
		pDock->bIsGrowingUp = cairo_dock_grow_up (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	}
	//g_print (" => %d, %d\n", pDock->bIsShrinkingDown, pDock->bIsGrowingUp);
	
	gboolean bContinue = pDock->bIsShrinkingDown || pDock->bIsGrowingUp;
	
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
			cairo_dock_notify (CAIRO_DOCK_UPDATE_ICON_SLOW, icon, pDock, &bIconIsAnimating);
			pDock->bKeepSlowAnimation |= bIconIsAnimating;
		}
		cairo_dock_notify (CAIRO_DOCK_UPDATE_ICON, icon, pDock, &bIconIsAnimating);
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
		cairo_dock_notify (CAIRO_DOCK_UPDATE_DOCK_SLOW, pDock, &pDock->bKeepSlowAnimation);
	}
	cairo_dock_notify (CAIRO_DOCK_UPDATE_DOCK, pDock, &bContinue);
	
	if (! bContinue && ! pDock->bKeepSlowAnimation)
	{
		pDock->iSidGLAnimation = 0;
		pDock->bIsGrowingUp = FALSE;
		pDock->bIsShrinkingDown = FALSE;
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
			cairo_dock_notify (CAIRO_DOCK_UPDATE_ICON_SLOW, pDesklet->pIcon, pDesklet, &bIconIsAnimating);
			pDesklet->bKeepSlowAnimation |= bIconIsAnimating;
		}
		
		cairo_dock_notify (CAIRO_DOCK_UPDATE_ICON, pDesklet->pIcon, pDesklet, &bIconIsAnimating);
		if (! bIconIsAnimating)
			pDesklet->pIcon->iAnimationState = CAIRO_DOCK_STATE_REST;
		else
			bContinue = TRUE;
	}
	
	if (bUpdateSlowAnimation)
	{
		cairo_dock_notify (CAIRO_DOCK_UPDATE_DESKLET_SLOW, pDesklet, &pDesklet->bKeepSlowAnimation);
	}
	
	cairo_dock_notify (CAIRO_DOCK_UPDATE_DESKLET, pDesklet, &bContinue);
	
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
			default :
				cd_warning ("This type of container has no animation capability yet");
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
	
	cairo_dock_register_notification (bFastPace ? CAIRO_DOCK_UPDATE_ICON : CAIRO_DOCK_UPDATE_ICON_SLOW,
		(CairoDockNotificationFunc) _cairo_dock_transition_step, CAIRO_DOCK_RUN_FIRST, pUserData);
	
	cairo_dock_launch_animation (pContainer);
}

void cairo_dock_remove_transition_on_icon (Icon *pIcon)
{
	CairoDockTransition *pTransition = cairo_dock_get_transition (pIcon);
	if (pTransition == NULL)
		return ;
	
	cairo_dock_remove_notification_func (pTransition->bFastPace ? CAIRO_DOCK_UPDATE_ICON : CAIRO_DOCK_UPDATE_ICON_SLOW,
		(CairoDockNotificationFunc) _cairo_dock_transition_step,
		pTransition->pUserData);

	g_free (pTransition);
	cairo_dock_set_transition (pIcon, NULL);
}
