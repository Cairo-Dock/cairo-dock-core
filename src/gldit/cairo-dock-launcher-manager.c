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

#include "gldi-config.h"  // GLDI_VERSION
#include "cairo-dock-icon-facility.h"  // cairo_dock_compare_icons_order
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-backends-manager.h"  // cairo_dock_set_renderer
#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-applications-manager.h"  // myTaskbarParam.bMixLauncherAppli
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-class-manager.h"  // cairo_dock_inhibite_class
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-separator-manager.h"  // cairo_dock_create_separator_surface
#include "cairo-dock-themes-manager.h"  // cairo_dock_update_conf_file
#include "cairo-dock-windows-manager.h"  // gldi_window_show
#include "cairo-dock-file-manager.h"  // g_iDesktopEnv
#include "cairo-dock-launcher-manager.h"

// public (manager, config, data)
GldiObjectManager myLauncherObjectMgr;

// dependancies
extern gchar *g_cCurrentLaunchersPath;
extern CairoDockDesktopEnv g_iDesktopEnv;

// private
#define CAIRO_DOCK_LAUNCHER_CONF_FILE "launcher.desktop"


static gboolean _get_launcher_params (Icon *icon, GKeyFile *pKeyFile)
{
	gboolean bNeedUpdate = FALSE;
	
	// get launcher params
	icon->cFileName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Icon", NULL);
	if (icon->cFileName != NULL && *icon->cFileName == '\0')
	{
		g_free (icon->cFileName);
		icon->cFileName = NULL;
	}
	
	icon->cName = cairo_dock_get_locale_string_from_conf_file (pKeyFile, "Desktop Entry", "Name", NULL);
	if (icon->cName != NULL && *icon->cName == '\0')
	{
		g_free (icon->cName);
		icon->cName = NULL;
	}
	
	char *cCommand = g_key_file_get_string (pKeyFile, "Desktop Entry", "Exec", NULL);
	if (cCommand != NULL && *cCommand != '\0')
	{
		icon->pCustomLauncher = g_desktop_app_info_new_from_keyfile (pKeyFile);
		g_free (cCommand);
	}
	
	gchar *cStartupWMClass = g_key_file_get_string (pKeyFile, "Desktop Entry", "StartupWMClass", NULL);
	if (cStartupWMClass && *cStartupWMClass == '\0')
	{
		g_free (cStartupWMClass);
		cStartupWMClass = NULL;
	}
	
	// get the origin of the desktop file.
	gchar *cClass = NULL;
	gsize length = 0;
	gchar **pOrigins = g_key_file_get_string_list (pKeyFile, "Desktop Entry", "Origin", &length, NULL);
	int iNumOrigin = -1;
	if (pOrigins != NULL)  // some origins are provided, try them one by one.
	{
		int i;
		for (i = 0; pOrigins[i] != NULL; i++)
		{
			cClass = cairo_dock_register_class2 (pOrigins[i], cStartupWMClass, FALSE);
			if (cClass != NULL)  // neat, this origin is a valid one, let's use it from now.
			{
				iNumOrigin = i;
				break;
			}
		}
		g_strfreev (pOrigins);
	}

	// if no origin class could be found, try to guess the class
	gchar *cFallbackClass = NULL;
	if (!cClass && icon->pCustomLauncher)  // no class found, maybe an old launcher or a custom one, try to guess from the info in the user desktop file.
	{
		// we try the StartupWMClass and the command line as fallbacks
		cFallbackClass = cairo_dock_guess_class (g_app_info_get_commandline (
			G_APP_INFO (icon->pCustomLauncher)), NULL);
			
		if (cStartupWMClass)
		{
			// first try based on the commandline (but fail if not found)
			cClass = cairo_dock_register_class2 (cFallbackClass, cStartupWMClass, FALSE);
			if (!cClass)
			{
				// re-try based on the WMClass below
				g_free (cFallbackClass);
				cFallbackClass = cairo_dock_guess_class (NULL, cStartupWMClass);
			}
		}
		
		// last attempt, register the class even if no info is found
		if (!cClass)
			cClass = cairo_dock_register_class2 (cFallbackClass, cStartupWMClass, TRUE);
	}

	// get common data from the class
	if (cClass != NULL)
	{
		icon->cClass = cClass;
		g_free (cFallbackClass);
		// this sets the display name, the icon filename and the GAppInfo used to launch the app
		cairo_dock_set_data_from_class (cClass, icon);
		if (iNumOrigin != 0)  // it's not the first origin that gave us the correct class, so let's write it down to avoid searching the next time.
		{
			g_key_file_set_string (pKeyFile, "Desktop Entry", "Origin", cairo_dock_get_class_desktop_file (cClass));
			bNeedUpdate = TRUE;
		}
	}
	
	gboolean bPreventFromInhibiting = g_key_file_get_boolean (pKeyFile, "Desktop Entry", "prevent inhibate", NULL);  // FALSE by default
	if (bPreventFromInhibiting)
	{
		g_free (icon->cClass);
		icon->cClass = NULL;
	}
	
	g_free (cStartupWMClass);
	return bNeedUpdate;
}

static void _show_appli_for_drop (Icon *pIcon)
{
	if (pIcon->pAppli != NULL)
		gldi_window_show (pIcon->pAppli);
}

static void init_object (GldiObject *obj, gpointer attr)
{
	Icon *icon = (Icon*)obj;
	GldiUserIconAttr *pAttributes = (GldiUserIconAttr*)attr;
	
	icon->iface.action_on_drag_hover = _show_appli_for_drop;  // we use the generic 'load_image' method
	
	if (!pAttributes->pKeyFile)
		return;
	
	//\____________ get additional parameters
	GKeyFile *pKeyFile = pAttributes->pKeyFile;
	gboolean bNeedUpdate = _get_launcher_params (icon, pKeyFile);
	
	if ( !(icon->pClassApp || icon->pCustomLauncher))  // no command could be found for this launcher -> mark it as invalid
		icon->reserved[0] = (gpointer)-1; // we use this as a way to tell the UserIcon manager that the icon is invalid (should add a new flag, but that would break ABI)
	
	//\____________ Make it an inhibator for its class.
	cd_message ("+ %s/%s", icon->cName, icon->cClass);
	if (icon->cClass != NULL)
	{
		cairo_dock_inhibite_class (icon->cClass, icon);  // gere le bMixLauncherAppli
	}
	
	//\____________ Update the conf file if needed.
	if (! bNeedUpdate)
		bNeedUpdate = cairo_dock_conf_file_needs_update (pKeyFile, GLDI_VERSION);
	if (bNeedUpdate)
	{
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pAttributes->cConfFileName);
		const gchar *cTemplateFile = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_LAUNCHER_CONF_FILE;
		cairo_dock_upgrade_conf_file (cDesktopFilePath, pKeyFile, cTemplateFile);  // update keys
		g_free (cDesktopFilePath);
	}
}


static GKeyFile* reload_object (GldiObject *obj, gboolean bReloadConf, GKeyFile *pKeyFile)
{
	Icon *icon = (Icon*)obj;
	if (bReloadConf)
		g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	gchar *cClass = icon->cClass;
	icon->cClass = NULL;
	gchar *cName = icon->cName;
	icon->cName = NULL;
	g_free (icon->cFileName);
	icon->cFileName = NULL;
	// need to set the appinfos to NULL to not confuse _get_launcher_params ()
	if (icon->pCustomLauncher)
	{
		g_object_unref (icon->pCustomLauncher);
		icon->pCustomLauncher = NULL;
	}
	if (icon->pClassApp)
	{
		g_object_unref (icon->pClassApp);
		icon->pClassApp = NULL;
	}
	
	//\__________________ get parameters
	_get_launcher_params (icon, pKeyFile);
	
	//\_____________ update the initial name if needed
	gboolean bNameChanged = FALSE;
	gboolean bUseInitialName = FALSE;
	if (icon->cInitialName)
	{
		bUseInitialName = !!g_strcmp0 (icon->cInitialName, cName);
		g_free (icon->cInitialName);
	}
	if (bUseInitialName)
	{
		icon->cInitialName = icon->cName;
		icon->cName = cName;
	}
	else
	{
		icon->cInitialName = NULL;
		bNameChanged = !!g_strcmp0 (cName, icon->cName);
		g_free (cName);
	}
	
	//\_____________ reload icon's buffers
	GldiContainer *pNewContainer = cairo_dock_get_icon_container (icon);
	cairo_dock_load_icon_image (icon, pNewContainer);
	
	if (bNameChanged) cairo_dock_load_icon_text (icon);
	
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
	cairo_dock_redraw_icon (icon);
	
	g_free (cClass);
	
	return pKeyFile;
}


void gldi_register_launchers_manager (void)
{
	// Object Manager
	memset (&myLauncherObjectMgr, 0, sizeof (GldiObjectManager));
	myLauncherObjectMgr.cName             = "Launcher";
	myLauncherObjectMgr.iObjectSize   = sizeof (GldiLauncherIcon);
	// interface
	myLauncherObjectMgr.init_object   = init_object;
	myLauncherObjectMgr.reload_object = reload_object;
	// signals
	gldi_object_install_notifications (&myLauncherObjectMgr, NB_NOTIFICATIONS_LAUNCHER);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myLauncherObjectMgr), &myUserIconObjectMgr);
}


Icon *gldi_launcher_new (const gchar *cConfFile, GKeyFile *pKeyFile)
{
	GldiLauncherIconAttr attr = {.cConfFileName = cConfFile, .pKeyFile = pKeyFile, GLDI_USER_ICON_TYPE_LAUNCHER};
	return (Icon*)gldi_object_new (&myLauncherObjectMgr, &attr);
}


gchar *gldi_launcher_add_conf_file (const gchar *cOrigin, const gchar *cDockName, double fOrder, gboolean bValidate)
{
	//\__________________ process the origin file path or name
	gchar *cFilePath = NULL;
	if (cOrigin != NULL && *cOrigin != '/')  // transform the origin URI into a path or a file name.
	{
		if (strncmp (cOrigin, "application://", 14) == 0)  // Ubuntu >= 11.04: it's now an "app" URI
			cFilePath = g_strdup (cOrigin + 14);  // in this case we don't have the actual path of the .desktop, but that doesn't matter.
		else
			// try to process as a file:// URI
			cFilePath = g_filename_from_uri (cOrigin, NULL, NULL);
	}
	if (!cFilePath)  // no origin, already a path, or possibly a .desktop ID.
		cFilePath = g_strdup (cOrigin);
	
	if (bValidate)
	{
		if (!cFilePath) return NULL;
		GDesktopAppInfo *app = NULL;
		if (*cFilePath == '/')
			app = g_desktop_app_info_new_from_filename (cFilePath);
		else app = g_desktop_app_info_new (cFilePath);
		if (!app)
		{
			g_free (cFilePath);
			return NULL;
		}
		g_object_unref (app); // will be re-read later, this could be optimized
	}
	
	//\__________________ open the template.
	const gchar *cTemplateFile = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_LAUNCHER_CONF_FILE;	
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cTemplateFile);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	//\__________________ fill the parameters
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Origin", cFilePath?cFilePath:"");
	
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);
	
	//\__________________ in the case of a script, set ourselves a valid name and command.
	if (cFilePath != NULL && g_str_has_suffix (cFilePath, ".sh"))
	{
		gchar *cName = g_path_get_basename (cFilePath);
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cName);
		g_free (cName);
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", cFilePath);
		g_key_file_set_boolean (pKeyFile, "Desktop Entry", "Terminal", TRUE);
	}
	
	//\__________________ in the case of a custom launcher, set a command (the launcher would be invalid without).
	if (cFilePath == NULL)
	{
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", _("Enter a command"));
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", _("New launcher"));
	}
	
	//\__________________ generate a unique and readable filename.
	gchar *cBaseName = (cFilePath ?
		*cFilePath == '/' ?
			g_path_get_basename (cFilePath) :
			g_steal_pointer (&cFilePath) :
		g_path_get_basename (cTemplateFile));

	gchar *cNewDesktopFilePath;
	if (! g_str_has_suffix (cBaseName, ".desktop")) // if we have a script (.sh file) => add '.desktop'
		cNewDesktopFilePath = g_strdup_printf ("%s/%s.desktop", g_cCurrentLaunchersPath, cBaseName);
	else cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cBaseName);
	g_free (cBaseName);
	
	//\__________________ write the keys.
	gchar *cResultPath = cairo_dock_write_keys_to_new_conf_file (pKeyFile, cNewDesktopFilePath);
	gchar *cNewDesktopFileName = g_path_get_basename (cResultPath);
	
	g_free (cNewDesktopFilePath);
	g_free (cResultPath);
	g_free (cFilePath);
	g_key_file_free (pKeyFile);
	return cNewDesktopFileName;
}


Icon *gldi_launcher_add_new_full (const gchar *cURI, CairoDock *pDock, double fOrder, gboolean bValidate)
{
	//\_________________ add a launcher in the current theme
	const gchar *cDockName = gldi_dock_get_name (pDock);
	if (fOrder == CAIRO_DOCK_LAST_ORDER)  // the order is not defined -> place at the end
	{
		Icon *pLastIcon = cairo_dock_get_last_launcher (pDock->icons);
		fOrder = (pLastIcon ? pLastIcon->fOrder + 1 : 1);
	}
	gchar *cNewDesktopFileName = gldi_launcher_add_conf_file (cURI, cDockName, fOrder, bValidate);
	g_return_val_if_fail (cNewDesktopFileName != NULL, NULL);
	
	//\_________________ load the new icon
	Icon *pNewIcon = gldi_user_icon_new (cNewDesktopFileName);
	g_free (cNewDesktopFileName);
	g_return_val_if_fail (pNewIcon, NULL);
	
	gldi_icon_insert_in_container (pNewIcon, CAIRO_CONTAINER(pDock), CAIRO_DOCK_ANIMATE_ICON);
	
	return pNewIcon;
}

Icon *gldi_launcher_add_new (const gchar *cURI, CairoDock *pDock, double fOrder)
{
	return gldi_launcher_add_new_full (cURI, pDock, fOrder, FALSE);
}

