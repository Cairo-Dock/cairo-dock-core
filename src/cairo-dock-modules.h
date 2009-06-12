
#ifndef __CAIRO_DOCK_MODULES__
#define  __CAIRO_DOCK_MODULES__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-desklet.h"
G_BEGIN_DECLS


/**
*@file cairo-dock-modules.h This class defines and handles the external and internal modules of Cairo-Dock.
* A module has an interface and a visit card :
*  - the visit card allows it to define itself (name, category, default icon, etc)
*  - the interface defines the entry points for init, stop, reload, read config, and reset datas.
* Modules can be instanciated several times; each time they are, an instance is created. This instance will hold all the data used by the module's functions : the icon and its container, the config structure and its conf file, the data structure and a slot to plug datas into containers and icons. All these parameters are optionnal; a module that has an icon is also called an applet.
* Internal modules are just simplified version of modules, and are used internally by Cairo-Dock. As a special feature, a module can bind itself to an internal module, if its purpose is to complete it.
*/

typedef enum {
	CAIRO_DOCK_CATEGORY_SYSTEM,
	CAIRO_DOCK_CATEGORY_THEME,
	CAIRO_DOCK_CATEGORY_APPLET_ACCESSORY,
	CAIRO_DOCK_CATEGORY_APPLET_DESKTOP,
	CAIRO_DOCK_CATEGORY_APPLET_CONTROLER,
	CAIRO_DOCK_CATEGORY_PLUG_IN,
	CAIRO_DOCK_NB_CATEGORY
	} CairoDockPluginCategory;
#define CAIRO_DOCK_CATEGORY_DESKTOP CAIRO_DOCK_CATEGORY_APPLET_DESKTOP
#define CAIRO_DOCK_CATEGORY_ACCESSORY CAIRO_DOCK_CATEGORY_APPLET_ACCESSORY
#define CAIRO_DOCK_CATEGORY_CONTROLER CAIRO_DOCK_CATEGORY_APPLET_CONTROLER

struct _CairoDockVisitCard {
	/// nom du module qui servira a l'identifier.
	gchar *cModuleName;
	/// numero de version majeure de cairo-dock necessaire au bon fonctionnement du module.
	short iMajorVersionNeeded;
	/// numero de version mineure de cairo-dock necessaire au bon fonctionnement du module.
	short iMinorVersionNeeded;
	/// numero de version micro de cairo-dock necessaire au bon fonctionnement du module.
	short iMicroVersionNeeded;
	/// chemin d'une image de previsualisation.
	gchar *cPreviewFilePath;
	/// Nom du domaine pour la traduction du module par 'gettext'.
	gchar *cGettextDomain;
	/// Version du dock pour laquelle a ete compilee le module.
	gchar *cDockVersionOnCompilation;
	/// version courante du module.
	gchar *cModuleVersion;
	/// repertoire du plug-in cote utilisateur.
	gchar *cUserDataDir;
	/// repertoire d'installation du plug-in.
	gchar *cShareDataDir;
	/// nom de son fichier de conf.
	gchar *cConfFileName;
	/// categorie de l'applet.
	CairoDockPluginCategory iCategory;
	/// chemin d'une image pour l'icone du module dans le panneau de conf du dock.
	gchar *cIconFilePath;
	/// taille de la structure contenant la config du module.
	gint iSizeOfConfig;
	/// taille de la structure contenant les donnees du module.
	gint iSizeOfData;
	/// VRAI ssi le plug-in peut etre instancie plusiers fois.
	gboolean bMultiInstance;
	/// description et mode d'emploi succint.
	gchar *cDescription;
	/// auteur/pseudo
	gchar *cAuthor;
	/// nom d'un module interne auquel ce module se rattache, ou NULL si aucun.
	const gchar *cInternalModule;
	/// octets reserves pour preserver la compatibilite binaire lors de futurs ajouts sur l'interface entre plug-ins et dock.
	char reserved[8];
};

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

struct _CairoDockModuleInstance {
	CairoDockModule *pModule;
	gchar *cConfFilePath;
	gboolean bCanDetach;
	Icon *pIcon;
	CairoContainer *pContainer;
	CairoDock *pDock;
	CairoDesklet *pDesklet;
	cairo_t *pDrawContext;
	gint iSlotID;
	/**gpointer *myConfig;
	gpointer *myData;*/
};

/// Construit et renvoie la carte de visite du module et son interface.
typedef gboolean (* CairoDockModulePreInit) (CairoDockVisitCard *pVisitCard, CairoDockModuleInterface *pInterface);

struct _CairoDockModule {
	/// chemin du .so
	gchar *cSoFilePath;
	/// structure du module, contenant les pointeurs vers les fonctions du module.
	GModule *pModule;
	/// fonctions d'interface du module.
	CairoDockModuleInterface *pInterface;
	/// carte de visite du module.
	CairoDockVisitCard *pVisitCard;
	/// chemin du fichier de conf du module.
	gchar *cConfFilePath;
	/// TRUE si le module est actif (c'est-a-dire utilise).
	///gboolean bActive;
	/// VRAI ssi l'appet est prevue pour pouvoir se detacher.
	gboolean bCanDetach;
	/// le container dans lequel va se charger le module, ou NULL.
	///CairoContainer *pContainer;
	/// Heure de derniere (re)activation du module.
	gdouble fLastLoadingTime;
	/// Liste d'instance du plug-in.
	GList *pInstancesList;
};

struct _CairoDockMinimalAppletConfig {
	gint iDesiredIconWidth;
	gint iDesiredIconHeight;
	gchar *cLabel;
	gchar *cIconFileName;
	gdouble fOrder;
	gchar *cDockName;
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
	gchar *cModuleName;
	gchar *cDescription;
	gchar *cIcon;
	gchar *cTitle;
	CairoDockPluginCategory iCategory;
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


void cairo_dock_initialize_module_manager (gchar *cModuleDirPath);

/**
*Verifie que le fichier de conf d'un plug-in est bien present dans le repertoire utilisateur du plug-in, sinon le copie a partir du fichier de conf fournit lors de l'installation. Cree au besoin le repertoire utilisateur du plug-in.
*@param pVisitCard la carte de visite du module.
*@return Le chemin du fichier de conf en espace utilisateur, ou NULL si le fichier n'a pu etre ni trouve, ni cree.
*/
gchar *cairo_dock_check_module_conf_file (CairoDockVisitCard *pVisitCard);

void cairo_dock_free_visit_card (CairoDockVisitCard *pVisitCard);

CairoDockModule * cairo_dock_load_module (gchar *cSoFilePath, GHashTable *pModuleTable, GError **erreur);

void cairo_dock_preload_module_from_directory (gchar *cModuleDirPath, GHashTable *pModuleTable, GError **erreur);



void cairo_dock_activate_modules_from_list (gchar **cActiveModuleList, double fTime);

void cairo_dock_deactivate_old_modules (double fTime);


void cairo_dock_free_module (CairoDockModule *module);

GKeyFile *cairo_dock_pre_read_module_instance_config (CairoDockModuleInstance *pInstance, CairoDockMinimalAppletConfig *pMinimalConfig);

void cairo_dock_free_minimal_config (CairoDockMinimalAppletConfig *pMinimalConfig);

void cairo_dock_activate_module (CairoDockModule *module, GError **erreur);

void cairo_dock_deactivate_module (CairoDockModule *module);

void cairo_dock_reload_module_instance (CairoDockModuleInstance *pInstance, gboolean bReloadAppletConf);
void cairo_dock_reload_module (CairoDockModule *module, gboolean bReloadAppletConf);


void cairo_dock_deactivate_all_modules (void);

void cairo_dock_activate_module_and_load (gchar *cModuleName);
void cairo_dock_deactivate_module_instance_and_unload (CairoDockModuleInstance *pInstance);
void cairo_dock_deactivate_module_and_unload (const gchar *cModuleName);

void cairo_dock_configure_module_instance (GtkWindow *pParentWindow, CairoDockModuleInstance *pModuleInstance, GError **erreur);
void cairo_dock_configure_inactive_module (GtkWindow *pParentWindow, CairoDockModule *pModule);
void cairo_dock_configure_module (GtkWindow *pParentWindow, const gchar *cModuleName);


CairoDockModule *cairo_dock_find_module_from_name (const gchar *cModuleName);

CairoDockModuleInstance *cairo_dock_foreach_desklet (CairoDockForeachDeskletFunc pCallback, gpointer user_data);
CairoDockModule *cairo_dock_foreach_module (GHRFunc pCallback, gpointer user_data);
CairoDockModule *cairo_dock_foreach_module_in_alphabetical_order (GCompareFunc pCallback, gpointer user_data);


gchar *cairo_dock_list_active_modules (void);
void cairo_dock_update_conf_file_with_active_modules (void);


int cairo_dock_get_nb_modules (void);

void cairo_dock_update_module_instance_order (CairoDockModuleInstance *pModuleInstance, double fOrder);


CairoDockModuleInstance *cairo_dock_instanciate_module (CairoDockModule *pModule, gchar *cConfFilePah);
void cairo_dock_free_module_instance (CairoDockModuleInstance *pInstance);
void cairo_dock_stop_module_instance (CairoDockModuleInstance *pInstance);
void cairo_dock_deinstanciate_module (CairoDockModuleInstance *pInstance);

void cairo_dock_remove_module_instance (CairoDockModuleInstance *pInstance);
void cairo_dock_add_module_instance (CairoDockModule *pModule);

void cairo_dock_read_module_config (GKeyFile *pKeyFile, CairoDockModuleInstance *pInstance);


gboolean cairo_dock_reserve_data_slot (CairoDockModuleInstance *pInstance);
void cairo_dock_release_data_slot (CairoDockModuleInstance *pInstance);

#define cairo_dock_get_icon_data(pIcon, pInstance) ((pIcon)->pDataSlot[pInstance->iSlotID])
#define cairo_dock_get_container_data(pContainer, pInstance) ((pContainer)->pDataSlot[pInstance->iSlotID])

#define cairo_dock_set_icon_data(pIcon, pInstance, pData) \
	(pIcon)->pDataSlot[pInstance->iSlotID] = pData
#define cairo_dock_set_container_data(pContainer, pInstance, pData) \
	(pContainer)->pDataSlot[pInstance->iSlotID] = pData


void cairo_dock_preload_internal_modules (GHashTable *pModuleTable);

void cairo_dock_reload_internal_module (CairoDockInternalModule *pModule, const gchar *cConfFilePath);

CairoDockInternalModule *cairo_dock_find_internal_module_from_name (const gchar *cModuleName);

gboolean cairo_dock_get_internal_module_config (CairoDockInternalModule *pModule, GKeyFile *pKeyFile);

gboolean cairo_dock_get_global_config (GKeyFile *pKeyFile);


void cairo_dock_popup_module_instance_description (CairoDockModuleInstance *pModuleInstance);


void cairo_dock_attach_to_another_module (CairoDockVisitCard *pVisitCard, const gchar *cOtherModuleName);

#define cairo_dock_module_is_auto_loaded(pModule) (pModule->pInterface->initModule == NULL || pModule->pInterface->stopModule == NULL || pModule->pVisitCard->cInternalModule != NULL)

G_END_DECLS
#endif
