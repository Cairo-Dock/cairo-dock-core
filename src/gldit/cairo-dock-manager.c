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

#include "gldi-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-module-manager.h"  // GldiVisitCard (for gldi_extend_manager)
#include "cairo-dock-keyfile-utilities.h"
#define __MANAGER_DEF__
#include "cairo-dock-manager.h"

// public (manager, config, data)
GldiObjectManager myManagerObjectMgr;

// dependencies
extern GldiContainer *g_pPrimaryContainer;
extern gchar *g_cConfFile;

// private
static GList *s_pManagers = NULL;


static void _gldi_init_manager (GldiManager *pManager)
{
	// be sure to init once only
	if (pManager->bInitIsDone)
		return;
	pManager->bInitIsDone = TRUE;
	
	// if the manager depends on another, init this one first.
	if (pManager->pDependence != NULL)
		_gldi_init_manager (pManager->pDependence);
	
	// init the manager
	if (pManager->init)
		pManager->init ();
}

static inline void _gldi_load_manager (GldiManager *pManager)
{
	if (pManager->load)
		pManager->load ();
}

static inline void _gldi_unload_manager (GldiManager *pManager)
{
	if (pManager->unload)
		pManager->unload ();
	
	if (pManager->iSizeOfConfig != 0 && pManager->pConfig != NULL && pManager->reset_config)
	{
		pManager->reset_config (pManager->pConfig);
		memset (pManager->pConfig, 0, pManager->iSizeOfConfig);
	}
}


static void _gldi_manager_reload_from_keyfile (GldiManager *pManager, GKeyFile *pKeyFile)
{
	gpointer *pPrevConfig = NULL;
	// get new config
	if (pManager->iSizeOfConfig != 0 && pManager->pConfig != NULL && pManager->get_config != NULL)
	{
		pPrevConfig = g_memdup2 (pManager->pConfig, pManager->iSizeOfConfig);
		memset (pManager->pConfig, 0, pManager->iSizeOfConfig);
		
		pManager->get_config (pKeyFile, pManager->pConfig);
	}
	
	// reload
	if (pManager->reload && g_pPrimaryContainer != NULL)  // in maintenance mode, no need to reload.
		pManager->reload (pPrevConfig, pManager->pConfig);
	
	// free old config
	if (pManager->reset_config)
		pManager->reset_config (pPrevConfig);
	g_free (pPrevConfig);
}


static gboolean gldi_manager_get_config (GldiManager *pManager, GKeyFile *pKeyFile)
{
	if (! pManager->get_config || ! pManager->pConfig || pManager->iSizeOfConfig == 0)
		return FALSE;
	if (pManager->reset_config)
	{
		pManager->reset_config (pManager->pConfig);
	}
	memset (pManager->pConfig, 0, pManager->iSizeOfConfig);
	return pManager->get_config (pKeyFile, pManager->pConfig);
}


void gldi_manager_extend (GldiVisitCard *pVisitCard, const gchar *cManagerName)
{
	GldiManager *pManager = gldi_manager_get (cManagerName);
	g_return_if_fail (pManager != NULL && pVisitCard->cInternalModule == NULL);
	pManager->pExternalModules = g_list_prepend (pManager->pExternalModules, (gpointer)pVisitCard->cModuleName);
	pVisitCard->cInternalModule = cManagerName;
}


  ///////////////
 /// MANAGER ///
///////////////

GldiManager *gldi_manager_get (const gchar *cName)
{
	GldiManager *pManager = NULL;
	GList *m;
	for (m = s_pManagers; m != NULL; m = m->next)
	{
		pManager = m->data;
		if (strcmp (cName, pManager->cModuleName) == 0)
			return pManager;
	}
	return NULL;
}


void gldi_managers_init (void)
{
	cd_message ("%s()", __func__);
	GldiManager *pManager;
	GList *m;
	for (m = s_pManagers; m != NULL; m = m->next)
	{
		pManager = m->data;
		_gldi_init_manager (pManager);
	}
}


gboolean gldi_managers_get_config_from_key_file (GKeyFile *pKeyFile)
{
	gboolean bFlushConfFileNeeded = FALSE;
	GldiManager *pManager;
	GList *m;
	for (m = s_pManagers; m != NULL; m = m->next)
	{
		pManager = m->data;
		bFlushConfFileNeeded |= gldi_manager_get_config (pManager, pKeyFile);
	}
	return bFlushConfFileNeeded;
}


void gldi_managers_get_config (const gchar *cConfFilePath, const gchar *cVersion)
{
	//\___________________ On ouvre le fichier de conf.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	//\___________________ On recupere la conf de tous les managers.
	gboolean bFlushConfFileNeeded = gldi_managers_get_config_from_key_file (pKeyFile);
	
	//\___________________ On met a jour le fichier sur le disque si necessaire.
	if (! bFlushConfFileNeeded && cVersion != NULL)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, cVersion);
	if (bFlushConfFileNeeded)
	{
		cairo_dock_upgrade_conf_file (cConfFilePath, pKeyFile, GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_CONF_FILE);
	}
	
	g_key_file_free (pKeyFile);
}


void gldi_managers_load (void)
{
	cd_message ("%s()", __func__);
	GldiManager *pManager;
	GList *m;
	for (m = s_pManagers; m != NULL; m = m->next)
	{
		pManager = m->data;
		_gldi_load_manager (pManager);
	}
}


void gldi_managers_unload (void)
{
	cd_message ("%s()", __func__);
	GldiManager *pManager;
	GList *m;
	for (m = s_pManagers; m != NULL; m = m->next)
	{
		pManager = m->data;
		_gldi_unload_manager (pManager);
	}
}


void gldi_managers_foreach (GFunc callback, gpointer data)
{
	g_list_foreach (s_pManagers, callback, data);
}


  //////////////
 /// OBJECT ///
//////////////

static void init_object (GldiObject *obj, G_GNUC_UNUSED gpointer attr)
{
	GldiManager *pManager = (GldiManager*)obj;
	
	s_pManagers = g_list_prepend (s_pManagers, pManager);  // we don't init the manager, since we want to do that after all managers have been created
}

static GKeyFile* reload_object (GldiObject *obj, gboolean bReloadConf, GKeyFile *pKeyFile)
{
	GldiManager *pManager = (GldiManager*)obj;
	cd_message ("reload %s (%d)", pManager->cModuleName, bReloadConf);
	if (bReloadConf && !pKeyFile)
	{
		pKeyFile = cairo_dock_open_key_file (g_cConfFile);
		g_return_val_if_fail (pKeyFile != NULL, NULL);
	}
	
	_gldi_manager_reload_from_keyfile (pManager, pKeyFile);
	
	return pKeyFile;
}

static gboolean delete_object (G_GNUC_UNUSED GldiObject *obj)
{
	return FALSE;  // don't allow to delete a manager
}

void gldi_register_managers_manager (void)
{
	// Object Manager
	memset (&myManagerObjectMgr, 0, sizeof (GldiObjectManager));
	myManagerObjectMgr.cName         = "Manager";
	myManagerObjectMgr.iObjectSize   = sizeof (GldiManager);
	// interface
	myManagerObjectMgr.init_object   = init_object;
	myManagerObjectMgr.reload_object = reload_object;
	myManagerObjectMgr.delete_object = delete_object;
	// signals
	gldi_object_install_notifications (GLDI_OBJECT (&myManagerObjectMgr), NB_NOTIFICATIONS_MANAGER);
}
