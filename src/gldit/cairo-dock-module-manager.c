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

#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <dlfcn.h>

#include "gldi-config.h"
#include "gldi-module-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_add_conf_file
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-log.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_get_width
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-config.h"
#include "cairo-dock-module-instance-manager.h"
#define _MANAGER_DEF_
#include "cairo-dock-module-manager-priv.h"

// public (manager, config, data)
GldiModulesParam myModulesParam;
GldiManager myModulesMgr;
GldiObjectManager myModuleObjectMgr;

GldiModuleInstance *g_pCurrentModule = NULL;  // only used to trace a possible crash in one of the modules.

gboolean g_bNoWaylandExclude = FALSE; // do not exclude modules on Wayland that are known to crash
gboolean g_bDisableAllModules = FALSE; // fail loading any module (for debugging only)
gboolean g_bNoCheckModuleVersion = FALSE; // do not check module version compatibility (similar to bEasterEggs, but only applies here)
gchar **g_cExcludedModules = NULL; // specific modules to exclude (try loading them but fail, for debugging)

// dependencies
extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;
extern gboolean g_bEasterEggs;
extern gboolean g_bUseOpenGL;

// private
static GHashTable *s_hModuleTable = NULL;
static GList *s_AutoLoadedModules = NULL;
static guint s_iSidWriteModules = 0;

typedef struct _GldiModuleAttr {
	GldiVisitCard *pVisitCard;
	GldiModuleInterface *pInterface;
} GldiModuleAttr;


  ///////////////
 /// MANAGER ///
///////////////

GldiModule *gldi_module_get (const gchar *cModuleName)
{
	g_return_val_if_fail (cModuleName != NULL, NULL);
	return g_hash_table_lookup (s_hModuleTable, cModuleName);
}

GldiModule *gldi_module_foreach (GHRFunc pCallback, gpointer user_data)
{
	return g_hash_table_find (s_hModuleTable, (GHRFunc) pCallback, user_data);
}

static int _sort_module_by_alphabetical_order (GldiModule *m1, GldiModule *m2)
{
	if (!m1 || !m1->pVisitCard || !m1->pVisitCard->cTitle)
		return 1;
	if (!m2 || !m2->pVisitCard || !m2->pVisitCard->cTitle)
		return -1;
	return g_ascii_strncasecmp (m1->pVisitCard->cTitle, m2->pVisitCard->cTitle, -1);
}
GldiModule *gldi_module_foreach_in_alphabetical_order (GCompareFunc pCallback, gpointer user_data)
{
	GList *pModuleList = g_hash_table_get_values (s_hModuleTable);
	pModuleList = g_list_sort (pModuleList, (GCompareFunc) _sort_module_by_alphabetical_order);
	
	GldiModule *pModule = (GldiModule *)g_list_find_custom (pModuleList, user_data, pCallback);
	
	g_list_free (pModuleList);
	return pModule;
}

int gldi_module_get_nb (void)
{
	return g_hash_table_size (s_hModuleTable);
}

static void _write_one_module_name (const gchar *cModuleName, GldiModule *pModule, GString *pString)
{
	if (pModule->pInstancesList != NULL && ! gldi_module_is_auto_loaded (pModule))
	{
		g_string_append_printf (pString, "%s;", cModuleName);
	}
}
static gchar *_gldi_module_list_active (void)
{
	GString *pString = g_string_new ("");
	
	g_hash_table_foreach (s_hModuleTable, (GHFunc) _write_one_module_name, pString);
	
	if (pString->len > 0)
		pString->str[pString->len-1] = '\0';
	
	return g_string_free (pString, FALSE);
}

static gboolean _write_modules_idle (G_GNUC_UNUSED gpointer data)
{
	gchar *cModuleNames = _gldi_module_list_active ();
	cd_debug ("%s", cModuleNames);
	cairo_dock_update_conf_file (g_cConfFile,
		G_TYPE_STRING, "System", "modules", cModuleNames,
		G_TYPE_INVALID);
	g_free (cModuleNames);
	s_iSidWriteModules = 0;
	return FALSE;
}
void gldi_modules_write_active (void)
{
	if (s_iSidWriteModules == 0)
		s_iSidWriteModules = g_idle_add (_write_modules_idle, NULL);
}


  /////////////////////
 /// MODULE LOADER ///
/////////////////////

GldiModule *gldi_module_new (GldiVisitCard *pVisitCard, GldiModuleInterface *pInterface)
{
	g_return_val_if_fail (pVisitCard != NULL && pVisitCard->cModuleName != NULL, NULL);
	
	GldiModuleAttr attr = {pVisitCard, pInterface};
	return (GldiModule*)gldi_object_new (&myModuleObjectMgr, &attr);
}

/** Create a new module from a .so file and add it to our hashtable of modules.
* @param cSoFilePath path to the .so file
*/
static void _module_new_from_so_file (const gchar *cSoFilePath)
{
	g_return_if_fail (cSoFilePath != NULL);
	GldiVisitCard *pVisitCard = NULL;
	GldiModuleInterface *pInterface = NULL;
	
	// open the .so file
	///GModule *module = g_module_open (pGldiModule->cSoFilePath, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
	gpointer handle = dlopen (cSoFilePath, RTLD_NOW | RTLD_LOCAL);
	if (! handle)
	{
		cd_warning ("while opening module '%s' : (%s)", cSoFilePath, dlerror());
		return;
	}
	
	// find the pre-init entry point
	GldiModulePreInit function_pre_init = NULL;
	/**bSymbolFound = g_module_symbol (module, "pre_init", (gpointer) &function_pre_init);
	if (bSymbolFound && function_pre_init != NULL)*/
	function_pre_init = dlsym (handle, "pre_init");
	if (function_pre_init == NULL)
	{
		cd_warning ("this module ('%s') does not have the common entry point 'pre_init', it may be broken or icompatible with cairo-dock", cSoFilePath);
		goto discard;
	}
	
	// run the pre-init entry point to get the necessary info about the module
	pVisitCard = g_new0 (GldiVisitCard, 1);
	pInterface = g_new0 (GldiModuleInterface, 1);
	gboolean bModuleLoaded = function_pre_init (pVisitCard, pInterface);
	if (! bModuleLoaded)
	{
		cd_debug ("module '%s' has not been loaded", cSoFilePath);  // can happen to xxx-integration or icon-effect for instance.
		goto discard;
	}
	
	if (!g_bNoCheckModuleVersion && !g_bEasterEggs)
	{
		// check module compatibility
		
		if (g_bDisableAllModules)
			goto discard;
		if (g_cExcludedModules)
		{
			gchar **tmp;
			for (tmp = g_cExcludedModules; *tmp; ++tmp)
			{
				const gchar *tmp2 = strrchr (cSoFilePath, '/');
				if (tmp2) tmp2++;
				else tmp2 = cSoFilePath;
				if (!strcmp (*tmp, tmp2)) goto discard;
				size_t x = strlen (tmp2); // compare without the .so extension
				if (x > 3 && !strncmp (*tmp, tmp2, x - 3)) goto discard;
			}
		}
		
		if (pVisitCard->iMajorVersionNeeded == 4)
		{
			// new version matching, based on ABI versions (stored in iMinorVersionNeeded) instead of release versions
			if (pVisitCard->iMinorVersionNeeded != GLDI_ABI_VERSION)
			{
				cd_warning ("this module ('%s') was compiled for Cairo-Dock ABI version %d, but currently running Cairo-Dock with ABI version %d\n  It will be ignored", cSoFilePath, pVisitCard->iMinorVersionNeeded, GLDI_ABI_VERSION);
				goto discard;
			}
			// test compatibility with the windowing system backend in use (X11 or Wayland)
			// note: iMicroVersionNeeded stores additional module flags in this case
			if (gldi_container_is_wayland_backend ())
			{
				if (! (pVisitCard->iMicroVersionNeeded & CAIRO_DOCK_MODULE_SUPPORTS_WAYLAND) &&
					!g_bNoWaylandExclude)
				{
					cd_message ("Not loading module ('%s') as it does not support Wayland\n", cSoFilePath);
					goto discard;
				}
			}
			else if (! (pVisitCard->iMicroVersionNeeded & CAIRO_DOCK_MODULE_SUPPORTS_X11))
			{
				cd_message ("Not loading module ('%s') as it does not support X11\n", cSoFilePath);
				goto discard;
			}
			// test if module requires OpenGL
			if ((pVisitCard->iMicroVersionNeeded & CAIRO_DOCK_MODULE_REQUIRES_OPENGL) && !g_bUseOpenGL)
			{
				cd_message ("Not loading module ('%s') as it requires OpenGL\n", cSoFilePath);
				goto discard;
			}
			if (pVisitCard->postLoad)
				if (!pVisitCard->postLoad (pVisitCard, pInterface, NULL))
					goto discard; // this module does not want to be loaded (can happen to xxx-integration or icon-effect for instance)
		}
		else
		{
			if (pVisitCard->iMajorVersionNeeded > g_iMajorVersion
				|| (pVisitCard->iMajorVersionNeeded == g_iMajorVersion && pVisitCard->iMinorVersionNeeded > g_iMinorVersion)
				|| (pVisitCard->iMajorVersionNeeded == g_iMajorVersion && pVisitCard->iMinorVersionNeeded == g_iMinorVersion && pVisitCard->iMicroVersionNeeded > g_iMicroVersion))
			{
				cd_warning ("this module ('%s') needs at least Cairo-Dock v%d.%d.%d, but Cairo-Dock is in v%d.%d.%d (%s)\n  It will be ignored", cSoFilePath, pVisitCard->iMajorVersionNeeded, pVisitCard->iMinorVersionNeeded, pVisitCard->iMicroVersionNeeded, g_iMajorVersion, g_iMinorVersion, g_iMicroVersion, GLDI_VERSION);
				goto discard;
			}
			if (pVisitCard->cDockVersionOnCompilation != NULL && strcmp (pVisitCard->cDockVersionOnCompilation, GLDI_VERSION) != 0)  // separation des versions en easter egg.
			{
				cd_warning ("this module ('%s') was compiled with Cairo-Dock v%s, but Cairo-Dock is in v%s\n  It will be ignored", cSoFilePath, pVisitCard->cDockVersionOnCompilation, GLDI_VERSION);
				goto discard;
			}
		}
	}
	
	// create a new module with these info
	GldiModule *pModule = gldi_module_new (pVisitCard, pInterface);  // takes ownership of pVisitCard and pInterface
	if (pModule)
		pModule->handle = handle;
	return;
	
discard:
	///g_module_close (pModule);
	dlclose (handle);
	g_free (pVisitCard); // toutes les chaines sont statiques.
	g_free (pInterface);
}

/* Load modules from cModuleDirPath which must be non-NULL */
static void _gldi_modules_new_from_directory2 (const gchar *cModuleDirPath, GError **erreur)
{
	cd_message ("%s (%s)", __func__, cModuleDirPath);
	GError *tmp_erreur = NULL;
	GDir *dir = g_dir_open (cModuleDirPath, 0, &tmp_erreur);
	if (tmp_erreur != NULL)
	{
		g_propagate_error (erreur, tmp_erreur);
		return ;
	}

	const gchar *cFileName;
	GString *sFilePath = g_string_new ("");
	do
	{
		cFileName = g_dir_read_name (dir);
		if (cFileName == NULL)
			break ;
		
		if (g_str_has_suffix (cFileName, ".so"))
		{
			g_string_printf (sFilePath, "%s/%s", cModuleDirPath, cFileName);
			_module_new_from_so_file (sFilePath->str);
		}
	}
	while (1);
	g_string_free (sFilePath, TRUE);
	g_dir_close (dir);
}

/* Load modules from cModuleDirPath or from the default paths if it is NULL */
void gldi_modules_new_from_directory (const gchar *cModuleDirPath, GError **erreur)
{
	if (cModuleDirPath == NULL)
	{
		_gldi_modules_new_from_directory2 (GLDI_MODULES_DIR, erreur);
#ifdef GLDI_MODULES_DIR_CORE
		// only if plugins are installed in a separate prefix
		_gldi_modules_new_from_directory2 (GLDI_MODULES_DIR_CORE, erreur);
#endif
	}
	else _gldi_modules_new_from_directory2 (cModuleDirPath, erreur);
}

gchar *gldi_module_get_config_dir (GldiModule *pModule)
{
	GldiVisitCard *pVisitCard = pModule->pVisitCard;
	if (pVisitCard->cConfFileName == NULL)
		return NULL;
	
	gchar *cUserDataDirPath = g_strdup_printf ("%s/plug-ins/%s", g_cCurrentThemePath, pVisitCard->cUserDataDir);
	if (! g_file_test (cUserDataDirPath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
	{
		cd_message ("directory %s doesn't exist, it will be added.", cUserDataDirPath);
		
		gchar *command = g_strdup_printf ("mkdir -p \"%s\"", cUserDataDirPath);
		int r = system (command);
		g_free (command);
		
		if (r != 0)
		{
			cd_warning ("couldn't create a directory for applet '%s' in '%s/plug-ins'\n check writing permissions", pVisitCard->cModuleName, g_cCurrentThemePath);
			g_free (cUserDataDirPath);
			g_free (pModule->cConfFilePath);
			pModule->cConfFilePath = NULL;
			return NULL;
		}
	}
	
	return cUserDataDirPath;
}


  /////////////////////////
 /// MODULES HIGH LEVEL///
/////////////////////////

static void _gldi_module_load_config_and_activate (GldiModule *module, gboolean bActivate)
{
	g_return_if_fail (module != NULL && module->pVisitCard != NULL);
	cd_debug ("%s (%s)", __func__, module->pVisitCard->cModuleName);
	
	if (module->pInstancesList != NULL)
	{
		cd_warning ("Module %s already active", module->pVisitCard->cModuleName);
		return ;
	}
	
	if (module->pVisitCard->cConfFileName != NULL)  // the module has a conf file -> create an instance for each of them.
	{
		// check that the module's config dir exists or create it.
		gchar *cUserDataDirPath = gldi_module_get_config_dir (module);
		if (cUserDataDirPath == NULL)
		{
			cd_warning ("Unable to open the config folder of module %s\nCheck permissions", module->pVisitCard->cModuleName);
			return;
		}
		
		// look for conf files inside this folder, and create an instance for each of them.
		int n = 0;
		if (module->pVisitCard->bMultiInstance)  // possibly several conf files.
		{
			// open it
			GError *tmp_erreur = NULL;
			GDir *dir = g_dir_open (cUserDataDirPath, 0, &tmp_erreur);
			if (tmp_erreur != NULL)
			{
				cd_warning ("couldn't open folder %s (%s)", cUserDataDirPath, tmp_erreur->message);
				g_error_free (tmp_erreur);
				g_free (cUserDataDirPath);
				return ;
			}
			
			// for each conf file inside, instanciate the module with it.
			const gchar *cFileName;
			gchar *cInstanceFilePath;
			
			while ((cFileName = g_dir_read_name (dir)) != NULL)
			{
				gchar *str = strstr (cFileName, ".conf");
				if (!str)
					continue;
				if (*(str+5) != '-' && *(str+5) != '\0')  // xxx.conf or xxx.conf-i
					continue;
				cInstanceFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, cFileName);
				gldi_module_instance_new_full (module, cInstanceFilePath, bActivate);  // takes ownership of 'cInstanceFilePath'.
				n ++;
			}
			g_dir_close (dir);
		}
		else  // only 1 conf file possible.
		{
			gchar *cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, module->pVisitCard->cConfFileName);
			if (g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
			{
				gldi_module_instance_new_full (module, cConfFilePath, bActivate);
				n = 1;
			}
			else
			{
				g_free (cConfFilePath);
			}
		}
		
		// if no conf file was present, copy the default one and instanciate the module with it.
		if (n == 0)  // no conf file was present.
		{
			gchar *cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, module->pVisitCard->cConfFileName);
			gboolean r = cairo_dock_copy_file (module->cConfFilePath, cConfFilePath);
			if (! r)  // the copy failed.
			{
				cd_warning ("couldn't copy %s into %s; check permissions and file's existence", module->cConfFilePath, cUserDataDirPath);
				g_free (cConfFilePath);
				g_free (cUserDataDirPath);
				return;
			}
			else
			{
				gldi_module_instance_new_full (module, cConfFilePath, bActivate);
			}
		}
		
		g_free (cUserDataDirPath);
	}
	else  // the module has no conf file, just instanciate it once.
	{
		gldi_module_instance_new_full (module, NULL, bActivate);
	}
}

void gldi_module_activate (GldiModule *module)
{
	_gldi_module_load_config_and_activate (module, TRUE);
}

void gldi_module_deactivate (GldiModule *module)  // stop all instances of a module
{
	g_return_if_fail (module != NULL);
	cd_debug ("%s (%s, %s)", __func__, module->pVisitCard->cModuleName, module->cConfFilePath);
	GList *pInstances = module->pInstancesList;
	module->pInstancesList = NULL;  // set to NULL already so that notifications don't get fooled. This can probably be avoided...
	g_list_free_full (pInstances, (GDestroyNotify)gldi_object_unref);
	gldi_object_notify (module, NOTIFICATION_MODULE_ACTIVATED, module->pVisitCard->cModuleName, FALSE);  // throw it since the list was NULL when the instances were destroyed
	gldi_modules_write_active ();  // same
}


void gldi_modules_load_auto_config (void)
{
	GldiModule *pModule;
	GList *m;
	for (m = s_AutoLoadedModules; m != NULL; m = m->next)
	{
		pModule = m->data;
		if (pModule->pInstancesList == NULL)  // not yet active
		{
			_gldi_module_load_config_and_activate (pModule, FALSE);
		}
	}
}

void gldi_modules_activate_from_list (gchar **cActiveModuleList)
{
	//\_______________ On active les modules auto-charges en premier.
	gchar *cModuleName;
	GldiModule *pModule;
	GList *m;
	for (m = s_AutoLoadedModules; m != NULL; m = m->next)
	{
		pModule = m->data;
		if (pModule->pInstancesList == NULL)  // not yet active
		{
			gldi_module_activate (pModule);
		}
		else
		{
			// note: auto-loaded modules only have a single instance
			GldiModuleInstance *pInstance = pModule->pInstancesList->data;
			if (pInstance && !pInstance->uActive.bIsActive)
			{
				if (pModule->pInterface->initModule)
					pModule->pInterface->initModule (pInstance, NULL);
				pInstance->uActive.bIsActive = TRUE;
			}
		}
	}
	
	if (cActiveModuleList == NULL)
		return ;
	
	//\_______________ On active tous les autres.
	int i;
	for (i = 0; cActiveModuleList[i] != NULL; i ++)
	{
		cModuleName = cActiveModuleList[i];
		pModule = g_hash_table_lookup (s_hModuleTable, cModuleName);
		if (pModule == NULL)
		{
			cd_debug ("No such module (%s)", cModuleName);
			continue ;
		}
		
		if (pModule->pInstancesList == NULL)  // not yet active
		{
			gldi_module_activate (pModule);
		}
	}
	
	// don't write down
	if (s_iSidWriteModules != 0)
	{
		g_source_remove (s_iSidWriteModules);
		s_iSidWriteModules = 0;
	}
}

static void _deactivate_one_module (G_GNUC_UNUSED gchar *cModuleName, GldiModule *pModule, G_GNUC_UNUSED gpointer data)
{
	if (! gldi_module_is_auto_loaded (pModule))
		gldi_module_deactivate (pModule);
}
void gldi_modules_deactivate_all (void)
{
	// first deactivate applets
	g_hash_table_foreach (s_hModuleTable, (GHFunc)_deactivate_one_module, NULL);
	
	// then deactivate auto-loaded modules (that have been loaded first)
	GldiModule *pModule;
	GList *m;
	for (m = s_AutoLoadedModules; m != NULL; m = m->next)
	{
		pModule = m->data;
		gldi_module_deactivate (pModule);
	}
	
	// don't write down
	if (s_iSidWriteModules != 0)
	{
		g_source_remove (s_iSidWriteModules);
		s_iSidWriteModules = 0;
	}
}


gchar *gldi_module_add_conf_file (GldiModule *pModule)
{
	gchar *cUserDataDirPath = gldi_module_get_config_dir (pModule);
	if (cUserDataDirPath == NULL)
		return NULL;
	
	// find a name that doesn't exist yet in the config dir.
	gchar *cConfFilePath;
	int i = 0;
	do
	{
		if (i == 0)
			cConfFilePath = g_strdup_printf ("%s/%s", cUserDataDirPath, pModule->pVisitCard->cConfFileName);
		else
			cConfFilePath = g_strdup_printf ("%s/%s-%d", cUserDataDirPath, pModule->pVisitCard->cConfFileName, i);
		if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
			break;
		g_free (cConfFilePath);
		i ++;
	} while (1);
	
	// copy one of the instances conf file, or the default one.
	GldiModuleInstance *pFirstInstance = NULL;
	if (pModule->pInstancesList != NULL)
	{
		GList *last = g_list_last (pModule->pInstancesList);
		pFirstInstance = last->data;  // instances are prepended.
		
		cairo_dock_add_conf_file (pFirstInstance->cConfFilePath, cConfFilePath);
		
		if (pFirstInstance->pDesklet)  // prevent desklets from overlapping.
		{
			int iX2, iX = pFirstInstance->pContainer->iWindowPositionX;
			int iWidth = pFirstInstance->pContainer->iWidth;
			if (iX + iWidth/2 <= gldi_desktop_get_width()/2)  // desklet on the left, we place the new one on its right.
				iX2 = iX + iWidth;
			else  // desklet on the right, we place the new one on its left.
				iX2 = iX - iWidth;
			
			int iRelativePositionX = (iX2 + iWidth/2 <= gldi_desktop_get_width()/2 ? iX2 : iX2 - gldi_desktop_get_width());
			cairo_dock_update_conf_file (cConfFilePath,
				G_TYPE_INT, "Desklet", "x position", iRelativePositionX,
				G_TYPE_BOOLEAN, "Desklet", "locked", FALSE,  // we'll probably want to move it
				G_TYPE_BOOLEAN, "Desklet", "no input", FALSE,
				G_TYPE_INVALID);
		}
	}
	else  // no instance yet, just copy the default conf file.
	{
		cairo_dock_add_conf_file (pModule->cConfFilePath, cConfFilePath);
	}
	
	g_free (cUserDataDirPath);
	
	return cConfFilePath;
}

void gldi_module_add_instance (GldiModule *pModule)
{
	// check that the module is already active
	if (pModule->pInstancesList == NULL)
	{
		cd_warning ("This module has not been instantiated yet");
		return ;
	}
	
	// check that the module can be multi-instantiated
	if (! pModule->pVisitCard->bMultiInstance)
	{
		cd_warning ("This module can't be instantiated more than once");
		return ;
	}
	
	// add a conf file
	gchar *cInstanceFilePath = gldi_module_add_conf_file (pModule);
	
	// create an instance for it
	gldi_module_instance_new (pModule, cInstanceFilePath);  // takes ownership of 'cInstanceFilePath'.
}


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, GldiModulesParam *pModules)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	gsize length=0;
	pModules->cActiveModuleList = cairo_dock_get_string_list_key_value (pKeyFile, "System", "modules", &bFlushConfFileNeeded, &length, NULL, "Applets", "modules_0");
	
	return bFlushConfFileNeeded;
}

  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (GldiModulesParam *pModules)
{
	g_free (pModules->cActiveModuleList);
}

  ////////////
 /// INIT ///
////////////

static void init (void)
{
	s_hModuleTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		NULL,  // module name (points directly on the 'cModuleName' field of the module).
		NULL);  // module
}

  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	GldiModule *pModule = (GldiModule*)obj;
	GldiModuleAttr *mattr = (GldiModuleAttr*)attr;
	
	// check everything is ok
	g_return_if_fail (mattr != NULL && mattr->pVisitCard != NULL && mattr->pVisitCard->cModuleName);
	
	if (g_hash_table_lookup (s_hModuleTable, mattr->pVisitCard->cModuleName) != NULL)
	{
		cd_warning ("a module with the name '%s' is already registered", mattr->pVisitCard->cModuleName);
		return;
	}
	
	// set params
	pModule->pVisitCard = mattr->pVisitCard;
	mattr->pVisitCard = NULL;
	pModule->pInterface = mattr->pInterface;
	mattr->pInterface = NULL;
	if (pModule->cConfFilePath == NULL && pModule->pVisitCard->cConfFileName)
		pModule->cConfFilePath = g_strdup_printf ("%s/%s", pModule->pVisitCard->cShareDataDir, pModule->pVisitCard->cConfFileName);
	
	// register the module
	g_hash_table_insert (s_hModuleTable, (gpointer)pModule->pVisitCard->cModuleName, pModule);
	
	if (gldi_module_is_auto_loaded (pModule))  // a module that doesn't have an init/stop entry point, or that extends a manager; we'll activate it automatically (and before the others).
		s_AutoLoadedModules = g_list_prepend (s_AutoLoadedModules, pModule);
	
	// notify everybody
	gldi_object_notify (&myModuleObjectMgr, NOTIFICATION_MODULE_REGISTERED, pModule->pVisitCard->cModuleName, TRUE);
}

static void reset_object (GldiObject *obj)
{
	GldiModule *pModule = (GldiModule*)obj;
	if (pModule->pVisitCard == NULL)  // we didn't register it, pass
		return;
	
	// deactivate the module, if it was active
	gldi_module_deactivate (pModule);
	
	// unregister the module
	g_hash_table_remove (s_hModuleTable, pModule->pVisitCard->cModuleName);
	
	// notify everybody
	gldi_object_notify (&myModuleObjectMgr, NOTIFICATION_MODULE_REGISTERED, pModule->pVisitCard->cModuleName, FALSE);
	
	// free data
	if (pModule->handle)
		dlclose (pModule->handle);
	g_free (pModule->pInterface);
	g_free (pModule->pVisitCard); // toutes les chaines sont statiques.
}

static GKeyFile* reload_object (GldiObject *obj, gboolean bReloadConf, G_GNUC_UNUSED GKeyFile *pKeyFile)
{
	GldiModule *pModule = (GldiModule*)obj;
	GList *pElement;
	GldiModuleInstance *pInstance;
	for (pElement = pModule->pInstancesList; pElement != NULL; pElement = pElement->next)
	{
		pInstance = pElement->data;
		gldi_object_reload (GLDI_OBJECT(pInstance), bReloadConf);
	}
	return NULL;
}

void gldi_register_modules_manager (void)
{
	// Manager
	memset (&myModulesMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myModulesMgr), &myManagerObjectMgr, NULL);
	myModulesMgr.cModuleName   = "Modules";
	// interface
	myModulesMgr.init          = init;
	myModulesMgr.load          = NULL;
	myModulesMgr.unload        = NULL;
	myModulesMgr.reload        = (GldiManagerReloadFunc)NULL;
	myModulesMgr.get_config    = (GldiManagerGetConfigFunc)get_config;
	myModulesMgr.reset_config  = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myModulesParam, 0, sizeof (GldiModulesParam));
	myModulesMgr.pConfig = (GldiManagerConfigPtr)&myModulesParam;
	myModulesMgr.iSizeOfConfig = sizeof (GldiModulesParam);
	// data
	myModulesMgr.pData = (GldiManagerDataPtr)NULL;
	myModulesMgr.iSizeOfData = 0;
	
	// Object Manager
	memset (&myModuleObjectMgr, 0, sizeof (GldiObjectManager));
	myModuleObjectMgr.cName         = "Module";
	myModuleObjectMgr.iObjectSize   = sizeof (GldiModule);
	// interface
	myModuleObjectMgr.init_object   = init_object;
	myModuleObjectMgr.reset_object  = reset_object;
	myModuleObjectMgr.reload_object = reload_object;
	// signals
	gldi_object_install_notifications (GLDI_OBJECT(&myModuleObjectMgr), NB_NOTIFICATIONS_MODULES);
}
