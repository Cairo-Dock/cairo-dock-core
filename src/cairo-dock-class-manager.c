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

#include "cairo-dock-icons.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-config.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-load.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-class-manager.h"

extern CairoDock *g_pMainDock;

static GHashTable *s_hClassTable = NULL;


void cairo_dock_initialize_class_manager (void)
{
	if (s_hClassTable == NULL)
		s_hClassTable = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			g_free,
			(GDestroyNotify) cairo_dock_free_class_appli);
}


static CairoDockClassAppli *cairo_dock_find_class_appli (const gchar *cClass)
{
	return (cClass != NULL ? g_hash_table_lookup (s_hClassTable, cClass) : NULL);
}

const GList *cairo_dock_list_existing_appli_with_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	
	CairoDockClassAppli *pClassAppli = cairo_dock_find_class_appli (cClass);
	return (pClassAppli != NULL ? pClassAppli->pAppliOfClass : NULL);
}


static void _cairo_dock_set_same_indicator_on_sub_dock (Icon *pInhibhatorIcon)
{
	CairoDock *pInhibhatorDock = cairo_dock_search_dock_from_name (pInhibhatorIcon->cParentDockName);
	if (pInhibhatorDock != NULL && pInhibhatorDock->iRefCount > 0)  // l'inhibiteur est dans un sous-dock.
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
			for (ic =pInhibhatorDock->icons ; ic != NULL; ic = ic->next)
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
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pInhibhatorDock, &pParentDock);
		if (pPointingIcon != NULL && pPointingIcon->bHasIndicator != bSubDockHasIndicator)
		{
			cd_message ("  pour le sous-dock %s : indicateur <- %d", pPointingIcon->cName, bSubDockHasIndicator);
			pPointingIcon->bHasIndicator = bSubDockHasIndicator;
			if (pParentDock != NULL)
				cairo_dock_redraw_icon (pPointingIcon, CAIRO_CONTAINER (pParentDock));
		}
	}
}

void cairo_dock_free_class_appli (CairoDockClassAppli *pClassAppli)
{
	GList *pElement;
	Icon *pInhibatorIcon;
	for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
	{
		pInhibatorIcon = pElement->data;
		cd_message ("%s perd sa mana", pInhibatorIcon->cName);
		pInhibatorIcon->Xid = 0;
		pInhibatorIcon->bHasIndicator = FALSE;
		_cairo_dock_set_same_indicator_on_sub_dock (pInhibatorIcon);
	}
	g_list_free (pClassAppli->pIconsOfClass);
	g_list_free (pClassAppli->pAppliOfClass);
	g_free (pClassAppli);
}

static CairoDockClassAppli *cairo_dock_get_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	
	CairoDockClassAppli *pClassAppli = cairo_dock_find_class_appli (cClass);
	if (pClassAppli == NULL)
	{
		pClassAppli = g_new0 (CairoDockClassAppli, 1);
		g_hash_table_insert (s_hClassTable, g_strdup (cClass), pClassAppli);
	}
	return pClassAppli;
}

gboolean cairo_dock_add_inhibator_to_class (const gchar *cClass, Icon *pIcon)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	g_return_val_if_fail (pClassAppli!= NULL, FALSE);
	
	g_return_val_if_fail (g_list_find (pClassAppli->pIconsOfClass, pIcon) == NULL, TRUE);
	pClassAppli->pIconsOfClass = g_list_prepend (pClassAppli->pIconsOfClass, pIcon);
	
	return TRUE;
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
		if (pDock != NULL)
		{
			cairo_dock_reload_one_icon_buffer_in_dock (pAppliIcon, pDock);
		}
		else
		{
			cairo_dock_fill_one_icon_buffer (pAppliIcon, (1 + myIcons.fAmplitude), g_pMainDock->container.bIsHorizontal, g_pMainDock->container.bDirectionUp);
		}
	}
	
	return TRUE;
}


static Window _cairo_dock_detach_appli_of_class (const gchar *cClass, gboolean bDetachAll)
{
	g_return_val_if_fail (cClass != NULL, 0);
	
	const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
	Icon *pIcon;
	const GList *pElement;
	gboolean bNeedsRedraw = FALSE, bDetached;
	CairoDock *pParentDock;
	Window XFirstFoundId = 0;
	for (pElement = pList; pElement != NULL; pElement = pElement->next)
	{
		pIcon = pElement->data;
		pParentDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
		if (pParentDock == NULL)  // pas dans un dock => rien a faire.
			continue;
		
		cd_debug ("detachement de l'icone %s (%d;%d)", pIcon->cName, bDetachAll, XFirstFoundId);
		gchar *cParentDockName = pIcon->cParentDockName;
		pIcon->cParentDockName = NULL;  // astuce.
		bDetached = cairo_dock_detach_icon_from_dock (pIcon, pParentDock, myIcons.iSeparateIcons);
		if (bDetached)  // detachee => on met a jour son dock.
		{
			if (! pParentDock->bIsMainDock)  // sous-dock de classe => on le met a jour / detruit.
			{
				if (pParentDock->icons == NULL)  // devient vide => on le detruit.
				{
					if (pParentDock->iRefCount != 0)  // on vire l'icone de paille qui pointe sur ce sous-dock.
					{
						CairoDock *pMainDock=NULL;
						Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pParentDock, &pMainDock);
						if (pMainDock && pPointingIcon && pPointingIcon->cDesktopFileName == NULL)
						{
							cairo_dock_remove_icon_from_dock (pMainDock, pPointingIcon);
							bNeedsRedraw |= pMainDock->bIsMainDock;
							cairo_dock_free_icon (pPointingIcon);
						}
					}
					cairo_dock_destroy_dock (pParentDock, cParentDockName);
				}
				else  // non vide => on le met a jour.
					cairo_dock_update_dock_size (pParentDock);
			}
			else  // main dock => on le met a jour a la fin.
				bNeedsRedraw = TRUE;
		}
		g_free (cParentDockName);
		
		if (XFirstFoundId == 0)  // on recupere la 1ere appli de la classe.
		{
			XFirstFoundId = pIcon->Xid;
			if (! bDetachAll)
				break;
		}
	}
	if (! cairo_dock_is_loading () && bNeedsRedraw)  // mise a jour du main dock en 1 coup.
	{
		cairo_dock_update_dock_size (g_pMainDock);
		cairo_dock_calculate_dock_icons (g_pMainDock);
		gtk_widget_queue_draw (g_pMainDock->container.pWidget);
	}
	return XFirstFoundId;
}
gboolean cairo_dock_inhibate_class (const gchar *cClass, Icon *pInhibatorIcon)
{
	g_return_val_if_fail (cClass != NULL, FALSE);
	cd_message ("%s (%s)", __func__, cClass);
	
	if (! cairo_dock_add_inhibator_to_class (cClass, pInhibatorIcon))  // on l'insere avant pour que les icones des applis puissent le trouver et prendre sa surface si necessaire.
		return FALSE;
	
	Window XFirstFoundId = _cairo_dock_detach_appli_of_class (cClass, (TRUE));
	if (pInhibatorIcon != NULL)
	{
		pInhibatorIcon->Xid = XFirstFoundId;
		pInhibatorIcon->bHasIndicator = (XFirstFoundId > 0);
		_cairo_dock_set_same_indicator_on_sub_dock (pInhibatorIcon);
		if (pInhibatorIcon->cClass != cClass)
		{
			g_free (pInhibatorIcon->cClass);
			pInhibatorIcon->cClass = g_strdup (cClass);
		}
		
		const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
		Icon *pIcon;
		const GList *pElement;
		for (pElement = pList; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			cd_debug ("une appli detachee (%s)", pIcon->cParentDockName);
			if (pIcon->Xid != XFirstFoundId && pIcon->cParentDockName == NULL)  // s'est faite detacher et doit etre rattacher.
				cairo_dock_insert_appli_in_dock (pIcon, g_pMainDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
		}
	}
	
	return TRUE;
}

gboolean cairo_dock_class_is_inhibated (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_find_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->pIconsOfClass != NULL);
}

gboolean cairo_dock_class_is_using_xicon (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_find_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->bUseXIcon);  // si pClassAppli == NULL, il n'y a pas de lanceur pouvant lui filer son icone, mais on peut en trouver une dans le theme d'icones systeme.
}

gboolean cairo_dock_class_is_expanded (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_find_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->bExpand);
}

gboolean cairo_dock_prevent_inhibated_class (Icon *pIcon)
{
	g_return_val_if_fail (pIcon != NULL, FALSE);
	//g_print ("%s (%s)\n", __func__, pIcon->cClass);
	
	gboolean bToBeInhibited = FALSE;
	CairoDockClassAppli *pClassAppli = cairo_dock_find_class_appli (pIcon->cClass);
	if (pClassAppli != NULL)
	{
		Icon *pInhibatorIcon;
		GList *pElement;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibatorIcon = pElement->data;
			if (pInhibatorIcon != NULL)  // un inhibiteur est present.
			{
				if (pInhibatorIcon->Xid == 0 && pInhibatorIcon->pSubDock == NULL)  // cette icone inhibe cette classe mais ne controle encore aucune appli, on s'y asservit.
				{
					pInhibatorIcon->Xid = pIcon->Xid;
					pInhibatorIcon->bIsHidden = pIcon->bIsHidden;
					cd_message (">>> %s prendra un indicateur au prochain redraw ! (Xid : %d)", pInhibatorIcon->cName, pInhibatorIcon->Xid);
					pInhibatorIcon->bHasIndicator = TRUE;
					_cairo_dock_set_same_indicator_on_sub_dock (pInhibatorIcon);
				}
				
				if (pInhibatorIcon->Xid == pIcon->Xid)  // cette icone nous controle.
				{
					CairoDock *pInhibhatorDock = cairo_dock_search_dock_from_name (pInhibatorIcon->cParentDockName);
					//\______________ On place l'icone pour X.
					if (! bToBeInhibited)  // on ne met le thumbnail que sur la 1ere.
					{
						if (pInhibhatorDock != NULL)
						{
							//g_print ("on positionne la miniature sur l'inhibiteur %s\n", pInhibatorIcon->cName);
							cairo_dock_set_one_icon_geometry_for_window_manager (pInhibatorIcon, pInhibhatorDock);
						}
						bToBeInhibited = TRUE;
					}
					//\______________ On met a jour l'etiquette de l'inhibiteur.
					if (pInhibhatorDock != NULL && pIcon->cName != NULL)
					{
						if (pInhibatorIcon->cInitialName == NULL)
							pInhibatorIcon->cInitialName = pInhibatorIcon->cName;
						else
							g_free (pInhibatorIcon->cName);
						pInhibatorIcon->cName = NULL;
						cairo_dock_set_icon_name (pIcon->cName, pInhibatorIcon, CAIRO_CONTAINER (pInhibhatorDock));
					}
				}
			}
		}
	}
	return bToBeInhibited;
}


gboolean cairo_dock_remove_icon_from_class (Icon *pInhibatorIcon)
{
	g_return_val_if_fail (pInhibatorIcon != NULL, FALSE);
	cd_message ("%s (%s)", __func__, pInhibatorIcon->cClass);
	
	gboolean bStillInhibated = FALSE;
	CairoDockClassAppli *pClassAppli = cairo_dock_find_class_appli (pInhibatorIcon->cClass);
	if (pClassAppli != NULL)
	{
		pClassAppli->pIconsOfClass = g_list_remove (pClassAppli->pIconsOfClass, pInhibatorIcon);
		if (pClassAppli->pIconsOfClass == NULL && pClassAppli->pAppliOfClass == NULL && ! pClassAppli->bUseXIcon)  // cette classe ne sert plus a rien.
		{
			cd_message ("  cette classe n'a plus d'interet");
			g_hash_table_remove (s_hClassTable, pInhibatorIcon->cClass);  // detruit pClassAppli.
			bStillInhibated = FALSE;
		}
		else
			bStillInhibated = (pClassAppli->pIconsOfClass != NULL);
	}
	return bStillInhibated;
}

void cairo_dock_deinhibate_class (const gchar *cClass, Icon *pInhibatorIcon)
{
	cd_message ("%s (%s)", __func__, cClass);
	gboolean bStillInhibated = cairo_dock_remove_icon_from_class (pInhibatorIcon);
	cd_debug (" bStillInhibated : %d", bStillInhibated);
	///if (! bStillInhibated)  // il n'y a plus personne dans cette classe.
	///	return ;
	
	if (pInhibatorIcon == NULL || pInhibatorIcon->Xid != 0)
	{
		const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
		Icon *pIcon;
		gboolean bNeedsRedraw = FALSE;
		CairoDock *pParentDock;
		const GList *pElement;
		for (pElement = pList; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			if (pInhibatorIcon == NULL || pIcon->Xid == pInhibatorIcon->Xid)
			{
				cd_message ("rajout de l'icone precedemment inhibee (Xid:%d)", pIcon->Xid);
				pIcon->fInsertRemoveFactor = 0;
				pIcon->fScale = 1.;
				pParentDock = cairo_dock_insert_appli_in_dock (pIcon, g_pMainDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
				bNeedsRedraw = (pParentDock != NULL && pParentDock->bIsMainDock);
				//if (pInhibatorIcon != NULL)
				//	break ;
			}
			pParentDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
			if (pParentDock != NULL)
			{
				cd_message ("on recharge l'icone de l'appli %s", pIcon->cName);
				cairo_dock_reload_one_icon_buffer_in_dock (pIcon, pParentDock);
			}
			else
			{
				cairo_dock_reload_one_icon_buffer_in_dock (pIcon, g_pMainDock);
			}
		}
		if (bNeedsRedraw)
			gtk_widget_queue_draw (g_pMainDock->container.pWidget);  /// pDock->pRenderer->calculate_icons (pDock); ?...
	}
	if (pInhibatorIcon != NULL)
	{
		cd_message (" l'inhibiteur a perdu toute sa mana");
		pInhibatorIcon->Xid = 0;
		pInhibatorIcon->bHasIndicator = FALSE;
		g_free (pInhibatorIcon->cClass);
		pInhibatorIcon->cClass = NULL;
		cd_debug ("  plus de classe");
	}
}


void cairo_dock_update_Xid_on_inhibators (Window Xid, const gchar *cClass)
{
	cd_message ("%s (%s)", __func__, cClass);
	CairoDockClassAppli *pClassAppli = cairo_dock_find_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		int iNextXid = -1;
		Icon *pSameClassIcon = NULL;
		Icon *pIcon;
		GList *pElement;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			if (pIcon->Xid == Xid)
			{
				if (iNextXid == -1)  // on prend la 1ere appli de meme classe.
				{
					GList *pList = pClassAppli->pAppliOfClass;
					Icon *pOneIcon;
					GList *ic;
					for (ic = pList; ic != NULL; ic = ic->next)
					{
						pOneIcon = ic->data;
						if (pOneIcon != NULL && ! cairo_dock_icon_is_being_removed (pOneIcon) && pOneIcon->Xid != Xid)  // la 2eme condition est a priori toujours vraie.
						{
							pSameClassIcon = pOneIcon;
							break ;
						}
					}
					iNextXid = (pSameClassIcon != NULL ? pSameClassIcon->Xid : 0);
					if (pSameClassIcon != NULL)
					{
						cd_message ("  c'est %s qui va la remplacer", pSameClassIcon->cName);
						CairoDock *pClassSubDock = cairo_dock_search_dock_from_name (pSameClassIcon->cParentDockName);
						if (pClassSubDock != NULL)
						{
							cairo_dock_detach_icon_from_dock (pSameClassIcon, pClassSubDock, myIcons.iSeparateIcons);
							if (pClassSubDock->icons == NULL && pClassSubDock == cairo_dock_search_dock_from_name (cClass))  // le sous-dock de la classe devient vide.
								cairo_dock_destroy_dock (pClassSubDock, cClass);
							else
								cairo_dock_update_dock_size (pClassSubDock);
						}
					}
				}
				pIcon->Xid = iNextXid;
				pIcon->bHasIndicator = (iNextXid != 0);
				_cairo_dock_set_same_indicator_on_sub_dock (pIcon);
				cd_message (" %s : bHasIndicator <- %d, Xid <- %d", pIcon->cName, pIcon->bHasIndicator, pIcon->Xid);
			}
		}
	}
}

static void _cairo_dock_remove_all_applis_from_class (gchar *cClass, CairoDockClassAppli *pClassAppli, gpointer data)
{
	g_list_free (pClassAppli->pAppliOfClass);
	pClassAppli->pAppliOfClass = NULL;
	
	Icon *pInhibatorIcon;
	GList *pElement;
	for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
	{
		pInhibatorIcon = pElement->data;
		pInhibatorIcon->bHasIndicator = FALSE;
		pInhibatorIcon->Xid = 0;
		_cairo_dock_set_same_indicator_on_sub_dock (pInhibatorIcon);
	}
}
void cairo_dock_remove_all_applis_from_class_table (void)
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_remove_all_applis_from_class, NULL);
}

void cairo_dock_reset_class_table (void)
{
	g_hash_table_remove_all (s_hClassTable);
}



cairo_surface_t *cairo_dock_duplicate_inhibator_surface_for_appli (Icon *pInhibatorIcon, int iWidth, int iHeight)
{
	CairoContainer *pInhibhatorContainer = cairo_dock_search_container_from_icon (pInhibatorIcon);
	int w, h;
	cairo_dock_get_icon_extent (pInhibatorIcon, pInhibhatorContainer, &w, &h);
	
	return cairo_dock_duplicate_surface (pInhibatorIcon->pIconBuffer,
		w,
		h,
		iWidth,
		iHeight);
}
cairo_surface_t *cairo_dock_create_surface_from_class (const gchar *cClass, int iWidth, int iHeight)
{
	cd_debug ("%s (%s)", __func__, cClass);
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		cd_debug ("bUseXIcon:%d", pClassAppli->bUseXIcon);
		if (pClassAppli->bUseXIcon)
			return NULL;
		
		GList *pElement;
		Icon *pInhibatorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibatorIcon = pElement->data;
			cd_debug ("  %s", pInhibatorIcon->cName);
			if (! CAIRO_DOCK_IS_APPLET (pInhibatorIcon))
			{
				cd_message ("%s va fournir genereusement sa surface", pInhibatorIcon->cName);
				return cairo_dock_duplicate_inhibator_surface_for_appli (pInhibatorIcon, iWidth, iHeight);
			}
		}
	}
	
	gchar *cIconFilePath = cairo_dock_search_icon_s_path (cClass);
	if (cIconFilePath != NULL)
	{
		cd_debug ("on remplace l'icone X par %s", cIconFilePath);
		cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
			iWidth,
			iHeight);
		g_free (cIconFilePath);
		return pSurface;
	}
	
	cd_debug ("classe %s prend l'icone X", cClass);
	
	return NULL;
}


void cairo_dock_update_visibility_on_inhibators (const gchar *cClass, Window Xid, gboolean bIsHidden)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibatorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibatorIcon = pElement->data;
			
			if (pInhibatorIcon->Xid == Xid)
			{
				cd_message (" %s aussi se %s", pInhibatorIcon->cName, (bIsHidden ? "cache" : "montre"));
				pInhibatorIcon->bIsHidden = bIsHidden;
				if (! CAIRO_DOCK_IS_APPLET (pInhibatorIcon) && myTaskBar.fVisibleAppliAlpha != 0)
				{
					CairoDock *pInhibhatorDock = cairo_dock_search_dock_from_name (pInhibatorIcon->cParentDockName);
					pInhibatorIcon->fAlpha = 1;  // on triche un peu.
					cairo_dock_redraw_icon (pInhibatorIcon, CAIRO_CONTAINER (pInhibhatorDock));
				}
			}
		}
	}
}

void cairo_dock_update_activity_on_inhibators (const gchar *cClass, Window Xid)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibatorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibatorIcon = pElement->data;
			
			if (pInhibatorIcon->Xid == Xid)
			{
				cd_message (" %s aussi devient active", pInhibatorIcon->cName);
				///pInhibatorIcon->bIsActive = TRUE;
				CairoDock *pParentDock = cairo_dock_search_dock_from_name (pInhibatorIcon->cParentDockName);
				if (pParentDock != NULL)
					cairo_dock_animate_icon_on_active (pInhibatorIcon, pParentDock);
			}
		}
	}
}

void cairo_dock_update_inactivity_on_inhibators (const gchar *cClass, Window Xid)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibatorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibatorIcon = pElement->data;
			
			if (pInhibatorIcon->Xid == Xid)
			{
				cd_message (" %s aussi devient inactive", pInhibatorIcon->cName);
				///pInhibatorIcon->bIsActive = FALSE;
				CairoDock *pParentDock = cairo_dock_search_dock_from_name (pInhibatorIcon->cParentDockName);
				if (pParentDock != NULL && ! pParentDock->bIsShrinkingDown)
					cairo_dock_redraw_icon (pInhibatorIcon, CAIRO_CONTAINER (pParentDock));
			}
		}
	}
}

void cairo_dock_update_name_on_inhibators (const gchar *cClass, Window Xid, gchar *cNewName)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibatorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibatorIcon = pElement->data;
			
			if (pInhibatorIcon->Xid == Xid)
			{
				CairoDock *pParentDock = cairo_dock_search_dock_from_name (pInhibatorIcon->cParentDockName);
				if (pParentDock != NULL)
				{
					if (! CAIRO_DOCK_IS_APPLET (pInhibatorIcon))
					{
						cd_message (" %s change son nom en %s", pInhibatorIcon->cName, cNewName);
						if (pInhibatorIcon->cInitialName == NULL)
						{
							pInhibatorIcon->cInitialName = pInhibatorIcon->cName;
							cd_debug ("pInhibatorIcon->cInitialName <- %s", pInhibatorIcon->cInitialName);
						}
						else
							g_free (pInhibatorIcon->cName);
						pInhibatorIcon->cName = NULL;
						
						cairo_dock_set_icon_name ((cNewName != NULL ? cNewName : pInhibatorIcon->cInitialName), pInhibatorIcon, CAIRO_CONTAINER (pParentDock));
					}
					if (! pParentDock->bIsShrinkingDown)
						cairo_dock_redraw_icon (pInhibatorIcon, CAIRO_CONTAINER (pParentDock));
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
		if (pFriendIcon == NULL || pFriendIcon->cParentDockName == NULL)  // on ne prend pas les inhibiteurs situes dans un desklet.
			continue ;
		cd_debug (" friend : %s (%d)", pFriendIcon->cName, pFriendIcon->Xid);
		if (pFriendIcon->Xid != 0 || pFriendIcon->pSubDock != NULL)
			return pFriendIcon;
	}
	
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL; pElement = pElement->next)
	{
		pFriendIcon = pElement->data;
		if (pFriendIcon != pIcon && pFriendIcon->cParentDockName != NULL && strcmp (pFriendIcon->cParentDockName, CAIRO_DOCK_MAIN_DOCK_NAME) == 0)
			return pFriendIcon;
	}
	
	return NULL;
}




gboolean cairo_dock_check_class_subdock_is_empty (CairoDock *pDock, const gchar *cClass)
{
	cd_debug ("%s (%s, %d)", __func__, cClass, g_list_length (pDock->icons));
	if (pDock->iRefCount == 0)
		return FALSE;
	if (pDock->icons == NULL)  // ne devrait plus arriver.
	{
		cd_warning ("the %s class sub-dock has no element, which is probably an error !\nit will be destroyed.", cClass);
		CairoDock *pFakeParentDock = NULL;
		Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
		cairo_dock_destroy_dock (pDock, cClass);
		pFakeClassIcon->pSubDock = NULL;
		cairo_dock_remove_icon_from_dock (pFakeParentDock, pFakeClassIcon);
		cairo_dock_free_icon (pFakeClassIcon);
		cairo_dock_update_dock_size (pFakeParentDock);
		cairo_dock_calculate_dock_icons (pFakeParentDock);
		return TRUE;
	}
	else if (pDock->icons->next == NULL)
	{
		cd_debug ("   le sous-dock de la classe %s n'a plus que 1 element et va etre vide puis detruit", cClass);
		Icon *pLastClassIcon = pDock->icons->data;
		
		CairoDock *pFakeParentDock = NULL;
		Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
		g_return_val_if_fail (pFakeClassIcon != NULL, TRUE);
		if (CAIRO_DOCK_IS_FAKE_LAUNCHER (pFakeClassIcon))  // le sous-dock est pointe par une icone de paille.
		{
			cd_debug ("trouve l'icone en papier (%x;%x)", pFakeClassIcon, pFakeParentDock);
			cairo_dock_detach_icon_from_dock (pLastClassIcon, pDock, FALSE);
			g_free (pLastClassIcon->cParentDockName);
			pLastClassIcon->cParentDockName = g_strdup (pFakeClassIcon->cParentDockName);
			pLastClassIcon->fOrder = pFakeClassIcon->fOrder;
			
			cd_debug ("on enleve l'icone de paille");
			cairo_dock_remove_icon_from_dock (pFakeParentDock, pFakeClassIcon);  // a faire avant que l'icone n'ait plus de sous-dock, sinon elle est consideree comme un separateur auto, et du coup le separateur n'est pas enleve.
			
			cd_debug (" on detruit le sous-dock...");
			cairo_dock_destroy_dock (pDock, cClass);
			pFakeClassIcon->pSubDock = NULL;
			
			cd_debug ("on detruit l'icone de paille");
			cairo_dock_free_icon (pFakeClassIcon);
			
			cd_debug (" puis on re-insere l'appli restante");
			if (! cairo_dock_icon_is_being_removed (pLastClassIcon))
			{
				cairo_dock_insert_icon_in_dock (pLastClassIcon, pFakeParentDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
				cairo_dock_calculate_dock_icons (pFakeParentDock);
				cairo_dock_redraw_icon (pLastClassIcon, CAIRO_CONTAINER (pFakeParentDock));  // on suppose que les tailles des 2 icones sont identiques.
			}
			else  // la derniere icone est en cours de suppression, inutile de la re-inserer. (c'est souvent lorsqu'on ferme toutes une classe d'un coup. donc les animations sont pratiquement dans le meme etat, donc la derniere icone en est aussi a la fin, donc on ne verrait de toute facon aucune animation.
			{
				cd_debug ("inutile de re-inserer l'icone restante\n");
				cairo_dock_free_icon (pLastClassIcon);
				cairo_dock_update_dock_size (pFakeParentDock);
				cairo_dock_calculate_dock_icons (pFakeParentDock);
				cairo_dock_redraw_container (CAIRO_CONTAINER (pFakeParentDock));
			}
		}
		else  // le sous-dock est pointe par un inhibiteur (normal launcher ou applet).
		{
			cairo_dock_detach_icon_from_dock (pLastClassIcon, pDock, FALSE);
			g_free (pLastClassIcon->cParentDockName);
			pLastClassIcon->cParentDockName = NULL;
			
			cairo_dock_destroy_dock (pDock, cClass);
			pFakeClassIcon->pSubDock = NULL;
			cd_debug ("sanity check : pFakeClassIcon->Xid : %d", pFakeClassIcon->Xid);
			if (! cairo_dock_icon_is_being_removed (pLastClassIcon))
			{
				cairo_dock_insert_appli_in_dock (pLastClassIcon, g_pMainDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);  // a priori inutile.
				cairo_dock_update_name_on_inhibators (cClass, pLastClassIcon->Xid, pLastClassIcon->cName);
			}
			else  // la derniere icone est en cours de suppression, inutile de la re-inserer
			{
				pFakeClassIcon->bHasIndicator = FALSE;
				cairo_dock_free_icon (pLastClassIcon);
			}
			cairo_dock_redraw_icon (pFakeClassIcon, CAIRO_CONTAINER (g_pMainDock));
		}
		return TRUE;
	}
	return FALSE;
}


static void _cairo_dock_reset_overwrite_exceptions (gchar *cClass, CairoDockClassAppli *pClassAppli, gpointer data)
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
		CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClassList[i]);
		pClassAppli->bUseXIcon = TRUE;
	}
	
	g_strfreev (cClassList);
}

static void _cairo_dock_reset_group_exceptions (gchar *cClass, CairoDockClassAppli *pClassAppli, gpointer data)
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
		CairoDockClassAppli *pClassAppli = cairo_dock_get_class (cClassList[i]);
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



Icon *cairo_dock_get_inhibator (Icon *pIcon, gboolean bOnlyInDock)
{
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (pIcon->cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibatorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibatorIcon = pElement->data;
			
			if (pInhibatorIcon->Xid == pIcon->Xid)
			{
				if (! bOnlyInDock || pInhibatorIcon->cParentDockName != NULL)
					return pInhibatorIcon;
			}
		}
	}
	return NULL;
}

void cairo_dock_set_class_order (Icon *pIcon)
{
	double fOrder = CAIRO_DOCK_LAST_ORDER;
	CairoDockClassAppli *pClassAppli = cairo_dock_get_class (pIcon->cClass);
	if (pClassAppli != NULL)  // on va chercher une icone de meme classe dans le dock principal
	{
		// on cherche un inhibiteur de cette classe, de preference un lanceur.
		Icon *pSameClassIcon = NULL;
		CairoDock *pDock;
		Icon *pInhibatorIcon;
		GList *ic;
		for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
		{
			pInhibatorIcon = ic->data;
			if (CAIRO_DOCK_IS_APPLET (pInhibatorIcon) && myIcons.iSeparateIcons)
				continue;
			pDock = cairo_dock_search_dock_from_name (pInhibatorIcon->cParentDockName);
			if (!pDock || !pDock->bIsMainDock)
				pInhibatorIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
			pSameClassIcon = pInhibatorIcon;
			if (CAIRO_DOCK_IS_LAUNCHER (pSameClassIcon))  // on prend les lanceurs de preference.
				break ;
		}
		
		// si aucun trouve, on cherche une autre appli de cette classe.
		if (pSameClassIcon == NULL)
		{
			Icon *pAppliIcon = NULL;
			for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
			{
				pAppliIcon = ic->data;
				if (pAppliIcon == pIcon)
					continue;
				pDock = cairo_dock_search_dock_from_name (pAppliIcon->cParentDockName);
				if (pDock && pDock->bIsMainDock)
				{
					pSameClassIcon = pAppliIcon;
					break ;
				}
			}
		}
		
		// on se place entre l'icone trouvee et la suivante de classe differente.
		if (pSameClassIcon != NULL)  // une icone de meme classe existe dans le main dock, on va se placer apres.
		{
			Icon *pNextIcon = NULL;
			ic = g_list_find (g_pMainDock->icons, pSameClassIcon);
			if (ic && ic->next != NULL)  // on remonte vers la droite toutes les icones de meme classe, et on cherche l'icone suivante.
			{
				ic = ic->next;
				for (;ic != NULL; ic = ic->next)
				{
					pNextIcon = ic->data;
					if (!pNextIcon->cClass || strcmp (pNextIcon->cClass, pIcon->cClass) != 0)
						break;
					pSameClassIcon = pNextIcon;
					pNextIcon = NULL;
				}
			}
			//g_print ("on se place apres la derniere icone de cette classe\n");
			if (pNextIcon != NULL && cairo_dock_get_icon_order (pNextIcon) == cairo_dock_get_icon_order (pSameClassIcon))  // l'icone suivante est dans le meme groupe que nous, on s'intercalle entre elle et pSameClassIcon.
				fOrder = (pNextIcon->fOrder + pSameClassIcon->fOrder) / 2;
			else  // aucune icone apres notre classe, ou alors dans un groupe different, on se place juste apres pSameClassIcon.
				fOrder = pSameClassIcon->fOrder + 1;
		}
	}
	
	if (fOrder == CAIRO_DOCK_LAST_ORDER)  // cette classe n'existe pas encore ou est vide ou n'a aucune icone dans le main dock => on va se positionner par rapport a la derniere appli.
	{
		GList* ic;
		Icon *icon;
		for (ic = g_list_last (g_pMainDock->icons); ic != NULL; ic = ic->prev)
		{
			icon = ic->data;
			if (icon->iType == CAIRO_DOCK_APPLI && ! cairo_dock_class_is_inhibated (icon->cClass))  // on verifie qu'elle n'est pas placé a cote de son lanceur, sinon cela cree des incoherences suivants que l'appli est lancee 2 fois ou pas.
			{
				break ;
			}
		}
		if (ic != NULL)  // on a trouve une icone d'appli.
		{
			//g_print ("on se place apres la derniere appli\n");
			ic = ic->next;
			Icon *next_icon = (ic ? ic->data : NULL);
			if (next_icon != NULL && cairo_dock_get_icon_order (next_icon) == cairo_dock_get_icon_order (icon))
				fOrder = (icon->fOrder + next_icon->fOrder) / 2;
			else
				fOrder = icon->fOrder + 1;
		}
		else if (myTaskBar.bMixLauncherAppli)  // on decide de la placer apres le dernier lanceur dans le cas ou les lanceurs peuvent faire appli.
		{
			for (ic = g_list_last (g_pMainDock->icons); ic != NULL; ic = ic->prev)
			{
				icon = ic->data;
				if (icon->iType == CAIRO_DOCK_LAUNCHER)
					break;
			}
			if (ic != NULL)  // on a trouve une icone de lanceur.
			{
				//g_print ("on se place apres le dernier lanceur\n");
				ic = ic->next;
				Icon *next_icon = (ic ? ic->data : NULL);
				if (next_icon != NULL && cairo_dock_get_icon_order (next_icon) == cairo_dock_get_icon_order (icon))
					fOrder = (icon->fOrder + next_icon->fOrder) / 2;
				else
					fOrder = icon->fOrder + 1;
			}
		}
	}
	pIcon->fOrder = fOrder;
}

static void _cairo_dock_reorder_one_class (gchar *cClass, CairoDockClassAppli *pClassAppli, int *iMaxOrder)
{
	// on touve un inhibiteur par rapport auquel se placer.
	Icon *pSameClassIcon = NULL;
	Icon *pInhibatorIcon;
	CairoDock *pParentDock;
	GList *ic;
	for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
	{
		pInhibatorIcon = ic->data;
		if (CAIRO_DOCK_IS_APPLET (pInhibatorIcon) && myIcons.iSeparateIcons)
			continue;
		
		pParentDock = cairo_dock_search_dock_from_name (pInhibatorIcon->cParentDockName);
		CairoDock *pDock;
		while (pParentDock && pParentDock->iRefCount != 0)
		{
			pDock = pParentDock;
			pInhibatorIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
		}
		if (!pParentDock || !pParentDock->bIsMainDock)  // on place les icones d'applis dans le main dock.
			continue;
		pSameClassIcon = pInhibatorIcon;
		if (CAIRO_DOCK_IS_LAUNCHER (pSameClassIcon))  // on prend les lanceurs de preference aux applets.
			break ;
	}
	
	// on cherche une icone qui nous borne a droite.
	Icon *pNextIcon = NULL;
	if (pSameClassIcon != NULL)  // on se place apres l'icone trouve.
	{
		ic = g_list_find (g_pMainDock->icons, pSameClassIcon);
		if (ic != NULL && ic->next != NULL)  // on remonte vers la droite jusqu'a trouver une icone de classe differente.
		{
			for (ic = ic->next; ic != NULL; ic = ic->next)
			{
				pNextIcon = ic->data;
				if (!pNextIcon->cClass || strcmp (pNextIcon->cClass, cClass) != 0)  // icone d'une autre classe. (pSameClassIcon->cClass peut etre NULL dans le cas d'un inhibiteur place dans un sous-dock, car alors pSameClassIcon est l'icone pointant sur le sous-dock.)
					break;
				pSameClassIcon = pNextIcon;
				pNextIcon = NULL;
			}
		}
	}
	
	// on se place entre les 2 icones, ou a la fin si aucune icone trouvee.
	Icon *pAppliIcon;
	CairoDock *pDock;
	if (pNextIcon != NULL)  // on se place entre les 2.
	{
		int i=1, iNbIcons = g_list_length (pClassAppli->pAppliOfClass);  // majorant.
		for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
		{
			pAppliIcon = ic->data;
			pDock = cairo_dock_search_dock_from_name (pAppliIcon->cParentDockName);
			if (pDock && pDock->iRefCount == 0)
			{
				pAppliIcon->fOrder = pSameClassIcon->fOrder + (pNextIcon->fOrder - pSameClassIcon->fOrder) * i / (iNbIcons + 1);
				i ++;
			}
		}
	}
	else  // on se place a la fin.
	{
		for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
		{
			pAppliIcon = ic->data;
			pDock = cairo_dock_search_dock_from_name (pAppliIcon->cParentDockName);
			if (pDock && pDock->iRefCount == 0)
			{
				pAppliIcon->fOrder = *iMaxOrder;
				*iMaxOrder ++;
			}
		}
	}
}
void cairo_dock_reorder_classes (void)
{
	Icon *pLastIcon = cairo_dock_get_last_icon (g_pMainDock->icons);
	int iMaxOrder = (pLastIcon ? pLastIcon->fOrder + 1 : 1);
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_reorder_one_class, &iMaxOrder);
}

