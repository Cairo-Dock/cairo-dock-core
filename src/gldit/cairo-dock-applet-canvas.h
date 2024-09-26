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

#ifndef __CAIRO_DOCK_APPLET_CANVAS__
#define  __CAIRO_DOCK_APPLET_CANVAS__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-applet-canvas.h This file defines numerous macros, that form a canvas for all the applets.
* 
* You probably won't need to dig into this file, since you can generate an applet with the 'generate-new-applet.sh' script, that will build the whole canvas for you.
* Moreover, you can have a look at an applet that has a similar functioning to yours.
*/

//\_________________________________ STRUCT
typedef struct _AppletConfig AppletConfig;
typedef struct _AppletData AppletData;

//\_________________________________ FUNCTIONS NAMES
#define CD_APPLET_DEFINE_FUNC pre_init
#define CD_APPLET_POST_LOAD_FUNC post_load
#define CD_APPLET_INIT_FUNC init
#define CD_APPLET_STOP_FUNC stop
#define CD_APPLET_RELOAD_FUNC reload

#define CD_APPLET_READ_CONFIG_FUNC read_conf_file
#define CD_APPLET_RESET_CONFIG_FUNC reset_config
#define CD_APPLET_RESET_DATA_FUNC reset_data

#define CD_APPLET_ON_CLICK_FUNC action_on_click
#define CD_APPLET_ON_BUILD_MENU_FUNC action_on_build_menu
#define CD_APPLET_ON_MIDDLE_CLICK_FUNC action_on_middle_click
#define CD_APPLET_ON_DOUBLE_CLICK_FUNC action_on_double_click
#define CD_APPLET_ON_DROP_DATA_FUNC action_on_drop_data
#define CD_APPLET_ON_SCROLL_FUNC action_on_scroll
#define CD_APPLET_ON_UPDATE_ICON_FUNC action_on_update_icon

//\_________________________________ PROTO
#define CD_APPLET_DEFINE_PROTO \
gboolean CD_APPLET_DEFINE_FUNC (GldiVisitCard *pVisitCard, GldiModuleInterface *pInterface)
#define CD_APPLET_POST_LOAD_PROTO \
gboolean CD_APPLET_POST_LOAD_FUNC (GldiVisitCard *pVisitCard, GldiModuleInterface *pInterface, gpointer)
#define CD_APPLET_INIT_PROTO(pApplet) \
void CD_APPLET_INIT_FUNC (GldiModuleInstance *pApplet, G_GNUC_UNUSED GKeyFile *pKeyFile)
#define CD_APPLET_STOP_PROTO \
void CD_APPLET_STOP_FUNC (GldiModuleInstance *myApplet)
#define CD_APPLET_RELOAD_PROTO \
gboolean CD_APPLET_RELOAD_FUNC (GldiModuleInstance *myApplet, GldiContainer *pOldContainer, G_GNUC_UNUSED GKeyFile *pKeyFile)

#define CD_APPLET_READ_CONFIG_PROTO \
gboolean CD_APPLET_READ_CONFIG_FUNC (GldiModuleInstance *myApplet, G_GNUC_UNUSED GKeyFile *pKeyFile)
#define CD_APPLET_RESET_CONFIG_PROTO \
void CD_APPLET_RESET_CONFIG_FUNC (GldiModuleInstance *myApplet)
#define CD_APPLET_RESET_DATA_PROTO \
void CD_APPLET_RESET_DATA_FUNC (GldiModuleInstance *myApplet)

#define CD_APPLET_ON_CLICK_PROTO \
gboolean CD_APPLET_ON_CLICK_FUNC (GldiModuleInstance *myApplet, Icon *pClickedIcon, GldiContainer *pClickedContainer, G_GNUC_UNUSED guint iButtonState)
#define CD_APPLET_ON_BUILD_MENU_PROTO \
gboolean CD_APPLET_ON_BUILD_MENU_FUNC (GldiModuleInstance *myApplet, Icon *pClickedIcon, GldiContainer *pClickedContainer, GtkWidget *pAppletMenu)
#define CD_APPLET_ON_MIDDLE_CLICK_PROTO \
gboolean CD_APPLET_ON_MIDDLE_CLICK_FUNC (GldiModuleInstance *myApplet, Icon *pClickedIcon, GldiContainer *pClickedContainer)
#define CD_APPLET_ON_DOUBLE_CLICK_PROTO \
gboolean CD_APPLET_ON_DOUBLE_CLICK_FUNC (GldiModuleInstance *myApplet, Icon *pClickedIcon, GldiContainer *pClickedContainer)
#define CD_APPLET_ON_DROP_DATA_PROTO \
gboolean CD_APPLET_ON_DROP_DATA_FUNC (GldiModuleInstance *myApplet, const gchar *cReceivedData, Icon *pClickedIcon, double fPosition, GldiContainer *pClickedContainer)
#define CD_APPLET_ON_SCROLL_PROTO \
gboolean CD_APPLET_ON_SCROLL_FUNC (GldiModuleInstance *myApplet, Icon *pClickedIcon, GldiContainer *pClickedContainer, int iDirection)
#define CD_APPLET_ON_UPDATE_ICON_PROTO \
gboolean CD_APPLET_ON_UPDATE_ICON_FUNC (GldiModuleInstance *myApplet, Icon *pIcon, GldiContainer *pContainer, gboolean *bContinueAnimation)

//\_________________________________ HEADERS
#define CD_APPLET_H \
CD_APPLET_DEFINE_PROTO; \
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
#define CD_APPLET_ON_DOUBLE_CLICK_H \
CD_APPLET_ON_DOUBLE_CLICK_PROTO;
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
*@param _cName nom de sous lequel l'applet sera enregistree par Cairo-Dock.
*@param _iMajorVersion version majeure du dock necessaire au bon fonctionnement de l'applet.
*@param _iMinorVersion version mineure du dock necessaire au bon fonctionnement de l'applet.
*@param _iMicroVersion version micro du dock necessaire au bon fonctionnement de l'applet.
*@param _iAppletCategory Categorie de l'applet (CAIRO_DOCK_CATEGORY_ACCESSORY, CAIRO_DOCK_CATEGORY_DESKTOP, CAIRO_DOCK_CATEGORY_CONTROLER)
*@param _cDescription description et mode d'emploi succint de l'applet.
*@param _cAuthor nom de l'auteur et eventuellement adresse mail.
*/
#define CD_APPLET_DEFINE_ALL_BEGIN(_cName, _iMajorVersion, _iMinorVersion, _iMicroVersion, _iAppletCategory, _cDescription, _cAuthor) \
CD_APPLET_DEFINE_PROTO \
{ \
	pVisitCard->cModuleName = _cName; \
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
	pVisitCard->cDescription = _cDescription;\
	pVisitCard->cTitle = _cName;\
	pVisitCard->iContainerType = CAIRO_DOCK_MODULE_CAN_DOCK | CAIRO_DOCK_MODULE_CAN_DESKLET;

#define CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE \
	pInterface->initModule = CD_APPLET_INIT_FUNC;\
	pInterface->stopModule = CD_APPLET_STOP_FUNC;\
	pInterface->reloadModule = CD_APPLET_RELOAD_FUNC;\
	pInterface->reset_config = CD_APPLET_RESET_CONFIG_FUNC;\
	pInterface->reset_data = CD_APPLET_RESET_DATA_FUNC;\
	pInterface->read_conf_file = CD_APPLET_READ_CONFIG_FUNC;

#define CD_APPLET_REDEFINE_TITLE(_cTitle) pVisitCard->cTitle = _cTitle;

#define CD_APPLET_SET_CONTAINER_TYPE(x) pVisitCard->iContainerType = x;

#define CD_APPLET_SET_UNRESIZABLE_DESKLET pVisitCard->bStaticDeskletSize = TRUE;

#define CD_APPLET_ALLOW_EMPTY_TITLE pVisitCard->bAllowEmptyTitle = TRUE;

#define CD_APPLET_ACT_AS_LAUNCHER pVisitCard->bActAsLauncher = TRUE;

/** Fin de la fonction de pre-initialisation de l'applet.
*/
#define CD_APPLET_DEFINE_END \
	pVisitCard->cTitle = dgettext (MY_APPLET_GETTEXT_DOMAIN, pVisitCard->cTitle);\
	return TRUE ;\
}
/** Fonction de pre-initialisation generique. Ne fais que definir l'applet (en appelant les 2 macros precedentes), la plupart du temps cela est suffisant.
*/
#define CD_APPLET_DEFINITION(cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
CD_APPLET_DEFINE_BEGIN (cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory, cDescription, cAuthor) \
CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE \
CD_APPLET_DEFINE_END


/** New type of applet definition. Uses the pre_init function only for filling out the visit card and
 *  the post_load function for all other init. */
#define CD_APPLET_DEFINE2_ALL_BEGIN(cName, iFlags, _iAppletCategory, _cDescription, _cAuthor) \
static CD_APPLET_POST_LOAD_PROTO; \
CD_APPLET_DEFINE_ALL_BEGIN (cName, 4, GLDI_ABI_VERSION, iFlags, _iAppletCategory, _cDescription, _cAuthor) \
	pVisitCard->postLoad = CD_APPLET_POST_LOAD_FUNC; \
CD_APPLET_DEFINE_END \
CD_APPLET_POST_LOAD_PROTO \
{

#define CD_APPLET_DEFINE2_END \
	return TRUE; \
}

#define CD_APPLET_DEFINITION2(cName, iFlags, iAppletCategory, cDescription, cAuthor) \
CD_APPLET_DEFINE2_BEGIN (cName, iFlags, iAppletCategory, cDescription, cAuthor) \
CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE \
CD_APPLET_DEFINE2_END



#define CD_APPLET_EXTEND_MANAGER(cManagerName) gldi_manager_extend (pVisitCard, cManagerName)

//\______________________ init.
/** Debut de la fonction d'initialisation de l'applet (celle qui est appelee a chaque chargement de l'applet).
*Lis le fichier de conf de l'applet, et cree son icone ainsi que son contexte de dessin.
*@param pApplet une instance du module.
*/
#define CD_APPLET_INIT_ALL_BEGIN(pApplet) \
CD_APPLET_INIT_PROTO (pApplet)\
{ \
	g_pCurrentModule = pApplet;\
	cd_message ("%s (%s)", __func__, pApplet->cConfFilePath);

/** Fin de la fonction d'initialisation de l'applet.
*/
#define CD_APPLET_INIT_END \
	g_pCurrentModule = NULL;\
}

//\______________________ stop.
/** Debut de la fonction d'arret de l'applet.
*/
#define CD_APPLET_STOP_BEGIN \
CD_APPLET_STOP_PROTO \
{\
	g_pCurrentModule = myApplet;

/** Fin de la fonction d'arret de l'applet.
*/
#define CD_APPLET_STOP_END \
	g_pCurrentModule = NULL;\
}

//\______________________ reload.
/** Debut de la fonction de rechargement de l'applet.
*/
#define CD_APPLET_RELOAD_ALL_BEGIN \
CD_APPLET_RELOAD_PROTO \
{ \
	g_pCurrentModule = myApplet;\
	cd_message ("%s (%s)", __func__, myApplet->cConfFilePath);

/** Fin de la fonction de rechargement de l'applet.
*/
#define CD_APPLET_RELOAD_END \
	g_pCurrentModule = NULL;\
	return TRUE; \
}


//\______________________ read_conf_file.
/** Debut de la fonction de configuration de l'applet (celle qui est appelee au debut de l'init).
*/
#define CD_APPLET_GET_CONFIG_ALL_BEGIN \
CD_APPLET_READ_CONFIG_PROTO \
{ \
	g_pCurrentModule = myApplet;\
	gboolean bFlushConfFileNeeded = FALSE;

/** Fin de la fonction de configuration de l'applet.
*/
#define CD_APPLET_GET_CONFIG_END \
	g_pCurrentModule = NULL;\
	return bFlushConfFileNeeded; \
}

//\______________________ reset_config.
/** Debut de la fonction de liberation des donnees de la config.
*/
#define CD_APPLET_RESET_CONFIG_ALL_BEGIN \
CD_APPLET_RESET_CONFIG_PROTO \
{\
	g_pCurrentModule = myApplet;
/** Fin de la fonction de liberation des donnees de la config.
*/
#define CD_APPLET_RESET_CONFIG_ALL_END \
	g_pCurrentModule = NULL;\
}

//\______________________ reset_data.
/** Debut de la fonction de liberation des donnees internes.
*/
#define CD_APPLET_RESET_DATA_BEGIN \
CD_APPLET_RESET_DATA_PROTO \
{\
	g_pCurrentModule = myApplet;
/** Fin de la fonction de liberation des donnees internes.
*/
#define CD_APPLET_RESET_DATA_ALL_END \
	g_pCurrentModule = NULL;\
}

//\______________________ on click.
/** Debut de la fonction de notification au clic gauche.
*/
#define CD_APPLET_ON_CLICK_BEGIN \
CD_APPLET_ON_CLICK_PROTO \
{ \
	g_pCurrentModule = myApplet;\
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{
/** Fin de la fonction de notification au clic gauche. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_CLICK_END \
		g_pCurrentModule = NULL;\
		return GLDI_NOTIFICATION_INTERCEPT; \
	} \
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; \
}

//\______________________ on build menu.
/** Debut de la fonction de notification de construction du menu.
*/
#define CD_APPLET_ON_BUILD_MENU_BEGIN \
CD_APPLET_ON_BUILD_MENU_PROTO \
{ \
	g_pCurrentModule = myApplet;\
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{ \
		GtkWidget *pMenuItem; \
		if (pClickedIcon == myIcon || (pClickedContainer == CAIRO_CONTAINER (myDesklet) && pClickedIcon == NULL)) {\
			pMenuItem = gtk_separator_menu_item_new (); \
			gtk_menu_shell_append(GTK_MENU_SHELL (pAppletMenu), pMenuItem); }
/** Fin de la fonction de notification de construction du menu. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_BUILD_MENU_END \
	} \
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; \
}

//\______________________ on middle-click.
/** Debut de la fonction de notification du clic du milieu.
*/
#define CD_APPLET_ON_MIDDLE_CLICK_BEGIN \
CD_APPLET_ON_MIDDLE_CLICK_PROTO \
{ \
	g_pCurrentModule = myApplet;\
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{
/** Fin de la fonction de notification du clic du milieu. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_MIDDLE_CLICK_END \
		g_pCurrentModule = NULL;\
		return GLDI_NOTIFICATION_INTERCEPT; \
	} \
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; \
}

//\______________________ on double-click.
/** Debut de la fonction de notification du clic du milieu.
*/
#define CD_APPLET_ON_DOUBLE_CLICK_BEGIN \
CD_APPLET_ON_DOUBLE_CLICK_PROTO \
{ \
	g_pCurrentModule = myApplet;\
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{
/** Fin de la fonction de notification du clic du milieu. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_DOUBLE_CLICK_END \
		g_pCurrentModule = NULL;\
		return GLDI_NOTIFICATION_INTERCEPT; \
	} \
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; \
}

//\______________________ on drop-data.
/** Debut de la fonction de notification du glisse-depose.
*/
#define CD_APPLET_ON_DROP_DATA_BEGIN \
CD_APPLET_ON_DROP_DATA_PROTO \
{ \
	g_pCurrentModule = myApplet;\
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{ \
		g_return_val_if_fail (cReceivedData != NULL, GLDI_NOTIFICATION_LET_PASS);
/** Fin de la fonction de notification du glisse-depose. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_DROP_DATA_END \
		g_pCurrentModule = NULL;\
		return GLDI_NOTIFICATION_INTERCEPT; \
	} \
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; \
}

//\______________________ on scroll.
/** Debut de la fonction de notification au scroll.
*/
#define CD_APPLET_ON_SCROLL_BEGIN \
CD_APPLET_ON_SCROLL_PROTO \
{ \
	g_pCurrentModule = myApplet;\
	if (pClickedIcon != NULL && (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet))) \
	{
/** Fin de la fonction de notification au scroll. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_SCROLL_END \
		g_pCurrentModule = NULL;\
		return GLDI_NOTIFICATION_INTERCEPT; \
	} \
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; \
}

//\______________________ on update icon.
/** Debut de la fonction de notification d'update icon.
*/
#define CD_APPLET_ON_UPDATE_ICON_BEGIN \
CD_APPLET_ON_UPDATE_ICON_PROTO \
{ \
	if (pIcon != myIcon)\
		return GLDI_NOTIFICATION_LET_PASS;\
	g_pCurrentModule = myApplet;
/** Fin de la fonction de notification d'update icon.
*/
#define CD_APPLET_ON_UPDATE_ICON_END \
	*bContinueAnimation = TRUE;\
	CD_APPLET_REDRAW_MY_ICON;\
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; \
}

/** Quit the update function immediately and wait for the next update.
*/
#define CD_APPLET_SKIP_UPDATE_ICON do { \
	*bContinueAnimation = TRUE; \
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; } while (0)

/** Quit the update function immediately with no more updates.
*/
#define CD_APPLET_STOP_UPDATE_ICON do { \
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; } while (0)

/** Quit the update function immediately with no more updates after redrawing the icon.
*/
#define CD_APPLET_PAUSE_UPDATE_ICON do { \
	CD_APPLET_REDRAW_MY_ICON; \
	g_pCurrentModule = NULL;\
	return GLDI_NOTIFICATION_LET_PASS; } while (0)


//\_________________________________ NOTIFICATIONS
//\______________________ notification clique gauche.
/** Abonne l'applet aux notifications du clic gauche. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_CLICK_EVENT gldi_object_register_notification (&myContainerObjectMgr, NOTIFICATION_CLICK_ICON, (GldiNotificationFunc) CD_APPLET_ON_CLICK_FUNC, GLDI_RUN_AFTER, myApplet);
/** Desabonne l'applet aux notifications du clic gauche. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_CLICK_EVENT gldi_object_remove_notification (&myContainerObjectMgr, NOTIFICATION_CLICK_ICON, (GldiNotificationFunc) CD_APPLET_ON_CLICK_FUNC, myApplet);
/** Abonne l'applet aux notifications de construction du menu. A effectuer lors de l'init de l'applet.
*/

//\______________________ notification construction menu.
#define CD_APPLET_REGISTER_FOR_BUILD_MENU_EVENT gldi_object_register_notification (&myContainerObjectMgr, NOTIFICATION_BUILD_ICON_MENU, (GldiNotificationFunc) CD_APPLET_ON_BUILD_MENU_FUNC, GLDI_RUN_FIRST, myApplet);
/** Desabonne l'applet aux notifications de construction du menu. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_BUILD_MENU_EVENT gldi_object_remove_notification (&myContainerObjectMgr, NOTIFICATION_BUILD_ICON_MENU, (GldiNotificationFunc) CD_APPLET_ON_BUILD_MENU_FUNC, myApplet);

//\______________________ notification clic milieu.
/** Abonne l'applet aux notifications du clic du milieu. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_MIDDLE_CLICK_EVENT gldi_object_register_notification (&myContainerObjectMgr, NOTIFICATION_MIDDLE_CLICK_ICON, (GldiNotificationFunc) CD_APPLET_ON_MIDDLE_CLICK_FUNC, GLDI_RUN_AFTER, myApplet)
/** Desabonne l'applet aux notifications du clic du milieu. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_MIDDLE_CLICK_EVENT gldi_object_remove_notification (&myContainerObjectMgr, NOTIFICATION_MIDDLE_CLICK_ICON, (GldiNotificationFunc) CD_APPLET_ON_MIDDLE_CLICK_FUNC, myApplet)

//\______________________ notification double clic.
/** Abonne l'applet aux notifications du double clic. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_DOUBLE_CLICK_EVENT do {\
	cairo_dock_listen_for_double_click (myIcon);\
	gldi_object_register_notification (&myContainerObjectMgr, NOTIFICATION_DOUBLE_CLICK_ICON, (GldiNotificationFunc) CD_APPLET_ON_DOUBLE_CLICK_FUNC, GLDI_RUN_AFTER, myApplet); } while (0)
/** Desabonne l'applet aux notifications du double clic. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_DOUBLE_CLICK_EVENT do {\
	gldi_object_remove_notification (&myContainerObjectMgr, NOTIFICATION_DOUBLE_CLICK_ICON, (GldiNotificationFunc) CD_APPLET_ON_DOUBLE_CLICK_FUNC, myApplet);\
	cairo_dock_stop_listening_for_double_click (myIcon); } while (0)

//\______________________ notification drag'n'drop.
/** Abonne l'applet aux notifications du glisse-depose. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_DROP_DATA_EVENT gldi_object_register_notification (&myContainerObjectMgr, NOTIFICATION_DROP_DATA, (GldiNotificationFunc) CD_APPLET_ON_DROP_DATA_FUNC, GLDI_RUN_FIRST, myApplet);
/** Desabonne l'applet aux notifications du glisse-depose. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_DROP_DATA_EVENT gldi_object_remove_notification (&myContainerObjectMgr, NOTIFICATION_DROP_DATA, (GldiNotificationFunc) CD_APPLET_ON_DROP_DATA_FUNC, myApplet);

//\______________________ notification de scroll molette.
/**
*Abonne l'applet aux notifications du clic gauche. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_SCROLL_EVENT gldi_object_register_notification (&myContainerObjectMgr, NOTIFICATION_SCROLL_ICON, (GldiNotificationFunc) CD_APPLET_ON_SCROLL_FUNC, GLDI_RUN_FIRST, myApplet)
/**
*Desabonne l'applet aux notifications du clic gauche. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_SCROLL_EVENT gldi_object_remove_notification (&myContainerObjectMgr, NOTIFICATION_SCROLL_ICON, (GldiNotificationFunc) CD_APPLET_ON_SCROLL_FUNC, myApplet)

//\______________________ notification de update icon.
/** Register the applet to the 'update icon' notifications of the slow rendering loop. 
*/
#define CD_APPLET_REGISTER_FOR_UPDATE_ICON_SLOW_EVENT gldi_object_register_notification (&myIconObjectMgr, NOTIFICATION_UPDATE_ICON_SLOW, (GldiNotificationFunc) CD_APPLET_ON_UPDATE_ICON_FUNC, GLDI_RUN_FIRST, myApplet)
/** Unregister the applet from the slow rendering loop. 
*/
#define CD_APPLET_UNREGISTER_FOR_UPDATE_ICON_SLOW_EVENT gldi_object_remove_notification (&myIconObjectMgr, NOTIFICATION_UPDATE_ICON_SLOW, (GldiNotificationFunc) CD_APPLET_ON_UPDATE_ICON_FUNC, myApplet)
/** Register the applet to the 'update icon' notifications of the fast rendering loop. 
*/
#define CD_APPLET_REGISTER_FOR_UPDATE_ICON_EVENT gldi_object_register_notification (&myIconObjectMgr, NOTIFICATION_UPDATE_ICON, (GldiNotificationFunc) CD_APPLET_ON_UPDATE_ICON_FUNC, GLDI_RUN_FIRST, myApplet)
/** Unregister the applet from the fast rendering loop. 
*/
#define CD_APPLET_UNREGISTER_FOR_UPDATE_ICON_EVENT gldi_object_remove_notification (&myIconObjectMgr, NOTIFICATION_UPDATE_ICON, (GldiNotificationFunc) CD_APPLET_ON_UPDATE_ICON_FUNC, myApplet)


//\_________________________________ INSTANCE
#ifdef CD_APPLET_MULTI_INSTANCE
#include <cairo-dock-applet-multi-instance.h>
#else
#include <cairo-dock-applet-single-instance.h>
#endif


G_END_DECLS
#endif
