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
*@file cairo-dock-manager.h This class defines the Managers. A Manager is like an internal module: it has a classic module interface, manages a set of ressources, and has its own configuration.
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


// facility
/*void gldi_manager_reload_from_keyfile (GldiManager *pManager, GKeyFile *pKeyFile);

void gldi_manager_reload (GldiManager *pManager, const gchar *cConfFilePath);  // expose pour Dbus.
*/
void gldi_manager_extend (GldiVisitCard *pVisitCard, const gchar *cManagerName);

// manager
GldiManager *gldi_manager_get (const gchar *cName);

void gldi_managers_init (void);

gboolean gldi_managers_get_config_from_key_file (GKeyFile *pKeyFile);

void gldi_managers_get_config (const gchar *cConfFilePath, const gchar *cVersion);

void gldi_managers_load (void);

void gldi_managers_unload (void);

void gldi_managers_foreach (GFunc callback, gpointer data);


#define	GLDI_STR_HELPER(x) #x
#define	GLDI_STR(x) GLDI_STR_HELPER(x)

#define GLDI_MGR_NAME(name) my##name##sMgr
#define GLDI_MGR_PARAM_NAME(name) my##name##sParam
#define GLDI_MGR_TYPE(name) Cairo##name##sManager
#define GLDI_MGR_OBJECT_TYPE(name) Cairo##name
#define GLDI_MGR_PARAM_TYPE(name) Cairo##name##sParam

#define GLDI_MGR_HAS_INIT         mgr->init          = init;
#define GLDI_MGR_HAS_LOAD         mgr->load          = load;
#define GLDI_MGR_HAS_UNLOAD       mgr->unload        = unload;
#define GLDI_MGR_HAS_RELOAD       mgr->reload        = (GldiManagerReloadFunc)reload;
#define GLDI_MGR_HAS_GET_CONFIG(name)   mgr->get_config    = (GldiManagerGetConfigFunc)get_config;\
	memset (&GLDI_MGR_PARAM_NAME(name), 0, sizeof (GLDI_MGR_PARAM_TYPE(name)));\
	mgr->pConfig = (GldiManagerConfigPtr)&GLDI_MGR_PARAM_NAME(name);\
	mgr->iSizeOfConfig = sizeof (GLDI_MGR_PARAM_TYPE(name));
#define GLDI_MGR_HAS_RESET_CONFIG mgr->reset_config  = (GldiManagerResetConfigFunc)reset_config;
#define GLDI_MGR_HAS_OBJECT(name)       mgr->iObjectSize   = sizeof (GLDI_MGR_OBJECT_TYPE(name));
#define GLDI_MGR_HAS_INIT_OBJECT  mgr->init_object   = init_object;
#define GLDI_MGR_HAS_RESET_OBJECT mgr->reset_object  = reset_object;

#define GLDI_MGR_DERIVES_FROM(mgr2) gldi_object_set_manager (GLDI_OBJECT (mgr), GLDI_MANAGER (mgr2));


#define GLDI_MANAGER_DEFINE_BEGIN(name, NAME) \
void gldi_register_##name##s_manager (void) { \
	GldiManager *mgr = GLDI_MANAGER(&GLDI_MGR_NAME(name)); \
	memset (mgr, 0, sizeof (GLDI_MGR_TYPE(name))); \
	mgr->cModuleName  = GLDI_STR(name##s); \
	gldi_object_install_notifications (mgr, NB_NOTIFICATIONS_##NAME); \
	gldi_register_manager (mgr);

#define GLDI_MANAGER_DEFINE_END }


void gldi_register_managers_manager (void);

G_END_DECLS
#endif
