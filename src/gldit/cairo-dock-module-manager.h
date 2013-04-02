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

#ifndef __CAIRO_DOCK_MODULES_MANAGER__
#define  __CAIRO_DOCK_MODULES_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-desklet-manager.h"
G_BEGIN_DECLS

/**
* @file cairo-dock-module-manager.h This class manages the external modules of Cairo-Dock.
*
* A module has an interface and a visit card :
*  - the visit card allows it to define itself (name, category, default icon, etc)
*  - the interface defines the entry points for init, stop, reload, read config, and reset datas.
*
* Modules can be instanciated several times; each time they are, an instance is created.
* Each instance holds all a set of the data : the icon and its container, the config structure and its conf file, the data structure and a slot to plug datas into containers and icons. All these parameters are optionnal; a module that has an icon is also called an applet.
*/

typedef struct _CairoModulesParam CairoModulesParam;
typedef struct _CairoModulesManager CairoModulesManager;
typedef struct _CairoModuleInstancesParam CairoModuleInstancesParam;
typedef struct _CairoModuleInstancesManager CairoModuleInstancesManager;

#ifndef _MANAGER_DEF_
extern CairoModulesParam myModulesParam;
extern CairoModulesManager myModulesMgr;
extern CairoModuleInstancesParam myModuleInstancesParam;
extern CairoModuleInstancesManager myModuleInstancesMgr;
#endif

// params
struct _CairoModulesParam {
	gchar **cActiveModuleList;
	};
struct _CairoModuleInstancesParam {
	gint unused;
	};

// manager
struct _CairoModulesManager {
	GldiManager mgr;
	void 			(*foreach_module) 				(GHRFunc pCallback, gpointer user_data);
	void 			(*foreach_module_in_alphabetical_order) (GCompareFunc pCallback, gpointer user_data);
	CairoDockModule* (*find_module_from_name) 		(const gchar *cModuleName);
	int 			(*get_nb_modules) 				(void);
	const gchar* 	(*get_modules_dir) 				(void);
	gchar* 			(*list_active_modules) 			(void);
	void 			(*write_active_modules_cb) 		(void);
};

struct _CairoModuleInstancesManager {
	GldiManager mgr;
};

// signals
typedef enum {
	NOTIFICATION_MODULE_REGISTERED = NB_NOTIFICATIONS_OBJECT,
	NOTIFICATION_MODULE_ACTIVATED,
	NB_NOTIFICATIONS_MODULES
	} CairoModulesNotifications;

typedef enum {
	NOTIFICATION_MODULE_INSTANCE_DETACHED = NB_NOTIFICATIONS_OBJECT,
	NB_NOTIFICATIONS_MODULE_INSTANCES
	} CairoModuleInstancesNotifications;


  /////////////
 // MANAGER //
/////////////

/** Get the module which has a given name.
*@param cModuleName the unique name of the module.
*@return the module, or NULL if not found.
*/
CairoDockModule *cairo_dock_find_module_from_name (const gchar *cModuleName);

CairoDockModule *cairo_dock_foreach_module (GHRFunc pCallback, gpointer user_data);
CairoDockModule *cairo_dock_foreach_module_in_alphabetical_order (GCompareFunc pCallback, gpointer user_data);

int cairo_dock_get_nb_modules (void);

gchar *cairo_dock_list_active_modules (void);


  ///////////////////
 // MODULE LOADER //
///////////////////

gboolean cairo_dock_register_module (CairoDockModule *pModule);
void cairo_dock_unregister_module (const gchar *cModuleName);

/** Load a module into the table of modules. The module is opened and its visit card and interface are retrieved.
*@param cSoFilePath path to the .so file.
*@return the newly allocated module.
*/
CairoDockModule * cairo_dock_load_module (const gchar *cSoFilePath);

/** Load all the modules of a given folder. If the path is NULL, plug-ins are taken in the gldi install dir.
*@param cModuleDirPath path to the a folder containing .so files.
*@param erreur error set if something bad happens.
*/
void cairo_dock_load_modules_in_directory (const gchar *cModuleDirPath, GError **erreur);

  /////////////
 // MODULES //
/////////////

void cairo_dock_activate_modules_from_list (gchar **cActiveModuleList);

void cairo_dock_deactivate_all_modules (void);


  ///////////////////////
 // MODULES HIGH LEVEL//
///////////////////////

// activate_module or reload, update_dock, redraw, write
void cairo_dock_activate_module_and_load (const gchar *cModuleName);
// deactivate_module_instance_and_unload all instances, write
void cairo_dock_deactivate_module_and_unload (const gchar *cModuleName);

// deactivate_module_instance_and_unload + remove file
void cairo_dock_remove_module_instance (CairoDockModuleInstance *pInstance);
// cp file
gchar *cairo_dock_add_module_conf_file (CairoDockModule *pModule);
// cp file + instanciate_module + update_dock_size
void cairo_dock_add_module_instance (CairoDockModule *pModule);
// update conf file + reload_module_instance
void cairo_dock_detach_module_instance (CairoDockModuleInstance *pInstance);
// update conf file + reload_module_instance
void cairo_dock_detach_module_instance_at_position (CairoDockModuleInstance *pInstance, int iCenterX, int iCenterY);


gboolean cairo_dock_reserve_data_slot (CairoDockModuleInstance *pInstance);
void cairo_dock_release_data_slot (CairoDockModuleInstance *pInstance);

#define cairo_dock_get_icon_data(pIcon, pInstance) ((pIcon)->pDataSlot[pInstance->iSlotID])
#define cairo_dock_get_container_data(pContainer, pInstance) ((pContainer)->pDataSlot[pInstance->iSlotID])

#define cairo_dock_set_icon_data(pIcon, pInstance, pData) \
	(pIcon)->pDataSlot[pInstance->iSlotID] = pData
#define cairo_dock_set_container_data(pContainer, pInstance, pData) \
	(pContainer)->pDataSlot[pInstance->iSlotID] = pData


void cairo_dock_write_active_modules (void);


void gldi_register_modules_manager (void);

G_END_DECLS
#endif
