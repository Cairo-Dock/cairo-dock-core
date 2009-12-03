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
#include "cairo-dock-launcher-factory.h"

extern CairoDock *g_pMainDock;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentLaunchersPath;


gchar *cairo_dock_search_icon_s_path (const gchar *cFileName)
{
	g_return_val_if_fail (cFileName != NULL, NULL);
	GString *sIconPath = g_string_new ("");
	const gchar *cSuffixTab[4] = {".svg", ".png", ".xpm", NULL};
	gboolean bAddSuffix, bFileFound, bHasVersion;
	GtkIconInfo* pIconInfo = NULL;
	int i, j;

	//\_______________________ On construit le chemin de l'icone a afficher.
	if (*cFileName == '~')
	{
		g_string_printf (sIconPath, "%s%s", g_getenv ("HOME"), cFileName+1);
	}
	else if (*cFileName == '/')
	{
		g_string_assign (sIconPath, cFileName);
	}
	else
	{
		//\_______________________ On determine si le suffixe est present ou non.
		bAddSuffix = FALSE;
		/*j = 0;
		while (cSuffixTab[j] != NULL && ! g_str_has_suffix (cFileName, cSuffixTab[j]))
			j ++;

		if (cSuffixTab[j] == NULL)
			bAddSuffix = TRUE;*/
		gchar *str = strrchr (cFileName, '.');
		bAddSuffix = (str == NULL || ! g_ascii_isalpha (*(str+1)));
		bHasVersion = (str != NULL && g_ascii_isdigit (*(str+1)) && g_ascii_isdigit (*(str-1)));
		
		//\_______________________ On parcourt les themes disponibles, en testant tous les suffixes connus.
		bFileFound = FALSE;
		if (myIcons.pDefaultIconDirectory != NULL)
		{
			for (i = 0; i < myIcons.iNbIconPlaces && ! bFileFound; i ++)
			{
				if (myIcons.pDefaultIconDirectory[2*i] != NULL)
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
				else
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
						64,
						GTK_ICON_LOOKUP_FORCE_SVG);
					if (pIconInfo != NULL)
					{
						g_string_assign (sIconPath, gtk_icon_info_get_filename (pIconInfo));
						bFileFound = TRUE;
						gtk_icon_info_free (pIconInfo);
					}
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
		
		//\_______________________ si rien trouve, on cherche dans le theme par defaut.
		/**if (! bFileFound)
		{
			g_string_assign (sIconPath, cFileName);
			if (! bAddSuffix)
			{
				gchar *str = strrchr (sIconPath->str, '.');
				if (str != NULL)
					*str = '\0';
			}
			//g_print ("on recherche %s dans le theme par defaut.\n", sIconPath->str);
			GtkIconTheme *pDefaultIconTheme = gtk_icon_theme_get_default ();
			pIconInfo = gtk_icon_theme_lookup_icon  (pDefaultIconTheme,
				sIconPath->str,
				64,
				GTK_ICON_LOOKUP_FORCE_SVG);

			if (pIconInfo != NULL)
			{
				g_string_assign (sIconPath, gtk_icon_info_get_filename (pIconInfo));
				gtk_icon_info_free (pIconInfo);
			}
			else
				g_string_printf (sIconPath, "");
		}*/
	}

	gchar *cIconPath = sIconPath->str;
	g_string_free (sIconPath, FALSE);
	if (cIconPath == NULL || *cIconPath == '\0')
	{
		g_free (cIconPath);
		cIconPath = NULL;
	}
	return cIconPath;
}


void cairo_dock_load_icon_info_from_desktop_file (const gchar *cDesktopFileName, Icon *icon)
{
	GError *erreur = NULL;
	gchar *cDesktopFilePath = (*cDesktopFileName == '/' ? g_strdup (cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cDesktopFileName));
	//g_print ("%s (%s)\n", __func__, cDesktopFilePath);
	GKeyFile* keyfile = cairo_dock_open_key_file (cDesktopFilePath);
	g_return_if_fail (keyfile != NULL);
	
	icon->iType = CAIRO_DOCK_LAUNCHER;
	g_free (icon->cDesktopFileName);
	icon->cDesktopFileName = g_strdup (cDesktopFileName);

	g_free (icon->cFileName);
	icon->cFileName = g_key_file_get_string (keyfile, "Desktop Entry", "Icon", &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cDesktopFileName, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	if (icon->cFileName != NULL && *icon->cFileName == '\0')
	{
		g_free (icon->cFileName);
		icon->cFileName = NULL;
	}


	g_free (icon->cName);
	icon->cName = g_key_file_get_locale_string (keyfile, "Desktop Entry", "Name", NULL, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cDesktopFileName, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	if (icon->cName != NULL && *icon->cName == '\0')
	{
		g_free (icon->cName);
		icon->cName = NULL;
	}

	g_free (icon->cCommand);
	icon->cCommand = g_key_file_get_string (keyfile, "Desktop Entry", "Exec", &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cDesktopFileName, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	if (icon->cCommand != NULL && *icon->cCommand == '\0')
	{
		g_free (icon->cCommand);
		icon->cCommand = NULL;
	}
	
	if (icon->cCommand != NULL)
	{
		g_free (icon->cWorkingDirectory);
		icon->cWorkingDirectory = g_key_file_get_string (keyfile, "Desktop Entry", "Path", NULL);
		if (icon->cWorkingDirectory != NULL && *icon->cWorkingDirectory == '\0')
		{
			g_free (icon->cWorkingDirectory);
			icon->cWorkingDirectory = NULL;
		}
	}
	
	icon->fOrder = g_key_file_get_double (keyfile, "Desktop Entry", "Order", &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cDesktopFileName, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}

	icon->cBaseURI = g_key_file_get_string (keyfile, "Desktop Entry", "Base URI", &erreur);
	if (erreur != NULL)
	{
		icon->cBaseURI = NULL;
		g_error_free (erreur);
		erreur = NULL;
	}
	if (icon->cBaseURI != NULL && *icon->cBaseURI == '\0')
	{
		g_free (icon->cBaseURI);
		icon->cBaseURI = NULL;
	}

	icon->iVolumeID = g_key_file_get_boolean (keyfile, "Desktop Entry", "Is mounting point", &erreur);
	if (erreur != NULL)
	{
		icon->iVolumeID = FALSE;
		g_error_free (erreur);
		erreur = NULL;
	}
	if (icon->iVolumeID)  // les infos dans le .desktop ne sont pas a jour.
	{
		g_free (icon->cName);
		icon->cName = NULL;
		g_free (icon->cCommand);
		icon->cCommand = NULL;
		g_free (icon->cFileName);
		icon->cFileName = NULL;

		gboolean bIsDirectory;  // on n'ecrase pas le fait que ce soit un container ou pas, car c'est l'utilisateur qui l'a decide.
		cairo_dock_fm_get_file_info (icon->cBaseURI, &icon->cName, &icon->cCommand, &icon->cFileName, &bIsDirectory, &icon->iVolumeID, &icon->fOrder, mySystem.iFileSortType);
	}
	
	
	g_free (icon->cParentDockName);
	icon->cParentDockName = g_key_file_get_string (keyfile, "Desktop Entry", "Container", &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cDesktopFileName, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
		icon->cParentDockName = NULL;
	}
	if (icon->cParentDockName == NULL || *icon->cParentDockName == '\0')
	{
		g_free (icon->cParentDockName);
		icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);
	}
	
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (pParentDock == NULL)
	{
		cd_message ("le dock parent (%s) n'existe pas, on le cree", icon->cParentDockName);
		pParentDock = cairo_dock_create_new_dock (icon->cParentDockName, NULL);
	}
	
	gboolean bIsContainer = g_key_file_get_boolean (keyfile, "Desktop Entry", "Is container", &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cDesktopFileName, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
		bIsContainer = FALSE;
	}
	if (bIsContainer && icon->cName != NULL)
	{
		gchar *cRendererName = g_key_file_get_string (keyfile, "Desktop Entry", "Renderer", NULL);
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

		g_free (cRendererName);
	}

	
	gboolean bPreventFromInhibating = g_key_file_get_boolean (keyfile, "Desktop Entry", "prevent inhibate", NULL);  // FALSE si la cle n'existe pas.
	
	g_free (icon->cClass);
	if (icon->cCommand != NULL && icon->cBaseURI == NULL)  /// ! bPreventFromInhibating && 
	{
		gchar *cStartupWMClass = g_key_file_get_string (keyfile, "Desktop Entry", "StartupWMClass", NULL);
		if (cStartupWMClass == NULL || *cStartupWMClass == '\0' || strcmp (cStartupWMClass, "Wine") == 0)  // on force pour wine, car meme si la classe est explicitement definie en tant que "Wine", cette information est inexploitable.
		{
			// plusieurs cas sont possibles : 
			// Exec=toto
			// Exec=toto -x -y
			// Exec=/path/to/toto -x -y
			// Exec=gksu toto
			// Exec=gksu --description /usr/share/applications/synaptic.desktop /usr/sbin/synaptic
			// Exec=wine "C:\Program Files\Starcraft\Starcraft.exe"
			// Exec=wine "/path/to/prog.exe"
			// Exec=env WINEPREFIX="/home/fab/.wine" wine "C:\Program Files\Starcraft\Starcraft.exe"
			g_free (cStartupWMClass);
			cStartupWMClass = g_ascii_strdown (icon->cCommand, -1);
			gchar *str, *cClass = cStartupWMClass;
			
			if (strncmp (cClass, "gksu", 4) == 0 || strncmp (cClass, "kdesu", 4) == 0)  // on prend la fin .
			{
				while (cClass[strlen(cClass)-1] == ' ')  // par securite on enleve les espaces en fin de ligne.
					cClass[strlen(cClass)-1] = '\0';
				str = strrchr (cClass, ' ');  // on cherche le dernier espace.
				if (str != NULL)  // on prend apres.
					cClass = str + 1;
				str = strrchr (cClass, '/');  // on cherche le dernier '/'.
				if (str != NULL)  // on prend apres.
					cClass = str + 1;
			}
			else if ((str = g_strstr_len (cClass, -1, "wine ")) != NULL)
			{
				cClass = str;  // on met deja la classe a "wine", c'est mieux que rien.
				*(str+4) = '\0';
				str += 5;
				gchar *exe = g_strstr_len (str, -1, ".exe");  // on cherche a isoler le nom de l'executable, puisque wine l'utilise dans le res_name.
				if (exe)
				{
					*(exe+4) = '\0';
					gchar *slash = strrchr (str, '\\');
					if (slash)
						cClass = slash+1;
					else
					{
						slash = strrchr (str, '/');
						if (slash)
							cClass = slash+1;
					}
				}
				g_print ("  special case : wine application => class = '%s'\n", cClass);
			}
			else
			{
				while (*cClass == ' ')  // par securite on enleve les espaces en debut de ligne.
					cClass ++;
				str = strchr (cClass, ' ');  // on cherche le premier espace.
				if (str != NULL)  // on vire apres.
					*str = '\0';
				str = strrchr (cClass, '/');  // on cherche le dernier '/'.
				if (str != NULL)  // on prend apres.
					cClass = str + 1;
			}
			
			if (*cClass != '\0')
				icon->cClass = g_strdup (cClass);
			else
				icon->cClass = NULL;
			cd_message ("no class defined for the launcher %s\n we will assume that its class is '%s'", cDesktopFileName, icon->cClass);
		}
		else
		{
			icon->cClass = g_ascii_strdown (cStartupWMClass, -1);
		}
		g_free (cStartupWMClass);
	}
	else
		icon->cClass = NULL;
	
	if (bPreventFromInhibating && icon->cClass != NULL)
	{
		///cairo_dock_deinhibate_class (icon->cClass, icon);  /// mis en commentaire le 21/09/2009
		g_free (icon->cClass);
		icon->cClass = NULL;
	}
	
	gboolean bExecInTerminal = g_key_file_get_boolean (keyfile, "Desktop Entry", "Terminal", NULL);
	if (bExecInTerminal)  // on le fait apres la classe puisqu'on change la commande.
	{
		gchar *cOldCommand = icon->cCommand;
		icon->cCommand = g_strdup_printf ("xterm -e \"%s\"", cOldCommand);
		g_free (cOldCommand);
	}

	icon->iSpecificDesktop = -1;
	gint iSpecificDesktop = g_key_file_get_integer (keyfile, "Desktop Entry", "ShowOnViewport", &erreur);
	if (erreur != NULL)
	{
		cd_warning ("while trying to load %s : %s", cDesktopFileName, erreur->message);
		g_error_free (erreur);
		erreur = NULL;
		iSpecificDesktop = -1;
	}
	if( iSpecificDesktop < -1 )
	{
		iSpecificDesktop = -1; // prevent wrong values
	}
	else if( iSpecificDesktop >= 0 )
	{
		// to define as a global variable 
		gint g_FoundNonStickyLaunchers = TRUE;
	}
		
	icon->iSpecificDesktop = iSpecificDesktop;
	
	g_key_file_free (keyfile);
}



Icon * cairo_dock_create_icon_from_desktop_file (const gchar *cDesktopFileName, cairo_t *pSourceContext)
{
	//g_print ("%s (%s)\n", __func__, cDesktopFileName);
	
	Icon *icon = g_new0 (Icon, 1);
	cairo_dock_load_icon_info_from_desktop_file (cDesktopFileName, icon);
	g_return_val_if_fail (icon->cDesktopFileName != NULL, NULL);
	
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
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
	CairoDock *pSubDock = icon->pSubDock;
	icon->pSubDock = NULL;
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
	cairo_dock_load_icon_info_from_desktop_file (cDesktopFileName, icon);
	g_return_if_fail (icon->cDesktopFileName != NULL);
	
	CairoDock *pNewDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	g_return_if_fail (pNewDock != NULL);
	
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pNewDock));
	icon->fWidth /= pDock->container.fRatio;
	icon->fHeight /= pDock->container.fRatio;
	cairo_dock_fill_icon_buffers_for_dock (icon, pCairoContext, pNewDock);
	icon->fWidth *= pDock->container.fRatio;
	icon->fHeight *= pDock->container.fRatio;
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
		if (pSubDock != icon->pSubDock)  // ca n'est plus le meme container, on transvase ou on detruit.
		{
			g_print ("on transvase dans le nouveau sous-dock\n");
			cairo_dock_destroy_dock (pSubDock, cName, icon->pSubDock, icon->cName);
		}
	}
	
	if (icon->pSubDock != NULL && pSubDock == icon->pSubDock)  // c'est le meme sous-dock, son rendu a pu changer.
	{
		if (cairo_dock_strings_differ (cRendererName, icon->pSubDock->cRendererName))
			cairo_dock_update_dock_size (icon->pSubDock);
	}

	//\_____________ On gere le changement de container ou d'ordre.
	if (pDock != pNewDock)  // on la detache de son container actuel et on l'insere dans le nouveau.
	{
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
		cairo_dock_insert_icon_in_dock (icon, pNewDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON);
		icon->cParentDockName = tmp;
	}
	else
	{
		icon->fWidth *= pNewDock->container.fRatio / pDock->container.fRatio;
		icon->fHeight *= pNewDock->container.fRatio / pDock->container.fRatio;
		cairo_dock_refresh_launcher_gui ();
		if (icon->fOrder != fOrder)  // On gere le changement d'ordre.
		{
			pNewDock->pFirstDrawnElement = NULL;
			pNewDock->icons = g_list_remove (pNewDock->icons, icon);
			pNewDock->icons = g_list_insert_sorted (pNewDock->icons,
				icon,
				(GCompareFunc) cairo_dock_compare_icons_order);
			cairo_dock_update_dock_size (pDock);  // la largeur max peut avoir ete influencee par le changement d'ordre.
		}
	}
	
	//\_____________ On gere l'inhibition de sa classe.
	gchar *cNowClass = icon->cClass;
	if (cClass != NULL && (cNowClass == NULL || strcmp (cNowClass, cClass) != 0))
	{
		icon->cClass = cClass;
		cairo_dock_deinhibate_class (cClass, icon);
		cClass = NULL;  // libere par la fonction precedente.
		icon->cClass = cNowClass;
	}
	if (myTaskBar.bMixLauncherAppli && cNowClass != NULL && (cClass == NULL || strcmp (cNowClass, cClass) != 0))
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
