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
#include <stdio.h>
#include <stdlib.h>
#define __USE_POSIX
#include <signal.h>

#include <cairo.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "gldi-config.h"
#ifdef HAVE_XEXTEND
#include <X11/extensions/Xcomposite.h>
//#include <X11/extensions/Xdamage.h>
#endif

#include "cairo-dock-icon-manager.h"
#include "cairo-dock-indicator-manager.h"  // myIndicatorsParam.bDrawIndicatorOnAppli
#include "cairo-dock-icon-facility.h"  // cairo_dock_update_icon_s_container_name
#include "cairo-dock-animations.h"  // cairo_dock_trigger_icon_removal_from_dock
#include "cairo-dock-application-factory.h"  // cairo_dock_new_appli_icon
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-container.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-draw-opengl.h"  // cairo_dock_create_texture_from_surface
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_open_key_file
#include "cairo-dock-application-facility.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-overlay.h"
#define _MANAGER_DEF_
#include "cairo-dock-applications-manager.h"

#define CAIRO_DOCK_TASKBAR_CHECK_INTERVAL 200
#define CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME "default-icon-appli.svg"

// public (manager, config, data)
CairoTaskbarParam myTaskbarParam;
CairoTaskbarManager myTaskbarMgr;

// dependancies
extern CairoDock *g_pMainDock;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern gboolean g_bUseOpenGL;
//extern int g_iDamageEvent;

// private
static Display *s_XDisplay = NULL;
static GHashTable *s_hXWindowTable = NULL;  // table des fenetres X affichees dans le dock.
static int s_bAppliManagerIsRunning = FALSE;
static int s_iTime = 1;  // on peut aller jusqu'a 2^31, soit 17 ans a 4Hz.
static int s_iNumWindow = 1;  // used to order appli icons by age (=creation date).
static Window s_iCurrentActiveWindow = 0;
static Atom s_aNetWmState;
static Atom s_aNetWmDesktop;
static Atom s_aNetWmName;
static Atom s_aUtf8String;
static Atom s_aWmName;
static Atom s_aString;
static Atom s_aNetWmIcon;
static Atom s_aWmHints;

static void cairo_dock_blacklist_appli (Window Xid);
static Icon * cairo_dock_create_icon_from_xwindow (Window Xid, CairoDock *pDock);


  //////////////////////////
 // Appli manager : core //
//////////////////////////

static void _cairo_dock_hide_show_windows_on_other_desktops (Window *Xid, Icon *icon, CairoDock *pMainDock)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && (! myTaskbarParam.bHideVisibleApplis || icon->bIsHidden))
	{
		cd_debug ("%s (%d)", __func__, *Xid);
		CairoDock *pParentDock = NULL;
		if (cairo_dock_appli_is_on_current_desktop (icon))
		{
			cd_debug (" => est sur le bureau actuel.");
			if (icon->cParentDockName == NULL)
			{
				pParentDock = cairo_dock_insert_appli_in_dock (icon, pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
			}
		}
		else
		{
			cd_debug (" => n'est pas sur le bureau actuel.");
			if (icon->cParentDockName != NULL)  // if in a dock, detach it
				pParentDock = cairo_dock_detach_appli (icon);
			else  // else if inhibited, detach from the inhibitor
				cairo_dock_detach_Xid_from_inhibitors (icon->Xid, icon->cClass);
		}
		if (pParentDock != NULL)
			gtk_widget_queue_draw (pParentDock->container.pWidget);
	}
}

static void _show_if_no_overlapping_window (CairoDock *pDock, gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (cairo_dock_is_temporary_hidden (pDock))
	{
		if (cairo_dock_search_window_overlapping_dock (pDock) == NULL)
		{
			cairo_dock_deactivate_temporary_auto_hide (pDock);
		}
	}
}

static void _hide_if_any_overlap (CairoDock *pDock, gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (!cairo_dock_is_temporary_hidden (pDock))
	{
		if (cairo_dock_search_window_overlapping_dock (pDock) != NULL)
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
}

static void _hide_if_any_overlap_or_show (CairoDock *pDock, gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (cairo_dock_is_temporary_hidden (pDock))
	{
		if (cairo_dock_search_window_overlapping_dock (pDock) == NULL)
		{
			cairo_dock_deactivate_temporary_auto_hide (pDock);
		}
	}
	else
	{
		if (cairo_dock_search_window_overlapping_dock (pDock) != NULL)
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
}

static void _hide_if_overlap (CairoDock *pDock, Icon *icon)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (! cairo_dock_is_temporary_hidden (pDock))
	{
		if (cairo_dock_appli_is_on_current_desktop (icon) && cairo_dock_appli_overlaps_dock (icon, pDock))
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
}

static void _hide_if_overlap_or_show_if_no_overlapping_window (CairoDock *pDock, Icon *icon)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
		return ;
	if (cairo_dock_appli_overlaps_dock (icon, pDock))  // cette fenetre peut provoquer l'auto-hide.
	{
		if (! cairo_dock_is_temporary_hidden (pDock))
		{
			cairo_dock_activate_temporary_auto_hide (pDock);
		}
	}
	else  // ne gene pas/plus.
	{
		if (cairo_dock_is_temporary_hidden (pDock))
		{
			if (cairo_dock_search_window_overlapping_dock (pDock) == NULL)
			{
				cairo_dock_deactivate_temporary_auto_hide (pDock);
			}
		}
	}
}

static void _unhide_all_docks (CairoDock *pDock, Icon *icon)
{
	if (cairo_dock_is_temporary_hidden (pDock))
		cairo_dock_deactivate_temporary_auto_hide (pDock);
}

static void _hide_show_if_on_our_way (CairoDock *pDock, Icon *icon)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP && ! myDocksParam.bAutoHideOnFullScreen)
		return ;
	if (_cairo_dock_appli_is_on_our_way (icon, pDock))  // la nouvelle fenetre active nous gene.
	{
		if (!cairo_dock_is_temporary_hidden (pDock))
			cairo_dock_activate_temporary_auto_hide (pDock);
	}
	else if (pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY)
	{
		if (cairo_dock_is_temporary_hidden (pDock))
			cairo_dock_deactivate_temporary_auto_hide (pDock);
	}
}

void cairo_dock_hide_show_if_current_window_is_on_our_way (CairoDock *pDock)
{
	Icon *pActiveAppli = cairo_dock_get_current_active_icon ();
	_hide_show_if_on_our_way (pDock, pActiveAppli);
}

void cairo_dock_hide_if_any_window_overlap_or_show (CairoDock *pDock)
{
	_hide_if_any_overlap_or_show (pDock, NULL);
}


static gboolean _cairo_dock_remove_old_applis (Window *Xid, Icon *icon, gpointer iTimePtr)
{
	if (icon == NULL)
		return FALSE;
	gint iTime = GPOINTER_TO_INT (iTimePtr);
	
	//g_print ("%s (%s(%ld) %d / %d)\n", __func__, icon->cName, icon->Xid, icon->iLastCheckTime, iTime);
	if (icon->iLastCheckTime >= 0 && icon->iLastCheckTime < iTime && ! cairo_dock_icon_is_being_removed (icon))
	{
		cd_message ("cette fenetre (%ld(%ld), %s) est trop vieille (%d / %d, %s)", *Xid, icon->Xid, icon->cName, icon->iLastCheckTime, iTime, icon->cParentDockName);
		if (CAIRO_DOCK_IS_APPLI (icon))
		{
			// notify everybody
			cairo_dock_notify_on_object (&myTaskbarMgr, NOTIFICATION_APPLI_DESTROYED, icon);
			
			CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			if (pParentDock != NULL)
			{
				cd_message ("  va etre supprimee");
				cairo_dock_trigger_icon_removal_from_dock (icon);
				
				icon->iLastCheckTime = -1;  // inutile de chercher a la desenregistrer par la suite, puisque ce sera fait ici. Cela sert aussi a bien supprimer l'icone en fin d'animation.
				///cairo_dock_unregister_appli (icon);
				cairo_dock_remove_appli_from_class (icon);  // elle reste une icone d'appli, et de la meme classe, mais devient invisible aux autres icones de sa classe. Inutile de tester les inhibiteurs, puisqu'elle est dans un dock.
			}
			else  // n'etait pas dans un dock, on la detruit donc immediatement.
			{
				cd_message ("  pas dans un container, on la detruit donc immediatement");
				cairo_dock_update_name_on_inhibitors (icon->cClass, *Xid, NULL);
				icon->iLastCheckTime = -1;  // pour ne pas la desenregistrer de la HashTable lors du 'free' puisqu'on est en train de parcourir la table.
				cairo_dock_free_icon (icon);
				/// redessiner les inhibiteurs ?...
			}
			
			cairo_dock_foreach_root_docks ((GFunc)_show_if_no_overlapping_window, NULL);
		}
		else
		{
			g_free (icon);
		}
		return TRUE;
	}
	return FALSE;
}
static void _on_update_applis_list (CairoDock *pDock)
{
	if (pDock == NULL)
		return;
	s_iTime ++;
	gulong i, iNbWindows = 0;
	Window *pXWindowsList = cairo_dock_get_windows_list (&iNbWindows, TRUE);  // TRUE => ordered by z-stack.
	
	Window Xid;
	Icon *icon;
	int iStackOrder = 0;
	gpointer pOriginalXid;
	gboolean bAppliAlreadyRegistered;
	
	for (i = 0; i < iNbWindows; i ++)
	{
		Xid = pXWindowsList[i];
		
		bAppliAlreadyRegistered = g_hash_table_lookup_extended (s_hXWindowTable, &Xid, &pOriginalXid, (gpointer *) &icon);
		if (! bAppliAlreadyRegistered)
		{
			cd_message (" cette fenetre (%ld) de la pile n'est pas dans la liste", Xid);
			icon = cairo_dock_create_icon_from_xwindow (Xid, pDock);
			if (icon != NULL)
			{
				icon->iLastCheckTime = s_iTime;
				icon->iStackOrder = iStackOrder ++;
				if (myTaskbarParam.bShowAppli)
				{
					cd_message (" insertion de %s ... (%d)", icon->cName, icon->iLastCheckTime);
					cairo_dock_insert_appli_in_dock (icon, pDock, CAIRO_DOCK_ANIMATE_ICON);
				}
				
				// visibilite
				cairo_dock_foreach_root_docks ((GFunc)_hide_if_overlap, icon);
				
				// notify everybody
				cairo_dock_notify_on_object (&myTaskbarMgr, NOTIFICATION_APPLI_CREATED, icon);
			}
			else
				cairo_dock_blacklist_appli (Xid);
		}
		else if (icon != NULL)
		{
			icon->iLastCheckTime = s_iTime;
			if (CAIRO_DOCK_IS_APPLI (icon))
				icon->iStackOrder = iStackOrder ++;
		}
	}
	
	g_hash_table_foreach_remove (s_hXWindowTable, (GHRFunc) _cairo_dock_remove_old_applis, GINT_TO_POINTER (s_iTime));

	XFree (pXWindowsList);
}

static gboolean _on_change_active_window_notification (gpointer data, Window *Xid)
{
	Window XActiveWindow = *Xid;
	if (s_iCurrentActiveWindow != XActiveWindow)  // la fenetre courante a change.
	{
		// on gere son animation et son indicateur.
		Icon *icon = g_hash_table_lookup (s_hXWindowTable, &XActiveWindow);
		CairoDock *pParentDock = NULL;
		if (CAIRO_DOCK_IS_APPLI (icon))
		{
			///if (icon->bIsDemandingAttention)  // on force ici, car il semble qu'on ne recoive pas toujours le retour a la normale.
			///	cairo_dock_appli_stops_demanding_attention (icon);
			
			pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			if (pParentDock == NULL)  // elle est soit inhibee, soit pas dans un dock.
			{
				cairo_dock_update_activity_on_inhibitors (icon->cClass, icon->Xid);
			}
			else
			{
				cairo_dock_animate_icon_on_active (icon, pParentDock);
			}
		}
		
		// on enleve l'indicateur sur la precedente appli active.
		gboolean bForceKbdStateRefresh = FALSE;
		Icon *pLastActiveIcon = g_hash_table_lookup (s_hXWindowTable, &s_iCurrentActiveWindow);
		if (CAIRO_DOCK_IS_APPLI (pLastActiveIcon))
		{
			CairoDock *pLastActiveParentDock = cairo_dock_search_dock_from_name (pLastActiveIcon->cParentDockName);
			if (pLastActiveParentDock == NULL)  // elle est soit inhibee, soit pas dans un dock.
			{
				cairo_dock_update_inactivity_on_inhibitors (pLastActiveIcon->cClass, pLastActiveIcon->Xid);
			}
			else
			{
				cairo_dock_redraw_icon (pLastActiveIcon, CAIRO_CONTAINER (pLastActiveParentDock));
				if (pLastActiveParentDock->iRefCount != 0)  // l'icone est dans un sous-dock, comme l'indicateur est aussi dessine sur l'icone pointant sur ce sous-dock, il faut la redessiner sans l'indicateur.
				{
					CairoDock *pMainDock = NULL;
					Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pLastActiveParentDock, &pMainDock);
					if (pPointingIcon && pMainDock)
					{
						cairo_dock_redraw_icon (pPointingIcon, CAIRO_CONTAINER (pMainDock));
					}
				}
			}
		}
		else  // il n'y avait pas de fenetre active avant.
			bForceKbdStateRefresh = TRUE;
		s_iCurrentActiveWindow = XActiveWindow;
		
		// on gere le masquage du dock.
		if (! CAIRO_DOCK_IS_APPLI (icon))
		{
			Window iPropWindow;
			XGetTransientForHint (s_XDisplay, XActiveWindow, &iPropWindow);
			icon = g_hash_table_lookup (s_hXWindowTable, &iPropWindow);
			//g_print ("*** la fenetre parente est : %s\n", icon?icon->cName:"aucune");
		}
		cairo_dock_foreach_root_docks ((GFunc)_hide_show_if_on_our_way, icon);
		
		// notification xklavier.
		if (bForceKbdStateRefresh)  // si on active une fenetre n'ayant pas de focus clavier, on n'aura pas d'evenement kbd_changed, pourtant en interne le clavier changera. du coup si apres on revient sur une fenetre qui a un focus clavier, il risque de ne pas y avoir de changement de clavier, et donc encore une fois pas d'evenement ! pour palier a ce, on considere que les fenetres avec focus clavier sont celles presentes en barre des taches. On decide de generer un evenement lorsqu'on revient sur une fenetre avec focus, a partir d'une fenetre sans focus (mettre a jour le clavier pour une fenetre sans focus n'a pas grand interet, autant le laisser inchange).
			cairo_dock_notify_on_object (&myDesktopMgr, NOTIFICATION_KBD_STATE_CHANGED, &XActiveWindow);
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static gboolean _on_change_current_desktop_viewport_notification (gpointer data)
{
	CairoDock *pDock = g_pMainDock;
	
	cd_debug ("*** applis du bureau seulement...");
	// applis du bureau courant seulement.
	if (myTaskbarParam.bAppliOnCurrentDesktopOnly && myTaskbarParam.bShowAppli)
	{
		g_hash_table_foreach (s_hXWindowTable, (GHFunc) _cairo_dock_hide_show_windows_on_other_desktops, pDock);
	}
	
	// visibilite
	Icon *pActiveAppli = cairo_dock_get_current_active_icon ();
	cairo_dock_foreach_root_docks ((GFunc)_hide_show_if_on_our_way, pActiveAppli);
	
	cairo_dock_foreach_root_docks ((GFunc)_hide_if_any_overlap_or_show, NULL);
	
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static void _on_change_window_state (Icon *icon)
{
	Window Xid = icon->Xid;
	gboolean bIsFullScreen, bIsHidden, bIsMaximized, bDemandsAttention, bPrevHidden = icon->bIsHidden;
	if (! cairo_dock_xwindow_is_fullscreen_or_hidden_or_maximized (Xid, &bIsFullScreen, &bIsHidden, &bIsMaximized, &bDemandsAttention))
	{
		//g_print ("Special case : this appli (%s, %ld) should be ignored from now !\n", icon->cName, Xid);
		CairoDock *pParentDock = cairo_dock_detach_appli (icon);
		if (pParentDock != NULL)
			gtk_widget_queue_draw (pParentDock->container.pWidget);
		cairo_dock_free_icon (icon);
		cairo_dock_blacklist_appli (Xid);
		return ;
	}
	
	// demande d'attention.
	// seems like this demand should be ignored, or we'll get tons of demands under Kwin. Let's just watch the UrgencyHint in the WMHints.
	/**if (bDemandsAttention && (myTaskbarParam.bDemandsAttentionWithDialog || myTaskbarParam.cAnimationOnDemandsAttention))  // elle peut demander l'attention plusieurs fois de suite.
	{
		cd_debug ("%s demande votre attention %s !", icon->cName, icon->bIsDemandingAttention?"encore une fois":"");
		cairo_dock_appli_demands_attention (icon);
	}
	else if (! bDemandsAttention)
	{
		if (icon->bIsDemandingAttention)
		{
			cd_debug ("%s se tait", icon->cName);
			cairo_dock_appli_stops_demanding_attention (icon);
		}
	}*/
	
	gboolean bHiddenChanged 	= (bIsHidden != icon->bIsHidden);
	gboolean bMaximizedChanged 	= (bIsMaximized != icon->bIsMaximized);
	gboolean bFullScreenChanged 	= (bIsFullScreen != icon->bIsFullScreen);
	
	icon->bIsMaximized = bIsMaximized;
	icon->bIsFullScreen = bIsFullScreen;
	icon->bIsHidden = bIsHidden;
	
	// masquage du dock.
	if (Xid == s_iCurrentActiveWindow)  // c'est la fenetre courante qui a change d'etat.
	{
		if (bHiddenChanged || bFullScreenChanged)  // si c'est l'etat maximise qui a change, on le verra au changement de dimensions.
		{
			cairo_dock_foreach_root_docks ((GFunc)_hide_show_if_on_our_way, icon);
		}
	}
	
	// on gere le cachage/apparition de l'icone (transparence ou miniature, applis minimisees seulement).
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (bHiddenChanged)
	{
		cd_message ("  changement de visibilite -> %d", bIsHidden);
		// visibilite
		if (!icon->bIsHidden)  // la fenetre reapparait.
			cairo_dock_foreach_root_docks ((GFunc)_hide_if_overlap, icon);
		else  // la fenetre se cache.
			cairo_dock_foreach_root_docks ((GFunc)_show_if_no_overlapping_window, NULL);
		
		// affichage des applis minimisees.
		if (g_bUseOpenGL && myTaskbarParam.iMinimizedWindowRenderType == 2)
		{
			if (pParentDock != NULL)
			{
				cairo_dock_draw_hidden_appli_icon (icon, CAIRO_CONTAINER (pParentDock), TRUE);
			}
		}
		else if (myTaskbarParam.iMinimizedWindowRenderType == 0)
		{
			// transparence sur les inhibiteurs.
			cairo_dock_update_visibility_on_inhibitors (icon->cClass, icon->Xid, icon->bIsHidden);
		}
		
		// applis minimisees seulement
		if (myTaskbarParam.bHideVisibleApplis && myTaskbarParam.bShowAppli)  // on insere/detache l'icone selon la visibilite de la fenetre, avec une animation.
		{
			if (bIsHidden)  // se cache => on insere son icone.
			{
				cd_message (" => se cache");
				///if (! myTaskbarParam.bAppliOnCurrentDesktopOnly || cairo_dock_appli_is_on_current_desktop (icon))
				///{
					pParentDock = cairo_dock_insert_appli_in_dock (icon, g_pMainDock, CAIRO_DOCK_ANIMATE_ICON);
					if (pParentDock != NULL)
					{
						if (g_bUseOpenGL && myTaskbarParam.iMinimizedWindowRenderType == 2)  // quand on est passe dans ce cas tout a l'heure l'icone n'etait pas encore dans son dock.
							cairo_dock_draw_hidden_appli_icon (icon, CAIRO_CONTAINER (pParentDock), TRUE);
						gtk_widget_queue_draw (pParentDock->container.pWidget);
					}
				///}
			}
			else  // se montre => on detache l'icone.
			{
				cd_message (" => re-apparait");
				cairo_dock_trigger_icon_removal_from_dock (icon);
			}
		}
		else if (myTaskbarParam.fVisibleAppliAlpha != 0)  // transparence
		{
			icon->fAlpha = 1;  // on triche un peu.
			if (pParentDock != NULL)
				cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pParentDock));
		}
		
		// miniature (on le fait apres l'avoir inseree/detachee, car comme ca dans le cas ou on l'enleve du dock apres l'avoir deminimisee, l'icone est marquee comme en cours de suppression, et donc on ne recharge pas son icone. Sinon l'image change pendant la transition, ce qui est pas top. Comme ca ne change pas la taille de l'icone dans le dock, on peut faire ca apres l'avoir inseree.
		#ifdef HAVE_XEXTEND
		if (myTaskbarParam.iMinimizedWindowRenderType == 1 && (pParentDock != NULL || myTaskbarParam.bHideVisibleApplis))  // on recupere la miniature ou au contraire on remet l'icone.
		{
			if (! icon->bIsHidden)  // fenetre mappee => BackingPixmap disponible.
			{
				if (icon->iBackingPixmap != 0)
					XFreePixmap (s_XDisplay, icon->iBackingPixmap);
				icon->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, Xid);
				cd_message ("new backing pixmap (bis) : %d", icon->iBackingPixmap);
			}
			// on redessine avec ou sans la miniature, suivant le nouvel etat.
			cairo_dock_reload_icon_image (icon, CAIRO_CONTAINER (pParentDock));
			if (pParentDock)
			{
				cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pParentDock));
				if (pParentDock->iRefCount != 0)  // on prevoit le redessin de l'icone pointant sur le sous-dock.
				{
					cairo_dock_trigger_redraw_subdock_content (pParentDock);
				}
			}
		}
		#endif
	}
	// notify everybody
	cairo_dock_notify_on_object (&myTaskbarMgr, NOTIFICATION_APPLI_STATE_CHANGED, icon, bHiddenChanged, bMaximizedChanged, bFullScreenChanged);
}

static void _on_change_window_desktop (Icon *icon)
{
	Window Xid = icon->Xid;
	icon->iNumDesktop = cairo_dock_get_xwindow_desktop (Xid);
	
	// applis du bureau courant seulement.
	if (myTaskbarParam.bAppliOnCurrentDesktopOnly && myTaskbarParam.bShowAppli)
	{
		_cairo_dock_hide_show_windows_on_other_desktops (&Xid, icon, g_pMainDock);  // si elle vient sur notre bureau, elle n'est pas forcement sur le meme viewport, donc il faut le verifier.
	}
	
	// visibilite
	if (Xid == s_iCurrentActiveWindow)  // c'est la fenetre courante qui a change de bureau.
	{
		cairo_dock_foreach_root_docks ((GFunc)_hide_show_if_on_our_way, icon);
	}
	
	if ((icon->iNumDesktop == -1 || icon->iNumDesktop == g_desktopGeometry.iCurrentDesktop) && icon->iViewPortX == g_desktopGeometry.iCurrentViewportX && icon->iViewPortY == g_desktopGeometry.iCurrentViewportY)  // petite optimisation : si l'appli arrive sur le bureau courant, on peut se contenter de ne verifier qu'elle.
	{
		cairo_dock_foreach_root_docks ((GFunc)_hide_if_overlap, icon);
	}
	else  // la fenetre n'est plus sur le bureau courant.
	{
		cairo_dock_foreach_root_docks ((GFunc)_show_if_no_overlapping_window, NULL);
	}
	// notify everybody
	cairo_dock_notify_on_object (&myTaskbarMgr, NOTIFICATION_APPLI_SIZE_POSITION_CHANGED, icon);
}

static void _on_change_window_size_position (Icon *icon, XConfigureEvent *e)
{
	Window Xid = icon->Xid;
	int x = e->x, y = e->y;
	int w = e->width, h = e->height;
	
	#ifdef HAVE_XEXTEND
	if (w != icon->windowGeometry.width || h != icon->windowGeometry.height)
	{
		if (icon->iBackingPixmap != 0)
			XFreePixmap (s_XDisplay, icon->iBackingPixmap);
		if (myTaskbarParam.iMinimizedWindowRenderType == 1)
		{
			icon->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, Xid);
			cd_message ("new backing pixmap : %d", icon->iBackingPixmap);
		}
		else
			icon->iBackingPixmap = 0;
	}
	#endif
	
	// get the correct coordinates (same reason as in cairo_dock_get_xwindow_geometry(), this is a workaround to a bug in X; we didn't need that before Ubuntu 11.10).
	// actually, the XConfigureEvent has more precise coordinates during a move (updated more often than the ones from XTranslateCoordinates), but it has nil coordinates during a resize (and we have no way to distinguish this case).
	Window child, root = DefaultRootWindow (s_XDisplay);
	XTranslateCoordinates (s_XDisplay, Xid, root, 0, 0, &x, &y, &child);
	//g_print (" :: %s ((%d;%d) <> (%d;%d), %dx%d)\n", __func__, x, y, e->x, e->y, w, h);
	
	// take into account the window borders (Note: we don't monitor the _NET_FRAME_EXTENTS property, so let's retrieve theme now; maybe we should monitor them...)
	int left=0, right=0, top=0, bottom=0;
	gulong iLeftBytes, iBufferNbElements = 0;
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	gulong *pBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, XInternAtom (s_XDisplay, "_NET_FRAME_EXTENTS", False), 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pBuffer);
	if (iBufferNbElements > 3)
	{
		left=pBuffer[0], right=pBuffer[1], top=pBuffer[2], bottom=pBuffer[3];
	}
	if (pBuffer)
		XFree (pBuffer);
	
	icon->windowGeometry.width = w + left + right;
	icon->windowGeometry.height = h + top + bottom;
	icon->windowGeometry.x = x - left;
	icon->windowGeometry.y = y - top;
	icon->iViewPortX = x / g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] + g_desktopGeometry.iCurrentViewportX;
	icon->iViewPortY = y / g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] + g_desktopGeometry.iCurrentViewportY;
	//g_print (" :: (%d;%d) %dx%d)\n", icon->windowGeometry.x, icon->windowGeometry.y, icon->windowGeometry.width, icon->windowGeometry.height);
	
	// on regarde si l'appli est sur le viewport courant.
	if (x + w <= 0 || x >= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] || y + h <= 0 || y >= g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL])  // not on this desktop (actually, it may be only a little bit on this desktop ... maybe we should use a % of the width, or a margin ...)
	{
		// applis du bureau courant seulement.
		if (myTaskbarParam.bAppliOnCurrentDesktopOnly)
		{
			if (icon->cParentDockName != NULL)
			{
				CairoDock *pParentDock = cairo_dock_detach_appli (icon);
				if (pParentDock)
					gtk_widget_queue_draw (pParentDock->container.pWidget);
			}
			else
				cairo_dock_detach_Xid_from_inhibitors (icon->Xid, icon->cClass);
		}
		
		// visibilite
		cairo_dock_foreach_root_docks ((GFunc)_show_if_no_overlapping_window, icon);
	}
	else  // elle est sur le viewport courant.
	{
		// applis du bureau courant seulement.
		if (myTaskbarParam.bAppliOnCurrentDesktopOnly && icon->cParentDockName == NULL && myTaskbarParam.bShowAppli)
		{
			//cd_message ("cette fenetre est sur le bureau courant (%d;%d)", x, y);
			cairo_dock_insert_appli_in_dock (icon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);  // the icon might be on this desktop and yet not in a dock (inhibited), in which case this function does nothing.
		}
		
		// visibilite
		cairo_dock_foreach_root_docks ((GFunc)_hide_if_overlap_or_show_if_no_overlapping_window, icon);
	}
	
	// masquage du dock.
	if (Xid == s_iCurrentActiveWindow)  // c'est la fenetre courante qui a change de bureau.
	{
		cairo_dock_foreach_root_docks ((GFunc)_hide_show_if_on_our_way, icon);
	}
	
	// notify everybody
	cairo_dock_notify_on_object (&myTaskbarMgr, NOTIFICATION_APPLI_SIZE_POSITION_CHANGED, icon);
}

static gboolean _on_window_configured_notification (gpointer data, Window Xid, XConfigureEvent *e)
{
	if (e != NULL)  // a window
	{
		Icon *icon = g_hash_table_lookup (s_hXWindowTable, &Xid);
		if (! CAIRO_DOCK_IS_APPLI (icon) || cairo_dock_icon_is_being_removed (icon))
			return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
		_on_change_window_size_position (icon, e);
	}
	else  // root window
		_on_update_applis_list (g_pMainDock);
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static void _on_change_window_name (Icon *icon, CairoDock *pDock, gboolean bSearchWmName)
{
	gchar *cName = cairo_dock_get_xwindow_name (icon->Xid, bSearchWmName);
	if (cName != NULL)
	{
		if (icon->cName == NULL || strcmp (icon->cName, cName) != 0)
		{
			cairo_dock_set_icon_name (cName, icon, NULL);
			
			cairo_dock_update_name_on_inhibitors (icon->cClass, icon->Xid, cName);
			
			// notify everybody
			cairo_dock_notify_on_object (&myTaskbarMgr, NOTIFICATION_APPLI_NAME_CHANGED, icon);
		}
		g_free (cName);
	}
}

static void _on_change_window_icon (Icon *icon, CairoDock *pDock)
{
	if (cairo_dock_class_is_using_xicon (icon->cClass) || ! myTaskbarParam.bOverWriteXIcons)
	{
		cairo_dock_reload_icon_image (icon, CAIRO_CONTAINER (pDock));
		if (pDock->iRefCount != 0)
			cairo_dock_trigger_redraw_subdock_content (pDock);
		cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pDock));
		// notify everybody
		cairo_dock_notify_on_object (&myTaskbarMgr, NOTIFICATION_APPLI_ICON_CHANGED, icon);
	}
}

static void _on_change_window_hints (Icon *icon, CairoDock *pDock, int iState)
{
	XWMHints *pWMHints = XGetWMHints (s_XDisplay, icon->Xid);
	if (pWMHints != NULL)
	{
		if (pWMHints->flags & XUrgencyHint)
		{
			if (myTaskbarParam.bDemandsAttentionWithDialog || myTaskbarParam.cAnimationOnDemandsAttention)
				cairo_dock_appli_demands_attention (icon);
		}
		else if (icon->bIsDemandingAttention)
		{
			cairo_dock_appli_stops_demanding_attention (icon);
		}
		
		if (iState == PropertyNewValue && (pWMHints->flags & (IconPixmapHint | IconMaskHint | IconWindowHint)))
		{
			//g_print ("%s change son icone\n", icon->cName);
			if (cairo_dock_class_is_using_xicon (icon->cClass) || ! myTaskbarParam.bOverWriteXIcons)
			{
				cairo_dock_reload_icon_image (icon, CAIRO_CONTAINER (pDock));
				if (pDock->iRefCount != 0)
					cairo_dock_trigger_redraw_subdock_content (pDock);
				cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pDock));
				// notify everybody
				cairo_dock_notify_on_object (&myTaskbarMgr, NOTIFICATION_APPLI_ICON_CHANGED, icon);
			}
		}
		XFree (pWMHints);  // "When finished with the data, free the space used for it by calling XFree()."
	}
	else  // no hints set on this window.
	{
		if (icon->bIsDemandingAttention)
			cairo_dock_appli_stops_demanding_attention (icon);
	}
}

static void _on_change_window_class (Icon *icon, CairoDock *pDock)
{
	//g_print ("WM_CLASS changed (%p, %s)!\n", icon->pMimeTypes, icon->cCommand);
	// retrieve the new class
	gchar *cClass = NULL, *cWmClass = NULL;
	cClass = cairo_dock_get_xwindow_class (icon->Xid, &cWmClass);
	if (! cairo_dock_strings_differ (cClass, icon->cClass) || ! cClass)  // cClass can be NULL (libreoffice when closing a window).
	{
		//g_print ("fausse alerte\n");
		g_free (cClass);
		g_free (cWmClass);
		return;
	}
	//g_print (" %s -> %s\n", icon->cClass, cClass);
	
	// remove the icon from the dock, and then from its class
	CairoDock *pParentDock = NULL;
	if (icon->cParentDockName != NULL)  // if in a dock, detach it
		pParentDock = cairo_dock_detach_appli (icon);
	else  // else if inhibited, detach from the inhibitor
		cairo_dock_detach_Xid_from_inhibitors (icon->Xid, icon->cClass);
	cairo_dock_remove_appli_from_class (icon);
	
	// set the new class
	g_free (icon->cClass);
	icon->cClass = cClass;
	g_free (icon->cWmClass);
	icon->cWmClass = cWmClass;
	cairo_dock_add_appli_to_class (icon);
	
	// re-insert the icon
	pParentDock = cairo_dock_insert_appli_in_dock (icon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
	if (pParentDock != NULL)
		gtk_widget_queue_draw (pParentDock->container.pWidget);
	
	// reload the icon
	g_strfreev (icon->pMimeTypes);
	icon->pMimeTypes = g_strdupv ((gchar**)cairo_dock_get_class_mimetypes (icon->cClass));
	g_free (icon->cCommand);
	icon->cCommand = g_strdup (cairo_dock_get_class_command (icon->cClass));
	cairo_dock_reload_icon_image (icon, CAIRO_CONTAINER (pParentDock));
	// notify everybody
	cairo_dock_notify_on_object (&myTaskbarMgr, NOTIFICATION_APPLI_ICON_CHANGED, icon);
}

static gboolean _on_property_changed_notification (gpointer data, Window Xid, Atom aProperty, int iState)
{
	Icon *icon = g_hash_table_lookup (s_hXWindowTable, &Xid);
	if (! CAIRO_DOCK_IS_APPLI (icon))  // appli blacklistee
	{
		if (! cairo_dock_xwindow_skip_taskbar (Xid))
		{
			//g_print ("Special case : this appli (%ld) should not be ignored any more!\n", Xid);
			g_hash_table_remove (s_hXWindowTable, &Xid);  // will be detected by the XEvent loop.
			g_free (icon);
		}
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION;
	}
	
	if (aProperty == s_aNetWmState)  // changement d'etat (hidden, maximized, fullscreen, demands attention)
	{
		_on_change_window_state (icon);
	}
	else if (aProperty == s_aNetWmDesktop)  // cela ne gere pas les changements de viewports, qui eux se font en changeant les coordonnees. Il faut donc recueillir les ConfigureNotify, qui donnent les redimensionnements et les deplacements.
	{
		_on_change_window_desktop (icon);
	}
	else
	{
		CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
		if (pParentDock == NULL)
			pParentDock = g_pMainDock;  /// bof ...
		if (iState == PropertyNewValue && (aProperty == s_aNetWmName || aProperty == s_aWmName))
		{
			_on_change_window_name (icon, pParentDock, aProperty == s_aWmName);
		}
		else if (iState == PropertyNewValue && aProperty == s_aNetWmIcon)
		{
			_on_change_window_icon (icon, pParentDock);
		}
		else if (aProperty == s_aWmHints)
		{
			_on_change_window_hints (icon, pParentDock, iState);
		}
		else if (aProperty == XInternAtom (s_XDisplay, "WM_CLASS", False))
		{
			_on_change_window_class (icon, pParentDock);
		}
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

  ///////////////////////////
 // Applis manager : core //
///////////////////////////

static void cairo_dock_register_appli (Icon *icon)
{
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cd_debug ("%s (%ld ; %s)", __func__, icon->Xid, icon->cName);
		// add to table
		Window *pXid = g_new (Window, 1);
		*pXid = icon->Xid;
		g_hash_table_insert (s_hXWindowTable, pXid, icon);
		
		// add to class
		cairo_dock_add_appli_to_class (icon);
		
		// start watching events
		cairo_dock_set_xwindow_mask (icon->Xid, PropertyChangeMask | StructureNotifyMask);
	}
}

static void cairo_dock_blacklist_appli (Window Xid)
{
	if (Xid > 0)
	{
		cd_debug ("%s (%ld)", __func__, Xid);
		// add a dummy icon to the table
		Icon *pNullIcon = cairo_dock_new_icon ();
		pNullIcon->iLastCheckTime = s_iTime;
		Window *pXid = g_new (Window, 1);
		*pXid = Xid;
		g_hash_table_insert (s_hXWindowTable, pXid, pNullIcon);
		
		// start watching events
		cairo_dock_set_xwindow_mask (Xid, PropertyChangeMask | StructureNotifyMask);  // on veut pouvoir etre notifie de ses changements d'etat (si "skip taskbar" disparait, elle reviendra dans la barre des taches).
	}
}

void cairo_dock_unregister_appli (Icon *icon)
{
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cd_debug ("%s (%ld ; %s)", __func__, icon->Xid, icon->cName);
		// remove from table
		if (icon->iLastCheckTime != -1)
			g_hash_table_remove (s_hXWindowTable, &icon->Xid);
		
		// remove from class
		cairo_dock_remove_appli_from_class (icon);  // n'efface pas sa classe (on peut en avoir besoin encore).
		cairo_dock_detach_Xid_from_inhibitors (icon->Xid, icon->cClass);
		
		// stop watching events
		cairo_dock_set_xwindow_mask (icon->Xid, None);
		
		if (icon->iBackingPixmap != 0)
		{
			XFreePixmap (s_XDisplay, icon->iBackingPixmap);
			icon->iBackingPixmap = 0;
		}
		
		icon->Xid = 0;  // hop, elle n'est plus une appli.
	}
}


void cairo_dock_start_applications_manager (CairoDock *pDock)
{
	g_return_if_fail (!s_bAppliManagerIsRunning);
	
	cairo_dock_set_overwrite_exceptions (myTaskbarParam.cOverwriteException);
	cairo_dock_set_group_exceptions (myTaskbarParam.cGroupException);
	
	//\__________________ On recupere l'ensemble des fenetres presentes.
	gulong i, iNbWindows = 0;
	Window *pXWindowsList = cairo_dock_get_windows_list (&iNbWindows, FALSE);  // ordered by creation date; this allows us to set the correct age to the icon, which is constant. On the next updates, the z-order (which is dynamic) will be set.
	
	if (s_iCurrentActiveWindow == 0)
		s_iCurrentActiveWindow = cairo_dock_get_active_xwindow ();
	
	//\__________________ On cree les icones de toutes ces applis.
	Window Xid;
	Icon *pIcon;
	for (i = 0; i < iNbWindows; i ++)
	{
		Xid = pXWindowsList[i];
		pIcon = cairo_dock_create_icon_from_xwindow (Xid, pDock);
		
		if (pIcon != NULL)  // authorised window.
		{
			pIcon->iLastCheckTime = s_iTime;
			if (myTaskbarParam.bShowAppli && pDock)
			{
				cairo_dock_insert_appli_in_dock (pIcon, pDock, ! CAIRO_DOCK_ANIMATE_ICON);
			}
		}
		else
			cairo_dock_blacklist_appli (Xid);
	}
	if (pXWindowsList != NULL)
		XFree (pXWindowsList);
	
	// masquage du dock, une fois que sa taille est correcte.
	Icon *pActiveAppli = cairo_dock_get_current_active_icon ();
	cairo_dock_foreach_root_docks ((GFunc)_hide_show_if_on_our_way, pActiveAppli);
	
	cairo_dock_foreach_root_docks ((GFunc)_hide_if_any_overlap, NULL);
	
	s_bAppliManagerIsRunning = TRUE;
}

static gboolean _cairo_dock_remove_one_appli (Window *pXid, Icon *pIcon, gpointer data)
{
	if (pIcon == NULL)
		return TRUE;
	if (pIcon->Xid == 0)
	{
		g_free (pIcon);
		return TRUE;
	}
	
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pDock != NULL)
	{
		gchar *cParentDockName = pIcon->cParentDockName;
		pIcon->cParentDockName = NULL;  // astuce.
		cairo_dock_detach_icon_from_dock (pIcon, pDock);
		if (! pDock->bIsMainDock)  // la taille du main dock est mis a jour 1 fois a la fin.
		{
			if (pDock->icons == NULL)  // le dock degage, le fake aussi.
			{
				CairoDock *pFakeClassParentDock = NULL;
				Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeClassParentDock);
				if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pFakeClassIcon)) // fake launcher
				{
					cd_debug ("on degage le fake qui pointe sur %s", cParentDockName);
					cairo_dock_detach_icon_from_dock (pFakeClassIcon, pFakeClassParentDock);
					cairo_dock_free_icon (pFakeClassIcon);
					if (! pFakeClassParentDock->bIsMainDock)
						cairo_dock_update_dock_size (pFakeClassParentDock);
				}
				
				cairo_dock_destroy_dock (pDock, cParentDockName);
			}
			///else
			///	cairo_dock_update_dock_size (pDock);
		}
		g_free (cParentDockName);
	}
	
	pIcon->Xid = 0;  // on ne veut pas passer dans le 'unregister'
	g_free (pIcon->cClass);  // ni la gestion de la classe.
	pIcon->cClass = NULL;
	if (pIcon->iBackingPixmap != 0)
	{
		XFreePixmap (s_XDisplay, pIcon->iBackingPixmap);
		pIcon->iBackingPixmap = 0;
	}
	cairo_dock_free_icon (pIcon);
	return TRUE;
}
static void _cairo_dock_stop_application_manager (void)
{
	s_bAppliManagerIsRunning = FALSE;
	
	cairo_dock_remove_all_applis_from_class_table ();  // enleve aussi les indicateurs.
	
	g_hash_table_foreach_remove (s_hXWindowTable, (GHRFunc) _cairo_dock_remove_one_appli, NULL);  // libere toutes les icones d'appli.
	///cairo_dock_update_dock_size (g_pMainDock);
	
	cairo_dock_foreach_root_docks ((GFunc)_unhide_all_docks, NULL);
}


/**static gboolean _cairo_dock_reset_appli_table_iter (Window *pXid, Icon *pIcon, gpointer data)
{
	// if not a valid appli, just free it.
	if (pIcon == NULL)
		return TRUE;
	if (pIcon->Xid == 0)
	{
		g_free (pIcon);
		return TRUE;
	}
	
	// if inside a dock, detach it first.
	CairoDock *pDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pDock != NULL)
	{
		cairo_dock_detach_icon_from_dock_full (pIcon, pDock, FALSE);
	}
	
	// make it an invalid appli, and free it.
	pIcon->Xid = 0;  // on ne veut pas passer dans le 'unregister'
	g_free (pIcon->cClass);  // ni la gestion de la classe.
	pIcon->cClass = NULL;
	cairo_dock_free_icon (pIcon);
	return TRUE;
}
void cairo_dock_reset_applications_manager (void)
{
	// reset class table
	cairo_dock_reset_class_table ();
	
	// reset appli table
	g_hash_table_foreach_remove (s_hXWindowTable, (GHRFunc) _cairo_dock_reset_appli_table_iter, NULL);  // libere toutes les icones d'appli.
}*/


  /////////////////////////////
 // Applis manager : access //
/////////////////////////////

static gboolean _cairo_dock_window_is_covering_dock (Window *Xid, Icon *icon, gpointer *data)
{
	gboolean bMaximizedWindow = GPOINTER_TO_INT (data[0]);
	gboolean bFullScreenWindow = GPOINTER_TO_INT (data[1]);
	CairoDock *pDock = data[2];
	if (CAIRO_DOCK_IS_APPLI (icon) && cairo_dock_appli_is_on_current_desktop (icon) && ! icon->bIsHidden && ! cairo_dock_icon_is_being_removed (icon) && icon->iLastCheckTime != -1)
	{
		if ((data[0] && icon->bIsMaximized) || (data[1] && icon->bIsFullScreen) || (!data[0] && ! data[1]))
		{
			cd_debug ("%s est genante (%d, %d) (%d;%d %dx%d)", icon->cName, icon->bIsMaximized, icon->bIsFullScreen, icon->windowGeometry.x, icon->windowGeometry.y, icon->windowGeometry.width, icon->windowGeometry.height);
			if (cairo_dock_appli_covers_dock (icon, pDock))
			{
				cd_debug (" et en plus elle empiete sur notre dock");
				return TRUE;
			}
		}
	}
	return FALSE;
}
Icon *cairo_dock_search_window_covering_dock (CairoDock *pDock, gboolean bMaximizedWindow, gboolean bFullScreenWindow)
{
	//cd_debug ("%s (%d, %d)", __func__, bMaximizedWindow, bFullScreenWindow);
	gpointer data[3] = {GINT_TO_POINTER (bMaximizedWindow), GINT_TO_POINTER (bFullScreenWindow), pDock};
	return g_hash_table_find (s_hXWindowTable, (GHRFunc) _cairo_dock_window_is_covering_dock, data);
}

static gboolean _cairo_dock_window_is_overlapping_dock (Window *Xid, Icon *icon, CairoDock *pDock)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && cairo_dock_appli_is_on_current_desktop (icon) && ! icon->bIsHidden && ! cairo_dock_icon_is_being_removed (icon) && icon->iLastCheckTime != -1)
	{
		if (cairo_dock_appli_overlaps_dock (icon, pDock))
		{
			return TRUE;
		}
	}
	return FALSE;
}
Icon *cairo_dock_search_window_overlapping_dock (CairoDock *pDock)
{
	return g_hash_table_find (s_hXWindowTable, (GHRFunc) _cairo_dock_window_is_overlapping_dock, pDock);
}



GList *cairo_dock_get_current_applis_list (void)
{
	return g_hash_table_get_values (s_hXWindowTable);
}

Window cairo_dock_get_current_active_window (void)
{
	return s_iCurrentActiveWindow;
}

Icon *cairo_dock_get_current_active_icon (void)
{
	Icon *pIcon = g_hash_table_lookup (s_hXWindowTable, &s_iCurrentActiveWindow);
	if (CAIRO_DOCK_IS_APPLI (pIcon))
		return pIcon;
	else
		return NULL;
}

Icon *cairo_dock_get_icon_with_Xid (Window Xid)
{
	Icon *pIcon = g_hash_table_lookup (s_hXWindowTable, &Xid);
	if (CAIRO_DOCK_IS_APPLI (pIcon))
		return pIcon;
	else
		return NULL;
}


static void _cairo_dock_for_one_appli (Window *Xid, Icon *icon, gpointer *data)
{
	if (! CAIRO_DOCK_IS_APPLI (icon) || cairo_dock_icon_is_being_removed (icon))
		return ;
	CairoDockForeachIconFunc pFunction = data[0];
	gpointer pUserData = data[1];
	gboolean bOutsideDockOnly =  GPOINTER_TO_INT (data[2]);
	
	if ((bOutsideDockOnly && icon->cParentDockName == NULL) || ! bOutsideDockOnly)
	{
		CairoDock *pParentDock = NULL;
		if (icon->cParentDockName != NULL)
			pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
		else
			pParentDock = g_pMainDock;
		pFunction (icon, CAIRO_CONTAINER (pParentDock), pUserData);
	}
}
void cairo_dock_foreach_applis (CairoDockForeachIconFunc pFunction, gboolean bOutsideDockOnly, gpointer pUserData)
{
	gpointer data[3] = {pFunction, pUserData, GINT_TO_POINTER (bOutsideDockOnly)};
	g_hash_table_foreach (s_hXWindowTable, (GHFunc) _cairo_dock_for_one_appli, data);
}


static void _cairo_dock_for_one_appli_on_viewport (Icon *pIcon, CairoContainer *pContainer, gpointer *data)
{
	int iNumDesktop = GPOINTER_TO_INT (data[0]);
	int iNumViewportX = GPOINTER_TO_INT (data[1]);
	int iNumViewportY = GPOINTER_TO_INT (data[2]);
	CairoDockForeachIconFunc pFunction = data[3];
	gpointer pUserData = data[4];
	
	if (! cairo_dock_appli_is_on_desktop (pIcon, iNumDesktop, iNumViewportX, iNumViewportY))
		return ;
	
	pFunction (pIcon, pContainer, pUserData);
}
void cairo_dock_foreach_applis_on_viewport (CairoDockForeachIconFunc pFunction, int iNumDesktop, int iNumViewportX, int iNumViewportY, gpointer pUserData)
{
	gpointer data[5] = {GINT_TO_POINTER (iNumDesktop), GINT_TO_POINTER (iNumViewportX), GINT_TO_POINTER (iNumViewportY), pFunction, pUserData};
	cairo_dock_foreach_applis ((CairoDockForeachIconFunc) _cairo_dock_for_one_appli_on_viewport, FALSE, data);
}


void cairo_dock_set_icons_geometry_for_window_manager (CairoDock *pDock)
{
	if (! s_bAppliManagerIsRunning)
		return ;
	//g_print ("%s (main:%d, ref:%d)\n", __func__, pDock->bIsMainDock, pDock->iRefCount);
	
	/*long *data = g_new0 (long, 1+6*g_list_length (pDock->icons));
	int i = 0;*/
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (icon))
		{
			cairo_dock_set_one_icon_geometry_for_window_manager (icon, pDock);
			/*data[1+6*i+0] = 5;
			data[1+6*i+1] = icon->Xid;
			data[1+6*i+2] = pDock->container.iWindowPositionX + icon->fXAtRest;
			data[1+6*i+3] = 0;
			data[1+6*i+4] = icon->fWidth;
			data[1+6*i+5] = icon->fHeight;
			i ++;*/
		}
	}
	
	/*data[0] = i;
	Atom atom = XInternAtom (cairo_dock_get_Xdisplay(), "_KDE_WINDOW_PREVIEW", False);
	Window Xid = GDK_WINDOW_XID (pDock->container.pWidget->window);
	XChangeProperty(cairo_dock_get_Xdisplay(), Xid, atom, atom, 32, PropModeReplace, data, 1+6*i);*/
	
	if (pDock->bIsMainDock && myTaskbarParam.bHideVisibleApplis)  // on complete avec les applis pas dans le dock, pour que l'effet de minimisation pointe (a peu pres) au bon endroit quand on la minimisera.
	{
		g_hash_table_foreach (s_hXWindowTable, (GHFunc) cairo_dock_reserve_one_icon_geometry_for_window_manager, pDock);
	}
}


  /////////////////////////////
 // Applis manager : icones //
/////////////////////////////

static cairo_surface_t *cairo_dock_create_surface_from_xpixmap (Pixmap Xid, int iWidth, int iHeight)
{
	g_return_val_if_fail (Xid > 0, NULL);
	GdkPixbuf *pPixbuf = cairo_dock_get_pixbuf_from_pixmap (Xid, TRUE);
	if (pPixbuf == NULL)
	{
		cd_warning ("No thumbnail available.\nEither the WM doesn't support this functionnality, or the window was minimized when the dock has been launched.");
		return NULL;
	}
	
	cd_debug ("window pixmap : %dx%d", gdk_pixbuf_get_width (pPixbuf), gdk_pixbuf_get_height (pPixbuf));
	double fWidth, fHeight;
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_pixbuf (pPixbuf,
		1.,
		iWidth, iHeight,
		CAIRO_DOCK_KEEP_RATIO | CAIRO_DOCK_FILL_SPACE,  // on conserve le ratio de la fenetre, tout en gardant la taille habituelle des icones d'appli.
		&fWidth, &fHeight,
		NULL, NULL);
	g_object_unref (pPixbuf);
	return pSurface;
}

static cairo_surface_t *cairo_dock_create_surface_from_xwindow (Window Xid, int iWidth, int iHeight)
{
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXIconBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmIcon, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXIconBuffer);

	if (iBufferNbElements > 2)
	{
		cairo_surface_t *pNewSurface = cairo_dock_create_surface_from_xicon_buffer (pXIconBuffer,
			iBufferNbElements,
			iWidth,
			iHeight);
		XFree (pXIconBuffer);
		return pNewSurface;
	}
	else  // sinon on tente avec l'icone eventuellement presente dans les WMHints.
	{
		XWMHints *pWMHints = XGetWMHints (s_XDisplay, Xid);
		if (pWMHints == NULL)
		{
			cd_debug ("  aucun WMHints");
			return NULL;
		}
		//\__________________ On recupere les donnees dans un  pixbuf.
		GdkPixbuf *pIconPixbuf = NULL;
		if (pWMHints->flags & IconWindowHint)
		{
			Window XIconID = pWMHints->icon_window;
			cd_debug ("  pas de _NET_WM_ICON, mais une fenetre (ID:%d)", XIconID);
			Pixmap iPixmap = cairo_dock_get_window_background_pixmap (XIconID);
			pIconPixbuf = cairo_dock_get_pixbuf_from_pixmap (iPixmap, TRUE);  /// A valider ...
		}
		else if (pWMHints->flags & IconPixmapHint)
		{
			cd_debug ("  pas de _NET_WM_ICON, mais un pixmap");
			Pixmap XPixmapID = pWMHints->icon_pixmap;
			pIconPixbuf = cairo_dock_get_pixbuf_from_pixmap (XPixmapID, TRUE);

			//\____________________ On lui applique le masque de transparence s'il existe.
			if (pWMHints->flags & IconMaskHint)
			{
				Pixmap XPixmapMaskID = pWMHints->icon_mask;
				GdkPixbuf *pMaskPixbuf = cairo_dock_get_pixbuf_from_pixmap (XPixmapMaskID, FALSE);

				int iNbChannels = gdk_pixbuf_get_n_channels (pIconPixbuf);
				int iRowstride = gdk_pixbuf_get_rowstride (pIconPixbuf);
				guchar *p, *pixels = gdk_pixbuf_get_pixels (pIconPixbuf);

				int iNbChannelsMask = gdk_pixbuf_get_n_channels (pMaskPixbuf);
				int iRowstrideMask = gdk_pixbuf_get_rowstride (pMaskPixbuf);
				guchar *q, *pixelsMask = gdk_pixbuf_get_pixels (pMaskPixbuf);

				int w = MIN (gdk_pixbuf_get_width (pIconPixbuf), gdk_pixbuf_get_width (pMaskPixbuf));
				int h = MIN (gdk_pixbuf_get_height (pIconPixbuf), gdk_pixbuf_get_height (pMaskPixbuf));
				int x, y;
				for (y = 0; y < h; y ++)
				{
					for (x = 0; x < w; x ++)
					{
						p = pixels + y * iRowstride + x * iNbChannels;
						q = pixelsMask + y * iRowstrideMask + x * iNbChannelsMask;
						if (q[0] == 0)
							p[3] = 0;
						else
							p[3] = 255;
					}
				}

				g_object_unref (pMaskPixbuf);
			}
		}
		XFree (pWMHints);

		//\____________________ On cree la surface.
		if (pIconPixbuf != NULL)
		{
			double fWidth, fHeight;
			cairo_surface_t *pNewSurface = cairo_dock_create_surface_from_pixbuf (pIconPixbuf,
				1.,
				iWidth,
				iHeight,
				CAIRO_DOCK_KEEP_RATIO | CAIRO_DOCK_FILL_SPACE,
				&fWidth,
				&fHeight,
				NULL, NULL);

			g_object_unref (pIconPixbuf);
			return pNewSurface;
		}
		return NULL;
	}
}

static void _load_appli (Icon *icon)
{
	if (cairo_dock_icon_is_being_removed (icon))
		return ;
	
	//\__________________ register the class to get its attributes, if it was not done yet.
	if (icon->cClass && !icon->pMimeTypes && !icon->cCommand)
	{
		gchar *cClass = cairo_dock_register_class_full (NULL, icon->cClass, icon->cWmClass);
		if (cClass != NULL)
		{
			g_free (cClass);
			icon->cCommand = g_strdup (cairo_dock_get_class_command (icon->cClass));
			icon->pMimeTypes = g_strdupv ((gchar**)cairo_dock_get_class_mimetypes (icon->cClass));
		}
	}
	
	//\__________________ then draw the icon
	int iWidth = icon->iImageWidth;
	int iHeight = icon->iImageHeight;
	cairo_surface_t *pPrevSurface = icon->pIconBuffer;
	GLuint iPrevTexture = icon->iIconTexture;
	icon->pIconBuffer = NULL;
	icon->iIconTexture = 0;
	
	// use the thumbnail in the case of a minimized window.
	if (myTaskbarParam.iMinimizedWindowRenderType == 1 && icon->bIsHidden && icon->iBackingPixmap != 0)
	{
		// create the thumbnail (window preview).
		if (g_bUseOpenGL)
		{
			///icon->iIconTexture = cairo_dock_texture_from_pixmap (icon->Xid, icon->iBackingPixmap);  // doesn't work any more since Compiz 0.9 :-(
		}
		if (icon->iIconTexture == 0)
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_xpixmap (icon->iBackingPixmap,
				iWidth,
				iHeight);
			if (g_bUseOpenGL)
				icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
		}
		// draw the previous image as an emblem.
		if (icon->iIconTexture != 0 && iPrevTexture != 0)
		{
			CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			cairo_dock_print_overlay_on_icon_from_texture (icon, CAIRO_CONTAINER (pParentDock), iPrevTexture, CAIRO_OVERLAY_LOWER_LEFT);
			/**CairoEmblem *e = cairo_dock_make_emblem_from_texture (iPrevTexture,icon);
			cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
			cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
			g_free (e);  // on n'utilise pas cairo_dock_free_emblem pour ne pas detruire la texture avec.*/
		}
		else if (icon->pIconBuffer != NULL && pPrevSurface != NULL)
		{
			CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			cairo_dock_print_overlay_on_icon_from_surface (icon, CAIRO_CONTAINER (pParentDock), pPrevSurface, 0, 0, CAIRO_OVERLAY_LOWER_LEFT);
			/**CairoEmblem *e = cairo_dock_make_emblem_from_surface (pPrevSurface, 0, 0, icon);
			cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
			cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
			g_free (e);  // meme remarque.*/
		}
	}
	// or use the class icon
	if (icon->iIconTexture == 0 && icon->pIconBuffer == NULL && myTaskbarParam.bOverWriteXIcons && ! cairo_dock_class_is_using_xicon (icon->cClass))
		icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass, iWidth, iHeight);
	// or use the X icon
	if (icon->iIconTexture == 0 && icon->pIconBuffer == NULL)
		icon->pIconBuffer = cairo_dock_create_surface_from_xwindow (icon->Xid, iWidth, iHeight);
	// or use a default image
	if (icon->iIconTexture == 0 && icon->pIconBuffer == NULL)  // certaines applis comme xterm ne definissent pas d'icone, on en met une par defaut.
	{
		cd_debug ("%s (%ld) doesn't define any icon, we set the default one.", icon->cName, icon->Xid);
		gchar *cIconPath = cairo_dock_search_image_s_path (CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
		if (cIconPath == NULL)  // image non trouvee.
		{
			cIconPath = g_strdup (GLDI_SHARE_DATA_DIR"/icons/"CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
		}
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			iWidth,
			iHeight);
		g_free (cIconPath);
	}
	// bent the icon in the case of a minimized window and if defined in the config.
	if (icon->bIsHidden && myTaskbarParam.iMinimizedWindowRenderType == 2)
	{
		CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
		if (pParentDock)
			cairo_dock_draw_hidden_appli_icon (icon, CAIRO_CONTAINER (pParentDock), FALSE);
	}
}

static void _show_appli_for_drop (Icon *pIcon)
{
	cairo_dock_show_xwindow (pIcon->Xid);
}

static gboolean _delete_appli (Icon *pIcon)
{
	cairo_dock_unregister_appli (pIcon);
	return FALSE;
}

static Icon * cairo_dock_create_icon_from_xwindow (Window Xid, CairoDock *pDock)
{
	//\__________________ On cree l'icone.
	Window XParentWindow = 0;
	Icon *icon = cairo_dock_new_appli_icon (Xid, &XParentWindow);
	
	if (XParentWindow != 0 && (myTaskbarParam.bDemandsAttentionWithDialog || myTaskbarParam.cAnimationOnDemandsAttention))
	{
		Icon *pParentIcon = cairo_dock_get_icon_with_Xid (XParentWindow);
		if (pParentIcon != NULL)
		{
			cd_debug ("%s requiert votre attention indirectement !", pParentIcon->cName);
			cairo_dock_appli_demands_attention (pParentIcon);
		}
		else
			cd_debug ("ce dialogue est bien bruyant ! (%d)", XParentWindow);
	}
	
	if (icon == NULL)
		return NULL;
	icon->iface.load_image = _load_appli;
	icon->iface.action_on_drag_hover = _show_appli_for_drop;
	icon->iface.on_delete = _delete_appli;
	icon->bHasIndicator = myIndicatorsParam.bDrawIndicatorOnAppli;
	if (myTaskbarParam.bSeparateApplis)
		icon->iGroup = CAIRO_DOCK_APPLI;
	else
		icon->iGroup = CAIRO_DOCK_LAUNCHER;
	
	//\____________ On remplit ses buffers.
	if (myTaskbarParam.bShowAppli)
	{
		#ifdef HAVE_XEXTEND
		if (myTaskbarParam.iMinimizedWindowRenderType == 1 && ! icon->bIsHidden)
		{
			//Display *display = gdk_x11_get_default_xdisplay ();
			icon->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, Xid);
			/*icon->iDamageHandle = XDamageCreate (s_XDisplay, Xid, XDamageReportNonEmpty);  // XDamageReportRawRectangles
			cd_debug ("backing pixmap : %d ; iDamageHandle : %d\n", icon->iBackingPixmap, icon->iDamageHandle);*/
		}
		#endif
		
		/**if (pDock)
		{
			cairo_dock_trigger_load_icon_buffers (icon);
		}*/
	}
	
	//\____________ On enregistre l'appli et on commence a la surveiller.
	icon->iAge = s_iNumWindow ++;
	cairo_dock_register_appli (icon);
	
	return icon;
}


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoTaskbarParam *pTaskBar)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	// comportement
	pTaskBar->bShowAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "show applications", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	
	if (pTaskBar->bShowAppli)
	{
		pTaskBar->bAppliOnCurrentDesktopOnly = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "current desktop only", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
		
		pTaskBar->bMixLauncherAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "mix launcher appli", &bFlushConfFileNeeded, TRUE, NULL, NULL);
		
		pTaskBar->bGroupAppliByClass = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "group by class", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
		pTaskBar->cGroupException = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "group exception", &bFlushConfFileNeeded, "pidgin;xchat", NULL, NULL);
		if (pTaskBar->cGroupException)
		{
			int i;
			for (i = 0; pTaskBar->cGroupException[i] != '\0'; i ++)  // on passe tout en minuscule.
				pTaskBar->cGroupException[i] = g_ascii_tolower (pTaskBar->cGroupException[i]);
		}
		
		pTaskBar->bHideVisibleApplis = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "hide visible", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
		
		pTaskBar->iIconPlacement = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "place icons", &bFlushConfFileNeeded, CAIRO_APPLI_AFTER_LAST_LAUNCHER, NULL, NULL);  // after the last launcher by default.
		pTaskBar->cRelativeIconName = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "relative icon", &bFlushConfFileNeeded, NULL, NULL, NULL);
		pTaskBar->bSeparateApplis = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "separate applis", &bFlushConfFileNeeded, TRUE, NULL, NULL);
		
		// representation
		pTaskBar->bOverWriteXIcons = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "overwrite xicon", &bFlushConfFileNeeded, TRUE, NULL, NULL);
		pTaskBar->cOverwriteException = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "overwrite exception", &bFlushConfFileNeeded, "pidgin;xchat", NULL, NULL);
		if (pTaskBar->cOverwriteException)
		{
			int i;
			for (i = 0; pTaskBar->cOverwriteException[i] != '\0'; i ++)
				pTaskBar->cOverwriteException[i] = g_ascii_tolower (pTaskBar->cOverwriteException[i]);
		}
		
		pTaskBar->iMinimizedWindowRenderType = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "minimized", &bFlushConfFileNeeded, -1, NULL, NULL);
		if (pTaskBar->iMinimizedWindowRenderType == -1)  // anciens parametres.
		{
			gboolean bShowThumbnail = g_key_file_get_boolean (pKeyFile, "TaskBar", "window thumbnail", NULL);
			if (bShowThumbnail)
				pTaskBar->iMinimizedWindowRenderType = 1;
			else
				pTaskBar->iMinimizedWindowRenderType = 0;
			g_key_file_set_integer (pKeyFile, "TaskBar", "minimized", pTaskBar->iMinimizedWindowRenderType);
		}
		
		if (pTaskBar->iMinimizedWindowRenderType == 1 && ! cairo_dock_xcomposite_is_available ())
		{
			cd_warning ("Sorry but either your X server does not have the XComposite extension, or your version of Cairo-Dock was not built with the support of XComposite.\n You can't have window thumbnails in the dock");
			pTaskBar->iMinimizedWindowRenderType = 0;
		}
		if (pTaskBar->iMinimizedWindowRenderType == 0)
			pTaskBar->fVisibleAppliAlpha = MIN (.6, cairo_dock_get_double_key_value (pKeyFile, "TaskBar", "visibility alpha", &bFlushConfFileNeeded, .35, "Applications", NULL));
		
		pTaskBar->iAppliMaxNameLength = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "max name length", &bFlushConfFileNeeded, 25, "Applications", NULL);
		
		
		// interaction
		pTaskBar->iActionOnMiddleClick = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "action on middle click", &bFlushConfFileNeeded, 1, NULL, NULL);
		pTaskBar->bMinimizeOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "minimize on click", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
		
		pTaskBar->bPresentClassOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "present class on click", &bFlushConfFileNeeded, TRUE, NULL, NULL);
		
		pTaskBar->bDemandsAttentionWithDialog = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "demands attention with dialog", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
		pTaskBar->iDialogDuration = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "duration", &bFlushConfFileNeeded, 2, NULL, NULL);
		pTaskBar->cAnimationOnDemandsAttention = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "animation on demands attention", &bFlushConfFileNeeded, "fire", NULL, NULL);
		gchar *cForceDemandsAttention = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "force demands attention", &bFlushConfFileNeeded, "pidgin;xchat", NULL, NULL);
		if (cForceDemandsAttention != NULL)
		{
			pTaskBar->cForceDemandsAttention = g_ascii_strdown (cForceDemandsAttention, -1);
			g_free (cForceDemandsAttention);
		}
		
		pTaskBar->cAnimationOnActiveWindow = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "animation on active window", &bFlushConfFileNeeded, "wobbly", NULL, NULL);
	}
	return bFlushConfFileNeeded;
}


  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (CairoTaskbarParam *pTaskBar)
{
	g_free (pTaskBar->cAnimationOnActiveWindow);
	g_free (pTaskBar->cAnimationOnDemandsAttention);
	g_free (pTaskBar->cOverwriteException);
	g_free (pTaskBar->cGroupException);
	g_free (pTaskBar->cForceDemandsAttention);
	g_free (pTaskBar->cRelativeIconName);
}

  ////////////
 /// LOAD ///
////////////

static void load (void)
{
	
}


  //////////////
 /// RELOAD ///
//////////////

static void reload (CairoTaskbarParam *pPrevTaskBar, CairoTaskbarParam *pTaskBar)
{
	CairoDock *pDock = g_pMainDock;
	
	gboolean bUpdateSize = FALSE;
	if (pPrevTaskBar->bGroupAppliByClass != pTaskBar->bGroupAppliByClass
		|| pPrevTaskBar->bHideVisibleApplis != pTaskBar->bHideVisibleApplis
		|| pPrevTaskBar->bAppliOnCurrentDesktopOnly != pTaskBar->bAppliOnCurrentDesktopOnly
		|| pPrevTaskBar->bMixLauncherAppli != pTaskBar->bMixLauncherAppli
		|| pPrevTaskBar->bOverWriteXIcons != pTaskBar->bOverWriteXIcons
		|| pPrevTaskBar->iMinimizedWindowRenderType != pTaskBar->iMinimizedWindowRenderType
		|| pPrevTaskBar->iAppliMaxNameLength != pTaskBar->iAppliMaxNameLength
		|| cairo_dock_strings_differ (pPrevTaskBar->cGroupException, pTaskBar->cGroupException)
		|| cairo_dock_strings_differ (pPrevTaskBar->cOverwriteException, pTaskBar->cOverwriteException)
		|| pPrevTaskBar->bShowAppli != pTaskBar->bShowAppli
		|| pPrevTaskBar->iIconPlacement != pTaskBar->iIconPlacement
		|| pPrevTaskBar->bSeparateApplis != pTaskBar->bSeparateApplis
		|| cairo_dock_strings_differ (pPrevTaskBar->cRelativeIconName, pTaskBar->cRelativeIconName))
	{
		_cairo_dock_stop_application_manager ();
		
		cairo_dock_start_applications_manager (pDock);
		
		cairo_dock_calculate_dock_icons (pDock);
		gtk_widget_queue_draw (pDock->container.pWidget);  // le 'gdk_window_move_resize' ci-dessous ne provoquera pas le redessin si la taille n'a pas change.
		
		cairo_dock_move_resize_dock (pDock);
	}
	else
	{
		gtk_widget_queue_draw (pDock->container.pWidget);  // pour le fVisibleAlpha
	}
}


  //////////////
 /// UNLOAD ///
//////////////

static gboolean _remove_appli (Window *pXid, Icon *pIcon, gpointer data)
{
	if (pIcon == NULL)
		return TRUE;
	if (pIcon->Xid == 0)
	{
		g_free (pIcon);
		return TRUE;
	}
	
	if (pIcon->iBackingPixmap != 0)
	{
		XFreePixmap (s_XDisplay, pIcon->iBackingPixmap);
		pIcon->iBackingPixmap = 0;
	}
	
	cairo_dock_set_xicon_geometry (pIcon->Xid, 0, 0, 0, 0);  // since we'll not detach the icons one by one, we do it here.
	cairo_dock_set_xwindow_mask (pIcon->Xid, None);  // we'll watch again anyway (even if we don't display the icon) but for the moment, stop it.
	
	// make it an invalid appli
	pIcon->Xid = 0;  // we don't want to go into the 'unregister'
	g_free (pIcon->cClass);  // nor the class manager.
	pIcon->cClass = NULL;
	
	// if not inside a dock, free it, else it will be freeed with the dock.
	if (pIcon->cParentDockName == NULL)  // not in a dock.
	{
		cairo_dock_free_icon (pIcon);
	}
	
	return TRUE;
}
static void unload (void)
{
	// empty the class table.
	cairo_dock_reset_class_table ();
	
	// empty the applis table.
	g_hash_table_foreach_remove (s_hXWindowTable, (GHRFunc) _remove_appli, NULL);
	
	s_bAppliManagerIsRunning = FALSE;
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	s_hXWindowTable = g_hash_table_new_full (g_int_hash,
		g_int_equal,
		g_free,
		NULL);
	
	s_XDisplay = cairo_dock_get_Xdisplay ();
	
	s_aNetWmState			= XInternAtom (s_XDisplay, "_NET_WM_STATE", False);
	s_aNetWmName 			= XInternAtom (s_XDisplay, "_NET_WM_NAME", False);
	s_aUtf8String 			= XInternAtom (s_XDisplay, "UTF8_STRING", False);
	s_aWmName 				= XInternAtom (s_XDisplay, "WM_NAME", False);
	s_aString 				= XInternAtom (s_XDisplay, "STRING", False);
	s_aNetWmIcon 			= XInternAtom (s_XDisplay, "_NET_WM_ICON", False);
	s_aWmHints 				= XInternAtom (s_XDisplay, "WM_HINTS", False);
	s_aNetWmDesktop			= XInternAtom (s_XDisplay, "_NET_WM_DESKTOP", False);
	
	cairo_dock_initialize_class_manager ();
	
	cairo_dock_register_notification_on_object (&myDesktopMgr,
		NOTIFICATION_WINDOW_ACTIVATED,
		(CairoDockNotificationFunc) _on_change_active_window_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification_on_object (&myDesktopMgr,
		NOTIFICATION_WINDOW_CONFIGURED,
		(CairoDockNotificationFunc) _on_window_configured_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification_on_object (&myDesktopMgr,
		NOTIFICATION_WINDOW_PROPERTY_CHANGED,
		(CairoDockNotificationFunc) _on_property_changed_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification_on_object (&myDesktopMgr,
		NOTIFICATION_DESKTOP_CHANGED,
		(CairoDockNotificationFunc) _on_change_current_desktop_viewport_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_applications_manager (void)
{
	// Manager
	memset (&myTaskbarMgr, 0, sizeof (CairoTaskbarManager));
	myTaskbarMgr.mgr.cModuleName 	= "Taskbar";
	myTaskbarMgr.mgr.init 		= init;
	myTaskbarMgr.mgr.load 		= NULL;  // the manager is started after the launchers&applets have been created, to avoid unecessary computations.
	myTaskbarMgr.mgr.unload 		= unload;
	myTaskbarMgr.mgr.reload 		= (GldiManagerReloadFunc)reload;
	myTaskbarMgr.mgr.get_config 	= (GldiManagerGetConfigFunc)get_config;
	myTaskbarMgr.mgr.reset_config = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myTaskbarParam, 0, sizeof (CairoTaskbarParam));
	myTaskbarMgr.mgr.pConfig = (GldiManagerConfigPtr)&myTaskbarParam;
	myTaskbarMgr.mgr.iSizeOfConfig = sizeof (CairoTaskbarParam);
	// data
	myTaskbarMgr.mgr.pData = (GldiManagerDataPtr)NULL;
	myTaskbarMgr.mgr.iSizeOfData = 0;
	// signals
	cairo_dock_install_notifications_on_object (&myTaskbarMgr, NB_NOTIFICATIONS_TASKBAR);
	// register
	gldi_register_manager (GLDI_MANAGER(&myTaskbarMgr));
}
