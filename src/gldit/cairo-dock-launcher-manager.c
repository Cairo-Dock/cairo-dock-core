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

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

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
#include "cairo-dock-animations.h"  // cairo_dock_launch_animation
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
		cd_message ("le dock parent (%s) n'existe pas, on le cree", icon->cParentDockName);
		pParentDock = cairo_dock_create_dock (icon->cParentDockName, NULL);
	}
	
	//\____________ On cree son sous-dock si necessaire.
	if (icon->iTrueType == CAIRO_DOCK_ICON_TYPE_CONTAINER && icon->cName != NULL)
	{
		CairoDock *pChildDock = cairo_dock_search_dock_from_name (icon->cName);
		if (pChildDock && pChildDock->iRefCount > 0 && pChildDock != icon->pSubDock)  // un sous-dock de meme nom existe deja, on change le nom de l'icone.
		{
			gchar *cUniqueDockName = cairo_dock_get_unique_dock_name ("New sub-dock");
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
			cd_message ("le dock fils (%s) n'existe pas, on le cree avec la vue %s", icon->cName, cRendererName);
			icon->pSubDock = cairo_dock_create_subdock_from_scratch (NULL, icon->cName, pParentDock);
		}
		else if (pChildDock != icon->pSubDock)
		{
			cairo_dock_reference_dock (pChildDock, pParentDock);
			icon->pSubDock = pChildDock;
			cd_message ("le dock devient un dock fils (%d, %d)", pChildDock->container.bIsHorizontal, pChildDock->container.bDirectionUp);
		}
		if (cRendererName != NULL && icon->pSubDock != NULL)
		{
			cairo_dock_set_renderer (icon->pSubDock, cRendererName);
			cairo_dock_update_dock_size (icon->pSubDock);
		}
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
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName);
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
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName);
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
	cairo_dock_trigger_load_icon_buffers (icon, CAIRO_CONTAINER (pParentDock));
	
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


void cairo_dock_build_docks_tree_with_desktop_files (const gchar *cDirectory)
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
				cairo_dock_insert_icon_in_dock_full (icon, pParentDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, ! CAIRO_DOCK_INSERT_SEPARATOR, NULL);
				/// synchroniser icon->pSubDock avec pParentDock ?...
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
	
	//\_____________ Ensure sub-dock's name unicity.
	if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))
	{
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
		GKeyFile* pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
		g_return_if_fail (pKeyFile != NULL);
		
		if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))  // on assure l'unicite du nom du sous-dock ici, car cela n'est volontairement pas fait dans la fonction de creation de l'icone.
		{
			gchar *cName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Name", NULL);
			if (cName == NULL || *cName == '\0')
				cName = g_strdup ("dock");
			if (icon->cName == NULL || strcmp (icon->cName, cName) != 0)  // le nom a change.
			{
				gchar *cUniqueName = cairo_dock_get_unique_dock_name (cName);
				if (strcmp (cName, cUniqueName) != 0)
				{
					g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cUniqueName);
					cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
					cd_debug ("on renomme a l'avance le sous-dock en %s", cUniqueName);
					if (icon->pSubDock != NULL)
						cairo_dock_rename_dock (icon->cName, icon->pSubDock, cUniqueName);  // on le renomme ici pour eviter de transvaser dans un nouveau dock (ca marche aussi ceci dit).
				}
				g_free (cUniqueName);
			}
			g_free (cName);
		}
		
		g_key_file_free (pKeyFile);
		g_free (cDesktopFilePath);
	}
	
	//\_____________ On memorise son etat.
	gchar *cPrevDockName = icon->cParentDockName;
	icon->cParentDockName = NULL;
	CairoDock *pDock = cairo_dock_search_dock_from_name (cPrevDockName);  // changement de l'ordre ou du container.
	double fOrder = icon->fOrder;
	
	CairoDock *pSubDock = icon->pSubDock;
	///icon->pSubDock = NULL;
	gchar *cClass = icon->cClass;
	icon->cClass = NULL;
	gchar *cDesktopFileName = icon->cDesktopFileName;
	icon->cDesktopFileName = NULL;
	gchar *cName = icon->cName;
	icon->cName = NULL;
	gchar *cRendererName = NULL;
	if (pSubDock != NULL)
	{
		cRendererName = pSubDock->cRendererName;
		pSubDock->cRendererName = NULL;
	}
	
	//\_____________ On recharge l'icone.
	gchar *cSubDockRendererName = NULL;
	cairo_dock_load_icon_info_from_desktop_file (cDesktopFileName, icon, &cSubDockRendererName);
	g_return_if_fail (icon->cDesktopFileName != NULL);
	
	CairoDock *pNewDock = _cairo_dock_handle_container (icon, cSubDockRendererName);
	g_free (cSubDockRendererName);
	g_return_if_fail (pNewDock != NULL);
	
	if (icon->pSubDock != NULL && icon->iSubdockViewType != 0)  // petite optimisation : vu que la taille du lanceur n'a pas change, on evite de detruire et refaire sa surface.
	{
		cairo_dock_draw_subdock_content_on_icon (icon, pNewDock);
	}
	else
	{
		cairo_dock_reload_icon_image (icon, CAIRO_CONTAINER (pNewDock));
	}
	
	//g_print ("icon : %.1fx%.1f", icon->fWidth, icon->fHeight);
	
	if (cName && ! icon->cName)
		icon->cName = g_strdup (" ");
	
	if (cairo_dock_strings_differ (cName, icon->cName))
		cairo_dock_load_icon_text (icon, &myIconsParam.iconTextDescription);
	
	//\_____________ On gere son sous-dock.
	if (icon->Xid != 0)
	{
		if (icon->pSubDock == NULL)
			icon->pSubDock = pSubDock;
		else  // ne devrait pas arriver (une icone de container n'est pas un lanceur pouvant prendre un Xid).
			cairo_dock_destroy_dock (pSubDock, cName);
	}
	else
	{
		if (pSubDock != NULL && pSubDock != icon->pSubDock)  // ca n'est plus le meme container, on transvase ou on detruit.
		{
			cd_debug ("on transvase dans le nouveau sous-dock");
			cairo_dock_remove_icons_from_dock (pSubDock, icon->pSubDock, icon->cName);
			cairo_dock_destroy_dock (pSubDock, cName);
			pSubDock = NULL;
		}
	}
	
	if (icon->pSubDock != NULL && pSubDock == icon->pSubDock)  // c'est le meme sous-dock, son rendu a pu changer.
	{
		if (cairo_dock_strings_differ (cRendererName, icon->pSubDock->cRendererName))
			cairo_dock_update_dock_size (icon->pSubDock);
		cairo_dock_synchronize_one_sub_dock_orientation (pSubDock, pNewDock, TRUE);
	}

	//\_____________ On gere le changement de container ou d'ordre.
	//g_print ("%x -> %x\n", pDock, pNewDock);
	if (pDock != pNewDock)  // changement de container.
	{
		// on la detache de son container actuel et on l'insere dans le nouveau.
		gchar *tmp = icon->cParentDockName;  // le detach_icon remet a 0 ce champ, il faut le donc conserver avant.
		icon->cParentDockName = NULL;
		cairo_dock_detach_icon_from_dock_full (icon, pDock, TRUE);
		if (pDock->icons == NULL && pDock->iRefCount == 0 && ! pDock->bIsMainDock)  // le dock devient vide, il se fera detruire automatiquement.
		{
			pDock = NULL;
		}
		else
		{
			cairo_dock_update_dock_size (pDock);
			cairo_dock_calculate_dock_icons (pDock);
			gtk_widget_queue_draw (pDock->container.pWidget);
		}
		cairo_dock_insert_icon_in_dock (icon, pNewDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON);  // le remove et le insert vont declencher le redessin de l'icone pointant sur l'ancien et le nouveau sous-dock le cas echeant.
		cairo_dock_launch_animation (CAIRO_CONTAINER (pNewDock));
		icon->cParentDockName = tmp;
	}
	else  // le container est identique, on gere juste le changement d'ordre.
	{
		icon->fWidth *= pNewDock->container.fRatio / pDock->container.fRatio;
		icon->fHeight *= pNewDock->container.fRatio / pDock->container.fRatio;
		if (icon->fOrder != fOrder)  // On gere le changement d'ordre.
		{
			pNewDock->pFirstDrawnElement = NULL;
			pNewDock->icons = g_list_remove (pNewDock->icons, icon);
			pNewDock->icons = g_list_insert_sorted (pNewDock->icons,
				icon,
				(GCompareFunc) cairo_dock_compare_icons_order);
			cairo_dock_update_dock_size (pDock);  // la largeur max peut avoir ete influencee par le changement d'ordre.
		}
		// on redessine l'icone pointant sur le sous-dock, pour le cas ou l'ordre et/ou l'image du lanceur aurait change.
		if (pNewDock->iRefCount != 0)
		{
			cairo_dock_redraw_subdock_content (pNewDock);
		}
		///cairo_dock_trigger_refresh_launcher_gui ();  // a faire par l'app
	}
	
	//\_____________ On gere l'inhibition de sa classe.
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

	//\_____________ On redessine les docks impactes.
	cairo_dock_calculate_dock_icons (pNewDock);
	cairo_dock_redraw_container (CAIRO_CONTAINER (pNewDock));

	g_free (cPrevDockName);
	g_free (cClass);
	g_free (cDesktopFileName);
	g_free (cName);
	g_free (cRendererName);
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
