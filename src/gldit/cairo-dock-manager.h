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
#include "cairo-dock-object.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-manager.h This class defines the Managers. A Manager is like an internal module: it has a classic module interface, manages a set of resources, and has its own configuration.
* 
* Each manager is initialized at the beginning.
* When loading the current theme, get_config and load are called.
* When unloading the current theme, unload and reset_config are called.
* When reloading a part of the current theme, reset_config, get_config and load are called.
*/

#ifndef __MANAGER_DEF__
extern GldiObjectManager myManagerObjectMgr;
#endif

// signals
typedef enum {
	NB_NOTIFICATIONS_MANAGER = NB_NOTIFICATIONS_OBJECT,
	} GldiManagerNotifications;

typedef gpointer GldiManagerConfigPtr;
typedef gpointer GldiManagerDataPtr;
typedef void (*GldiManagerInitFunc) (void);
typedef void (*GldiManagerLoadFunc) (void);
typedef void (*GldiManagerUnloadFunc) (void);
typedef void (* GldiManagerReloadFunc) (GldiManagerConfigPtr pPrevConfig, GldiManagerConfigPtr pNewConfig);
typedef gboolean (* GldiManagerGetConfigFunc) (GKeyFile *pKeyFile, GldiManagerConfigPtr pConfig);
typedef void (* GldiManagerResetConfigFunc) (GldiManagerConfigPtr pConfig);
/// Definition of a Manager.
struct _GldiManager {
	/// object
	GldiObject object;
	//\_____________ Visit card.
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
	gboolean bInitIsDone;
	GldiManager *pDependence;  // only 1 at the moment, can be a GSList if needed
};

#define GLDI_MANAGER(m) ((GldiManager*)(m))

/** Say if an object is a Manager.
*@param obj the object.
*@return TRUE if the object is a Manager.
*/
#define GLDI_OBJECT_IS_MANAGER(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myManagerObjectMgr)

void gldi_manager_extend (GldiVisitCard *pVisitCard, const gchar *cManagerName);

// manager
GldiManager *gldi_manager_get (const gchar *cName);

void gldi_managers_init (void);

gboolean gldi_managers_get_config_from_key_file (GKeyFile *pKeyFile);

void gldi_managers_get_config (const gchar *cConfFilePath, const gchar *cVersion);

void gldi_managers_load (void);

void gldi_managers_unload (void);

void gldi_managers_foreach (GFunc callback, gpointer data);


void gldi_register_managers_manager (void);


G_END_DECLS
#endif
