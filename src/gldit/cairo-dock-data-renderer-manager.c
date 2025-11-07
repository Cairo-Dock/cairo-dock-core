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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gldi-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-manager.h"
#include "cairo-dock-config.h"  // cairo_dock_get_string_key_value
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_write_keys_to_file
#include "cairo-dock-opengl-font.h"
#define _MANAGER_DEF_
#include "cairo-dock-data-renderer-manager.h"

// public (manager, config, data)
GldiManager myDataRenderersMgr;
GldiObjectManager myDataRendererObjectMgr;

// dependencies
extern gboolean g_bUseOpenGL;
extern gchar *g_cExtrasDirPath;

// private
static GHashTable *s_hDataRendererTable = NULL;  // table des rendus de donnees disponibles.
static CairoDockGLFont *s_pFont = NULL;

  ////////////
 /// FONT ///
////////////

#define _init_data_renderer_font(...) s_pFont = cairo_dock_load_textured_font ("Monospace Bold 12", 0, 184)  // on va jusqu'a ø

CairoDockGLFont *cairo_dock_get_default_data_renderer_font (void)
{
	if (s_pFont == NULL)
		_init_data_renderer_font ();
	return s_pFont;
}

void cairo_dock_unload_default_data_renderer_font (void)
{
	cairo_dock_free_gl_font (s_pFont);
	s_pFont = NULL;
}


  /////////////////////////
 /// LIST OF RENDERERS ///
/////////////////////////

CairoDockDataRendererRecord *cairo_dock_get_data_renderer_record (const gchar *cRendererName)
{
	if (cRendererName != NULL)
		return g_hash_table_lookup (s_hDataRendererTable, cRendererName);
	else
		return NULL;
}

void cairo_dock_register_data_renderer (const gchar *cRendererName, CairoDockDataRendererRecord *pRecord)
{
	cd_message ("%s (%s)", __func__, cRendererName);
	g_hash_table_insert (s_hDataRendererTable, g_strdup (cRendererName), pRecord);
}

void cairo_dock_remove_data_renderer (const gchar *cRendererName)
{
	g_hash_table_remove (s_hDataRendererTable, cRendererName);
}


CairoDataRenderer *cairo_dock_new_data_renderer (const gchar *cRendererName)
{
	CairoDockDataRendererRecord *pRecord = cairo_dock_get_data_renderer_record (cRendererName);
	g_return_val_if_fail (pRecord != NULL && pRecord->iStructSize != 0, NULL);
	
	if (g_bUseOpenGL && s_pFont == NULL)
	{
		_init_data_renderer_font ();
	}
	
	CairoDataRenderer *pRenderer = g_malloc0 (pRecord->iStructSize);
	memcpy (&pRenderer->interface, &pRecord->interface, sizeof (CairoDataRendererInterface));
	pRenderer->bUseOverlay = pRecord->bUseOverlay;
	return pRenderer;
}


  //////////////////////
 /// LIST OF THEMES ///
//////////////////////

GHashTable *cairo_dock_list_available_themes_for_data_renderer (const gchar *cRendererName)
{
	CairoDockDataRendererRecord *pRecord = cairo_dock_get_data_renderer_record (cRendererName);
	g_return_val_if_fail (pRecord != NULL, NULL);
	
	if (pRecord->cThemeDirName == NULL && pRecord->cDistantThemeDirName == NULL)
		return NULL;
	gchar *cGaugeShareDir = g_strdup_printf ("%s/%s", GLDI_SHARE_DATA_DIR, pRecord->cThemeDirName);
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s", g_cExtrasDirPath, pRecord->cThemeDirName);
	GHashTable *pGaugeTable = cairo_dock_list_packages (cGaugeShareDir, cGaugeUserDir, pRecord->cDistantThemeDirName, NULL);
	
#ifdef GLDI_PLUGINS_DATA_ROOT
	// case when plugins are installed under a separate prefix -- they can provide additional themes as well
	g_free (cGaugeShareDir);
	cGaugeShareDir = g_strdup_printf ("%s/%s", GLDI_PLUGINS_DATA_ROOT, pRecord->cThemeDirName);
	pGaugeTable = cairo_dock_list_packages (cGaugeShareDir, NULL, NULL, pGaugeTable);
#endif

	g_free (cGaugeShareDir);
	g_free (cGaugeUserDir);
	return pGaugeTable;
}


gchar *cairo_dock_get_data_renderer_theme_path (const gchar *cRendererName, const gchar *cThemeName, CairoDockPackageType iType)  // utile pour DBus aussi.
{
	CairoDockDataRendererRecord *pRecord = cairo_dock_get_data_renderer_record (cRendererName);
	g_return_val_if_fail (pRecord != NULL, NULL);
	
	if (pRecord->cThemeDirName == NULL && pRecord->cDistantThemeDirName == NULL)
		return NULL;
	
	gchar *cThemePath = NULL;
	gchar *cGaugeShareDir = g_strdup_printf (GLDI_SHARE_DATA_DIR"/%s", pRecord->cThemeDirName);
#ifdef GLDI_PLUGINS_DATA_ROOT
	cThemePath = cairo_dock_get_package_path (cThemeName, cGaugeShareDir, NULL, NULL, iType);
	g_free (cGaugeShareDir);
	if (cThemePath) return cThemePath;
	cGaugeShareDir = g_strdup_printf (GLDI_PLUGINS_DATA_ROOT"/%s", pRecord->cThemeDirName);
#endif
	gchar *cGaugeUserDir = g_strdup_printf ("%s/%s", g_cExtrasDirPath, pRecord->cThemeDirName);
	cThemePath = cairo_dock_get_package_path (cThemeName, cGaugeShareDir, cGaugeUserDir, pRecord->cDistantThemeDirName, iType);
	g_free (cGaugeUserDir);
	g_free (cGaugeShareDir);
	return cThemePath;
}

gchar *cairo_dock_get_package_path_for_data_renderer (const gchar *cRendererName, const gchar *cAppletConfFilePath, GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultThemeName)
{
	CairoDockDataRendererRecord *pRecord = cairo_dock_get_data_renderer_record (cRendererName);
	g_return_val_if_fail (pRecord != NULL, NULL);
	
	gchar *cChosenThemeName = cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, bFlushConfFileNeeded, cDefaultThemeName, NULL, NULL);
	if (cChosenThemeName == NULL)
		cChosenThemeName = g_strdup (pRecord->cDefaultTheme);
	
	CairoDockPackageType iType = cairo_dock_extract_package_type_from_name (cChosenThemeName);
	gchar *cThemePath = cairo_dock_get_data_renderer_theme_path (cRendererName, cChosenThemeName, iType);
	
	if (cThemePath == NULL)  // theme introuvable.
		cThemePath = g_strdup_printf (GLDI_SHARE_DATA_DIR"/%s/%s", pRecord->cThemeDirName, pRecord->cDefaultTheme);
	
	if (iType != CAIRO_DOCK_ANY_PACKAGE)
	{
		g_key_file_set_string (pKeyFile, cGroupName, cKeyName, cChosenThemeName);
		cairo_dock_write_keys_to_file (pKeyFile, cAppletConfFilePath);
	}
	cd_debug ("DataRenderer's theme : %s", cThemePath);
	g_free (cChosenThemeName);
	return cThemePath;
}


  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	cairo_dock_free_gl_font (s_pFont);
	s_pFont = NULL;
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	s_hDataRendererTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		g_free);
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_data_renderers_manager (void)
{
	// Manager
	memset (&myDataRenderersMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myDataRenderersMgr), &myManagerObjectMgr, NULL);
	myDataRenderersMgr.cModuleName 	= "Data-Renderers";
	// interface
	myDataRenderersMgr.init 		= init;
	myDataRenderersMgr.load 		= NULL;  // loaded on demand
	myDataRenderersMgr.unload 		= unload;
	myDataRenderersMgr.reload 		= (GldiManagerReloadFunc)NULL;
	myDataRenderersMgr.get_config 	= (GldiManagerGetConfigFunc)NULL;
	myDataRenderersMgr.reset_config = (GldiManagerResetConfigFunc)NULL;
	// Config
	myDataRenderersMgr.pConfig = (GldiManagerConfigPtr)NULL;
	myDataRenderersMgr.iSizeOfConfig = 0;
	// data
	myDataRenderersMgr.pData = (GldiManagerDataPtr)NULL;
	myDataRenderersMgr.iSizeOfData = 0;
	
	// Object Manager
	memset (&myDataRendererObjectMgr, 0, sizeof (GldiObjectManager));
	myDataRendererObjectMgr.cName 	= "Data-Renderers";
	// signals
	gldi_object_install_notifications (&myDataRendererObjectMgr, NB_NOTIFICATIONS_DATA_RENDERERS);
}
