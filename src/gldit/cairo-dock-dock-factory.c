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
#include <gtk/gtk.h>

#include "cairo-dock-applications-manager.h"  // myTaskbarParam.bHideVisibleApplis
#include "cairo-dock-module-factory.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-separator-manager.h"  // cairo_dock_insert_automatic_separator_in_dock
#include "cairo-dock-launcher-manager.h"  // cairo_dock_create_icon_from_desktop_file
#include "cairo-dock-backends-manager.h"  // myBackendsParam.fSubDockSizeRatio
#include "cairo-dock-X-utilities.h"  // cairo_dock_set_xicon_geometry
#include "cairo-dock-log.h"
#include "cairo-dock-application-facility.h"  // cairo_dock_detach_appli
#include "cairo-dock-dialog-manager.h"  // cairo_dock_replace_all_dialogs
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-class-manager.h"   // cairo_dock_get_class_subdock
#include "cairo-dock-animations.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-draw-opengl.h"  // for the redirected texture
#include "cairo-dock-data-renderer.h"  // cairo_dock_reload_data_renderer_on_icon

#include "cairo-dock-dock-factory.h"

extern gchar *g_cCurrentLaunchersPath;

extern CairoDockGLConfig g_openglConfig;
extern CairoDockHidingEffect *g_pHidingBackend;
extern gboolean g_bUseOpenGL;


static gboolean _cairo_dock_grow_up (CairoDock *pDock)
{
	//g_print ("%s (%d ; %2f ; bInside:%d)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->container.bInside);
	
	pDock->iMagnitudeIndex += myBackendsParam.iGrowUpInterval;
	if (pDock->iMagnitudeIndex > CAIRO_DOCK_NB_MAX_ITERATIONS)
		pDock->iMagnitudeIndex = CAIRO_DOCK_NB_MAX_ITERATIONS;

	if (pDock->fFoldingFactor != 0)
	{
		int iAnimationDeltaT = cairo_dock_get_animation_delta_t (pDock);
		pDock->fFoldingFactor -= (double) iAnimationDeltaT / myBackendsParam.iUnfoldingDuration;
		if (pDock->fFoldingFactor < 0)
			pDock->fFoldingFactor = 0;
	}
	
	gldi_container_update_mouse_position (CAIRO_CONTAINER (pDock));
	
	Icon *pLastPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	Icon *pPointedIcon = cairo_dock_calculate_dock_icons (pDock);
	if (! pDock->bIsGrowingUp)
		return FALSE;
	
	if (pLastPointedIcon != pPointedIcon && pDock->container.bInside)
		cairo_dock_on_change_icon (pLastPointedIcon, pPointedIcon, pDock);

	if (pDock->iMagnitudeIndex == CAIRO_DOCK_NB_MAX_ITERATIONS && pDock->fFoldingFactor == 0)  // fin de grossissement et de depliage.
	{
		/// TODO: check doing this in update_dock_size directly...
		/**if (pDock->bWMIconsNeedUpdate)
		{
			cairo_dock_trigger_set_WM_icons_geometry (pDock);
			pDock->bWMIconsNeedUpdate = FALSE;
		}*/
		
		cairo_dock_replace_all_dialogs ();
		return FALSE;
	}
	else
		return TRUE;
}

static gboolean _cairo_dock_shrink_down (CairoDock *pDock)
{
	//g_print ("%s (%d, %f, %f)\n", __func__, pDock->iMagnitudeIndex, pDock->fFoldingFactor, pDock->fDecorationsOffsetX);
	//\_________________ On fait decroitre la magnitude du dock.
	pDock->iMagnitudeIndex -= myBackendsParam.iShrinkDownInterval;
	if (pDock->iMagnitudeIndex < 0)
		pDock->iMagnitudeIndex = 0;
	
	//\_________________ On replie le dock.
	if (pDock->fFoldingFactor != 0 && pDock->fFoldingFactor != 1)
	{
		int iAnimationDeltaT = cairo_dock_get_animation_delta_t (pDock);
		pDock->fFoldingFactor += (double) iAnimationDeltaT / myBackendsParam.iUnfoldingDuration;
		if (pDock->fFoldingFactor > 1)
			pDock->fFoldingFactor = 1;
	}
	
	//\_________________ On remet les decorations a l'equilibre.
	pDock->fDecorationsOffsetX *= .8;
	if (fabs (pDock->fDecorationsOffsetX) < 3)
		pDock->fDecorationsOffsetX = 0.;
	
	//\_________________ On recupere la position de la souris manuellement (car a priori on est hors du dock).
	gldi_container_update_mouse_position (CAIRO_CONTAINER (pDock));  // ce n'est pas le motion_notify qui va nous donner des coordonnees en dehors du dock, et donc le fait d'etre dedans va nous faire interrompre le shrink_down et re-grossir, du coup il faut le faire ici. L'inconvenient, c'est que quand on sort par les cotes, il n'y a soudain plus d'icone pointee, et donc le dock devient tout plat subitement au lieu de le faire doucement. Heureusement j'ai trouve une astuce. ^_^
	
	//\_________________ On recalcule les icones.
	///if (iPrevMagnitudeIndex != 0)
	{
		cairo_dock_calculate_dock_icons (pDock);
		if (! pDock->bIsShrinkingDown)
			return FALSE;
		
		///cairo_dock_replace_all_dialogs ();
	}

	if (pDock->iMagnitudeIndex == 0 && (pDock->fFoldingFactor == 0 || pDock->fFoldingFactor == 1))  // on est arrive en bas.
	{
		//g_print ("equilibre atteint (%d)\n", pDock->container.bInside);
		if (! pDock->container.bInside)  // on peut etre hors des icones sans etre hors de la fenetre.
		{
			//g_print ("rideau !\n");
			
			//\__________________ On repasse derriere si on etait devant.
			if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && ! pDock->bIsBelow)
				cairo_dock_pop_down (pDock);
			
			//\__________________ On se redimensionne en taille normale.
			if (! pDock->bAutoHide && pDock->iRefCount == 0/** && ! pDock->bMenuVisible*/)  // fin de shrink sans auto-hide => taille normale.
			{
				//g_print ("taille normale (%x; %d)\n", pDock->pShapeBitmap , pDock->iInputState);
				if (pDock->pShapeBitmap && pDock->iInputState != CAIRO_DOCK_INPUT_AT_REST)
				{
					//g_print ("+++ input shape at rest on end shrinking\n");
					cairo_dock_set_input_shape_at_rest (pDock);
					pDock->iInputState = CAIRO_DOCK_INPUT_AT_REST;
					///cairo_dock_replace_all_dialogs ();
				}
			}
			
			//\__________________ On se cache si sous-dock.
			if (pDock->iRefCount > 0)
			{
				//g_print ("on cache ce sous-dock en sortant par lui\n");
				gtk_widget_hide (pDock->container.pWidget);
				cairo_dock_hide_parent_dock (pDock);
			}
			cairo_dock_hide_after_shortcut ();
		}
		else
		{
			cairo_dock_calculate_dock_icons (pDock);  // relance le grossissement si on est dedans.
		}
		if (!pDock->bIsGrowingUp)
			cairo_dock_replace_all_dialogs ();
		return (!pDock->bIsGrowingUp && (pDock->fDecorationsOffsetX != 0 || (pDock->fFoldingFactor != 0 && pDock->fFoldingFactor != 1)));
	}
	else
	{
		return (!pDock->bIsGrowingUp);
	}
}

static gboolean _cairo_dock_hide (CairoDock *pDock)
{
	//g_print ("%s (%d, %.2f, %.2f)\n", __func__, pDock->iMagnitudeIndex, pDock->fHideOffset, pDock->fPostHideOffset);
	
	if (pDock->fHideOffset < 1)  // the hiding animation is running.
	{
		pDock->fHideOffset += 1./myBackendsParam.iHideNbSteps;
		if (pDock->fHideOffset > .99)  // fin d'anim.
		{
			pDock->fHideOffset = 1;
			
			//g_print ("on arrete le cachage\n");
			gboolean bVisibleIconsPresent = FALSE;
			Icon *pIcon;
			GList *ic;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				pIcon = ic->data;
				if (pIcon->fInsertRemoveFactor != 0)  // on accelere l'animation d'apparition/disparition.
				{
					if (pIcon->fInsertRemoveFactor > 0)
						pIcon->fInsertRemoveFactor = 0.05;
					else
						pIcon->fInsertRemoveFactor = - 0.05;
				}
				
				if (! pIcon->bIsDemandingAttention && ! pIcon->bAlwaysVisible)
					cairo_dock_stop_icon_animation (pIcon);  // s'il y'a une autre animation en cours, on l'arrete.
				else
					bVisibleIconsPresent = TRUE;
			}
			
			pDock->pRenderer->calculate_icons (pDock);
			///pDock->fFoldingFactor = (myBackendsParam.bAnimateOnAutoHide ? .99 : 0.);  // on arme le depliage.
			cairo_dock_allow_entrance (pDock);
			
			cairo_dock_replace_all_dialogs ();
			
			if (bVisibleIconsPresent)  // il y'a des icones a montrer progressivement, on reste dans la boucle.
			{
				pDock->fPostHideOffset = 0.05;
				return TRUE;
			}
			else
			{
				pDock->fPostHideOffset = 1;  // pour que les icones demandant l'attention plus tard soient visibles.
				return FALSE;
			}
		}
	}
	else if (pDock->fPostHideOffset > 0 && pDock->fPostHideOffset < 1)  // the post-hiding animation is running.
	{
		pDock->fPostHideOffset += 1./myBackendsParam.iHideNbSteps;
		if (pDock->fPostHideOffset > .99)
		{
			pDock->fPostHideOffset = 1.;
			return FALSE;
		}
	}
	else  // else no hiding animation is running.
		return FALSE;
	return TRUE;
}

static gboolean _cairo_dock_show (CairoDock *pDock)
{
	pDock->fHideOffset -= 1./myBackendsParam.iUnhideNbSteps;
	if (pDock->fHideOffset < 0.01)
	{
		pDock->fHideOffset = 0;
		cairo_dock_allow_entrance (pDock);
		cairo_dock_replace_all_dialogs ();  // we need it here so that a modal dialog is replaced when the dock unhides (else it would stay behind).
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
		if (pIcon->fInsertRemoveFactor == (gdouble)0.05)
		{
			gboolean bIsAppli = CAIRO_DOCK_IS_NORMAL_APPLI (pIcon);
			if (bIsAppli && pIcon->iLastCheckTime != -1)  // c'est une icone d'appli non vieille qui disparait, elle s'est probablement cachee => on la detache juste.
			{
				cd_message ("cette (%s) appli est toujours valide, on la detache juste", pIcon->cName);
				pIcon->fInsertRemoveFactor = 0.;  // on le fait avant le reload, sinon l'icone n'est pas rechargee.
				if (!pIcon->bIsHidden && myTaskbarParam.bHideVisibleApplis)  // on lui remet l'image normale qui servira d'embleme lorsque l'icone sera inseree a nouveau dans le dock.
					cairo_dock_reload_icon_image (pIcon, CAIRO_CONTAINER (pDock));
				pDock = cairo_dock_detach_appli (pIcon);
				if (pDock == NULL)  // the dock has been destroyed (empty class sub-dock).
				{
					cairo_dock_free_icon (pIcon);
					return FALSE;
				}
			}
			else
			{
				cd_message (" - %s va etre supprimee", pIcon->cName);
				cairo_dock_remove_icon_from_dock (pDock, pIcon);  // enleve le separateur automatique avec; supprime le .desktop et le sous-dock des lanceurs; stoppe les applets; marque le theme.
				
				if (pIcon->cClass != NULL && pDock == cairo_dock_get_class_subdock (pIcon->cClass))  // appli icon in its class sub-dock => destroy the class sub-dock if it becomes empty (we don't want an empty sub-dock).
				{
					gboolean bEmptyClassSubDock = cairo_dock_check_class_subdock_is_empty (pDock, pIcon->cClass);
					if (bEmptyClassSubDock)
					{
						cairo_dock_free_icon (pIcon);
						return FALSE;
					}
				}
				
				cairo_dock_free_icon (pIcon);
			}
		}
		else if (pIcon->fInsertRemoveFactor == (gdouble)-0.05)
		{
			pIcon->fInsertRemoveFactor = 0;  // cela n'arrete pas l'animation, qui peut se poursuivre meme apres que l'icone ait atteint sa taille maximale.
			bRecalculateIcons = TRUE;
		}
		else if (pIcon->fInsertRemoveFactor != 0)
		{
			bRecalculateIcons = TRUE;
		}
		ic = next_ic;
	}
	
	if (bRecalculateIcons)
		cairo_dock_calculate_dock_icons (pDock);
	return TRUE;
}

static gboolean _cairo_dock_dock_animation_loop (CairoContainer *pContainer)
{
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	gboolean bContinue = FALSE;
	gboolean bUpdateSlowAnimation = FALSE;
	pContainer->iAnimationStep ++;
	if (pContainer->iAnimationStep * pContainer->iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pContainer->iAnimationStep = 0;
		pContainer->bKeepSlowAnimation = FALSE;
	}
	
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
		//g_print ("le dock se cache\n");
		pDock->bIsHiding = _cairo_dock_hide (pDock);
		gtk_widget_queue_draw (pContainer->pWidget);  // on n'utilise pas cairo_dock_redraw_container, sinon a la derniere iteration, le dock etant cache, la fonction ne le redessine pas.
		bContinue |= pDock->bIsHiding;
	}
	if (pDock->bIsShowing)
	{
		pDock->bIsShowing = _cairo_dock_show (pDock);
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
		bContinue |= pDock->bIsShowing;
	}
	//g_print (" => %d, %d\n", pDock->bIsShrinkingDown, pDock->bIsGrowingUp);
	
	double fDockMagnitude = cairo_dock_calculate_magnitude (pDock->iMagnitudeIndex);
	gboolean bIconIsAnimating;
	gboolean bNoMoreDemandingAttention = FALSE;
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		
		icon->fDeltaYReflection = 0;
		if (myIconsParam.fAlphaAtRest != 1)
			icon->fAlpha = fDockMagnitude + myIconsParam.fAlphaAtRest * (1 - fDockMagnitude);
		
		bIconIsAnimating = FALSE;
		if (bUpdateSlowAnimation)
		{
			cairo_dock_notify_on_object (icon, NOTIFICATION_UPDATE_ICON_SLOW, icon, pDock, &bIconIsAnimating);
			pContainer->bKeepSlowAnimation |= bIconIsAnimating;
		}
		cairo_dock_notify_on_object (icon, NOTIFICATION_UPDATE_ICON, icon, pDock, &bIconIsAnimating);
		
		if ((icon->bIsDemandingAttention || icon->bAlwaysVisible) && cairo_dock_is_hidden (pDock))  // animation d'une icone demandant l'attention dans un dock cache => on force le dessin qui normalement ne se fait pas.
		{
			gtk_widget_queue_draw (pContainer->pWidget);
		}
		
		bContinue |= bIconIsAnimating;
		if (! bIconIsAnimating)
		{
			icon->iAnimationState = CAIRO_DOCK_STATE_REST;
			if (icon->bIsDemandingAttention)
			{
				icon->bIsDemandingAttention = FALSE;
				bNoMoreDemandingAttention = TRUE;
			}
		}
	}
	bContinue |= pContainer->bKeepSlowAnimation;
	
	if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && bNoMoreDemandingAttention && ! pDock->bIsBelow && ! pContainer->bInside)
	{
		//g_print ("plus de raison d'etre devant\n");
		cairo_dock_pop_down (pDock);
	}
	
	if (! _cairo_dock_handle_inserting_removing_icons (pDock))
	{
		cd_debug ("ce dock n'a plus de raison d'etre");
		return FALSE;
	}
	
	if (bUpdateSlowAnimation)
	{
		cairo_dock_notify_on_object (pDock, NOTIFICATION_UPDATE_SLOW, pDock, &pContainer->bKeepSlowAnimation);
	}
	cairo_dock_notify_on_object (pDock, NOTIFICATION_UPDATE, pDock, &bContinue);
	
	if (! bContinue && ! pContainer->bKeepSlowAnimation)
	{
		pContainer->iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}

static gboolean _on_dock_destroyed (GtkWidget *menu, CairoContainer *pContainer);
static void _on_menu_deactivated (G_GNUC_UNUSED GtkMenuShell *menu, CairoDock *pDock)
{
	//g_print ("\n+++ %s ()\n\n", __func__);
	g_return_if_fail (CAIRO_DOCK_IS_DOCK (pDock));
	if (pDock->bHasModalWindow)  // don't send the signal if the menu was already deactivated.
	{
		pDock->bHasModalWindow = FALSE;
		cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
	}
}
static void _on_menu_destroyed (GtkWidget *menu, CairoDock *pDock)
{
	//g_print ("\n+++ %s ()\n\n", __func__);
	cairo_dock_remove_notification_func_on_object (pDock,
		NOTIFICATION_DESTROY,
		(CairoDockNotificationFunc) _on_dock_destroyed,
		menu);
}
static gboolean _on_dock_destroyed (GtkWidget *menu, CairoContainer *pContainer)
{
	//g_print ("\n+++ %s ()\n\n", __func__);
	g_signal_handlers_disconnect_matched
		(menu,
		G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
		0,
		0,
		NULL,
		_on_menu_deactivated,
		pContainer);
	g_signal_handlers_disconnect_matched
		(menu,
		G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
		0,
		0,
		NULL,
		_on_menu_destroyed,
		pContainer);
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}
static void _setup_menu (CairoContainer *pContainer, G_GNUC_UNUSED Icon *pIcon, GtkWidget *pMenu)
{
	// keep the dock visible
	CAIRO_DOCK (pContainer)->bHasModalWindow = TRUE;
	
	// connect signals
	if (g_signal_handler_find (pMenu,
		G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
		0,
		0,
		NULL,
		_on_menu_deactivated,
		pContainer) == 0)  // on evite de connecter 2 fois ce signal, donc la fonction est appelable plusieurs fois sur un meme menu.
	{
		// when the menu is deactivated, hide the dock back if necessary.
		g_signal_connect (G_OBJECT (pMenu),
			"deactivate",
			G_CALLBACK (_on_menu_deactivated),
			pContainer);
		// when the menu is destroyed, remove the 'destroyed' notification on the dock.
		g_signal_connect (G_OBJECT (pMenu),
			"destroy",
			G_CALLBACK (_on_menu_destroyed),
			pContainer);
		// when the dock is destroyed, remove the 2 signals on the menu.
		cairo_dock_register_notification_on_object (pContainer,
			NOTIFICATION_DESTROY,
			(CairoDockNotificationFunc) _on_dock_destroyed,
			CAIRO_DOCK_RUN_AFTER, pMenu);  // the menu can stay alive even if the container disappear, so we need to ensure we won't call the callbacks then.
	}
}


CairoDock *cairo_dock_new_dock (void)
{
	//\__________________ create a dock.
	CairoDock * pDock = gldi_container_new (CairoDock, &myDocksMgr, CAIRO_DOCK_TYPE_DOCK);
	
	//\__________________ initialize its parameters (it's a root dock by default)
	pDock->container.iface.animation_loop = _cairo_dock_dock_animation_loop;
	pDock->container.iface.setup_menu = _setup_menu;
	
	pDock->iAvoidingMouseIconType = -1;
	pDock->fFlatDockWidth = - myIconsParam.iIconGap;
	pDock->fMagnitudeMax = 1.;
	pDock->fPostHideOffset = 1.;
	pDock->iInputState = CAIRO_DOCK_INPUT_AT_REST;  // le dock est cree au repos. La zone d'input sera mis en place lors du configure.
	
	//\__________________ set up its window
	GtkWidget *pWindow = pDock->container.pWidget;
	gtk_container_set_border_width (GTK_CONTAINER (pWindow), 0);
	gtk_window_set_gravity (GTK_WINDOW (pWindow), GDK_GRAVITY_STATIC);
	gtk_window_set_type_hint (GTK_WINDOW (pWindow), GDK_WINDOW_TYPE_HINT_DOCK);
	gtk_window_set_title (GTK_WINDOW (pWindow), "cairo-dock");
	
	cairo_dock_register_notification_on_object (pDock,
		NOTIFICATION_RENDER,
		(CairoDockNotificationFunc) cairo_dock_render_dock_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	
	//\__________________ connect to events.
	gtk_widget_add_events (pWindow,
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK |
		GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
		GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	
	g_signal_connect (G_OBJECT (pWindow),
		#if (GTK_MAJOR_VERSION < 3)
		"expose-event",
		#else
		"draw",
		#endif
		G_CALLBACK (cairo_dock_on_expose),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (cairo_dock_on_configure),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"key-release-event",
		G_CALLBACK (cairo_dock_on_key_release),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"key-press-event",
		G_CALLBACK (cairo_dock_on_key_release),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-press-event",
		G_CALLBACK (cairo_dock_on_button_press),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-release-event",
		G_CALLBACK (cairo_dock_on_button_press),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"scroll-event",
		G_CALLBACK (cairo_dock_on_scroll),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"motion-notify-event",
		G_CALLBACK (cairo_dock_on_motion_notify),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"enter-notify-event",
		G_CALLBACK (cairo_dock_on_enter_notify),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"leave-notify-event",
		G_CALLBACK (cairo_dock_on_leave_notify),
		pDock);
	gldi_container_enable_drop (CAIRO_CONTAINER (pDock),
		G_CALLBACK (cairo_dock_on_drag_data_received),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag-motion",
		G_CALLBACK (cairo_dock_on_drag_motion),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag-leave",
		G_CALLBACK (cairo_dock_on_drag_leave),
		pDock);
	/*g_signal_connect (G_OBJECT (pWindow),
		"drag-drop",
		G_CALLBACK (cairo_dock_on_drag_drop),
		pDock);*/

	gtk_window_get_size (GTK_WINDOW (pWindow), &pDock->container.iWidth, &pDock->container.iHeight);  // ca n'est que la taille initiale allouee par GTK.
	gtk_widget_show_all (pWindow);
	#if (GTK_MAJOR_VERSION < 3)
	gdk_window_set_back_pixmap (pWindow->window, NULL, FALSE);
	#else
	gdk_window_set_background_pattern (gldi_container_get_gdk_window (CAIRO_CONTAINER (pDock)), NULL);
	#endif
	return pDock;
}

void cairo_dock_free_dock (CairoDock *pDock)
{
	if (pDock->iSidUnhideDelayed != 0)
		g_source_remove (pDock->iSidUnhideDelayed);
	if (pDock->iSidHideBack != 0)
		g_source_remove (pDock->iSidHideBack);
	if (pDock->iSidMoveResize != 0)
		g_source_remove (pDock->iSidMoveResize);
	if (pDock->iSidLeaveDemand != 0)
		g_source_remove (pDock->iSidLeaveDemand);
	if (pDock->iSidUpdateWMIcons != 0)
		g_source_remove (pDock->iSidUpdateWMIcons);
	if (pDock->iSidLoadBg != 0)
		g_source_remove (pDock->iSidLoadBg);
	if (pDock->iSidDestroyEmptyDock != 0)
		g_source_remove (pDock->iSidDestroyEmptyDock);
	if (pDock->iSidTestMouseOutside != 0)
		g_source_remove (pDock->iSidTestMouseOutside);
	if (pDock->iSidUpdateDockSize != 0)
		g_source_remove (pDock->iSidUpdateDockSize);
	
	GList *icons = pDock->icons;
	pDock->icons = NULL;  // remove the icons first, to avoid any use of 'icons' in the 'destroy' callbacks.
	g_list_foreach (icons, (GFunc) cairo_dock_free_icon, NULL);
	g_list_free (icons);
	
	if (pDock->pShapeBitmap != NULL)
		gldi_shape_destroy (pDock->pShapeBitmap);
	
	if (pDock->pHiddenShapeBitmap != NULL)
		gldi_shape_destroy (pDock->pHiddenShapeBitmap);
	
	if (pDock->pActiveShapeBitmap != NULL)
		gldi_shape_destroy (pDock->pActiveShapeBitmap);
	
	if (pDock->pRenderer != NULL && pDock->pRenderer->free_data != NULL)
	{
		pDock->pRenderer->free_data (pDock);
	}
	
	g_free (pDock->cRendererName);
	
	g_free (pDock->cBgImagePath);
	cairo_dock_unload_image_buffer (&pDock->backgroundBuffer);
	
	if (pDock->iFboId != 0)
		glDeleteFramebuffersEXT (1, &pDock->iFboId);
	if (pDock->iRedirectedTexture != 0)
		_cairo_dock_delete_texture (pDock->iRedirectedTexture);
	
	cairo_dock_finish_container (CAIRO_CONTAINER (pDock));  // -> NOTIFICATION_DESTROY
	
	g_free (pDock);
}

void cairo_dock_make_sub_dock (CairoDock *pDock, CairoDock *pParentDock, const gchar *cRendererName)
{
	//\__________________ set sub-dock flag
	pDock->iRefCount = 1;
	gtk_window_set_title (GTK_WINDOW (pDock->container.pWidget), "cairo-dock-sub");
	
	//\__________________ set the orientation relatively to the parent dock
	pDock->container.bIsHorizontal = pParentDock->container.bIsHorizontal;
	pDock->container.bDirectionUp = pParentDock->container.bDirectionUp;
	pDock->iNumScreen = pParentDock->iNumScreen;
	
	//\__________________ set a renderer
	cairo_dock_set_renderer (pDock, cRendererName);
	
	//\__________________ update the icons size and the ratio.
	double fPrevRatio = pDock->container.fRatio;
	pDock->container.fRatio = MIN (pDock->container.fRatio, myBackendsParam.fSubDockSizeRatio);
	pDock->iIconSize = pParentDock->iIconSize;
	
	Icon *icon;
	GList *ic;
	pDock->fFlatDockWidth = - myIconsParam.iIconGap;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		cairo_dock_icon_set_requested_size (icon, 0, 0);  // no request
		icon->fWidth = icon->fHeight = 0;  /// useful ?...
		cairo_dock_set_icon_size_in_dock (pDock, icon);
		pDock->fFlatDockWidth += icon->fWidth + myIconsParam.iIconGap;
	}
	pDock->iMaxIconHeight *= pDock->container.fRatio / fPrevRatio;
	
	//\__________________ remove any input shape
	if (pDock->pShapeBitmap != NULL)
	{
		gldi_shape_destroy (pDock->pShapeBitmap);
		pDock->pShapeBitmap = NULL;
		if (pDock->iInputState != CAIRO_DOCK_INPUT_ACTIVE)
		{
			cairo_dock_set_input_shape_active (pDock);
			pDock->iInputState = CAIRO_DOCK_INPUT_ACTIVE;
		}
	}
	
	//\__________________ hide the dock
	pDock->bAutoHide = FALSE;
	gtk_widget_hide (pDock->container.pWidget);
	
	///cairo_dock_update_dock_size (pDock);
}


void cairo_dock_insert_icon_in_dock_full (Icon *icon, CairoDock *pDock, gboolean bAnimated, gboolean bInsertSeparator, GCompareFunc pCompareFunc)
{
	//g_print ("%s (%s)\n", __func__, icon->cName);
	g_return_if_fail (icon != NULL);
	if (g_list_find (pDock->icons, icon) != NULL)  // elle est deja dans ce dock.
		return ;
	if (icon->pContainer != NULL)
	{
		cd_warning ("This icon (%s) is already inside a container !", icon->cName);
	}

	// maybe the icon was detached before and then cParentDockName is NULL
	if (icon->cParentDockName == NULL)
		icon->cParentDockName = g_strdup (cairo_dock_search_dock_name (pDock));

	//\______________ check if a separator is needed (ie, if the group of the new icon (not its order) is new).
	gboolean bSeparatorNeeded = FALSE;
	if (bInsertSeparator && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		Icon *pSameTypeIcon = cairo_dock_get_first_icon_of_group (pDock->icons, icon->iGroup);
		if (pSameTypeIcon == NULL && pDock->icons != NULL)
		{
			bSeparatorNeeded = TRUE;
			//g_print ("separateur necessaire\n");
		}
	}

	//\______________ insert the icon in the list.
	if (icon->fOrder == CAIRO_DOCK_LAST_ORDER)
	{
		Icon *pLastIcon = cairo_dock_get_last_icon_of_order (pDock->icons, icon->iGroup);
		if (pLastIcon != NULL)
			icon->fOrder = pLastIcon->fOrder + 1;
		else
			icon->fOrder = 1;
	}
	
	if (pCompareFunc == NULL)
		pCompareFunc = (GCompareFunc) cairo_dock_compare_icons_order;
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon,
		pCompareFunc);
	cairo_dock_set_icon_container (icon, pDock);
	
	//\______________ set the icon size, now that it's inside a container.
	int wi = icon->image.iWidth, hi = icon->image.iHeight;
	cairo_dock_set_icon_size_in_dock (pDock, icon);
	
	if (wi != cairo_dock_icon_get_allocated_width (icon) || hi != cairo_dock_icon_get_allocated_height (icon)  // if size has changed, reload the buffers
	|| (! icon->image.pSurface && ! icon->image.iTexture))  // might happen, for instance if the icon is a launcher pinned on a desktop and was detached before being loaded.
		cairo_dock_trigger_load_icon_buffers (icon);
	
	pDock->fFlatDockWidth += myIconsParam.iIconGap + icon->fWidth;
	if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);

	//\______________ On insere un separateur si necessaire.
	if (bSeparatorNeeded)
	{
		// insert a separator after if needed
		Icon *pNextIcon = cairo_dock_get_next_icon (pDock->icons, icon);
		if (pNextIcon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pNextIcon))
		{
			int iSeparatorGroup = cairo_dock_get_icon_order (icon) +
				(cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pNextIcon) ? 0 : 1);  // for separators, group = order.
			double fOrder = (cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pNextIcon) ? (icon->fOrder + pNextIcon->fOrder) / 2 : 0);
			cairo_dock_insert_automatic_separator_in_dock (iSeparatorGroup, fOrder, pNextIcon->cParentDockName, pDock);
		}
		
		// insert a separator before if needed
		Icon *pPrevIcon = cairo_dock_get_previous_icon (pDock->icons, icon);
		if (pPrevIcon != NULL && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pPrevIcon))
		{
			int iSeparatorGroup = cairo_dock_get_icon_order (icon) -
				(cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pPrevIcon) ? 0 : 1);  // for separators, group = order.
			double fOrder = (cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pPrevIcon) ? (icon->fOrder + pPrevIcon->fOrder) / 2 : 0);
			cairo_dock_insert_automatic_separator_in_dock (iSeparatorGroup, fOrder, pPrevIcon->cParentDockName, pDock);
		}
	}
	
	//\______________ On effectue les actions demandees.
	if (bAnimated)
	{
		if (cairo_dock_animation_will_be_visible (pDock))
			icon->fInsertRemoveFactor = - 0.95;
		else
			icon->fInsertRemoveFactor = - 0.05;
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
	else
		icon->fInsertRemoveFactor = 0.;
	
	cairo_dock_trigger_update_dock_size (pDock);
	
	if (pDock->iRefCount != 0 && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))  // on prevoit le redessin de l'icone pointant sur le sous-dock.
	{
		cairo_dock_trigger_redraw_subdock_content (pDock);
	}
	
	if (icon->pSubDock != NULL)
		cairo_dock_synchronize_one_sub_dock_orientation (icon->pSubDock, pDock, FALSE);
	
	//\______________ Notify everybody.
	cairo_dock_notify_on_object (pDock, NOTIFICATION_INSERT_ICON, icon, pDock);
}


static gboolean _destroy_empty_dock (CairoDock *pDock)
{
	const gchar *cDockName = cairo_dock_search_dock_name (pDock);  // safe meme si le dock n'existe plus.
	g_return_val_if_fail (cDockName != NULL, FALSE);  // si NULL, cela signifie que le dock n'existe plus, donc on n'y touche pas. Ce cas ne devrait jamais arriver, c'est de la pure parano.
	
	if (pDock->bIconIsFlyingAway)
		return TRUE;
	pDock->iSidDestroyEmptyDock = 0;
	if (pDock->icons == NULL && pDock->iRefCount == 0 && ! pDock->bIsMainDock)  // le dock est toujours a detruire.
	{
		cd_debug ("The dock '%s' is empty. No icon, no dock.", cDockName);
		cairo_dock_destroy_dock (pDock, cDockName);
	}
	return FALSE;
}
gboolean cairo_dock_detach_icon_from_dock_full (Icon *icon, CairoDock *pDock, gboolean bCheckUnusedSeparator)
{
	if (pDock == NULL)
		return FALSE;
	if (icon->pContainer == NULL)
	{
		cd_warning ("This icon (%s) is already not inside a container !", icon->cName);  // not a big deal, just print that for debug.
	}
	
	//\___________________ On trouve l'icone et ses 2 voisins.
	GList *prev_ic = NULL, *ic, *next_ic;
	Icon *pPrevIcon = NULL, *pNextIcon = NULL;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		if (ic -> data == icon)
		{
			prev_ic = ic->prev;
			next_ic = ic->next;
			if (prev_ic)
				pPrevIcon = prev_ic->data;
			if (next_ic)
				pNextIcon = next_ic->data;
			break;
		}
	}
	if (ic == NULL)  // elle est deja detachee.
		return FALSE;
	
	cd_message ("%s (%s)", __func__, icon->cName);
	
	//\___________________ On stoppe ses animations.
	cairo_dock_stop_icon_animation (icon);
	
	//\___________________ On desactive sa miniature.
	if (icon->Xid != 0)
	{
		//cd_debug ("on desactive la miniature de %s (Xid : %lx)", icon->cName, icon->Xid);
		cairo_dock_set_xicon_geometry (icon->Xid, 0, 0, 0, 0);
	}
	
	//\___________________ On l'enleve de la liste.
	pDock->icons = g_list_delete_link (pDock->icons, ic);
	ic = NULL;
	pDock->fFlatDockWidth -= icon->fWidth + myIconsParam.iIconGap;
	
	//\___________________ On enleve le separateur si c'est la derniere icone de son type.
	if (bCheckUnusedSeparator && ! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
	{
		if ((pPrevIcon == NULL || CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pPrevIcon)) && CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pNextIcon))
		{
			pDock->icons = g_list_delete_link (pDock->icons, next_ic);
			next_ic = NULL;
			pDock->fFlatDockWidth -= pNextIcon->fWidth + myIconsParam.iIconGap;
			cairo_dock_free_icon (pNextIcon);
			pNextIcon = NULL;
		}
		if ((pNextIcon == NULL || CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pNextIcon)) && CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pPrevIcon))
		{
			pDock->icons = g_list_delete_link (pDock->icons, prev_ic);
			prev_ic = NULL;
			pDock->fFlatDockWidth -= pPrevIcon->fWidth + myIconsParam.iIconGap;
			cairo_dock_free_icon (pPrevIcon);
			pPrevIcon = NULL;
		}
	}
	
	//\___________________ Cette icone realisait peut-etre le max des hauteurs, comme on l'enleve on recalcule ce max.
	Icon *pOtherIcon;
	if (icon->fHeight >= pDock->iMaxIconHeight)
	{
		pDock->iMaxIconHeight = 0;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			pOtherIcon = ic->data;
			if (! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pOtherIcon))
			{
				pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, pOtherIcon->fHeight);
				if (pOtherIcon->fHeight == icon->fHeight)  // on sait qu'on n'ira pas plus haut.
					break;
			}
		}
	}

	//\___________________ On la remet a la taille normale en vue d'une reinsertion quelque part.
	icon->fWidth /= pDock->container.fRatio;
	icon->fHeight /= pDock->container.fRatio;
	
	//\___________________ On prevoit le redessin de l'icone pointant sur le sous-dock.
	if (pDock->iRefCount != 0 && ! CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
	{
		cairo_dock_trigger_redraw_subdock_content (pDock);
	}
	
	//\___________________ On prevoit la destruction du dock si c'est un dock principal qui devient vide.
	if (pDock->iRefCount == 0 && pDock->icons == NULL && ! pDock->bIsMainDock)  // on supprime les docks principaux vides.
	{
		if (pDock->iSidDestroyEmptyDock == 0)
			pDock->iSidDestroyEmptyDock = g_idle_add ((GSourceFunc) _destroy_empty_dock, pDock);  // on ne passe pas le nom du dock en parametre, car le dock peut se faire renommer (improbable, mais bon).
	}
	else
	{
		cairo_dock_trigger_update_dock_size (pDock);
	}
	
	//\___________________ Notify everybody.
	icon->fInsertRemoveFactor = 0.;
	cairo_dock_notify_on_object (pDock, NOTIFICATION_REMOVE_ICON, icon, pDock);
	
	//\___________________ unset the container, now that it's completely detached from it.
	g_free (icon->cParentDockName);
	icon->cParentDockName = NULL;
	cairo_dock_set_icon_container (icon, NULL);
	
	return TRUE;
}
void cairo_dock_remove_icon_from_dock_full (CairoDock *pDock, Icon *icon, gboolean bCheckUnusedSeparator)
{
	g_return_if_fail (icon != NULL);
	
	//\___________________ On detache l'icone du dock.
	if (pDock != NULL)
		cairo_dock_detach_icon_from_dock_full (icon, pDock, bCheckUnusedSeparator);  // on le fait maintenant, pour que l'icone ait son type correct, et ne soit pas confondue avec un separateur
	
	//\___________________ On supprime l'icone du theme courant.
	cairo_dock_delete_icon_from_current_theme (icon);
}


void cairo_dock_remove_automatic_separators (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon;
	GList *ic = pDock->icons, *next_ic;
	while (ic != NULL)
	{
		icon = ic->data;
		next_ic = ic->next;  // si l'icone se fait enlever, on perdrait le fil.
		if (CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
		{
			cairo_dock_remove_one_icon_from_dock (pDock, icon);
			cairo_dock_free_icon (icon);
		}
		ic = next_ic;
	}
}

void cairo_dock_insert_automatic_separators_in_dock (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon, *pNextIcon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
		{
			if (ic->next != NULL)
			{
				pNextIcon = ic->next->data;
				if (! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (pNextIcon) && icon->iGroup != pNextIcon->iGroup)
				{
					int iSeparatorGroup = cairo_dock_get_icon_order (icon) +
						(cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pNextIcon) ? 0 : 1);  // for separators, group = order.
					double fOrder = (cairo_dock_get_icon_order (icon) == cairo_dock_get_icon_order (pNextIcon) ? (icon->fOrder + pNextIcon->fOrder) / 2 : 0);
					cairo_dock_insert_automatic_separator_in_dock (iSeparatorGroup, fOrder, icon->cParentDockName, pDock);
				}
			}
		}
	}
}


Icon *cairo_dock_add_new_launcher_by_uri_or_type (const gchar *cExternDesktopFileURI, CairoDockDesktopFileType iType, CairoDock *pReceivingDock, double fOrder)
{
	//\_________________ On ajoute un fichier desktop dans le repertoire des lanceurs du theme courant.
	GError *erreur = NULL;
	const gchar *cDockName = cairo_dock_search_dock_name (pReceivingDock);
	if (fOrder == CAIRO_DOCK_LAST_ORDER && pReceivingDock != NULL)
	{
		Icon *pLastIcon = cairo_dock_get_last_launcher (pReceivingDock->icons);
		if (pLastIcon != NULL)
			fOrder = pLastIcon->fOrder + 1;
		else
			fOrder = 1;
	}
	gchar *cNewDesktopFileName;
	if (cExternDesktopFileURI != NULL)
		cNewDesktopFileName = cairo_dock_add_desktop_file_from_uri (cExternDesktopFileURI, cDockName, fOrder, &erreur);
	else
		cNewDesktopFileName = cairo_dock_add_desktop_file_from_type (iType, cDockName, fOrder, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return NULL;
	}
	
	//\_________________ On verifie ici l'unicite du sous-dock.
	if (iType == CAIRO_DOCK_DESKTOP_FILE_FOR_CONTAINER && cExternDesktopFileURI == NULL)
	{
		
	}
	
	//\_________________ On charge ce nouveau lanceur.
	Icon *pNewIcon = NULL;
	if (cNewDesktopFileName != NULL)
	{
		pNewIcon = cairo_dock_create_icon_from_desktop_file (cNewDesktopFileName);
		g_free (cNewDesktopFileName);

		if (pNewIcon != NULL)
		{
			cairo_dock_insert_icon_in_dock (pNewIcon, pReceivingDock, CAIRO_DOCK_ANIMATE_ICON);
			
			if (pNewIcon->pSubDock != NULL)
				cairo_dock_trigger_redraw_subdock_content (pNewIcon->pSubDock);
		}
	}
	return pNewIcon;
}


void cairo_dock_remove_icons_from_dock (CairoDock *pDock, CairoDock *pReceivingDock, const gchar *cReceivingDockName)
{
	GList *pIconsList = pDock->icons;
	pDock->icons = NULL;
	Icon *icon;
	GList *ic;
	for (ic = pIconsList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		if (icon->pSubDock != NULL)
		{
			cairo_dock_remove_icons_from_dock (icon->pSubDock, pReceivingDock, cReceivingDockName);
		}

		if (pReceivingDock == NULL || cReceivingDockName == NULL)  // alors on les jete.
		{
			cairo_dock_delete_icon_from_current_theme (icon);
			
			if (CAIRO_DOCK_IS_APPLET (icon))
			{
				cairo_dock_update_icon_s_container_name (icon, CAIRO_DOCK_MAIN_DOCK_NAME);  // on decide de les remettre dans le dock principal la prochaine fois qu'ils seront actives.
			}
			cairo_dock_free_icon (icon);  // de-instancie l'applet.
		}
		else  // on les re-attribue au dock receveur.
		{
			cairo_dock_update_icon_s_container_name (icon, cReceivingDockName);
			
			icon->fWidth /= pDock->container.fRatio;  // optimization: no need to detach the icon, we just steal all of them.
			icon->fHeight /= pDock->container.fRatio;
			
			cd_debug (" on re-attribue %s au dock %s", icon->cName, icon->cParentDockName);
			cairo_dock_insert_icon_in_dock (icon, pReceivingDock, CAIRO_DOCK_ANIMATE_ICON);
			
			if (CAIRO_DOCK_IS_APPLET (icon))
			{
				icon->pModuleInstance->pContainer = CAIRO_CONTAINER (pReceivingDock);  // astuce pour ne pas avoir a recharger le fichier de conf ^_^
				icon->pModuleInstance->pDock = pReceivingDock;
				cairo_dock_reload_module_instance (icon->pModuleInstance, FALSE);
			}
			else if (cairo_dock_get_icon_data_renderer (icon) != NULL)
				cairo_dock_reload_data_renderer_on_icon (icon, CAIRO_CONTAINER (pReceivingDock));
		}
	}

	g_list_free (pIconsList);
}


void cairo_dock_reload_buffers_in_dock (CairoDock *pDock, gboolean bRecursive, gboolean bUpdateIconSize)
{
	//g_print ("************%s (%d, %d)\n", __func__, pDock->bIsMainDock, bRecursive);
	if (bUpdateIconSize && pDock->bGlobalIconSize)
		pDock->iIconSize = myIconsParam.iIconWidth;
	
	// for each icon, reload its buffer (size may change).
	Icon* icon;
	GList* ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		
		if (CAIRO_DOCK_IS_APPLET (icon))  // for an applet, we need to let the module know that the size or the theme has changed, so that it can reload its private buffers.
		{
			cairo_dock_reload_module_instance (icon->pModuleInstance, FALSE);
		}
		else
		{
			if (bUpdateIconSize)
			{
				cairo_dock_icon_set_requested_size (icon, 0, 0);
				cairo_dock_set_icon_size_in_dock (pDock, icon);
			}
			cairo_dock_trigger_load_icon_buffers (icon);
			
			if (bUpdateIconSize && cairo_dock_get_icon_data_renderer (icon) != NULL)
				cairo_dock_reload_data_renderer_on_icon (icon, CAIRO_CONTAINER (pDock));
		}
		
		if (bRecursive && icon->pSubDock != NULL)  // we handle the sub-dock for applets too, so that they don't need to care.
		{
			///cairo_dock_synchronize_one_sub_dock_orientation (icon->pSubDock, pDock, FALSE);  /// should probably not be here.
			if (bUpdateIconSize)
				icon->pSubDock->iIconSize = pDock->iIconSize;
			cairo_dock_reload_buffers_in_dock (icon->pSubDock, bRecursive, bUpdateIconSize);
		}
	}
	
	if (bUpdateIconSize)
	{
		cairo_dock_update_dock_size (pDock);
		cairo_dock_calculate_dock_icons (pDock);
		
		cairo_dock_move_resize_dock (pDock);
		if (pDock->iVisibility == CAIRO_DOCK_VISI_RESERVE)  // la position/taille a change, il faut refaire la reservation.
			cairo_dock_reserve_space_for_dock (pDock, TRUE);
		gtk_widget_queue_draw (pDock->container.pWidget);
	}
}


void cairo_dock_set_icon_size_in_dock (CairoDock *pDock, Icon *icon)
{
	if (pDock->pRenderer && pDock->pRenderer->set_icon_size)  // the view wants to decide the icons size.
	{
		pDock->pRenderer->set_icon_size (icon, pDock);
	}
	else  // generic method: icon extent = base size * max zoom
	{
		int wi, hi;  // icon size (icon size displayed at rest, as defined in the config)
		int wa, ha;  // allocated size (surface/texture).
		gboolean bIsHorizontal = (pDock->container.bIsHorizontal || (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) && myIconsParam.bRevolveSeparator));
		
		// get the displayed icon size as defined in the config
		if (! pDock->bGlobalIconSize && pDock->iIconSize != 0)
		{
			wi = hi = pDock->iIconSize;
		}
		else  // same size as main dock.
		{
			wi = myIconsParam.iIconWidth;
			hi = myIconsParam.iIconHeight;
		}
		
		if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))  // separators have their own size.
		{
			wi = myIconsParam.iSeparatorWidth;
			hi = MIN (myIconsParam.iSeparatorHeight, hi);
		}
		
		// take into account the requested displayed size if any
		int wir = cairo_dock_icon_get_requested_display_width (icon);
		if (wir != 0)
			wi = wir;
		int hir = cairo_dock_icon_get_requested_display_height (icon);
		if (hir != 0)
			hi = MIN (hir, hi);  // limit the icon height to the default height.
		
		// get the requested size if any
		wa = cairo_dock_icon_get_requested_width (icon);
		ha = cairo_dock_icon_get_requested_height (icon);
		
		// compute the missing size (allocated or displayed).
		double fMaxScale = 1 + myIconsParam.fAmplitude;
		if (wa == 0)
		{
			wa = (bIsHorizontal ? wi : hi) * fMaxScale;
		}
		else
		{
			if (bIsHorizontal)
				wi = wa / fMaxScale;
			else
				hi = wa / fMaxScale;
		}
		if (ha == 0)
		{
			ha = (bIsHorizontal ? hi : wi) * fMaxScale;
		}
		else
		{
			if (bIsHorizontal)
				hi = ha / fMaxScale;
			else
				wi = ha / fMaxScale;
		}
		
		// set both allocated and displayed size 
		cairo_dock_icon_set_allocated_size (icon, wa, ha);
		icon->fWidth = wi;
		icon->fHeight = hi;
	}
	// take into account the current ratio
	icon->fWidth *= pDock->container.fRatio;
	icon->fHeight *= pDock->container.fRatio;
}


void cairo_dock_create_redirect_texture_for_dock (CairoDock *pDock)
{
	if (! g_openglConfig.bFboAvailable)
		return ;
	if (pDock->iRedirectedTexture == 0)
	{
		pDock->iRedirectedTexture = cairo_dock_create_texture_from_raw_data (NULL,
			(pDock->container.bIsHorizontal ? pDock->container.iWidth : pDock->container.iHeight),
			(pDock->container.bIsHorizontal ? pDock->container.iHeight : pDock->container.iWidth));
	}
	if (pDock->iFboId == 0)
		glGenFramebuffersEXT(1, &pDock->iFboId);
}
