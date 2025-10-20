/*
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


#ifndef __CAIRO_DOCK_DATA_RENDERER_MANAGER__
#define  __CAIRO_DOCK_DATA_RENDERER_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-object.h"
#include "cairo-dock-data-renderer.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-data-renderer-manager.h This class manages the list of available Data Renderers and their global ressources.
*/

// manager

#ifndef _MANAGER_DEF_
extern GldiObjectManager myDataRendererObjectMgr;
#endif

// no params

// signals
typedef enum {
	NB_NOTIFICATIONS_DATA_RENDERERS = NB_NOTIFICATIONS_OBJECT
	} CairoDataRendererNotifications;

struct _CairoDockDataRendererRecord {
	CairoDataRendererInterface interface;
	gulong iStructSize;
	const gchar *cThemeDirName;
	const gchar *cDistantThemeDirName;
	const gchar *cDefaultTheme;
	/// whether the data-renderer draws on an overlay rather than directly on the icon.
	gboolean bUseOverlay;
	};


/** Say if an object is a DataRenderer.
*@param obj the object.
*@return TRUE if the object is a DataRenderer.
*/
#define GLDI_OBJECT_IS_DATA_RENDERER(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myDataRendererObjectMgr)


/** Get the default GLX font for Data Renderer. It can render strings of ASCII characters fastly. Don't destroy it.
*@return the default GLX font*/
CairoDockGLFont *cairo_dock_get_default_data_renderer_font (void);

void cairo_dock_unload_default_data_renderer_font (void);  // merge with unload


CairoDockDataRendererRecord *cairo_dock_get_data_renderer_record (const gchar *cRendererName);  // peu utile.

void cairo_dock_register_data_renderer (const gchar *cRendererName, CairoDockDataRendererRecord *pRecord);

void cairo_dock_remove_data_renderer (const gchar *cRendererName);


CairoDataRenderer *cairo_dock_new_data_renderer (const gchar *cRendererName);


GHashTable *cairo_dock_list_available_themes_for_data_renderer (const gchar *cRendererName);

gchar *cairo_dock_get_data_renderer_theme_path (const gchar *cRendererName, const gchar *cThemeName, CairoDockPackageType iType);

gchar *cairo_dock_get_package_path_for_data_renderer (const gchar *cRendererName, const gchar *cAppletConfFilePath, GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultThemeName);


void gldi_register_data_renderers_manager (void);

G_END_DECLS
#endif
