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

#ifndef __GLDI_MODULES_MANAGER__
#define  __GLDI_MODULES_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-desklet-factory.h"  // CairoDeskletAttribute
#include "cairo-dock-manager.h"
G_BEGIN_DECLS

/**
* @file cairo-dock-module-manager.h This class manages the external modules of Cairo-Dock.
*
* A module has an interface and a visit card :
*  - the visit card allows it to define itself (name, category, default icon, etc)
*  - the interface defines the entry points for init, stop, reload, read config, and reset data.
*
* Modules can be instanciated several times; each time they are, an instance \ref _GldiModuleInstance is created.
* Each instance holds a set of data: the icon and its container, the config structure and its conf file, the data structure and a slot to plug datas into containers and icons. All these data are optionnal; a module that has an icon is also called an applet.
*/

/**
 * Define the current ABI version. Used by the new plugin loader interface
 * to check compatibility. This version should be incremented if the layout
 * or size of public structures changes, function parameters change, or
 * a macro is converted to a function or vice versa.
 * It is not required the change this when adding a function to the
 * public API (loading the module will fail if it refers to an
 * unresolved symbol anyway). */
#define GLDI_ABI_VERSION 20240921

// manager
typedef struct _GldiModulesParam GldiModulesParam;
typedef struct _GldiModuleAttr GldiModuleAttr;

#ifndef _MANAGER_DEF_
extern GldiModulesParam myModulesParam;
extern GldiManager myModulesMgr;
extern GldiObjectManager myModuleObjectMgr;
#endif


struct _GldiModuleAttr {
	GldiVisitCard *pVisitCard;
	GldiModuleInterface *pInterface;
};

// params
struct _GldiModulesParam {
	gchar **cActiveModuleList;
	};

// signals
typedef enum {
	NOTIFICATION_MODULE_REGISTERED = NB_NOTIFICATIONS_OBJECT,
	NOTIFICATION_MODULE_ACTIVATED,
	NOTIFICATION_LOGOUT,
	NB_NOTIFICATIONS_MODULES
	} GldiModuleNotifications;


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
	} GldiModuleCategory;

typedef enum {
	CAIRO_DOCK_MODULE_IS_PLUGIN 	= 0,
	CAIRO_DOCK_MODULE_CAN_DOCK 		= 1<<0,
	CAIRO_DOCK_MODULE_CAN_DESKLET 	= 1<<1,
	CAIRO_DOCK_MODULE_CAN_OTHERS 	= 1<<2
	} GldiModuleContainerType;

/// Definition of the visit card of a module. Contains everything that is statically defined for a module.
struct _GldiVisitCard {
	// nom du module qui servira a l'identifier.
	const gchar *cModuleName;
	// minimum major version needed for this module (if <= 3), or indicator of new API / ABI (if == 4)
	gint iMajorVersionNeeded;
	// minimum minor version needed for this module (if major version <= 3), or exact ABI version needed if major version == 4
	gint iMinorVersionNeeded;
	// numero de version micro de cairo-dock necessaire au bon fonctionnement du module (not used if major version == 4)
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
	GldiModuleCategory iCategory;
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
	GldiModuleContainerType iContainerType;
	gboolean bStaticDeskletSize;
	// whether to display the applet's name on the icon's label if it's NULL or not.
	gboolean bAllowEmptyTitle;
	// if TRUE and the applet inhibites a class, then appli icons will be placed after the applet icon.
	gboolean bActAsLauncher;
	// function called after the module has been successfully loaded; use this to register functionality that should always be active
	// only available if iMajorVersionNeeded == 4
	void (* postLoad) (GldiVisitCard *pVisitCard);
	gpointer reserved;
};

/// Definition of the interface of a module.
struct _GldiModuleInterface {
	void		(* initModule)			(GldiModuleInstance *pInstance, GKeyFile *pKeyFile);
	void		(* stopModule)			(GldiModuleInstance *pInstance);
	gboolean	(* reloadModule)		(GldiModuleInstance *pInstance, GldiContainer *pOldContainer, GKeyFile *pKeyFile);
	gboolean	(* read_conf_file)		(GldiModuleInstance *pInstance, GKeyFile *pKeyFile);
	void		(* reset_config)		(GldiModuleInstance *pInstance);
	void		(* reset_data)			(GldiModuleInstance *pInstance);
	void		(* load_custom_widget)	(GldiModuleInstance *pInstance, GKeyFile *pKeyFile, GSList *pWidgetList);
	void		(* save_custom_widget)	(GldiModuleInstance *pInstance, GKeyFile *pKeyFile, GSList *pWidgetList);
};

/// Pre-init function of a module. Fills the visit card and the interface of a module.
typedef gboolean (* GldiModulePreInit) (GldiVisitCard *pVisitCard, GldiModuleInterface *pInterface);

/// Definition of an external module.
struct _GldiModule {
	/// object
	GldiObject object;
	/// interface of the module.
	GldiModuleInterface *pInterface;
	/// visit card of the module.
	GldiVisitCard *pVisitCard;
	/// conf file of the module.
	gchar *cConfFilePath;
	/// if the module interface is provided by a dynamic library, handle to this library.
	gpointer handle;
	/// list of instances of the module.
	GList *pInstancesList;
	gpointer reserved[2];
};

struct _CairoDockMinimalAppletConfig {
	gint iDesiredIconWidth;
	gint iDesiredIconHeight;
	gchar *cLabel;
	gchar *cIconFileName;
	gdouble fOrder;
	gchar *cDockName;
	gboolean bAlwaysVisible;
	GldiColor *pHiddenBgColor;
	CairoDeskletAttr deskletAttribute;
	gboolean bIsDetached;
};

/** Say if an object is a Module.
*@param obj the object.
*@return TRUE if the object is a Module.
*/
#define GLDI_OBJECT_IS_MODULE(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myModuleObjectMgr)

  ///////////////////
 // MODULE LOADER //
///////////////////

#define gldi_module_is_auto_loaded(pModule) (pModule->pInterface->initModule == NULL || pModule->pInterface->stopModule == NULL || pModule->pVisitCard->cInternalModule != NULL)

/** Create a new module. The module takes ownership of the 2 arguments, unless an error occured.
* @param pVisitCard the visit card of the module
* @param pInterface the interface of the module
* @return the new module, or NULL if the visit card is invalid.
*/
GldiModule *gldi_module_new (GldiVisitCard *pVisitCard, GldiModuleInterface *pInterface);

/** Create a new module from a .so file.
* @param cSoFilePath path to the .so file
* @return the new module, or NULL if an error occured.
*/
GldiModule *gldi_module_new_from_so_file (const gchar *cSoFilePath);

/** Create new modules from all the .so files contained in the given folder.
* @param cModuleDirPath path to the folder
* @param erreur an error
* @return the new module, or NULL if an error occured.
*/
void gldi_modules_new_from_directory (const gchar *cModuleDirPath, GError **erreur);

/** Get the path to the folder containing the config files of a module (one file per instance). The folder is created if needed.
* If the module is not configurable, or if the folder couldn't be created, NULL is returned.
* @param pModule the module
* @return the path to the folder (free it after use).
*/
gchar *gldi_module_get_config_dir (GldiModule *pModule);

void cairo_dock_free_visit_card (GldiVisitCard *pVisitCard);


  /////////////
 // MANAGER //
/////////////

/** Get the module which has a given name.
*@param cModuleName the unique name of the module.
*@return the module, or NULL if not found.
*/
GldiModule *gldi_module_get (const gchar *cModuleName);

GldiModule *gldi_module_foreach (GHRFunc pCallback, gpointer user_data);

GldiModule *gldi_module_foreach_in_alphabetical_order (GCompareFunc pCallback, gpointer user_data);

int gldi_module_get_nb (void);
#define cairo_dock_get_nb_modules gldi_module_get_nb

void gldi_modules_write_active (void);


  ///////////////////////
 // MODULES HIGH LEVEL//
///////////////////////

/** Create and initialize all the instances of a module.
*@param module the module to activate.
*/
void gldi_module_activate (GldiModule *module);

/** Stop and destroy all the instances of a module.
*@param module the module to deactivate
*/
void gldi_module_deactivate (GldiModule *module);

void gldi_modules_activate_from_list (gchar **cActiveModuleList);

void gldi_modules_deactivate_all (void);

// cp file
gchar *gldi_module_add_conf_file (GldiModule *pModule);  /// should maybe be in the module-instance too...
// cp file + instanciate_module
void gldi_module_add_instance (GldiModule *pModule);


void gldi_register_modules_manager (void);

G_END_DECLS
#endif
