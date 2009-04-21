/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
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
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-load.h"
#include "cairo-dock-container.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-icons.h"

extern gchar *g_cCurrentLaunchersPath;
extern gboolean g_bUseOpenGL;


void cairo_dock_free_icon (Icon *icon)
{
	if (icon == NULL)
		return ;
	cd_debug ("%s (%s , %s)", __func__, icon->acName, icon->cClass);
	
	cairo_dock_remove_dialog_if_any (icon);
	if (CAIRO_DOCK_IS_NORMAL_APPLI (icon))
		cairo_dock_unregister_appli (icon);
	else if (icon->cClass != NULL)  // c'est un inhibiteur.
		cairo_dock_deinhibate_class (icon->cClass, icon);
	if (icon->pModuleInstance != NULL)
		cairo_dock_deinstanciate_module (icon->pModuleInstance);
	cairo_dock_stop_icon_animation (icon);
	
	cairo_dock_free_notification_table (icon->pNotificationsTab);
	cd_debug ("icon stopped\n");
	cairo_dock_free_icon_buffers (icon);
	cd_debug ("icon freeed\n");
	g_free (icon);
}

void cairo_dock_free_icon_buffers (Icon *icon)
{
	if (icon == NULL)
		return ;
	
	g_free (icon->acDesktopFileName);
	g_free (icon->acFileName);
	g_free (icon->acName);
	g_free (icon->cInitialName);
	g_free (icon->acCommand);
	g_free (icon->cWorkingDirectory);
	g_free (icon->cBaseURI);
	g_free (icon->cParentDockName);  // on ne liberera pas le sous-dock ici sous peine de se mordre la queue, donc il faut l'avoir fait avant.
	g_free (icon->cClass);
	g_free (icon->cQuickInfo);

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

CairoDockIconType cairo_dock_get_icon_type (Icon *icon)
{
	if (CAIRO_DOCK_IS_APPLI (icon))
		return CAIRO_DOCK_APPLI;
	else if (CAIRO_DOCK_IS_APPLET (icon))
		return CAIRO_DOCK_APPLET;
	else if (CAIRO_DOCK_IS_SEPARATOR (icon))
		return CAIRO_DOCK_SEPARATOR12;
	else
		return CAIRO_DOCK_LAUNCHER;
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
	if (icon1->acName == NULL)
		return -1;
	if (icon2->acName == NULL)
		return 1;
	gchar *cURI_1 = g_ascii_strdown (icon1->acName, -1);
	gchar *cURI_2 = g_ascii_strdown (icon2->acName, -1);
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

Icon* cairo_dock_get_first_icon_of_type (GList *pIconList, CairoDockIconType iType)
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
Icon* cairo_dock_get_last_icon_of_type (GList *pIconList, CairoDockIconType iType)
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
	int iGroupOrder = cairo_dock_get_group_order (iType);
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
	int iGroupOrder = cairo_dock_get_group_order (iType);
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

Icon *cairo_dock_get_removing_or_inserting_icon (GList *pIconList)
{
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->fPersonnalScale != 0)
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

Icon *cairo_dock_get_icon_with_command (GList *pIconList, gchar *cCommand)
{
	g_return_val_if_fail (cCommand != NULL, NULL);
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->acCommand != NULL && strncmp (icon->acCommand, cCommand, MIN (strlen (icon->acCommand), strlen (cCommand))) == 0)
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
		//cd_message ("  icon->acName : %s\n", icon->acName);
		if (icon->acName != NULL && strcmp (icon->acName, cName) == 0)
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

Icon *cairo_dock_get_icon_with_class (GList *pIconList, gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (icon) && icon->cClass != NULL && strcmp (icon->cClass, cClass) == 0)
			return icon;
	}
	return NULL;
}


void cairo_dock_get_icon_extent (Icon *pIcon, CairoContainer *pContainer, int *iWidth, int *iHeight)
{
	double fMaxScale = cairo_dock_get_max_scale (pContainer);
	double fRatio = (CAIRO_DOCK_IS_DESKLET (pContainer) ? 1. : pContainer->fRatio);  // on ne tient pas compte de l'effet de zoom initial du desklet.
	if (pContainer->bIsHorizontal)
	{
		*iWidth = (int) (pIcon->fWidth / fRatio * fMaxScale);
		*iHeight = (int) (pIcon->fHeight / fRatio * fMaxScale);
	}
	else
	{
		*iHeight = (int) (pIcon->fWidth / fRatio * fMaxScale);
		*iWidth = (int) (pIcon->fHeight / fRatio * fMaxScale);
	}
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
	if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->bUseReflect)
	{
		fReflectSize = myIcons.fReflectSize * icon->fScale * fabs (icon->fHeightFactor) + icon->fDeltaYReflection;
	}
	
	double fX = icon->fDrawX;
	fX += icon->fWidth * icon->fScale * (1 - fabs (icon->fWidthFactor))/2;
	
	double fY = icon->fDrawY;
	fY += (pContainer->bDirectionUp ? icon->fHeight * icon->fScale * (1 - icon->fHeightFactor)/2 : - fReflectSize);
	if (fY < 0)
		fY = 0;
	
	if (pContainer->bIsHorizontal)
	{
		pArea->x = (int) floor (fX);
		pArea->y = (int) floor (fY);
		pArea->width = (int) ceil (icon->fWidth * icon->fScale * fabs (icon->fWidthFactor)) + 1;
		pArea->height = (int) ceil (icon->fHeight * icon->fScale * fabs (icon->fHeightFactor) + fReflectSize);
	}
	else
	{
		pArea->x = (int) floor (fY);
		pArea->y = (int) floor (fX);
		pArea->width = ((int) ceil (icon->fHeight * icon->fScale * fabs (icon->fHeightFactor) + fReflectSize)) + 1;
		pArea->height = (int) ceil (icon->fWidth * icon->fScale * fabs (icon->fWidthFactor));
	}
}



void cairo_dock_normalize_icons_order (GList *pIconList, CairoDockIconType iType)
{
	cd_message ("%s (%d)", __func__, iType);
	int iOrder = 1;
	int iGroupOrder = cairo_dock_get_group_order (iType);
	GString *sDesktopFilePath = g_string_new ("");
	GList* ic;
	Icon *icon;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (cairo_dock_get_icon_order (icon) != iGroupOrder)
			continue;
		
		icon->fOrder = iOrder ++;
		if (icon->acDesktopFileName != NULL)
		{
			g_string_printf (sDesktopFilePath, "%s/%s", g_cCurrentLaunchersPath, icon->acDesktopFileName);
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

void cairo_dock_swap_icons (CairoDock *pDock, Icon *icon1, Icon *icon2)
{
	//g_print ("%s (%s, %s) : %.2f <-> %.2f\n", __func__, icon1->acName, icon2->acName, icon1->fOrder, icon2->fOrder);
	if (! ( (CAIRO_DOCK_IS_APPLI (icon1) && CAIRO_DOCK_IS_APPLI (icon2)) || (CAIRO_DOCK_IS_LAUNCHER (icon1) && CAIRO_DOCK_IS_LAUNCHER (icon2)) || (CAIRO_DOCK_IS_APPLET (icon1) && CAIRO_DOCK_IS_APPLET (icon2)) ) )
		return ;

	//\_________________ On intervertit les ordres des 2 lanceurs.
	double fSwap = icon1->fOrder;
	icon1->fOrder = icon2->fOrder;
	icon2->fOrder = fSwap;

	//\_________________ On change l'ordre dans les fichiers des 2 lanceurs.
	if (CAIRO_DOCK_IS_LAUNCHER (icon1))  // ce sont des lanceurs.
	{
		GError *erreur = NULL;
		gchar *cDesktopFilePath;
		GKeyFile* pKeyFile;

		if (icon1->acDesktopFileName != NULL)
		{
			cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon1->acDesktopFileName);
			pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
			if (pKeyFile == NULL)
				return ;

			g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", icon1->fOrder);
			cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
			g_key_file_free (pKeyFile);
			g_free (cDesktopFilePath);
		}

		if (icon2->acDesktopFileName != NULL)
		{
			cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon2->acDesktopFileName);
			pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
			if (pKeyFile == NULL)
				return ;

			g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", icon2->fOrder);
			cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
			g_key_file_free (pKeyFile);
			g_free (cDesktopFilePath);
		}
	}

	//\_________________ On les intervertit dans la liste.
	if (pDock->pFirstDrawnElement != NULL && (pDock->pFirstDrawnElement->data == icon1 || pDock->pFirstDrawnElement->data == icon2))
		pDock->pFirstDrawnElement = NULL;
	pDock->icons = g_list_remove (pDock->icons, icon1);
	pDock->icons = g_list_remove (pDock->icons, icon2);
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon1,
		(GCompareFunc) cairo_dock_compare_icons_order);
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon2,
		(GCompareFunc) cairo_dock_compare_icons_order);

	//\_________________ On recalcule la largeur max, qui peut avoir ete influencee par le changement d'ordre.
	cairo_dock_update_dock_size (pDock);

	//\_________________ On met a jour l'ordre des applets dans le fichier de conf.
	if (CAIRO_DOCK_IS_APPLET (icon1))
		cairo_dock_update_module_instance_order (icon1->pModuleInstance, icon1->fOrder);
	if (CAIRO_DOCK_IS_APPLET (icon2))
		cairo_dock_update_module_instance_order (icon2->pModuleInstance, icon2->fOrder);
	if (fabs (icon2->fOrder - icon1->fOrder) < 1e-3)
	{
		cairo_dock_normalize_icons_order (pDock->icons, icon1->iType);
	}
}

void cairo_dock_move_icon_after_icon (CairoDock *pDock, Icon *icon1, Icon *icon2)
{
	//g_print ("%s (%s, %.2f, %x)\n", __func__, icon1->acName, icon1->fOrder, icon2);
	///if ((icon2 != NULL) && (! ( (CAIRO_DOCK_IS_APPLI (icon1) && CAIRO_DOCK_IS_APPLI (icon2)) || (CAIRO_DOCK_IS_LAUNCHER (icon1) && CAIRO_DOCK_IS_LAUNCHER (icon2)) || (CAIRO_DOCK_IS_APPLET (icon1) && CAIRO_DOCK_IS_APPLET (icon2)) ) ))
	if ((icon2 != NULL) && fabs (cairo_dock_get_icon_order (icon1) - cairo_dock_get_icon_order (icon2)) > 1)
		return ;
	//\_________________ On change l'ordre de l'icone.
	gboolean bForceUpdate = FALSE;
	if (icon2 != NULL)
	{
		Icon *pNextIcon = cairo_dock_get_next_icon (pDock->icons, icon2);
		if (pNextIcon != NULL && fabs (pNextIcon->fOrder - icon2->fOrder) < 1e-3)
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
	if ((CAIRO_DOCK_IS_LAUNCHER (icon1) || CAIRO_DOCK_IS_SEPARATOR (icon1)) && icon1->acDesktopFileName != NULL)
	{
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon1->acDesktopFileName);
		cairo_dock_update_conf_file (cDesktopFilePath,
			G_TYPE_DOUBLE, "Desktop Entry", "Order", icon1->fOrder,
			G_TYPE_INVALID);
		g_free (cDesktopFilePath);
	}
	else if (CAIRO_DOCK_IS_APPLET (icon1))
		cairo_dock_update_module_instance_order (icon1->pModuleInstance, icon1->fOrder);

	//\_________________ On change sa place dans la liste.
	pDock->pFirstDrawnElement = NULL;
	pDock->icons = g_list_remove (pDock->icons, icon1);
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon1,
		(GCompareFunc) cairo_dock_compare_icons_order);

	//\_________________ On recalcule la largeur max, qui peut avoir ete influencee par le changement d'ordre.
	cairo_dock_update_dock_size (pDock);
	
	if (bForceUpdate)
		cairo_dock_normalize_icons_order (pDock->icons, icon1->iType);
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

	if (CAIRO_DOCK_IS_NORMAL_LAUNCHER (icon))  // icon->acDesktopFileName != NULL
	{
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->acDesktopFileName);

		GKeyFile *pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
		if (pKeyFile == NULL)
			return ;

		g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cNewParentDockName);
		cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);

		g_free (cDesktopFilePath);
		g_key_file_free (pKeyFile);
	}
	else if (CAIRO_DOCK_IS_APPLET (icon))
	{
		GKeyFile *pKeyFile = cairo_dock_open_key_file (icon->pModuleInstance->cConfFilePath);
		if (pKeyFile == NULL)
			return ;
		
		g_key_file_set_string (pKeyFile, "Icon", "dock name", cNewParentDockName);
		cairo_dock_write_keys_to_file (pKeyFile, icon->pModuleInstance->cConfFilePath);
		g_key_file_free (pKeyFile);
	}
}


void cairo_dock_set_icon_name (cairo_t *pSourceContext, const gchar *cIconName, Icon *pIcon, CairoContainer *pContainer)  // fonction proposee par Necropotame.
{
	g_return_if_fail (pIcon != NULL && pContainer != NULL);  // le contexte sera verifie plus loin.
	gchar *cUniqueName = NULL;
	
	if (pIcon->pSubDock != NULL)
	{
		cUniqueName = cairo_dock_get_unique_dock_name (cIconName);
		cIconName = cUniqueName;
		cairo_dock_rename_dock (pIcon->acName, pIcon->pSubDock, cUniqueName);
	}
	
	if (pIcon->acName != cIconName)
	{
		g_free (pIcon->acName);
		pIcon->acName = g_strdup (cIconName);
	}
	
	g_free (cUniqueName);
	
	cairo_dock_fill_one_text_buffer(
		pIcon,
		pSourceContext,
		&myLabels.iconTextDescription);
}
void cairo_dock_set_icon_name_full (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconNameFormat, ...)
{
	va_list args;
	va_start (args, cIconNameFormat);
	gchar *cFullText = g_strdup_vprintf (cIconNameFormat, args);
	cairo_dock_set_icon_name (pSourceContext, cFullText, pIcon, pContainer);
	g_free (cFullText);
	va_end (args);
}

void cairo_dock_set_quick_info (cairo_t *pSourceContext, const gchar *cQuickInfo, Icon *pIcon, double fMaxScale)
{
	g_return_if_fail (pIcon != NULL);  // le contexte sera verifie plus loin.

	if (pIcon->cQuickInfo != cQuickInfo)
	{
		g_free (pIcon->cQuickInfo);
		pIcon->cQuickInfo = g_strdup (cQuickInfo);
	}
	
	cairo_dock_fill_one_quick_info_buffer (pIcon,
		pSourceContext,
		&myLabels.quickInfoTextDescription,
		fMaxScale);
}

void cairo_dock_set_quick_info_full (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, const gchar *cQuickInfoFormat, ...)
{
	va_list args;
	va_start (args, cQuickInfoFormat);
	gchar *cFullText = g_strdup_vprintf (cQuickInfoFormat, args);
	cairo_dock_set_quick_info (pSourceContext, cFullText, pIcon, (CAIRO_DOCK_IS_DOCK (pContainer) ? (1 + myIcons.fAmplitude) / 1 : 1));
	g_free (cFullText);
	va_end (args);
}
