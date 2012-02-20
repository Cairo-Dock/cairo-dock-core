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

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <cairo.h>

#include "gldi-config.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_conf_file_needs_update
#include "cairo-dock-log.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-X-manager.h"  // g_desktopGeometry
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-config.h"
#include "cairo-dock-module-factory.h"
#define _MANAGER_DEF_
#include "cairo-dock-module-manager.h"

// public (manager, config, data)
CairoModulesParam myModulesParam;
CairoModulesManager myModulesMgr;

CairoDockModuleInstance *g_pCurrentModule = NULL;  // only used to trace a possible crash in one of the modules.

// dependancies
extern gchar *g_cConfFile;
extern CairoDockDesktopGeometry g_desktopGeometry;

// private
static GHashTable *s_hModuleTable = NULL;
static GList *s_AutoLoadedModules = NULL;
static guint s_iSidWriteModules = 0;


  ///////////////
 /// MANAGER ///
///////////////

CairoDockModule *cairo_dock_find_module_from_name (const gchar *cModuleName)
{
	//g_print ("%s (%s)\n", __func__, cModuleName);
	g_return_val_if_fail (cModuleName != NULL, NULL);
	return g_hash_table_lookup (s_hModuleTable, cModuleName);
}

CairoDockModule *cairo_dock_foreach_module (GHRFunc pCallback, gpointer user_data)
{
	return g_hash_table_find (s_hModuleTable, (GHRFunc) pCallback, user_data);
}

static int _sort_module_by_alphabetical_order (CairoDockModule *m1, CairoDockModule *m2)
{
	if (!m1 || !m1->pVisitCard || !m1->pVisitCard->cTitle)
		return 1;
	if (!m2 || !m2->pVisitCard || !m2->pVisitCard->cTitle)
		return -1;
	return g_ascii_strncasecmp (m1->pVisitCard->cTitle, m2->pVisitCard->cTitle, -1);
}
CairoDockModule *cairo_dock_foreach_module_in_alphabetical_order (GCompareFunc pCallback, gpointer user_data)
{
	GList *pModuleList = g_hash_table_get_values (s_hModuleTable);
	pModuleList = g_list_sort (pModuleList, (GCompareFunc) _sort_module_by_alphabetical_order);
	
	CairoDockModule *pModule = (CairoDockModule *)g_list_find_custom (pModuleList, user_data, pCallback);
	
	g_list_free (pModuleList);
	return pModule;
}

int cairo_dock_get_nb_modules (void)
{
	return g_hash_table_size (s_hModuleTable);
}

static void _cairo_dock_write_one_module_name (const gchar *cModuleName, CairoDockModule *pModule, GString *pString)
{
	if (pModule->pInstancesList != NULL && ! cairo_dock_module_is_auto_loaded (pModule))
	{
		g_string_append_printf (pString, "%s;", cModuleName);
	}
}
gchar *cairo_dock_list_active_modules (void)
{
	GString *pString = g_string_new ("");
	
	g_hash_table_foreach (s_hModuleTable, (GHFunc) _cairo_dock_write_one_module_name, pString);
	
	if (pString->len > 0)
		pString->str[pString->len-1] = '\0';
	
	gchar *cModuleNames = pString->str;
	g_string_free (pString, FALSE);
	return cModuleNames;
}


  /////////////////////
 /// MODULE LOADER ///
/////////////////////

gboolean cairo_dock_register_module (CairoDockModule *pModule)
{
	g_return_val_if_fail (pModule->pVisitCard != NULL && pModule->pVisitCard->cModuleName != NULL, FALSE);
	
	if (g_hash_table_lookup (s_hModuleTable, pModule->pVisitCard->cModuleName) != NULL)
	{
		cd_warning ("a module with the name '%s' is already registered", pModule->pVisitCard->cModuleName);
		return FALSE;
	}
	
	if (pModule->pVisitCard->cDockVersionOnCompilation == NULL)
		pModule->pVisitCard->cDockVersionOnCompilation = GLDI_VERSION;
	
	g_hash_table_insert (s_hModuleTable, (gpointer)pModule->pVisitCard->cModuleName, pModule);
	
	if (cairo_dock_module_is_auto_loaded (pModule))  // c'est un module qui soit ne peut etre active ou desactive, soit etend un manager; on l'activera donc automatiquement (et avant les autres modules).
		s_AutoLoadedModules = g_list_prepend (s_AutoLoadedModules, pModule);
	
	cairo_dock_notify_on_object (&myModulesMgr, NOTIFICATION_MODULE_REGISTERED, pModule->pVisitCard->cModuleName, TRUE);
	return TRUE;
}

void cairo_dock_unregister_module (const gchar *cModuleName)
{
	g_return_if_fail (cModuleName != NULL);
	gchar *name = g_strdup (cModuleName);
	g_hash_table_remove (s_hModuleTable, cModuleName);
	cairo_dock_notify_on_object (&myModulesMgr, NOTIFICATION_MODULE_REGISTERED, name, FALSE);
	g_free (name);
}

CairoDockModule * cairo_dock_load_module (const gchar *cSoFilePath)  // cSoFilePath vers un fichier de la forme 'libtruc.so'. Le module est rajoute dans la table des modules.
{
	//g_print ("%s (%s)\n", __func__, cSoFilePath);
	g_return_val_if_fail (cSoFilePath != NULL, NULL);
	
	GError *erreur = NULL;
	CairoDockModule *pModule = cairo_dock_new_module (cSoFilePath, &erreur);
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		return NULL;
	}
	if (pModule == NULL)
		return NULL;
	g_return_val_if_fail (pModule->pVisitCard != NULL, NULL);
	
	g_hash_table_insert (s_hModuleTable, (gpointer)pModule->pVisitCard->cModuleName, pModule);
	
	if (cairo_dock_module_is_auto_loaded (pModule))  // c'est un module qui soit ne peut etre activer et/ou desactiver, soit etend un manager; on l'active donc automatiquement.
	{
		s_AutoLoadedModules = g_list_prepend (s_AutoLoadedModules, pModule);
	}
		
	cairo_dock_notify_on_object (&myModulesMgr, NOTIFICATION_MODULE_REGISTERED, pModule->pVisitCard->cModuleName, TRUE);
	return pModule;
}

void cairo_dock_load_modules_in_directory (const gchar *cModuleDirPath, GError **erreur)
{
	if (!g_module_supported ())
		return;
	if (cModuleDirPath == NULL)
		cModuleDirPath = GLDI_MODULES_DIR;
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
			cairo_dock_load_module (sFilePath->str);
		}
	}
	while (1);
	g_string_free (sFilePath, TRUE);
	g_dir_close (dir);
}

  ///////////////
 /// MODULES ///
///////////////

void cairo_dock_activate_modules_from_list (gchar **cActiveModuleList)
{
	//\_______________ On active les modules auto-charges en premier.
	GError *erreur = NULL;
	gchar *cModuleName;
	CairoDockModule *pModule;
	GList *m;
	for (m = s_AutoLoadedModules; m != NULL; m = m->next)
	{
		pModule = m->data;
		if (pModule->pInstancesList == NULL)
		{
			cairo_dock_activate_module (pModule, &erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
		}
	}
	
	if (cActiveModuleList == NULL)
		return ;
	
	//\_______________ On active tous les autres.
	int i;
	GList *pNotFoundModules = NULL;
	for (i = 0; cActiveModuleList[i] != NULL; i ++)
	{
		cModuleName = cActiveModuleList[i];
		pModule = g_hash_table_lookup (s_hModuleTable, cModuleName);
		if (pModule == NULL)
		{
			cd_debug ("No such module (%s)", cModuleName);
			pNotFoundModules = g_list_prepend (pNotFoundModules, cModuleName);
			continue ;
		}
		
		if (pModule->pInstancesList == NULL)
		{
			cairo_dock_activate_module (pModule, &erreur);
			if (erreur != NULL)
			{
				cd_warning (erreur->message);
				g_error_free (erreur);
				erreur = NULL;
			}
		}
	}
}

static void _cairo_dock_deactivate_one_module (gchar *cModuleName, CairoDockModule *pModule, gpointer data)
{
	if (! cairo_dock_module_is_auto_loaded (pModule))
		cairo_dock_deactivate_module (pModule);
}
void cairo_dock_deactivate_all_modules (void)
{
	// first deactivate applets
	g_hash_table_foreach (s_hModuleTable, (GHFunc) _cairo_dock_deactivate_one_module, NULL);
	if (s_iSidWriteModules != 0)
	{
		g_source_remove (s_iSidWriteModules);
		s_iSidWriteModules = 0;
	}
	
	// then deactivate auto-loaded modules (that have been loaded first)
	CairoDockModule *pModule;
	GList *m;
	for (m = s_AutoLoadedModules; m != NULL; m = m->next)
	{
		pModule = m->data;
		cairo_dock_deactivate_module (pModule);
	}
}


  /////////////////////////
 /// MODULES HIGH LEVEL///
/////////////////////////

void cairo_dock_activate_module_and_load (const gchar *cModuleName)
{
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	g_return_if_fail (pModule != NULL);
	
	///pModule->fLastLoadingTime = 0;
	if (pModule->pInstancesList == NULL)
	{
		GError *erreur = NULL;
		cairo_dock_activate_module (pModule, &erreur);
		if (erreur != NULL)
		{
			cd_warning (erreur->message);
			g_error_free (erreur);
		}
	}
	else
	{
		cairo_dock_reload_module (pModule, FALSE);
	}
	
	GList *pElement;
	CairoDockModuleInstance *pInstance;
	for (pElement = pModule->pInstancesList; pElement != NULL; pElement = pElement->next)
	{
		pInstance = pElement->data;
		if (pInstance->pDock)
		{
			cairo_dock_trigger_update_dock_size (pInstance->pDock);
			///gtk_widget_queue_draw (pInstance->pDock->container.pWidget);
		}
	}
	
	cairo_dock_write_active_modules ();
}

// deinstanciate_module, remove icon, free_icon, write
static void cairo_dock_deactivate_module_instance_and_unload (CairoDockModuleInstance *pInstance)
{
	g_return_if_fail (pInstance != NULL);
	cd_message ("%s (%s)", __func__, pInstance->cConfFilePath);
	
	Icon *pIcon = pInstance->pIcon;  // l'instance va etre detruite.
	CairoDock *pDock = pInstance->pDock;
	if (pDock)
	{
		cairo_dock_remove_icon_from_dock (pDock, pInstance->pIcon);  // desinstancie le module et tout.
		cairo_dock_update_dock_size (pDock);
		gtk_widget_queue_draw (pDock->container.pWidget);
	}
	else
	{
		cairo_dock_deinstanciate_module (pInstance);
		if (pIcon)
			pIcon->pModuleInstance = NULL;
	}
	cairo_dock_free_icon (pIcon);
}

void cairo_dock_deactivate_module_and_unload (const gchar *cModuleName)
{
	CairoDockModule *pModule = cairo_dock_find_module_from_name (cModuleName);
	g_return_if_fail (pModule != NULL);
	
	GList *pElement = pModule->pInstancesList, *pNextElement;
	CairoDockModuleInstance *pInstance;
	cd_debug ("%d instance(s) a arreter", g_list_length (pModule->pInstancesList));
	while (pElement != NULL)
	{
		pInstance = pElement->data;
		pNextElement = pElement->next;
		cairo_dock_deactivate_module_instance_and_unload (pInstance);
		pElement = pNextElement;
	}
	
	cairo_dock_write_active_modules ();
}


/*
* Stoppe une instance d'un module, et la supprime.
*/
void cairo_dock_remove_module_instance (CairoDockModuleInstance *pInstance)
{
	cd_message ("%s (%s)", __func__, pInstance->cConfFilePath);
	g_return_if_fail (pInstance->pModule->pInstancesList != NULL);
	//\_________________ Si c'est la derniere instance, on desactive le module.
	if (pInstance->pModule->pInstancesList->next == NULL)
	{
		cairo_dock_deactivate_module_and_unload (pInstance->pModule->pVisitCard->cModuleName);
		return ;
	}
	
	//\_________________ On efface le fichier de conf de cette instance.
	cd_debug ("on efface %s", pInstance->cConfFilePath);
	g_remove (pInstance->cConfFilePath);
	
	//\_________________ On supprime cette instance (on le fait maintenant, pour que son fichier de conf n'existe plus lors du 'stop'.
	gchar *cConfFilePath = pInstance->cConfFilePath;
	pInstance->cConfFilePath = NULL;
	CairoDockModule *pModule = pInstance->pModule;
	cairo_dock_deactivate_module_instance_and_unload (pInstance);  // pInstance n'est plus.
}

gchar *cairo_dock_add_module_conf_file (CairoDockModule *pModule)
{
	gchar *cUserDataDirPath = cairo_dock_check_module_conf_dir (pModule);
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
	CairoDockModuleInstance *pFirstInstance = NULL;
	if (pModule->pInstancesList != NULL)
	{
		GList *last = g_list_last (pModule->pInstancesList);
		pFirstInstance = last->data;  // instances are prepended.
		
		cairo_dock_copy_file (pFirstInstance->cConfFilePath, cConfFilePath);
		/**gchar *cCommand = g_strdup_printf ("cp \"%s\" \"%s\"", pFirstInstance->cConfFilePath, cConfFilePath);
		int r = system (cCommand);
		g_free (cCommand);*/
		
		if (pFirstInstance->pDesklet)  // prevent desklets from overlapping.
		{
			int iX2, iX = pFirstInstance->pContainer->iWindowPositionX;
			int iWidth = pFirstInstance->pContainer->iWidth;
			if (iX + iWidth/2 <= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]/2)  // desklet on the left, we place the new one on its right.
				iX2 = iX + iWidth;
			else  // desklet on the right, we place the new one on its left.
				iX2 = iX - iWidth;
			
			int iRelativePositionX = (iX2 + iWidth/2 <= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]/2 ? iX2 : iX2 - g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]);
			cairo_dock_update_conf_file (cConfFilePath,
				G_TYPE_INT, "Desklet", "x position", iRelativePositionX,
				G_TYPE_BOOLEAN, "Desklet", "locked", FALSE,  // we'll probably want to move it
				G_TYPE_BOOLEAN, "Desklet", "no input", FALSE,
				G_TYPE_INVALID);
		}
	}
	else  // no instance yet, just copy the default conf file.
	{
		cairo_dock_copy_file (pModule->cConfFilePath, cConfFilePath);
		/**gchar *cCommand = g_strdup_printf ("cp \"%s\" \"%s\"", pModule->cConfFilePath, cConfFilePath);
		int r = system (cCommand);
		g_free (cCommand);*/
	}
	
	g_free (cUserDataDirPath);
	
	return cConfFilePath;
}

void cairo_dock_add_module_instance (CairoDockModule *pModule)
{
	if (pModule->pInstancesList == NULL)
	{
		cd_warning ("This module has not been instanciated yet");
		return ;
	}
	
	gchar *cInstanceFilePath = cairo_dock_add_module_conf_file (pModule);
	
	CairoDockModuleInstance *pNewInstance = cairo_dock_instanciate_module (pModule, cInstanceFilePath);  // prend le 'cInstanceFilePath'.
	
	if (pNewInstance != NULL && pNewInstance->pDock)
	{
		cairo_dock_update_dock_size (pNewInstance->pDock);
	}
}

void cairo_dock_detach_module_instance (CairoDockModuleInstance *pInstance)
{
	//g_return_if_fail (pInstance->pDesklet == NULL);
	//\__________________ On recupere l'etat actuel.
	gboolean bIsDetached = (pInstance->pDesklet != NULL);
	if ((bIsDetached && pInstance->pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DOCK) ||
		(!bIsDetached && pInstance->pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DESKLET))
	{
		//\__________________ On enregistre l'etat 'detache'.
		cairo_dock_update_conf_file (pInstance->cConfFilePath,
			G_TYPE_BOOLEAN, "Desklet", "initially detached", !bIsDetached,
			G_TYPE_INT, "Desklet", "accessibility", CAIRO_DESKLET_NORMAL,
			G_TYPE_INVALID);
		//\__________________ On detache l'applet.
		cairo_dock_reload_module_instance (pInstance, TRUE);
		if (pInstance->pDesklet)  // on a bien detache l'applet.
			cairo_dock_zoom_out_desklet (pInstance->pDesklet);
		//\__________________ On met a jour le panneau de conf s'il etait ouvert sur cette applet.
		cairo_dock_notify_on_object (&myModulesMgr, NOTIFICATION_MODULE_INSTANCE_DETACHED, pInstance, !bIsDetached);
	}
}

void cairo_dock_detach_module_instance_at_position (CairoDockModuleInstance *pInstance, int iCenterX, int iCenterY)
{
	g_return_if_fail (pInstance->pDesklet == NULL);
	//\__________________ On enregistre les nouvelles coordonnees qu'aura le desklet, ainsi que l'etat 'detache'.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (pInstance->cConfFilePath);
	g_return_if_fail (pKeyFile != NULL);
	
	//\__________________ compute coordinates of the center of the desklet.
	int iDeskletWidth = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "width", NULL, 92, NULL, NULL);
	int iDeskletHeight = cairo_dock_get_integer_key_value (pKeyFile, "Desklet", "height", NULL, 92, NULL, NULL);
	
	int iDeskletPositionX = iCenterX - iDeskletWidth/2;
	int iDeskletPositionY = iCenterY - iDeskletHeight/2;
	
	int iRelativePositionX = (iDeskletPositionX + iDeskletWidth/2 <= g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]/2 ? iDeskletPositionX : iDeskletPositionX - g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]);
	int iRelativePositionY = (iDeskletPositionY + iDeskletHeight/2 <= g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]/2 ? iDeskletPositionY : iDeskletPositionY - g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
	
	//\__________________ update the conf file of the applet.
	g_key_file_set_double (pKeyFile, "Desklet", "x position", iDeskletPositionX);
	g_key_file_set_double (pKeyFile, "Desklet", "y position", iDeskletPositionY);
	g_key_file_set_boolean (pKeyFile, "Desklet", "initially detached", TRUE);
	g_key_file_set_double (pKeyFile, "Desklet", "locked", FALSE);  // we usually will want to adjust the position of the new desklet.
	g_key_file_set_double (pKeyFile, "Desklet", "no input", FALSE);  // we usually will want to adjust the position of the new desklet.
	g_key_file_set_double (pKeyFile, "Desklet", "accessibility", CAIRO_DESKLET_NORMAL);  // prevent "unforseen consequences".
	
	cairo_dock_write_keys_to_file (pKeyFile, pInstance->cConfFilePath);
	g_key_file_free (pKeyFile);
	
	//\__________________ On met a jour le panneau de conf s'il etait ouvert sur cette applet.
	cairo_dock_notify_on_object (&myModulesMgr, NOTIFICATION_MODULE_INSTANCE_DETACHED, pInstance, TRUE);  // inutile de notifier du changement de taille, le configure-event du desklet s'en chargera.
	
	//\__________________ On detache l'applet.
	cairo_dock_reload_module_instance (pInstance, TRUE);
	if (pInstance->pDesklet)  // on a bien detache l'applet.
		cairo_dock_zoom_out_desklet (pInstance->pDesklet);
}


static int s_iNbUsedSlots = 0;
static CairoDockModuleInstance *s_pUsedSlots[CAIRO_DOCK_NB_DATA_SLOT+1];
gboolean cairo_dock_reserve_data_slot (CairoDockModuleInstance *pInstance)
{
	g_return_val_if_fail (s_iNbUsedSlots < CAIRO_DOCK_NB_DATA_SLOT, FALSE);
	if (s_iNbUsedSlots == 0)
		memset (s_pUsedSlots, 0, (CAIRO_DOCK_NB_DATA_SLOT+1) * sizeof (CairoDockModuleInstance*));
	
	if (pInstance->iSlotID == 0)
	{
		s_iNbUsedSlots ++;
		if (s_pUsedSlots[s_iNbUsedSlots] == NULL)
		{
			pInstance->iSlotID = s_iNbUsedSlots;
			s_pUsedSlots[s_iNbUsedSlots] = pInstance;
		}
		else
		{
			int i;
			for (i = 1; i < s_iNbUsedSlots; i ++)
			{
				if (s_pUsedSlots[i] == NULL)
				{
					pInstance->iSlotID = i;
					s_pUsedSlots[i] = pInstance;
					break ;
				}
			}
		}
	}
	return TRUE;
}

void cairo_dock_release_data_slot (CairoDockModuleInstance *pInstance)
{
	if (pInstance->iSlotID == 0)
		return;
	s_iNbUsedSlots --;
	s_pUsedSlots[pInstance->iSlotID] = NULL;
	pInstance->iSlotID = 0;
}

static gboolean _write_modules (gpointer data)
{
	gchar *cModuleNames = cairo_dock_list_active_modules ();
	
	cairo_dock_update_conf_file (g_cConfFile,
		G_TYPE_STRING, "System", "modules", cModuleNames,
		G_TYPE_INVALID);
	g_free (cModuleNames);
}
static gboolean _write_modules_idle (gpointer data)
{
	_write_modules (data);
	s_iSidWriteModules = 0;
	return FALSE;
}
void cairo_dock_write_active_modules (void)
{
	if (s_iSidWriteModules == 0)
		s_iSidWriteModules = g_idle_add (_write_modules_idle, NULL);
}


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoModulesParam *pModules)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	gsize length=0;
	pModules->cActiveModuleList = cairo_dock_get_string_list_key_value (pKeyFile, "System", "modules", &bFlushConfFileNeeded, &length, NULL, "Applets", "modules_0");
	
	return bFlushConfFileNeeded;
}


  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (CairoModulesParam *pModules)
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
		NULL,  // la cle est le nom du module, et pointe directement sur le champ 'cModuleName' du module.
		(GDestroyNotify) cairo_dock_free_module);
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_modules_manager (void)
{
	// Manager
	memset (&myModulesMgr, 0, sizeof (CairoModulesManager));
	myModulesMgr.mgr.cModuleName 	= "Modules";
	myModulesMgr.mgr.init 			= init;
	myModulesMgr.mgr.load 			= NULL;
	myModulesMgr.mgr.unload 		= NULL;
	myModulesMgr.mgr.reload 		= (GldiManagerReloadFunc)NULL;
	myModulesMgr.mgr.get_config 	= (GldiManagerGetConfigFunc)get_config;
	myModulesMgr.mgr.reset_config 	= (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myModulesParam, 0, sizeof (CairoModulesParam));
	myModulesMgr.mgr.pConfig = (GldiManagerConfigPtr)&myModulesParam;
	myModulesMgr.mgr.iSizeOfConfig = sizeof (CairoModulesParam);
	// data
	myModulesMgr.mgr.pData = (GldiManagerDataPtr)NULL;
	myModulesMgr.mgr.iSizeOfData = 0;
	// signals
	cairo_dock_install_notifications_on_object (&myModulesMgr, NB_NOTIFICATIONS_MODULES);
	// register
	gldi_register_manager (GLDI_MANAGER(&myModulesMgr));
}
