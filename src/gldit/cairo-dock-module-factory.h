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

#ifndef __CAIRO_DOCK_MODULE_FACTORY__
#define  __CAIRO_DOCK_MODULE_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-module-manager.h"
G_BEGIN_DECLS

/**
* @file cairo-dock-module-factory.h This class defines the external modules of Cairo-Dock.
*
* A module has an interface and a visit card :
*  - the visit card allows it to define itself (name, category, default icon, etc)
*  - the interface defines the entry points for init, stop, reload, read config, and reset datas.
*
* Modules can be instanciated several times; each time they are, an instance is created.
* Each instance holds all a set of the data : the icon and its container, the config structure and its conf file, the data structure and a slot to plug datas into containers and icons. All these parameters are optionnal; a module that has an icon is also called an applet.
*/

/// Categories a module can be in.
typedef enum {
	CAIRO_DOCK_CATEGORY_BEHAVIOR=0,
	CAIRO_DOCK_CATEGORY_THEME,
	CAIRO_DOCK_CATEGORY_APPLET_FILES,
	CAIRO_DOCK_CATEGORY_APPLET_INTERNET,
	CAIRO_DOCK_CATEGORY_APPLET_DESKTOP,
	CAIRO_DOCK_CATEGORY_APPLET_ACCESSORY,
	CAIRO_DOCK_CATEGORY_APPLET_SYSTEM,
	CAIRO_DOCK_CATEGORY_APPLET_FUN,
	CAIRO_DOCK_NB_CATEGORY
	} CairoDockModuleCategory;

typedef enum {
	CAIRO_DOCK_MODULE_IS_PLUGIN 	= 0,
	CAIRO_DOCK_MODULE_CAN_DOCK 		= 1<<0,
	CAIRO_DOCK_MODULE_CAN_DESKLET 	= 1<<1,
	CAIRO_DOCK_MODULE_CAN_OTHERS 	= 1<<2
	} CairoDockModuleContainerType;
	

/// Definition of the visit card of a module. Contains everything that is statically defined for a module.
struct _CairoDockVisitCard {
	// nom du module qui servira a l'identifier.
	const gchar *cModuleName;
	// numero de version majeure de cairo-dock necessaire au bon fonctionnement du module.
	gint iMajorVersionNeeded;
	// numero de version mineure de cairo-dock necessaire au bon fonctionnement du module.
	gint iMinorVersionNeeded;
	// numero de version micro de cairo-dock necessaire au bon fonctionnement du module.
	gint iMicroVersionNeeded;
	// chemin d'une image de previsualisation.
	const gchar *cPreviewFilePath;
	// Nom du domaine pour la traduction du module par 'gettext'.
	const gchar *cGettextDomain;
	// Version du dock pour laquelle a ete compilee le module.
	const gchar *cDockVersionOnCompilation;
	// version courante du module.
	const gchar *cModuleVersion;
	// repertoire du plug-in cote utilisateur.
	const gchar *cUserDataDir;
	// repertoire d'installation du plug-in.
	const gchar *cShareDataDir;
	// nom de son fichier de conf.
	const gchar *cConfFileName;
	// categorie de l'applet.
	CairoDockModuleCategory iCategory;
	// chemin d'une image pour l'icone du module dans le panneau de conf du dock.
	const gchar *cIconFilePath;
	// taille de la structure contenant la config du module.
	gint iSizeOfConfig;
	// taille de la structure contenant les donnees du module.
	gint iSizeOfData;
	// VRAI ssi le plug-in peut etre instancie plusiers fois.
	gboolean bMultiInstance;
	// description et mode d'emploi succint.
	const gchar *cDescription;
	// auteur/pseudo
	const gchar *cAuthor;
	// nom d'un module interne auquel ce module se rattache, ou NULL si aucun.
	const gchar *cInternalModule;
	// nom du module tel qu'affiche a l'utilisateur.
	const gchar *cTitle;
	CairoDockModuleContainerType iContainerType;
	gboolean bStaticDeskletSize;
	// whether to display the applet's name on the icon's label if it's NULL or not.
	gboolean bAllowEmptyTitle;
	// octets reserves pour preserver la compatibilite binaire lors de futurs ajouts sur l'interface entre plug-ins et dock.
	char reserved[8];
};

/// Definition of the interface of a module.
struct _CairoDockModuleInterface {
	void		(* initModule)			(CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
	void		(* stopModule)			(CairoDockModuleInstance *pInstance);
	gboolean	(* reloadModule)		(CairoDockModuleInstance *pInstance, CairoContainer *pOldContainer, GKeyFile *pKeyFile);
	gboolean	(* read_conf_file)		(CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
	void		(* reset_config)		(CairoDockModuleInstance *pInstance);
	void		(* reset_data)			(CairoDockModuleInstance *pInstance);
	void		(* load_custom_widget)	(CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
	void		(* save_custom_widget)	(CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
};

/// Definition of an instance of a module. A module can be instanciated several times.
struct _CairoDockModuleInstance {
	/// the module this instance represents.
	CairoDockModule *pModule;
	/// conf file of the instance.
	gchar *cConfFilePath;
	/// TRUE if the instance can be detached from docks (desklet mode).
	gboolean bCanDetach;
	/// the icon holding the instance.
	Icon *pIcon;
	/// container of the icon.
	CairoContainer *pContainer;
	/// this field repeats the 'pContainer' field if the container is a dock, and is NULL otherwise.
	CairoDock *pDock;
	/// this field repeats the 'pContainer' field if the container is a desklet, and is NULL otherwise.
	CairoDesklet *pDesklet;
	/// a drawing context on the icon.
	cairo_t *pDrawContext;
	/// a unique ID to insert external data on icons and containers.
	gint iSlotID;
	/// pointer to a structure containing the config parameters of the applet.
	gpointer *pConfig;
	/// pointer to a structure containing the data of the applet.
	gpointer *pData;
};

/// Pre-init function of a module. Fills the visit card and the interface of a module.
typedef gboolean (* CairoDockModulePreInit) (CairoDockVisitCard *pVisitCard, CairoDockModuleInterface *pInterface);

/// Definition of an external module.
struct _CairoDockModule {
	/// path to the .so file.
	gchar *cSoFilePath;
	/// internal structure of the .so file, once it has been opened.
	//GModule *pModule;
	gpointer handle;
	/// interface of the module.
	CairoDockModuleInterface *pInterface;
	/// visit card of the module.
	CairoDockVisitCard *pVisitCard;
	/// conf file of the module.
	gchar *cConfFilePath;
	/// TRUE if the appet can be detached from a dock (desklet mode).
	gboolean bCanDetach;
	/// last time the module was (re)activated.
	gdouble fLastLoadingTime;
	/// List of instances of the module.
	GList *pInstancesList;
};

struct _CairoDockMinimalAppletConfig {
	gint iDesiredIconWidth;
	gint iDesiredIconHeight;
	gchar *cLabel;
	gchar *cIconFileName;
	gdouble fOrder;
	gchar *cDockName;
	gboolean bAlwaysVisible;
	CairoDeskletAttribute deskletAttribute;
	gboolean bIsDetached;
};


typedef gpointer CairoInternalModuleConfigPtr;
typedef gpointer CairoInternalModuleDataPtr;
typedef void (* CairoDockInternalModuleReloadFunc) (CairoInternalModuleConfigPtr *pPrevConfig, CairoInternalModuleConfigPtr *pNewConfig);
typedef gboolean (* CairoDockInternalModuleGetConfigFunc) (GKeyFile *pKeyFile, CairoInternalModuleConfigPtr *pConfig);
typedef void (* CairoDockInternalModuleResetConfigFunc) (CairoInternalModuleConfigPtr *pConfig);
typedef void (* CairoDockInternalModuleResetDataFunc) (CairoInternalModuleDataPtr *pData);
struct _CairoDockInternalModule {
	//\_____________ Carte de visite.
	const gchar *cModuleName;
	const gchar *cDescription;
	const gchar *cIcon;
	const gchar *cTitle;
	CairoDockModuleCategory iCategory;
	gint iSizeOfConfig;
	gint iSizeOfData;
	const gchar **cDependencies;  // NULL terminated.
	//\_____________ Interface.
	CairoDockInternalModuleReloadFunc reload;
	CairoDockInternalModuleGetConfigFunc get_config;
	CairoDockInternalModuleResetConfigFunc reset_config;
	CairoDockInternalModuleResetDataFunc reset_data;
	//\_____________ Instance.
	CairoInternalModuleConfigPtr pConfig;
	CairoInternalModuleDataPtr pData;
	GList *pExternalModules;
};


  ///////////////////
 // MODULE LOADER //
///////////////////

#define cairo_dock_module_is_auto_loaded(pModule) (pModule->pInterface->initModule == NULL || pModule->pInterface->stopModule == NULL || pModule->pVisitCard->cInternalModule != NULL)

CairoDockModule *cairo_dock_new_module (const gchar *cSoFilePath, GError **erreur);

void cairo_dock_free_module (CairoDockModule *module);

/* Verifie que le fichier de conf d'un plug-in est bien present dans le repertoire utilisateur du plug-in, sinon le copie a partir du fichier de conf fournit lors de l'installation. Cree au besoin le repertoire utilisateur du plug-in.
*@param pVisitCard la carte de visite du module.
*@return Le chemin du fichier de conf en espace utilisateur, ou NULL si le fichier n'a pu etre ni trouve, ni cree.
*/
gchar *cairo_dock_check_module_conf_file (CairoDockVisitCard *pVisitCard);

gchar *cairo_dock_check_module_conf_dir (CairoDockModule *pModule);

void cairo_dock_free_visit_card (CairoDockVisitCard *pVisitCard);


  /////////////////////
 // MODULE INSTANCE //
/////////////////////

GKeyFile *cairo_dock_pre_read_module_instance_config (CairoDockModuleInstance *pInstance, CairoDockMinimalAppletConfig *pMinimalConfig);

/* Cree une nouvelle instance d'un module. Cree l'icone et le container associe, et les place ou il faut.
*/
CairoDockModuleInstance *cairo_dock_instanciate_module (CairoDockModule *pModule, gchar *cConfFilePah);

/** Stop and free a module instance. If it was an applet, the icon is not destroyed (but is no more a valid applet). If it was in a desklet, the desklet is destroyed.
*@param pInstance the instance to stop.
*/
void cairo_dock_deinstanciate_module (CairoDockModuleInstance *pInstance);

/** Reload an instance of a module.
*@param pInstance the instance to reload
*@param bReloadAppletConf TRUE to reload the config of the instance before reloading it.
*/
void cairo_dock_reload_module_instance (CairoDockModuleInstance *pInstance, gboolean bReloadAppletConf);


  /////////////
 // MODULES //
/////////////

void cairo_dock_free_minimal_config (CairoDockMinimalAppletConfig *pMinimalConfig);

/** Create and initialize all the instances of a module.
*@param module the module to activate.
*@param erreur error set if something bad happens.
*/
void cairo_dock_activate_module (CairoDockModule *module, GError **erreur);

/** Stop and destroy all the instances of a module.
*@param module the module to deactivate
*/
void cairo_dock_deactivate_module (CairoDockModule *module);

/** Reload all the instances of a module.
*@param module the module to reload
*@param bReloadAppletConf TRUE to reload the config of the instances before reloading them.
*/
void cairo_dock_reload_module (CairoDockModule *module, gboolean bReloadAppletConf);


void cairo_dock_popup_module_instance_description (CairoDockModuleInstance *pModuleInstance);


G_END_DECLS
#endif
