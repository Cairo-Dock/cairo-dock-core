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
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>


#include "cairo-dock-icon-facility.h"  // cairo_dock_compare_icons_order
#include "cairo-dock-config.h"  // cairo_dock_update_conf_file
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-backends-manager.h"  // cairo_dock_set_renderer
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.bMixLauncherAppli
#include "cairo-dock-class-manager.h"  // cairo_dock_inhibite_class
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_mark_current_theme_as_modified
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
//#include "cairo-dock-animations.h"  // cairo_dock_launch_animation
#include "cairo-dock-launcher-factory.h"  // cairo_dock_new_launcher_icon
#include "cairo-dock-separator-manager.h"  // cairo_dock_create_separator_surface
#include "cairo-dock-X-utilities.h"  // cairo_dock_show_xwindow
#include "cairo-dock-launcher-manager.h"

extern CairoDock *g_pMainDock;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentLaunchersPath;

static CairoDock *_cairo_dock_handle_container (Icon *icon, const gchar *cRendererName)
{
	//\____________ On cree son container si necessaire.
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (pParentDock == NULL)
	{
		cd_message ("The parent dock (%s) doesn't exist: we create it", icon->cParentDockName);
		pParentDock = cairo_dock_create_dock (icon->cParentDockName);
	}
	
	//\____________ On cree son sous-dock si necessaire.
	if (icon->iTrueType == CAIRO_DOCK_ICON_TYPE_CONTAINER && icon->cName != NULL)
	{
		CairoDock *pChildDock = cairo_dock_search_dock_from_name (icon->cName);
		if (pChildDock && (pChildDock->iRefCount > 0 || pChildDock->bIsMainDock))  // un sous-dock de meme nom existe deja, on change le nom de l'icone.
		{
			gchar *cUniqueDockName = cairo_dock_get_unique_dock_name (icon->cName);
			cd_warning ("A sub-dock with the same name (%s) already exists, we'll change it to %s", icon->cName, cUniqueDockName);
			gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
			cairo_dock_update_conf_file (cDesktopFilePath,
				G_TYPE_STRING, "Desktop Entry", "Name", cUniqueDockName,
				G_TYPE_INVALID);
			g_free (cDesktopFilePath);
			g_free (icon->cName);
			icon->cName = cUniqueDockName;
			pChildDock = NULL;
		}
		if (pChildDock == NULL)
		{
			cd_message ("The child dock (%s) doesn't exist, we create it with this view: %s", icon->cName, cRendererName);
			pChildDock = cairo_dock_create_subdock (icon->cName, cRendererName, pParentDock, NULL);
		}
		else
		{
			cd_message ("The dock is now a 'child-dock' (%d, %d)", pChildDock->container.bIsHorizontal, pChildDock->container.bDirectionUp);
			cairo_dock_main_dock_to_sub_dock (pChildDock, pParentDock, cRendererName);
		}
		icon->pSubDock = pChildDock;
		if (icon->iSubdockViewType != 0)
			cairo_dock_trigger_redraw_subdock_content_on_icon (icon);
	}
	
	return pParentDock;
}

static void _load_launcher (Icon *icon)
{
	int iWidth = icon->iImageWidth;
	int iHeight = icon->iImageHeight;
	
	if (icon->pSubDock != NULL && icon->iSubdockViewType != 0)  // icone de sous-dock avec un rendu specifique, on le redessinera lorsque les icones du sous-dock auront ete chargees.
	{
		icon->pIconBuffer = cairo_dock_create_blank_surface (iWidth, iHeight);
	}
	else if (icon->cFileName)  // icone possedant une image, on affiche celle-ci.
	{
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName, MAX (iWidth, iHeight));
		if (cIconPath != NULL && *cIconPath != '\0')
			icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
				iWidth,
				iHeight);
		g_free (cIconPath);
	}
}

static void _load_user_separator (Icon *icon)
{
	int iWidth = icon->iImageWidth;
	int iHeight = icon->iImageHeight;
	
	icon->pIconBuffer = NULL;
	if (icon->cFileName != NULL)
	{
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName, MAX (iWidth, iHeight));
		if (cIconPath != NULL && *cIconPath != '\0')
		{
			icon->pIconBuffer = cairo_dock_create_surface_from_image_simple (cIconPath,
				iWidth,
				iHeight);
		}
		g_free (cIconPath);
	}
	if (icon->pIconBuffer == NULL)
	{
		icon->pIconBuffer = cairo_dock_create_separator_surface (
			iWidth,
			iHeight);
	}
}

static gboolean _delete_launcher (Icon *icon)
{
	gboolean r = FALSE;
	if (icon->cDesktopFileName != NULL && icon->cDesktopFileName[0] != '/')
	{
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
		g_remove (cDesktopFilePath);
		g_free (cDesktopFilePath);
		r = TRUE;
	}
	
	if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon) && icon->pSubDock != NULL)  /// really useful ?...
	{
		Icon *pSubIcon;
		GList *ic;
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pSubIcon = ic->data;
			if (pSubIcon->iface.on_delete)
				r |= pSubIcon->iface.on_delete (pSubIcon);
		}
		cairo_dock_destroy_dock (icon->pSubDock, icon->cName);
		icon->pSubDock = NULL;
	}
	return r;
}

static void _show_appli_for_drop (Icon *pIcon)
{
	if (pIcon->Xid != 0)
		cairo_dock_show_xwindow (pIcon->Xid);
}

Icon * cairo_dock_create_icon_from_desktop_file (const gchar *cDesktopFileName)
{
	//g_print ("%s (%s)\n", __func__, cDesktopFileName);
	
	//\____________ On cree l'icone.
	gchar *cRendererName = NULL;
	Icon *icon = cairo_dock_new_launcher_icon (cDesktopFileName, &cRendererName);
	if (icon == NULL)  // couldn't load the icon (unreadable desktop file, unvalid or does not correspond to any installed program)
		return NULL;
	
	if (icon->iTrueType == CAIRO_DOCK_ICON_TYPE_SEPARATOR)
	{
		icon->iface.load_image = _load_user_separator;
	}
	else
	{
		icon->iface.load_image = _load_launcher;
		icon->iface.action_on_drag_hover = _show_appli_for_drop;
	}
	icon->iface.on_delete = _delete_launcher;
	
	//\____________ On gere son dock et sous-dock.
	CairoDock *pParentDock = _cairo_dock_handle_container (icon, cRendererName);
	g_free (cRendererName);
	
	//\____________ On remplit ses buffers.
	///cairo_dock_trigger_load_icon_buffers (icon);
	
	//\____________ On gere son role d'inhibiteur.
	cd_message ("+ %s/%s", icon->cName, icon->cClass);
	if (icon->cClass != NULL)
	{
		cairo_dock_inhibite_class (icon->cClass, icon);  // gere le bMixLauncherAppli
	}
	
	return icon;
}


Icon * cairo_dock_create_dummy_launcher (gchar *cName, gchar *cFileName, gchar *cCommand, gchar *cQuickInfo, double fOrder)
{
	//\____________ On cree l'icone.
	gchar *cRendererName = NULL;
	Icon *pIcon = cairo_dock_new_icon ();
	pIcon->iTrueType = CAIRO_DOCK_ICON_TYPE_OTHER;
	pIcon->iGroup = CAIRO_DOCK_LAUNCHER;
	pIcon->cName = cName;
	pIcon->cFileName = cFileName;
	pIcon->cQuickInfo = cQuickInfo;
	pIcon->fOrder = fOrder;
	pIcon->fScale = 1.;
	pIcon->fAlpha = 1.;
	pIcon->fWidthFactor = 1.;
	pIcon->fHeightFactor = 1.;
	pIcon->cCommand = cCommand;
	pIcon->iface.load_image = _load_launcher;
	
	return pIcon;
}


void cairo_dock_load_launchers_from_dir (const gchar *cDirectory)
{
	cd_message ("%s (%s)", __func__, cDirectory);
	GDir *dir = g_dir_open (cDirectory, 0, NULL);
	g_return_if_fail (dir != NULL);
	
	Icon* icon;
	const gchar *cFileName;
	CairoDock *pParentDock;

	while ((cFileName = g_dir_read_name (dir)) != NULL)
	{
		if (g_str_has_suffix (cFileName, ".desktop"))
		{
			icon = cairo_dock_create_icon_from_desktop_file (cFileName);
			if (icon == NULL)  // if the icon couldn't be loaded, remove it from the theme (it's useless to try and fail to load it each time).
			{
				cd_warning ("Unable to load a valid icon from '%s/%s'; the file is either unreadable, unvalid or does not correspond to any installed program, and will be deleted", g_cCurrentLaunchersPath, cFileName);
				gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cFileName);
				g_remove (cDesktopFilePath);
				g_free (cDesktopFilePath);
				continue;
			}
			
			pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
			if (pParentDock != NULL)  // a priori toujours vrai.
			{
				cairo_dock_insert_icon_in_dock_full (icon, pParentDock, ! CAIRO_DOCK_ANIMATE_ICON, ! CAIRO_DOCK_INSERT_SEPARATOR, NULL);
			}
		}
	}
	g_dir_close (dir);
}



void cairo_dock_reload_launcher (Icon *icon)
{
	if (icon->cDesktopFileName == NULL || strcmp (icon->cDesktopFileName, "none") == 0)
	{
		cd_warning ("missing config file for icon %s", icon->cName);
		return ;
	}
	
	//\_____________ Rename sub-dock and ensure sub-dock's name unicity before we reload the icon.
	if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))
	{
		// get the new name of the icon (and therefore of its sub-dock).
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
		GKeyFile* pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
		g_return_if_fail (pKeyFile != NULL);
		gchar *cName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Name", NULL);
		
		// set a default name if none.
		if (cName == NULL || *cName == '\0')  // no name defined, we need one.
		{
			if (icon->cName != NULL)
				cName = g_strdup (icon->cName);
			else
				cName = cairo_dock_get_unique_dock_name ("sub-dock");
			g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cName);
			cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
		}
		
		// if it has changed, ensure its unicity, and rename the sub-dock to be able to link with it again.
		if (icon->cName == NULL || strcmp (icon->cName, cName) != 0)  // name has changed -> rename the sub-dock.
		{
			// ensure unicity
			gchar *cUniqueName = cairo_dock_get_unique_dock_name (cName);
			if (strcmp (cName, cUniqueName) != 0)
			{
				g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cUniqueName);
				cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
			}
			// rename sub-dock
			cd_debug ("on renomme a l'avance le sous-dock en %s", cUniqueName);
			if (icon->pSubDock != NULL)
				cairo_dock_rename_dock (icon->cName, icon->pSubDock, cUniqueName);  // on le renomme ici pour eviter de transvaser dans un nouveau dock (ca marche aussi ceci dit).
			g_free (cUniqueName);
		}
		g_free (cName);
		
		g_key_file_free (pKeyFile);
		g_free (cDesktopFilePath);
	}
	
	//\_____________ On memorise son etat.
	gchar *cPrevDockName = icon->cParentDockName;
	icon->cParentDockName = NULL;
	CairoDock *pDock = cairo_dock_search_dock_from_name (cPrevDockName);  // changement de l'ordre ou du container.
	double fOrder = icon->fOrder;
	
	gchar *cClass = icon->cClass;
	icon->cClass = NULL;
	gchar *cDesktopFileName = icon->cDesktopFileName;
	icon->cDesktopFileName = NULL;
	gchar *cName = icon->cName;
	icon->cName = NULL;
	
	//\_____________ get its new params.
	gchar *cSubDockRendererName = NULL;
	cairo_dock_load_icon_info_from_desktop_file (cDesktopFileName, icon, &cSubDockRendererName);
	g_return_if_fail (icon->cDesktopFileName != NULL);
	
	// get its (possibly new) container.
	CairoDock *pNewDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (pNewDock == NULL)
	{
		cd_message ("The parent dock (%s) doesn't exist, we create it", icon->cParentDockName);
		pNewDock = cairo_dock_create_dock (icon->cParentDockName);
	}
	g_return_if_fail (pNewDock != NULL);
	
	//\_____________ manage the change of container or order.
	if (pDock != pNewDock)  // container has changed.
	{
		// on la detache de son container actuel et on l'insere dans le nouveau.
		gchar *tmp = icon->cParentDockName;  // le detach_icon remet a 0 ce champ, il faut le donc conserver avant.
		icon->cParentDockName = NULL;
		cairo_dock_detach_icon_from_dock_full (icon, pDock, TRUE);
		
		cairo_dock_insert_icon_in_dock (icon, pNewDock, CAIRO_DOCK_ANIMATE_ICON);  // le remove et le insert vont declencher le redessin de l'icone pointant sur l'ancien et le nouveau sous-dock le cas echeant.
		icon->cParentDockName = tmp;
	}
	else  // same container, but different order.
	{
		if (icon->fOrder != fOrder)  // On gere le changement d'ordre.
		{
			pNewDock->icons = g_list_remove (pNewDock->icons, icon);
			pNewDock->icons = g_list_insert_sorted (pNewDock->icons,
				icon,
				(GCompareFunc) cairo_dock_compare_icons_order);
			cairo_dock_update_dock_size (pDock);  // -> recalculate icons and update input shape
		}
		// on redessine l'icone pointant sur le sous-dock, pour le cas ou l'ordre et/ou l'image du lanceur aurait change.
		if (pNewDock->iRefCount != 0)
		{
			cairo_dock_redraw_subdock_content (pNewDock);
		}
	}
	
	//\_____________ reload icon
	// redraw icon
	if (icon->pSubDock != NULL && icon->iSubdockViewType != 0)  // petite optimisation : vu que la taille du lanceur n'a pas change, on evite de detruire et refaire sa surface.
	{
		cairo_dock_draw_subdock_content_on_icon (icon, pNewDock);
	}
	else
	{
		cairo_dock_reload_icon_image (icon, CAIRO_CONTAINER (pNewDock));
	}
	
	// reload label
	if (cName && ! icon->cName)
		icon->cName = g_strdup (" ");
	
	if (cairo_dock_strings_differ (cName, icon->cName))
		cairo_dock_load_icon_text (icon, &myIconsParam.iconTextDescription);
	
	// set sub-dock renderer
	if (icon->pSubDock != NULL)  // son rendu a pu changer.
	{
		if (cairo_dock_strings_differ (cSubDockRendererName, icon->pSubDock->cRendererName))
		{
			cairo_dock_set_renderer (icon->pSubDock, cSubDockRendererName);
			cairo_dock_update_dock_size (icon->pSubDock);
		}
	}
	g_free (cSubDockRendererName);
	
	//\_____________ handle class inhibition.
	gchar *cNowClass = icon->cClass;
	if (cClass != NULL && (cNowClass == NULL || strcmp (cNowClass, cClass) != 0))  // la classe a change, on desinhibe l'ancienne.
	{
		icon->cClass = cClass;
		cairo_dock_deinhibite_class (cClass, icon);
		cClass = NULL;  // libere par la fonction precedente.
		icon->cClass = cNowClass;
	}
	if (myTaskbarParam.bMixLauncherAppli && cNowClass != NULL && (cClass == NULL || strcmp (cNowClass, cClass) != 0))  // la classe a change, on inhibe la nouvelle.
		cairo_dock_inhibite_class (cNowClass, icon);

	//\_____________ redraw dock.
	///cairo_dock_calculate_dock_icons (pNewDock);
	cairo_dock_redraw_container (CAIRO_CONTAINER (pNewDock));

	g_free (cPrevDockName);
	g_free (cClass);
	g_free (cDesktopFileName);
	g_free (cName);
	cairo_dock_mark_current_theme_as_modified (TRUE);
}



gchar *cairo_dock_launch_command_sync (const gchar *cCommand)
{
	gchar *standard_output=NULL, *standard_error=NULL;
	gint exit_status=0;
	GError *erreur = NULL;
	gboolean r = g_spawn_command_line_sync (cCommand,
		&standard_output,
		&standard_error,
		&exit_status,
		&erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		g_free (standard_error);
		return NULL;
	}
	if (standard_error != NULL && *standard_error != '\0')
	{
		cd_warning (standard_error);
	}
	g_free (standard_error);
	if (standard_output != NULL && *standard_output == '\0')
	{
		g_free (standard_output);
		return NULL;
	}
	if (standard_output[strlen (standard_output) - 1] == '\n')
		standard_output[strlen (standard_output) - 1] ='\0';
	return standard_output;
}

gboolean cairo_dock_launch_command_printf (const gchar *cCommandFormat, gchar *cWorkingDirectory, ...)
{
	va_list args;
	va_start (args, cWorkingDirectory);
	gchar *cCommand = g_strdup_vprintf (cCommandFormat, args);
	va_end (args);
	
	gboolean r = cairo_dock_launch_command_full (cCommand, cWorkingDirectory);
	g_free (cCommand);
	
	return r;
}

static gpointer _cairo_dock_launch_threaded (gchar *cCommand)
{
	int r;
	r = system (cCommand);
	if (r != 0)
		cd_warning ("couldn't launch this command (%s)", cCommand);
	g_free (cCommand);
	return NULL;
}
gboolean cairo_dock_launch_command_full (const gchar *cCommand, gchar *cWorkingDirectory)
{
	g_return_val_if_fail (cCommand != NULL, FALSE);
	cd_debug ("%s (%s , %s)", __func__, cCommand, cWorkingDirectory);
	
	gchar *cBGCommand = NULL;
	if (cCommand[strlen (cCommand)-1] != '&')
		cBGCommand = g_strconcat (cCommand, " &", NULL);
	
	gchar *cCommandFull = NULL;
	if (cWorkingDirectory != NULL)
	{
		cCommandFull = g_strdup_printf ("cd \"%s\" && %s", cWorkingDirectory, cBGCommand ? cBGCommand : cCommand);
		g_free (cBGCommand);
		cBGCommand = NULL;
	}
	else if (cBGCommand != NULL)
	{
		cCommandFull = cBGCommand;
		cBGCommand = NULL;
	}
	
	if (cCommandFull == NULL)
		cCommandFull = g_strdup (cCommand);
	
	GError *erreur = NULL;
	GThread* pThread = g_thread_create ((GThreadFunc) _cairo_dock_launch_threaded, cCommandFull, FALSE, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("couldn't launch this command (%s : %s)", cCommandFull, erreur->message);
		g_error_free (erreur);
		g_free (cCommandFull);
		return FALSE;
	}
	return TRUE;
}
