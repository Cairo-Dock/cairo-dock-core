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
#include "cairo-dock-module-factory.h"  // for gldi_extend_manager
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-manager.h"

extern CairoDock *g_pPrimaryContainer;

static GList *s_pManagers = NULL;


static inline void _gldi_init_manager (GldiManager *pManager)
{
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


void gldi_reload_manager_from_keyfile (GldiManager *pManager, GKeyFile *pKeyFile)
{
	gpointer *pPrevConfig = NULL;
	// get new config
	if (pManager->iSizeOfConfig != 0 && pManager->pConfig != NULL && pManager->get_config != NULL)
	{
		pPrevConfig = g_memdup (pManager->pConfig, pManager->iSizeOfConfig);
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


void gldi_reload_manager (GldiManager *pManager, const gchar *cConfFilePath)  // expose pour Dbus.
{
	g_return_if_fail (pManager != NULL);
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	if (pKeyFile == NULL)
		return;
	
	gldi_reload_manager_from_keyfile (pManager, pKeyFile);
	
	g_key_file_free (pKeyFile);
}
 

static gboolean gldi_get_manager_config (GldiManager *pManager, GKeyFile *pKeyFile)
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


void gldi_extend_manager (CairoDockVisitCard *pVisitCard, const gchar *cManagerName)
{
	GldiManager *pManager = gldi_get_manager (cManagerName);
	g_return_if_fail (pManager != NULL && pVisitCard->cInternalModule == NULL);
	pManager->pExternalModules = g_list_prepend (pManager->pExternalModules, (gpointer)pVisitCard->cModuleName);
	pVisitCard->cInternalModule = cManagerName;
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_manager (GldiManager *pManager)
{
	s_pManagers = g_list_prepend (s_pManagers, pManager);
}


GldiManager *gldi_get_manager (const gchar *cName)
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


void gldi_init_managers (void)
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


gboolean gldi_get_managers_config_from_key_file (GKeyFile *pKeyFile)
{
	gboolean bFlushConfFileNeeded = FALSE;
	GldiManager *pManager;
	GList *m;
	for (m = s_pManagers; m != NULL; m = m->next)
	{
		pManager = m->data;
		bFlushConfFileNeeded |= gldi_get_manager_config (pManager, pKeyFile);
	}
	return bFlushConfFileNeeded;
}


void gldi_get_managers_config (const gchar *cConfFilePath, const gchar *cVersion)
{
	//\___________________ On ouvre le fichier de conf.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	//\___________________ On recupere la conf de tous les managers.
	gboolean bFlushConfFileNeeded = gldi_get_managers_config_from_key_file (pKeyFile);
	
	//\___________________ On met a jour le fichier sur le disque si necessaire.
	if (! bFlushConfFileNeeded && cVersion != NULL)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, cVersion);
	if (bFlushConfFileNeeded)
	{
		///cairo_dock_flush_conf_file (pKeyFile, cConfFilePath, GLDI_SHARE_DATA_DIR, CAIRO_DOCK_CONF_FILE);
		cairo_dock_upgrade_conf_file (cConfFilePath, pKeyFile, GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_CONF_FILE);
	}
	
	g_key_file_free (pKeyFile);
}


void gldi_load_managers (void)
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


void gldi_unload_managers (void)
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
