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
#include <gio/gdesktopappinfo.h>

#include <cairo.h>


#include "cairo-dock-struct.h"
#include "cairo-dock-dialog-manager.h"  // gldi_dialogs_foreach
#include "cairo-dock-dialog-factory.h"
#include "cairo-dock-module-instance-manager.h"  // GldiModuleInstance
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"  // gldi_class_startup_notify
#include "cairo-dock-indicator-manager.h"
#include "cairo-dock-applications-manager.h"  // GLDI_OBJECT_IS_APPLI_ICON
#include "cairo-dock-applet-manager.h"  // GLDI_OBJECT_IS_APPLET_ICON
#include "cairo-dock-separator-manager.h"  // GLDI_OBJECT_IS_SEPARATOR_ICON
#include "cairo-dock-themes-manager.h"  // cairo_dock_update_conf_file
#include "cairo-dock-log.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"  // CairoDockHidingEffect
#include "cairo-dock-utils.h"  // cairo_dock_launch_app_info
#include "cairo-dock-icon-facility.h"

extern gchar *g_cCurrentLaunchersPath;
extern CairoDockHidingEffect *g_pHidingBackend;

extern CairoDockImageBuffer g_pIconBackgroundBuffer;


void gldi_icon_set_appli (Icon *pIcon, GldiWindowActor *pAppli)
{
	if (pIcon->pAppli == pAppli)  // nothing to do
		return;
	
	// unset the current appli if any
	if (pIcon->pAppli != NULL)
		gldi_object_unref (GLDI_OBJECT (pIcon->pAppli));
	
	// set the appli
	if (pAppli)
		gldi_object_ref (GLDI_OBJECT (pAppli));
	pIcon->pAppli = pAppli;
}

CairoDockIconGroup cairo_dock_get_icon_type (Icon *icon)
{
	/*CairoDockIconGroup iGroup;
	if (GLDI_OBJECT_IS_SEPARATOR_ICON (icon))
		iGroup = CAIRO_DOCK_SEPARATOR12;
	else
		iGroup = (icon->iGroup < CAIRO_DOCK_NB_GROUPS ? icon->iGroup : icon->iGroup & 1);
	return iGroup;*/
	return (icon->iGroup < CAIRO_DOCK_NB_GROUPS ? icon->iGroup : icon->iGroup & 1);
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
	for (ic = pSortedIconList; ic != NULL; ic = ic->next)
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
		//cd_message ("  icon->cBaseURI : %s", icon->cBaseURI);
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
		//cd_message ("  icon->cName : %s", icon->cName);
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

static gboolean _has_dialog (CairoDialog *pDialog, Icon *pIcon)
{
	return (pDialog->pIcon == pIcon);
}
gboolean gldi_icon_has_dialog (Icon *pIcon)
{
	CairoDialog *pDialog = gldi_dialogs_foreach ((GCompareFunc)_has_dialog, pIcon);
	return (pDialog != NULL);
}

Icon *gldi_icons_get_without_dialog (GList *pIconList)
{
	if (pIconList == NULL)
		return NULL;

	Icon *pIcon = cairo_dock_get_first_icon_of_group (pIconList, CAIRO_DOCK_SEPARATOR12);
	if (pIcon != NULL && ! gldi_icon_has_dialog (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
		return pIcon;
	
	pIcon = cairo_dock_get_pointed_icon (pIconList);
	if (pIcon != NULL && ! CAIRO_DOCK_IS_NORMAL_APPLI (pIcon) && ! GLDI_OBJECT_IS_APPLET_ICON (pIcon) && ! gldi_icon_has_dialog (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
		return pIcon;

	GList *ic;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (! gldi_icon_has_dialog (pIcon) && ! CAIRO_DOCK_IS_NORMAL_APPLI (pIcon) && ! GLDI_OBJECT_IS_APPLET_ICON (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
			return pIcon;
	}
	
	pIcon = cairo_dock_get_first_icon (pIconList);
	return pIcon;
}

void cairo_dock_get_icon_extent (Icon *pIcon, int *iWidth, int *iHeight)
{
	*iWidth = pIcon->image.iWidth;
	*iHeight = pIcon->image.iHeight;
}

void cairo_dock_get_current_icon_size (Icon *pIcon, GldiContainer *pContainer, double *fSizeX, double *fSizeY)
{
	if (pContainer->bIsHorizontal)
	{
		if (myIconsParam.bConstantSeparatorSize && GLDI_OBJECT_IS_SEPARATOR_ICON (pIcon))
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
		if (myIconsParam.bConstantSeparatorSize && GLDI_OBJECT_IS_SEPARATOR_ICON (pIcon))
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

void cairo_dock_compute_icon_area (Icon *icon, GldiContainer *pContainer, GdkRectangle *pArea)
{
	double fReflectSize = 0;
	if (pContainer->bUseReflect)
	{
		fReflectSize = /**myIconsParam.fReflectSize*/icon->fHeight * myIconsParam.fReflectHeightRatio * icon->fScale * fabs (icon->fHeightFactor) + icon->fDeltaYReflection + myDocksParam.iFrameMargin;  // un peu moyen le iFrameMargin mais bon ...
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
	//g_print ("redraw : %d;%d %dx%d (%s)\n", pArea->x, pArea->y, pArea->width,pArea->height, icon->cName);
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
	if ((icon2 != NULL) && abs ((int)cairo_dock_get_icon_order (icon1) - (int)cairo_dock_get_icon_order (icon2)) > 1)  // cast to int because enums can be unsigned (depending on the compiler)
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
	gldi_theme_icon_write_order_in_conf_file (icon1, icon1->fOrder);
	
	//\_________________ On change sa place dans la liste.
	pDock->icons = g_list_remove (pDock->icons, icon1);
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon1,
		(GCompareFunc) cairo_dock_compare_icons_order);

	//\_________________ On recalcule la largeur max, qui peut avoir ete influencee par le changement d'ordre.
	cairo_dock_trigger_update_dock_size (pDock);
	
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
	gldi_object_notify (pDock, NOTIFICATION_ICON_MOVED, icon1, pDock);
}


void gldi_icon_set_name (Icon *pIcon, const gchar *cIconName)  // fonction proposee par Necropotame.
{
	g_return_if_fail (pIcon != NULL);  // le contexte sera verifie plus loin.
	gchar *cUniqueName = NULL;
	
	if (pIcon->pSubDock != NULL)
	{
		cUniqueName = cairo_dock_get_unique_dock_name (cIconName);
		cIconName = cUniqueName;
		gldi_dock_rename (pIcon->pSubDock, cUniqueName);
	}
	if (pIcon->cName != cIconName)
	{
		g_free (pIcon->cName);
		pIcon->cName = g_strdup (cIconName);
	}
	
	g_free (cUniqueName);
	
	cairo_dock_load_icon_text (pIcon);
	
	if (pIcon->pContainer && pIcon->pContainer->bInside)  // for a dock, in this case the label will be visible.
		cairo_dock_redraw_container (pIcon->pContainer);  // this is not really optimized, ideally the view should provide a way to redraw the label area only...
}

void gldi_icon_set_name_printf (Icon *pIcon, const gchar *cIconNameFormat, ...)
{
	va_list args;
	va_start (args, cIconNameFormat);
	gchar *cFullText = g_strdup_vprintf (cIconNameFormat, args);
	gldi_icon_set_name (pIcon, cFullText);
	g_free (cFullText);
	va_end (args);
}

void gldi_icon_set_quick_info (Icon *pIcon, const gchar *cQuickInfo)
{
	g_return_if_fail (pIcon != NULL);
	
	if (pIcon->cQuickInfo != cQuickInfo)  // be paranoid, in case one passes pIcon->cQuickInfo to the function
	{
		if (g_strcmp0 (cQuickInfo, pIcon->cQuickInfo) == 0)  // if the text is the same, no need to reload it.
			return;
		g_free (pIcon->cQuickInfo);
		pIcon->cQuickInfo = g_strdup (cQuickInfo);
	}
	
	cairo_dock_load_icon_quickinfo (pIcon);
}

void gldi_icon_set_quick_info_printf (Icon *pIcon, const gchar *cQuickInfoFormat, ...)
{
	va_list args;
	va_start (args, cQuickInfoFormat);
	gchar *cFullText = g_strdup_vprintf (cQuickInfoFormat, args);
	gldi_icon_set_quick_info (pIcon, cFullText);
	g_free (cFullText);
	va_end (args);
}


GdkPixbuf *cairo_dock_icon_buffer_to_pixbuf (Icon *icon)
{
	g_return_val_if_fail (icon != NULL, NULL);
	
	return cairo_dock_image_buffer_to_pixbuf (&icon->image, 24, 24);
}

cairo_t *cairo_dock_begin_draw_icon_cairo (Icon *pIcon, gint iRenderingMode, cairo_t *pCairoContext)
{
	/*if (pIcon->pContainer)
		// g_print ("= %s %dx%d\n", pIcon->cName, pIcon->pContainer->iWidth, pIcon->pContainer->iHeight);
	else
		// g_print ("= %s no container yet\n", pIcon->cName); // e.g. 'indicator' applets -> maybe a return is needed?
	*/

	cairo_t *ctx = cairo_dock_begin_draw_image_buffer_cairo (&pIcon->image, iRenderingMode, pCairoContext);
	
	if (ctx && iRenderingMode != 1)
	{
		if (g_pIconBackgroundBuffer.pSurface != NULL
		&& ! GLDI_OBJECT_IS_SEPARATOR_ICON (pIcon))
		{
			int iWidth, iHeight;
			cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
			cairo_dock_apply_image_buffer_surface_at_size (&g_pIconBackgroundBuffer, ctx, iWidth, iHeight, 0, 0, 1);
			pIcon->bNeedApplyBackground = FALSE;
		}
	}
	
	return ctx;
}

void cairo_dock_end_draw_icon_cairo (Icon *pIcon)
{
	cairo_dock_end_draw_image_buffer_cairo (&pIcon->image);
}

gboolean cairo_dock_begin_draw_icon (Icon *pIcon, gint iRenderingMode)
{
	gboolean r = cairo_dock_begin_draw_image_buffer_opengl (&pIcon->image, pIcon->pContainer, iRenderingMode);
	
	if (r && iRenderingMode != 1)
	{
		if (g_pIconBackgroundBuffer.iTexture != 0
		&& ! GLDI_OBJECT_IS_SEPARATOR_ICON (pIcon))
		{
			int iWidth, iHeight;
			cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
			_cairo_dock_enable_texture ();
			_cairo_dock_set_blend_pbuffer ();
			_cairo_dock_set_alpha (1.);
			_cairo_dock_apply_texture_at_size (g_pIconBackgroundBuffer.iTexture, iWidth, iHeight);
			_cairo_dock_disable_texture ();
			pIcon->bNeedApplyBackground = FALSE;
		}
	}
	
	pIcon->bDamaged = !r;
	
	return r;
}

void cairo_dock_end_draw_icon (Icon *pIcon)
{
	cairo_dock_end_draw_image_buffer_opengl (&pIcon->image, pIcon->pContainer);
}


void gldi_theme_icon_write_container_name_in_conf_file (Icon *pIcon, const gchar *cParentDockName)
{
	if (GLDI_OBJECT_IS_USER_ICON (pIcon))
	{
		g_return_if_fail (pIcon->cDesktopFileName != NULL);
		gchar *cDesktopFilePath = *pIcon->cDesktopFileName == '/' ? g_strdup (pIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pIcon->cDesktopFileName);
		cairo_dock_update_conf_file (cDesktopFilePath,
			G_TYPE_STRING, "Desktop Entry", "Container", cParentDockName,
			G_TYPE_INVALID);
		g_free (cDesktopFilePath);
	}
	else if (GLDI_OBJECT_IS_APPLET_ICON (pIcon))
	{
		cairo_dock_update_conf_file (pIcon->pModuleInstance->cConfFilePath,
			G_TYPE_STRING, "Icon", "dock name", cParentDockName,
			G_TYPE_INVALID);
	}
}

void gldi_theme_icon_write_order_in_conf_file (Icon *pIcon, double fOrder)
{
	if (GLDI_OBJECT_IS_USER_ICON (pIcon))
	{
		g_return_if_fail (pIcon->cDesktopFileName != NULL);
		gchar *cDesktopFilePath = *pIcon->cDesktopFileName == '/' ? g_strdup (pIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pIcon->cDesktopFileName);
		cairo_dock_update_conf_file (cDesktopFilePath,
			G_TYPE_DOUBLE, "Desktop Entry", "Order", fOrder,
			G_TYPE_INVALID);
		g_free (cDesktopFilePath);
	}
	else if (GLDI_OBJECT_IS_APPLET_ICON (pIcon))
	{
		cairo_dock_update_conf_file (pIcon->pModuleInstance->cConfFilePath,
			G_TYPE_DOUBLE, "Icon", "order", fOrder,
			G_TYPE_INVALID);
	}
}



gboolean gldi_icon_launch_command (Icon *pIcon)
{
	// notify startup
	gldi_class_startup_notify (pIcon);

	GDesktopAppInfo *app = pIcon->pCustomLauncher;
	if (!app && pIcon->pAppInfo) app = pIcon->pAppInfo->app;
	if (app)
	{
		cd_debug ("launching app from desktop file info: %s", pIcon->cClass);
		return cairo_dock_launch_app_info (app);
	}
	cd_warning ("cannot launch icon with no app associated to it!");
	return FALSE;
}
