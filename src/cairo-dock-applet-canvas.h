/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */

#ifndef __CAIRO_DOCK_APPLET_CANVAS__
#define  __CAIRO_DOCK_APPLET_CANVAS__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


//\_________________________________ STRUCT
typedef struct _AppletConfig AppletConfig;
typedef struct _AppletData AppletData;

//\_________________________________ FUNCTIONS NAMES
#define CD_APPLET_PRE_INIT_FUNC pre_init
#define CD_APPLET_INIT_FUNC init
#define CD_APPLET_STOP_FUNC stop
#define CD_APPLET_RELOAD_FUNC reload

#define CD_APPLET_READ_CONFIG_FUNC read_conf_file
#define CD_APPLET_RESET_CONFIG_FUNC reset_config
#define CD_APPLET_RESET_DATA_FUNC reset_data

#define CD_APPLET_ON_CLICK_FUNC action_on_click
#define CD_APPLET_ON_BUILD_MENU_FUNC action_on_build_menu
#define CD_APPLET_ON_MIDDLE_CLICK_FUNC action_on_middle_click
#define CD_APPLET_ON_DROP_DATA_FUNC action_on_drop_data
#define CD_APPLET_ON_SCROLL_FUNC action_on_scroll
#define CD_APPLET_ON_UPDATE_ICON_FUNC action_on_update_icon

//\_________________________________ PROTO
#define CD_APPLET_PRE_INIT_PROTO \
gboolean CD_APPLET_PRE_INIT_FUNC (CairoDockVisitCard *pVisitCard, CairoDockModuleInterface *pInterface)
#define CD_APPLET_INIT_PROTO(pApplet) \
void CD_APPLET_INIT_FUNC (CairoDockModuleInstance *pApplet, GKeyFile *pKeyFile)
#define CD_APPLET_STOP_PROTO \
void CD_APPLET_STOP_FUNC (CairoDockModuleInstance *myApplet)
#define CD_APPLET_RELOAD_PROTO \
gboolean CD_APPLET_RELOAD_FUNC (CairoDockModuleInstance *myApplet, CairoContainer *pOldContainer, GKeyFile *pKeyFile)

#define CD_APPLET_READ_CONFIG_PROTO \
gboolean CD_APPLET_READ_CONFIG_FUNC (CairoDockModuleInstance *myApplet, GKeyFile *pKeyFile)
#define CD_APPLET_RESET_CONFIG_PROTO \
void CD_APPLET_RESET_CONFIG_FUNC (CairoDockModuleInstance *myApplet)
#define CD_APPLET_RESET_DATA_PROTO \
void CD_APPLET_RESET_DATA_FUNC (CairoDockModuleInstance *myApplet)

#define CD_APPLET_ON_CLICK_PROTO \
gboolean CD_APPLET_ON_CLICK_FUNC (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer, guint iButtonState)
#define CD_APPLET_ON_BUILD_MENU_PROTO \
gboolean CD_APPLET_ON_BUILD_MENU_FUNC (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer, GtkWidget *pAppletMenu)
#define CD_APPLET_ON_MIDDLE_CLICK_PROTO \
gboolean CD_APPLET_ON_MIDDLE_CLICK_FUNC (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer)
#define CD_APPLET_ON_DROP_DATA_PROTO \
gboolean CD_APPLET_ON_DROP_DATA_FUNC (CairoDockModuleInstance *myApplet, const gchar *cReceivedData, Icon *pClickedIcon, double fPosition, CairoContainer *pClickedContainer)
#define CD_APPLET_ON_SCROLL_PROTO \
gboolean CD_APPLET_ON_SCROLL_FUNC (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer, int iDirection)
#define CD_APPLET_ON_UPDATE_ICON_PROTO \
gboolean CD_APPLET_ON_UPDATE_ICON_FUNC (CairoDockModuleInstance *myApplet, Icon *pIcon, CairoContainer *pContainer, gboolean *bContinueAnimation)

//\_________________________________ HEADERS
#define CD_APPLET_H \
CD_APPLET_PRE_INIT_PROTO; \
CD_APPLET_INIT_PROTO (pApplet); \
CD_APPLET_STOP_PROTO; \
CD_APPLET_RELOAD_PROTO;

#define CD_APPLET_CONFIG_H \
CD_APPLET_READ_CONFIG_PROTO; \
CD_APPLET_RESET_CONFIG_PROTO; \
CD_APPLET_RESET_DATA_PROTO;

#define CD_APPLET_ON_CLICK_H \
CD_APPLET_ON_CLICK_PROTO;
#define CD_APPLET_ON_BUILD_MENU_H \
CD_APPLET_ON_BUILD_MENU_PROTO;
#define CD_APPLET_ON_MIDDLE_CLICK_H \
CD_APPLET_ON_MIDDLE_CLICK_PROTO;
#define CD_APPLET_ON_DROP_DATA_H \
CD_APPLET_ON_DROP_DATA_PROTO;
#define CD_APPLET_ON_SCROLL_H \
CD_APPLET_ON_SCROLL_PROTO;
#define CD_APPLET_ON_UPDATE_ICON_H \
CD_APPLET_ON_UPDATE_ICON_PROTO;

//\_________________________________ BODY
//\______________________ pre_init.
/** Debut de la fonction de pre-initialisation de l'applet (celle qui est appele a l'enregistrement de tous les plug-ins).
*Definit egalement les variables globales suivantes : myIcon, myDock, myDesklet, myContainer, et myDrawContext.
*@param cName nom de sous lequel l'applet sera enregistree par Cairo-Dock.
*@param iMajorVersion version majeure du dock necessaire au bon fonctionnement de l'applet.
*@param iMinorVersion version mineure du dock necessaire au bon fonctionnement de l'applet.
*@param iMicroVersion version micro du dock necessaire au bon fonctionnement de l'applet.
*@param iAppletCategory Categorie de l'applet (CAIRO_DOCK_CATEGORY_ACCESSORY, CAIRO_DOCK_CATEGORY_DESKTOP, CAIRO_DOCK_CATEGORY_CONTROLER)
*/
#define CD_APPLET_PRE_INIT_ALL_BEGIN(_cName, _iMajorVersion, _iMinorVersion, _iMicroVersion, _iAppletCategory, _cDescription, _cAuthor) \
CD_APPLET_PRE_INIT_PROTO \
{ \
	pVisitCard->cModuleName = g_strdup (_cName); \
	pVisitCard->iMajorVersionNeeded = _iMajorVersion; \
	pVisitCard->iMinorVersionNeeded = _iMinorVersion; \
	pVisitCard->iMicroVersionNeeded = _iMicroVersion; \
	pVisitCard->cPreviewFilePath = MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_PREVIEW_FILE; \
	pVisitCard->cGettextDomain = MY_APPLET_GETTEXT_DOMAIN; \
	pVisitCard->cDockVersionOnCompilation = MY_APPLET_DOCK_VERSION; \
	pVisitCard->cUserDataDir = MY_APPLET_USER_DATA_DIR; \
	pVisitCard->cShareDataDir = MY_APPLET_SHARE_DATA_DIR; \
	pVisitCard->cConfFileName = (MY_APPLET_CONF_FILE != NULL && strcmp (MY_APPLET_CONF_FILE, "none") != 0 ? MY_APPLET_CONF_FILE : NULL); \
	pVisitCard->cModuleVersion = MY_APPLET_VERSION;\
	pVisitCard->iCategory = _iAppletCategory; \
	pVisitCard->cIconFilePath = MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE; \
	pVisitCard->iSizeOfConfig = sizeof (AppletConfig);\
	pVisitCard->iSizeOfData = sizeof (AppletData);\
	pVisitCard->cAuthor = _cAuthor;\
	pVisitCard->cDescription = _cDescription;

#define CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE \
	pInterface->initModule = CD_APPLET_INIT_FUNC;\
	pInterface->stopModule = CD_APPLET_STOP_FUNC;\
	pInterface->reloadModule = CD_APPLET_RELOAD_FUNC;\
	pInterface->reset_config = CD_APPLET_RESET_CONFIG_FUNC;\
	pInterface->reset_data = CD_APPLET_RESET_DATA_FUNC;\
	pInterface->read_conf_file = CD_APPLET_READ_CONFIG_FUNC;

/** Fin de la fonction de pre-initialisation de l'applet.
*/
#define CD_APPLET_PRE_INIT_END \
	return TRUE ;\
}
/** Fonction de pre-initialisation generique. Ne fais que definir l'applet (en appelant les 2 macros precedentes), la plupart du temps cela est suffisant.
*/
#define CD_APPLET_DEFINITION(cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
CD_APPLET_PRE_INIT_BEGIN (cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE \
CD_APPLET_PRE_INIT_END

#define CD_APPLET_CAN_DETACH TRUE

//\______________________ init.
/** Debut de la fonction d'initialisation de l'applet (celle qui est appelee a chaque chargement de l'applet).
*Lis le fichier de conf de l'applet, et cree son icone ainsi que son contexte de dessin.
*@param erreur une GError, utilisable pour reporter une erreur ayant lieu durant l'initialisation.
*/
#define CD_APPLET_INIT_ALL_BEGIN(pApplet) \
CD_APPLET_INIT_PROTO (pApplet)\
{ \
	cd_message ("%s (%s)", __func__, pApplet->cConfFilePath);

/** Fin de la fonction d'initialisation de l'applet.
*/
#define CD_APPLET_INIT_END \
}

//\______________________ stop.
/** Debut de la fonction d'arret de l'applet.
*/
#define CD_APPLET_STOP_BEGIN \
CD_APPLET_STOP_PROTO \
{

/** Fin de la fonction d'arret de l'applet.
*/
#define CD_APPLET_STOP_END \
	cairo_dock_release_data_slot (myApplet); \
}

//\______________________ reload.
/** Debut de la fonction de rechargement de l'applet.
*/
#define CD_APPLET_RELOAD_ALL_BEGIN \
CD_APPLET_RELOAD_PROTO \
{ \
	cd_message ("%s (%s)\n", __func__, myApplet->cConfFilePath);

/** Fin de la fonction de rechargement de l'applet.
*/
#define CD_APPLET_RELOAD_END \
	return TRUE; \
}

/** TRUE ssi le fichier de conf de l'applet a change juste avant le reload.
*/
#define CD_APPLET_MY_CONFIG_CHANGED (pKeyFile != NULL)

/** TRUE ssi le type de container a change.
*/
#define CD_APPLET_MY_CONTAINER_TYPE_CHANGED (myApplet->pContainer == NULL || myApplet->pContainer->iType != pOldContainer->iType)

/** Le conteneur precedent le reload.
*/
#define CD_APPLET_MY_OLD_CONTAINER pOldContainer;

/** Chemin du fichier de conf de l'applet, appelable durant les fonctions d'init, de config, et de reload.
*/
#define CD_APPLET_MY_CONF_FILE myApplet->cConfFilePath
/** Fichier de cles de l'applet, appelable durant les fonctions d'init, de config, et de reload.
*/
#define CD_APPLET_MY_KEY_FILE pKeyFile


//\______________________ read_conf_file.
/** Debut de la fonction de configuration de l'applet (celle qui est appelee au debut de l'init).
*/
#define CD_APPLET_GET_CONFIG_ALL_BEGIN \
CD_APPLET_READ_CONFIG_PROTO \
{ \
	gboolean bFlushConfFileNeeded = FALSE;

/** Fin de la fonction de configuration de l'applet.
*/
#define CD_APPLET_GET_CONFIG_END \
	return bFlushConfFileNeeded; \
}

//\______________________ reset_config.
/** Debut de la fonction de liberation des donnees de la config.
*/
#define CD_APPLET_RESET_CONFIG_ALL_BEGIN \
CD_APPLET_RESET_CONFIG_PROTO \
{
/** Fin de la fonction de liberation des donnees de la config.
*/
#define CD_APPLET_RESET_CONFIG_END \
}

//\______________________ reset_data.
/** Debut de la fonction de liberation des donnees internes.
*/
#define CD_APPLET_RESET_DATA_BEGIN \
CD_APPLET_RESET_DATA_PROTO \
{
/** Fin de la fonction de liberation des donnees internes.
*/
#define CD_APPLET_RESET_DATA_ALL_END \
}

//\______________________ on click.
/** Debut de la fonction de notification au clic gauche.
*/
#define CD_APPLET_ON_CLICK_BEGIN \
CD_APPLET_ON_CLICK_PROTO \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{
/** Fin de la fonction de notification au clic gauche. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_CLICK_END \
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION; \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}

//\______________________ on build menu.
/** Debut de la fonction de notification de construction du menu.
*/
#define CD_APPLET_ON_BUILD_MENU_BEGIN \
CD_APPLET_ON_BUILD_MENU_PROTO \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{ \
		GtkWidget *pMenuItem, *image; \
		pMenuItem = gtk_separator_menu_item_new (); \
		gtk_menu_shell_append(GTK_MENU_SHELL (pAppletMenu), pMenuItem);
/** Fin de la fonction de notification de construction du menu. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_BUILD_MENU_END \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}

//\______________________ on middle-click.
/** Debut de la fonction de notification du clic du milieu.
*/
#define CD_APPLET_ON_MIDDLE_CLICK_BEGIN \
CD_APPLET_ON_MIDDLE_CLICK_PROTO \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{
/** Fin de la fonction de notification du clic du milieu. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_MIDDLE_CLICK_END \
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION; \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}

//\______________________ on drop-data.
/** Debut de la fonction de notification du glisse-depose.
*/
#define CD_APPLET_ON_DROP_DATA_BEGIN \
CD_APPLET_ON_DROP_DATA_PROTO \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{ \
		g_return_val_if_fail (cReceivedData != NULL, CAIRO_DOCK_LET_PASS_NOTIFICATION);
/** Fin de la fonction de notification du glisse-depose. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_DROP_DATA_END \
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION; \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}

//\______________________ on scroll.
/** Debut de la fonction de notification au scroll.
*/
#define CD_APPLET_ON_SCROLL_BEGIN \
CD_APPLET_ON_SCROLL_PROTO \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{
/** Fin de la fonction de notification au scroll. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_SCROLL_END \
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION; \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}

//\______________________ on update icon.
/** Debut de la fonction de notification d'update icon.
*/
#define CD_APPLET_ON_UPDATE_ICON_BEGIN \
CD_APPLET_ON_UPDATE_ICON_PROTO \
{ \
	if (pIcon != myIcon) \
		return CAIRO_DOCK_LET_PASS_NOTIFICATION;
/** Fin de la fonction de notification d'update icon.
*/
#define CD_APPLET_ON_UPDATE_ICON_END \
	*bContinueAnimation = TRUE;\
	CD_APPLET_REDRAW_MY_ICON; \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}

#define CD_APPLET_SKIP_UPDATE_ICON do { \
	*bContinueAnimation = TRUE; \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; } while (0)

#define CD_APPLET_STOP_UPDATE_ICON \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION

#define CD_APPLET_PAUSE_UPDATE_ICON do { \
	CD_APPLET_REDRAW_MY_ICON; \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; } while (0)


//\_________________________________ NOTIFICATIONS
//\______________________ notification clique gauche.
/** Abonne l'applet aux notifications du clic gauche. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_CLICK_EVENT cairo_dock_register_notification (CAIRO_DOCK_CLICK_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_CLICK_FUNC, CAIRO_DOCK_RUN_AFTER, myApplet);
/** Desabonne l'applet aux notifications du clic gauche. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_CLICK_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_CLICK_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_CLICK_FUNC, myApplet);
/** Abonne l'applet aux notifications de construction du menu. A effectuer lors de l'init de l'applet.
*/

//\______________________ notification construction menu.
#define CD_APPLET_REGISTER_FOR_BUILD_MENU_EVENT cairo_dock_register_notification (CAIRO_DOCK_BUILD_MENU, (CairoDockNotificationFunc) CD_APPLET_ON_BUILD_MENU_FUNC, CAIRO_DOCK_RUN_FIRST, myApplet);
/** Desabonne l'applet aux notifications de construction du menu. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_BUILD_MENU_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_BUILD_MENU, (CairoDockNotificationFunc) CD_APPLET_ON_BUILD_MENU_FUNC, myApplet);

//\______________________ notification clique milieu.
/** Abonne l'applet aux notifications du clic du milieu. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_MIDDLE_CLICK_EVENT cairo_dock_register_notification (CAIRO_DOCK_MIDDLE_CLICK_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_MIDDLE_CLICK_FUNC, CAIRO_DOCK_RUN_AFTER, myApplet)
/** Desabonne l'applet aux notifications du clic du milieu. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_MIDDLE_CLICK_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_MIDDLE_CLICK_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_MIDDLE_CLICK_FUNC, myApplet)

//\______________________ notification drag'n'drop.
/** Abonne l'applet aux notifications du glisse-depose. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_DROP_DATA_EVENT cairo_dock_register_notification (CAIRO_DOCK_DROP_DATA, (CairoDockNotificationFunc) CD_APPLET_ON_DROP_DATA_FUNC, CAIRO_DOCK_RUN_FIRST, myApplet);
/** Desabonne l'applet aux notifications du glisse-depose. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_DROP_DATA_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_DROP_DATA, (CairoDockNotificationFunc) CD_APPLET_ON_DROP_DATA_FUNC, myApplet);

//\______________________ notification de scroll molette.
/**
*Abonne l'applet aux notifications du clic gauche. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_SCROLL_EVENT cairo_dock_register_notification (CAIRO_DOCK_SCROLL_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_SCROLL_FUNC, CAIRO_DOCK_RUN_FIRST, myApplet)
/**
*Desabonne l'applet aux notifications du clic gauche. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_SCROLL_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_SCROLL_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_SCROLL_FUNC, myApplet)

//\______________________ notification de update icon.
#define CD_APPLET_REGISTER_FOR_UPDATE_ICON_SLOW_EVENT cairo_dock_register_notification (CAIRO_DOCK_UPDATE_ICON_SLOW, (CairoDockNotificationFunc) CD_APPLET_ON_UPDATE_ICON_FUNC, CAIRO_DOCK_RUN_FIRST, myApplet)
#define CD_APPLET_UNREGISTER_FOR_UPDATE_ICON_SLOW_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_UPDATE_ICON_SLOW, (CairoDockNotificationFunc) CD_APPLET_ON_UPDATE_ICON_FUNC, myApplet)
#define CD_APPLET_REGISTER_FOR_UPDATE_ICON_EVENT cairo_dock_register_notification (CAIRO_DOCK_UPDATE_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_UPDATE_ICON_FUNC, CAIRO_DOCK_RUN_FIRST, myApplet)
#define CD_APPLET_UNREGISTER_FOR_UPDATE_ICON_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_UPDATE_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_UPDATE_ICON_FUNC, myApplet)


//\_________________________________ INSTANCE
#ifdef CD_APPLET_MULTI_INSTANCE
#include <cairo-dock-applet-multi-instance.h>
#else
#include <cairo-dock-applet-single-instance.h>
#endif


G_END_DECLS
#endif
