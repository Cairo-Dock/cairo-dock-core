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

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "cairo-dock-load.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-themes-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-emblem.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-launcher-manager.h"

extern CairoDock *g_pMainDock;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentLaunchersPath;
extern int g_iNbNonStickyLaunchers;

gchar *cairo_dock_search_icon_s_path (const gchar *cFileName)
{
	g_return_val_if_fail (cFileName != NULL, NULL);
	
	//\_______________________ cas faciles : l'entree est deja un chemin.
	if (*cFileName == '~')
	{
		return g_strdup_printf ("%s%s", g_getenv ("HOME"), cFileName+1);
	}
	
	if (*cFileName == '/')
	{
		return g_strdup (cFileName);
	}
	
	//\_______________________ On determine si le suffixe et une version sont presents ou non.
	g_return_val_if_fail (myIcons.pDefaultIconDirectory != NULL, NULL);
	
	GString *sIconPath = g_string_new ("");
	const gchar *cSuffixTab[4] = {".svg", ".png", ".xpm", NULL};
	gboolean bAddSuffix=FALSE, bFileFound=FALSE, bHasVersion=FALSE;
	GtkIconInfo* pIconInfo = NULL;
	int i, j;
	gchar *str = strrchr (cFileName, '.');
	bAddSuffix = (str == NULL || ! g_ascii_isalpha (*(str+1)));  // exemple : firefox-3.0
	bHasVersion = (str != NULL && g_ascii_isdigit (*(str+1)) && g_ascii_isdigit (*(str-1)) && str-1 != cFileName);  // doit finir par x.y, x et y ayant autant de chiffres que l'on veut.
	
	//\_______________________ On parcourt les themes disponibles, en testant tous les suffixes connus.
	for (i = 0; i < myIcons.iNbIconPlaces && ! bFileFound; i ++)
	{
		if (myIcons.pDefaultIconDirectory[2*i] != NULL)  // repertoire.
		{
			//g_print ("on recherche %s dans le repertoire %s\n", sIconPath->str, myIcons.pDefaultIconDirectory[2*i]);
			j = 0;
			while (! bFileFound && (cSuffixTab[j] != NULL || ! bAddSuffix))
			{
				g_string_printf (sIconPath, "%s/%s", (gchar *)myIcons.pDefaultIconDirectory[2*i], cFileName);
				if (bAddSuffix)
					g_string_append_printf (sIconPath, "%s", cSuffixTab[j]);
				//g_print ("  -> %s\n", sIconPath->str);
				if ( g_file_test (sIconPath->str, G_FILE_TEST_EXISTS) )
					bFileFound = TRUE;
				j ++;
				if (! bAddSuffix)
					break;
			}
		}
		else  // theme d'icones.
		{
			g_string_assign (sIconPath, cFileName);
			if (! bAddSuffix)  // on vire le suffixe pour chercher tous les formats dans le theme d'icones.
			{
				gchar *str = strrchr (sIconPath->str, '.');
				if (str != NULL)
					*str = '\0';
			}
			//g_print ("on recherche %s dans le theme d'icones\n", sIconPath->str);
			GtkIconTheme *pIconTheme;
			if (myIcons.pDefaultIconDirectory[2*i+1] != NULL)
				pIconTheme = myIcons.pDefaultIconDirectory[2*i+1];
			else
				pIconTheme = gtk_icon_theme_get_default ();
			pIconInfo = gtk_icon_theme_lookup_icon  (GTK_ICON_THEME (pIconTheme),
				sIconPath->str,
				128,
				GTK_ICON_LOOKUP_FORCE_SVG);
			if (pIconInfo != NULL)
			{
				g_string_assign (sIconPath, gtk_icon_info_get_filename (pIconInfo));
				bFileFound = TRUE;
				gtk_icon_info_free (pIconInfo);
			}
		}
	}
	
	//\_______________________ si rien trouve, on cherche sans le numero de version.
	if (! bFileFound && bHasVersion)
	{
		cd_debug ("on cherche sans le numero de version...");
		g_string_assign (sIconPath, cFileName);
		gchar *str = strrchr (sIconPath->str, '.');
		str --;  // on sait que c'est un digit.
		str --;
		while ((g_ascii_isdigit (*str) || *str == '.' || *str == '-') && (str != sIconPath->str))
			str --;
		if (str != sIconPath->str)
		{
			*(str+1) = '\0';
			cd_debug (" on cherche '%s'...\n", sIconPath->str);
			gchar *cPath = cairo_dock_search_icon_s_path (sIconPath->str);
			if (cPath != NULL)
			{
				bFileFound = TRUE;
				g_string_assign (sIconPath, cPath);
				g_free (cPath);
			}
		}
	}
	
	if (! bFileFound)
	{
		g_string_free (sIconPath, TRUE);
		return NULL;
	}
	
	gchar *cIconPath = sIconPath->str;
	g_string_free (sIconPath, FALSE);
	return cIconPath;
}


static CairoDock *_cairo_dock_handle_container (Icon *icon, const gchar *cRendererName)
{
	//\____________ On cree son container si necessaire.
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (pParentDock == NULL)
	{
		cd_message ("le dock parent (%s) n'existe pas, on le cree", icon->cParentDockName);
		pParentDock = cairo_dock_create_new_dock (icon->cParentDockName, NULL);
	}
	
	//\____________ On cree son sous-dock si necessaire.
	if (icon->iNbSubIcons != 0 && icon->cName != NULL)
	{
		CairoDock *pChildDock = cairo_dock_search_dock_from_name (icon->cName);
		if (pChildDock == NULL)
		{
			cd_message ("le dock fils (%s) n'existe pas, on le cree avec la vue %s", icon->cName, cRendererName);
			if (icon->cBaseURI == NULL)
				icon->pSubDock = cairo_dock_create_subdock_from_scratch (NULL, icon->cName, pParentDock);
			else
				cairo_dock_fm_create_dock_from_directory (icon, pParentDock);
		}
		else
		{
			cairo_dock_reference_dock (pChildDock, pParentDock);
			icon->pSubDock = pChildDock;
			cd_message ("le dock devient un dock fils (%d, %d)", pChildDock->container.bIsHorizontal, pChildDock->container.bDirectionUp);
		}
		if (cRendererName != NULL && icon->pSubDock != NULL)
			cairo_dock_set_renderer (icon->pSubDock, cRendererName);
	}
	
	//\____________ On met a jour les infos dans le cas d'un point de montage.
	if (icon->iVolumeID)  // les infos dans le .desktop ne sont pas a jour.
	{
		g_free (icon->cName);
		icon->cName = NULL;
		g_free (icon->cCommand);
		icon->cCommand = NULL;
		g_free (icon->cFileName);
		icon->cFileName = NULL;

		gboolean bIsDirectory;  // on n'ecrase pas le fait que ce soit un container ou pas, car c'est l'utilisateur qui l'a decide.
		cairo_dock_fm_get_file_info (icon->cBaseURI, &icon->cName, &icon->cCommand, &icon->cFileName, &bIsDirectory, &icon->iVolumeID, &icon->fOrder, CAIRO_DOCK_FM_SORT_BY_NAME);  // son ordre nous importe peu ici, puisqu'il est definie par le champ 'Order'.
	}
	return pParentDock;
}

Icon * cairo_dock_create_icon_from_desktop_file (const gchar *cDesktopFileName, cairo_t *pSourceContext)
{
	//g_print ("%s (%s)\n", __func__, cDesktopFileName);
	
	//\____________ On cree l'icone.
	gchar *cRendererName = NULL;
	Icon *icon = cairo_dock_new_launcher_icon (cDesktopFileName, &cRendererName);
	g_return_val_if_fail (icon != NULL, NULL);
	
	//\____________ On gere son dock et sous-dock.
	CairoDock *pParentDock = _cairo_dock_handle_container (icon, cRendererName);
	g_free (cRendererName);
	
	//\____________ On remplit ses buffers.
	cairo_dock_fill_icon_buffers_for_dock (icon, pSourceContext, pParentDock);
	
	cd_message ("+ %s/%s", icon->cName, icon->cClass);
	if (CAIRO_DOCK_IS_NORMAL_LAUNCHER (icon) && icon->cClass != NULL)
	{
		if (myTaskBar.bMixLauncherAppli)
			cairo_dock_inhibate_class (icon->cClass, icon);
		else  // on l'insere quand meme dans la classe pour pouvoir ecraser l'icone X avec la sienne.
			cairo_dock_add_inhibator_to_class (icon->cClass, icon);
	}
	
	return icon;
}



void cairo_dock_reload_launcher (Icon *icon)
{
	if (icon->cDesktopFileName == NULL || strcmp (icon->cDesktopFileName, "none") == 0)
	{
		cd_warning ("tried to reload a launcher whereas this icon (%s) is obviously not a launcher", icon->cName);
		return ;
	}
	GError *erreur = NULL;
	
	//\_____________ On assure la coherence du nouveau fichier de conf.
	if (CAIRO_DOCK_IS_LAUNCHER (icon))
	{
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
		GKeyFile* pKeyFile = cairo_dock_open_key_file (cDesktopFilePath);
		g_return_if_fail (pKeyFile != NULL);
		
		if (icon->pSubDock != NULL)  // on assure l'unicite du nom du sous-dock ici, car cela n'est volontairement pas fait dans la fonction de creation de l'icone.
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
					g_print ("on renomme a l'avance le sous-dock en %s\n", cUniqueName);
					cairo_dock_rename_dock (icon->cName, icon->pSubDock, cUniqueName);  // on le renomme ici pour eviter de transvaser dans un nouveau dock (ca marche aussi ceci dit).
				}
				g_free (cUniqueName);
			}
			g_free (cName);
		}
		if (icon->cCommand != NULL)  // on assure que ca reste un lanceur.
		{
			gchar *cCommand = g_key_file_get_string (pKeyFile, "Desktop Entry", "Exec", NULL);
			if (cCommand == NULL || *cCommand == '\0')
			{
				cCommand = g_strdup ("no command");
				g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", cCommand);
				cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
			}
			g_free (cCommand);
		}
		if (icon->cBaseURI != NULL)  // on assure que ca reste un fichier.
		{
			gchar *cBaseURI = g_key_file_get_string (pKeyFile, "Desktop Entry", "Base URI", NULL);
			if (cBaseURI == NULL || *cBaseURI == '\0')
			{
				cBaseURI = g_strdup (icon->cBaseURI);
				g_key_file_set_string (pKeyFile, "Desktop Entry", "Base URI", cBaseURI);
				cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
			}
			g_free (cBaseURI);
		}
		
		g_key_file_free (pKeyFile);
		g_free (cDesktopFilePath);
	}
	
	//\_____________ On memorise son etat.
	gchar *cPrevDockName = icon->cParentDockName;
	icon->cParentDockName = NULL;
	CairoDock *pDock = cairo_dock_search_dock_from_name (cPrevDockName);  // changement de l'ordre ou du container.
	double fOrder = icon->fOrder;
	
	if (CAIRO_DOCK_IS_URI_LAUNCHER (icon) && icon->pSubDock != NULL)  // dans le cas d'un repertoire, beaucoup de parametres peuvent affecter le contenu du sous-dock : nombre de fichiers max, ordre, et meme l'URI. Donc on s'embete pas trop, on le recree.
	{
		cairo_dock_destroy_dock (icon->pSubDock, icon->cName, NULL, NULL);
		icon->pSubDock = NULL;
	}
	
	CairoDock *pSubDock = icon->pSubDock;
	icon->pSubDock = NULL;
	gchar *cClass = icon->cClass;
	icon->cClass = NULL;
	gchar *cDesktopFileName = icon->cDesktopFileName;
	icon->cDesktopFileName = NULL;
	gchar *cName = icon->cName;
	icon->cName = NULL;
	gchar *cRendererName = NULL;
	int iNbSubIcons = icon->iNbSubIcons;
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
	
	cairo_t *pCairoContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pNewDock));
	
	if (icon->pSubDock != NULL && icon->iSubdockViewType != 0)
	{
		cairo_dock_draw_subdock_content_on_icon (icon, pNewDock);
	}
	else
	{
		icon->fWidth /= pDock->container.fRatio;
		icon->fHeight /= pDock->container.fRatio;
		cairo_dock_fill_icon_buffers_for_dock (icon, pCairoContext, pNewDock);
		icon->fWidth *= pDock->container.fRatio;
		icon->fHeight *= pDock->container.fRatio;
	}
	
	//g_print ("icon : %.1fx%.1f", icon->fWidth, icon->fHeight);
	
	if (cName && ! icon->cName)
		icon->cName = g_strdup (" ");
	
	//\_____________ On gere son sous-dock.
	if (icon->Xid != 0)
	{
		if (icon->pSubDock == NULL)
			icon->pSubDock = pSubDock;
		else  // ne devrait pas arriver (une icone de container n'est pas un lanceur pouvant prendre un Xid).
			cairo_dock_destroy_dock (pSubDock, cName, g_pMainDock, CAIRO_DOCK_MAIN_DOCK_NAME);
	}
	else
	{
		if (pSubDock != NULL && pSubDock != icon->pSubDock)  // ca n'est plus le meme container, on transvase ou on detruit.
		{
			g_print ("on transvase dans le nouveau sous-dock\n");
			if (CAIRO_DOCK_IS_URI_LAUNCHER (icon))  // dans ce cas on ne transvase pas puisque le sous-dock est cree a partir du contenu du repertoire.
				cairo_dock_destroy_dock (pSubDock, cName, NULL, NULL);
			else
				cairo_dock_destroy_dock (pSubDock, cName, icon->pSubDock, icon->cName);
			pSubDock = NULL;
		}
	}
	
	if (icon->pSubDock != NULL && pSubDock == icon->pSubDock)  // c'est le meme sous-dock, son rendu a pu changer.
	{
		if (cairo_dock_strings_differ (cRendererName, icon->pSubDock->cRendererName))
			cairo_dock_update_dock_size (icon->pSubDock);
	}

	//\_____________ On gere le changement de container ou d'ordre.
	g_print ("%x -> %x\n", pDock, pNewDock);
	if (pDock != pNewDock)  // changement de container.
	{
		// on la detache de son container actuel et on l'insere dans le nouveau.
		gchar *tmp = icon->cParentDockName;  // le detach_icon remet a 0 ce champ, il faut le donc conserver avant.
		icon->cParentDockName = NULL;
		cairo_dock_detach_icon_from_dock (icon, pDock, TRUE);
		if (pDock->icons == NULL && pDock->iRefCount == 0 && ! pDock->bIsMainDock)  // on supprime les docks principaux vides.
		{
			cd_message ("dock %s vide => a la poubelle", cPrevDockName);
			cairo_dock_destroy_dock (pDock, cPrevDockName, NULL, NULL);
			pDock = NULL;
		}
		else
		{
			cairo_dock_update_dock_size (pDock);
			cairo_dock_calculate_dock_icons (pDock);
			gtk_widget_queue_draw (pDock->container.pWidget);
		}
		cairo_dock_insert_icon_in_dock (icon, pNewDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);  // le remove et le insert vont declencher le redessin de l'icone pointant sur l'ancien et le nouveau sous-dock le cas echeant.
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
		cairo_dock_refresh_launcher_gui ();
	}
	
	//\_____________ On gere l'inhibition de sa classe.
	gchar *cNowClass = icon->cClass;
	if (cClass != NULL && (cNowClass == NULL || strcmp (cNowClass, cClass) != 0))  // la classe a change, on desinhibe l'ancienne.
	{
		icon->cClass = cClass;
		cairo_dock_deinhibate_class (cClass, icon);
		cClass = NULL;  // libere par la fonction precedente.
		icon->cClass = cNowClass;
	}
	if (myTaskBar.bMixLauncherAppli && cNowClass != NULL && (cClass == NULL || strcmp (cNowClass, cClass) != 0))  // la classe a change, on inhibe la nouvelle.
		cairo_dock_inhibate_class (cNowClass, icon);

	//\_____________ On redessine les docks impactes.
	cairo_dock_calculate_dock_icons (pNewDock);
	cairo_dock_redraw_container (CAIRO_CONTAINER (pNewDock));

	g_free (cPrevDockName);
	g_free (cClass);
	g_free (cDesktopFileName);
	g_free (cName);
	g_free (cRendererName);
	cairo_destroy (pCairoContext);
	cairo_dock_mark_theme_as_modified (TRUE);
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
