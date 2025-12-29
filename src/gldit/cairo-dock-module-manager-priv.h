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

#ifndef __GLDI_MODULES_MANAGER_PRIV__
#define __GLDI_MODULES_MANAGER_PRIV__

#include "cairo-dock-module-manager.h"
G_BEGIN_DECLS

typedef struct _GldiModulesParam GldiModulesParam;
// params
struct _GldiModulesParam {
	gchar **cActiveModuleList;
	};

#ifndef _MANAGER_DEF_
extern GldiModulesParam myModulesParam;
#endif


#define gldi_module_is_auto_loaded(pModule) ((pModule->pInterface->initModule == NULL || pModule->pInterface->stopModule == NULL || pModule->pVisitCard->cInternalModule != NULL) && pModule->pVisitCard->iContainerType == CAIRO_DOCK_MODULE_IS_PLUGIN)

/** Create new modules from all the .so files contained in the given folder.
* @param cModuleDirPath path to the folder
* @param erreur an error
* @return the new module, or NULL if an error occurred.
*/
void gldi_modules_new_from_directory (const gchar *cModuleDirPath, GError **erreur);

//!! to be removed
void gldi_modules_write_active (void);


/** Load the config of all auto-loaded modules without activating them. */
void gldi_modules_load_auto_config (void);

void gldi_modules_activate_from_list (gchar **cActiveModuleList);

void gldi_modules_deactivate_all (void);

void gldi_register_modules_manager (void);

G_END_DECLS
#endif

