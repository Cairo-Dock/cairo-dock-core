/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <gtk/gtk.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "cairo-dock-icons.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-config.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-desktop-file-factory.h"

extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentLaunchersPath;

void cairo_dock_remove_html_spaces (gchar *cString)
{
	gchar *str = cString;
	do
	{
		str = g_strstr_len (str, -1, "%20");
		if (str == NULL)
			break ;
		*str = ' ';
		str ++;
		strcpy (str, str+2);
	}
	while (TRUE);
}

gchar *cairo_dock_generate_desktop_file_for_launcher (const gchar *cDesktopURI, const gchar *cDockName, double fOrder, CairoDock *pDock, GError **erreur)
{
	g_return_val_if_fail (cDesktopURI != NULL, NULL);
	GError *tmp_erreur = NULL;
	gchar *cFilePath = (*cDesktopURI == '/' ? g_strdup (cDesktopURI) : g_filename_from_uri (cDesktopURI, NULL, &tmp_erreur));
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return NULL;
	}

	//\___________________ On supprime a la main les '%20' qui foutent le boxon (rare).
	cairo_dock_remove_html_spaces (cFilePath);

	//\___________________ On ouvre le patron.
	GKeyFile *pKeyFile = g_key_file_new ();
	g_key_file_load_from_file (pKeyFile, cFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return NULL;
	}

	//\___________________ On renseigne nos champs.
	///g_key_file_set_string (pKeyFile, "Desktop Entry", "frame_extra", "");
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);
	///g_key_file_set_boolean (pKeyFile, "Desktop Entry", "Is container", FALSE);
	///g_key_file_set_string (pKeyFile, "Desktop Entry", "Base URI", "");
	
	//\___________________ On elimine les indesirables.
	g_key_file_remove_key (pKeyFile, "Desktop Entry", "X-Ubuntu-Gettext-Domain", NULL);
	gchar *cCommand = g_key_file_get_string (pKeyFile, "Desktop Entry", "Exec", &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		g_key_file_free (pKeyFile);
		return NULL;
	}
	gchar *str = strchr (cCommand, '%');
	if (str != NULL)
	{
		*str = '\0';
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", cCommand);
	}
	g_free (cCommand);

	gchar *cIconName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Icon", &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		g_key_file_free (pKeyFile);
		return NULL;
	}
	if (g_str_has_suffix (cIconName, ".svg") || g_str_has_suffix (cIconName, ".png") || g_str_has_suffix (cIconName, ".xpm"))
	{
		cIconName[strlen(cIconName) - 4] = '\0';
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Icon", cIconName);
	}
	g_free (cIconName);

	//\___________________ On lui choisit un nom de fichier tel qu'il n'y ait pas de collision.
	gchar *cBaseName = g_path_get_basename (cFilePath);
	gchar *cNewDesktopFileName = cairo_dock_generate_desktop_filename (cBaseName, g_cCurrentLaunchersPath);
	g_free (cBaseName);

	//\___________________ On ecrit tout ca dans un fichier base sur le template.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_flush_conf_file_full (pKeyFile, cNewDesktopFilePath, CAIRO_DOCK_SHARE_DATA_DIR, FALSE, CAIRO_DOCK_LAUNCHER_CONF_FILE);
	
	g_free (cNewDesktopFilePath);
	g_key_file_free (pKeyFile);
	g_free (cFilePath);

	return cNewDesktopFileName;
}

gchar *cairo_dock_generate_desktop_file_for_edition (CairoDockNewLauncherType iNewLauncherType, const gchar *cDockName, double fOrder, CairoDock *pDock, GError **erreur)
{
	//\___________________ On ouvre le patron.
	gchar *cDesktopFileTemplate = cairo_dock_get_launcher_template_conf_file (iNewLauncherType);

	GKeyFile *pKeyFile = g_key_file_new ();
	GError *tmp_erreur = NULL;
	g_key_file_load_from_file (pKeyFile, cDesktopFileTemplate, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &tmp_erreur);
	g_free (cDesktopFileTemplate);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return NULL;
	}

	//\___________________ On renseigne ce qu'on peut.
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);

	//\___________________ On lui choisit un nom de fichier tel qu'il n'y ait pas de collision, et qu'il soit comprehensible par l'utilisateur, au cas ou il voudrait les modifier a la main.
	gchar *cNewDesktopFileName = cairo_dock_generate_desktop_filename (iNewLauncherType == CAIRO_DOCK_LAUNCHER_FOR_CONTAINER ? "container.desktop" : iNewLauncherType == CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR ? "separator.desktop" : "launcher.desktop", g_cCurrentLaunchersPath);

	//\___________________ On ecrit tout.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_write_keys_to_file (pKeyFile, cNewDesktopFilePath);
	g_free (cNewDesktopFilePath);
	g_key_file_free (pKeyFile);
	return cNewDesktopFileName;
}

gchar *cairo_dock_generate_desktop_file_for_file (const gchar *cURI, const gchar *cDockName, double fOrder, CairoDock *pDock, GError **erreur)
{
	//\___________________ On recupere le type mime du fichier.
	gchar *cIconName = NULL, *cName = NULL, *cRealURI = NULL;
	gboolean bIsDirectory;
	int iVolumeID;
	double fUnusedOrder;
	if (! cairo_dock_fm_get_file_info (cURI, &cName, &cRealURI, &cIconName, &bIsDirectory, &iVolumeID, &fUnusedOrder, mySystem.iFileSortType) || cIconName == NULL)
		return NULL;
	cd_message (" -> cIconName : %s; bIsDirectory : %d; iVolumeID : %d\n", cIconName, bIsDirectory, iVolumeID);

	if (bIsDirectory)
	{
		int answer = cairo_dock_ask_general_question_and_wait (_("Do you want to monitor the content of the directory ?"));
		if (answer != GTK_RESPONSE_YES)
			bIsDirectory = FALSE;
	}

	//\___________________ On ouvre le patron.
	gchar *cDesktopFileTemplate = cairo_dock_get_launcher_template_conf_file (bIsDirectory ? CAIRO_DOCK_LAUNCHER_FOR_CONTAINER : CAIRO_DOCK_LAUNCHER_FROM_DESKTOP_FILE);

	GKeyFile *pKeyFile = g_key_file_new ();
	GError *tmp_erreur = NULL;
	g_key_file_load_from_file (pKeyFile, cDesktopFileTemplate, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &tmp_erreur);
	g_free (cDesktopFileTemplate);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return NULL;
	}

	//\___________________ On renseigne ce qu'on peut.
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Base URI", cURI);

	g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cName);
	g_free (cName);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", cRealURI);
	g_free (cRealURI);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Icon", (cIconName != NULL ? cIconName : ""));
	g_free (cIconName);

	g_key_file_set_boolean (pKeyFile, "Desktop Entry", "Is mounting point", (iVolumeID > 0));

	//\___________________ On lui choisit un nom de fichier tel qu'il n'y ait pas de collision.
	gchar *cNewDesktopFileName = cairo_dock_generate_desktop_filename ("file-launcher.desktop", g_cCurrentLaunchersPath);

	//\___________________ On ecrit tout.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_write_keys_to_file (pKeyFile, cNewDesktopFilePath);
	g_free (cNewDesktopFilePath);
	g_key_file_free (pKeyFile);

	return cNewDesktopFileName;
}


gchar *cairo_dock_add_desktop_file_from_uri_full (const gchar *cURI, const gchar *cDockName, double fOrder, CairoDock *pDock, CairoDockNewLauncherType iNewLauncherType, GError **erreur)
{
	cd_message ("%s (%s)", __func__, cURI);
	double fEffectiveOrder;
	if (fOrder == CAIRO_DOCK_LAST_ORDER)
	{
		Icon *pLastIcon = cairo_dock_get_last_launcher (pDock->icons);
		if (pLastIcon != NULL)
			fEffectiveOrder = pLastIcon->fOrder + 1;
		else
			fEffectiveOrder = 1;
	}
	else
		fEffectiveOrder = fOrder;

	//\_________________ On regarde si c'est un repertoire ou un fichier ou sinon un fichier cree a partir de zero.
	GError *tmp_erreur = NULL;
	gchar *cNewDesktopFileName;
	if (cURI == NULL)
	{
		cNewDesktopFileName = cairo_dock_generate_desktop_file_for_edition (iNewLauncherType, cDockName, fEffectiveOrder, pDock, &tmp_erreur);
	}
	else if (g_str_has_suffix (cURI, ".desktop"))  // && (strncmp (cURI, "file://", 7) == 0 || *cURI == '/')
	{
		cNewDesktopFileName = cairo_dock_generate_desktop_file_for_launcher (cURI, cDockName, fEffectiveOrder, pDock, &tmp_erreur);
	}
	else
	{
		cNewDesktopFileName = cairo_dock_generate_desktop_file_for_file (cURI, cDockName, fEffectiveOrder, pDock, &tmp_erreur);
	}

	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
	}
	return cNewDesktopFileName;
}


gchar *cairo_dock_generate_desktop_filename (gchar *cBaseName, gchar *cCairoDockDataDir)
{
	int iPrefixNumber = 0;
	GString *sFileName = g_string_new ("");

	do
	{
		iPrefixNumber ++;
		g_string_printf (sFileName, "%s/%02d%s", cCairoDockDataDir, iPrefixNumber, cBaseName);
	} while (iPrefixNumber < 99 && g_file_test (sFileName->str, G_FILE_TEST_EXISTS));

	g_string_free (sFileName, TRUE);
	if (iPrefixNumber == 99)
		return NULL;
	else
		return g_strdup_printf ("%02d%s", iPrefixNumber, cBaseName);
}


void cairo_dock_update_launcher_desktop_file (gchar *cDesktopFilePath, CairoDockNewLauncherType iLauncherType)
{
	GError *erreur = NULL;
	GKeyFile *pKeyFile = g_key_file_new ();
	g_key_file_load_from_file (pKeyFile, cDesktopFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &erreur);
	if (erreur != NULL)
	{
		cd_warning ("Attention : %s", erreur->message);
		g_error_free (erreur);
		return ;
	}

	if (cairo_dock_conf_file_needs_update (pKeyFile, CAIRO_DOCK_VERSION))
	{
		cairo_dock_flush_conf_file_full (pKeyFile, cDesktopFilePath, CAIRO_DOCK_SHARE_DATA_DIR, FALSE, (iLauncherType == CAIRO_DOCK_LAUNCHER_FOR_CONTAINER ? CAIRO_DOCK_CONTAINER_CONF_FILE : iLauncherType == CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR ? CAIRO_DOCK_SEPARATOR_CONF_FILE : CAIRO_DOCK_LAUNCHER_CONF_FILE));
		g_key_file_free (pKeyFile);
		pKeyFile = g_key_file_new ();
		g_key_file_load_from_file (pKeyFile, cDesktopFilePath, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);
	}
	
	cairo_dock_update_conf_file_with_containers (pKeyFile, cDesktopFilePath);
	
	g_key_file_free (pKeyFile);
}


gchar *cairo_dock_get_launcher_template_conf_file (CairoDockNewLauncherType iNewLauncherType)
{
	return g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, (iNewLauncherType == CAIRO_DOCK_LAUNCHER_FOR_CONTAINER ? CAIRO_DOCK_CONTAINER_CONF_FILE : iNewLauncherType == CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR ? CAIRO_DOCK_SEPARATOR_CONF_FILE : CAIRO_DOCK_LAUNCHER_CONF_FILE));
}
