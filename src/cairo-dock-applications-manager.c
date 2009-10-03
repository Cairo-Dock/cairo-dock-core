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
#include "cairo-dock-dialogs.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-position.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-applications-manager.h"

#define CAIRO_DOCK_TASKBAR_CHECK_INTERVAL 200

extern CairoDock *g_pMainDock;

extern int g_iXScreenWidth[2], g_iXScreenHeight[2];

extern int g_iNbDesktops;
extern int g_iNbViewportX,g_iNbViewportY ;
//extern int g_iDamageEvent;

static GHashTable *s_hXWindowTable = NULL;  // table des fenetres X affichees dans le dock.
static Display *s_XDisplay = NULL;
static int s_iSidUpdateAppliList = 0;
static int s_iTime = 1;  // on peut aller jusqu'a 2^31, soit 17 ans a 4Hz.
static Window s_iCurrentActiveWindow = 0;
static int s_iCurrentDesktop = 0;
static int s_iCurrentViewportX = 0;
static int s_iCurrentViewportY = 0;
static Atom s_aNetClientList;
static Atom s_aNetActiveWindow;
static Atom s_aNetCurrentDesktop;
static Atom s_aNetDesktopViewport;
static Atom s_aNetDesktopGeometry;
static Atom s_aNetWmState;
static Atom s_aNetWmDesktop;
static Atom s_aRootMapID;
static Atom s_aNetNbDesktops;
static Atom s_aXKlavierState;


void cairo_dock_initialize_application_manager (Display *pDisplay)
{
	s_XDisplay = pDisplay;

	s_hXWindowTable = g_hash_table_new_full (g_int_hash,
		g_int_equal,
		g_free,
		NULL);
	
	s_aNetClientList		= XInternAtom (s_XDisplay, "_NET_CLIENT_LIST_STACKING", False);
	s_aNetActiveWindow		= XInternAtom (s_XDisplay, "_NET_ACTIVE_WINDOW", False);
	s_aNetCurrentDesktop	= XInternAtom (s_XDisplay, "_NET_CURRENT_DESKTOP", False);
	s_aNetDesktopViewport	= XInternAtom (s_XDisplay, "_NET_DESKTOP_VIEWPORT", False);
	s_aNetDesktopGeometry	= XInternAtom (s_XDisplay, "_NET_DESKTOP_GEOMETRY", False);
	s_aNetWmState			= XInternAtom (s_XDisplay, "_NET_WM_STATE", False);
	s_aNetWmDesktop			= XInternAtom (s_XDisplay, "_NET_WM_DESKTOP", False);
	s_aRootMapID			= XInternAtom (s_XDisplay, "_XROOTPMAP_ID", False);
	s_aNetNbDesktops		= XInternAtom (s_XDisplay, "_NET_NUMBER_OF_DESKTOPS", False);
	s_aXKlavierState		= XInternAtom (s_XDisplay, "XKLAVIER_STATE", False);
}

void cairo_dock_register_appli (Icon *icon)
{
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cd_debug ("%s (%ld ; %s)\n", __func__, icon->Xid, icon->cName);
		Window *pXid = g_new (Window, 1);
			*pXid = icon->Xid;
		g_hash_table_insert (s_hXWindowTable, pXid, icon);
		
		cairo_dock_add_appli_to_class (icon);
	}
}

void cairo_dock_blacklist_appli (Window Xid)
{
	if (Xid > 0)
	{
		cd_debug ("%s (%ld)\n", __func__, Xid);
		Window *pXid = g_new (Window, 1);
			*pXid = Xid;
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
		
		//cairo_dock_unregister_pid (icon);  // on n'efface pas sa classe ici car on peut en avoir besoin encore.
		
		if (icon->iBackingPixmap != 0)
		{
			XFreePixmap (s_XDisplay, icon->iBackingPixmap);
			icon->iBackingPixmap = 0;
		}
		
		cairo_dock_remove_appli_from_class (icon);
		cairo_dock_update_Xid_on_inhibators (icon->Xid, icon->cClass);
		
		icon->Xid = 0;  // hop, elle n'est plus une appli.
	}
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
		cairo_dock_detach_icon_from_dock (pIcon, pDock, myIcons.bUseSeparator);
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
					cairo_dock_detach_icon_from_dock (pFakeClassIcon, pFakeClassParentDock, myIcons.bUseSeparator);
					cairo_dock_free_icon (pFakeClassIcon);
					if (! pFakeClassParentDock->bIsMainDock)
						cairo_dock_update_dock_size (pFakeClassParentDock);
				}
				
				cairo_dock_destroy_dock (pDock, cParentDockName, NULL, NULL);
			}
			else
				cairo_dock_update_dock_size (pDock);
		}
		g_free (cParentDockName);
	}
	
	cairo_dock_free_icon_buffers (pIcon);  // on ne veut pas passer dans le 'unregister' ni la gestion de la classe.
	//cairo_dock_unregister_pid (pIcon);
	g_free (pIcon);
	return TRUE;
}
static void cairo_dock_reset_appli_table (void)
{
	g_hash_table_foreach_remove (s_hXWindowTable, (GHRFunc) _cairo_dock_reset_appli_table_iter, NULL);
	cairo_dock_update_dock_size (g_pMainDock);
}



static inline void _cairo_dock_retrieve_current_desktop_and_viewport (void)
{
	s_iCurrentDesktop = cairo_dock_get_current_desktop ();
	cairo_dock_get_current_viewport (&s_iCurrentViewportX, &s_iCurrentViewportY);
	s_iCurrentViewportX /= g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
	s_iCurrentViewportY /= g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
}

gboolean cairo_dock_appli_is_on_desktop (Icon *pIcon, int iNumDesktop, int iNumViewportX, int iNumViewportY)
{
	// On calcule les coordonnees en repere absolu.
	int x = pIcon->windowGeometry.x;  // par rapport au viewport courant.
	x += s_iCurrentViewportX * g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL];  // repere absolu
	if (x < 0)
		x += g_iNbViewportX * g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
	int y = pIcon->windowGeometry.y;
	y += s_iCurrentViewportY * g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
	if (y < 0)
		y += g_iNbViewportY * g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
	int w = pIcon->windowGeometry.width, h = pIcon->windowGeometry.height;
	
	// test d'intersection avec le viewport donne.
	return ((pIcon->iNumDesktop == -1 || pIcon->iNumDesktop == iNumDesktop) &&
		x + w > iNumViewportX * g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] &&
		x < (iNumViewportX + 1) * g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] &&
		y + h > iNumViewportY * g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] &&
		y < (iNumViewportY + 1) * g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
}

gboolean cairo_dock_appli_is_on_current_desktop (Icon *pIcon)
{
	int iWindowDesktopNumber, iGlobalPositionX, iGlobalPositionY, iWidthExtent, iHeightExtent;  // coordonnees du coin haut gauche dans le referentiel du viewport actuel.
	iWindowDesktopNumber = pIcon->iNumDesktop;
	iGlobalPositionX = pIcon->windowGeometry.x;
	iGlobalPositionY = pIcon->windowGeometry.y;
	iWidthExtent = pIcon->windowGeometry.width;
	iHeightExtent = pIcon->windowGeometry.height;

	return ( (iWindowDesktopNumber == s_iCurrentDesktop || iWindowDesktopNumber == -1) &&
		iGlobalPositionX + iWidthExtent > 0 &&
		iGlobalPositionX < g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] &&
		iGlobalPositionY + iHeightExtent > 0 &&
		iGlobalPositionY < g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] );  // -1 <=> 0xFFFFFFFF en unsigned.
}


static gboolean _cairo_dock_window_hovers_dock (GtkAllocation *pWindowGeometry, CairoDock *pDock)
{
	if (pWindowGeometry->width != 0 && pWindowGeometry->height != 0)
	{
		int iDockX, iDockY, iDockWidth, iDockHeight;
		if (pDock->container.bIsHorizontal)
		{
			iDockX = pDock->container.iWindowPositionX;
			iDockY = pDock->container.iWindowPositionY + (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iMinDockHeight : 0);
			iDockWidth = pDock->iMinDockWidth;
			iDockHeight = pDock->iMinDockHeight;
		}
		else
		{
			iDockX = pDock->container.iWindowPositionY + (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iMinDockHeight : 0);
			iDockY = pDock->container.iWindowPositionX;
			iDockWidth = pDock->iMinDockHeight;
			iDockHeight = pDock->iMinDockWidth;
		}
		
		cd_debug ("dock : (%d;%d) %dx%d", iDockX, iDockY, iDockWidth, iDockHeight);
		if ((pWindowGeometry->x < iDockX + iDockWidth && pWindowGeometry->x + pWindowGeometry->width > iDockX) && (pWindowGeometry->y < iDockY + iDockHeight && pWindowGeometry->y + pWindowGeometry->height > iDockY))
		{
			return TRUE;
		}
	}
	else
	{
		cd_warning (" on ne peut pas dire ou elle est sur l'ecran, on va supposer qu'elle recouvre le dock");
		return TRUE;
	}
	return FALSE;
}

static gboolean _cairo_dock_window_is_on_our_way (Window *Xid, Icon *icon, gboolean *data)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && cairo_dock_appli_is_on_current_desktop (icon))
	{
		if ((data[0] && icon->bIsMaximized && ! icon->bIsHidden) || (data[1] && icon->bIsFullScreen && ! icon->bIsHidden) || (!data[0] && ! data[1]))
		{
			cd_debug ("%s est genante (%d, %d) (%d;%d %dx%d)", icon->cName, icon->bIsMaximized, icon->bIsFullScreen, icon->windowGeometry.x, icon->windowGeometry.y, icon->windowGeometry.width, icon->windowGeometry.height);
			if (_cairo_dock_window_hovers_dock (&icon->windowGeometry, g_pMainDock))
			{
				cd_debug (" et en plus elle empiete sur notre dock");
				return TRUE;
			}
		}
	}
	return FALSE;
}
Icon * cairo_dock_search_window_on_our_way (gboolean bMaximizedWindow, gboolean bFullScreenWindow)
{
	cd_debug ("%s (%d, %d)", __func__, bMaximizedWindow, bFullScreenWindow);
	gboolean data[2] = {bMaximizedWindow, bFullScreenWindow};
	return g_hash_table_find (s_hXWindowTable, (GHRFunc) _cairo_dock_window_is_on_our_way, data);
}

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

gboolean cairo_dock_unstack_Xevents (CairoDock *pDock)
{
	static XEvent event;
	static gboolean bHackMeToo = FALSE;
	g_return_val_if_fail (pDock != NULL, FALSE);
	
	long event_mask = 0xFFFFFFFF;  // on les recupere tous, ca vide la pile au fur et a mesure plutot que tout a la fin.
	Window Xid;
	Window root = DefaultRootWindow (s_XDisplay);
	Icon *icon;
	if (bHackMeToo)
	{
		//g_print ("HACK ME\n");
		bHackMeToo = FALSE;
		if (pDock->container.bIsHorizontal)
			gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseX, &pDock->container.iMouseY, NULL);
		else
			gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseY, &pDock->container.iMouseX, NULL);
		cairo_dock_calculate_dock_icons (pDock);  // pour faire retrecir le dock si on n'est pas dedans, merci X de nous faire sortir du dock alors que la souris est toujours dedans :-/
	}
	
	while (XCheckMaskEvent (s_XDisplay, event_mask, &event))
	{
		icon = NULL;
		Xid = event.xany.window;
		//g_print ("  type : %d; atom : %s; window : %d\n", event.type, gdk_x11_get_xatom_name (event.xproperty.atom), Xid);
		//if (event.type == ClientMessage)
		//cd_message ("\n\n\n >>>>>>>>>>>< event.type : %d\n\n", event.type);
		if (Xid == root)
		{
			if (event.type == PropertyNotify)  // PropertyNotify sur root
			{
				if (event.xproperty.atom == s_aNetClientList)
				{
					s_iTime ++;
					cairo_dock_update_applis_list (pDock, s_iTime);
					cairo_dock_notify (CAIRO_DOCK_WINDOW_CONFIGURED, NULL);
				}
				else if (event.xproperty.atom == s_aNetActiveWindow)
				{
					Window XActiveWindow = cairo_dock_get_active_xwindow ();
					//g_print ("%d devient active (%d)\n", XActiveWindow, root);
					if (s_iCurrentActiveWindow != XActiveWindow)  // la fenetre courante a change.
					{
						icon = g_hash_table_lookup (s_hXWindowTable, &XActiveWindow);
						CairoDock *pParentDock = NULL;
						if (CAIRO_DOCK_IS_APPLI (icon))
						{
							cd_message ("%s devient active", icon->cName);
							pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
							if (pParentDock == NULL)  // elle est inhibee.
							{
								cairo_dock_update_activity_on_inhibators (icon->cClass, icon->Xid);
							}
							else
							{
								cairo_dock_animate_icon_on_active (icon, pParentDock);
							}
							if (icon->bIsDemandingAttention)  // on force ici, car il semble qu'on ne recoive pas toujours le retour a la normale.
								cairo_dock_appli_stops_demanding_attention (icon);
						}
						
						gboolean bHackMe = FALSE;
						Icon *pLastActiveIcon = g_hash_table_lookup (s_hXWindowTable, &s_iCurrentActiveWindow);
						if (CAIRO_DOCK_IS_APPLI (pLastActiveIcon))
						{
							CairoDock *pLastActiveParentDock = cairo_dock_search_dock_from_name (pLastActiveIcon->cParentDockName);
							if (pLastActiveParentDock != NULL)
							{
								if (! pLastActiveParentDock->bIsShrinkingDown)
									cairo_dock_redraw_icon (pLastActiveIcon, CAIRO_CONTAINER (pLastActiveParentDock));
							}
							else
							{
								cairo_dock_update_inactivity_on_inhibators (pLastActiveIcon->cClass, pLastActiveIcon->Xid);
							}
						}
						else
							bHackMe = TRUE;
						s_iCurrentActiveWindow = XActiveWindow;
						cairo_dock_notify (CAIRO_DOCK_WINDOW_ACTIVATED, &XActiveWindow);
						if (bHackMe)  // si on active une fenetre n'ayant pas de focus clavier, on n'aura pas d'evenement kbd_changed, pourtant en interne le clavier changera. du coup si apres on revient sur une fenetre qui a un focus clavier, il risque de ne pas y avoir de changement de clavier, et donc encore une fois pas d'evenement ! pour palier a ce, on considere que les fenetres avec focus clavier sont celles presentes en barre des taches. On decide de generer un evenement lorsqu'on revient sur une fenetre avec focus, a partir d'une fenetre sans focus (mettre a jour le clavier pour une fenetre sans focus n'a pas grand interet, autant le laisser inchange).
							cairo_dock_notify (CAIRO_DOCK_KBD_STATE_CHANGED, &XActiveWindow);
					}
				}
				else if (event.xproperty.atom == s_aNetCurrentDesktop || event.xproperty.atom == s_aNetDesktopViewport)
				{
					cd_message ("on change de bureau/viewport");
					
					_cairo_dock_retrieve_current_desktop_and_viewport ();
					
					// applis du bureau courant seulement.
					if (myTaskBar.bAppliOnCurrentDesktopOnly)
					{
						int data[2] = {s_iCurrentDesktop, GPOINTER_TO_INT (pDock)};
						g_hash_table_foreach (s_hXWindowTable, (GHFunc) _cairo_dock_hide_show_windows_on_other_desktops, pDock);
					}
					
					// auto-hide sur appli maximisee/plein-ecran
					if (myAccessibility.bAutoHideOnFullScreen || myAccessibility.bAutoHideOnMaximized)
					{
						if (cairo_dock_quick_hide_is_activated ())
						{
							if (cairo_dock_search_window_on_our_way (myAccessibility.bAutoHideOnMaximized, myAccessibility.bAutoHideOnFullScreen) == NULL)
							{
								cd_message (" => plus aucune fenetre genante");
								cairo_dock_deactivate_temporary_auto_hide ();
							}
						}
						else
						{
							if (cairo_dock_search_window_on_our_way (myAccessibility.bAutoHideOnMaximized, myAccessibility.bAutoHideOnFullScreen) != NULL)
							{
								cd_message (" => une fenetre est genante");
								cairo_dock_activate_temporary_auto_hide ();
							}
						}
					}
					
					// on propage la notification.
					cairo_dock_notify (CAIRO_DOCK_DESKTOP_CHANGED);  // , NULL
					
					// on gere le cas delicat de X qui nous fait sortir du dock.
					if (! pDock->bIsShrinkingDown && ! pDock->bIsGrowingUp)
					{
						if (pDock->container.bIsHorizontal)
							gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseX, &pDock->container.iMouseY, NULL);
						else
							gdk_window_get_pointer (pDock->container.pWidget->window, &pDock->container.iMouseY, &pDock->container.iMouseX, NULL);
						//g_print ("on met a jour de force\n");
						cairo_dock_calculate_dock_icons (pDock);  // pour faire retrecir le dock si on n'est pas dedans, merci X de nous faire sortir du dock alors que la souris est toujours dedans :-/
						if (pDock->container.bInside)
							bHackMeToo = TRUE;
						//g_print (">>> %d;%d, %dx%d\n", pDock->container.iMouseX, pDock->container.iMouseY,pDock->container.iWidth,  pDock->container.iHeight);
					}
				}
				else if (event.xproperty.atom == s_aNetNbDesktops)
				{
					cd_message ("changement du nombre de bureaux virtuels");
					
					g_iNbDesktops = cairo_dock_get_nb_desktops ();
					_cairo_dock_retrieve_current_desktop_and_viewport ();  // au cas ou on enleve le bureau courant.
					
					cairo_dock_notify (CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED);
				}
				else if (event.xproperty.atom == s_aNetDesktopGeometry)
				{
					cd_message ("geometrie du bureau alteree");
					
					cairo_dock_get_nb_viewports (&g_iNbViewportX, &g_iNbViewportY);
					
					if (cairo_dock_update_screen_geometry ())  // modification de la resolution.
					{
						cd_message ("resolution alteree");
						cairo_dock_reposition_root_docks (FALSE);  // main dock compris.
					}
					
					_cairo_dock_retrieve_current_desktop_and_viewport ();  // au cas ou on enleve le viewport courant.
					
					cairo_dock_notify (CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED);
				}
				else if (event.xproperty.atom == s_aRootMapID)
				{
					cd_message ("changement du fond d'ecran");
					if (mySystem.bUseFakeTransparency)
						cairo_dock_load_desktop_background_surface ();
					else
						cairo_dock_invalidate_desktop_bg_surface ();
					cairo_dock_notify (CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED);
				}
				else if (event.xproperty.atom == s_aXKlavierState)
				{
					cairo_dock_notify (CAIRO_DOCK_KBD_STATE_CHANGED, NULL);
				}
			}  // fin de PropertyNotify sur root.
		}
		else  // evenement sur une fenetre.
		{
			icon = g_hash_table_lookup (s_hXWindowTable, &Xid);
			if (! CAIRO_DOCK_IS_APPLI (icon))  // appli blacklistee
			{
				if (! cairo_dock_xwindow_skip_taskbar (Xid))
				{
					g_print ("Special case : this appli (%ld) should not be ignored any more!\n", Xid);
					//g_hash_table_remove (s_hXWindowTable, &Xid);
					//g_free (icon);
				}
				continue;
			}
			else if (icon->fPersonnalScale > 0)  // pour une icone en cours de supression, on ne fait rien.
			{
				continue;
			}
			if (event.type == PropertyNotify)  // PropertyNotify sur une fenetre
			{
				if (event.xproperty.atom == s_aNetWmState)  // changement d'etat (hidden, maximized, fullscreen, demands attention)
				{
					gboolean bIsFullScreen, bIsHidden, bIsMaximized, bDemandsAttention, bPrevHidden = icon->bIsHidden;
					if (! cairo_dock_xwindow_is_fullscreen_or_hidden_or_maximized (Xid, &bIsFullScreen, &bIsHidden, &bIsMaximized, &bDemandsAttention))
					{
						g_print ("Special case : this appli (%s, %ld) should be ignored from now !\n", icon->cName, Xid);
						CairoDock *pParentDock = cairo_dock_detach_appli (icon);
						if (pParentDock != NULL)
							gtk_widget_queue_draw (pParentDock->container.pWidget);
						cairo_dock_free_icon (icon);
						cairo_dock_blacklist_appli (Xid);
						continue;
					}
					cd_debug ("changement d'etat de %d => {%d ; %d ; %d ; %d}", Xid, bIsFullScreen, bIsHidden, bIsMaximized, bDemandsAttention);
					
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
					
					// auto-hide on maximised/fullscreen windows.
					if (myAccessibility.bAutoHideOnMaximized || myAccessibility.bAutoHideOnFullScreen)
					{
						if ((bIsMaximized != icon->bIsMaximized) ||
							((bIsMaximized || bIsFullScreen) && bIsHidden != icon->bIsHidden) ||
							(bIsFullScreen != icon->bIsFullScreen))  // changement dans l'etat.
						{
							icon->bIsMaximized = bIsMaximized;
							icon->bIsFullScreen = bIsFullScreen;
							icon->bIsHidden = bIsHidden;
							
							if ( ((bIsMaximized && ! bIsHidden && myAccessibility.bAutoHideOnMaximized) || (bIsFullScreen && ! bIsHidden && myAccessibility.bAutoHideOnFullScreen)) && ! cairo_dock_quick_hide_is_activated ())
							{
								cd_message (" => %s devient genante", CAIRO_DOCK_IS_APPLI (icon) ? icon->cName : "une fenetre");
								if (CAIRO_DOCK_IS_APPLI (icon) && _cairo_dock_window_hovers_dock (&icon->windowGeometry, g_pMainDock))
									cairo_dock_activate_temporary_auto_hide ();
							}
							else if ((! bIsMaximized || ! myAccessibility.bAutoHideOnMaximized || bIsHidden) && (! bIsFullScreen || ! myAccessibility.bAutoHideOnFullScreen || bIsHidden) && cairo_dock_quick_hide_is_activated ())
							{
								if (cairo_dock_search_window_on_our_way (myAccessibility.bAutoHideOnMaximized, myAccessibility.bAutoHideOnFullScreen) == NULL)
								{
									cd_message (" => plus aucune fenetre genante");
									cairo_dock_deactivate_temporary_auto_hide ();
								}
							}
						}
					}
					icon->bIsMaximized = bIsMaximized;  // on peut en avoir besoin plus tard.
					icon->bIsFullScreen = bIsFullScreen;
					
					// on gere le cachage/apparition de l'icone (transparence ou miniature, applis minimisees seulement).
					CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
					if (bIsHidden != bPrevHidden)
					{
						cd_message ("  changement de visibilite -> %d", bIsHidden);
						icon->bIsHidden = bIsHidden;
						
						// transparence sur les inhibiteurs.
						cairo_dock_update_visibility_on_inhibators (icon->cClass, icon->Xid, icon->bIsHidden);
						
						// miniature
						#ifdef HAVE_XEXTEND
						if (myTaskBar.bShowThumbnail && (pParentDock != NULL || myTaskBar.bHideVisibleApplis))  // on recupere la miniature ou au contraire on remet l'icone.
						{
							if (! icon->bIsHidden)  // fenetre mappee => BackingPixmap disponible.
							{
								if (icon->iBackingPixmap != 0)
									XFreePixmap (s_XDisplay, icon->iBackingPixmap);
								if (myTaskBar.bShowThumbnail)
									icon->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, Xid);
								else
									icon->iBackingPixmap = 0;
								cd_message ("new backing pixmap (bis) : %d", icon->iBackingPixmap);
							}
							// on redessine avec ou sans la miniature.
							cairo_dock_reload_one_icon_buffer_in_dock (icon, pParentDock ? pParentDock : g_pMainDock);
							if (pParentDock)
								cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pParentDock));
						}
						#endif
						
						// applis minimisees seulement
						if (myTaskBar.bHideVisibleApplis)  // on insere/detache l'icone selon la visibilite de la fenetre.
						{
							if (bIsHidden)  // se cache => on insere son icone.
							{
								cd_message (" => se cache");
								if (! myTaskBar.bAppliOnCurrentDesktopOnly || cairo_dock_appli_is_on_current_desktop (icon))
								{
									pParentDock = cairo_dock_insert_appli_in_dock (icon, pDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);  /// ! CAIRO_DOCK_ANIMATE_ICON
									if (pParentDock != NULL)
										gtk_widget_queue_draw (pParentDock->container.pWidget);
								}
							}
							else  // se montre => on detache l'icone.
							{
								cd_message (" => re-apparait");
								///pParentDock = cairo_dock_detach_appli (icon);
								///if (pParentDock != NULL)
								///	gtk_widget_queue_draw (pParentDock->container.pWidget);
								cairo_dock_trigger_icon_removal_from_dock (icon);
							}
						}
						else if (myTaskBar.fVisibleAppliAlpha != 0)  // transparence
						{
							icon->fAlpha = 1;  // on triche un peu.
							if (pParentDock != NULL)
								cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pParentDock));
						}
					}
				}
				else if (event.xproperty.atom == s_aNetWmDesktop)  // cela ne gere pas les changements de viewports, qui eux se font en changeant les coordonnees. Il faut donc recueillir les ConfigureNotify, qui donnent les redimensionnements et les deplacements.
				{
					cd_message ("changement de bureau pour %d", Xid);
					icon->iNumDesktop = cairo_dock_get_xwindow_desktop (Xid);
					
					// applis du bureau courant seulement.
					if (myTaskBar.bAppliOnCurrentDesktopOnly)
					{
						_cairo_dock_hide_show_windows_on_other_desktops (&Xid, icon, pDock);  // si elle vient sur notre bureau, elle n'est pas forcement sur le meme viewport, donc il faut le verifier.
					}
					
					// auto-hide sur appli maximisee/plein-ecran
					if ((myAccessibility.bAutoHideOnMaximized && icon->bIsMaximized) || (myAccessibility.bAutoHideOnFullScreen && icon->bIsFullScreen))  // cette fenetre peut provoquer l'auto-hide.
					{
						if (! cairo_dock_quick_hide_is_activated () && (icon->iNumDesktop == -1 || icon->iNumDesktop == s_iCurrentDesktop))  // l'appli arrive sur le bureau courant.
						{
							int data[3] = {s_iCurrentDesktop, myAccessibility.bAutoHideOnMaximized, myAccessibility.bAutoHideOnFullScreen};
							if (_cairo_dock_window_is_on_our_way (&Xid, icon, data))  // on s'assure qu'en plus d'etre sur le bureau courant, elle est aussi sur le viewport courant.
							{
								cd_message (" => cela nous gene");
								cairo_dock_activate_temporary_auto_hide ();
							}
						}
						else if (cairo_dock_quick_hide_is_activated () && (icon->iNumDesktop != -1 && icon->iNumDesktop != s_iCurrentDesktop))  // l'appli quitte sur le bureau courant.
						{
							if (cairo_dock_search_window_on_our_way (myAccessibility.bAutoHideOnMaximized, myAccessibility.bAutoHideOnFullScreen) == NULL)
							{
								cd_message (" => plus aucune fenetre genante");
								cairo_dock_deactivate_temporary_auto_hide ();
							}
						}
					}
				}
				else if (event.xproperty.atom == s_aXKlavierState)
				{
					cairo_dock_notify (CAIRO_DOCK_KBD_STATE_CHANGED, &Xid);
				}
				else
				{
					CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
					if (pParentDock == NULL)
						pParentDock = pDock;
					cairo_dock_Xproperty_changed (icon, event.xproperty.atom, event.xproperty.state, pParentDock);
				}
			}
			else if (event.type == ConfigureNotify)  // ConfigureNotify sur une fenetre.
			{
				//g_print ("  type : %d; (%d;%d) %dx%d window : %d\n", event.xconfigure.type, event.xconfigure.x, event.xconfigure.y, event.xconfigure.width, event.xconfigure.height, Xid);
				// On met a jour la taille du backing pixmap.
				#ifdef HAVE_XEXTEND
				if (event.xconfigure.width != icon->windowGeometry.width || event.xconfigure.height != icon->windowGeometry.height)
				{
					if (icon->iBackingPixmap != 0)
						XFreePixmap (s_XDisplay, icon->iBackingPixmap);
					if (myTaskBar.bShowThumbnail)
						icon->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, Xid);
					cd_message ("new backing pixmap : %d", icon->iBackingPixmap);
				}
				#endif
				memcpy (&icon->windowGeometry, &event.xconfigure.x, sizeof (GtkAllocation));
				
				// on regarde si l'appli a change de viewport.
				if (event.xconfigure.x + event.xconfigure.width <= 0 || event.xconfigure.x >= g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] || event.xconfigure.y + event.xconfigure.height <= 0 || event.xconfigure.y >= g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL])  // en fait il faudrait faire ca modulo le nombre de viewports * la largeur d'un bureau, car avec une fenetre a droite, elle peut revenir sur le bureau par la gauche si elle est tres large...
				{
					// applis du bureau courant seulement.
					if (myTaskBar.bAppliOnCurrentDesktopOnly && icon->cParentDockName != NULL)
					{
						CairoDock *pParentDock = cairo_dock_detach_appli (icon);
						if (pParentDock)
							gtk_widget_queue_draw (pParentDock->container.pWidget);
					}
					
					// auto-hide sur appli maximisee/plein-ecran
					if ((myAccessibility.bAutoHideOnMaximized && icon->bIsMaximized) || (myAccessibility.bAutoHideOnFullScreen && icon->bIsFullScreen))  // cette fenetre peut provoquer l'auto-hide.
					{
						if (cairo_dock_quick_hide_is_activated ())
						{
							if (cairo_dock_search_window_on_our_way (myAccessibility.bAutoHideOnMaximized, myAccessibility.bAutoHideOnFullScreen) == NULL)
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
						cd_message ("cette fenetre est sur le bureau courant (%d;%d)", event.xconfigure.x, event.xconfigure.y);
						gboolean bInsideDock = (icon->cParentDockName != NULL);  // jamais verifie mais ca devrait etre bon.
						if (! bInsideDock)
							cairo_dock_insert_appli_in_dock (icon, pDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
					}
					
					// auto-hide sur appli maximisee/plein-ecran
					if ((myAccessibility.bAutoHideOnMaximized && icon->bIsMaximized) || (myAccessibility.bAutoHideOnFullScreen && icon->bIsFullScreen))  // cette fenetre peut provoquer l'auto-hide.
					{
						if (! cairo_dock_quick_hide_is_activated ())
						{
							cd_message (" sur le viewport courant => cela nous gene");
							cairo_dock_activate_temporary_auto_hide ();
						}
					}
				}
				
				cairo_dock_notify (CAIRO_DOCK_WINDOW_CONFIGURED, &event.xconfigure);
			}
			/*else if (event.type == g_iDamageEvent + XDamageNotify)
			{
				XDamageNotifyEvent *e = (XDamageNotifyEvent *) &event;
				g_print ("window %s has been damaged (%d;%d %dx%d)\n", e->drawable, e->area.x, e->area.y, e->area.width, e->area.height);
				// e->drawable is the window ID of the damaged window
				// e->geometry is the geometry of the damaged window	
				// e->area     is the bounding rect for the damaged area	
				// e->damage   is the damage handle returned by XDamageCreate()
		
				// Subtract all the damage, repairing the window.
				XDamageSubtract (s_XDisplay, e->damage, None, None);
			}
			else
				g_print ("  type : %d (%d); window : %d\n", event.type, XDamageNotify, Xid);*/
		}  // fin d'evenement sur une fenetre.
	}
	if (XEventsQueued (s_XDisplay, QueuedAlready) != 0)
		XSync (s_XDisplay, True);  // True <=> discard.
	//g_print ("XEventsQueued : %d\n", XEventsQueued (s_XDisplay, QueuedAfterFlush));  // QueuedAlready, QueuedAfterReading, QueuedAfterFlush
	
	return TRUE;
}


static gboolean _cairo_dock_remove_old_applis (Window *Xid, Icon *icon, gpointer iTimePtr)
{
	if (icon == NULL)
		return FALSE;
	gint iTime = GPOINTER_TO_INT (iTimePtr);
	
	//g_print ("%s (%s(%ld) %d / %d)\n", __func__, icon->cName, icon->Xid, icon->iLastCheckTime, iTime);
	if (icon->iLastCheckTime >= 0 && icon->iLastCheckTime < iTime && icon->fPersonnalScale <= 0)
	{
		cd_message ("cette fenetre (%ld(%ld), %s) est trop vieille (%d / %d)", *Xid, icon->Xid, icon->cName, icon->iLastCheckTime, iTime);
		if (CAIRO_DOCK_IS_APPLI (icon))
		{
			if (cairo_dock_quick_hide_is_activated () && ((icon->bIsFullScreen && myAccessibility.bAutoHideOnFullScreen) || (icon->bIsMaximized && myAccessibility.bAutoHideOnMaximized)))  // cette fenetre peut avoir gener.
			{
				if (cairo_dock_search_window_on_our_way (myAccessibility.bAutoHideOnMaximized, myAccessibility.bAutoHideOnFullScreen) == NULL)  // on regarde si une autre gene encore.
				{
					cd_message (" => plus aucune fenetre genante");
					cairo_dock_deactivate_temporary_auto_hide ();
				}
			}
			
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
		}
		else
		{
			g_free (icon);
		}
		return TRUE;
	}
	return FALSE;
}
void cairo_dock_update_applis_list (CairoDock *pDock, gint iTime)
{
	gulong i, iNbWindows = 0;
	Window *pXWindowsList = cairo_dock_get_windows_list (&iNbWindows);
	
	Window Xid;
	Icon *icon;
	int iStackOrder = 0;
	gpointer pOriginalXid;
	gboolean bAppliAlreadyRegistered;
	gboolean bUpdateMainDockSize = FALSE;
	CairoDock *pParentDock;
	cairo_t *pCairoContext = NULL;
	
	for (i = 0; i < iNbWindows; i ++)
	{
		Xid = pXWindowsList[i];
		
		bAppliAlreadyRegistered = g_hash_table_lookup_extended (s_hXWindowTable, &Xid, &pOriginalXid, (gpointer *) &icon);
		if (! bAppliAlreadyRegistered)
		{
			cd_message (" cette fenetre (%ld) de la pile n'est pas dans la liste", Xid);
			if (pCairoContext == NULL)
				pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
			if (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS)
				icon = cairo_dock_create_icon_from_xwindow (pCairoContext, Xid, pDock);
			else
				cd_warning ("couldn't create a cairo context => this window (%ld) will not have an icon", Xid);
			if (icon != NULL)
			{
				icon->iLastCheckTime = iTime;
				icon->iStackOrder = iStackOrder ++;
				if (/*(icon->bIsHidden || ! myTaskBar.bHideVisibleApplis) && */(! myTaskBar.bAppliOnCurrentDesktopOnly || cairo_dock_xwindow_is_on_current_desktop (Xid)))
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
				if ((myAccessibility.bAutoHideOnMaximized && icon->bIsMaximized) || (myAccessibility.bAutoHideOnFullScreen && icon->bIsFullScreen))
				{
					if (! cairo_dock_quick_hide_is_activated ())
					{
						if (cairo_dock_xwindow_is_on_this_desktop (Xid, s_iCurrentDesktop) && _cairo_dock_window_hovers_dock (&icon->windowGeometry, pDock))
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
			icon->iLastCheckTime = iTime;
			if (CAIRO_DOCK_IS_APPLI (icon))
				icon->iStackOrder = iStackOrder ++;
		}
	}
	if (pCairoContext != NULL)
		cairo_destroy (pCairoContext);
	
	g_hash_table_foreach_remove (s_hXWindowTable, (GHRFunc) _cairo_dock_remove_old_applis, GINT_TO_POINTER (iTime));
	
	if (bUpdateMainDockSize)
		cairo_dock_update_dock_size (pDock);

	XFree (pXWindowsList);
}


void cairo_dock_start_application_manager (CairoDock *pDock)
{
	g_return_if_fail (s_iSidUpdateAppliList == 0);
	
	cairo_dock_set_overwrite_exceptions (myTaskBar.cOverwriteException);
	cairo_dock_set_group_exceptions (myTaskBar.cGroupException);
	
	//\__________________ On recupere l'ensemble des fenetres presentes.
	Window root = DefaultRootWindow (s_XDisplay);
	cairo_dock_set_xwindow_mask (root, PropertyChangeMask /*| StructureNotifyMask | SubstructureNotifyMask | ResizeRedirectMask | SubstructureRedirectMask*/);

	gulong i, iNbWindows = 0;
	Window *pXWindowsList = cairo_dock_get_windows_list (&iNbWindows);

	//\__________________ On recupere le bureau courant.
	_cairo_dock_retrieve_current_desktop_and_viewport ();
	
	//\__________________ On cree les icones de toutes ces applis.
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	g_return_if_fail (cairo_status (pCairoContext) == CAIRO_STATUS_SUCCESS);
	
	CairoDock *pParentDock;
	gboolean bUpdateMainDockSize = FALSE;
	int iStackOrder = 0;
	Window Xid;
	Icon *pIcon;
	for (i = 0; i < iNbWindows; i ++)
	{
		Xid = pXWindowsList[i];
		pIcon = cairo_dock_create_icon_from_xwindow (pCairoContext, Xid, pDock);
		
		if (pIcon != NULL)
		{
			pIcon->iStackOrder = iStackOrder ++;
			pIcon->iLastCheckTime = s_iTime;
			if (/*(pIcon->bIsHidden || ! myTaskBar.bHideVisibleApplis) && */(! myTaskBar.bAppliOnCurrentDesktopOnly || cairo_dock_appli_is_on_current_desktop (pIcon)))
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
			if ((myAccessibility.bAutoHideOnMaximized && pIcon->bIsMaximized) || (myAccessibility.bAutoHideOnFullScreen && pIcon->bIsFullScreen))
			{
				if (! cairo_dock_quick_hide_is_activated () && cairo_dock_appli_is_on_current_desktop (pIcon))
				{
					if (_cairo_dock_window_hovers_dock (&pIcon->windowGeometry, pDock))
					{
						cd_message (" elle empiete sur notre dock");
						cairo_dock_activate_temporary_auto_hide ();
					}
				}
			}
		}
		else
			cairo_dock_blacklist_appli (Xid);
	}
	cairo_destroy (pCairoContext);
	if (pXWindowsList != NULL)
		XFree (pXWindowsList);
	
	if (bUpdateMainDockSize)
		cairo_dock_update_dock_size (pDock);
	
	//\__________________ On lance le gestionnaire d'evenements X.
	s_iSidUpdateAppliList = g_timeout_add (CAIRO_DOCK_TASKBAR_CHECK_INTERVAL, (GSourceFunc) cairo_dock_unstack_Xevents, (gpointer) pDock);  // un g_idle_add () consomme 90% de CPU ! :-/
	
	if (s_iCurrentActiveWindow == 0)
		s_iCurrentActiveWindow = cairo_dock_get_active_xwindow ();
}

void cairo_dock_pause_application_manager (void)
{
	if (s_iSidUpdateAppliList != 0)
	{
		Window root = DefaultRootWindow (s_XDisplay);
		cairo_dock_set_xwindow_mask (root, 0);
		
		XSync (s_XDisplay, True);  // True <=> discard.
		
		g_source_remove (s_iSidUpdateAppliList);
		s_iSidUpdateAppliList = 0;
	}
}

void cairo_dock_stop_application_manager (void)
{
	cairo_dock_pause_application_manager ();
	
	cairo_dock_remove_all_applis_from_class_table ();  // enleve aussi les indicateurs.
	
	cairo_dock_reset_appli_table ();  // libere les icones des applis.
}

gboolean cairo_dock_application_manager_is_running (void)
{
	return (s_iSidUpdateAppliList != 0);
}


void cairo_dock_get_current_desktop_and_viewport (int *iCurrentDesktop, int *iCurrentViewportX, int *iCurrentViewportY)
{
	*iCurrentDesktop = s_iCurrentDesktop;
	*iCurrentViewportX = s_iCurrentViewportX;
	*iCurrentViewportY = s_iCurrentViewportY;
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
	if (! CAIRO_DOCK_IS_APPLI (icon) || icon->fPersonnalScale > 0)
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



CairoDock *cairo_dock_insert_appli_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bUpdateSize, gboolean bAnimate)
{
	cd_message ("%s (%s, %d)", __func__, icon->cName, icon->Xid);
	
	//\_________________ On gere ses eventuels inhibiteurs.
	if (myTaskBar.bMixLauncherAppli && cairo_dock_prevent_inhibated_class (icon))
	{
		cd_message (" -> se fait inhiber");
		return NULL;
	}
	
	//\_________________ On gere les filtres.
	if (!icon->bIsHidden && myTaskBar.bHideVisibleApplis)
		return NULL;
	
	//\_________________ On determine dans quel dock l'inserer.
	CairoDock *pParentDock = cairo_dock_manage_appli_class (icon, pMainDock);  // renseigne cParentDockName.
	g_return_val_if_fail (pParentDock != NULL, NULL);

	//\_________________ On l'insere dans son dock parent en animant ce dernier eventuellement.
	if (myIcons.bMixApplisAndLaunchers && pParentDock->iRefCount == 0)
	{
		cairo_dock_set_class_order (icon);
	}
	else
		icon->fOrder == CAIRO_DOCK_LAST_ORDER;  // en dernier.
	cairo_dock_insert_icon_in_dock (icon, pParentDock, bUpdateSize, bAnimate);
	cd_message (" insertion de %s complete (%.2f %.2fx%.2f) dans %s", icon->cName, icon->fPersonnalScale, icon->fWidth, icon->fHeight, icon->cParentDockName);

	if (bAnimate && cairo_dock_animation_will_be_visible (pParentDock))
	{
		cairo_dock_launch_animation (CAIRO_CONTAINER (pParentDock));
	}
	else
	{
		icon->fPersonnalScale = 0;
		icon->fScale = 1.;
	}

	return pParentDock;
}

CairoDock * cairo_dock_detach_appli (Icon *pIcon)
{
	cd_debug ("%s (%s)", __func__, pIcon->cName);
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pParentDock == NULL)
		return NULL;
	
	cairo_dock_detach_icon_from_dock (pIcon, pParentDock, TRUE);
	
	if (pIcon->cClass != NULL && pParentDock == cairo_dock_search_dock_from_name (pIcon->cClass))
	{
		gboolean bEmptyClassSubDock = cairo_dock_check_class_subdock_is_empty (pParentDock, pIcon->cClass);
		if (bEmptyClassSubDock)
			return NULL;
	}
	cairo_dock_update_dock_size (pParentDock);
	return pParentDock;
}

void cairo_dock_animate_icon_on_active (Icon *icon, CairoDock *pParentDock)
{
	g_return_if_fail (pParentDock != NULL);
	if (icon->fPersonnalScale == 0)  // sinon on laisse l'animation actuelle.
	{
		if (myTaskBar.cAnimationOnActiveWindow)
		{
			if (cairo_dock_animation_will_be_visible (pParentDock) && icon->iAnimationState == CAIRO_DOCK_STATE_REST)
				cairo_dock_request_icon_animation (icon, pParentDock, myTaskBar.cAnimationOnActiveWindow, 1);
		}
		else if (! pParentDock->bIsShrinkingDown)
		{
			cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pParentDock));
		}
	}
}

void  cairo_dock_set_one_icon_geometry_for_window_manager (Icon *icon, CairoDock *pDock)
{
	//g_print ("%s (%s)\n", __func__, icon->cName);
	int iX, iY, iWidth, iHeight;
	iX = pDock->container.iWindowPositionX + icon->fXAtRest + (pDock->container.iWidth - pDock->fFlatDockWidth) / 2;
	iY = pDock->container.iWindowPositionY + icon->fDrawY - icon->fHeight * myIcons.fAmplitude * pDock->fMagnitudeMax;  // il faudrait un fYAtRest ...
	iWidth = icon->fWidth;
	iHeight = icon->fHeight * (1. + 2*myIcons.fAmplitude * pDock->fMagnitudeMax);  // on elargit en haut et en bas, pour gerer les cas ou l'icone grossirait vers le haut ou vers le bas.
	
	if (pDock->container.bIsHorizontal)
		cairo_dock_set_xicon_geometry (icon->Xid, iX, iY, iWidth, iHeight);
	else
		cairo_dock_set_xicon_geometry (icon->Xid, iY, iX, iHeight, iWidth);
}

void cairo_dock_set_icons_geometry_for_window_manager (CairoDock *pDock)
{
	if (s_iSidUpdateAppliList <= 0)
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
}
