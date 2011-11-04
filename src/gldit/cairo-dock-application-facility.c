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

#include "cairo-dock-icon-facility.h"  // cairo_dock_set_icon_name
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-X-utilities.h"  // cairo_dock_set_xicon_geometry
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-X-manager.h"
#include "cairo-dock-indicator-manager.h"  // myIndicatorsParam.bUseClassIndic
#include "cairo-dock-application-facility.h"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern CairoDockHidingEffect *g_pHidingBackend;  // cairo_dock_is_hidden

static void _cairo_dock_appli_demands_attention (Icon *icon, CairoDock *pDock, gboolean bForceDemand, Icon *pHiddenIcon)
{
	cd_debug ("%s (%s, force:%d)", __func__, icon->cName, bForceDemand);
	if (CAIRO_DOCK_IS_APPLET (icon))  // on considere qu'une applet prenant le controle d'une icone d'appli dispose de bien meilleurs moyens pour interagir avec l'appli que la barre des taches.
		return ;
	if (! pHiddenIcon)
		icon->bIsDemandingAttention = TRUE;
	//\____________________ On montre le dialogue.
	if (myTaskbarParam.bDemandsAttentionWithDialog)
	{
		CairoDialog *pDialog;
		if (pHiddenIcon == NULL)
		{
			pDialog = cairo_dock_show_temporary_dialog_with_icon (icon->cName, icon, CAIRO_CONTAINER (pDock), 1000*myTaskbarParam.iDialogDuration, "same icon");
		}
		else
		{
			pDialog = cairo_dock_show_temporary_dialog (pHiddenIcon->cName, icon, CAIRO_CONTAINER (pDock), 1000*myTaskbarParam.iDialogDuration); // mieux vaut montrer pas d'icone dans le dialogue que de montrer une icone qui n'a pas de rapport avec l'appli demandant l'attention.
			g_return_if_fail (pDialog != NULL);
			cairo_dock_set_new_dialog_icon_surface (pDialog, pHiddenIcon->pIconBuffer, pDialog->iIconSize);
		}
		if (pDialog && bForceDemand)
		{
			cd_debug ("force dock and dialog on top");
			if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && pDock->bIsBelow)
				cairo_dock_pop_up (pDock);
			gtk_window_set_keep_above (GTK_WINDOW (pDialog->container.pWidget), TRUE);
			Window Xid = GDK_WINDOW_XID (pDialog->container.pWidget->window);
			cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_DOCK");  // pour passer devant les fenetres plein ecran; depend du WM.
		}
	}
	//\____________________ On montre l'icone avec une animation.
	if (myTaskbarParam.cAnimationOnDemandsAttention && ! pHiddenIcon)  // on ne l'anime pas si elle n'est pas dans un dock.
	{
		if (pDock->iRefCount == 0)
		{
			if (bForceDemand/** && cairo_dock_search_window_covering_dock (pDock, FALSE, TRUE) == NULL*/)
			{
				if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && pDock->bIsBelow)
					cairo_dock_pop_up (pDock);
			}
		}
		/**else if (bForceDemand)
		{
			cd_debug ("force sub-dock to raise\n");
			CairoDock *pParentDock = NULL;
			Icon *pPointedIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
			if (pParentDock)
				cairo_dock_show_subdock (pPointedIcon, pParentDock);
		}*/
		cairo_dock_request_icon_animation (icon, CAIRO_CONTAINER (pDock), myTaskbarParam.cAnimationOnDemandsAttention, 10000);  // animation de 2-3 heures.
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
	
	gboolean bForceDemand = (myTaskbarParam.cForceDemandsAttention && icon->cClass && g_strstr_len (myTaskbarParam.cForceDemandsAttention, -1, icon->cClass));
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (pParentDock == NULL)  // appli inhibee ou non affichee.
	{
		icon->bIsDemandingAttention = TRUE;  // on met a TRUE meme si ce n'est pas reellement elle qui va prendre la demande.
		Icon *pInhibitorIcon = cairo_dock_get_inhibitor (icon, TRUE);  // on cherche son inhibiteur dans un dock.
		if (pInhibitorIcon != NULL)  // appli inhibee.
		{
			pParentDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
			if (pParentDock != NULL)
				_cairo_dock_appli_demands_attention (pInhibitorIcon, pParentDock, bForceDemand, NULL);
		}
		else if (bForceDemand)  // appli pas affichee, mais on veut tout de meme etre notifie.
		{
			Icon *pOneIcon = cairo_dock_get_dialogless_icon ();  // on prend une icone dans le main dock.
			if (pOneIcon != NULL)
				_cairo_dock_appli_demands_attention (pOneIcon, g_pMainDock, bForceDemand, icon);
		}
	}
	else  // appli dans un dock.
		_cairo_dock_appli_demands_attention (icon, pParentDock, bForceDemand, NULL);
}

static void _cairo_dock_appli_stops_demanding_attention (Icon *icon, CairoDock *pDock)
{
	if (CAIRO_DOCK_IS_APPLET (icon))  // cf remarque plus haut.
		return ;
	if (myTaskbarParam.bDemandsAttentionWithDialog)
		cairo_dock_remove_dialog_if_any (icon);
	if (myTaskbarParam.cAnimationOnDemandsAttention)
	{
		cairo_dock_stop_icon_animation (icon);  // arrete l'animation precedemment lancee par la demande.
		gtk_widget_queue_draw (pDock->container.pWidget);  // optimisation possible : ne redessiner que l'icone en tenant compte de la zone de sa derniere animation (pulse ou rebond).
	}
	icon->bIsDemandingAttention = FALSE;
	if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && ! pDock->bIsBelow && ! pDock->container.bInside)
		cairo_dock_pop_down (pDock);
}
void cairo_dock_appli_stops_demanding_attention (Icon *icon)
{
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (pParentDock == NULL)
	{
		icon->bIsDemandingAttention = FALSE;  // idem que plus haut.
		Icon *pInhibitorIcon = cairo_dock_get_inhibitor (icon, TRUE);
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
		if (myTaskbarParam.cAnimationOnActiveWindow)
		{
			if (cairo_dock_animation_will_be_visible (pParentDock) && icon->iAnimationState == CAIRO_DOCK_STATE_REST)
				cairo_dock_request_icon_animation (icon, CAIRO_CONTAINER (pParentDock), myTaskbarParam.cAnimationOnActiveWindow, 1);
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



gboolean cairo_dock_appli_covers_dock (Icon *pIcon, CairoDock *pDock)
{
	GtkAllocation *pWindowGeometry = &pIcon->windowGeometry;
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
		
		if (! pIcon->bIsHidden && pWindowGeometry->x <= iDockX && pWindowGeometry->x + pWindowGeometry->width >= iDockX + iDockWidth && pWindowGeometry->y <= iDockY && pWindowGeometry->y + pWindowGeometry->height >= iDockY + iDockHeight)
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

gboolean cairo_dock_appli_overlaps_dock (Icon *pIcon, CairoDock *pDock)
{
	GtkAllocation *pWindowGeometry = &pIcon->windowGeometry;
	if (pWindowGeometry->width != 0 && pWindowGeometry->height != 0)
	{
		int iDockX, iDockY, iDockWidth, iDockHeight;
		if (pDock->container.bIsHorizontal)
		{
			iDockWidth = pDock->iMinDockWidth;
			iDockHeight = pDock->iMinDockHeight;
			iDockX = pDock->container.iWindowPositionX + (pDock->container.iWidth - iDockWidth)/2;
			iDockY = pDock->container.iWindowPositionY + (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iMinDockHeight : 0);
		}
		else
		{
			iDockWidth = pDock->iMinDockHeight;
			iDockHeight = pDock->iMinDockWidth;
			iDockX = pDock->container.iWindowPositionY + (pDock->container.bDirectionUp ? pDock->container.iHeight - pDock->iMinDockHeight : 0);
			iDockY = pDock->container.iWindowPositionX + (pDock->container.iWidth - iDockHeight)/2;
		}
		
		if (! pIcon->bIsHidden && pWindowGeometry->x < iDockX + iDockWidth && pWindowGeometry->x + pWindowGeometry->width > iDockX && pWindowGeometry->y < iDockY + iDockHeight && pWindowGeometry->y + pWindowGeometry->height > iDockY)
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


static void _load_class_icon (Icon *icon)
{
	int iWidth = icon->iImageWidth;
	int iHeight = icon->iImageHeight;
	if (icon->pSubDock != NULL && !myIndicatorsParam.bUseClassIndic)  // icone de sous-dock avec un rendu specifique, on le redessinera lorsque les icones du sous-dock auront ete chargees.
	{
		icon->pIconBuffer = cairo_dock_create_blank_surface (iWidth, iHeight);
	}
	else
	{
		//g_print ("%s (%dx%d)\n", __func__, iWidth, iHeight);
		icon->pIconBuffer = cairo_dock_create_surface_from_class (icon->cClass,
			iWidth,
			iHeight);
		if (icon->pIconBuffer == NULL)  // aucun inhibiteur ou aucune image correspondant a cette classe, on cherche a copier une des icones d'appli de cette classe.
		{
			const GList *pApplis = cairo_dock_list_existing_appli_with_class (icon->cClass);
			if (pApplis != NULL)
			{
				Icon *pOneIcon = (Icon *) (g_list_last ((GList*)pApplis)->data);  // on prend le dernier car les applis sont inserees a l'envers, et on veut avoir celle qui etait deja present dans le dock (pour 2 raison : continuite, et la nouvelle (en 1ere position) n'est pas forcement deja dans un dock, ce qui fausse le ratio).
				//g_print ("  load from %s (%dx%d)\n", pOneIcon->cName, iWidth, iHeight);
				icon->pIconBuffer = cairo_dock_duplicate_inhibitor_surface_for_appli (pOneIcon,
					iWidth,
					iHeight);
			}
		}
	}
}
static Icon *cairo_dock_create_icon_for_class_subdock (Icon *pSameClassIcon, CairoDock *pClassMateParentDock, CairoDock *pClassDock)
{
	Icon *pFakeClassIcon = cairo_dock_new_icon ();
	pFakeClassIcon->iTrueType = CAIRO_DOCK_ICON_TYPE_CLASS_CONTAINER;
	pFakeClassIcon->iface.load_image = _load_class_icon;
	pFakeClassIcon->iGroup = pSameClassIcon->iGroup;
	
	pFakeClassIcon->cName = g_strdup (pSameClassIcon->cClass);
	pFakeClassIcon->cCommand = g_strdup (pSameClassIcon->cCommand);
	pFakeClassIcon->pMimeTypes = g_strdupv (pSameClassIcon->pMimeTypes);
	pFakeClassIcon->cClass = g_strdup (pSameClassIcon->cClass);
	pFakeClassIcon->fOrder = pSameClassIcon->fOrder;
	pFakeClassIcon->cParentDockName = g_strdup (pSameClassIcon->cParentDockName);
	pFakeClassIcon->bHasIndicator = pSameClassIcon->bHasIndicator;
	
	pFakeClassIcon->pSubDock = pClassDock;
	return pFakeClassIcon;
}

static CairoDock *_cairo_dock_set_parent_dock_name_for_appli (Icon *icon, CairoDock *pMainDock, const gchar *cMainDockName)
{
	cd_message ("%s (%s)", __func__, icon->cName);
	CairoDock *pParentDock = pMainDock;
	g_free (icon->cParentDockName);
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskbarParam.bGroupAppliByClass && icon->cClass != NULL && ! cairo_dock_class_is_expanded (icon->cClass))
	{
		Icon *pSameClassIcon = cairo_dock_get_classmate (icon);  // un inhibiteur dans un dock OU une appli de meme classe dans un dock != class-sub-dock.
		if (pSameClassIcon == NULL)  // aucun classmate => elle va dans son class sub-dock ou dans le main dock.
		{
			cd_message ("  classe %s encore vide", icon->cClass);
			pParentDock = cairo_dock_search_dock_from_name (icon->cClass);
			if (pParentDock == NULL)  // no class sub-dock => go to main dock
			{
				pParentDock = cairo_dock_search_dock_from_name (cMainDockName);
				icon->cParentDockName = g_strdup (cMainDockName);
			}
			else  // go to class sub-dock
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
				pMainDock = cairo_dock_search_dock_from_name (pSameClassIcon->cParentDockName);  // can be NULL (even if in practice will never be).
				pParentDock = cairo_dock_create_subdock_from_scratch (NULL, icon->cClass, pMainDock);
			}
			else
				cd_message ("  sous-dock de la classe %s existant", icon->cClass);
			
			if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pSameClassIcon) || CAIRO_DOCK_ICON_TYPE_IS_APPLET (pSameClassIcon))  // c'est un inhibiteur.
			{
				if (pSameClassIcon->Xid != 0)  // actuellement l'inhibiteur inhibe 1 seule appli.
				{
					cd_debug ("actuellement l'inhibiteur inhibe 1 seule appli");
					Icon *pInhibitedIcon = cairo_dock_get_icon_with_Xid (pSameClassIcon->Xid);
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
					if (pInhibitedIcon != NULL)
					{
						cd_debug (" on insere %s dans le dock de la classe", pInhibitedIcon->cName);
						g_free (pInhibitedIcon->cParentDockName);
						pInhibitedIcon->cParentDockName = g_strdup (icon->cClass);
						cairo_dock_insert_icon_in_dock_full (pInhibitedIcon, pParentDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, ! CAIRO_DOCK_INSERT_SEPARATOR, NULL);
					}
				}
				else if (pSameClassIcon->pSubDock != pParentDock)
					cd_warning ("this inhibitor doesn't hold the class dock !");
			}
			else  // c'est donc une appli du main dock.
			{
				//\______________ On cree une icone de paille.
				cd_debug (" on cree un fake...");
				CairoDock *pClassMateParentDock = cairo_dock_search_dock_from_name (pSameClassIcon->cParentDockName);  // c'est en fait le main dock.
				Icon *pFakeClassIcon = cairo_dock_create_icon_for_class_subdock (pSameClassIcon, pClassMateParentDock, pParentDock);
				
				//\______________ On la charge.
				cairo_dock_trigger_load_icon_buffers (pFakeClassIcon, CAIRO_CONTAINER (pClassMateParentDock));
				
				//\______________ On detache le classmate, on le place dans le sous-dock, et on lui substitue le faux.
				cd_debug (" on detache %s pour la passer dans le sous-dock de sa classe", pSameClassIcon->cName);
				cairo_dock_detach_icon_from_dock_full (pSameClassIcon, pClassMateParentDock, FALSE);
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
	{
		icon->cParentDockName = g_strdup (cMainDockName);
	}
	return pParentDock;
}

CairoDock *cairo_dock_insert_appli_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bUpdateSize, gboolean bAnimate)
{
	if (! myTaskbarParam.bShowAppli)
		return NULL;
	cd_message ("%s (%s, %d)", __func__, icon->cName, icon->Xid);
	
	if (myTaskbarParam.bAppliOnCurrentDesktopOnly && ! cairo_dock_appli_is_on_current_desktop (icon))
		return NULL;
	
	//\_________________ On gere ses eventuels inhibiteurs.
	if (myTaskbarParam.bMixLauncherAppli && cairo_dock_prevent_inhibited_class (icon))
	{
		cd_message (" -> se fait inhiber");
		return NULL;
	}
	
	//\_________________ On gere le filtre 'applis minimisees seulement'.
	if (!icon->bIsHidden && myTaskbarParam.bHideVisibleApplis)
	{
		cairo_dock_reserve_one_icon_geometry_for_window_manager (&icon->Xid, icon, pMainDock);  // on reserve la position de l'icone dans le dock pour que l'effet de minimisation puisse viser la bonne position avant que l'icone ne soit effectivement dans le dock.
		return NULL;
	}
	
	//\_________________ On determine dans quel dock l'inserer (cree au besoin).
	CairoDock *pParentDock = _cairo_dock_set_parent_dock_name_for_appli (icon, pMainDock, CAIRO_DOCK_MAIN_DOCK_NAME);  // renseigne cParentDockName.   /// TODO: we should actually use the name of 'pMainDock' if one day we want to place the taskbar inside another dock...
	g_return_val_if_fail (pParentDock != NULL, NULL);

	//\_________________ On l'insere dans son dock parent en animant ce dernier eventuellement.
	if (myTaskbarParam.bMixLauncherAppli && pParentDock->iRefCount == 0)  // this appli is amongst the launchers in the main dock
	{
		cairo_dock_set_class_order_in_dock (icon, pParentDock);
	}
	else  // this appli is either in a different group or in the class sub-dock
	{
		cairo_dock_set_class_order_amongst_applis (icon, pParentDock);
	}
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
	
	cairo_dock_detach_icon_from_dock (pIcon, pParentDock);
	
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
#define y_icon_geometry(icon, pDock) (pDock->container.iWindowPositionY + icon->fDrawY - icon->fHeight * myIconsParam.fAmplitude * pDock->fMagnitudeMax)
void  cairo_dock_set_one_icon_geometry_for_window_manager (Icon *icon, CairoDock *pDock)
{
	//g_print ("%s (%s)\n", __func__, icon->cName);
	int iX, iY, iWidth, iHeight;
	iX = x_icon_geometry (icon, pDock);
	iY = y_icon_geometry (icon, pDock);  // il faudrait un fYAtRest ...
	//g_print (" -> %d;%d (%.2f)\n", iX - pDock->container.iWindowPositionX, iY - pDock->container.iWindowPositionY, icon->fXAtRest);
	iWidth = icon->fWidth;
	iHeight = icon->fHeight * (1. + 2*myIconsParam.fAmplitude * pDock->fMagnitudeMax);  // on elargit en haut et en bas, pour gerer les cas ou l'icone grossirait vers le haut ou vers le bas.
	
	if (pDock->container.bIsHorizontal)
		cairo_dock_set_xicon_geometry (icon->Xid, iX, iY, iWidth, iHeight);
	else
		cairo_dock_set_xicon_geometry (icon->Xid, iY, iX, iHeight, iWidth);
}

void cairo_dock_reserve_one_icon_geometry_for_window_manager (Window *Xid, Icon *icon, CairoDock *pMainDock)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && icon->cParentDockName == NULL)
	{
		/// TODO: use the same algorithm as the class-manager to find the future position of the icon ...
		
		Icon *pInhibitor = cairo_dock_get_inhibitor (icon, FALSE);  // FALSE <=> meme en-dehors d'un dock
		if (pInhibitor == NULL)  // cette icone n'est pas inhibee, donc se minimisera dans le dock en une nouvelle icone.
		{
			int x, y;
			Icon *pClassmate = cairo_dock_get_classmate (icon);
			CairoDock *pClassmateDock = (pClassmate ? cairo_dock_search_dock_from_name (pClassmate->cParentDockName) : NULL);
			if (myTaskbarParam.bGroupAppliByClass && pClassmate != NULL && pClassmateDock != NULL)  // on va se grouper avec cette icone.
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
			else if (myTaskbarParam.bMixLauncherAppli && pClassmate != NULL && pClassmateDock != NULL)  // on va se placer a cote.
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
				Icon *pIcon, *pLastLauncher = NULL;
				GList *ic, *last_launcher_ic = NULL;
				for (ic = pMainDock->icons; ic != NULL; ic = ic->next)
				{
					pIcon = ic->data;
					if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)  // launcher, even without class
					|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)  // container icon (likely to contain some launchers)
					|| (CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon) && pIcon->cClass != NULL)  // applet acting like a launcher
					|| (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon)))  // separator (user or auto).
					{
						pLastLauncher = pIcon;
						last_launcher_ic = ic;
					}
				}
						
				if (pLastLauncher != NULL)  // on se placera juste apres.
				{
					x = x_icon_geometry (pLastLauncher, pMainDock) + pLastLauncher->fWidth/2;
					if (cairo_dock_is_hidden (pMainDock))
					{
						y = (pMainDock->container.bDirectionUp ? 0 : g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
					}
					else
					{
						y = y_icon_geometry (pLastLauncher, pMainDock);
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
