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

#include "cairo-dock-applications-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-load.h"
#include "cairo-dock-config.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-internal-position.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-container.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-dock-manager.h"

extern CairoDock *g_pMainDock;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;

static GHashTable *s_hDocksTable = NULL;  // table des docks existant.
static int s_iSidPollScreenEdge = 0;


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
		}
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
	
	//\__________________ On l'enregistre.
	if (g_hash_table_size (s_hDocksTable) == 0)  // c'est le 1er (on pourrait aussi se baser sur son nom).
	{
		pDock->bIsMainDock = TRUE;
		g_pMainDock = pDock;
	}
	g_hash_table_insert (s_hDocksTable, g_strdup (cDockName), pDock);
	
	//\__________________ On le positionne si ce n'est pas le main dock.
	if (! pDock->bIsMainDock)
	{
		if (cairo_dock_get_root_dock_position (cDockName, pDock))
			cairo_dock_place_root_dock (pDock);
	}
	return pDock;
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
		cairo_dock_load_buffers_in_one_dock (pSubDock);
	}
	return pSubDock;
}

void cairo_dock_destroy_dock (CairoDock *pDock, const gchar *cDockName)
{
	if (pDock == NULL)
		return;
	cd_debug ("%s (%s, %d)", __func__, cDockName, pDock->iRefCount);
	if (pDock->bIsMainDock)  // utiliser cairo_dock_free_all_docks ().
		return;
	pDock->iRefCount --;
	if (pDock->iRefCount > 0)
		return ;
	if (cairo_dock_search_dock_from_name (cDockName) != pDock)
	{
		cDockName = cairo_dock_search_dock_name (pDock);
		cd_warning ("dock's name mismatch !\nThe real name is %s", cDockName);
	}
	
	Icon *pPointedIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
	if (pPointedIcon != NULL)
		pPointedIcon->pSubDock = NULL;
	
	if (pDock->iRefCount == -1 && ! pDock->bIsMainDock)  // c'etait un dock racine.
		cairo_dock_remove_root_dock_config (cDockName);
	
	g_hash_table_remove (s_hDocksTable, cDockName);
	
	cairo_dock_free_dock (pDock);
	
	cairo_dock_trigger_refresh_launcher_gui ();
}

static gboolean _cairo_dock_free_one_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	cairo_dock_free_dock (pDock);
	return TRUE;
}
void cairo_dock_reset_docks_table (void)
{
	g_hash_table_foreach_remove (s_hDocksTable, (GHRFunc) _cairo_dock_free_one_dock, NULL);  // pour pouvoir enlever les elements tout en parcourant la table.
	g_pMainDock = NULL;
}

void cairo_dock_free_all_docks (void)
{
	if (g_pMainDock == NULL)
		return ;

	cairo_dock_deactivate_all_modules ();  // y compris les modules qui n'ont pas d'icone.
	
	cairo_dock_reset_class_table ();  // enleve aussi les inhibiteurs.
	cairo_dock_stop_application_manager ();
	cairo_dock_stop_polling_screen_edge ();

	cairo_dock_reset_docks_table ();  // detruit tous les docks, vide la table, et met le main-dock a NULL.
	
	cairo_dock_unload_additionnal_textures ();
}



static gboolean _cairo_dock_search_dock_name_from_subdock (gchar *cDockName, CairoDock *pDock, gpointer *data)
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

	g_hash_table_find (s_hDocksTable, (GHRFunc)_cairo_dock_search_dock_name_from_subdock, data);
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


static void _cairo_dock_get_one_decoration_size (gchar *cDockName, CairoDock *pDock, int *data)
{
	if (pDock->iDecorationsWidth > data[0])
		data[0] = pDock->iDecorationsWidth;
	if (pDock->iDecorationsHeight > data[1])
		data[1] = pDock->iDecorationsHeight;
}
void cairo_dock_search_max_decorations_size (int *iWidth, int *iHeight)
{
	int data[2] = {0, 0};
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_get_one_decoration_size, &data);
	cd_message ("  decorations max : %dx%d", data[0], data[1]);
	*iWidth = data[0];
	*iHeight = data[1];
}


static gboolean _cairo_dock_hide_dock_if_parent (gchar *cDockName, CairoDock *pDock, CairoDock *pChildDock)
{
	if (pDock->container.bInside)
		return FALSE;
		
	Icon *pPointedIcon;
	///pPointedIcon = cairo_dock_get_pointed_icon (pDock->icons);
	///if (pPointedIcon == NULL || pPointedIcon->pSubDock != pChildDock)
		pPointedIcon = cairo_dock_get_icon_with_subdock (pDock->icons, pChildDock);

	if (pPointedIcon != NULL)
	{
		//g_print (" il faut cacher ce dock parent (%d)\n", pDock->iRefCount);
		if (pDock->iRefCount == 0)
		{
			cairo_dock_emit_leave_signal (pDock);
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
		if (icon->pSubDock != NULL && icon->iSubdockViewType != 0 && CAIRO_DOCK_IS_LAUNCHER (icon) && icon->iSidRedrawSubdockContent == 0)  // icone de sous-dock ou de repertoire ou de classe.
		{
			cairo_dock_draw_subdock_content_on_icon (icon, pDock);
		}
	}
}
void cairo_dock_draw_subdock_icons (void)
{
	//g_print ("%s ()\n", __func__);
	g_hash_table_foreach (s_hDocksTable, (GHFunc)_cairo_dock_draw_one_subdock_icon, NULL);
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
		if (pDock->iRefCount == 0 && myAccessibility.bReserveSpace)
			cairo_dock_reserve_space_for_dock (pDock, TRUE);
	}
}
void cairo_dock_set_all_views_to_default (int iDockType)
{
	//g_print ("%s ()\n", __func__);
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_set_one_dock_view_to_default, GINT_TO_POINTER (iDockType));
}



void cairo_dock_write_root_dock_gaps (CairoDock *pDock)
{
	if (pDock->iRefCount > 0)
		return;
	
	cairo_dock_prevent_dock_from_out_of_screen (pDock);
	g_print ("%s (%d;%d)\n", __func__, pDock->iGapX, pDock->iGapY);
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
		
		cairo_dock_update_conf_file_with_position (cConfFilePath, pDock->iGapX, pDock->iGapY);
		g_free (cConfFilePath);
	}
}

gboolean cairo_dock_get_root_dock_position (const gchar *cDockName, CairoDock *pDock)
{
	g_return_val_if_fail (cDockName != NULL && pDock != NULL, FALSE);
	if (pDock->iRefCount > 0)
		return FALSE;
	if (strcmp (cDockName, "cairo-dock") == 0)
		return TRUE;
	
	//g_print ("%s (%s)\n", __func__, cDockName);
	gchar *cConfFilePath = (pDock->bIsMainDock ? NULL : g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName));
	if (! g_file_test (cConfFilePath ? cConfFilePath : g_cConfFile, G_FILE_TEST_EXISTS))
	{
		pDock->container.bIsHorizontal = g_pMainDock->container.bIsHorizontal;
		pDock->container.bDirectionUp = g_pMainDock->container.bDirectionUp;
		pDock->fAlign = g_pMainDock->fAlign;
		
		g_free (cConfFilePath);
		return FALSE;
	}
	
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath ? cConfFilePath : g_cConfFile);
	if (pKeyFile == NULL)
	{
		cd_warning ("no conf file !");
		g_free (cConfFilePath);
		return FALSE;
	}
	gboolean bFlushConfFileNeeded = FALSE;
	pDock->iGapX = cairo_dock_get_integer_key_value (pKeyFile, "Position", "x gap", &bFlushConfFileNeeded, 0, NULL, NULL);
	pDock->iGapY = cairo_dock_get_integer_key_value (pKeyFile, "Position", "y gap", &bFlushConfFileNeeded, 0, NULL, NULL);
	
	CairoDockPositionType iScreenBorder = cairo_dock_get_integer_key_value (pKeyFile, "Position", "screen border", &bFlushConfFileNeeded, 0, NULL, NULL);
	
	switch (iScreenBorder)
	{
		case CAIRO_DOCK_BOTTOM :
		default :
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
	
	pDock->fAlign = cairo_dock_get_double_key_value (pKeyFile, "Position", "alignment", &bFlushConfFileNeeded, 0.5, NULL, NULL);
	
	if (pDock->bIsMainDock)
		pDock->bAutoHide = (cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "visibility", &bFlushConfFileNeeded, FALSE, NULL, NULL) == 3);
	else
		pDock->bAutoHide = cairo_dock_get_boolean_key_value (pKeyFile, "Position", "auto-hide", &bFlushConfFileNeeded, FALSE, "Auto-Hide", "auto-hide");
	
	if (myPosition.bUseXinerama)
	{
		int iNumScreen = cairo_dock_get_integer_key_value (pKeyFile, "Position", "num screen", &bFlushConfFileNeeded, 0, NULL, NULL);
		pDock->iNumScreen = iNumScreen;
		cairo_dock_get_screen_offsets (iNumScreen, &pDock->iScreenOffsetX, &pDock->iScreenOffsetY);
	}
	else
		pDock->iNumScreen = pDock->iScreenOffsetX = pDock->iScreenOffsetY = 0;
	
	g_free (pDock->cRendererName);
	pDock->cRendererName = cairo_dock_get_string_key_value (pKeyFile, "Views", "main dock view", &bFlushConfFileNeeded, NULL, NULL, NULL);
	
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

gchar *cairo_dock_add_root_dock_config (const gchar *cDockName)
{
	// on genere un nom unique.
	gchar *cValidDockName = cairo_dock_get_unique_dock_name (cDockName ? cDockName : "dock");
	
	// on cree le fichier de conf a partir du template.
	gchar *cCommand = g_strdup_printf ("cp '%s/%s' '%s/%s.conf'", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_MAIN_DOCK_CONF_FILE, g_cCurrentThemePath, cValidDockName);
	int r = system (cCommand);
	g_free (cCommand);
	
	// on placera le nouveau dock a l'oppose du main dock.
	gchar *cDesktopFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cValidDockName);
	cairo_dock_update_conf_file (cDesktopFilePath,
		G_TYPE_INT,
		"Position",
		"screen border",
		(g_pMainDock->container.bIsHorizontal ?
			(g_pMainDock->container.bDirectionUp ? 1 : 0) :
			(g_pMainDock->container.bDirectionUp ? 3 : 2)),
		G_TYPE_INVALID);
	g_free (cDesktopFilePath);
	
	return cValidDockName;
}

void cairo_dock_reload_one_root_dock (const gchar *cDockName, CairoDock *pDock)
{
	cairo_dock_get_root_dock_position (cDockName, pDock);
		
	cairo_dock_load_buffers_in_one_dock (pDock);  // recharge les icones et les applets.
	cairo_dock_synchronize_sub_docks_position (pDock, TRUE);
	
	cairo_dock_set_default_renderer (pDock);
	cairo_dock_update_dock_size (pDock);
	cairo_dock_calculate_dock_icons (pDock);
	
	cairo_dock_place_root_dock (pDock);
	if (myAccessibility.bReserveSpace)
		cairo_dock_reserve_space_for_dock (pDock, TRUE);
	gtk_widget_queue_draw (pDock->container.pWidget);
}

static void _cairo_dock_redraw_one_root_dock (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount == 0 && ! (data && pDock->bIsMainDock))
	{
		gtk_widget_queue_draw (pDock->container.pWidget);
	}
}
void cairo_dock_redraw_root_docks (gboolean bExceptMainDock)
{
	g_hash_table_foreach (s_hDocksTable, (GHFunc)_cairo_dock_redraw_one_root_dock, GINT_TO_POINTER (bExceptMainDock));
}

static void _cairo_dock_reposition_one_root_dock (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount == 0 && ! (data && pDock->bIsMainDock))
	{
		cairo_dock_get_root_dock_position (cDockName, pDock);  // relit toute la conf.
		cairo_dock_update_dock_size (pDock);  // la taille max du dock depend de la taille de l'ecran, donc on recalcule son ratio.
		cairo_dock_place_root_dock (pDock);
		gtk_widget_queue_draw (pDock->container.pWidget);
	}
}
void cairo_dock_reposition_root_docks (gboolean bExceptMainDock)
{
	g_hash_table_foreach (s_hDocksTable, (GHFunc)_cairo_dock_reposition_one_root_dock, GINT_TO_POINTER (bExceptMainDock));
}



static gboolean s_bTemporaryAutoHide = FALSE;
static gboolean s_bQuickHide = FALSE;

static void _cairo_dock_quick_hide_one_root_dock (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount == 0)
	{
		pDock->bAutoHideInitialValue = pDock->bAutoHide;
		pDock->bAutoHide = TRUE;
		pDock->bEntranceDisabled = TRUE;
		if (!pDock->container.bInside)  // on ne cache pas le dock si l'on change par exemple de bureau via e switcher ou un clic sur une appli.
			cairo_dock_emit_leave_signal (pDock);
	}
}
void cairo_dock_activate_temporary_auto_hide (void)
{
	//g_print ("%s (%d)\n", __func__, s_bTemporaryAutoHide);
	if (! s_bTemporaryAutoHide)
	{
		s_bTemporaryAutoHide = TRUE;
		g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_quick_hide_one_root_dock, NULL);
	}
}
void cairo_dock_quick_hide_all_docks (void)
{
	if (! s_bTemporaryAutoHide)
	{
		s_bTemporaryAutoHide = TRUE;
		s_bQuickHide = TRUE;
		g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_quick_hide_one_root_dock, NULL);
	}
}

static void _cairo_dock_stop_quick_hide_one_root_dock (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount == 0)
	{
		pDock->bAutoHide = pDock->bAutoHideInitialValue;
		
		if (! pDock->container.bInside && ! pDock->bAutoHide)  // on le fait re-apparaitre.
		{
			//g_print ("%s (%.2f)\n", __func__, pDock->fFoldingFactor);
			if (pDock->fFoldingFactor != 0)  // le dock est plie, mais on ne le fait pas grossir, et donc il ne se deplie pas, donc on le deplie d'un coup ici.
			{
				pDock->fFoldingFactor = 0;
				pDock->pRenderer->calculate_icons (pDock);
			}
			
			cairo_dock_start_showing (pDock);  // l'input shape sera mise lors du configure.
		}
	}
}
void cairo_dock_deactivate_temporary_auto_hide (void)
{
	//g_print ("%s (%d)\n", __func__, s_bTemporaryAutoHide);
	if (s_bTemporaryAutoHide)
	{
		s_bTemporaryAutoHide = FALSE;
		g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_stop_quick_hide_one_root_dock, NULL);
	}
}
void cairo_dock_stop_quick_hide (void)
{
	cd_message ("");
	if (s_bTemporaryAutoHide && s_bQuickHide && ((!myAccessibility.bAutoHideOnMaximized && !myAccessibility.bAutoHideOnFullScreen) || cairo_dock_search_window_on_our_way (g_pMainDock, myAccessibility.bAutoHideOnMaximized, myAccessibility.bAutoHideOnFullScreen) == NULL))
	{
		s_bTemporaryAutoHide = FALSE;
		g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_stop_quick_hide_one_root_dock, NULL);
	}
	s_bQuickHide = FALSE;
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

gboolean cairo_dock_quick_hide_is_activated (void)
{
	return s_bTemporaryAutoHide;
}



void cairo_dock_synchronize_one_sub_dock_position (CairoDock *pSubDock, CairoDock *pDock, gboolean bReloadBuffersIfNecessary)
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
		
		cairo_dock_synchronize_sub_docks_position (pSubDock, bReloadBuffersIfNecessary);
	}
}
void cairo_dock_synchronize_sub_docks_position (CairoDock *pDock, gboolean bReloadBuffersIfNecessary)
{
	GList* ic;
	Icon *icon;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pSubDock != NULL)
			cairo_dock_synchronize_one_sub_dock_position (icon->pSubDock, pDock, bReloadBuffersIfNecessary);
	}
}



void cairo_dock_start_polling_screen_edge (CairoDock *pMainDock)
{
	if (s_iSidPollScreenEdge == 0)
		s_iSidPollScreenEdge = g_timeout_add (333, (GSourceFunc) cairo_dock_poll_screen_edge, (gpointer) pMainDock);
}
void cairo_dock_stop_polling_screen_edge (void)
{
	if (s_iSidPollScreenEdge != 0)
	{
		g_source_remove (s_iSidPollScreenEdge);
		s_iSidPollScreenEdge = 0;
	}
}

static void _cairo_dock_pop_up_one_root_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount > 0)
		return ;
	CairoDockPositionType iScreenBorder = GPOINTER_TO_INT (data);
	
	CairoDockPositionType iDockScreenBorder = (((! pDock->container.bIsHorizontal) << 1) | (! pDock->container.bDirectionUp));
	if (iDockScreenBorder == iScreenBorder)
	{
		cd_debug ("%s passe en avant-plan", cDockName);
		cairo_dock_pop_up (pDock);
		if (pDock->iSidPopDown == 0)
			pDock->iSidPopDown = g_timeout_add (2000, (GSourceFunc) cairo_dock_pop_down, (gpointer) pDock);  // au cas ou on serait pas dedans.
	}
}
void cairo_dock_pop_up_root_docks_on_screen_edge (CairoDockPositionType iScreenBorder)
{
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_pop_up_one_root_dock, GINT_TO_POINTER (iScreenBorder));
}

static void _cairo_dock_set_one_dock_on_top_layer (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (data && pDock->iRefCount > 0)
		return ;
	cd_debug ("set on top layer");
	gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), FALSE);
}
void cairo_dock_set_docks_on_top_layer (gboolean bRootDocksOnly)
{
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_set_one_dock_on_top_layer, GINT_TO_POINTER (bRootDocksOnly));
}


static void _cairo_dock_reserve_space_for_one_dock (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount == 0)
		cairo_dock_reserve_space_for_dock (pDock, GPOINTER_TO_INT (data));
}
void cairo_dock_reserve_space_for_all_root_docks (gboolean bReserve)
{
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_reserve_space_for_one_dock, GINT_TO_POINTER (bReserve));
}


gchar *cairo_dock_get_unique_dock_name (const gchar *cPrefix)
{
	const gchar *cNamepattern = (cPrefix != NULL && *cPrefix != '\0' && strcmp (cPrefix, "cairo-dock") != 0 ? cPrefix : "dock");
	GString *sNameString = g_string_new (cNamepattern);
	
	int i = 1;
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
