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

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <cairo.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-config.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-load.h"
#include "cairo-dock-container.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-desktop-file-factory.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-icons.h"

int g_iNbNonStickyLaunchers = 0;

extern CairoDockDesktopGeometry g_desktopGeometry;
extern gchar *g_cCurrentLaunchersPath;
extern gboolean g_bUseOpenGL;

static GList *s_DetachedLaunchersList = NULL;


static void _cairo_dock_free_icon_buffers (Icon *icon)
{
	if (icon == NULL)
		return ;
	
	g_free (icon->cDesktopFileName);
	g_free (icon->cFileName);
	g_free (icon->cName);
	g_free (icon->cInitialName);
	g_free (icon->cCommand);
	g_free (icon->cWorkingDirectory);
	g_free (icon->cBaseURI);
	g_free (icon->cParentDockName);  // on ne liberera pas le sous-dock ici sous peine de se mordre la queue, donc il faut l'avoir fait avant.
	g_free (icon->cClass);
	g_free (icon->cQuickInfo);
	g_free (icon->cLastAttentionDemand);
	
	cairo_surface_destroy (icon->pIconBuffer);
	cairo_surface_destroy (icon->pReflectionBuffer);
	cairo_surface_destroy (icon->pTextBuffer);
	cairo_surface_destroy (icon->pQuickInfoBuffer);
	
	if (icon->iIconTexture != 0)
		_cairo_dock_delete_texture (icon->iIconTexture);
	if (icon->iLabelTexture != 0)
		_cairo_dock_delete_texture (icon->iLabelTexture);
	if (icon->iQuickInfoTexture != 0)
		_cairo_dock_delete_texture (icon->iQuickInfoTexture);
}
void cairo_dock_free_icon (Icon *icon)
{
	if (icon == NULL)
		return ;
	cd_debug ("%s (%s , %s)", __func__, icon->cName, icon->cClass);
	
	cairo_dock_remove_dialog_if_any (icon);
	if (icon->iSidRedrawSubdockContent != 0)
		g_source_remove (icon->iSidRedrawSubdockContent);
	if (icon->iSidLoadImage != 0)
		g_source_remove (icon->iSidLoadImage);
	if (icon->cBaseURI != NULL)
		cairo_dock_fm_remove_monitor (icon);
	if (CAIRO_DOCK_IS_NORMAL_APPLI (icon))
		cairo_dock_unregister_appli (icon);
	else if (icon->cClass != NULL)  // c'est un inhibiteur.
		cairo_dock_deinhibate_class (icon->cClass, icon);
	if (icon->pModuleInstance != NULL)
		cairo_dock_deinstanciate_module (icon->pModuleInstance);
	cairo_dock_notify (CAIRO_DOCK_STOP_ICON, icon);
	cairo_dock_remove_transition_on_icon (icon);
	
	if (icon->iSpecificDesktop != 0)
	{
		g_iNbNonStickyLaunchers --;
		s_DetachedLaunchersList = g_list_remove(s_DetachedLaunchersList, icon);
	}
	
	cairo_dock_free_notification_table (icon->pNotificationsTab);
	_cairo_dock_free_icon_buffers (icon);
	cd_debug ("icon freeed");
	g_free (icon);
}

CairoDockIconType cairo_dock_get_icon_type (Icon *icon)
{
	int iType;
	if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		iType = CAIRO_DOCK_SEPARATOR12;
	else
		iType = (icon->iType < CAIRO_DOCK_NB_TYPES ? icon->iType : icon->iType & 1);
	
	return iType;
	/**if (CAIRO_DOCK_IS_APPLI (icon))
		return CAIRO_DOCK_APPLI;
	else if (CAIRO_DOCK_IS_APPLET (icon))
		return CAIRO_DOCK_APPLET;
	else if (CAIRO_DOCK_IS_SEPARATOR (icon))
		return CAIRO_DOCK_SEPARATOR12;
	else
		return CAIRO_DOCK_LAUNCHER;*/
}


int cairo_dock_compare_icons_order (Icon *icon1, Icon *icon2)
{
	int iOrder1 = cairo_dock_get_icon_order (icon1);
	int iOrder2 = cairo_dock_get_icon_order (icon2);
	if (iOrder1 < iOrder2)
		return -1;
	else if (iOrder1 > iOrder2)
		return 1;
	else
	{
		if (icon1->fOrder < icon2->fOrder)
			return -1;
		else if (icon1->fOrder > icon2->fOrder)
			return 1;
		else
			return 0;
	}
}
int cairo_dock_compare_icons_name (Icon *icon1, Icon *icon2)
{
	if (icon1->cName == NULL)
		return -1;
	if (icon2->cName == NULL)
		return 1;
	gchar *cURI_1 = g_ascii_strdown (icon1->cName, -1);
	gchar *cURI_2 = g_ascii_strdown (icon2->cName, -1);
	int iOrder = strcmp (cURI_1, cURI_2);
	g_free (cURI_1);
	g_free (cURI_2);
	return iOrder;
}

int cairo_dock_compare_icons_extension (Icon *icon1, Icon *icon2)
{
	if (icon1->cBaseURI == NULL)
		return -1;
	if (icon2->cBaseURI == NULL)
		return 1;
	
	gchar *ext1 = strrchr (icon1->cBaseURI, '.');
	gchar *ext2 = strrchr (icon2->cBaseURI, '.');
	if (ext1 == NULL)
		return -1;
	if (ext2 == NULL)
		return 1;
	
	ext1 = g_ascii_strdown (ext1+1, -1);
	ext2 = g_ascii_strdown (ext2+1, -1);
	
	int iOrder = strcmp (ext1, ext2);
	g_free (ext1);
	g_free (ext2);
	return iOrder;
}

GList *cairo_dock_sort_icons_by_order (GList *pIconList)
{
	return g_list_sort (pIconList, (GCompareFunc) cairo_dock_compare_icons_order);
}

GList *cairo_dock_sort_icons_by_name (GList *pIconList)
{
	GList *pSortedIconList = g_list_sort (pIconList, (GCompareFunc) cairo_dock_compare_icons_name);
	int iOrder = 0;
	Icon *icon;
	GList *ic;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		icon->fOrder = iOrder ++;
	}
	return pSortedIconList;
}



Icon* cairo_dock_get_first_icon (GList *pIconList)
{
	GList *pListHead = g_list_first(pIconList);
	return (pListHead != NULL ? pListHead->data : NULL);
}

Icon* cairo_dock_get_last_icon (GList *pIconList)
{
	GList *pListTail = g_list_last(pIconList);
	return (pListTail != NULL ? pListTail->data : NULL);
}

Icon *cairo_dock_get_first_drawn_icon (CairoDock *pDock)
{
	if (pDock->pFirstDrawnElement != NULL)
		return pDock->pFirstDrawnElement->data;
	else
		return cairo_dock_get_first_icon (pDock->icons);
}

Icon *cairo_dock_get_last_drawn_icon (CairoDock *pDock)
{
	if (pDock->pFirstDrawnElement != NULL)
	{
		if (pDock->pFirstDrawnElement->prev != NULL)
			return pDock->pFirstDrawnElement->prev->data;
		else
			return cairo_dock_get_last_icon (pDock->icons);
	}
	else
		return cairo_dock_get_last_icon (pDock->icons);;
}

Icon* cairo_dock_get_first_icon_of_group (GList *pIconList, CairoDockIconType iType)
{
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->iType == iType)
			return icon;
	}
	return NULL;
}
Icon* cairo_dock_get_last_icon_of_group (GList *pIconList, CairoDockIconType iType)
{
	GList* ic;
	Icon *icon;
	for (ic = g_list_last (pIconList); ic != NULL; ic = ic->prev)
	{
		icon = ic->data;
		if (icon->iType == iType)
			return icon;
	}
	return NULL;
}
Icon* cairo_dock_get_first_icon_of_order (GList *pIconList, CairoDockIconType iType)
{
	CairoDockIconType iGroupOrder = cairo_dock_get_group_order (iType);
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (cairo_dock_get_icon_order (icon) == iGroupOrder)
			return icon;
	}
	return NULL;
}
Icon* cairo_dock_get_last_icon_of_order (GList *pIconList, CairoDockIconType iType)
{
	CairoDockIconType iGroupOrder = cairo_dock_get_group_order (iType);
	GList* ic;
	Icon *icon;
	for (ic = g_list_last (pIconList); ic != NULL; ic = ic->prev)
	{
		icon = ic->data;
		if (cairo_dock_get_icon_order (icon) == iGroupOrder)
			return icon;
	}
	return NULL;
}
Icon* cairo_dock_get_last_icon_until_order (GList *pIconList, CairoDockIconType iType)
{
	CairoDockIconType iGroupOrder = cairo_dock_get_group_order (iType);
	GList* ic;
	Icon *icon = NULL;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (cairo_dock_get_icon_order (icon) > iGroupOrder)
		{
			if (ic->prev != NULL)
				icon = ic->prev->data;
			else
				icon = NULL;
			break;
		}
	}
	return icon;
}

Icon* cairo_dock_get_pointed_icon (GList *pIconList)
{
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->bPointed)
			return icon;
	}
	return NULL;
}


Icon *cairo_dock_get_next_icon (GList *pIconList, Icon *pIcon)
{
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon == pIcon)
		{
			if (ic->next != NULL)
				return ic->next->data;
			else
				return NULL;
		}
	}
	return NULL;
}

Icon *cairo_dock_get_previous_icon (GList *pIconList, Icon *pIcon)
{
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon == pIcon)
		{
			if (ic->prev != NULL)
				return ic->prev->data;
			else
				return NULL;
		}
	}
	return NULL;
}

Icon *cairo_dock_get_icon_with_command (GList *pIconList, const gchar *cCommand)
{
	g_return_val_if_fail (cCommand != NULL, NULL);
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->cCommand != NULL && strncmp (icon->cCommand, cCommand, MIN (strlen (icon->cCommand), strlen (cCommand))) == 0)
			return icon;
	}
	return NULL;
}

Icon *cairo_dock_get_icon_with_base_uri (GList *pIconList, const gchar *cBaseURI)
{
	g_return_val_if_fail (cBaseURI != NULL, NULL);
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		//cd_message ("  icon->cBaseURI : %s\n", icon->cBaseURI);
		if (icon->cBaseURI != NULL && strcmp (icon->cBaseURI, cBaseURI) == 0)
			return icon;
	}
	return NULL;
}

Icon *cairo_dock_get_icon_with_name (GList *pIconList, const gchar *cName)
{
	g_return_val_if_fail (cName != NULL, NULL);
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		//cd_message ("  icon->cName : %s\n", icon->cName);
		if (icon->cName != NULL && strcmp (icon->cName, cName) == 0)
			return icon;
	}
	return NULL;
}

Icon *cairo_dock_get_icon_with_subdock (GList *pIconList, CairoDock *pSubDock)
{
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pSubDock == pSubDock)
			return icon;
	}
	return NULL;
}

Icon *cairo_dock_get_icon_with_module (GList *pIconList, CairoDockModule *pModule)
{
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pModuleInstance->pModule == pModule)
			return icon;
	}
	return NULL;
}

void cairo_dock_get_icon_extent (Icon *pIcon, CairoContainer *pContainer, int *iWidth, int *iHeight)
{
	/**double fMaxScale = cairo_dock_get_max_scale (pContainer);
	double fRatio = (CAIRO_DOCK_IS_DOCK (pContainer) ? pContainer->fRatio : 1.);  // on ne tient pas compte de l'effet de zoom initial du desklet.
	if (!pContainer || pContainer->bIsHorizontal)
	{
		*iWidth = (int) (pIcon->fWidth / fRatio * fMaxScale);
		*iHeight = (int) (pIcon->fHeight / fRatio * fMaxScale);
	}
	else
	{
		*iHeight = (int) (pIcon->fWidth / fRatio * fMaxScale);
		*iWidth = (int) (pIcon->fHeight / fRatio * fMaxScale);
	}*/
	*iWidth = pIcon->iImageWidth;
	*iHeight = pIcon->iImageHeight;
}

void cairo_dock_get_current_icon_size (Icon *pIcon, CairoContainer *pContainer, double *fSizeX, double *fSizeY)
{
	if (pContainer->bIsHorizontal)
	{
		if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (pIcon))
		{
			*fSizeX = pIcon->fWidth;
			*fSizeY = pIcon->fHeight;
		}
		else
		{
			*fSizeX = pIcon->fWidth * pIcon->fWidthFactor * pIcon->fScale * pIcon->fGlideScale;
			*fSizeY = pIcon->fHeight * pIcon->fHeightFactor * pIcon->fScale * pIcon->fGlideScale;
		}
	}
	else
	{
		if (myIcons.bConstantSeparatorSize && CAIRO_DOCK_IS_SEPARATOR (pIcon))
		{
			*fSizeX = pIcon->fHeight;
			*fSizeY = pIcon->fWidth;
		}
		else
		{
			*fSizeX = pIcon->fHeight * pIcon->fHeightFactor * pIcon->fScale * pIcon->fGlideScale;
			*fSizeY = pIcon->fWidth * pIcon->fWidthFactor * pIcon->fScale * pIcon->fGlideScale;
		}
	}
}

void cairo_dock_compute_icon_area (Icon *icon, CairoContainer *pContainer, GdkRectangle *pArea)
{
	double fReflectSize = 0;
	if (pContainer->bUseReflect)
	{
		fReflectSize = myIcons.fReflectSize * icon->fScale * fabs (icon->fHeightFactor) + icon->fDeltaYReflection + myBackground.iFrameMargin;  // un peu moyen le iFrameMargin mais bon ...
	}
	if (! myIndicators.bIndicatorOnIcon)
		fReflectSize = MAX (fReflectSize, myIndicators.fIndicatorDeltaY * icon->fHeight);
	
	double fX = icon->fDrawX;
	fX += icon->fWidth * icon->fScale * (1 - fabs (icon->fWidthFactor))/2 + icon->fGlideOffset * icon->fWidth * icon->fScale;
	
	double fY = icon->fDrawY;
	fY += (pContainer->bDirectionUp ? icon->fHeight * icon->fScale * (1 - icon->fHeightFactor)/2 : - fReflectSize);
	if (fY < 0)
		fY = 0;
	
	if (pContainer->bIsHorizontal)
	{
		pArea->x = (int) floor (fX) - 1;
		pArea->y = (int) floor (fY);
		pArea->width = (int) ceil (icon->fWidth * icon->fScale * fabs (icon->fWidthFactor)) + 2;
		pArea->height = (int) ceil (icon->fHeight * icon->fScale * fabs (icon->fHeightFactor) + fReflectSize);
	}
	else
	{
		pArea->x = (int) floor (fY);
		pArea->y = (int) floor (fX) - 1;
		pArea->width = ((int) ceil (icon->fHeight * icon->fScale * fabs (icon->fHeightFactor) + fReflectSize));
		pArea->height = (int) ceil (icon->fWidth * icon->fScale * fabs (icon->fWidthFactor)) + 2;
	}
	//g_print ("redraw : %d;%d %dx%d\n", pArea->x, pArea->y, pArea->width,pArea->height);
}



void cairo_dock_normalize_icons_order (GList *pIconList, CairoDockIconType iType)
{
	cd_message ("%s (%d)", __func__, iType);
	int iOrder = 1;
	CairoDockIconType iGroupOrder = cairo_dock_get_group_order (iType);
	GString *sDesktopFilePath = g_string_new ("");
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (cairo_dock_get_icon_order (icon) != iGroupOrder)
			continue;
		
		icon->fOrder = iOrder ++;
		if (icon->cDesktopFileName != NULL)
		{
			g_string_printf (sDesktopFilePath, "%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
			cairo_dock_update_conf_file (sDesktopFilePath->str,
				G_TYPE_DOUBLE, "Desktop Entry", "Order", icon->fOrder,
				G_TYPE_INVALID);
		}
		else if (CAIRO_DOCK_IS_APPLET (icon))
		{
			cairo_dock_update_conf_file (icon->pModuleInstance->cConfFilePath,
				G_TYPE_DOUBLE, "Icon", "order", icon->fOrder,
				G_TYPE_INVALID);
		}
	}
	g_string_free (sDesktopFilePath, TRUE);
	cairo_dock_trigger_refresh_launcher_gui ();
}

void cairo_dock_move_icon_after_icon (CairoDock *pDock, Icon *icon1, Icon *icon2)
{
	//g_print ("%s (%s, %.2f, %x)\n", __func__, icon1->cName, icon1->fOrder, icon2);
	if ((icon2 != NULL) && fabs (cairo_dock_get_icon_order (icon1) - cairo_dock_get_icon_order (icon2)) > 1)
		return ;
	//\_________________ On change l'ordre de l'icone.
	gboolean bForceUpdate = FALSE;
	if (icon2 != NULL)
	{
		Icon *pNextIcon = cairo_dock_get_next_icon (pDock->icons, icon2);
		if (pNextIcon != NULL && fabs (pNextIcon->fOrder - icon2->fOrder) < 1e-2)
		{
			bForceUpdate = TRUE;
		}
		if (pNextIcon == NULL || cairo_dock_get_icon_order (pNextIcon) != cairo_dock_get_icon_order (icon2))
			icon1->fOrder = icon2->fOrder + 1;
		else
			icon1->fOrder = (pNextIcon->fOrder - icon2->fOrder > 1 ? icon2->fOrder + 1 : (pNextIcon->fOrder + icon2->fOrder) / 2);
	}
	else
	{
		Icon *pFirstIcon = cairo_dock_get_first_icon_of_order (pDock->icons, icon1->iType);
		if (pFirstIcon != NULL)
			icon1->fOrder = pFirstIcon->fOrder - 1;
		else
			icon1->fOrder = 1;
	}
	//g_print ("icon1->fOrder:%.2f\n", icon1->fOrder);
	
	//\_________________ On change l'ordre dans le fichier du lanceur 1.
	cairo_dock_write_order_in_conf_file (icon1, icon1->fOrder);
	
	//\_________________ On change sa place dans la liste.
	pDock->pFirstDrawnElement = NULL;
	pDock->icons = g_list_remove (pDock->icons, icon1);
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon1,
		(GCompareFunc) cairo_dock_compare_icons_order);

	//\_________________ On recalcule la largeur max, qui peut avoir ete influencee par le changement d'ordre.
	cairo_dock_update_dock_size (pDock);
	
	if (icon1->pSubDock != NULL && icon1->cClass != NULL)
	{
		cairo_dock_trigger_set_WM_icons_geometry (icon1->pSubDock);
	}
	
	if (pDock->iRefCount != 0)
	{
		cairo_dock_redraw_subdock_content (pDock);
	}
	
	if (bForceUpdate)
		cairo_dock_normalize_icons_order (pDock->icons, icon1->iType);
	if (CAIRO_DOCK_IS_STORED_LAUNCHER (icon1) || CAIRO_DOCK_IS_USER_SEPARATOR (icon1) || CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon1))
		cairo_dock_trigger_refresh_launcher_gui ();
}


Icon *cairo_dock_foreach_icons_of_type (GList *pIconList, CairoDockIconType iType, CairoDockForeachIconFunc pFuntion, gpointer data)
{
	//g_print ("%s (%d)\n", __func__, iType);
	if (pIconList == NULL)
		return NULL;

	Icon *icon;
	GList *ic = pIconList, *next_ic;
	gboolean bOneIconFound = FALSE;
	Icon *pSeparatorIcon = NULL;
	while (ic != NULL)  // on parcourt tout, inutile de complexifier la chose pour gagner 3ns.
	{
		icon = ic->data;
		next_ic = ic->next;
		if (icon->iType == iType)
		{
			bOneIconFound = TRUE;
			pFuntion (icon, NULL, data);
		}
		else
		{
			if (CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
			{
				if ( (bOneIconFound && pSeparatorIcon == NULL) || (! bOneIconFound) )
					pSeparatorIcon = icon;
			}
		}
		ic = next_ic;
	}

	if (bOneIconFound)
		return pSeparatorIcon;
	else
		return NULL;
}



void cairo_dock_update_icon_s_container_name (Icon *icon, const gchar *cNewParentDockName)
{
	g_free (icon->cParentDockName);
	icon->cParentDockName = g_strdup (cNewParentDockName);
	
	cairo_dock_write_container_name_in_conf_file (icon, cNewParentDockName);
}


void cairo_dock_set_icon_name (const gchar *cIconName, Icon *pIcon, CairoContainer *pContainer)  // fonction proposee par Necropotame.
{
	g_return_if_fail (pIcon != NULL && pContainer != NULL);  // le contexte sera verifie plus loin.
	gchar *cUniqueName = NULL;
	
	if (pIcon->pSubDock != NULL)
	{
		cUniqueName = cairo_dock_get_unique_dock_name (cIconName);
		cIconName = cUniqueName;
		cairo_dock_rename_dock (pIcon->cName, pIcon->pSubDock, cUniqueName);
	}
	if (pIcon->cName != cIconName)
	{
		g_free (pIcon->cName);
		pIcon->cName = g_strdup (cIconName);
	}
	
	g_free (cUniqueName);
	
	cairo_dock_load_icon_text (pIcon, &myLabels.iconTextDescription);
}

void cairo_dock_set_icon_name_printf (Icon *pIcon, CairoContainer *pContainer, const gchar *cIconNameFormat, ...)
{
	va_list args;
	va_start (args, cIconNameFormat);
	gchar *cFullText = g_strdup_vprintf (cIconNameFormat, args);
	cairo_dock_set_icon_name (cFullText, pIcon, pContainer);
	g_free (cFullText);
	va_end (args);
}

void cairo_dock_set_quick_info (Icon *pIcon, CairoContainer *pContainer, const gchar *cQuickInfo)
{
	g_return_if_fail (pIcon != NULL);  // le contexte sera verifie plus loin.

	if (pIcon->cQuickInfo != cQuickInfo)
	{
		g_free (pIcon->cQuickInfo);
		pIcon->cQuickInfo = g_strdup (cQuickInfo);
	}
	
	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	cairo_dock_load_icon_quickinfo (pIcon,
		&myLabels.quickInfoTextDescription,
		fMaxScale);
}

void cairo_dock_set_quick_info_printf (Icon *pIcon, CairoContainer *pContainer, const gchar *cQuickInfoFormat, ...)
{
	va_list args;
	va_start (args, cQuickInfoFormat);
	gchar *cFullText = g_strdup_vprintf (cQuickInfoFormat, args);
	cairo_dock_set_quick_info (pIcon, pContainer, cFullText);
	g_free (cFullText);
	va_end (args);
}



static CairoDock *_cairo_dock_insert_launcher_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bUpdateSize, gboolean bAnimate)
{
	cd_message ("%s (%s)", __func__, icon->cName);
	g_return_val_if_fail (pMainDock != NULL, NULL);
	
	cairo_dock_reserve_one_icon_geometry_for_window_manager (&icon->Xid, icon, pMainDock);
	
	//\_________________ On determine dans quel dock l'inserer.
	CairoDock *pParentDock = pMainDock;
	g_return_val_if_fail (pParentDock != NULL, NULL);

	//\_________________ On l'insere dans son dock parent en animant ce dernier eventuellement.
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
static CairoDock * _cairo_dock_detach_launcher(Icon *pIcon)
{
	cd_debug ("%s (%s, parent dock=%s)", __func__, pIcon->cName, pIcon->cParentDockName);
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (pIcon->cParentDockName);
	if (pParentDock == NULL)
		return NULL;

	gchar *cParentDockName = g_strdup(pIcon->cParentDockName);
	cairo_dock_detach_icon_from_dock (pIcon, pParentDock, TRUE); // this will set cParentDockName to NULL
	
	pIcon->cParentDockName = cParentDockName; // put it back !

	cairo_dock_update_dock_size (pParentDock);
	return pParentDock;
}
static void _cairo_dock_hide_show_launchers_on_other_desktops (Icon *icon, CairoContainer *pContainer, CairoDock *pMainDock)
{
	if (CAIRO_DOCK_IS_LAUNCHER(icon))
	{
		cd_debug ("%s (%s, iNumViewport=%d)", __func__, icon->cName, icon->iSpecificDesktop);
		CairoDock *pParentDock = NULL;
		// a specific desktop/viewport has been selected for this icon

		int iCurrentDesktop = 0, iCurrentViewportX = 0, iCurrentViewportY = 0;
		cairo_dock_get_current_desktop_and_viewport (&iCurrentDesktop, &iCurrentViewportX, &iCurrentViewportY);
		int index = iCurrentDesktop * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY + iCurrentViewportX * g_desktopGeometry.iNbViewportY + iCurrentViewportY + 1;  // +1 car on commence a compter a partir de 1.
		
		if (icon->iSpecificDesktop == 0 || icon->iSpecificDesktop == index || icon->iSpecificDesktop > g_desktopGeometry.iNbDesktops * g_desktopGeometry.iNbViewportX * g_desktopGeometry.iNbViewportY)
		/**if( icon->iSpecificDesktop <= 0 ||
		// metacity case: metacity uses desktop instead of viewport.
		    (g_desktopGeometry.iNbViewportX*g_desktopGeometry.iNbViewportY==1 &&
		    (icon->iSpecificDesktop-1 == iCurrentDesktop || 
		     icon->iSpecificDesktop >= g_desktopGeometry.iNbDesktops)) ||
		// compiz case: viewports are used within a desktop
				(g_desktopGeometry.iNbViewportX*g_desktopGeometry.iNbViewportY>1 &&
		    (icon->iSpecificDesktop-1 == iCurrentViewportX + g_desktopGeometry.iNbViewportX*iCurrentViewportY ||
		     icon->iSpecificDesktop >= g_desktopGeometry.iNbViewportX*g_desktopGeometry.iNbViewportY)) )*/
		{
			cd_debug (" => est visible sur ce viewport (iSpecificDesktop = %d).",icon->iSpecificDesktop);
			// check that it is in the detached list
			if( g_list_find(s_DetachedLaunchersList, icon) != NULL )
			{
				pParentDock = _cairo_dock_insert_launcher_in_dock (icon, pMainDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
				s_DetachedLaunchersList = g_list_remove(s_DetachedLaunchersList, icon);
			}
		}
		else
		{
			cd_debug (" Viewport actuel = %d => n'est pas sur le viewport actuel.", iCurrentViewportX + g_desktopGeometry.iNbViewportX*iCurrentViewportY);
			if( g_list_find(s_DetachedLaunchersList, icon) == NULL ) // only if not yet detached
			{
				cd_debug( "Detach launcher %s", icon->cName);
				pParentDock = _cairo_dock_detach_launcher(icon);
				s_DetachedLaunchersList = g_list_prepend(s_DetachedLaunchersList, icon);
			}
		}
		if (pParentDock != NULL)
			gtk_widget_queue_draw (pParentDock->container.pWidget);
	}
}

void cairo_dock_hide_show_launchers_on_other_desktops (CairoDock *pDock)
{
	if (g_iNbNonStickyLaunchers <= 0)
		return ;
	// first we detach what shouldn't be show on this viewport
	cairo_dock_foreach_icons_of_type (pDock->icons, CAIRO_DOCK_LAUNCHER, (CairoDockForeachIconFunc)_cairo_dock_hide_show_launchers_on_other_desktops, pDock);
	// then we reattach what was eventually missing
	cairo_dock_foreach_icons_of_type (s_DetachedLaunchersList, CAIRO_DOCK_LAUNCHER, (CairoDockForeachIconFunc)_cairo_dock_hide_show_launchers_on_other_desktops, pDock);
}
