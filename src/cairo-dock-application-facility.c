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
#include <cairo.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>

#include "cairo-dock-load.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dialog-factory.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-application-facility.h"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopGeometry g_desktopGeometry;


static void _cairo_dock_appli_demands_attention (Icon *icon, CairoDock *pDock, gboolean bForceDemand, Icon *pHiddenIcon)
{
	cd_debug ("%s (%s, force:%d)", __func__, icon->cName, bForceDemand);
	if (CAIRO_DOCK_IS_APPLET (icon))  // on considere qu'une applet prenant le controle d'une icone d'appli dispose de bien meilleurs moyens pour interagir avec l'appli que la barre des taches.
		return ;
	if (! pHiddenIcon)
		icon->bIsDemandingAttention = TRUE;
	//\____________________ On montre le dialogue.
	if (myTaskBar.bDemandsAttentionWithDialog)
	{
		CairoDialog *pDialog;
		if (pHiddenIcon == NULL)
		{
			pDialog = cairo_dock_show_temporary_dialog_with_icon (icon->cName, icon, CAIRO_CONTAINER (pDock), 1000*myTaskBar.iDialogDuration, "same icon");
		}
		else
		{
			pDialog = cairo_dock_show_temporary_dialog (pHiddenIcon->cName, icon, CAIRO_CONTAINER (pDock), 1000*myTaskBar.iDialogDuration); // mieux vaut montrer d'icone dans le dialogue que de montrer une icone qui n'a pas de rapport avec l'appli demandant l'attention.
			g_return_if_fail (pDialog != NULL);
			cairo_dock_set_new_dialog_icon_surface (pDialog, pHiddenIcon->pIconBuffer, pDialog->iIconSize);
		}
		if (pDialog && bForceDemand)
		{
			cd_debug ("force dialog on top");
			gtk_window_set_keep_above (GTK_WINDOW (pDialog->container.pWidget), TRUE);
			Window Xid = GDK_WINDOW_XID (pDialog->container.pWidget->window);
			cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_DOCK");  // pour passer devant les fenetres plein ecran; depend du WM.
		}
	}
	//\____________________ On montre l'icone avec une animation.
	if (myTaskBar.cAnimationOnDemandsAttention && ! pHiddenIcon)  // on ne l'anime pas si elle n'est pas dans un dock.
	{
		if (pDock->iRefCount == 0)
		{
			if (bForceDemand || cairo_dock_search_window_on_our_way (pDock, FALSE, TRUE) == NULL)
			{
				if (pDock->iSidPopDown != 0)
				{
					g_source_remove(pDock->iSidPopDown);
					pDock->iSidPopDown = 0;
				}
				cairo_dock_pop_up (pDock);
			}
			/*if (pDock->bAutoHide && bForceDemand)
			{
				g_print ("force dock to raise\n");
				cairo_dock_emit_enter_signal (pDock);
			}*/
		}
		else if (bForceDemand)
		{
			cd_debug ("force sub-dock to raise\n");
			CairoDock *pParentDock = NULL;
			Icon *pPointedIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
			if (pParentDock)
				cairo_dock_show_subdock (pPointedIcon, pParentDock);
		}
		cairo_dock_request_icon_animation (icon, pDock, myTaskBar.cAnimationOnDemandsAttention, 10000);
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));  // dans le au cas ou le dock ne serait pas encore visible, la fonction precedente n'a pas lance l'animation.
	}
}
void cairo_dock_appli_demands_attention (Icon *icon)
{
	cd_debug ("%s (%s / %s , %d)", __func__, icon->cName, icon->cLastAttentionDemand, icon->bIsDemandingAttention);
	if (icon->Xid == cairo_dock_get_current_active_window ())  // apparemment ce cas existe, et conduit a ne pas pouvoir stopper l'animation de demande d'attention facilement.
	{
		cd_message ("cette fenetre a deja le focus, elle ne peut demander l'attention en plus.");
		return ;
	}
	if (icon->bIsDemandingAttention &&
		/*cairo_dock_icon_has_dialog (icon) &&*/
		icon->cLastAttentionDemand && icon->cName && strcmp (icon->cLastAttentionDemand, icon->cName) == 0)  // le message n'a pas change entre les 2 demandes.
	{
		return ;
	}
	g_free (icon->cLastAttentionDemand);
	icon->cLastAttentionDemand = g_strdup (icon->cName);
	
	gboolean bForceDemand = (myTaskBar.cForceDemandsAttention && icon->cClass && g_strstr_len (myTaskBar.cForceDemandsAttention, -1, icon->cClass));
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (pParentDock == NULL)  // appli inhibee ou non affichee.
	{
		icon->bIsDemandingAttention = TRUE;  // on met a TRUE meme si ce n'est pas reellement elle qui va prendre la demande.
		Icon *pInhibitorIcon = cairo_dock_get_inhibator (icon, TRUE);  // on cherche son inhibiteur dans un dock.
		if (pInhibitorIcon != NULL)  // appli inhibee.
		{
			pParentDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
			if (pParentDock != NULL)
				_cairo_dock_appli_demands_attention (pInhibitorIcon, pParentDock, bForceDemand, NULL);
		}
		else if (bForceDemand)  // appli pas affichee, mais on veut tout de même etre notifie.
		{
			Icon *pOneIcon = cairo_dock_get_dialogless_icon ();
			if (pOneIcon != NULL)
				_cairo_dock_appli_demands_attention (pOneIcon, g_pMainDock, bForceDemand, icon);
		}
	}
	else  // appli dans un dock.
		_cairo_dock_appli_demands_attention (icon, pParentDock, bForceDemand, NULL);
}

static void _cairo_dock_appli_stops_demanding_attention (Icon *icon, CairoDock *pDock)
{
	if (CAIRO_DOCK_IS_APPLET (icon))
		return ;
	icon->bIsDemandingAttention = FALSE;
	if (myTaskBar.bDemandsAttentionWithDialog)
		cairo_dock_remove_dialog_if_any (icon);
	if (myTaskBar.cAnimationOnDemandsAttention)
	{
		cairo_dock_stop_icon_animation (icon);  // arrete l'animation precedemment lancee par la demande.
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));  // optimisation possible : ne redessiner que l'icone en tenant compte de la zone de sa derniere animation (pulse ou rebond).
	}
	if (! pDock->container.bInside)
	{
		//g_print ("pop down the dock\n");
		cairo_dock_pop_down (pDock);
		
		if (pDock->bAutoHide)
		{
			//g_print ("force dock to auto-hide\n");
			cairo_dock_emit_leave_signal (pDock);
		}
	}
}
void cairo_dock_appli_stops_demanding_attention (Icon *icon)
{
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (pParentDock == NULL)
	{
		icon->bIsDemandingAttention = FALSE;  // idem que plus haut.
		Icon *pInhibitorIcon = cairo_dock_get_inhibator (icon, TRUE);
		if (pInhibitorIcon != NULL)
		{
			pParentDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
			if (pParentDock != NULL)
				_cairo_dock_appli_stops_demanding_attention (pInhibitorIcon, pParentDock);
		}
	}
	else
		_cairo_dock_appli_stops_demanding_attention (icon, pParentDock);
}


void cairo_dock_animate_icon_on_active (Icon *icon, CairoDock *pParentDock)
{
	g_return_if_fail (pParentDock != NULL);
	if (! cairo_dock_icon_is_being_inserted_or_removed (icon))  // sinon on laisse l'animation actuelle.
	{
		if (myTaskBar.cAnimationOnActiveWindow)
		{
			if (cairo_dock_animation_will_be_visible (pParentDock) && icon->iAnimationState == CAIRO_DOCK_STATE_REST)
				cairo_dock_request_icon_animation (icon, pParentDock, myTaskBar.cAnimationOnActiveWindow, 1);
		}
		else
		{
			cairo_dock_redraw_icon (icon, CAIRO_CONTAINER (pParentDock));  // Si pas d'animation, on le fait pour redessiner l'indicateur.
		}
		if (pParentDock->iRefCount != 0)  // l'icone est dans un sous-dock, on veut que l'indicateur soit aussi dessine sur l'icone pointant sur ce sous-dock.
		{
			CairoDock *pMainDock = NULL;
			Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pParentDock, &pMainDock);
			if (pPointingIcon && pMainDock)
			{
				cairo_dock_redraw_icon (pPointingIcon, CAIRO_CONTAINER (pMainDock));  // on se contente de redessiner cette icone sans l'animer. Une facon comme une autre de differencier ces 2 cas.
			}
		}
	}
}



static inline gboolean _cairo_dock_window_hovers_dock (GtkAllocation *pWindowGeometry, CairoDock *pDock)
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
		
		if ((pWindowGeometry->x < iDockX + iDockWidth && pWindowGeometry->x + pWindowGeometry->width > iDockX) && (pWindowGeometry->y < iDockY + iDockHeight && pWindowGeometry->y + pWindowGeometry->height > iDockY))
		{
			return TRUE;
		}
	}
	else
	{
		cd_warning (" unknown window geometry");
	}
	return FALSE;
}
gboolean cairo_dock_appli_hovers_dock (Icon *pIcon, CairoDock *pDock)
{
	return _cairo_dock_window_hovers_dock (&pIcon->windowGeometry, pDock);
}




static CairoDock *_cairo_dock_set_parent_dock_name_for_appli (Icon *icon, CairoDock *pMainDock)
{
	cd_message ("%s (%s)", __func__, icon->cName);
	CairoDock *pParentDock = pMainDock;
	g_free (icon->cParentDockName);
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.bGroupAppliByClass && icon->cClass != NULL && ! cairo_dock_class_is_expanded (icon->cClass))
	{
		Icon *pSameClassIcon = cairo_dock_get_classmate (icon);  // un inhibiteur dans un dock OU une appli de meme classe dans le main dock.
		if (pSameClassIcon == NULL)  // aucun classmate => elle va dans le main dock.
		{
			cd_message ("  classe %s encore vide", icon->cClass);
			pParentDock = cairo_dock_search_dock_from_name (icon->cClass);
			if (pParentDock == NULL)
			{
				pParentDock = pMainDock;
				icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);
			}
			else
			{
				icon->cParentDockName = g_strdup (icon->cClass);
			}
		}
		else  // on la met dans le sous-dock de sa classe.
		{
			icon->cParentDockName = g_strdup (icon->cClass);

			//\____________ On cree ce sous-dock si necessaire.
			pParentDock = cairo_dock_search_dock_from_name (icon->cClass);
			if (pParentDock == NULL)  // alors il faut creer le sous-dock, qu'on associera soit a pSameClassIcon soit a un fake.
			{
				cd_message ("  creation du dock pour la classe %s", icon->cClass);
				pParentDock = cairo_dock_create_subdock_from_scratch (NULL, icon->cClass, pMainDock);
			}
			else
				cd_message ("  sous-dock de la classe %s existant", icon->cClass);
			
			if (CAIRO_DOCK_IS_LAUNCHER (pSameClassIcon) || CAIRO_DOCK_IS_APPLET (pSameClassIcon))  // c'est un inhibiteur.
			{
				if (pSameClassIcon->Xid != 0)  // actuellement l'inhibiteur inhibe 1 seule appli.
				{
					cd_debug ("actuellement l'inhibiteur inhibe 1 seule appli");
					Icon *pInhibatedIcon = cairo_dock_get_icon_with_Xid (pSameClassIcon->Xid);
					pSameClassIcon->Xid = 0;  // on lui laisse par contre l'indicateur.
					if (pSameClassIcon->pSubDock == NULL)
					{
						if (pSameClassIcon->cInitialName != NULL)
						{
							CairoDock *pSameClassDock = cairo_dock_search_dock_from_name (pSameClassIcon->cParentDockName);
							if (pSameClassDock != NULL)
							{
								cairo_dock_set_icon_name (pSameClassIcon->cInitialName, pSameClassIcon, CAIRO_CONTAINER (pSameClassDock));  // on lui remet son nom de lanceur.
							}
						}
						pSameClassIcon->pSubDock = pParentDock;
						CairoDock *pRootDock = cairo_dock_search_dock_from_name (pSameClassIcon->cParentDockName);
						if (pRootDock != NULL)
							cairo_dock_redraw_icon (pSameClassIcon, CAIRO_CONTAINER (pRootDock));  // on la redessine car elle prend l'indicateur de classe.
					}
					else if (pSameClassIcon->pSubDock != pParentDock)
						cd_warning ("this launcher (%s) already has a subdock, but it's not the class's subdock !", pSameClassIcon->cName);
					if (pInhibatedIcon != NULL)
					{
						cd_debug (" on insere %s dans le dock de la classe", pInhibatedIcon->cName);
						g_free (pInhibatedIcon->cParentDockName);
						pInhibatedIcon->cParentDockName = g_strdup (icon->cClass);
						cairo_dock_insert_icon_in_dock_full (pInhibatedIcon, pParentDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, ! CAIRO_DOCK_INSERT_SEPARATOR, NULL);
					}
				}
				else if (pSameClassIcon->pSubDock != pParentDock)
					cd_warning ("this inhibator doesn't hold the class dock !");
			}
			else  // c'est donc une appli du main dock.
			{
				//\______________ On cree une icone de paille.
				cd_debug (" on cree un fake...");
				CairoDock *pClassMateParentDock = cairo_dock_search_dock_from_name (pSameClassIcon->cParentDockName);  // c'est en fait le main dock.
				Icon *pFakeClassIcon = g_new0 (Icon, 1);
				pFakeClassIcon->cName = g_strdup (pSameClassIcon->cClass);
				pFakeClassIcon->cClass = g_strdup (pSameClassIcon->cClass);
				pFakeClassIcon->iType = pSameClassIcon->iType;
				pFakeClassIcon->fOrder = pSameClassIcon->fOrder;
				pFakeClassIcon->cParentDockName = g_strdup (pSameClassIcon->cParentDockName);
				pFakeClassIcon->fWidth = pSameClassIcon->fWidth / pClassMateParentDock->container.fRatio;
				pFakeClassIcon->fHeight = pSameClassIcon->fHeight / pClassMateParentDock->container.fRatio;
				pFakeClassIcon->fXMax = pSameClassIcon->fXMax;
				pFakeClassIcon->fXMin = pSameClassIcon->fXMin;
				pFakeClassIcon->fXAtRest = pSameClassIcon->fXAtRest;
				pFakeClassIcon->pSubDock = pParentDock;  // grace a cela ce sera un lanceur.
				
				//\______________ On la charge.
				cairo_dock_load_one_icon_from_scratch (pFakeClassIcon, CAIRO_CONTAINER (pClassMateParentDock));
				
				//\______________ On detache le classmate, on le place dans le sous-dock, et on lui substitue le faux.
				cd_debug (" on detache %s pour la passer dans le sous-dock de sa classe", pSameClassIcon->cName);
				cairo_dock_detach_icon_from_dock (pSameClassIcon, pClassMateParentDock, FALSE);
				g_free (pSameClassIcon->cParentDockName);
				pSameClassIcon->cParentDockName = g_strdup (pSameClassIcon->cClass);
				cairo_dock_insert_icon_in_dock_full (pSameClassIcon, pParentDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, ! CAIRO_DOCK_INSERT_SEPARATOR, NULL);
				
				cd_debug (" on lui substitue le fake");
				cairo_dock_insert_icon_in_dock_full (pFakeClassIcon, pClassMateParentDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, ! CAIRO_DOCK_INSERT_SEPARATOR, NULL);
				cairo_dock_calculate_dock_icons (pClassMateParentDock);
				cairo_dock_redraw_icon (pFakeClassIcon, CAIRO_CONTAINER (pClassMateParentDock));
			}
		}
	}
	else
		icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);

	return pParentDock;
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
	
	//\_________________ On gere le filtre 'applis minimisees seulement'.
	if (!icon->bIsHidden && myTaskBar.bHideVisibleApplis)
	{
		cairo_dock_reserve_one_icon_geometry_for_window_manager (&icon->Xid, icon, pMainDock);  // on reserve la position de l'icone dans le dock pour que l'effet de minimisation puisse viser la bonne position avant que l'icone ne soit effectivement dans le dock.
		return NULL;
	}
	
	//\_________________ On determine dans quel dock l'inserer.
	CairoDock *pParentDock = _cairo_dock_set_parent_dock_name_for_appli (icon, pMainDock);  // renseigne cParentDockName.
	g_return_val_if_fail (pParentDock != NULL, NULL);

	//\_________________ On l'insere dans son dock parent en animant ce dernier eventuellement.
	if ((myIcons.iSeparateIcons == 0 || myIcons.iSeparateIcons == 2) && pParentDock->iRefCount == 0)
	{
		cairo_dock_set_class_order (icon);
	}
	else
		icon->fOrder == CAIRO_DOCK_LAST_ORDER;  // en dernier.
	cairo_dock_insert_icon_in_dock (icon, pParentDock, bUpdateSize, bAnimate);
	cd_message (" insertion de %s complete (%.2f %.2fx%.2f) dans %s", icon->cName, icon->fInsertRemoveFactor, icon->fWidth, icon->fHeight, icon->cParentDockName);

	if (bAnimate && cairo_dock_animation_will_be_visible (pParentDock))
	{
		cairo_dock_launch_animation (CAIRO_CONTAINER (pParentDock));
	}
	else
	{
		icon->fInsertRemoveFactor = 0;
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

#define x_icon_geometry(icon, pDock) (pDock->container.iWindowPositionX + icon->fXAtRest + (pDock->container.iWidth - pDock->fFlatDockWidth) / 2 + (pDock->iOffsetForExtend * (pDock->fAlign - .5) * 2))
#define y_icon_geometry(icon, pDock) (pDock->container.iWindowPositionY + icon->fDrawY - icon->fHeight * myIcons.fAmplitude * pDock->fMagnitudeMax)
void  cairo_dock_set_one_icon_geometry_for_window_manager (Icon *icon, CairoDock *pDock)
{
	//g_print ("%s (%s)\n", __func__, icon->cName);
	int iX, iY, iWidth, iHeight;
	iX = x_icon_geometry (icon, pDock);
	iY = y_icon_geometry (icon, pDock);  // il faudrait un fYAtRest ...
	//g_print (" -> %d;%d (%.2f)\n", iX - pDock->container.iWindowPositionX, iY - pDock->container.iWindowPositionY, icon->fXAtRest);
	iWidth = icon->fWidth;
	iHeight = icon->fHeight * (1. + 2*myIcons.fAmplitude * pDock->fMagnitudeMax);  // on elargit en haut et en bas, pour gerer les cas ou l'icone grossirait vers le haut ou vers le bas.
	
	if (pDock->container.bIsHorizontal)
		cairo_dock_set_xicon_geometry (icon->Xid, iX, iY, iWidth, iHeight);
	else
		cairo_dock_set_xicon_geometry (icon->Xid, iY, iX, iHeight, iWidth);
}

void cairo_dock_reserve_one_icon_geometry_for_window_manager (Window *Xid, Icon *icon, CairoDock *pMainDock)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && icon->cParentDockName == NULL)
	{
		Icon *pInhibator = cairo_dock_get_inhibator (icon, FALSE);  // FALSE <=> meme en-dehors d'un dock
		if (pInhibator == NULL)  // cette icone n'est pas inhinbee, donc se minimisera dans le dock en une nouvelle icone.
		{
			int x, y;
			Icon *pClassmate = cairo_dock_get_classmate (icon);
			CairoDock *pClassmateDock = (pClassmate ? cairo_dock_search_dock_from_name (pClassmate->cParentDockName) : NULL);
			if (myTaskBar.bGroupAppliByClass && pClassmate != NULL && pClassmateDock != NULL)  // on va se grouper avec cette icone.
			{
				x = x_icon_geometry (pClassmate, pClassmateDock);
				if (cairo_dock_is_hidden (pMainDock))
				{
					y = (pClassmateDock->container.bDirectionUp ? 0 : g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
				}
				else
				{
					y = y_icon_geometry (pClassmate, pClassmateDock);
				}
			}
			else if (!myIcons.iSeparateIcons && pClassmate != NULL && pClassmateDock != NULL)  // on va se placer a cote.
			{
				x = x_icon_geometry (pClassmate, pClassmateDock) + pClassmate->fWidth/2;
				if (cairo_dock_is_hidden (pClassmateDock))
				{
					y = (pClassmateDock->container.bDirectionUp ? 0 : g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
				}
				else
				{
					y = y_icon_geometry (pClassmate, pClassmateDock);
				}
			}
			else  // on va se placer a la fin de la barre des taches.
			{
				Icon *pLastAppli = cairo_dock_get_last_icon_until_order (pMainDock->icons, CAIRO_DOCK_APPLI);
				if (pLastAppli != NULL)  // on se placera juste apres.
				{
					x = x_icon_geometry (pLastAppli, pMainDock) + pLastAppli->fWidth/2;
					if (cairo_dock_is_hidden (pMainDock))
					{
						y = (pMainDock->container.bDirectionUp ? 0 : g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
					}
					else
					{
						y = y_icon_geometry (pLastAppli, pMainDock);
					}
				}
				else  // aucune icone avant notre groupe, on sera insere en 1er.
				{
					x = pMainDock->container.iWindowPositionX + 0 + (pMainDock->container.iWidth - pMainDock->fFlatDockWidth) / 2;
					if (cairo_dock_is_hidden (pMainDock))
					{
						y = (pMainDock->container.bDirectionUp ? 0 : g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
					}
					else
					{
						y = pMainDock->container.iWindowPositionY;
					}
				}
			}
			//g_print (" - %s en (%d;%d)\n", icon->cName, x, y);
			if (pMainDock->container.bIsHorizontal)
				cairo_dock_set_xicon_geometry (icon->Xid, x, y, 1, 1);
			else
				cairo_dock_set_xicon_geometry (icon->Xid, y, x, 1, 1);
		}
		else
		{
			/// gerer les desklets...
			
		}
	}
}



gboolean cairo_dock_appli_is_on_desktop (Icon *pIcon, int iNumDesktop, int iNumViewportX, int iNumViewportY)
{
	// On calcule les coordonnees en repere absolu.
	int x = pIcon->windowGeometry.x;  // par rapport au viewport courant.
	x += g_desktopGeometry.iCurrentViewportX * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL];  // repere absolu
	if (x < 0)
		x += g_desktopGeometry.iNbViewportX * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
	int y = pIcon->windowGeometry.y;
	y += g_desktopGeometry.iCurrentViewportY * g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
	if (y < 0)
		y += g_desktopGeometry.iNbViewportY * g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
	int w = pIcon->windowGeometry.width, h = pIcon->windowGeometry.height;
	
	// test d'intersection avec le viewport donne.
	return ((pIcon->iNumDesktop == -1 || pIcon->iNumDesktop == iNumDesktop) &&
		x + w > iNumViewportX * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] &&
		x < (iNumViewportX + 1) * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] &&
		y + h > iNumViewportY * g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] &&
		y < (iNumViewportY + 1) * g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
}

gboolean cairo_dock_appli_is_on_current_desktop (Icon *pIcon)
{
	int iWindowDesktopNumber, iGlobalPositionX, iGlobalPositionY, iWidthExtent, iHeightExtent;  // coordonnees du coin haut gauche dans le referentiel du viewport actuel.
	iWindowDesktopNumber = pIcon->iNumDesktop;
	iGlobalPositionX = pIcon->windowGeometry.x;
	iGlobalPositionY = pIcon->windowGeometry.y;
	iWidthExtent = pIcon->windowGeometry.width;
	iHeightExtent = pIcon->windowGeometry.height;
	
	return ( (iWindowDesktopNumber == g_desktopGeometry.iCurrentDesktop || iWindowDesktopNumber == -1) &&
		iGlobalPositionX + iWidthExtent > 0 &&
		iGlobalPositionX < g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] &&
		iGlobalPositionY + iHeightExtent > 0 &&
		iGlobalPositionY < g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] );  // -1 <=> 0xFFFFFFFF en unsigned.
}

void cairo_dock_move_window_to_desktop (Icon *pIcon, int iNumDesktop, int iNumViewportX, int iNumViewportY)
{
	cairo_dock_move_xwindow_to_nth_desktop (pIcon->Xid,
		iNumDesktop,
		(iNumViewportX - g_desktopGeometry.iCurrentViewportX) * g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL],
		(iNumViewportY - g_desktopGeometry.iCurrentViewportY) * g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
}

void cairo_dock_move_window_to_current_desktop (Icon *pIcon)
{
	cairo_dock_move_xwindow_to_nth_desktop (pIcon->Xid,
		g_desktopGeometry.iCurrentDesktop,
		0,
		0);  // on ne veut pas decaler son viewport par rapport a nous.
}
