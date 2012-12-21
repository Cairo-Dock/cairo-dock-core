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

#include <cairo.h>

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-config.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-X-utilities.h"  // cairo_dock_remove_version_from_string
#include "cairo-dock-class-manager.h"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopEnv g_iDesktopEnv;

static GHashTable *s_hClassTable = NULL;


static void cairo_dock_free_class_appli (CairoDockClassAppli *pClassAppli)
{
	g_list_free (pClassAppli->pIconsOfClass);
	g_list_free (pClassAppli->pAppliOfClass);
	g_free (pClassAppli->cDesktopFile);
	g_free (pClassAppli->cCommand);
	g_free (pClassAppli->cName);
	g_free (pClassAppli->cIcon);
	g_free (pClassAppli->cStartupWMClass);
	g_free (pClassAppli->cWorkingDirectory);
	if (pClassAppli->pMimeTypes)
		g_strfreev (pClassAppli->pMimeTypes);
	g_list_foreach (pClassAppli->pMenuItems, (GFunc)g_strfreev, NULL);
	g_list_free (pClassAppli->pMenuItems);
	g_free (pClassAppli);
}

void cairo_dock_initialize_class_manager (void)
{
	if (s_hClassTable == NULL)
		s_hClassTable = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			g_free,
			(GDestroyNotify) cairo_dock_free_class_appli);
}


static inline CairoDockClassAppli *_cairo_dock_lookup_class_appli (const gchar *cClass)
{
	return (cClass != NULL ? g_hash_table_lookup (s_hClassTable, cClass) : NULL);
}

const GList *cairo_dock_list_existing_appli_with_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL ? pClassAppli->pAppliOfClass : NULL);
}


static CairoDockClassAppli *cairo_dock_get_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli == NULL)
	{
		pClassAppli = g_new0 (CairoDockClassAppli, 1);
		g_hash_table_insert (s_hClassTable, g_strdup (cClass), pClassAppli);
	}
	return pClassAppli;
}

static gboolean _cairo_dock_add_inhibitor_to_class (const gchar *cClass, Icon *pIcon)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	g_return_val_if_fail (pClassAppli!= NULL, FALSE);
	
	g_return_val_if_fail (g_list_find (pClassAppli->pIconsOfClass, pIcon) == NULL, TRUE);
	pClassAppli->pIconsOfClass = g_list_prepend (pClassAppli->pIconsOfClass, pIcon);
	
	return TRUE;
}

CairoDock *cairo_dock_get_class_subdock (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	g_return_val_if_fail (pClassAppli!= NULL, NULL);
	
	return cairo_dock_search_dock_from_name (pClassAppli->cDockName);
}

gchar *cairo_dock_get_class_subdock_name (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	g_return_val_if_fail (pClassAppli!= NULL, NULL);
	
	return pClassAppli->cDockName;
}

CairoDock* cairo_dock_create_class_subdock (const gchar *cClass, CairoDock *pParentDock)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	g_return_val_if_fail (pClassAppli!= NULL, NULL);
	
	CairoDock *pDock = cairo_dock_search_dock_from_name (pClassAppli->cDockName);
	if (pDock == NULL)  // cDockName not yet defined, or previous class subdock no longer exists
	{
		g_free (pClassAppli->cDockName);
		pClassAppli->cDockName = cairo_dock_get_unique_dock_name (cClass);
		pDock = cairo_dock_create_subdock (pClassAppli->cDockName, NULL, pParentDock, NULL);
	}
	
	return pDock;
}

static void cairo_dock_destroy_class_subdock (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	g_return_if_fail (pClassAppli!= NULL);
	
	CairoDock *pDock = cairo_dock_search_dock_from_name (pClassAppli->cDockName);
	if (pDock)
	{
		cairo_dock_destroy_dock (pDock, pClassAppli->cDockName);
	}
	
	g_free (pClassAppli->cDockName);
	pClassAppli->cDockName = NULL;
}

gboolean cairo_dock_add_appli_to_class (Icon *pIcon)
{
	g_return_val_if_fail (pIcon!= NULL, FALSE);
	cd_message ("%s (%s)", __func__, pIcon->cClass);
	
	if (pIcon->cClass == NULL)
	{
		cd_message (" %s n'a pas de classe, c'est po bien", pIcon->cName);
		return FALSE;
	}
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (pIcon->cClass);
	g_return_val_if_fail (pClassAppli!= NULL, FALSE);
	
	///if (pClassAppli->iAge == 0)  // age is > 0, so it means we have never set it yet.
	if (pClassAppli->pAppliOfClass == NULL)  // the first appli of a class defines the age of the class.
		pClassAppli->iAge = pIcon->iAge;
	
	g_return_val_if_fail (g_list_find (pClassAppli->pAppliOfClass, pIcon) == NULL, TRUE);
	pClassAppli->pAppliOfClass = g_list_prepend (pClassAppli->pAppliOfClass, pIcon);
	
	return TRUE;
}

gboolean cairo_dock_remove_appli_from_class (Icon *pIcon)
{
	g_return_val_if_fail (pIcon!= NULL, FALSE);
	cd_message ("%s (%s, %s)", __func__, pIcon->cClass, pIcon->cName);
	
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (pIcon->cClass);
	g_return_val_if_fail (pClassAppli!= NULL, FALSE);
	
	pClassAppli->pAppliOfClass = g_list_remove (pClassAppli->pAppliOfClass, pIcon);
	
	return TRUE;
}

gboolean cairo_dock_set_class_use_xicon (const gchar *cClass, gboolean bUseXIcon)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	g_return_val_if_fail (pClassAppli!= NULL, FALSE);
	
	if (pClassAppli->bUseXIcon == bUseXIcon)  // rien a faire.
		return FALSE;
	
	CairoDock *pDock;
	GList *pElement;
	Icon *pAppliIcon;
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL; pElement = pElement->next)
	{
		pAppliIcon = pElement->data;
		if (bUseXIcon)
		{
			cd_message ("%s prend l'icone de X", pAppliIcon->cName);
		}
		else
		{
			cd_message ("%s n'utilise plus l'icone de X", pAppliIcon->cName);
		}
		
		pDock = cairo_dock_search_dock_from_name (pAppliIcon->cParentDockName);
		cairo_dock_reload_icon_image (pAppliIcon, CAIRO_CONTAINER (pDock));
	}
	
	return TRUE;
}


static void _cairo_dock_set_same_indicator_on_sub_dock (Icon *pInhibhatorIcon)
{
	CairoDock *pInhibatorDock = cairo_dock_search_dock_from_name (pInhibhatorIcon->cParentDockName);
	if (pInhibatorDock != NULL && pInhibatorDock->iRefCount > 0)  // l'inhibiteur est dans un sous-dock.
	{
		gboolean bSubDockHasIndicator = FALSE;
		if (pInhibhatorIcon->bHasIndicator)
		{
			bSubDockHasIndicator = TRUE;
		}
		else
		{
			GList* ic;
			Icon *icon;
			for (ic =pInhibatorDock->icons ; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if (icon->bHasIndicator)
				{
					bSubDockHasIndicator = TRUE;
					break;
				}
			}
		}
		CairoDock *pParentDock = NULL;
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pInhibatorDock, &pParentDock);
		if (pPointingIcon != NULL && pPointingIcon->bHasIndicator != bSubDockHasIndicator)
		{
			cd_message ("  pour le sous-dock %s : indicateur <- %d", pPointingIcon->cName, bSubDockHasIndicator);
			pPointingIcon->bHasIndicator = bSubDockHasIndicator;
			if (pParentDock != NULL)
				cairo_dock_redraw_icon (pPointingIcon, CAIRO_CONTAINER (pParentDock));
		}
	}
}

static Window _cairo_dock_detach_appli_of_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, 0);
	
	const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
	Icon *pIcon;
	const GList *pElement;
	///gboolean bNeedsRedraw = FALSE;
	gboolean bDetached;
	CairoDock *pParentDock;
	Window XFirstFoundId = 0;
	for (pElement = pList; pElement != NULL; pElement = pElement->next)
	{
		pIcon = pElement->data;
		pParentDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
		if (pParentDock == NULL)  // pas dans un dock => rien a faire.
			continue;
		
		cd_debug ("detachement de l'icone %s (%d)", pIcon->cName, XFirstFoundId);
		gchar *cParentDockName = pIcon->cParentDockName;
		pIcon->cParentDockName = NULL;  // astuce.
		bDetached = cairo_dock_detach_icon_from_dock (pIcon, pParentDock);
		if (bDetached)  // detachee => on met a jour son dock.
		{
			if (pParentDock == cairo_dock_get_class_subdock (cClass))  // sous-dock de classe => on le met a jour / detruit.
			{
				if (pParentDock->icons == NULL)  // devient vide => on le detruit.
				{
					if (pParentDock->iRefCount != 0)  // on vire l'icone de paille qui pointe sur ce sous-dock.
					{
						CairoDock *pMainDock = NULL;
						Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pParentDock, &pMainDock);
						if (pMainDock && CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pPointingIcon))
						{
							cairo_dock_remove_icon_from_dock (pMainDock, pPointingIcon);
							///bNeedsRedraw |= pMainDock->bIsMainDock;
							cairo_dock_free_icon (pPointingIcon);
						}
					}
					cairo_dock_destroy_class_subdock (cClass);
				}
				///else  // non vide => on le met a jour.
				///	cairo_dock_update_dock_size (pParentDock);
			}
			///else  // main dock => on le met a jour a la fin.
			///	bNeedsRedraw = TRUE;
		}
		g_free (cParentDockName);
		
		if (XFirstFoundId == 0)  // on recupere la 1ere appli de la classe.
		{
			XFirstFoundId = pIcon->Xid;
		}
	}
	/**if (! cairo_dock_is_loading () && bNeedsRedraw)  // mise a jour du main dock en 1 coup.
	{
		cairo_dock_update_dock_size (g_pMainDock);
		cairo_dock_calculate_dock_icons (g_pMainDock);
		gtk_widget_queue_draw (g_pMainDock->container.pWidget);
	}*/
	return XFirstFoundId;
}
gboolean cairo_dock_inhibite_class (const gchar *cClass, Icon *pInhibitorIcon)
{
	g_return_val_if_fail (cClass != NULL, FALSE);
	cd_message ("%s (%s)", __func__, cClass);
	
	// add inhibitor to class (first, so that applis can find it and take its surface if neccessary)
	if (! _cairo_dock_add_inhibitor_to_class (cClass, pInhibitorIcon))
		return FALSE;
	
	// set class name on the inhibitor if not already done.
	if (pInhibitorIcon && pInhibitorIcon->cClass != cClass)
	{
		g_free (pInhibitorIcon->cClass);
		pInhibitorIcon->cClass = g_strdup (cClass);
	}

	// if launchers are mixed with applis, steal applis icons.
	if (!myTaskbarParam.bMixLauncherAppli)
		return TRUE;
	Window XFirstFoundId = _cairo_dock_detach_appli_of_class (cClass);  // detach existing applis, and then retach them to the inhibitor.
	if (pInhibitorIcon != NULL)
	{
		// inhibitor takes control of the first existing appli of the class.
		pInhibitorIcon->Xid = XFirstFoundId;
		pInhibitorIcon->bHasIndicator = (XFirstFoundId > 0);
		_cairo_dock_set_same_indicator_on_sub_dock (pInhibitorIcon);
		
		// other applis icons are retached to the inhibitor.
		const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
		Icon *pIcon;
		const GList *pElement;
		for (pElement = pList; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			cd_debug ("une appli detachee (%s)", pIcon->cParentDockName);
			if (pIcon->Xid != XFirstFoundId && pIcon->cParentDockName == NULL)  // s'est faite detacher et doit etre rattachee.
				cairo_dock_insert_appli_in_dock (pIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
		}
	}
	
	return TRUE;
}

gboolean cairo_dock_class_is_inhibited (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->pIconsOfClass != NULL);
}

gboolean cairo_dock_class_is_using_xicon (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->bUseXIcon);  // si pClassAppli == NULL, il n'y a pas de lanceur pouvant lui filer son icone, mais on peut en trouver une dans le theme d'icones systeme.
}

gboolean cairo_dock_class_is_expanded (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->bExpand);
}

gboolean cairo_dock_prevent_inhibited_class (Icon *pIcon)
{
	g_return_val_if_fail (pIcon != NULL, FALSE);
	//g_print ("%s (%s)\n", __func__, pIcon->cClass);
	
	gboolean bToBeInhibited = FALSE;
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	if (pClassAppli != NULL)
	{
		Icon *pInhibitorIcon;
		GList *pElement;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			if (pInhibitorIcon != NULL)  // un inhibiteur est present.
			{
				if (pInhibitorIcon->Xid == 0 && pInhibitorIcon->pSubDock == NULL)  // cette icone inhibe cette classe mais ne controle encore aucune appli, on s'y asservit.
				{
					pInhibitorIcon->Xid = pIcon->Xid;
					pInhibitorIcon->bIsHidden = pIcon->bIsHidden;
					cd_message (">>> %s prendra un indicateur au prochain redraw ! (Xid : %d)", pInhibitorIcon->cName, pInhibitorIcon->Xid);
					pInhibitorIcon->bHasIndicator = TRUE;
					_cairo_dock_set_same_indicator_on_sub_dock (pInhibitorIcon);
				/**}
				
				if (pInhibitorIcon->Xid == pIcon->Xid)  // cette icone nous controle.
				{*/
					CairoDock *pInhibatorDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
					//\______________ On place l'icone pour X.
					if (! bToBeInhibited)  // on ne met le thumbnail que sur la 1ere.
					{
						if (pInhibatorDock != NULL)
						{
							//g_print ("on positionne la miniature sur l'inhibiteur %s\n", pInhibitorIcon->cName);
							cairo_dock_set_one_icon_geometry_for_window_manager (pInhibitorIcon, pInhibatorDock);
						}
					}
					//\______________ On met a jour l'etiquette de l'inhibiteur.
					if (pInhibatorDock != NULL && pIcon->cName != NULL)
					{
						if (pInhibitorIcon->cInitialName == NULL)
							pInhibitorIcon->cInitialName = pInhibitorIcon->cName;
						else
							g_free (pInhibitorIcon->cName);
						pInhibitorIcon->cName = NULL;
						cairo_dock_set_icon_name (pIcon->cName, pInhibitorIcon, CAIRO_CONTAINER (pInhibatorDock));
					}
				}
				bToBeInhibited = (pInhibitorIcon->Xid == pIcon->Xid);
			}
		}
	}
	return bToBeInhibited;
}


static gboolean _cairo_dock_remove_icon_from_class (Icon *pInhibitorIcon)
{
	g_return_val_if_fail (pInhibitorIcon != NULL, FALSE);
	cd_message ("%s (%s)", __func__, pInhibitorIcon->cClass);
	
	gboolean bStillInhibited = FALSE;
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pInhibitorIcon->cClass);
	if (pClassAppli != NULL)
	{
		pClassAppli->pIconsOfClass = g_list_remove (pClassAppli->pIconsOfClass, pInhibitorIcon);
		bStillInhibited = (pClassAppli->pIconsOfClass != NULL);  // don't delete the class even if it's totally empty, as we don't want to read the .desktop file again if it appears again.
	}
	return bStillInhibited;
}

void cairo_dock_deinhibite_class (const gchar *cClass, Icon *pInhibitorIcon)
{
	cd_message ("%s (%s)", __func__, cClass);
	gboolean bStillInhibited = _cairo_dock_remove_icon_from_class (pInhibitorIcon);
	cd_debug (" bStillInhibited : %d", bStillInhibited);
	///if (! bStillInhibited)  // il n'y a plus personne dans cette classe.
	///	return ;
	
	if (pInhibitorIcon != NULL && pInhibitorIcon->pSubDock != NULL && pInhibitorIcon->pSubDock == cairo_dock_get_class_subdock (cClass))  // the launcher is controlling several appli icons, place them back in the taskbar.
	{
		// first destroy the class sub-dock, so that the appli icons won't go inside again.
		// we empty the sub-dock then destroy it, then re-insert the appli icons
		GList *icons = pInhibitorIcon->pSubDock->icons;
		pInhibitorIcon->pSubDock->icons = NULL;  // empty the sub-dock
		cairo_dock_destroy_class_subdock (cClass);  // destroy the sub-dock without destroying its icons
		
		// then re-insert the appli icons.
		Icon *pAppli;
		GList *ic;
		for (ic = icons; ic != NULL; ic = ic->next)
		{
			pAppli = ic->data;
			cairo_dock_set_icon_container (pAppli, NULL);  // manually "detach" it
			cairo_dock_insert_appli_in_dock (pAppli, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
		}
		g_list_free (icons);
	}
	
	if (pInhibitorIcon == NULL || pInhibitorIcon->Xid != 0)  // the launcher is controlling 1 appli icon, or we deinhibate all the inhibitors.
	{
		const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
		Icon *pIcon;
		gboolean bNeedsRedraw = FALSE;
		CairoDock *pParentDock;
		const GList *pElement;
		for (pElement = pList; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			if (pInhibitorIcon == NULL || pIcon->Xid == pInhibitorIcon->Xid)
			{
				cd_message ("rajout de l'icone precedemment inhibee (Xid:%d)", pIcon->Xid);
				pIcon->fInsertRemoveFactor = 0;
				pIcon->fScale = 1.;
				pParentDock = cairo_dock_insert_appli_in_dock (pIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
				bNeedsRedraw = (pParentDock != NULL && pParentDock->bIsMainDock);
				//if (pInhibitorIcon != NULL)
				//	break ;
			}
			pParentDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
			cairo_dock_reload_icon_image (pIcon, CAIRO_CONTAINER (pParentDock));  /// question : pourquoi le faire pour toutes les icones ?...
		}
		if (bNeedsRedraw)
			gtk_widget_queue_draw (g_pMainDock->container.pWidget);  /// pDock->pRenderer->calculate_icons (pDock); ?...
	}
	if (pInhibitorIcon != NULL)
	{
		cd_message (" l'inhibiteur a perdu toute sa mana");
		pInhibitorIcon->Xid = 0;
		pInhibitorIcon->bHasIndicator = FALSE;
		g_free (pInhibitorIcon->cClass);
		pInhibitorIcon->cClass = NULL;
		cd_debug ("  plus de classe");
	}
}


void cairo_dock_detach_Xid_from_inhibitors (Window Xid, const gchar *cClass)
{
	cd_message ("%s (%s)", __func__, cClass);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		int iNextXid = -1;  // next window that will be inhibited.
		Icon *pSameClassIcon = NULL;
		Icon *pIcon;
		GList *pElement;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			if (pIcon->Xid == Xid)  // this inhibitor controls the given window -> make it control another (possibly none).
			{
				if (iNextXid == -1)  // we didn't search the next window yet, do it now.
				{
					Icon *pOneIcon;
					GList *ic;
					for (ic = g_list_last (pClassAppli->pAppliOfClass); ic != NULL; ic = ic->prev)  // reverse order, to take the oldest window of this class.
					{
						pOneIcon = ic->data;
						if (pOneIcon != NULL
						&& ! cairo_dock_icon_is_being_removed (pOneIcon)  // small optimization
						&& pOneIcon->Xid != Xid  // not the window we precisely want to avoid
						&& (! myTaskbarParam.bAppliOnCurrentDesktopOnly || cairo_dock_appli_is_on_current_desktop (pOneIcon)))  // can actually be displayed
						{
							pSameClassIcon = pOneIcon;
							break ;
						}
					}
					iNextXid = (pSameClassIcon != NULL ? pSameClassIcon->Xid : 0);
					if (pSameClassIcon != NULL)  // this icon will be inhibited, we need to detach it if needed
					{
						cd_message ("  c'est %s qui va la remplacer", pSameClassIcon->cName);
						CairoDock *pSameClassDock = cairo_dock_search_dock_from_name (pSameClassIcon->cParentDockName);
						if (pSameClassDock != NULL)  // it's inside a dock -> detach it
						{
							cairo_dock_detach_icon_from_dock (pSameClassIcon, pSameClassDock);
							///cairo_dock_update_dock_size (pSameClassDock);  // it can't be the class sub-dock, because pIcon had the Xid, so it doesn't hold the class sub-dock and the class is not grouped (otherwise they would all be in the class sub-dock).
						}
					}
				}
				pIcon->Xid = iNextXid;
				pIcon->bHasIndicator = (iNextXid != 0);
				_cairo_dock_set_same_indicator_on_sub_dock (pIcon);
				if (! pIcon->bHasIndicator)
				{
					cairo_dock_set_icon_name (pIcon->cInitialName ,pIcon, NULL);
				}
				cd_message (" %s : bHasIndicator <- %d, Xid <- %d", pIcon->cName, pIcon->bHasIndicator, pIcon->Xid);
				CairoDock *pParentDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
				if (pParentDock)
					gtk_widget_queue_draw (pParentDock->container.pWidget);
			}
		}
	}
}

static void _cairo_dock_remove_all_applis_from_class (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, G_GNUC_UNUSED gpointer data)
{
	g_list_free (pClassAppli->pAppliOfClass);
	pClassAppli->pAppliOfClass = NULL;
	
	Icon *pInhibitorIcon;
	GList *pElement;
	for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
	{
		pInhibitorIcon = pElement->data;
		pInhibitorIcon->bHasIndicator = FALSE;
		pInhibitorIcon->Xid = 0;
		_cairo_dock_set_same_indicator_on_sub_dock (pInhibitorIcon);
	}
}
void cairo_dock_remove_all_applis_from_class_table (void)  // pour le stop_application_manager
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_remove_all_applis_from_class, NULL);
}

void cairo_dock_reset_class_table (void)
{
	g_hash_table_remove_all (s_hClassTable);
}



cairo_surface_t *cairo_dock_duplicate_inhibitor_surface_for_appli (Icon *pInhibitorIcon, int iWidth, int iHeight)
{
	int w, h;
	cairo_dock_get_icon_extent (pInhibitorIcon, &w, &h);
	
	return cairo_dock_duplicate_surface (pInhibitorIcon->image.pSurface,
		w,
		h,
		iWidth,
		iHeight);
}
cairo_surface_t *cairo_dock_create_surface_from_class (const gchar *cClass, int iWidth, int iHeight)
{
	cd_debug ("%s (%s)", __func__, cClass);
	// first we try to get an icon from one of the inhibator.
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		cd_debug ("bUseXIcon:%d", pClassAppli->bUseXIcon);
		if (pClassAppli->bUseXIcon)
			return NULL;
		
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			cd_debug ("  %s", pInhibitorIcon->cName);
			if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon))
			{
				if (pInhibitorIcon->pSubDock == NULL || myIndicatorsParam.bUseClassIndic)  // dans le cas d'un lanceur qui aurait deja plusieurs instances de sa classe, et qui les representerait en pile, on ne prend pas son icone.
				{
					cd_debug ("%s will give its surface", pInhibitorIcon->cName);
					return cairo_dock_duplicate_inhibitor_surface_for_appli (pInhibitorIcon, iWidth, iHeight);
				}
				else if (pInhibitorIcon->cFileName != NULL)
				{
					gchar *cIconFilePath = cairo_dock_search_icon_s_path (pInhibitorIcon->cFileName, MAX (iWidth, iHeight));
					if (cIconFilePath != NULL)
					{
						cd_debug ("we replace X icon by %s", cIconFilePath);
						cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
							iWidth,
							iHeight);
						g_free (cIconFilePath);
						if (pSurface)
							return pSurface;
					}
				}
			}
		}
	}
	
	// if we didn't find one, we use the icon defined in the class.
	if (pClassAppli != NULL && pClassAppli->cIcon != NULL)
	{
		cd_debug ("get the class icon (%s)", pClassAppli->cIcon);
		gchar *cIconFilePath = cairo_dock_search_icon_s_path (pClassAppli->cIcon, MAX (iWidth, iHeight));
		cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
			iWidth,
			iHeight);
		g_free (cIconFilePath);
		if (pSurface)
			return pSurface;
	}
	else
	{
		cd_debug ("no icon for the class %s", cClass);
	}
	
	// if not found or not defined, try to find an icon based on the name class.
	gchar *cIconFilePath = cairo_dock_search_icon_s_path (cClass, MAX (iWidth, iHeight));
	if (cIconFilePath != NULL)
	{
		cd_debug ("on remplace l'icone X par %s", cIconFilePath);
		cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
			iWidth,
			iHeight);
		g_free (cIconFilePath);
		if (pSurface)
			return pSurface;
	}
	
	cd_debug ("classe %s prendra l'icone X", cClass);
	return NULL;
}


void cairo_dock_update_visibility_on_inhibitors (const gchar *cClass, Window Xid, gboolean bIsHidden)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			
			if (pInhibitorIcon->Xid == Xid)
			{
				cd_debug (" %s aussi se %s", pInhibitorIcon->cName, (bIsHidden ? "cache" : "montre"));
				pInhibitorIcon->bIsHidden = bIsHidden;
				if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon) && myTaskbarParam.fVisibleAppliAlpha != 0)
				{
					CairoDock *pInhibatorDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
					pInhibitorIcon->fAlpha = 1;  // on triche un peu.
					cairo_dock_redraw_icon (pInhibitorIcon, CAIRO_CONTAINER (pInhibatorDock));
				}
			}
		}
	}
}

void cairo_dock_update_activity_on_inhibitors (const gchar *cClass, Window Xid)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			
			if (pInhibitorIcon->Xid == Xid)
			{
				cd_debug (" %s aussi devient active", pInhibitorIcon->cName);
				///pInhibitorIcon->bIsActive = TRUE;
				CairoDock *pParentDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
				if (pParentDock != NULL)
					cairo_dock_animate_icon_on_active (pInhibitorIcon, pParentDock);
			}
		}
	}
}

void cairo_dock_update_inactivity_on_inhibitors (const gchar *cClass, Window Xid)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			
			if (pInhibitorIcon->Xid == Xid)
			{
				cd_debug (" %s aussi devient inactive", pInhibitorIcon->cName);
				///pInhibitorIcon->bIsActive = FALSE;
				CairoDock *pParentDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
				if (pParentDock != NULL && ! pParentDock->bIsShrinkingDown)
					cairo_dock_redraw_icon (pInhibitorIcon, CAIRO_CONTAINER (pParentDock));
			}
		}
	}
}

void cairo_dock_update_name_on_inhibitors (const gchar *cClass, Window Xid, gchar *cNewName)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			
			if (pInhibitorIcon->Xid == Xid)
			{
				CairoDock *pParentDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
				if (pParentDock != NULL)
				{
					if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon))
					{
						cd_debug (" %s change son nom en %s", pInhibitorIcon->cName, cNewName);
						if (pInhibitorIcon->cInitialName == NULL)
						{
							pInhibitorIcon->cInitialName = pInhibitorIcon->cName;
							cd_debug ("pInhibitorIcon->cInitialName <- %s", pInhibitorIcon->cInitialName);
						}
						else
							g_free (pInhibitorIcon->cName);
						pInhibitorIcon->cName = NULL;
						
						cairo_dock_set_icon_name ((cNewName != NULL ? cNewName : pInhibitorIcon->cInitialName), pInhibitorIcon, CAIRO_CONTAINER (pParentDock));
					}
					if (! pParentDock->bIsShrinkingDown)
						cairo_dock_redraw_icon (pInhibitorIcon, CAIRO_CONTAINER (pParentDock));
				}
			}
		}
	}
}

Icon *cairo_dock_get_classmate (Icon *pIcon)
{
	cd_debug ("%s (%s)", __func__, pIcon->cClass);
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (pIcon->cClass);
	if (pClassAppli == NULL)
		return NULL;
	
	Icon *pFriendIcon = NULL;
	GList *pElement;
	for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
	{
		pFriendIcon = pElement->data;
		if (pFriendIcon == NULL || pFriendIcon->cParentDockName == NULL)  // if not inside a dock, ignore.
			continue ;
		cd_debug (" friend : %s (%d)", pFriendIcon->cName, pFriendIcon->Xid);
		if (pFriendIcon->Xid != 0 || pFriendIcon->pSubDock != NULL)  // is linked to a window, 1 or several times (Xid or class sub-dock).
			return pFriendIcon;
	}
	
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL; pElement = pElement->next)
	{
		pFriendIcon = pElement->data;
		if (pFriendIcon == pIcon)  // skip ourselves
			continue ;
		if (pFriendIcon->cParentDockName != NULL && strcmp (pFriendIcon->cParentDockName, pIcon->cClass) != 0)  // inside a dock, but not the class sub-dock
		//if (pFriendIcon != pIcon && pFriendIcon->cParentDockName != NULL && strcmp (pFriendIcon->cParentDockName, CAIRO_DOCK_MAIN_DOCK_NAME) == 0)  // is inside a dock
			return pFriendIcon;
	}
	
	return NULL;
}



gboolean cairo_dock_check_class_subdock_is_empty (CairoDock *pDock, const gchar *cClass)
{
	cd_debug ("%s (%s, %d)", __func__, cClass, g_list_length (pDock->icons));
	if (pDock->iRefCount == 0)
		return FALSE;
	if (pDock->icons == NULL)  // shouldn't happen, handle this case and make some noise.
	{
		cd_warning ("the %s class sub-dock has no element, which is probably an error !\nit will be destroyed.", cClass);
		CairoDock *pFakeParentDock = NULL;
		Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
		cairo_dock_destroy_class_subdock (cClass);
		pFakeClassIcon->pSubDock = NULL;
		if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pFakeClassIcon))
		{
			cairo_dock_remove_icon_from_dock (pFakeParentDock, pFakeClassIcon);
			cairo_dock_free_icon (pFakeClassIcon);
			cairo_dock_update_dock_size (pFakeParentDock);
			cairo_dock_calculate_dock_icons (pFakeParentDock);
		}
		return TRUE;
	}
	else if (pDock->icons->next == NULL)  // only 1 icon left in the sub-dock -> destroy it.
	{
		cd_debug ("   le sous-dock de la classe %s n'a plus que 1 element et va etre vide puis detruit", cClass);
		Icon *pLastClassIcon = pDock->icons->data;
		
		CairoDock *pFakeParentDock = NULL;
		Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
		g_return_val_if_fail (pFakeClassIcon != NULL, TRUE);
		if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pFakeClassIcon))  // le sous-dock est pointe par une icone de paille.
		{
			cd_debug ("trouve l'icone en papier (%x;%x)", pFakeClassIcon, pFakeParentDock);
			gboolean bLastIconIsRemoving = cairo_dock_icon_is_being_removed (pLastClassIcon);  // keep the removing state because when we detach the icon, it returns to normal state.
			cairo_dock_detach_icon_from_dock_full (pLastClassIcon, pDock, FALSE);
			g_free (pLastClassIcon->cParentDockName);
			pLastClassIcon->cParentDockName = g_strdup (pFakeClassIcon->cParentDockName);
			pLastClassIcon->fOrder = pFakeClassIcon->fOrder;
			
			cd_debug (" on detruit le sous-dock...");
			cairo_dock_destroy_class_subdock (cClass);
			pFakeClassIcon->pSubDock = NULL;
			
			cd_debug ("on enleve l'icone de paille");
			cairo_dock_remove_icon_from_dock (pFakeParentDock, pFakeClassIcon);
			
			cd_debug ("on detruit l'icone de paille");
			cairo_dock_free_icon (pFakeClassIcon);
			
			cd_debug (" puis on re-insere l'appli restante");
			if (! bLastIconIsRemoving)
			{
				cairo_dock_insert_icon_in_dock (pLastClassIcon, pFakeParentDock, ! CAIRO_DOCK_ANIMATE_ICON);
				///cairo_dock_calculate_dock_icons (pFakeParentDock);
				///cairo_dock_redraw_icon (pLastClassIcon, CAIRO_CONTAINER (pFakeParentDock));  // on suppose que les tailles des 2 icones sont identiques.
				///cairo_dock_redraw_container (CAIRO_CONTAINER (pFakeParentDock));
			}
			else  // la derniere icone est en cours de suppression, inutile de la re-inserer. (c'est souvent lorsqu'on ferme toutes une classe d'un coup. donc les animations sont pratiquement dans le meme etat, donc la derniere icone en est aussi a la fin, donc on ne verrait de toute facon aucune animation.
			{
				cd_debug ("inutile de re-inserer l'icone restante");
				cairo_dock_free_icon (pLastClassIcon);
				///cairo_dock_update_dock_size (pFakeParentDock);
				///cairo_dock_calculate_dock_icons (pFakeParentDock);
				///cairo_dock_redraw_container (CAIRO_CONTAINER (pFakeParentDock));
			}
		}
		else  // le sous-dock est pointe par un inhibiteur (normal launcher ou applet).
		{
			gboolean bLastIconIsRemoving = cairo_dock_icon_is_being_removed (pLastClassIcon);  // keep the removing state because when we detach the icon, it returns to normal state.
			cairo_dock_detach_icon_from_dock_full (pLastClassIcon, pDock, FALSE);
			g_free (pLastClassIcon->cParentDockName);
			pLastClassIcon->cParentDockName = NULL;
			
			cairo_dock_destroy_class_subdock (cClass);
			pFakeClassIcon->pSubDock = NULL;
			cd_debug ("sanity check : pFakeClassIcon->Xid : %d", pFakeClassIcon->Xid);
			if (! bLastIconIsRemoving)
			{
				cairo_dock_insert_appli_in_dock (pLastClassIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);  // a priori inutile.
				cairo_dock_update_name_on_inhibitors (cClass, pLastClassIcon->Xid, pLastClassIcon->cName);
			}
			else  // la derniere icone est en cours de suppression, inutile de la re-inserer
			{
				pFakeClassIcon->bHasIndicator = FALSE;
				cairo_dock_free_icon (pLastClassIcon);
			}
			cairo_dock_redraw_icon (pFakeClassIcon, CAIRO_CONTAINER (g_pMainDock));
		}
		cd_debug ("no more dock");
		return TRUE;
	}
	return FALSE;
}


static void _cairo_dock_reset_overwrite_exceptions (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, G_GNUC_UNUSED gpointer data)
{
	pClassAppli->bUseXIcon = FALSE;
}
void cairo_dock_set_overwrite_exceptions (const gchar *cExceptions)
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_reset_overwrite_exceptions, NULL);
	if (cExceptions == NULL)
		return ;
	
	gchar **cClassList = g_strsplit (cExceptions, ";", -1);
	if (cClassList == NULL || cClassList[0] == NULL || *cClassList[0] == '\0')
	{
		g_strfreev (cClassList);
		return ;
	}
	CairoDockClassAppli *pClassAppli;
	int i;
	for (i = 0; cClassList[i] != NULL; i ++)
	{
		pClassAppli = cairo_dock_get_class (cClassList[i]);
		pClassAppli->bUseXIcon = TRUE;
	}
	
	g_strfreev (cClassList);
}

static void _cairo_dock_reset_group_exceptions (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, G_GNUC_UNUSED gpointer data)
{
	pClassAppli->bExpand = FALSE;
}
void cairo_dock_set_group_exceptions (const gchar *cExceptions)
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_reset_group_exceptions, NULL);
	if (cExceptions == NULL)
		return ;
	
	gchar **cClassList = g_strsplit (cExceptions, ";", -1);
	if (cClassList == NULL || cClassList[0] == NULL || *cClassList[0] == '\0')
	{
		g_strfreev (cClassList);
		return ;
	}
	CairoDockClassAppli *pClassAppli;
	int i;
	for (i = 0; cClassList[i] != NULL; i ++)
	{
		pClassAppli = cairo_dock_get_class (cClassList[i]);
		pClassAppli->bExpand = TRUE;
	}
	
	g_strfreev (cClassList);
}


Icon *cairo_dock_get_prev_next_classmate_icon (Icon *pIcon, gboolean bNext)
{
	cd_debug ("%s (%s, %s)", __func__, pIcon->cClass, pIcon->cName);
	g_return_val_if_fail (pIcon->cClass != NULL, NULL);
	
	Icon *pActiveIcon = cairo_dock_get_current_active_icon ();
	if (pActiveIcon == NULL || pActiveIcon->cClass == NULL || strcmp (pActiveIcon->cClass, pIcon->cClass) != 0)  // la fenetre active n'est pas de notre classe, on active l'icone fournies en entree.
	{
		cd_debug ("on active la classe %s", pIcon->cClass);
		return pIcon;
	}
	
	//\________________ on va chercher dans la classe la fenetre active, et prendre la suivante ou la precedente.
	Icon *pNextIcon = NULL;
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (pIcon->cClass);
	if (pClassAppli == NULL)
		return NULL;
	
	//\________________ On cherche dans les icones d'applis.
	Icon *pClassmateIcon;
	GList *pElement, *ic;
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL && pNextIcon == NULL; pElement = pElement->next)
	{
		pClassmateIcon = pElement->data;
		cd_debug (" %s est-elle active ?", pClassmateIcon->cName);
		if (pClassmateIcon->Xid == pActiveIcon->Xid)  // on a trouve la fenetre active.
		{
			cd_debug ("  fenetre active trouvee (%s; %d)", pClassmateIcon->cName, pClassmateIcon->Xid);
			if (bNext)  // on prend la 1ere non nulle qui suit.
			{
				ic = pElement;
				do
				{
					ic = cairo_dock_get_next_element (ic, pClassAppli->pAppliOfClass);
					if (ic == pElement)
					{
						cd_debug ("  on a fait le tour sans rien trouve");
						break ;
					}
					pClassmateIcon = ic->data;
					if (pClassmateIcon != NULL && pClassmateIcon->Xid != 0)
					{
						cd_debug ("  ok on prend celle-la (%s; %d)", pClassmateIcon->cName, pClassmateIcon->Xid);
						pNextIcon = pClassmateIcon;
						break ;
					}
					cd_debug ("un coup pour rien");
				}
				while (1);
			}
			else  // on prend la 1ere non nulle qui precede.
			{
				ic = pElement;
				do
				{
					ic = cairo_dock_get_previous_element (ic, pClassAppli->pAppliOfClass);
					if (ic == pElement)
						break ;
					pClassmateIcon = ic->data;
					if (pClassmateIcon != NULL && pClassmateIcon->Xid != 0)
					{
						pNextIcon = pClassmateIcon;
						break ;
					}
				}
				while (1);
			}
			break ;
		}
	}
	return pNextIcon;
}



Icon *cairo_dock_get_inhibitor (Icon *pIcon, gboolean bOnlyInDock)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (pIcon->cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			
			if (pInhibitorIcon->Xid == pIcon->Xid)
			{
				if (! bOnlyInDock || pInhibitorIcon->cParentDockName != NULL)
					return pInhibitorIcon;
			}
		}
	}
	return NULL;
}

/* Not used
static gboolean _appli_is_older (Icon *pIcon1, Icon *pIcon2, CairoDockClassAppli *pClassAppli)  // TRUE if icon1 older than icon2
{
	Icon *pAppliIcon;
	GList *ic;
	for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
	{
		pAppliIcon = ic->data;
		if (pAppliIcon == pIcon1)  // we found the icon1 first, so icon1 is more recent (prepend).
			return FALSE;
		if (pAppliIcon == pIcon2)  // we found the icon2 first, so icon2 is more recent (prepend).
			return TRUE;
	}
	return FALSE;
}
*/
static inline double _get_previous_order (GList *ic)
{
	if (ic == NULL)
		return 0;
	double fOrder;
	Icon *icon = ic->data;
	Icon *prev_icon = (ic->prev ? ic->prev->data : NULL);
	if (prev_icon != NULL && cairo_dock_get_icon_order (prev_icon) == cairo_dock_get_icon_order (icon))

		fOrder = (icon->fOrder + prev_icon->fOrder) / 2;
	else
		fOrder = icon->fOrder - 1;
	return fOrder;
}
static inline double _get_next_order (GList *ic)
{
	if (ic == NULL)
		return 0;
	double fOrder;
	Icon *icon = ic->data;
	Icon *next_icon = (ic->next ? ic->next->data : NULL);
	if (next_icon != NULL && cairo_dock_get_icon_order (next_icon) == cairo_dock_get_icon_order (icon))
		fOrder = (icon->fOrder + next_icon->fOrder) / 2;
	else
		fOrder = icon->fOrder + 1;
	return fOrder;
}
static inline double _get_first_appli_order (CairoDock *pDock, GList *first_launcher_ic, GList *last_launcher_ic)
{
	double fOrder;
	switch (myTaskbarParam.iIconPlacement)
	{
		case CAIRO_APPLI_BEFORE_FIRST_ICON:
			fOrder = _get_previous_order (pDock->icons);
		break;
		
		case CAIRO_APPLI_BEFORE_FIRST_LAUNCHER:
			if (first_launcher_ic != NULL)
			{
				//g_print (" go just before the first launcher (%s)\n", ((Icon*)first_launcher_ic->data)->cName);
				fOrder = _get_previous_order (first_launcher_ic);  // 'first_launcher_ic' includes the separators, so we can just take the previous order.
			}
			else  // no launcher, go to the beginning of the dock.
			{
				fOrder = _get_previous_order (pDock->icons);
			}
		break;
		
		case CAIRO_APPLI_AFTER_ICON:
		{
			Icon *icon;
			GList *ic = NULL;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if ((icon->cDesktopFileName != NULL && g_strcmp0 (icon->cDesktopFileName, myTaskbarParam.cRelativeIconName) == 0)
				|| (icon->pModuleInstance && g_strcmp0 (icon->pModuleInstance->cConfFilePath, myTaskbarParam.cRelativeIconName) == 0))
					break;
			}
			
			if (ic != NULL)  // icon found
			{
				fOrder = _get_next_order (ic);
				break;
			}  // else don't break, and go to the 'CAIRO_APPLI_AFTER_LAST_LAUNCHER' case, which will be the fallback.
		}
		
		case CAIRO_APPLI_AFTER_LAST_LAUNCHER:
		default:
			if (last_launcher_ic != NULL)
			{
				//g_print (" go just after the last launcher (%s)\n", ((Icon*)last_launcher_ic->data)->cName);
				fOrder = _get_next_order (last_launcher_ic);
			}
			else  // no launcher, go to the beginning of the dock.
			{
				fOrder = _get_previous_order (pDock->icons);
			}
		break;
		
		case CAIRO_APPLI_AFTER_LAST_ICON:
			fOrder = _get_next_order (g_list_last (pDock->icons));
		break;
	}
	return fOrder;
}
static inline int _get_class_age (CairoDockClassAppli *pClassAppli)
{
	if (pClassAppli->pAppliOfClass == NULL)
		return 0;
	return pClassAppli->iAge;
}
// Set the order of an appli when they are mixed amongst launchers and no class sub-dock exists (because either they are not grouped by class, or just it's the first appli of this class in the dock)
// First try to see if an inhibitor is present in the dock; if not, see if an appli of the same class is present in the dock.
// -> if yes, place it next to it, ordered by age (go to the right until our age is greater)
// -> if no, place it amongst the other appli icons, ordered by age (search the last launcher, skip any automatic separator, and then go to the right until our age is greater or there is no more appli).
void cairo_dock_set_class_order_in_dock (Icon *pIcon, CairoDock *pDock)
{
	//g_print ("%s (%s, %d)\n", __func__, pIcon->cClass, pIcon->iAge);
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (pIcon->cClass);
	g_return_if_fail (pClassAppli != NULL);
	
	// Look for an icon of the same class in the dock, to place ourself relatively to it.
	Icon *pSameClassIcon = NULL;
	GList *same_class_ic = NULL;

	// First look for an inhibitor of this class, preferably a launcher.
	CairoDock *pParentDock;
	Icon *pInhibitorIcon;
	GList *ic;
	for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
	{
		pInhibitorIcon = ic->data;

		pParentDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
		if (!pParentDock)  // not inside a dock, for instance a desklet -> skip
			continue;
		///if (pParentDock->iRefCount != 0)  // inside a sub-dock, take the pointing icon inside the main dock.
		///	pInhibitorIcon = cairo_dock_search_icon_pointing_on_dock (pParentDock, NULL);
		pSameClassIcon = pInhibitorIcon;
		same_class_ic = ic;
		//g_print (" found an inhibitor of this class: %s (%d)\n", pSameClassIcon->cName, pSameClassIcon->iAge);
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pSameClassIcon))  // it's a launcher, we wont't find better -> quit
			break ;
	}

	// if no inhibitor found, look for an appli of this class in the dock.
	if (pSameClassIcon == NULL)
	{
		Icon *pAppliIcon;
		for (ic = g_list_last (pClassAppli->pAppliOfClass); ic != NULL; ic = ic->prev)  // check the older icons first (prepend), because then we'll place ourself to their right.
		{
			pAppliIcon = ic->data;
			if (pAppliIcon == pIcon)  // skip ourself
				continue;
			pParentDock = cairo_dock_search_dock_from_name (pAppliIcon->cParentDockName);
			if (pParentDock != NULL)  // && pParentDock->bIsMainDock
			{
				pSameClassIcon = pAppliIcon;
				same_class_ic = ic;
				//g_print (" found an appli of this class: %s (%d)\n", pSameClassIcon->cName, pSameClassIcon->iAge);
				break ;
			}
		}
		pIcon->iGroup = (myTaskbarParam.bSeparateApplis ? CAIRO_DOCK_APPLI : CAIRO_DOCK_LAUNCHER);  // no inhibitor, so we'll go in the taskbar group.
	}
	else  // an inhibitor is present, we'll go next to it, so we'll be in its group.
	{
		pIcon->iGroup = pSameClassIcon->iGroup;
	}
	
	// if we found one, place next to it, ordered by age amongst the other appli of this class already in the dock.
	if (pSameClassIcon != NULL)
	{
		same_class_ic = g_list_find (pDock->icons, pSameClassIcon);
		g_return_if_fail (same_class_ic != NULL);
		Icon *pNextIcon = NULL;  // the next icon after all the icons of our class, or NULL if we reach the end of the dock.
		for (ic = same_class_ic->next; ic != NULL; ic = ic->next)
		{
			pNextIcon = ic->data;
			//g_print ("  next icon: %s (%d)\n", pNextIcon->cName, pNextIcon->iAge);
			if (!pNextIcon->cClass || strcmp (pNextIcon->cClass, pIcon->cClass) != 0)  // not our class any more, quit.
				break;
			
			if (pIcon->iAge > pNextIcon->iAge)  // we are more recent than this icon -> place on its right -> continue
			{
				pSameClassIcon = pNextIcon;  // 'pSameClassIcon' will be the last icon of our class older than us.
				pNextIcon = NULL;
			}
			else  // we are older than it -> go just before it -> quit
			{
				break;
			}
		}
		//g_print (" pNextIcon: %s (%d)\n", pNextIcon?pNextIcon->cName:"none", pNextIcon?pNextIcon->iAge:-1);
		
		if (pNextIcon != NULL && cairo_dock_get_icon_order (pNextIcon) == cairo_dock_get_icon_order (pSameClassIcon))  // l'icone suivante est dans le meme groupe que nous, on s'intercalle entre elle et pSameClassIcon.
			pIcon->fOrder = (pNextIcon->fOrder + pSameClassIcon->fOrder) / 2;
		else  // aucune icone apres notre classe, ou alors dans un groupe different, on se place juste apres pSameClassIcon.
			pIcon->fOrder = pSameClassIcon->fOrder + 1;
		
		return;
	}
	
	// if no icon of our class is present in the dock, place it amongst the other appli icons, after the first appli or after the launchers, and ordered by age.
	// search the last launcher and the first appli.
	Icon *icon;
	Icon *pFirstLauncher = NULL;
	GList *first_appli_ic = NULL, *last_launcher_ic = NULL, *first_launcher_ic = NULL;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)  // launcher, even without class
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon)  // container icon (likely to contain some launchers)
		|| (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && icon->pModuleInstance->pModule->pVisitCard->bActAsLauncher)  // applet acting like a launcher
		/**|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon)*/)  // separator (user or auto).
		{
			// pLastLauncher = icon;
			last_launcher_ic = ic;
			if (pFirstLauncher == NULL)
			{
				pFirstLauncher = icon;
				first_launcher_ic = ic;
			}
		}
		else if ((CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) || CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
		&& ! cairo_dock_class_is_inhibited (icon->cClass))  // an appli not placed next to its inhibitor.
		{
			// pFirstAppli = icon;
			first_appli_ic = ic;
			break ;
		}
	}
	//g_print (" last launcher: %s\n", pLastLauncher?pLastLauncher->cName:"none");
	//g_print (" first appli: %s\n", pFirstAppli?pFirstAppli->cName:"none");
	
	// place amongst the other applis, or after the last launcher.
	if (first_appli_ic != NULL)  // if an appli exists in the dock, use it as an anchor.
	{
		int iAge = _get_class_age (pClassAppli);  // the age of our class.
		
		GList *last_appli_ic = NULL;  // last appli whose class is older than ours => we'll go just after.
		for (ic = first_appli_ic; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			if (! CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) && ! CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
				break;
			
			// get the age of this class (= age of the oldest icon of this class)
			CairoDockClassAppli *pOtherClassAppli = _cairo_dock_lookup_class_appli (icon->cClass);
			if (! pOtherClassAppli || ! pOtherClassAppli->pAppliOfClass)  // should never happen
				continue;
			
			int iOtherClassAge = _get_class_age (pOtherClassAppli);
			//g_print (" age of class %s: %d\n", icon->cClass, iOtherClassAge);
			
			// compare to our class.
			if (iOtherClassAge < iAge)  // it's older than our class -> skip this whole class, we'll go after.
			{
				Icon *next_icon;
				while (ic->next != NULL)
				{
					next_icon = ic->next->data;
					if (next_icon->cClass && strcmp (next_icon->cClass, icon->cClass) == 0)  // next icon is of the same class -> skip
						ic = ic->next;
					else
						break;
				}
				last_appli_ic = ic;
			}
			else  // we are older -> discard and quit.
			{
				break;
			}
		}
		
		if (last_appli_ic == NULL)  // we are the oldest class -> go just before the first appli
		{
			//g_print (" we are the oldest class\n");
			pIcon->fOrder = _get_previous_order (first_appli_ic);
		}
		else  // go just after the last one
		{
			//g_print (" go just after %s\n", ((Icon*)last_appli_ic->data)->cName);
			pIcon->fOrder = _get_next_order (last_appli_ic);
		}
	}
	else  // no appli yet in the dock -> place it at the taskbar position defined in conf.
	{
		pIcon->fOrder = _get_first_appli_order (pDock, first_launcher_ic, last_launcher_ic);
	}
}

void cairo_dock_set_class_order_amongst_applis (Icon *pIcon, CairoDock *pDock)  // set the order of an appli amongst the other applis of a given dock (class sub-dock or main dock).
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (pIcon->cClass);
	g_return_if_fail (pClassAppli != NULL);
	
	// place the icon amongst the other appli icons of this class, or after the last appli if none.
	if (myTaskbarParam.bSeparateApplis)
		pIcon->iGroup = CAIRO_DOCK_APPLI;
	else
		pIcon->iGroup = CAIRO_DOCK_LAUNCHER;
	Icon *icon;
	GList *ic, *last_ic = NULL, *first_appli_ic = NULL;
	GList *last_launcher_ic = NULL, *first_launcher_ic = NULL;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon))
		{
			if (! first_appli_ic)
				first_appli_ic = ic;
			if (icon->cClass && strcmp (icon->cClass, pIcon->cClass) == 0)  // this icon is in our class.
			{
				if (icon->iAge < pIcon->iAge)  // it's older than us => we are more recent => go after => continue.
				{
					last_ic = ic;  // remember the last item of our class.
				}
				else  // we are older than it => go just before it.
				{
					pIcon->fOrder = _get_previous_order (ic);
					return ;
				}
			}
		}
		else if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)  // launcher, even without class
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon)  // container icon (likely to contain some launchers)
		|| (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && icon->cClass != NULL && icon->pModuleInstance->pModule->pVisitCard->bActAsLauncher)  // applet acting like a launcher
		|| (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon)))  // separator (user or auto).
		{
			last_launcher_ic = ic;
			if (first_launcher_ic == NULL)
			{
				first_launcher_ic = ic;
			}
		}
	}
	
	if (last_ic != NULL)  // there are some applis of our class, but none are more recent than us, so we are the most recent => go just after the last one we found previously.
	{
		pIcon->fOrder = _get_next_order (last_ic);
	}
	else  // we didn't find a single icon of our class => place amongst the other applis from age.
	{
		if (first_appli_ic != NULL)  // if an appli exists in the dock, use it as an anchor.
		{
			Icon *pOldestAppli = g_list_last (pClassAppli->pAppliOfClass)->data;  // prepend
			int iAge = pOldestAppli->iAge;  // the age of our class.
			
			GList *last_appli_ic = NULL;  // last appli whose class is older than ours => we'll go just after.
			for (ic = first_appli_ic; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if (! CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon))
					break;
				
				// get the age of this class (= age of the oldest icon of this class)
				CairoDockClassAppli *pOtherClassAppli = _cairo_dock_lookup_class_appli (icon->cClass);
				if (! pOtherClassAppli || ! pOtherClassAppli->pAppliOfClass)  // should never happen
					continue;
				
				Icon *pOldestAppli = g_list_last (pOtherClassAppli->pAppliOfClass)->data;  // prepend
				
				// compare to our class.
				if (pOldestAppli->iAge < iAge)  // it's older than our class -> skip this whole class, we'll go after.
				{
					while (ic->next != NULL)
					{
						icon = ic->next->data;
						if (icon->cClass && strcmp (icon->cClass, pOldestAppli->cClass) == 0)  // next icon is of the same class -> skip
							ic = ic->next;
						else
							break;
					}
					last_appli_ic = ic;
				}
				else  // we are older -> discard and quit.
				{
					break;
				}
			}
			
			if (last_appli_ic == NULL)  // we are the oldest class -> go just before the first appli
			{
				pIcon->fOrder = _get_previous_order (first_appli_ic);
			}
			else  // go just after the last one
			{
				pIcon->fOrder = _get_next_order (last_appli_ic);
			}
		}
		else  // no appli, use the defined placement.
		{
			pIcon->fOrder = _get_first_appli_order (pDock, first_launcher_ic, last_launcher_ic);
		}
	}
}


static inline CairoDockClassAppli *_get_class_appli_with_attributes (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (! pClassAppli->bSearchedAttributes)
	{
		gchar *cClass2 = cairo_dock_register_class (cClass);
		g_free (cClass2);
	}
	return pClassAppli;
}
const gchar *cairo_dock_get_class_command (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _get_class_appli_with_attributes (cClass);
	return pClassAppli->cCommand;
}

const gchar *cairo_dock_get_class_name (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _get_class_appli_with_attributes (cClass);
	return pClassAppli->cName;
}

const gchar **cairo_dock_get_class_mimetypes (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _get_class_appli_with_attributes (cClass);
	return (const gchar **)pClassAppli->pMimeTypes;
}

const gchar *cairo_dock_get_class_desktop_file (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _get_class_appli_with_attributes (cClass);
	return pClassAppli->cDesktopFile;
}

const gchar *cairo_dock_get_class_icon (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _get_class_appli_with_attributes (cClass);
	return pClassAppli->cIcon;
}

const GList *cairo_dock_get_class_menu_items (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _get_class_appli_with_attributes (cClass);
	return pClassAppli->pMenuItems;
}

const gchar *cairo_dock_get_class_wm_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _get_class_appli_with_attributes (cClass);
	return pClassAppli->cStartupWMClass;
}

const CairoDockImageBuffer *cairo_dock_get_class_image_buffer (const gchar *cClass)
{
	static CairoDockImageBuffer image;
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	Icon *pIcon;
	GList *ic;
	for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon) && pIcon->image.pSurface)  // avoid applets
		{
			memcpy (&image, &pIcon->image, sizeof (CairoDockImageBuffer));
			return &image;
		}
	}
	for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->image.pSurface)
		{
			memcpy (&image, &pIcon->image, sizeof (CairoDockImageBuffer));
			return &image;
		}
	}
	
	return NULL;
}


static gchar *_search_desktop_file (const gchar *cDesktopFile)  // file, path or even class
{
	if (cDesktopFile == NULL)
		return NULL;
	if (*cDesktopFile == '/' && g_file_test (cDesktopFile, G_FILE_TEST_EXISTS))  // it's a path and it exists.
	{
		return g_strdup (cDesktopFile);
	}
	
	gchar *cDesktopFileName = NULL;
	if (*cDesktopFile == '/')
		cDesktopFileName = g_path_get_basename (cDesktopFile);
	else if (! g_str_has_suffix (cDesktopFile, ".desktop"))
		cDesktopFileName = g_strdup_printf ("%s.desktop", cDesktopFile);
	
	const gchar *cFileName = (cDesktopFileName ? cDesktopFileName : cDesktopFile);
	gboolean bFound = TRUE;
	GString *sDesktopFilePath = g_string_new ("");
	g_string_printf (sDesktopFilePath, "/usr/share/applications/%s", cFileName);
	if (! g_file_test (sDesktopFilePath->str, G_FILE_TEST_EXISTS))
	{
		g_string_printf (sDesktopFilePath, "/usr/share/applications/%c%s", g_ascii_toupper (*cFileName), cFileName+1);  // handle stupid cases like Thunar.desktop
		if (! g_file_test (sDesktopFilePath->str, G_FILE_TEST_EXISTS))
		{
			g_string_printf (sDesktopFilePath, "/usr/share/applications/xfce4/%s", cFileName);
			if (! g_file_test (sDesktopFilePath->str, G_FILE_TEST_EXISTS))
			{
				g_string_printf (sDesktopFilePath, "/usr/share/applications/kde4/%s", cFileName);
				if (! g_file_test (sDesktopFilePath->str, G_FILE_TEST_EXISTS))
				{
					g_string_printf (sDesktopFilePath, "%s/.local/share/applications/%s", g_getenv ("HOME"), cFileName);
					if (! g_file_test (sDesktopFilePath->str, G_FILE_TEST_EXISTS))
					{
						bFound = FALSE;
					}
				}
			}
		}
	}
	g_free (cDesktopFileName);
	
	gchar *cResult;
	if (bFound)
	{
		cResult = sDesktopFilePath->str;
		g_string_free (sDesktopFilePath, FALSE);
	}
	else
	{
		cResult = NULL;
		g_string_free (sDesktopFilePath, TRUE);
	}
	return cResult;
}

gchar *cairo_dock_guess_class (const gchar *cCommand, const gchar *cStartupWMClass)
{
	// plusieurs cas sont possibles :
	// Exec=toto
	// Exec=toto-1.2
	// Exec=toto -x -y
	// Exec=/path/to/toto -x -y
	// Exec=gksu nautilus /  (or kdesu)
	// Exec=su-to-root -X -c /usr/sbin/synaptic
	// Exec=gksu --description /usr/share/applications/synaptic.desktop /usr/sbin/synaptic
	// Exec=wine "C:\Program Files\Starcraft\Starcraft.exe"
	// Exec=wine "/path/to/prog.exe"
	// Exec=env WINEPREFIX="/home/fab/.wine" wine "C:\Program Files\Starcraft\Starcraft.exe"
	
	cd_debug ("%s (%s, '%s')", __func__, cCommand, cStartupWMClass);
	gchar *cResult = NULL;
	if (cStartupWMClass == NULL || *cStartupWMClass == '\0' || g_strcmp0 (cStartupWMClass, "Wine") == 0)  // on force pour wine, car meme si la classe est explicitement definie en tant que "Wine", cette information est inexploitable.
	{
		if (cCommand == NULL || *cCommand == '\0')
			return NULL;
		gchar *cDefaultClass = g_ascii_strdown (cCommand, -1);
		gchar *str;
		const gchar *cClass = cDefaultClass;  // pointer to the current class.
		
		if (strncmp (cClass, "gksu", 4) == 0 || strncmp (cClass, "kdesu", 5) == 0 || strncmp (cClass, "su-to-root", 10) == 0)  // on prend la fin.
		{
			str = (gchar*)cClass + strlen(cClass) - 1;  // last char.
			while (*str == ' ')  // par securite on enleve les espaces en fin de ligne.
				*(str--) = '\0';
			str = strchr (cClass, ' ');  // on cherche le premier espace.
			if (str != NULL)  // on prend apres.
			{
				while (*str == ' ')
					str ++;
				cClass = str;
			}  // la on a vire le gksu.
			if (*cClass == '-')  // option, on prend le dernier argument de la commande.
			{
				str = strrchr (cClass, ' ');  // on cherche le dernier espace.
				if (str != NULL)  // on prend apres.
					cClass = str + 1;
			}
			else  // on prend le premier argument.
			{
				str = strchr (cClass, ' ');  // on cherche le premier espace.
				if (str != NULL)  // on vire apres.
					*str = '\0';
			}
			
			str = strrchr (cClass, '/');  // on cherche le dernier '/'.
			if (str != NULL)  // on prend apres.
				cClass = str + 1;
		}
		else if ((str = g_strstr_len (cClass, -1, "wine ")) != NULL)
		{
			cClass = str;  // on met deja la classe a "wine", c'est mieux que rien.
			*(str+4) = '\0';
			str += 5;
			while (*str == ' ')  // on enleve les espaces supplementaires.
				str ++;
			gchar *exe = g_strstr_len (str, -1, ".exe");  // on cherche a isoler le nom de l'executable, puisque wine l'utilise dans le res_name.
			if (!exe)
				exe = g_strstr_len (str, -1, ".EXE");
			if (exe)
			{
				*exe = '\0';  // vire l'extension par la meme occasion.
				gchar *slash = strrchr (str, '\\');
				if (slash)
					cClass = slash+1;
				else
				{
					slash = strrchr (str, '/');
					if (slash)
						cClass = slash+1;
					else
						cClass = str;
				}
			}
			cd_debug ("  special case : wine application => class = '%s'", cClass);
		}
		else
		{
			while (*cClass == ' ')  // par securite on enleve les espaces en debut de ligne.
				cClass ++;
			str = strchr (cClass, ' ');  // on cherche le premier espace.
			if (str != NULL)  // on vire apres.
				*str = '\0';
			str = strrchr (cClass, '/');  // on cherche le dernier '/'.
			if (str != NULL)  // on prend apres.
				cClass = str + 1;
			str = strchr (cClass, '.');  // on vire les .xxx, sinon on ne sait pas detecter l'absence d'extension quand on cherche l'icone (openoffice.org), ou tout simplement ca empeche de trouver l'icone (jbrout.py).
			if (str != NULL && str != cClass)
				*str = '\0';
		}
		
		// handle the cases of programs where command != class.
		if (*cClass != '\0')
		{
			if (strncmp (cClass, "oo", 2) == 0)
			{
				if (strcmp (cClass, "ooffice") == 0 || strcmp (cClass, "oowriter") == 0 || strcmp (cClass, "oocalc") == 0 || strcmp (cClass, "oodraw") == 0 || strcmp (cClass, "ooimpress") == 0)  // openoffice poor design: there is no way to bind its windows to the launcher without this trick.
					cClass = "openoffice";
			}
			else if (strncmp (cClass, "libreoffice", 11) == 0)  // libreoffice has different classes according to the launching option (--writer => libreoffice-writer, --calc => libreoffice-calc, etc)
			{
				gchar *str = strchr (cCommand, ' ');
				if (str && *(str+1) == '-')
				{
					g_free (cDefaultClass);
					cDefaultClass = g_strdup_printf ("%s%s", "libreoffice", str+2);
					str = strchr (cDefaultClass, ' ');  // remove the additionnal params of the command.
					if (str)
						*str = '\0';
					cClass = cDefaultClass;  // "libreoffice-writer"
				}
			}
			cResult = g_strdup (cClass);
		}
		g_free (cDefaultClass);
	}
	else
	{
		cResult = g_ascii_strdown (cStartupWMClass, -1);
		gchar *str = strchr (cResult, '.');  // on vire les .xxx, sinon on ne sait pas detecter l'absence d'extension quand on cherche l'icone (openoffice.org), ou tout simplement ca empeche de trouver l'icone (jbrout.py).
		if (str != NULL)
			*str = '\0';
	}
	cairo_dock_remove_version_from_string (cResult);
	cd_debug (" -> '%s'", cResult);
	
	return cResult;
}

static void _add_action_menus (GKeyFile *pKeyFile, CairoDockClassAppli *pClassAppli, const gchar *cGettextDomain, const gchar *cMenuListKey, const gchar *cMenuGroup, gboolean bActionFirstInGroupKey)
{
	gsize length = 0;
	gchar **pMenuList = g_key_file_get_string_list (pKeyFile, "Desktop Entry", cMenuListKey, &length, NULL);  
	if (pMenuList != NULL)
	{
		gchar *cGroup;
		int i;
		for (i = 0; pMenuList[i] != NULL; i++)
		{
			cGroup = g_strdup_printf ("%s %s",
				bActionFirstInGroupKey ? pMenuList[i] : cMenuGroup,   // [NewWindow Shortcut Group]
				bActionFirstInGroupKey ? cMenuGroup : pMenuList [i]); // [Desktop Action NewWindow]

			if (g_key_file_has_group (pKeyFile, cGroup))
			{
				gchar **pMenuItem = g_new0 (gchar*, 4);
				// for a few apps, the translations are directly available in the .desktop file (e.g. firefox)
				gchar *cName = g_key_file_get_locale_string (pKeyFile, cGroup, "Name", NULL, NULL);
				pMenuItem[0] = g_strdup (dgettext (cGettextDomain, cName)); // but most of the time, it's available in the .mo file
				g_free (cName);
				gchar *cCommand = g_key_file_get_string (pKeyFile, cGroup, "Exec", NULL);
				if (cCommand != NULL)  // remove the launching options %x.
				{
					gchar *str = strchr (cCommand, '%');  // search the first one.
					if (str != NULL)
					{
						if (str != cCommand && (*(str-1) == '"' || *(str-1) == '\''))  // take care of "" around the option.
							str --;
						*str = '\0';  // il peut rester un espace en fin de chaine, ce n'est pas grave.
					}
				}
				pMenuItem[1] = cCommand;
				pMenuItem[2] = g_key_file_get_string (pKeyFile, cGroup, "Icon", NULL);
				
				pClassAppli->pMenuItems = g_list_append (pClassAppli->pMenuItems, pMenuItem);
			}
			g_free (cGroup);
		}
		g_strfreev (pMenuList);
	}
}

/*
register from desktop-file name/path (+class-name):
  if class-name: guess class -> lookup class -> if already registered => quit
  search complete path -> not found => abort
  get main info from file (Exec, StartupWMClass)
  if class-name NULL: guess class from Exec+StartupWMClass
  if already registered => quit
  make new class
  get additional params from file (MimeType, Icon, etc) and store them in the class

register from class name (window or old launchers):
  guess class -> lookup class -> if already registered => quit
  search complete path -> not found => abort
  make new class
  get additional params from file (MimeType, Icon, etc) and store them in the class
*/
gchar *cairo_dock_register_class_full (const gchar *cDesktopFile, const gchar *cClassName, const gchar *cWmClass)
{
	g_return_val_if_fail (cDesktopFile != NULL || cClassName != NULL, NULL);
	//g_print ("%s (%s, %s, %s)\n", __func__, cDesktopFile, cClassName, cWmClass);
	
	//\__________________ if the class is already registered and filled, quit.
	gchar *cClass = NULL;
	if (cClassName != NULL)
		cClass = cairo_dock_guess_class (NULL, cClassName);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass?cClass:cDesktopFile);
	
	if (pClassAppli != NULL && pClassAppli->bSearchedAttributes && pClassAppli->cDesktopFile)  // we already searched this class, and we did find its .desktop file, so let's end here.
	{
		//g_print ("class %s already known (%s)\n", cClass?cClass:cDesktopFile, pClassAppli->cDesktopFile);
		if (pClassAppli->cStartupWMClass == NULL && cWmClass != NULL)  // if the cStartupWMClass was not defined in the .desktop file, store it now.
			pClassAppli->cStartupWMClass = g_strdup (cWmClass);
		return (cClass?cClass:g_strdup (cDesktopFile));
	}
	
	//\__________________ search the desktop file's path.
	gchar *cDesktopFilePath = _search_desktop_file (cDesktopFile?cDesktopFile:cClass);
	if (cDesktopFilePath == NULL)  // couldn't find the .desktop
	{
		if (cClass != NULL)  // make a class anyway to store the few info we have.
		{
			if (pClassAppli == NULL)
				pClassAppli = cairo_dock_get_class (cClass);
			if (pClassAppli != NULL)
			{
				if (pClassAppli->cStartupWMClass == NULL && cWmClass != NULL)
					pClassAppli->cStartupWMClass = g_strdup (cWmClass);
				pClassAppli->bSearchedAttributes = TRUE;
			}
		}
		cd_debug ("couldn't find the desktop file %s", cDesktopFile?cDesktopFile:cClass);
		return cClass;  /// NULL
	}
	
	//\__________________ open it.
	cd_debug ("+ parsing class desktop file %s...", cDesktopFilePath);
	GKeyFile* pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	//\__________________ guess the class name.
	gchar *cCommand = g_key_file_get_string (pKeyFile, "Desktop Entry", "Exec", NULL);
	gchar *cStartupWMClass = g_key_file_get_string (pKeyFile, "Desktop Entry", "StartupWMClass", NULL);
	if (cStartupWMClass && *cStartupWMClass == '\0')
	{
		g_free (cStartupWMClass);
		cStartupWMClass = NULL;
	}
	if (cClass == NULL)
		cClass = cairo_dock_guess_class (cCommand, cStartupWMClass);
	if (cClass == NULL)
	{
		cd_debug ("couldn't guess the class for %s", cDesktopFile);
		g_free (cDesktopFilePath);
		g_free (cCommand);
		g_free (cStartupWMClass);
		return NULL;
	}
	
	//\__________________ make a new class or get the existing one.
	pClassAppli = cairo_dock_get_class (cClass);
	g_return_val_if_fail (pClassAppli!= NULL, NULL);
	
	//\__________________ if we already searched and found the attributes beforehand, quit.
	if (pClassAppli->bSearchedAttributes && pClassAppli->cDesktopFile)
	{
		if (pClassAppli->cStartupWMClass == NULL && cWmClass != NULL)  // we already searched this class before, but we couldn't have its WM class.
			pClassAppli->cStartupWMClass = g_strdup (cWmClass);
		g_free (cDesktopFilePath);
		g_free (cCommand);
		g_free (cStartupWMClass);
		return cClass;
	}
	pClassAppli->bSearchedAttributes = TRUE;
	
	//\__________________ get the attributes.
	pClassAppli->cDesktopFile = cDesktopFilePath;
	
	pClassAppli->cName = g_key_file_get_locale_string (pKeyFile, "Desktop Entry", "Name", NULL, NULL);
	
	if (cCommand != NULL)  // remove the launching options %x.
	{
		gchar *str = strchr (cCommand, '%');  // search the first one.
		
		if (str && *(str+1) == 'c')  // this one (caption) is the only one that is expected (ex.: kreversi -caption "%c"; if we let '-caption' with nothing after, the appli will melt down); others are either URL or icon that can be empty as per the freedesktop specs, so we can sefely remove them completely from the command line.
		{
			*str = '\0';
			gchar *cmd2 = g_strdup_printf ("%s%s%s", cCommand, pClassAppli->cName, str+2);  // replace %c with the localized name.
			g_free (cCommand);
			cCommand = cmd2;
			str = strchr (cCommand, '%');  // jump to the next one.
		}
		
		if (str != NULL)  // remove everything from the first option to the end.
		{
			if (str != cCommand && (*(str-1) == '"' || *(str-1) == '\''))  // take care of "" around the option.
				str --;
			*str = '\0';  // il peut rester un espace en fin de chaine, ce n'est pas grave.
		}
	}
	pClassAppli->cCommand = cCommand;
	
	if (pClassAppli->cStartupWMClass == NULL)
		pClassAppli->cStartupWMClass = (cStartupWMClass ? cStartupWMClass : g_strdup (cWmClass));
	
	pClassAppli->cIcon = g_key_file_get_string (pKeyFile, "Desktop Entry", "Icon", NULL);
	if (pClassAppli->cIcon != NULL && *pClassAppli->cIcon != '/')  // remove any extension.
	{
		gchar *str = strrchr (pClassAppli->cIcon, '.');
		if (str && (strcmp (str+1, "png") == 0 || strcmp (str+1, "svg") == 0 || strcmp (str+1, "xpm") == 0))
			*str = '\0';
	}
	
	gsize length = 0;
	pClassAppli->pMimeTypes = g_key_file_get_string_list (pKeyFile, "Desktop Entry", "MimeType", &length, NULL);
	
	pClassAppli->cWorkingDirectory = g_key_file_get_string (pKeyFile, "Desktop Entry", "Path", NULL);

	//\__________________ Gettext domain
	// The translations of the quicklist menus are generally available in a .mo file
	gchar *cGettextDomain = g_key_file_get_string (pKeyFile, "Desktop Entry", "X-Ubuntu-Gettext-Domain", NULL);
	if (cGettextDomain == NULL)
		cGettextDomain = g_key_file_get_string (pKeyFile, "Desktop Entry", "X-GNOME-Gettext-Domain", NULL); // Yes, they like doing that :P
		// a few time ago, it seems that it was 'X-Gettext-Domain'

	//______________ Quicklist menus.
	_add_action_menus (pKeyFile, pClassAppli, cGettextDomain, "X-Ayatana-Desktop-Shortcuts", "Shortcut Group", TRUE); // oh crap, with a name like that you can be sure it will change 25 times before they decide a definite name :-/
	_add_action_menus (pKeyFile, pClassAppli, cGettextDomain, "Actions", "Desktop Action", FALSE); // yes, it's true ^^ => Ubuntu Quantal

	g_free (cGettextDomain);
	
	g_key_file_free (pKeyFile);
	cd_debug (" -> class '%s'", cClass);
	return cClass;
}

void cairo_dock_set_data_from_class (const gchar *cClass, Icon *pIcon)
{
	g_return_if_fail (cClass != NULL && pIcon != NULL);
	cd_debug ("%s (%s)", __func__, cClass);
	
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli == NULL || ! pClassAppli->bSearchedAttributes)
	{
		cd_debug ("no class %s or no attributes", cClass);
		return;
	}
	
	if (pIcon->cCommand == NULL)
		pIcon->cCommand = g_strdup (pClassAppli->cCommand);
	
	if (pIcon->cWorkingDirectory == NULL)
		pIcon->cWorkingDirectory = g_strdup (pClassAppli->cWorkingDirectory);
	
	if (pIcon->cName == NULL)
		pIcon->cName = g_strdup (pClassAppli->cName);
	
	if (pIcon->cFileName == NULL)
		pIcon->cFileName = g_strdup (pClassAppli->cIcon);
	
	if (pIcon->pMimeTypes == NULL)
		pIcon->pMimeTypes = g_strdupv ((gchar**)pClassAppli->pMimeTypes);	
}


