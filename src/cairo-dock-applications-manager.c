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

#include "../config.h"
#ifdef HAVE_XEXTEND
#include <X11/extensions/Xcomposite.h>
//#include <X11/extensions/Xdamage.h>
#endif

#include "cairo-dock-icons.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-load.h"
#include "cairo-dock-application-factory.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-container.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-applications-manager.h"

#define CAIRO_DOCK_TASKBAR_CHECK_INTERVAL 200
#define CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME "default-icon-appli.svg"

extern CairoDock *g_pMainDock;

extern CairoDockDesktopGeometry g_desktopGeometry;

extern gboolean g_bUseOpenGL;
//extern int g_iDamageEvent;

static Display *s_XDisplay = NULL;
static GHashTable *s_hXWindowTable = NULL;  // table des fenetres X affichees dans le dock.
static int s_bAppliManagerIsRunning = FALSE;
static int s_iTime = 1;  // on peut aller jusqu'a 2^31, soit 17 ans a 4Hz.
static Window s_iCurrentActiveWindow = 0;
static Atom s_aNetWmState;
static Atom s_aNetWmDesktop;
static Atom s_aNetShowingDesktop;
static Atom s_aRootMapID;
static Atom s_aNetNbDesktops;
static Atom s_aNetWmName;
static Atom s_aUtf8String;
static Atom s_aWmName;
static Atom s_aString;
static Atom s_aNetWmIcon;
static Atom s_aWmHints;

static void cairo_dock_blacklist_appli (Window Xid);
///static gboolean _cairo_dock_window_is_on_our_way (Window *Xid, Icon *icon, gpointer *data);

  //////////////////////////
 // Appli manager : core //
//////////////////////////

static void _cairo_dock_hide_show_windows_on_other_desktops (Window *Xid, Icon *icon, CairoDock *pMainDock)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && (! myTaskBar.bHideVisibleApplis || icon->bIsHidden))
	{
		cd_debug ("%s (%d)", __func__, *Xid);
		CairoDock *pParentDock = NULL;
		if (cairo_dock_appli_is_on_current_desktop (icon))
		{
			cd_debug (" => est sur le bureau actuel.");
			if (icon->cParentDockName == NULL)
			{
				pParentDock = cairo_dock_insert_appli_in_dock (icon, pMainDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
			}
		}
		else
		{
			cd_debug (" => n'est pas sur le bureau actuel.");
			pParentDock = cairo_dock_detach_appli (icon);
		}
		if (pParentDock != NULL)
			gtk_widget_queue_draw (pParentDock->container.pWidget);
	}
}

static gboolean _cairo_dock_remove_old_applis (Window *Xid, Icon *icon, gpointer iTimePtr)
{
	if (icon == NULL)
		return FALSE;
	gint iTime = GPOINTER_TO_INT (iTimePtr);
	
	//g_print ("%s (%s(%ld) %d / %d)\n", __func__, icon->cName, icon->Xid, icon->iLastCheckTime, iTime);
	if (icon->iLastCheckTime >= 0 && icon->iLastCheckTime < iTime && ! cairo_dock_icon_is_being_removed (icon))
	{
		cd_message ("cette fenetre (%ld(%ld), %s) est trop vieille (%d / %d)", *Xid, icon->Xid, icon->cName, icon->iLastCheckTime, iTime);
		if (CAIRO_DOCK_IS_APPLI (icon))
		{
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
				cairo_dock_update_name_on_inhibators (icon->cClass, *Xid, NULL);
				icon->iLastCheckTime = -1;  // pour ne pas la desenregistrer de la HashTable lors du 'free' puisqu'on est en train de parcourir la table.
				cairo_dock_free_icon (icon);
				/// redessiner les inhibiteurs ?...
			}
			
			if (myAccessibility.bAutoHideOnAnyOverlap && cairo_dock_is_temporary_hidden (g_pMainDock))
			{
				/// le faire pour tous les docks principaux ...
				if (cairo_dock_search_window_overlapping_dock (g_pMainDock) == NULL)  // on regarde si une autre gene encore.
				{
					cd_message (" => plus aucune fenetre genante");
					cairo_dock_deactivate_temporary_auto_hide ();
				}
			}
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
	Window *pXWindowsList = cairo_dock_get_windows_list (&iNbWindows, TRUE);
	
	Window Xid;
	Icon *icon;
	int iStackOrder = 0;
	gpointer pOriginalXid;
	gboolean bAppliAlreadyRegistered;
	gboolean bUpdateMainDockSize = FALSE;
	CairoDock *pParentDock;
	
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
				if ((! myTaskBar.bAppliOnCurrentDesktopOnly || cairo_dock_xwindow_is_on_current_desktop (Xid)))  // bHideVisibleApplis est gere lors de l'insertion.
				{
					cd_message (" insertion de %s ... (%d)", icon->cName, icon->iLastCheckTime);
					pParentDock = cairo_dock_insert_appli_in_dock (icon, pDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
					if (pParentDock != NULL)
					{
						if (pParentDock->bIsMainDock)
							bUpdateMainDockSize = TRUE;
						else
							cairo_dock_update_dock_size (pParentDock);
					}
				}
				else if (myTaskBar.bMixLauncherAppli)  // on met tout de meme l'indicateur sur le lanceur.
				{
					cairo_dock_prevent_inhibated_class (icon);
				}
				/**if ((myAccessibility.bAutoHideOnMaximized && icon->bIsMaximized && ! icon->bIsHidden) || (myAccessibility.bAutoHideOnFullScreen && icon->bIsFullScreen && ! icon->bIsHidden))
				{
					if (! cairo_dock_is_temporary_hidden (pDock))
					{
						if (cairo_dock_xwindow_is_on_current_desktop (Xid) && cairo_dock_appli_hovers_dock (icon, pDock))
						{
							cd_message (" cette nouvelle fenetre empiete sur notre dock");
							cairo_dock_activate_temporary_auto_hide ();
						}
					}
				}*/
				if (myAccessibility.bAutoHideOnAnyOverlap)
				{
					if (! cairo_dock_is_temporary_hidden (pDock))
					{
						if (cairo_dock_xwindow_is_on_current_desktop (Xid) && cairo_dock_appli_overlaps_dock (icon, pDock))
						{
							cd_message (" cette nouvelle fenetre empiete sur notre dock");
							cairo_dock_activate_temporary_auto_hide ();
						}
					}
				}
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
	
	if (bUpdateMainDockSize)
		cairo_dock_update_dock_size (pDock);

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
			if (icon->bIsDemandingAttention)  // on force ici, car il semble qu'on ne recoive pas toujours le retour a la normale.
				cairo_dock_appli_stops_demanding_attention (icon);
			
			pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			if (pParentDock == NULL)  // elle est soit inhibee, soit pas dans un dock.
			{
				cairo_dock_update_activity_on_inhibators (icon->cClass, icon->Xid);
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
				cairo_dock_update_inactivity_on_inhibators (pLastActiveIcon->cClass, pLastActiveIcon->Xid);
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
		if (myAccessibility.bAutoHideOnOverlap || myAccessibility.bAutoHideOnFullScreen)
		{
			if (! CAIRO_DOCK_IS_APPLI (icon))
			{
				Window iPropWindow;
				XGetTransientForHint (s_XDisplay, XActiveWindow, &iPropWindow);
				icon = g_hash_table_lookup (s_hXWindowTable, &iPropWindow);
				//g_print ("*** la fenetre parente est : %s\n", icon?icon->cName:"aucune");
			}
			if (_cairo_dock_appli_is_on_our_way (icon, g_pMainDock))  // la nouvelle fenetre active nous gene.
			{
				if (!cairo_dock_is_temporary_hidden (g_pMainDock))
					cairo_dock_activate_temporary_auto_hide ();
			}
			else
			{
				if (cairo_dock_is_temporary_hidden (g_pMainDock))
					cairo_dock_deactivate_temporary_auto_hide ();
			}
		}
		
		// notification xklavier.
		if (bForceKbdStateRefresh)  // si on active une fenetre n'ayant pas de focus clavier, on n'aura pas d'evenement kbd_changed, pourtant en interne le clavier changera. du coup si apres on revient sur une fenetre qui a un focus clavier, il risque de ne pas y avoir de changement de clavier, et donc encore une fois pas d'evenement ! pour palier a ce, on considere que les fenetres avec focus clavier sont celles presentes en barre des taches. On decide de generer un evenement lorsqu'on revient sur une fenetre avec focus, a partir d'une fenetre sans focus (mettre a jour le clavier pour une fenetre sans focus n'a pas grand interet, autant le laisser inchange).
			cairo_dock_notify (CAIRO_DOCK_KBD_STATE_CHANGED, &XActiveWindow);
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

static gboolean _on_change_current_desktop_viewport_notification (gpointer data)
{
	CairoDock *pDock = g_pMainDock;
	
	// applis du bureau courant seulement.
	if (myTaskBar.bAppliOnCurrentDesktopOnly)
	{
		g_hash_table_foreach (s_hXWindowTable, (GHFunc) _cairo_dock_hide_show_windows_on_other_desktops, pDock);
	}
	
	cairo_dock_hide_show_launchers_on_other_desktops(pDock);
	
	// masquage du dock.
	if (myAccessibility.bAutoHideOnFullScreen || myAccessibility.bAutoHideOnOverlap)
	{
		Icon *pActiveAppli = cairo_dock_get_current_active_icon ();
		if (_cairo_dock_appli_is_on_our_way (pActiveAppli, pDock))  // la fenetre active nous gene.
		{
			if (!cairo_dock_is_temporary_hidden (pDock))
				cairo_dock_activate_temporary_auto_hide ();
		}
		else
		{
			if (cairo_dock_is_temporary_hidden (pDock))
				cairo_dock_deactivate_temporary_auto_hide ();
		}
		/**if (cairo_dock_is_temporary_hidden (pDock))
		{
			if (cairo_dock_search_window_overlapping_dock (pDock) == NULL)
			{
				cd_message (" => plus aucune fenetre genante");
				cairo_dock_deactivate_temporary_auto_hide ();
			}
		}
		else
		{
			if (cairo_dock_search_window_overlapping_dock (pDock) != NULL)
			{
				cd_message (" => une fenetre est genante");
				cairo_dock_activate_temporary_auto_hide ();
			}
		}*/
	}
	if (myAccessibility.bAutoHideOnAnyOverlap)
	{
		if (cairo_dock_is_temporary_hidden (pDock))
		{
			if (cairo_dock_search_window_overlapping_dock (pDock) == NULL)
			{
				cd_message (" => plus aucune fenetre genante");
				cairo_dock_deactivate_temporary_auto_hide ();
			}
		}
		else
		{
			if (cairo_dock_search_window_overlapping_dock (pDock) != NULL)
			{
				cd_message (" => une fenetre est genante");
				cairo_dock_activate_temporary_auto_hide ();
			}
		}
	}
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
	if (bDemandsAttention && (myTaskBar.bDemandsAttentionWithDialog || myTaskBar.cAnimationOnDemandsAttention))  // elle peut demander l'attention plusieurs fois de suite.
	{
		cd_debug ("%s demande votre attention %s !", icon->cName, icon->bIsDemandingAttention?"encore une fois":"");
		cairo_dock_appli_demands_attention (icon);
	}
	else if (! bDemandsAttention)
	{
		if (icon->bIsDemandingAttention)
		{
			cd_debug ("%s se tait", icon->cName);
			cairo_dock_appli_stops_demanding_attention (icon);  // ca c'est plus une precaution qu'autre chose.
		}
	}
	
	// masquage du dock.
	if (myAccessibility.bAutoHideOnOverlap || myAccessibility.bAutoHideOnFullScreen)
	{
		if (Xid == s_iCurrentActiveWindow)  // c'est la fenetre courante qui a change d'etat.
		{
			if ((bIsHidden != icon->bIsHidden) || (bIsFullScreen != icon->bIsFullScreen))  // si c'est l'etat maximise qui a change, on le verra au changement de dimensions.
			{
				icon->bIsFullScreen = bIsFullScreen;
				icon->bIsHidden = bIsHidden;
				if (_cairo_dock_appli_is_on_our_way (icon, g_pMainDock))  // la fenetre active nous gene.
				{
					if (!cairo_dock_is_temporary_hidden (g_pMainDock))
						cairo_dock_activate_temporary_auto_hide ();
				}
				else
				{
					if (cairo_dock_is_temporary_hidden (g_pMainDock))
						cairo_dock_deactivate_temporary_auto_hide ();
				}
			}
		}
	}
	
	icon->bIsMaximized = bIsMaximized;
	icon->bIsFullScreen = bIsFullScreen;
	icon->bIsHidden = bIsHidden;
	
	// on gere le cachage/apparition de l'icone (transparence ou miniature, applis minimisees seulement).
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (bIsHidden != bPrevHidden)
	{
		cd_message ("  changement de visibilite -> %d", bIsHidden);
		icon->bIsHidden = bIsHidden;
		
		if (myAccessibility.bAutoHideOnAnyOverlap)
		{
			if (! bIsHidden && ! cairo_dock_is_temporary_hidden (g_pMainDock))
			{
				cd_message (" => %s devient genante", CAIRO_DOCK_IS_APPLI (icon) ? icon->cName : "une fenetre");
				if (CAIRO_DOCK_IS_APPLI (icon) && cairo_dock_appli_overlaps_dock (icon, g_pMainDock))
					cairo_dock_activate_temporary_auto_hide ();
			}
			else if (bIsHidden && cairo_dock_is_temporary_hidden (g_pMainDock))
			{
				if (cairo_dock_search_window_overlapping_dock (g_pMainDock) == NULL)
				{
					cd_message (" => plus aucune fenetre genante");
					cairo_dock_deactivate_temporary_auto_hide ();
				}
			}
		}
		
		// affichage des applis minimisees.
		if (g_bUseOpenGL && myTaskBar.iMinimizedWindowRenderType == 2)
		{
			CairoDock *pDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			if (pDock != NULL)
			{
				cairo_dock_draw_hidden_appli_icon (icon, CAIRO_CONTAINER (pDock), TRUE);
			}
		}
		else if (myTaskBar.iMinimizedWindowRenderType == 0)
		{
			// transparence sur les inhibiteurs.
			cairo_dock_update_visibility_on_inhibators (icon->cClass, icon->Xid, icon->bIsHidden);
		}
		
		// applis minimisees seulement
		if (myTaskBar.bHideVisibleApplis)  // on insere/detache l'icone selon la visibilite de la fenetre, avec une animation.
		{
			if (bIsHidden)  // se cache => on insere son icone.
			{
				cd_message (" => se cache");
				if (! myTaskBar.bAppliOnCurrentDesktopOnly || cairo_dock_appli_is_on_current_desktop (icon))
				{
					pParentDock = cairo_dock_insert_appli_in_dock (icon, g_pMainDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);
					if (pParentDock != NULL)
					{
						if (g_bUseOpenGL && myTaskBar.iMinimizedWindowRenderType == 2)  // quand on est passe dans ce cas tout a l'heure l'icone n'etait pas encore dans son dock.
							cairo_dock_draw_hidden_appli_icon (icon, CAIRO_CONTAINER (pParentDock), TRUE);
						gtk_widget_queue_draw (pParentDock->container.pWidget);
					}
				}
			}
			else  // se montre => on detache l'icone.
			{
				cd_message (" => re-apparait");
				cairo_dock_trigger_icon_removal_from_dock (icon);
			}
		}
		else if (myTaskBar.fVisibleAppliAlpha != 0)  // transparence
		{
			icon->fAlpha = 1;  // on triche un peu.
			if (pParentDock != NULL)
				cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pParentDock));
		}
		
		// miniature (on le fait apres l'avoir inseree/detachee, car comme ca dans le cas ou on l'enleve du dock apres l'avoir deminimisee, l'icone est marquee comme en cours de suppression, et donc on ne recharge pas son icone. Sinon l'image change pendant la transition, ce qui est pas top. Comme ca ne change pas la taille de l'icone dans le dock, on peut faire ca apres l'avoir inseree.
		#ifdef HAVE_XEXTEND
		if (myTaskBar.iMinimizedWindowRenderType == 1 && (pParentDock != NULL || myTaskBar.bHideVisibleApplis))  // on recupere la miniature ou au contraire on remet l'icone.
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
				cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pParentDock));
		}
		#endif
	}
}

static void _on_change_window_desktop (Icon *icon)
{
	Window Xid = icon->Xid;
	icon->iNumDesktop = cairo_dock_get_xwindow_desktop (Xid);
	
	// applis du bureau courant seulement.
	if (myTaskBar.bAppliOnCurrentDesktopOnly)
	{
		_cairo_dock_hide_show_windows_on_other_desktops (&Xid, icon, g_pMainDock);  // si elle vient sur notre bureau, elle n'est pas forcement sur le meme viewport, donc il faut le verifier.
	}
	
	// masquage du dock.
	if (myAccessibility.bAutoHideOnOverlap || myAccessibility.bAutoHideOnFullScreen)
	{
		if (Xid == s_iCurrentActiveWindow)  // c'est la fenetre courante qui a change de bureau.
		{
			if (_cairo_dock_appli_is_on_our_way (icon, g_pMainDock))  // la fenetre active nous gene.
			{
				if (!cairo_dock_is_temporary_hidden (g_pMainDock))
					cairo_dock_activate_temporary_auto_hide ();
			}
			else
			{
				if (cairo_dock_is_temporary_hidden (g_pMainDock))
					cairo_dock_deactivate_temporary_auto_hide ();
			}
		}
	}
	if (myAccessibility.bAutoHideOnAnyOverlap)
	{
		if (! cairo_dock_is_temporary_hidden (g_pMainDock))
		{
			if ((icon->iNumDesktop == -1 || icon->iNumDesktop == g_desktopGeometry.iCurrentDesktop) && icon->iViewPortX == g_desktopGeometry.iCurrentViewportX && icon->iViewPortY == g_desktopGeometry.iCurrentViewportY)  // l'appli arrive sur le bureau courant.
			{
				if (cairo_dock_appli_overlaps_dock (icon, g_pMainDock))
				{
					cd_message (" => cela nous gene");
					cairo_dock_activate_temporary_auto_hide ();
				}
			}
		}
		else if (icon->iNumDesktop != -1 && icon->iNumDesktop != g_desktopGeometry.iCurrentDesktop)  // l'appli quitte sur le bureau courant.
		{
			if (cairo_dock_search_window_overlapping_dock (g_pMainDock) == NULL)
			{
				cd_message (" => plus aucune fenetre genante");
				cairo_dock_deactivate_temporary_auto_hide ();
			}
		}
	}
}

static void _on_change_window_size_position (Icon *icon, XConfigureEvent *e)
{
	Window Xid = icon->Xid;
	
	#ifdef HAVE_XEXTEND
	if (e->width != icon->windowGeometry.width || e->height != icon->windowGeometry.height)
	{
		if (icon->iBackingPixmap != 0)
			XFreePixmap (s_XDisplay, icon->iBackingPixmap);
		if (myTaskBar.iMinimizedWindowRenderType == 1)
		{
			icon->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, Xid);
			cd_message ("new backing pixmap : %d", icon->iBackingPixmap);
		}
		else
			icon->iBackingPixmap = 0;
	}
	#endif
	icon->windowGeometry.width = e->width;
	icon->windowGeometry.height = e->height;
	icon->windowGeometry.x = e->x;
	icon->windowGeometry.y = e->y;
	icon->iViewPortX = e->x / g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] + g_desktopGeometry.iCurrentViewportX;
	icon->iViewPortY = e->y / g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] + g_desktopGeometry.iCurrentViewportY;
	
	// on regarde si l'appli est sur le viewport courant.
	if (e->x + e->width <= 0 || e->x >= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] || e->y + e->height <= 0 || e->y >= g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL])  // en fait il faudrait faire ca modulo le nombre de viewports * la largeur d'un bureau, car avec une fenetre a droite, elle peut revenir sur le bureau par la gauche si elle est tres large...
	{
		// applis du bureau courant seulement.
		if (myTaskBar.bAppliOnCurrentDesktopOnly && icon->cParentDockName != NULL)
		{
			CairoDock *pParentDock = cairo_dock_detach_appli (icon);
			if (pParentDock)
				gtk_widget_queue_draw (pParentDock->container.pWidget);
		}
		
		if (myAccessibility.bAutoHideOnAnyOverlap)
		{
			if (cairo_dock_is_temporary_hidden (g_pMainDock))
			{
				if (cairo_dock_search_window_overlapping_dock (g_pMainDock) == NULL)
				{
					cd_message (" chgt de viewport => plus aucune fenetre genante");
					cairo_dock_deactivate_temporary_auto_hide ();
				}
			}
		}
	}
	else  // elle est sur le bureau.
	{
		// applis du bureau courant seulement.
		if (myTaskBar.bAppliOnCurrentDesktopOnly && icon->cParentDockName == NULL)
		{
			cd_message ("cette fenetre est sur le bureau courant (%d;%d)", e->x, e->y);
			gboolean bInsideDock = (icon->cParentDockName != NULL);  // jamais verifie mais ca devrait etre bon.
			if (! bInsideDock)
				cairo_dock_insert_appli_in_dock (icon, g_pMainDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
		}
		
		if (myAccessibility.bAutoHideOnAnyOverlap)
		{
			if (cairo_dock_appli_overlaps_dock (icon, g_pMainDock))  // cette fenetre peut provoquer l'auto-hide.
			{
				if (! cairo_dock_is_temporary_hidden (g_pMainDock))
				{
					cd_message (" sur le viewport courant => cela nous gene");
					cairo_dock_activate_temporary_auto_hide ();
				}
			}
			else  // ne gene pas/plus.
			{
				if (cairo_dock_is_temporary_hidden (g_pMainDock))
				{
					if (cairo_dock_search_window_overlapping_dock (g_pMainDock) == NULL)
					{
						cd_message (" chgt de viewport => plus aucune fenetre genante");
						cairo_dock_deactivate_temporary_auto_hide ();
					}
				}
			}
		}
	}
	
	// masquage du dock.
	if (myAccessibility.bAutoHideOnOverlap || myAccessibility.bAutoHideOnFullScreen)
	{
		if (Xid == s_iCurrentActiveWindow)  // c'est la fenetre courante qui a change de bureau.
		{
			if (_cairo_dock_appli_is_on_our_way (icon, g_pMainDock))  // la fenetre active nous gene.
			{
				if (!cairo_dock_is_temporary_hidden (g_pMainDock))
					cairo_dock_activate_temporary_auto_hide ();
			}
			else
			{
				if (cairo_dock_is_temporary_hidden (g_pMainDock))
					cairo_dock_deactivate_temporary_auto_hide ();
			}
		}
	}
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
			g_free (icon->cName);
			icon->cName = cName;
			
			cairo_dock_load_icon_text (icon, &myLabels.iconTextDescription);
			
			cairo_dock_update_name_on_inhibators (icon->cClass, icon->Xid, icon->cName);
		}
		else
			g_free (cName);
	}
}

static void _on_change_window_icon (Icon *icon, CairoDock *pDock)
{
	if (cairo_dock_class_is_using_xicon (icon->cClass) || ! myTaskBar.bOverWriteXIcons)
	{
		cairo_dock_reload_icon_image (icon, CAIRO_CONTAINER (pDock));
		if (pDock->iRefCount != 0)
			cairo_dock_trigger_redraw_subdock_content (pDock);
		cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pDock));
	}
}

static void _on_change_window_hints (Icon *icon, CairoDock *pDock, int iState)
{
	XWMHints *pWMHints = XGetWMHints (s_XDisplay, icon->Xid);
	if (pWMHints != NULL)
	{
		if ((pWMHints->flags & XUrgencyHint) && (myTaskBar.bDemandsAttentionWithDialog || myTaskBar.cAnimationOnDemandsAttention))
		{
			if (iState == PropertyNewValue)
			{
				cd_debug ("%s vous interpelle !", icon->cName);
				cairo_dock_appli_demands_attention (icon);
			}
			else if (iState == PropertyDelete)
			{
				cd_debug ("%s arrette de vous interpeler.", icon->cName);
				cairo_dock_appli_stops_demanding_attention (icon);
			}
			else
				cd_warning ("  etat du changement d'urgence inconnu sur %s !", icon->cName);
		}
		if (iState == PropertyNewValue && (pWMHints->flags & (IconPixmapHint | IconMaskHint | IconWindowHint)))
		{
			//g_print ("%s change son icone\n", icon->cName);
			if (cairo_dock_class_is_using_xicon (icon->cClass) || ! myTaskBar.bOverWriteXIcons)
			{
				cairo_dock_reload_icon_image (icon, CAIRO_CONTAINER (pDock));
				if (pDock->iRefCount != 0)
					cairo_dock_trigger_redraw_subdock_content (pDock);
				cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pDock));
			}
		}
	}
}

static gboolean _on_property_changed_notification (gpointer data, Window Xid, Atom aProperty, int iState)
{
	Icon *icon = g_hash_table_lookup (s_hXWindowTable, &Xid);
	if (! CAIRO_DOCK_IS_APPLI (icon))  // appli blacklistee
	{
		if (! cairo_dock_xwindow_skip_taskbar (Xid))
		{
			//g_print ("Special case : this appli (%ld) should not be ignored any more!\n", Xid);
			g_hash_table_remove (s_hXWindowTable, &Xid);
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
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}

  ///////////////////////////
 // Applis manager : core //
///////////////////////////

void cairo_dock_initialize_application_manager (Display *pDisplay)
{
	s_XDisplay = pDisplay;

	s_hXWindowTable = g_hash_table_new_full (g_int_hash,
		g_int_equal,
		g_free,
		NULL);
	
	s_aNetWmState			= XInternAtom (s_XDisplay, "_NET_WM_STATE", False);
	s_aNetWmName 			= XInternAtom (s_XDisplay, "_NET_WM_NAME", False);
	s_aUtf8String 			= XInternAtom (s_XDisplay, "UTF8_STRING", False);
	s_aWmName 				= XInternAtom (s_XDisplay, "WM_NAME", False);
	s_aString 				= XInternAtom (s_XDisplay, "STRING", False);
	s_aNetWmIcon 			= XInternAtom (s_XDisplay, "_NET_WM_ICON", False);
	s_aWmHints 				= XInternAtom (s_XDisplay, "WM_HINTS", False);
	s_aNetWmDesktop			= XInternAtom (s_XDisplay, "_NET_WM_DESKTOP", False);
	s_aNetShowingDesktop 	= XInternAtom (s_XDisplay, "_NET_SHOWING_DESKTOP", False);
	s_aRootMapID			= XInternAtom (s_XDisplay, "_XROOTPMAP_ID", False);
	s_aNetNbDesktops		= XInternAtom (s_XDisplay, "_NET_NUMBER_OF_DESKTOPS", False);
	cairo_dock_initialize_application_factory (pDisplay);
}

static void cairo_dock_register_appli (Icon *icon)
{
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cd_debug ("%s (%ld ; %s)", __func__, icon->Xid, icon->cName);
		Window *pXid = g_new (Window, 1);
			*pXid = icon->Xid;
		g_hash_table_insert (s_hXWindowTable, pXid, icon);
		
		cairo_dock_set_xwindow_mask (icon->Xid, PropertyChangeMask | StructureNotifyMask);
		
		cairo_dock_add_appli_to_class (icon);
	}
}

static void cairo_dock_blacklist_appli (Window Xid)
{
	if (Xid > 0)
	{
		cd_debug ("%s (%ld)", __func__, Xid);
		Window *pXid = g_new (Window, 1);
		*pXid = Xid;
		cairo_dock_set_xwindow_mask (Xid, PropertyChangeMask | StructureNotifyMask);  // on veut pouvoir etre notifie de ses changements d'etat (si "skip taskbar" disparait, elle reviendra dans la barre des taches).
		
		Icon *pNullIcon = g_new0 (Icon, 1);
		pNullIcon->iLastCheckTime = s_iTime;
		g_hash_table_insert (s_hXWindowTable, pXid, pNullIcon);  // NULL
	}
}

void cairo_dock_unregister_appli (Icon *icon)
{
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cd_message ("%s (%ld ; %s)", __func__, icon->Xid, icon->cName);
		if (icon->iLastCheckTime != -1)
			g_hash_table_remove (s_hXWindowTable, &icon->Xid);
		
		cairo_dock_set_xwindow_mask (icon->Xid, None);
		
		if (icon->iBackingPixmap != 0)
		{
			XFreePixmap (s_XDisplay, icon->iBackingPixmap);
			icon->iBackingPixmap = 0;
		}
		
		cairo_dock_remove_appli_from_class (icon);  // n'efface pas sa classe (on peut en avoir besoin encore).
		cairo_dock_update_Xid_on_inhibators (icon->Xid, icon->cClass);
		
		icon->Xid = 0;  // hop, elle n'est plus une appli.
	}
}


void cairo_dock_start_application_manager (CairoDock *pDock)
{
	g_return_if_fail (!s_bAppliManagerIsRunning && myTaskBar.bShowAppli);
	
	cairo_dock_set_overwrite_exceptions (myTaskBar.cOverwriteException);
	cairo_dock_set_group_exceptions (myTaskBar.cGroupException);
	
	//\__________________ On s'enregistre aux signaux
	cairo_dock_register_notification (CAIRO_DOCK_WINDOW_ACTIVATED,
		(CairoDockNotificationFunc) _on_change_active_window_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_WINDOW_CONFIGURED,
		(CairoDockNotificationFunc) _on_window_configured_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_WINDOW_PROPERTY_CHANGED,
		(CairoDockNotificationFunc) _on_property_changed_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	cairo_dock_register_notification (CAIRO_DOCK_DESKTOP_CHANGED,
		(CairoDockNotificationFunc) _on_change_current_desktop_viewport_notification,
		CAIRO_DOCK_RUN_FIRST, NULL);
	
	//\__________________ On recupere l'ensemble des fenetres presentes.
	gulong i, iNbWindows = 0;
	Window *pXWindowsList = cairo_dock_get_windows_list (&iNbWindows, FALSE);  // on recupere les fenetres par ordre de creation, de facon a ce que si on redemarre la barre des taches, les lanceurs soient lies aux memes fenetres. Au prochain update, la liste sera recuperee par z-order, ce qui remettra le z-order de chaque icone a jour.
	
	if (s_iCurrentActiveWindow == 0)
		s_iCurrentActiveWindow = cairo_dock_get_active_xwindow ();
	
	//\__________________ On cree les icones de toutes ces applis.
	CairoDock *pParentDock;
	gboolean bUpdateMainDockSize = FALSE;
	
	/*Icon * pLastAppli = cairo_dock_get_last_appli (pDock->icons);
	int iOrder = (pLastAppli != NULL ? pLastAppli->fOrder + 1 : 1);*/
	Window Xid;
	Icon *pIcon;
	for (i = 0; i < iNbWindows; i ++)
	{
		Xid = pXWindowsList[i];
		pIcon = cairo_dock_create_icon_from_xwindow (Xid, pDock);
		
		if (pIcon != NULL)
		{
			//pIcon->fOrder = iOrder++;
			pIcon->iLastCheckTime = s_iTime;
			if (! myTaskBar.bAppliOnCurrentDesktopOnly || cairo_dock_appli_is_on_current_desktop (pIcon))  // le filtre 'bHideVisibleApplis' est gere dans la fonction d'insertion.
			{
				pParentDock = cairo_dock_insert_appli_in_dock (pIcon, pDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
				//g_print (">>>>>>>>>>>> Xid : %d\n", Xid);
				if (pParentDock != NULL)
				{
					if (pParentDock->bIsMainDock)
						bUpdateMainDockSize = TRUE;
					else
						cairo_dock_update_dock_size (pParentDock);
				}
			}
			else if (myTaskBar.bMixLauncherAppli)  // on met tout de meme l'indicateur sur le lanceur.
			{
				cairo_dock_prevent_inhibated_class (pIcon);
			}
			
			if (myAccessibility.bAutoHideOnAnyOverlap)
			{
				if (! cairo_dock_is_temporary_hidden (pDock) && cairo_dock_appli_is_on_current_desktop (pIcon))
				{
					if (cairo_dock_appli_overlaps_dock (pIcon, pDock))
					{
						cairo_dock_activate_temporary_auto_hide ();
					}
				}
			}
		}
		else
			cairo_dock_blacklist_appli (Xid);
	}
	if (pXWindowsList != NULL)
		XFree (pXWindowsList);
	
	if (bUpdateMainDockSize)
		cairo_dock_update_dock_size (pDock);
	
	// masquage du dock.
	if (myAccessibility.bAutoHideOnOverlap || myAccessibility.bAutoHideOnFullScreen)
	{
		Icon *pActiveAppli = cairo_dock_get_current_active_icon ();
		if (_cairo_dock_appli_is_on_our_way (pActiveAppli, pDock))  // la fenetre active nous gene.
		{
			if (!cairo_dock_is_temporary_hidden (pDock))
			{
				cairo_dock_activate_temporary_auto_hide ();
			}
		}
	}
	else if (myAccessibility.bAutoHideOnAnyOverlap)
	{
		if (cairo_dock_search_window_overlapping_dock (pDock) != NULL)
		{
			if (!cairo_dock_is_temporary_hidden (pDock))
				cairo_dock_activate_temporary_auto_hide ();
		}
	}
	
	s_bAppliManagerIsRunning = TRUE;
}

static gboolean _cairo_dock_reset_appli_table_iter (Window *pXid, Icon *pIcon, gpointer data)
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
		cairo_dock_detach_icon_from_dock (pIcon, pDock, myIcons.iSeparateIcons);
		if (! pDock->bIsMainDock)  // la taille du main dock est mis a jour 1 fois a la fin.
		{
			if (pDock->icons == NULL)  // le dock degage, le fake aussi.
			{
				CairoDock *pFakeClassParentDock = NULL;
				Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeClassParentDock);
				if (CAIRO_DOCK_IS_FAKE_LAUNCHER (pFakeClassIcon))
				//if (pFakeClassIcon != NULL && ! CAIRO_DOCK_IS_APPLI (pFakeClassIcon) && ! CAIRO_DOCK_IS_APPLET (pFakeClassIcon) && ! CAIRO_DOCK_IS_NORMAL_LAUNCHER (pFakeClassIcon) && pFakeClassIcon->cClass != NULL && pFakeClassIcon->cName != NULL && strcmp (pFakeClassIcon->cClass, pFakeClassIcon->cName) == 0)  // stop la parano.
				{
					cd_debug ("on degage le fake qui pointe sur %s", cParentDockName);
					cairo_dock_detach_icon_from_dock (pFakeClassIcon, pFakeClassParentDock, myIcons.iSeparateIcons);
					cairo_dock_free_icon (pFakeClassIcon);
					if (! pFakeClassParentDock->bIsMainDock)
						cairo_dock_update_dock_size (pFakeClassParentDock);
				}
				
				cairo_dock_destroy_dock (pDock, cParentDockName);
			}
			else
				cairo_dock_update_dock_size (pDock);
		}
		g_free (cParentDockName);
	}
	
	pIcon->Xid = 0;  // on ne veut pas passer dans le 'unregister'
	g_free (pIcon->cClass);  // ni la gestion de la classe.
	pIcon->cClass = NULL;
	cairo_dock_free_icon (pIcon);
	return TRUE;
}
void cairo_dock_stop_application_manager (void)
{
	s_bAppliManagerIsRunning = FALSE;
	
	cairo_dock_remove_notification_func (CAIRO_DOCK_WINDOW_ACTIVATED,
		(CairoDockNotificationFunc) _on_change_active_window_notification,
		NULL);
	cairo_dock_remove_notification_func (CAIRO_DOCK_WINDOW_CONFIGURED,
		(CairoDockNotificationFunc) _on_window_configured_notification,
		NULL);
	cairo_dock_remove_notification_func (CAIRO_DOCK_WINDOW_PROPERTY_CHANGED,
		(CairoDockNotificationFunc) _on_property_changed_notification,
		NULL);
	cairo_dock_remove_notification_func (CAIRO_DOCK_DESKTOP_CHANGED,
		(CairoDockNotificationFunc) _on_change_current_desktop_viewport_notification,
		NULL);
	
	cairo_dock_remove_all_applis_from_class_table ();  // enleve aussi les indicateurs.
	
	g_hash_table_foreach_remove (s_hXWindowTable, (GHRFunc) _cairo_dock_reset_appli_table_iter, NULL);  // libere toutes les icones d'appli.
	cairo_dock_update_dock_size (g_pMainDock);
	
	if (cairo_dock_is_temporary_hidden (g_pMainDock))
		cairo_dock_deactivate_temporary_auto_hide ();
}

gboolean cairo_dock_application_manager_is_running (void)
{
	return s_bAppliManagerIsRunning;
}


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
	cd_debug ("%s (main:%d)", __func__, pDock->bIsMainDock);

	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (icon))
		{
			cairo_dock_set_one_icon_geometry_for_window_manager (icon, pDock);
		}
	}
	
	if (pDock->bIsMainDock && myTaskBar.bHideVisibleApplis)  // on complete avec les applis pas dans le dock, pour que l'effet de minimisation pointe (a peu pres) au bon endroit quand on la minimisera.
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
	int iWidth = icon->iImageWidth;
	int iHeight = icon->iImageHeight;
	
	cairo_surface_t *pPrevSurface = icon->pIconBuffer;
	GLuint iPrevTexture = icon->iIconTexture;
	icon->pIconBuffer = NULL;
	icon->iIconTexture = 0;
	//g_print ("%s (%dx%d / %ld)\n", __func__, iWidth, iHeight, icon->iBackingPixmap);
	if (myTaskBar.iMinimizedWindowRenderType == 1 && icon->bIsHidden && icon->iBackingPixmap != 0)
	{
		// on cree la miniature.
		if (g_bUseOpenGL)
		{
			icon->iIconTexture = cairo_dock_texture_from_pixmap (icon->Xid, icon->iBackingPixmap);
			//g_print ("opengl thumbnail : %d\n", icon->iIconTexture);
		}
		if (icon->iIconTexture == 0)
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_xpixmap (icon->iBackingPixmap,
				iWidth,
				iHeight);
			if (g_bUseOpenGL)
				icon->iIconTexture = cairo_dock_create_texture_from_surface (icon->pIconBuffer);
		}
		// on affiche l'image precedente en embleme.
		if (icon->iIconTexture != 0 && iPrevTexture != 0)
		{
			CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			if (pParentDock)  // le dessin de l'embleme utilise cairo_dock_get_icon_extent()
			{
				icon->fWidth *= pParentDock->container.fRatio;
				icon->fHeight *= pParentDock->container.fRatio;
			}
			CairoEmblem *e = cairo_dock_make_emblem_from_texture (iPrevTexture,icon, CAIRO_CONTAINER (pParentDock));
			cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
			cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
			g_free (e);  // on n'utilise pas cairo_dock_free_emblem pour ne pas detruire la texture avec.
			if (pParentDock)
			{
				icon->fWidth /= pParentDock->container.fRatio;
				icon->fHeight /= pParentDock->container.fRatio;
			}
		}
		else if (icon->pIconBuffer != NULL && pPrevSurface != NULL)
		{
			CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			if (pParentDock)  // le dessin de l'embleme utilise cairo_dock_get_icon_extent()
			{
				icon->fWidth *= pParentDock->container.fRatio;
				icon->fHeight *= pParentDock->container.fRatio;
			}
			CairoEmblem *e = cairo_dock_make_emblem_from_surface (pPrevSurface, 0, 0, icon, CAIRO_CONTAINER (pParentDock));
			cairo_dock_set_emblem_position (e, CAIRO_DOCK_EMBLEM_LOWER_LEFT);
			cairo_dock_draw_emblem_on_icon (e, icon, CAIRO_CONTAINER (pParentDock));
			g_free (e);  // meme remarque.
			if (pParentDock)
			{
				icon->fWidth /= pParentDock->container.fRatio;
				icon->fHeight /= pParentDock->container.fRatio;
			}
		}
	}
	if (icon->pIconBuffer == NULL && myTaskBar.bOverWriteXIcons && ! cairo_dock_class_is_using_xicon (icon->cClass))
		icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass, iWidth, iHeight);
	if (icon->pIconBuffer == NULL)
		icon->pIconBuffer = cairo_dock_create_surface_from_xwindow (icon->Xid, iWidth, iHeight);
	if (icon->pIconBuffer == NULL)  // certaines applis comme xterm ne definissent pas d'icone, on en met une par defaut.
	{
		cd_debug ("%s (%ld) doesn't define any icon, we set the default one.\n", icon->cName, icon->Xid);
		gchar *cIconPath = cairo_dock_generate_file_path (CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
		if (cIconPath == NULL || ! g_file_test (cIconPath, G_FILE_TEST_EXISTS))
		{
			g_free (cIconPath);
			cIconPath = g_strdup (CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
		}
		icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
			iWidth,
			iHeight);
		g_free (cIconPath);
	}
}

static void _show_appli_for_drop (Icon *pIcon)
{
	cairo_dock_show_xwindow (pIcon->Xid);
}

Icon * cairo_dock_create_icon_from_xwindow (Window Xid, CairoDock *pDock)
{
	//\__________________ On cree l'icone.
	Window XParentWindow = 0;
	Icon *icon = cairo_dock_new_appli_icon (Xid, &XParentWindow);
	
	if (XParentWindow != 0 && (myTaskBar.bDemandsAttentionWithDialog || myTaskBar.cAnimationOnDemandsAttention))
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
	icon->load_image = _load_appli;
	icon->action_on_drag_hover = _show_appli_for_drop;
	icon->bHasIndicator = myTaskBar.bDrawIndicatorOnAppli;
	
	//\____________ On remplit ses buffers.
	#ifdef HAVE_XEXTEND
	if (myTaskBar.iMinimizedWindowRenderType == 1 && ! icon->bIsHidden)
	{
		//Display *display = gdk_x11_get_default_xdisplay ();
		icon->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, Xid);
		/*icon->iDamageHandle = XDamageCreate (s_XDisplay, Xid, XDamageReportNonEmpty);  // XDamageReportRawRectangles
		cd_debug ("backing pixmap : %d ; iDamageHandle : %d\n", icon->iBackingPixmap, icon->iDamageHandle);*/
	}
	#endif
	
	cairo_dock_load_icon_buffers (icon, CAIRO_CONTAINER (pDock));
	
	if (icon->bIsHidden && myTaskBar.iMinimizedWindowRenderType == 2)
	{
		cairo_dock_draw_hidden_appli_icon (icon, CAIRO_CONTAINER (pDock), FALSE);
	}
	
	//\____________ On enregistre l'appli et on commence a la surveiller.
	cairo_dock_register_appli (icon);

	return icon;
}
