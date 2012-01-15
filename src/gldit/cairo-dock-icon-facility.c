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


#include "cairo-dock-struct.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-module-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-manager.h"  // cairo_dock_trigger_refresh_launcher_gui
#include "cairo-dock-animations.h"  // CairoDockHidingEffect
#include "cairo-dock-icon-facility.h"

extern gchar *g_cCurrentLaunchersPath;
extern CairoDockHidingEffect *g_pHidingBackend;


CairoDockIconGroup cairo_dock_get_icon_type (Icon *icon)
{
	CairoDockIconGroup iGroup;
	if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
		iGroup = CAIRO_DOCK_SEPARATOR12;
	else
		iGroup = (icon->iGroup < CAIRO_DOCK_NB_GROUPS ? icon->iGroup : icon->iGroup & 1);
	
	return iGroup;
	/**if (CAIRO_DOCK_IS_APPLI (icon))
		return CAIRO_DOCK_APPLI;
	else if (CAIRO_DOCK_IS_APPLET (icon))
		return CAIRO_DOCK_APPLET;
	else if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon))
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
	int iOrder1 = cairo_dock_get_icon_order (icon1);
	int iOrder2 = cairo_dock_get_icon_order (icon2);
	if (iOrder1 < iOrder2)
		return -1;
	else if (iOrder1 > iOrder2)
		return 1;
	
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
	int iOrder1 = cairo_dock_get_icon_order (icon1);
	int iOrder2 = cairo_dock_get_icon_order (icon2);
	if (iOrder1 < iOrder2)
		return -1;
	else if (iOrder1 > iOrder2)
		return 1;
	
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
	
	guint iCurrentGroup = -1;
	double fCurrentOrder = 0.;
	Icon *icon;
	GList *ic;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->iGroup != iCurrentGroup)
		{
			iCurrentGroup = icon->iGroup;
			fCurrentOrder = 0.;
		}
		icon->fOrder = fCurrentOrder++;
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

Icon* cairo_dock_get_first_icon_of_group (GList *pIconList, CairoDockIconGroup iGroup)
{
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->iGroup == iGroup)
			return icon;
	}
	return NULL;
}
Icon* cairo_dock_get_last_icon_of_group (GList *pIconList, CairoDockIconGroup iGroup)
{
	GList* ic;
	Icon *icon;
	for (ic = g_list_last (pIconList); ic != NULL; ic = ic->prev)
	{
		icon = ic->data;
		if (icon->iGroup == iGroup)
			return icon;
	}
	return NULL;
}
Icon* cairo_dock_get_first_icon_of_order (GList *pIconList, CairoDockIconGroup iGroup)
{
	CairoDockIconGroup iGroupOrder = cairo_dock_get_group_order (iGroup);
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
Icon* cairo_dock_get_last_icon_of_order (GList *pIconList, CairoDockIconGroup iGroup)
{
	CairoDockIconGroup iGroupOrder = cairo_dock_get_group_order (iGroup);
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
Icon* cairo_dock_get_first_icon_of_true_type (GList *pIconList, CairoDockIconTrueType iType)
{
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->iTrueType == iType)
			return icon;
	}
	return NULL;
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

void cairo_dock_get_icon_extent (Icon *pIcon, int *iWidth, int *iHeight)
{
	*iWidth = pIcon->iImageWidth;
	*iHeight = pIcon->iImageHeight;
}

void cairo_dock_get_current_icon_size (Icon *pIcon, CairoContainer *pContainer, double *fSizeX, double *fSizeY)
{
	if (pContainer->bIsHorizontal)
	{
		if (myIconsParam.bConstantSeparatorSize && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
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
		if (myIconsParam.bConstantSeparatorSize && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
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
		fReflectSize = myIconsParam.fReflectSize * icon->fScale * fabs (icon->fHeightFactor) + icon->fDeltaYReflection + myDocksParam.iFrameMargin;  // un peu moyen le iFrameMargin mais bon ...
	}
	if (! myIndicatorsParam.bIndicatorOnIcon)
		fReflectSize = MAX (fReflectSize, myIndicatorsParam.fIndicatorDeltaY * icon->fHeight);
	
	double fX = icon->fDrawX;
	fX += icon->fWidth * icon->fScale * (1 - fabs (icon->fWidthFactor))/2 + icon->fGlideOffset * icon->fWidth * icon->fScale;
	
	double fY = icon->fDrawY;
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		if (cairo_dock_is_hidden (pDock) && (g_pHidingBackend == NULL || !g_pHidingBackend->bCanDisplayHiddenDock))
		{
			fY = (pDock->container.bDirectionUp ? pDock->container.iHeight - icon->fHeight * icon->fScale : 0.);
		}
	}
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



void cairo_dock_normalize_icons_order (GList *pIconList, CairoDockIconGroup iGroup)
{
	cd_message ("%s (%d)", __func__, iGroup);
	int iOrder = 1;
	CairoDockIconGroup iGroupOrder = cairo_dock_get_group_order (iGroup);
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
}

void cairo_dock_move_icon_after_icon (CairoDock *pDock, Icon *icon1, Icon *icon2)  // move icon1 after icon2,or at the beginning of the dock/group if icon2 is NULL.
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
		Icon *pFirstIcon = cairo_dock_get_first_icon_of_order (pDock->icons, icon1->iGroup);
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
		cairo_dock_normalize_icons_order (pDock->icons, icon1->iGroup);
	
	//\_________________ Notify everybody.
	cairo_dock_notify_on_object (pDock, NOTIFICATION_ICON_MOVED, icon1, pDock);
}



void cairo_dock_update_icon_s_container_name (Icon *icon, const gchar *cNewParentDockName)
{
	g_free (icon->cParentDockName);
	icon->cParentDockName = g_strdup (cNewParentDockName);
	
	cairo_dock_write_container_name_in_conf_file (icon, cNewParentDockName);
}


void cairo_dock_set_icon_name (const gchar *cIconName, Icon *pIcon, CairoContainer *pContainer_useless)  // fonction proposee par Necropotame.
{
	g_return_if_fail (pIcon != NULL);  // le contexte sera verifie plus loin.
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
	
	cairo_dock_load_icon_text (pIcon, &myIconsParam.iconTextDescription);
}

void cairo_dock_set_icon_name_printf (Icon *pIcon, CairoContainer *pContainer_useless, const gchar *cIconNameFormat, ...)
{
	va_list args;
	va_start (args, cIconNameFormat);
	gchar *cFullText = g_strdup_vprintf (cIconNameFormat, args);
	cairo_dock_set_icon_name (cFullText, pIcon, NULL);
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
		&myIconsParam.quickInfoTextDescription,
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


gchar *cairo_dock_cut_string (const gchar *cString, int iNbCaracters)  // gere l'UTF-8
{
	g_return_val_if_fail (cString != NULL, NULL);
	gchar *cTruncatedName = NULL;
	gsize bytes_read, bytes_written;
	GError *erreur = NULL;
	gchar *cUtf8Name = g_locale_to_utf8 (cString,
		-1,
		&bytes_read,
		&bytes_written,
		&erreur);  // inutile sur Ubuntu, qui est nativement UTF8, mais sur les autres on ne sait pas.
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	if (cUtf8Name == NULL)  // une erreur s'est produite, on tente avec la chaine brute.
		cUtf8Name = g_strdup (cString);
	
	const gchar *cEndValidChain = NULL;
	int iStringLength;
	if (g_utf8_validate (cUtf8Name, -1, &cEndValidChain))
	{
		iStringLength = g_utf8_strlen (cUtf8Name, -1);
		int iNbChars = -1;
		if (iNbCaracters < 0)
		{
			iNbChars = MAX (0, iStringLength + iNbCaracters);
		}
		else if (iStringLength > iNbCaracters)
		{
			iNbChars = iNbCaracters;
		}
		
		if (iNbChars != -1)
		{
			cTruncatedName = g_new0 (gchar, 8 * (iNbChars + 4));  // 8 octets par caractere.
			if (iNbChars != 0)
				g_utf8_strncpy (cTruncatedName, cUtf8Name, iNbChars);
			
			gchar *cTruncature = g_utf8_offset_to_pointer (cTruncatedName, iNbChars);
			*cTruncature = '.';
			*(cTruncature+1) = '.';
			*(cTruncature+2) = '.';
		}
	}
	else
	{
		iStringLength = strlen (cString);
		int iNbChars = -1;
		if (iNbCaracters < 0)
		{
			iNbChars = MAX (0, iStringLength + iNbCaracters);
		}
		else if (iStringLength > iNbCaracters)
		{
			iNbChars = iNbCaracters;
		}
		
		if (iNbChars != -1)
		{
			cTruncatedName = g_new0 (gchar, iNbCaracters + 4);
			if (iNbChars != 0)
				strncpy (cTruncatedName, cString, iNbChars);
			
			cTruncatedName[iNbChars] = '.';
			cTruncatedName[iNbChars+1] = '.';
			cTruncatedName[iNbChars+2] = '.';
		}
	}
	if (cTruncatedName == NULL)
		cTruncatedName = cUtf8Name;
	else
		g_free (cUtf8Name);
	//g_print (" -> etiquette : %s\n", cTruncatedName);
	return cTruncatedName;
}
