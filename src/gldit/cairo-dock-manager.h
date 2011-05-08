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

#ifndef __GLDI_MANAGER__
#define  __GLDI_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-manager.h This class defines the Managers. Managers are the core of Cairo-Dock. A Manager is a set of parameters and an interface, and manages all the ressources associated to its functions.
* 
* Each manager is initialized at the beginning.
* When loading the current theme, get_config and load are called.
* When unloading the current theme, unload and reset_config are called.
* When reloading a part of the current theme, reset_config, get_config and load are called.
*/

typedef gpointer GldiManagerConfigPtr;
typedef gpointer GldiManagerDataPtr;
typedef void (*GldiManagerInitFunc) (void);
typedef void (*GldiManagerLoadFunc) (void);
typedef void (*GldiManagerUnloadFunc) (void);
typedef void (* GldiManagerReloadFunc) (GldiManagerConfigPtr *pPrevConfig, GldiManagerConfigPtr *pNewConfig);
typedef gboolean (* GldiManagerGetConfigFunc) (GKeyFile *pKeyFile, GldiManagerConfigPtr *pConfig);
typedef void (* GldiManagerResetConfigFunc) (GldiManagerConfigPtr *pConfig);
struct _GldiManager {
	// list of available notifications.
	GPtrArray *pNotificationsTab;
	//\_____________ Carte de visite.
	const gchar *cModuleName;
	gint iSizeOfConfig;
	gint iSizeOfData;
	//\_____________ Interface.
	/// function called once and for all at the init of the core.
	GldiManagerInitFunc init;
	/// function called when loading the current theme, after getting the config
	GldiManagerLoadFunc load;
	/// function called when unloading the current theme, before resetting the config.
	GldiManagerUnloadFunc unload;
	/// function called when reloading a part of the current theme.
	GldiManagerReloadFunc reload;
	/// function called when getting the config of the current theme, or a part of it.
	GldiManagerGetConfigFunc get_config;
	/// function called when resetting the current theme, or a part of it.
	GldiManagerResetConfigFunc reset_config;
	//\_____________ Instance.
	GldiManagerConfigPtr pConfig;
	GldiManagerDataPtr pData;
	GList *pExternalModules;
};

#define GLDI_MANAGER(m) ((GldiManager*)(m))

// facility
void gldi_reload_manager_from_keyfile (GldiManager *pManager, GKeyFile *pKeyFile);

void gldi_reload_manager (GldiManager *pManager, const gchar *cConfFilePath);  // expose pour Dbus.

void gldi_extend_manager (CairoDockVisitCard *pVisitCard, const gchar *cManagerName);

// manager
void gldi_register_manager (GldiManager *pManager);

GldiManager *gldi_get_manager (const gchar *cName);

void gldi_init_managers (void);

gboolean gldi_get_managers_config_from_key_file (GKeyFile *pKeyFile);

void gldi_get_managers_config (const gchar *cConfFilePath, const gchar *cVersion);

void gldi_load_managers (void);

void gldi_unload_managers (void);


#endif
