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

#include "gldi-config.h"  // GLDI_VERSION
#include "cairo-dock-icon-facility.h"  // 
#include "cairo-dock-dock-facility.h"  // cairo_dock_trigger_redraw_subdock_content_on_icon
#include "cairo-dock-config.h"  // cairo_dock_update_conf_file
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-backends-manager.h"  // cairo_dock_set_renderer
#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_generate_unique_filename
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_conf_file_needs_update
#include "cairo-dock-themes-manager.h"  // cairo_dock_update_conf_file
#include "cairo-dock-stack-icon-manager.h"

// public (manager, config, data)
GldiObjectManager myStackIconObjectMgr;

// dependancies
extern gchar *g_cCurrentLaunchersPath;

// private
#define CAIRO_DOCK_CONTAINER_CONF_FILE "container.desktop"


static void _load_image (Icon *icon)
{
	int iWidth = cairo_dock_icon_get_allocated_width (icon);
	int iHeight = cairo_dock_icon_get_allocated_height (icon);
	cairo_surface_t *pSurface = NULL;
	
	if (icon->pSubDock != NULL && icon->iSubdockViewType != 0)  // a stack rendering is specified, we'll draw it when the sub-icons will be loaded
	{
		pSurface = cairo_dock_create_blank_surface (iWidth, iHeight);
		cairo_dock_trigger_redraw_subdock_content_on_icon (icon);  // now that the icon has a surface/texture, we can draw the sub-dock content on it.
	}
	else if (icon->cFileName)  // else simply draw the image
	{
		gchar *cIconPath = cairo_dock_search_icon_s_path (icon->cFileName, MAX (iWidth, iHeight));
		if (cIconPath != NULL && *cIconPath != '\0')
			pSurface = cairo_dock_create_surface_from_image_simple (cIconPath,
				iWidth,
				iHeight);
		g_free (cIconPath);
	}
	cairo_dock_load_image_buffer_from_surface (&icon->image, pSurface, iWidth, iHeight);
}


gchar *gldi_stack_icon_add_conf_file (const gchar *cDockName, double fOrder)
{
	//\__________________ open the template.
	const gchar *cTemplateFile = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_CONTAINER_CONF_FILE;	
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cTemplateFile);
	g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	//\__________________ fill the parameters
	g_key_file_set_double (pKeyFile, "Desktop Entry", "Order", fOrder);
	g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", cDockName);
	
	gchar *cNewDesktopFileName = cairo_dock_generate_unique_filename ("container.desktop", g_cCurrentLaunchersPath);
	
	//\__________________ write the keys.
	gchar *cNewDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, cNewDesktopFileName);
	cairo_dock_write_keys_to_conf_file (pKeyFile, cNewDesktopFilePath);
	g_free (cNewDesktopFilePath);
	g_key_file_free (pKeyFile);
	return cNewDesktopFileName;
}


Icon *gldi_stack_icon_add_new (CairoDock *pDock, double fOrder)
{
	//\_________________ add a launcher in the current theme
	const gchar *cDockName = gldi_dock_get_name (pDock);
	if (fOrder == CAIRO_DOCK_LAST_ORDER)  // the order is not defined -> place at the end
	{
		Icon *pLastIcon = cairo_dock_get_last_launcher (pDock->icons);
		fOrder = (pLastIcon ? pLastIcon->fOrder + 1 : 1);
	}
	gchar *cNewDesktopFileName = gldi_stack_icon_add_conf_file (cDockName, fOrder);
	g_return_val_if_fail (cNewDesktopFileName != NULL, NULL);
	
	//\_________________ load the new icon
	Icon *pNewIcon = gldi_user_icon_new (cNewDesktopFileName);
	g_free (cNewDesktopFileName);
	g_return_val_if_fail (pNewIcon, NULL);
	
	gldi_icon_insert_in_container (pNewIcon, CAIRO_CONTAINER(pDock), CAIRO_DOCK_ANIMATE_ICON);
	
	/// TODO: check without these 2 lines, with a box drawer...
	///if (pNewIcon->pSubDock != NULL)
	///	cairo_dock_trigger_redraw_subdock_content (pNewIcon->pSubDock);
	
	return pNewIcon;
}


  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	Icon *icon = (Icon*)obj;
	GldiUserIconAttr *pAttributes = (GldiUserIconAttr*)attr;
	g_return_if_fail (pAttributes->pKeyFile != NULL);
	
	icon->iface.load_image = _load_image;
	
	// get additional parameters
	GKeyFile *pKeyFile = pAttributes->pKeyFile;
	gboolean bNeedUpdate = FALSE;
	
	icon->cFileName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Icon", NULL);
	if (icon->cFileName != NULL && *icon->cFileName == '\0')
	{
		g_free (icon->cFileName);
		icon->cFileName = NULL;
	}
	
	icon->cName = g_key_file_get_locale_string (pKeyFile, "Desktop Entry", "Name", NULL, NULL);
	if (icon->cName != NULL && *icon->cName == '\0')
	{
		g_free (icon->cName);
		icon->cName = NULL;
	}
	if (g_strcmp0 (icon->cName, icon->cParentDockName) == 0)  // it shouldn't happen, but if ever it does, be sure to forbid an icon pointing on itself.
	{
		cd_warning ("It seems we have a sub-dock in itself! => its parent dock is now the main dock");
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Container", CAIRO_DOCK_MAIN_DOCK_NAME);  // => to the main dock
		bNeedUpdate = TRUE;
		g_free (icon->cParentDockName);
		icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);
	}
	g_return_if_fail (icon->cName != NULL);
	
	icon->iSubdockViewType = g_key_file_get_integer (pKeyFile, "Desktop Entry", "render", NULL);  // on a besoin d'un entier dans le panneau de conf pour pouvoir degriser des options selon le rendu choisi. De plus c'est utile aussi pour Animated Icons...
	
	// create its sub-dock
	gchar *cSubDockRendererName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Renderer", NULL);
	if (icon->cName != NULL)
	{
		CairoDock *pChildDock = gldi_dock_get (icon->cName);  // the dock might already exists (if an icon inside has been created beforehand), in this case it's still a root dock
		if (pChildDock && (pChildDock->iRefCount > 0 || pChildDock->bIsMainDock))  // if ever a sub-dock with this name already exists, change the icon's name to avoid the conflict
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
		CairoDock *pParentDock = gldi_dock_get (icon->cParentDockName);  // the icon is not yet inserted, but its dock has been created by the parent class.
		if (pChildDock == NULL)
		{
			cd_message ("The child dock (%s) doesn't exist, we create it with this view: %s", icon->cName, cSubDockRendererName);
			pChildDock = gldi_subdock_new (icon->cName, cSubDockRendererName, pParentDock, NULL);
		}
		else
		{
			cd_message ("The dock is now a 'child-dock' (%d, %d)", pChildDock->container.bIsHorizontal, pChildDock->container.bDirectionUp);
			gldi_dock_make_subdock (pChildDock, pParentDock, cSubDockRendererName);
		}
		icon->pSubDock = pChildDock;
	}
	g_free (cSubDockRendererName);
	
	if (! bNeedUpdate)
		bNeedUpdate = cairo_dock_conf_file_needs_update (pKeyFile, GLDI_VERSION);
	if (bNeedUpdate)
	{
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pAttributes->cConfFileName);
		const gchar *cTemplateFile = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_CONTAINER_CONF_FILE;
		cairo_dock_upgrade_conf_file (cDesktopFilePath, pKeyFile, cTemplateFile);  // update keys
		g_free (cDesktopFilePath);
	}
}

static gboolean delete_object (GldiObject *obj)
{
	Icon *icon = (Icon*)obj;
	
	if (icon->pSubDock != NULL)  // delete all sub-icons as well
	{
		GList *pSubIcons = icon->pSubDock->icons;
		icon->pSubDock->icons = NULL;
		GList *ic;
		for (ic = pSubIcons; ic != NULL; ic = ic->next)
		{
			Icon *pIcon = ic->data;
			cairo_dock_set_icon_container (pIcon, NULL);
			gldi_object_delete (GLDI_OBJECT(pIcon));
		}
		g_list_free (pSubIcons);
		
		gldi_object_unref (GLDI_OBJECT(icon->pSubDock));  // probably not useful to do that here...
		icon->pSubDock = NULL;
	}
	return TRUE;
}

static GKeyFile* reload_object (GldiObject *obj, gboolean bReloadConf, GKeyFile *pKeyFile)
{
	Icon *icon = (Icon*)obj;
	if (bReloadConf)
		g_return_val_if_fail (pKeyFile != NULL, NULL);
	
	// get additional parameters
	g_free (icon->cFileName);
	icon->cFileName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Icon", NULL);
	if (icon->cFileName != NULL && *icon->cFileName == '\0')
	{
		g_free (icon->cFileName);
		icon->cFileName = NULL;
	}
	
	gchar *cName = icon->cName;
	icon->cName = g_key_file_get_locale_string (pKeyFile, "Desktop Entry", "Name", NULL, NULL);
	if (icon->cName == NULL || *icon->cName == '\0')  // no name defined, we need one.
	{
		g_free (icon->cName);
		if (cName != NULL)
			icon->cName = g_strdup (cName);
		else
			icon->cName = cairo_dock_get_unique_dock_name ("sub-dock");
		g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", icon->cName);
		gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
		cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
		g_free (cDesktopFilePath);
	}
	
	// if it has changed, ensure its unicity, and rename the sub-dock to be able to link with it again.
	if (g_strcmp0 (icon->cName, cName) != 0)  // name has changed -> rename the sub-dock.
	{
		// ensure unicity
		gchar *cUniqueName = cairo_dock_get_unique_dock_name (icon->cName);
		if (strcmp (icon->cName, cUniqueName) != 0)
		{
			g_free (icon->cName);
			icon->cName = cUniqueName;
			cUniqueName = NULL;
			g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", icon->cName);
			gchar *cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->cDesktopFileName);
			cairo_dock_write_keys_to_file (pKeyFile, cDesktopFilePath);
			g_free (cDesktopFilePath);
		}
		g_free (cUniqueName);
		
		// rename sub-dock
		cd_debug ("on renomme a l'avance le sous-dock en %s", icon->cName);
		if (icon->pSubDock != NULL)
			gldi_dock_rename (icon->pSubDock, icon->cName);  // also updates sub-icon's container name
	}
	
	icon->iSubdockViewType = g_key_file_get_integer (pKeyFile, "Desktop Entry", "render", NULL);  // on a besoin d'un entier dans le panneau de conf pour pouvoir degriser des options selon le rendu choisi. De plus c'est utile aussi pour Animated Icons...
	
	gchar *cSubDockRendererName = g_key_file_get_string (pKeyFile, "Desktop Entry", "Renderer", NULL);
	
	//\_____________ reload icon
	// redraw icon
	cairo_dock_load_icon_image (icon, icon->pContainer);
	
	// reload label
	if (g_strcmp0 (cName, icon->cName) != 0)
		cairo_dock_load_icon_text (icon);
	
	// set sub-dock renderer
	if (icon->pSubDock != NULL)
	{
		if (g_strcmp0 (cSubDockRendererName, icon->pSubDock->cRendererName) != 0)
		{
			cairo_dock_set_renderer (icon->pSubDock, cSubDockRendererName);
			cairo_dock_update_dock_size (icon->pSubDock);
		}
	}
	
	g_free (cSubDockRendererName);
	g_free (cName);
	
	return pKeyFile;
}

void gldi_register_stack_icons_manager (void)
{
	// Object Manager
	memset (&myStackIconObjectMgr, 0, sizeof (GldiObjectManager));
	myStackIconObjectMgr.cName         = "StackIcon";
	myStackIconObjectMgr.iObjectSize   = sizeof (GldiStackIcon);
	// interface
	myStackIconObjectMgr.init_object   = init_object;
	myStackIconObjectMgr.reset_object  = NULL;  // no need to unref the sub-dock, it's done upstream
	myStackIconObjectMgr.delete_object = delete_object;
	myStackIconObjectMgr.reload_object = reload_object;
	// signals
	gldi_object_install_notifications (GLDI_OBJECT (&myStackIconObjectMgr), NB_NOTIFICATIONS_STACK_ICON);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myStackIconObjectMgr), &myUserIconObjectMgr);
}
