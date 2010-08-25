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

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cairo.h>
#include <pango/pango.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#ifdef HAVE_GLITZ
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "../config.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-load.h"
#include "cairo-dock-config.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-internal-position.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-container.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-dock-manager.h"

CairoDock *g_pMainDock;  // pointeur sur le dock principal.
gboolean g_bKeepAbove = FALSE;
CairoDockImageBuffer g_pVisibleZoneBuffer;

extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern CairoDockDesktopGeometry g_desktopGeometry;

static GHashTable *s_hDocksTable = NULL;  // table des docks existant.
static guint s_iSidPollScreenEdge = 0;
static int s_iNbPolls = 0;
static GList *s_pRootDockList = NULL;
static gboolean s_bQuickHide = FALSE;

static gboolean cairo_dock_read_root_dock_config (const gchar *cDockName, CairoDock *pDock);

  /////////////
 // MANAGER //
/////////////

void cairo_dock_init_dock_manager (void)
{
	cd_message ("");
	if (s_hDocksTable == NULL)
	{
		s_hDocksTable = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			g_free,
			NULL);  // donc on peut utiliser g_hash_table_remove plutot que g_hash_table_steal.
		
		cairo_dock_register_notification (CAIRO_DOCK_RENDER_DOCK,
			(CairoDockNotificationFunc) cairo_dock_render_dock_notification,
			CAIRO_DOCK_RUN_FIRST, NULL);
		cairo_dock_register_notification (CAIRO_DOCK_LEAVE_DOCK,
			(CairoDockNotificationFunc) cairo_dock_on_leave_dock_notification,
			CAIRO_DOCK_RUN_FIRST, NULL);
	}
	memset (&g_pVisibleZoneBuffer, 0, sizeof (CairoDockImageBuffer));
	
	g_pMainDock = NULL;
}

// LOAD //
void cairo_dock_load_visible_zone (const gchar *cVisibleZoneImageFile, int iVisibleZoneWidth, int iVisibleZoneHeight, double fVisibleZoneAlpha)
{
	cairo_dock_unload_image_buffer (&g_pVisibleZoneBuffer);
	
	cairo_dock_load_image_buffer_full (&g_pVisibleZoneBuffer,
		cVisibleZoneImageFile,
		iVisibleZoneWidth,
		iVisibleZoneHeight,
		CAIRO_DOCK_FILL_SPACE,
		fVisibleZoneAlpha);
}


// UNLOAD //
static gboolean _cairo_dock_free_one_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_free_dock (pDock);
	return TRUE;
}
void cairo_dock_reset_docks_table (void)
{
	g_hash_table_foreach_remove (s_hDocksTable, (GHRFunc) _cairo_dock_free_one_dock, NULL);  // pour pouvoir enlever les elements tout en parcourant la table.
	g_pMainDock = NULL;
	
	g_list_free (s_pRootDockList);
	s_pRootDockList = NULL;
	
	cairo_dock_unload_image_buffer (&g_pVisibleZoneBuffer);
}


CairoDock *cairo_dock_create_dock (const gchar *cDockName, const gchar *cRendererName)
{
	cd_message ("%s (%s)", __func__, cDockName);
	g_return_val_if_fail (cDockName != NULL, NULL);
	
	//\__________________ On verifie qu'il n'existe pas deja.
	CairoDock *pExistingDock = g_hash_table_lookup (s_hDocksTable, cDockName);
	if (pExistingDock != NULL)
	{
		return pExistingDock;
	}
	
	//\__________________ On cree un nouveau dock.
	CairoDock *pDock = cairo_dock_new_dock (cRendererName);
	
	if (g_bKeepAbove)
		gtk_window_set_keep_above (GTK_WINDOW (pDock->container.pWidget), g_bKeepAbove);
	if (mySystem.bUseFakeTransparency)
		gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), TRUE);
	
	//\__________________ On l'enregistre.
	if (g_hash_table_size (s_hDocksTable) == 0)  // c'est le 1er.
	{
		pDock->bIsMainDock = TRUE;
		g_pMainDock = pDock;
		pDock->bGlobalBg = TRUE;
	}
	g_hash_table_insert (s_hDocksTable, g_strdup (cDockName), pDock);
	s_pRootDockList = g_list_prepend (s_pRootDockList, pDock);
	
	//\__________________ On le positionne si ce n'est pas le main dock.
	if (! pDock->bIsMainDock)
	{
		if (cairo_dock_read_root_dock_config (cDockName, pDock))  // si pas de config (sub-dock ou config inexistante), se contente de lui mettre la meme position et renvoit FALSE.
			cairo_dock_move_resize_dock (pDock);
	}
	return pDock;
}

void cairo_dock_reference_dock (CairoDock *pDock, CairoDock *pParentDock)
{
	pDock->iRefCount ++;
	
	if (pDock->iRefCount == 1)  // il devient un sous-dock.
	{
		if (pParentDock == NULL)
			pParentDock = g_pMainDock;
		cairo_dock_make_sub_dock (pDock, pParentDock);
		
		const gchar *cDockName = cairo_dock_search_dock_name (pDock);
		cairo_dock_remove_root_dock_config (cDockName);
		
		cairo_dock_set_dock_visibility (pDock, CAIRO_DOCK_VISI_KEEP_ABOVE);  // si la visibilite n'avait pas ete mise (sub-dock), ne fera rien (vu que la visibilite par defaut est KEEP_ABOVE).
		
		s_pRootDockList = g_list_remove (s_pRootDockList, pDock);
	}
}

CairoDock *cairo_dock_create_subdock_from_scratch (GList *pIconList, gchar *cDockName, CairoDock *pParentDock)
{
	CairoDock *pSubDock = cairo_dock_create_dock (cDockName, NULL);
	g_return_val_if_fail (pSubDock != NULL, NULL);
	
	cairo_dock_reference_dock (pSubDock, pParentDock);  // on le fait tout de suite pour avoir la bonne reference avant le 'load'.
	
	pSubDock->icons = pIconList;
	if (pIconList != NULL)
	{
		Icon *icon;
		GList *ic;
		for (ic = pIconList; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			if (icon->cParentDockName == NULL)
				icon->cParentDockName = g_strdup (cDockName);
		}
		cairo_dock_load_buffers_in_one_dock (pSubDock);  // reload en idle.
	}
	return pSubDock;
}

void cairo_dock_destroy_dock (CairoDock *pDock, const gchar *cDockName)
{
	if (pDock == NULL)
		return;
	cd_debug ("%s (%s, %d)", __func__, cDockName, pDock->iRefCount);
	if (pDock->bIsMainDock)  // utiliser cairo_dock_free_all ().
		return;
	if (cairo_dock_search_dock_from_name (cDockName) != pDock)
	{
		cDockName = cairo_dock_search_dock_name (pDock);
		if (cDockName == NULL)
		{
			cd_warning ("this dock doesn't exist any more !");
			return ;
		}
		cd_warning ("dock's name mismatch !\nThe real name is %s", cDockName);
	}
	pDock->iRefCount --;
	if (pDock->iRefCount > 0)
		return ;
	
	Icon *pPointedIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
	if (pPointedIcon != NULL)
		pPointedIcon->pSubDock = NULL;
	
	if (pDock->iRefCount == -1 && ! pDock->bIsMainDock)  // c'etait un dock racine.
		cairo_dock_remove_root_dock_config (cDockName);
	
	g_hash_table_remove (s_hDocksTable, cDockName);
	s_pRootDockList = g_list_remove (s_pRootDockList, pDock);
	
	// s'il etait en train de scruter la souris, on l'arrete.
	if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP ||
		pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY ||
		pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE ||
		pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW)
	{
		cairo_dock_stop_polling_screen_edge ();
	}
	
	cairo_dock_free_dock (pDock);
	
	cairo_dock_trigger_refresh_launcher_gui ();
}


static void _cairo_dock_stop_polling_screen_edge (void)
{
	if (s_iSidPollScreenEdge != 0)
	{
		g_source_remove (s_iSidPollScreenEdge);
		s_iSidPollScreenEdge = 0;
	}
	s_iNbPolls = 0;
}

void cairo_dock_free_all (void)
{
	if (g_pMainDock == NULL)
		return ;

	cairo_dock_deactivate_all_modules ();  // y compris les modules qui n'ont pas d'icone.
	
	cairo_dock_reset_class_table ();  // enleve aussi les inhibiteurs.
	cairo_dock_stop_application_manager ();
	
	_cairo_dock_stop_polling_screen_edge ();
	s_bQuickHide = FALSE;
	
	cairo_dock_reset_docks_table ();  // detruit tous les docks, vide la table, et met le main-dock a NULL.
	
	cairo_dock_unload_additionnal_textures ();
}



static gboolean _check_dock_match (gchar *cDockName, CairoDock *pDock, gpointer *data)
{
	if (pDock == data[0])
	{
		* ((gchar **) data[1]) = cDockName;
		return TRUE;
	}
	else
		return FALSE;
}
const gchar *cairo_dock_search_dock_name (CairoDock *pDock)
{
	gchar *cDockName = NULL;
	gpointer data[2] = {pDock, &cDockName};

	g_hash_table_find (s_hDocksTable, (GHRFunc)_check_dock_match, data);
	return cDockName;
}

CairoDock *cairo_dock_search_dock_from_name (const gchar *cDockName)
{
	if (cDockName == NULL)
		return NULL;
	return g_hash_table_lookup (s_hDocksTable, cDockName);
}

static gboolean _cairo_dock_search_icon_from_subdock (gchar *cDockName, CairoDock *pDock, gpointer *data)
{
	if (pDock == data[0])
		return FALSE;
	Icon **pIconFound = data[1];
	CairoDock **pDockFound = data[2];
	Icon *icon = cairo_dock_get_icon_with_subdock (pDock->icons, data[0]);
	if (icon != NULL)
	{
		*pIconFound = icon;
		if (pDockFound != NULL)
			*pDockFound = pDock;
		return TRUE;
	}
	else
		return FALSE;
}
Icon *cairo_dock_search_icon_pointing_on_dock (CairoDock *pDock, CairoDock **pParentDock)  // pParentDock peut etre NULL.
{
	if (pDock->bIsMainDock)  // par definition. On n'utilise pas iRefCount, car si on est en train de detruire un dock, sa reference est deja decrementee. C'est dommage mais c'est comme ca.
		return NULL;
	Icon *pPointingIcon = NULL;
	gpointer data[3] = {pDock, &pPointingIcon, pParentDock};
	g_hash_table_find (s_hDocksTable, (GHRFunc)_cairo_dock_search_icon_from_subdock, data);
	return pPointingIcon;
}

gchar *cairo_dock_get_unique_dock_name (const gchar *cPrefix)
{
	const gchar *cNamepattern = (cPrefix != NULL && *cPrefix != '\0' && strcmp (cPrefix, "cairo-dock") != 0 ? cPrefix : "dock");
	GString *sNameString = g_string_new (cNamepattern);
	
	int i = 2;
	while (g_hash_table_lookup (s_hDocksTable, sNameString->str) != NULL)
	{
		g_string_printf (sNameString, "%s-%d", cNamepattern, i);
		i ++;
	}
	
	gchar *cUniqueName = sNameString->str;
	g_string_free (sNameString, FALSE);
	return cUniqueName;
}

gboolean cairo_dock_check_unique_subdock_name (Icon *pIcon)
{
	cd_debug ("%s (%s)", __func__, pIcon->cName);
	gchar *cUniqueName = cairo_dock_get_unique_dock_name (pIcon->cName);
	if (pIcon->cName == NULL || strcmp (pIcon->cName, cUniqueName) != 0)
	{
		g_free (pIcon->cName);
		pIcon->cName = cUniqueName;
		cd_debug (" cName <- %s", cUniqueName);
		return TRUE;
	}
	return FALSE;
}

CairoDock *cairo_dock_alter_dock_name (const gchar *cDockName, CairoDock *pDock, const gchar *cNewName)
{
	g_return_val_if_fail (cDockName != NULL && cNewName != NULL, NULL);
	if (pDock == NULL)
	{
		pDock = g_hash_table_lookup (s_hDocksTable, cDockName);
		g_return_val_if_fail (pDock != NULL, NULL);
	}
	
	g_hash_table_remove (s_hDocksTable, cDockName);  // libere la cle, mais pas la valeur puisque la GDestroyFunc est a NULL.
	g_hash_table_insert (s_hDocksTable, g_strdup (cNewName), pDock);
	
	return pDock;
}

void cairo_dock_rename_dock (const gchar *cDockName, CairoDock *pDock, const gchar *cNewName)
{
	if (cDockName == NULL)
		cDockName = cairo_dock_search_dock_name (pDock);
	pDock = cairo_dock_alter_dock_name (cDockName, pDock, cNewName);
	g_return_if_fail (pDock != NULL);
	
	GList* ic;
	Icon *icon;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		cairo_dock_update_icon_s_container_name (icon, cNewName);
	}
}


static void _cairo_dock_foreach_icons_in_dock (gchar *cDockName, CairoDock *pDock, gpointer *data)
{
	CairoDockForeachIconFunc pFunction = data[0];
	gpointer pUserData = data[1];
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		pFunction ((Icon*)ic->data, CAIRO_CONTAINER (pDock), pUserData);
	}
}
static gboolean _cairo_dock_foreach_icons_in_desklet (CairoDesklet *pDesklet, gpointer *data)
{
	CairoDockForeachIconFunc pFunction = data[0];
	gpointer pUserData = data[1];
	if (pDesklet->pIcon != NULL)
		pFunction (pDesklet->pIcon, CAIRO_CONTAINER (pDesklet), pUserData);
	Icon *pIcon;
	GList *ic;
	for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
	{
		pFunction ((Icon*)ic->data, CAIRO_CONTAINER (pDesklet), pUserData);
	}
	return FALSE;
}
void cairo_dock_foreach_icons (CairoDockForeachIconFunc pFunction, gpointer pUserData)
{
	gpointer data[2] = {pFunction, pUserData};
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_foreach_icons_in_dock, data);
	cairo_dock_foreach_desklet ((CairoDockForeachDeskletFunc) _cairo_dock_foreach_icons_in_desklet, data);
}
void cairo_dock_foreach_icons_in_docks (CairoDockForeachIconFunc pFunction, gpointer pUserData)
{
	gpointer data[2] = {pFunction, pUserData};
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_foreach_icons_in_dock, data);
}
void cairo_dock_foreach_icons_in_desklets (CairoDockForeachIconFunc pFunction, gpointer pUserData)
{
	gpointer data[2] = {pFunction, pUserData};
	cairo_dock_foreach_desklet ((CairoDockForeachDeskletFunc) _cairo_dock_foreach_icons_in_desklet, data);
}


void cairo_dock_foreach_docks (GHFunc pFunction, gpointer data)
{
	g_hash_table_foreach (s_hDocksTable, pFunction, data);
}

void cairo_dock_foreach_root_docks (GFunc pFunction, gpointer data)
{
	g_list_foreach (s_pRootDockList, pFunction, data);
}


static gboolean _cairo_dock_hide_dock_if_parent (gchar *cDockName, CairoDock *pDock, CairoDock *pChildDock)
{
	if (pDock->container.bInside)
		return FALSE;
		
	Icon *pPointedIcon = cairo_dock_get_icon_with_subdock (pDock->icons, pChildDock);
	if (pPointedIcon != NULL)
	{
		//g_print (" il faut cacher ce dock parent (%d)\n", pDock->iRefCount);
		if (pDock->iRefCount == 0)
		{
			cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
		}
		else
		{
			if (pDock->iScrollOffset != 0)  // on remet systematiquement a 0 l'offset pour les containers.
			{
				pDock->iScrollOffset = 0;
				pDock->container.iMouseX = pDock->container.iWidth / 2;  // utile ?
				pDock->container.iMouseY = 0;
				cairo_dock_calculate_dock_icons (pDock);
			}

			//cd_message ("on cache %s par parente", cDockName);
			gtk_widget_hide (pDock->container.pWidget);
			cairo_dock_hide_parent_dock (pDock);
		}
		return TRUE;
	}
	return FALSE;
}
void cairo_dock_hide_parent_dock (CairoDock *pDock)
{
	g_hash_table_find (s_hDocksTable, (GHRFunc)_cairo_dock_hide_dock_if_parent, pDock);
}

gboolean cairo_dock_hide_child_docks (CairoDock *pDock)
{
	GList* ic;
	Icon *icon;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pSubDock == NULL)
			continue;
		if (GTK_WIDGET_VISIBLE (icon->pSubDock->container.pWidget))
		{
			if (icon->pSubDock->container.bInside)
			{
				//cd_debug ("on est dans le sous-dock, donc on ne le cache pas");
				pDock->container.bInside = FALSE;
				//pDock->bAtTop = FALSE;
				return FALSE;
			}
			else if (icon->pSubDock->iSidLeaveDemand == 0)  // si on sort du dock sans passer par le sous-dock, par exemple en sortant par le bas.
			{
				//cd_debug ("on cache %s par filiation", icon->cName);
				icon->pSubDock->iScrollOffset = 0;
				icon->pSubDock->fFoldingFactor = (mySystem.bAnimateSubDock ? 1 : 0);  /// 0
				gtk_widget_hide (icon->pSubDock->container.pWidget);
			}
		}
	}
	return TRUE;
}


void cairo_dock_reload_buffers_in_all_docks (gboolean bReloadAppletsToo)
{
	g_hash_table_foreach (s_hDocksTable, (GHFunc) cairo_dock_reload_buffers_in_dock, GINT_TO_POINTER (bReloadAppletsToo));
	
	cairo_dock_draw_subdock_icons ();
}


static void _cairo_dock_draw_one_subdock_icon (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pSubDock != NULL && (icon->iSubdockViewType != 0 || (icon->cClass != NULL && !myIndicators.bUseClassIndic)) && CAIRO_DOCK_IS_LAUNCHER (icon) && icon->iSidRedrawSubdockContent == 0)  // icone de sous-dock ou de repertoire ou de classe.
		{
			cairo_dock_trigger_redraw_subdock_content_on_icon (icon);
			///cairo_dock_draw_subdock_content_on_icon (icon, pDock);
		}
	}
}
void cairo_dock_draw_subdock_icons (void)
{
	//g_print ("%s ()\n", __func__);
	g_hash_table_foreach (s_hDocksTable, (GHFunc)_cairo_dock_draw_one_subdock_icon, NULL);
}


static void _cairo_dock_reset_one_dock_view (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_set_renderer (pDock, NULL);  // on met NULL plutot que CAIRO_DOCK_DEFAULT_RENDERER_NAME pour ne pas ecraser le nom de la vue.
}
void cairo_dock_reset_all_views (void)
{
	//g_print ("%s ()\n", __func__);
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_reset_one_dock_view, NULL);
}

static void _cairo_dock_set_one_dock_view_to_default (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	//g_print ("%s (%s)\n", __func__, cDockName);
	int iDockType = GPOINTER_TO_INT (data);
	if (iDockType == 0 || (iDockType == 1 && pDock->iRefCount == 0) || (iDockType == 2 && pDock->iRefCount != 0))
	{
		cairo_dock_set_default_renderer (pDock);
		cairo_dock_update_dock_size (pDock);
		pDock->pRenderer->calculate_icons (pDock);
		if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_RESERVE)
			cairo_dock_reserve_space_for_dock (pDock, TRUE);
	}
}
void cairo_dock_set_all_views_to_default (int iDockType)
{
	//g_print ("%s ()\n", __func__);
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_set_one_dock_view_to_default, GINT_TO_POINTER (iDockType));
}


  //////////////////////
 // ROOT DOCK CONFIG //
//////////////////////

void cairo_dock_write_root_dock_gaps (CairoDock *pDock)
{
	if (pDock->iRefCount > 0)
		return;
	
	cairo_dock_prevent_dock_from_out_of_screen (pDock);
	if (pDock->bIsMainDock)
	{
		cairo_dock_update_conf_file_with_position (g_cConfFile, pDock->iGapX, pDock->iGapY);
	}
	else
	{
		const gchar *cDockName = cairo_dock_search_dock_name (pDock);
		gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
		if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
		{
			gchar *cCommand = g_strdup_printf ("cp '%s/%s' '%s'", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_MAIN_DOCK_CONF_FILE, cConfFilePath);
			int r = system (cCommand);
			g_free (cCommand);
		}
		
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_INT, "Behavior", "x gap", pDock->iGapX,
			G_TYPE_INT, "Behavior", "y gap", pDock->iGapY,
			G_TYPE_INVALID);
		g_free (cConfFilePath);
	}
}

static gboolean cairo_dock_read_root_dock_config (const gchar *cDockName, CairoDock *pDock)
{
	g_return_val_if_fail (cDockName != NULL && pDock != NULL, FALSE);
	if (pDock->iRefCount > 0)
		return FALSE;
	if (strcmp (cDockName, "cairo-dock") == 0 || pDock->bIsMainDock)
		return TRUE;
	
	//\______________ On verifie la presence du fichier de conf associe.
	//g_print ("%s (%s)\n", __func__, cDockName);
	gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))  // pas encore de fichier de conf pour ce dock.
	{
		pDock->container.bIsHorizontal = g_pMainDock->container.bIsHorizontal;
		pDock->container.bDirectionUp = g_pMainDock->container.bDirectionUp;
		pDock->fAlign = g_pMainDock->fAlign;
		
		g_free (cConfFilePath);
		return FALSE;
	}
	
	//\______________ On ouvre le fichier de conf.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath ? cConfFilePath : g_cConfFile);
	if (pKeyFile == NULL)
	{
		cd_warning ("wrong conf file (%s) !", cConfFilePath);
		g_free (cConfFilePath);
		return FALSE;
	}
	
	//\______________ Position.
	gboolean bFlushConfFileNeeded = FALSE;
	pDock->iGapX = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "x gap", &bFlushConfFileNeeded, 0, "Position", NULL);
	pDock->iGapY = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "y gap", &bFlushConfFileNeeded, 0, "Position", NULL);
	
	CairoDockPositionType iScreenBorder = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "screen border", &bFlushConfFileNeeded, 0, "Position", NULL);
	cairo_dock_set_dock_orientation (pDock, iScreenBorder);
	
	pDock->fAlign = cairo_dock_get_double_key_value (pKeyFile, "Behavior", "alignment", &bFlushConfFileNeeded, 0.5, "Position", NULL);
	
	if (myPosition.bUseXinerama)
	{
		int iNumScreen = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "num screen", &bFlushConfFileNeeded, 0, "Position", NULL);
		pDock->iNumScreen = iNumScreen;
		cairo_dock_get_screen_offsets (iNumScreen, &pDock->iScreenOffsetX, &pDock->iScreenOffsetY);
	}
	else
		pDock->iNumScreen = pDock->iScreenOffsetX = pDock->iScreenOffsetY = 0;
	
	//\______________ Visibilite.
	CairoDockVisibility iVisibility = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "visibility", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	cairo_dock_set_dock_visibility (pDock, iVisibility);
	
	//\______________ Vue.
	g_free (pDock->cRendererName);
	pDock->cRendererName = cairo_dock_get_string_key_value (pKeyFile, "Appearance", "main dock view", &bFlushConfFileNeeded, NULL, "Views", NULL);
	
	//\______________ Background.
	int iBgType = cairo_dock_get_integer_key_value (pKeyFile, "Appearance", "fill bg", &bFlushConfFileNeeded, 0, NULL, NULL);
	
	pDock->bGlobalBg = (iBgType == 0);
	
	if (!pDock->bGlobalBg)
	{
		if (iBgType == 1)
		{
			gchar *cBgImage = cairo_dock_get_string_key_value (pKeyFile, "Appearance", "background image", &bFlushConfFileNeeded, NULL, NULL, NULL);
			g_free (pDock->cBgImagePath);
			if (cBgImage != NULL)
			{
				pDock->cBgImagePath = cairo_dock_generate_file_path (cBgImage);
				g_free (cBgImage);
			}
			else
				pDock->cBgImagePath = NULL;
			
			pDock->bBgImageRepeat = cairo_dock_get_boolean_key_value (pKeyFile, "Appearance", "repeat image", &bFlushConfFileNeeded, FALSE, NULL, NULL);
		}
		// on recupere la couleur tout le temps pour avoir un plan B.
		double couleur[4] = {.7, .7, 1., .7};
		cairo_dock_get_double_list_key_value (pKeyFile, "Appearance", "stripes color dark", &bFlushConfFileNeeded, pDock->fBgColorDark, 4, couleur, NULL, NULL);
		
		double couleur2[4] = {.7, .9, .7, .4};
		cairo_dock_get_double_list_key_value (pKeyFile, "Appearance", "stripes color bright", &bFlushConfFileNeeded, pDock->fBgColorBright, 4, couleur2, NULL, NULL);
	}
	
	
	//\______________ On met a jour le fichier de conf.
	if (! bFlushConfFileNeeded)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, GLDI_VERSION);
	if (bFlushConfFileNeeded)
	{
		cairo_dock_flush_conf_file (pKeyFile, cConfFilePath, CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_MAIN_DOCK_CONF_FILE);
	}
	
	g_key_file_free (pKeyFile);
	g_free (cConfFilePath);
	return TRUE;
}

void cairo_dock_remove_root_dock_config (const gchar *cDockName)
{
	if (! cDockName || strcmp (cDockName, "cairo-dock") == 0)
		return ;
	gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
	if (g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
	{
		g_remove (cConfFilePath);
	}
	g_free (cConfFilePath);
}

void cairo_dock_add_root_dock_config_for_name (const gchar *cDockName)
{
	// on cree le fichier de conf a partir du template.
	gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
	gchar *cCommand = g_strdup_printf ("cp '%s/%s' '%s'", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_MAIN_DOCK_CONF_FILE, cConfFilePath);
	int r = system (cCommand);
	g_free (cCommand);
	
	// on placera le nouveau dock a l'oppose du main dock, meme ecran et meme visibilite.
	cairo_dock_update_conf_file (cConfFilePath,
		G_TYPE_INT, "Behavior", "screen border",
		(g_pMainDock->container.bIsHorizontal ?
			(g_pMainDock->container.bDirectionUp ? 1 : 0) :
			(g_pMainDock->container.bDirectionUp ? 3 : 2)),
		G_TYPE_INT, "Behavior", "visibility",
		g_pMainDock->iVisibility,
		G_TYPE_INT, "Behavior", "num screen",
		g_pMainDock->iNumScreen,
		G_TYPE_INVALID);
	g_free (cConfFilePath);
}

gchar *cairo_dock_add_root_dock_config (void)
{
	// on genere un nom unique.
	gchar *cValidDockName = cairo_dock_get_unique_dock_name (CAIRO_DOCK_MAIN_DOCK_NAME);  // meme nom que le main dock avec un numero en plus, plus facile pour les reperer.
	
	// on genere un fichier de conf pour ce nom.
	cairo_dock_add_root_dock_config_for_name (cValidDockName);
	
	return cValidDockName;
}

void cairo_dock_reload_one_root_dock (const gchar *cDockName, CairoDock *pDock)
{
	cairo_dock_read_root_dock_config (cDockName, pDock);
	
	cairo_dock_load_buffers_in_one_dock (pDock);  // recharge les icones et les applets.
	
	pDock->backgroundBuffer.iWidth ++;  // pour forcer le chargement du fond.
	cairo_dock_set_default_renderer (pDock);
	cairo_dock_update_dock_size (pDock);
	cairo_dock_calculate_dock_icons (pDock);
	
	cairo_dock_move_resize_dock (pDock);
	if (pDock->iVisibility == CAIRO_DOCK_VISI_RESERVE)  // la position/taille a change, il faut refaire la reservation.
		cairo_dock_reserve_space_for_dock (pDock, TRUE);
	gtk_widget_queue_draw (pDock->container.pWidget);
}

static void _cairo_dock_redraw_one_root_dock (CairoDock *pDock, gpointer data)
{
	if (! (data && pDock->bIsMainDock))
	{
		gtk_widget_queue_draw (pDock->container.pWidget);
	}
}
void cairo_dock_redraw_root_docks (gboolean bExceptMainDock)
{
	g_list_foreach (s_pRootDockList, (GFunc)_cairo_dock_redraw_one_root_dock, GINT_TO_POINTER (bExceptMainDock));
}

static void _cairo_dock_reposition_one_root_dock (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount == 0 && ! (data && pDock->bIsMainDock))
	{
		if (!pDock->bIsMainDock)
			cairo_dock_read_root_dock_config (cDockName, pDock);  // relit toute la conf.
		else
		{
			if (myPosition.bUseXinerama)
				cairo_dock_get_screen_offsets (pDock->iNumScreen, &pDock->iScreenOffsetX, &pDock->iScreenOffsetY);
			else
				pDock->iNumScreen = pDock->iScreenOffsetX = pDock->iScreenOffsetY = 0;
		}
		cairo_dock_update_dock_size (pDock);  // la taille max du dock depend de la taille de l'ecran, donc on recalcule son ratio.
		cairo_dock_move_resize_dock (pDock);
		gtk_widget_queue_draw (pDock->container.pWidget);
	}
}
void cairo_dock_reposition_root_docks (gboolean bExceptMainDock)
{
	g_hash_table_foreach (s_hDocksTable, (GHFunc)_cairo_dock_reposition_one_root_dock, GINT_TO_POINTER (bExceptMainDock));
}

void cairo_dock_synchronize_one_sub_dock_orientation (CairoDock *pSubDock, CairoDock *pDock, gboolean bReloadBuffersIfNecessary)
{
	if (pSubDock->container.bDirectionUp != pDock->container.bDirectionUp || pSubDock->container.bIsHorizontal != pDock->container.bIsHorizontal)
	{
		pSubDock->container.bDirectionUp = pDock->container.bDirectionUp;
		pSubDock->container.bIsHorizontal = pDock->container.bIsHorizontal;
		pSubDock->iScreenOffsetX = pDock->iScreenOffsetX;
		pSubDock->iScreenOffsetY = pDock->iScreenOffsetY;
		if (bReloadBuffersIfNecessary)
			cairo_dock_reload_reflects_in_dock (pSubDock);
		cairo_dock_update_dock_size (pSubDock);
		
		cairo_dock_synchronize_sub_docks_orientation (pSubDock, bReloadBuffersIfNecessary);
	}
}

void cairo_dock_synchronize_sub_docks_orientation (CairoDock *pDock, gboolean bReloadBuffersIfNecessary)
{
	GList* ic;
	Icon *icon;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pSubDock != NULL)
			cairo_dock_synchronize_one_sub_dock_orientation (icon->pSubDock, pDock, bReloadBuffersIfNecessary);
	}
}

void cairo_dock_set_dock_orientation (CairoDock *pDock, CairoDockPositionType iScreenBorder)
{
	CairoDockTypeHorizontality bWasHorizontal = pDock->container.bIsHorizontal;
	gboolean bWasDirectionUp = pDock->container.bDirectionUp;
	switch (iScreenBorder)
	{
		case CAIRO_DOCK_BOTTOM :
			pDock->container.bIsHorizontal = CAIRO_DOCK_HORIZONTAL;
			pDock->container.bDirectionUp = TRUE;
		break;
		case CAIRO_DOCK_TOP :
			pDock->container.bIsHorizontal = CAIRO_DOCK_HORIZONTAL;
			pDock->container.bDirectionUp = FALSE;
		break;
		case CAIRO_DOCK_RIGHT :
			pDock->container.bIsHorizontal = CAIRO_DOCK_VERTICAL;
			pDock->container.bDirectionUp = TRUE;
		break;
		case CAIRO_DOCK_LEFT :
			pDock->container.bIsHorizontal = CAIRO_DOCK_VERTICAL;
			pDock->container.bDirectionUp = FALSE;
		break;
	}
	cairo_dock_synchronize_sub_docks_orientation (pDock, FALSE);  // synchronize l'orientation et l'offset Xinerama des sous-docks ainsi que les reflets cairo.
}


  ////////////////
 // VISIBILITY //
////////////////

static void _cairo_dock_quick_hide_one_root_dock (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount == 0)
	{
		pDock->bAutoHide = TRUE;
		cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
	}
}
void cairo_dock_quick_hide_all_docks (void)
{
	if (! s_bQuickHide)
	{
		s_bQuickHide = TRUE;
		g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_quick_hide_one_root_dock, NULL);
		cairo_dock_start_polling_screen_edge ();
	}
}

static void _cairo_dock_stop_quick_hide_one_root_dock (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount == 0 && ! pDock->bTemporaryHidden && pDock->bAutoHide)
	{
		pDock->bAutoHide = FALSE;
		
		if (! pDock->container.bInside)  // on le fait re-apparaitre.
		{
			cairo_dock_start_showing (pDock);  // l'input shape sera mise lors du configure.
		}
	}
}
void cairo_dock_stop_quick_hide (void)
{
	if (s_bQuickHide)
	{
		s_bQuickHide = FALSE;
		cairo_dock_stop_polling_screen_edge ();
		
		g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_stop_quick_hide_one_root_dock, NULL);
	}
}

void cairo_dock_allow_entrance (CairoDock *pDock)
{
	pDock->bEntranceDisabled = FALSE;
}

void cairo_dock_disable_entrance (CairoDock *pDock)
{
	pDock->bEntranceDisabled = TRUE;
}

gboolean cairo_dock_entrance_is_allowed (CairoDock *pDock)
{
	return (! pDock->bEntranceDisabled);
}

void cairo_dock_activate_temporary_auto_hide (CairoDock *pDock)
{
	if (pDock->iRefCount == 0 && ! pDock->bTemporaryHidden)
	{
		//g_print ("hide\n");
		pDock->bTemporaryHidden = TRUE;
		pDock->bAutoHide = TRUE;
		if (!pDock->container.bInside)  // on ne declenche pas le cachage lorsque l'on change par exemple de bureau via le switcher ou un clic sur une appli.
			cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));  // un cairo_dock_start_hiding ne cacherait pas les sous-docks.
	}
}

void cairo_dock_deactivate_temporary_auto_hide (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	if (pDock->iRefCount == 0 && pDock->bTemporaryHidden && ! s_bQuickHide)
	{
		pDock->bTemporaryHidden = FALSE;
		pDock->bAutoHide = FALSE;
		
		if (! pDock->container.bInside)  // on le fait re-apparaitre.
		{
			cairo_dock_start_showing (pDock);
		}
	}
}


static gboolean _cairo_dock_hide_back_dock (CairoDock *pDock)
{
	//g_print ("hide back\n");
	if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && ! pDock->container.bInside)
		cairo_dock_pop_down (pDock);
	else if (pDock->bAutoHide)
		cairo_dock_start_hiding (pDock);
	pDock->iSidHideBack = 0;
	return FALSE;
}
static gboolean _cairo_dock_unhide_dock_delayed (CairoDock *pDock)
{
	if (pDock->container.bInside)  // on est deja dedans, inutile de le re-montrer.
	{
		pDock->iSidUnhideDelayed = 0;
		return FALSE;
	}
	
	//g_print ("let's show this dock (%d)\n", pDock->bIsMainDock);
	if (pDock->bAutoHide)
		cairo_dock_start_showing (pDock);
	if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW)
		cairo_dock_pop_up (pDock);
	
	if (pDock->iSidHideBack == 0)  // on se recachera dans 2s si on n'est pas entre dans le dock entre-temps.
		pDock->iSidHideBack = g_timeout_add (2000, (GSourceFunc) _cairo_dock_hide_back_dock, (gpointer) pDock);
	pDock->iSidUnhideDelayed = 0;
	return FALSE;
}
static void _cairo_dock_unhide_root_dock_on_mouse_hit (CairoDock *pDock, int *pMouse)
{
	static int iPrevPointerX = -1, iPrevPointerY = -1;
	if (! pDock->bAutoHide && pDock->iVisibility != CAIRO_DOCK_VISI_KEEP_BELOW)
		return;
	
	//\________________ On recupere la position du pointeur.
	gint x, y;
	if (! pMouse[0])  // pas encore recupere le pointeur.
	{
		pMouse[0] = TRUE;
		gdk_display_get_pointer (gdk_display_get_default(), NULL, &x, &y, NULL);
		if (x == pMouse[1] && y == pMouse[2])  // le pointeur n'a pas bouge, on quitte.
		{
			pMouse[3] = TRUE;
			return ;
		}
		pMouse[3] = FALSE;
		pMouse[1] = x;
		pMouse[2] = y;
	}
	else  // le pointeur a ete recupere auparavant.
	{
		if (pMouse[3])  // position inchangee.
			return;
		x = pMouse[1];
		y = pMouse[2];
	}
	
	if (!pDock->container.bIsHorizontal)
	{
		x = pMouse[2];
		y = pMouse[1];
	}
	if (pDock->container.bDirectionUp)
	{
		y = g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] - 1 - y;
	}
	
	//\________________ On verifie les conditions.
	int x1, x2;
	gboolean bShow = FALSE;
	switch (myAccessibility.iCallbackMethod)
	{
		case 0:
		default:
			if (y != 0)
				break;
			bShow = TRUE;
		break;
		case 1:
			if (y != 0)
				break;
			x1 = pDock->container.iWindowPositionX + MIN (15, pDock->container.iWidth/3);  // on evite les coins, car c'est en fait le but de cette option (les coins peuvent etre utilises par le WM pour declencher des actions).
			x2 = pDock->container.iWindowPositionX + pDock->container.iWidth - MIN (15, pDock->container.iWidth/3);
			if (x < x1 || x > x2)
				break;
			bShow = TRUE;
		break;
		case 2:
			if (y != 0)
				break;
			if (x > 0 && x < g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] - 1)
				break ;
			bShow = TRUE;
		break;
		case 3:
			if (y > myAccessibility.iZoneHeight)
				break;
			x1 = pDock->container.iWindowPositionX + (pDock->container.iWidth - myAccessibility.iZoneWidth)/2;  // on evite les coins, car c'est en fait le but de cette option (les coins peuvent etre utilises par le WM pour declencher des actions).
			x2 = x1 + myAccessibility.iZoneWidth;
			if (x < x1 || x > x2)
				break;
			bShow = TRUE;
		break;
	}
	
	if (! bShow)
	{
		if (pDock->iSidUnhideDelayed != 0)
		{
			g_source_remove (pDock->iSidUnhideDelayed);
			pDock->iSidUnhideDelayed = 0;
		}
		return;
	}
	
	//\________________ On montre ou on programme le montrage du dock.
	//g_print (" dock will be shown (%d)\n", pDock->bIsMainDock);
	if (myAccessibility.iUnhideDockDelay != 0)  // on programme une apparition.
	{
		if (pDock->iSidUnhideDelayed == 0)
			pDock->iSidUnhideDelayed = g_timeout_add (myAccessibility.iUnhideDockDelay, (GSourceFunc) _cairo_dock_unhide_dock_delayed, (gpointer) pDock);
	}
	else  // on montre le dock tout de suite.
	{
		_cairo_dock_unhide_dock_delayed (pDock);
	}
}

static gboolean _cairo_dock_poll_screen_edge (gpointer data)  // thanks to Smidgey for the pop-up patch !
{
	static int mouse[4];
	mouse[0] = FALSE;
	
	g_list_foreach (s_pRootDockList, (GFunc) _cairo_dock_unhide_root_dock_on_mouse_hit, mouse);
	
	return TRUE;
}
void cairo_dock_start_polling_screen_edge (void)
{
	s_iNbPolls ++;
	cd_debug ("%s (%d)", __func__, s_iNbPolls);
	if (s_iSidPollScreenEdge == 0)
		s_iSidPollScreenEdge = g_timeout_add (200, (GSourceFunc) _cairo_dock_poll_screen_edge, NULL);
}

void cairo_dock_stop_polling_screen_edge (void)
{
	cd_debug ("%s (%d)", __func__, s_iNbPolls);
	s_iNbPolls --;
	if (s_iNbPolls <= 0)
	{
		_cairo_dock_stop_polling_screen_edge ();  // remet tout a 0.
	}
}

void cairo_dock_set_dock_visibility (CairoDock *pDock, CairoDockVisibility iVisibility)
{
	//\_______________ jeu de parametres.
	gboolean bReserveSpace = (iVisibility == CAIRO_DOCK_VISI_RESERVE);
	gboolean bKeepBelow = (iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW);
	gboolean bAutoHideOnOverlap = (iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP);
	gboolean bAutoHideOnAnyOverlap = (iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY);
	gboolean bAutoHide = (iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE);
	
	gboolean bReserveSpace0 = (pDock->iVisibility == CAIRO_DOCK_VISI_RESERVE);
	gboolean bKeepBelow0 = (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW);
	gboolean bAutoHideOnOverlap0 = (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP);
	gboolean bAutoHideOnAnyOverlap0 = (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY);
	gboolean bAutoHide0 = (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE);
	
	pDock->iVisibility = iVisibility;
	
	//\_______________ changement dans le Reserve Space.
	if (bReserveSpace != bReserveSpace0)
		cairo_dock_reserve_space_for_dock (pDock, bReserveSpace);
	
	//\_______________ changement dans le Keep below.
	if (bKeepBelow != bKeepBelow0)
	{
		if (bKeepBelow)
			cairo_dock_pop_down (pDock);
		else
			cairo_dock_pop_up (pDock);
	}
	
	//\_______________ changement dans l'Auto-Hide
	if (bAutoHideOnOverlap != bAutoHideOnOverlap0 ||
		bAutoHideOnAnyOverlap != bAutoHideOnAnyOverlap0 ||
		bAutoHide != bAutoHide0)
	{
		if (bAutoHide)
		{
			pDock->bTemporaryHidden = FALSE;
			pDock->bAutoHide = TRUE;
			cairo_dock_start_hiding (pDock);
		}
		else if (bAutoHideOnAnyOverlap)
		{
			cairo_dock_hide_if_any_window_overlap_or_show (pDock);
		}
		else
		{
			if (! bAutoHideOnOverlap)
			{
				pDock->bTemporaryHidden = FALSE;
				pDock->bAutoHide = FALSE;
				cairo_dock_start_showing (pDock);
			}
			if (bAutoHideOnOverlap || myAccessibility.bAutoHideOnFullScreen)
			{
				cairo_dock_hide_show_if_current_window_is_on_our_way (pDock);
			}
		}
	}
	
	//\_______________ on arrete/demarre la scrutation des bords.
	gboolean bIsPolling = (bAutoHide0 || bAutoHideOnOverlap0 || bAutoHideOnAnyOverlap0 || bKeepBelow0);
	gboolean bShouldPoll = (bAutoHide || bAutoHideOnOverlap || bAutoHideOnAnyOverlap || bKeepBelow);
	if (bIsPolling && ! bShouldPoll)
		cairo_dock_stop_polling_screen_edge ();
	else if (!bIsPolling && bShouldPoll)
		cairo_dock_start_polling_screen_edge ();
}
