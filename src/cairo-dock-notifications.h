
#ifndef __CAIRO_DOCK_NOTIFICATIONS__
#define  __CAIRO_DOCK_NOTIFICATIONS__

#include <glib.h>
#include "cairo-dock-struct.h"

G_BEGIN_DECLS


typedef gboolean (* CairoDockNotificationFunc) (gpointer pUserData, ...);

typedef struct {
	CairoDockNotificationFunc pFunction;
	gpointer pUserData;
	} CairoDockNotificationRecord;

typedef enum {
	/// notification appellee lorsque l'utilisateur clique sur une icone; l'animation est preparee juste avant, et lancee juste apres. data : {Icon, CairoDock, iState}
	CAIRO_DOCK_CLICK_ICON=0,
	/// notification appellee lorsque l'utilisateur double clique sur une icone. data : {Icon, CairoDock}
	CAIRO_DOCK_DOUBLE_CLICK_ICON,
	/// notification appellee lorsque l'utilisateur clique-milieu  sur une icone. data : {Icon, CairoDock}
	CAIRO_DOCK_MIDDLE_CLICK_ICON,
	/// data : {Icon, CairoDock, GtkMenu}
	CAIRO_DOCK_BUILD_MENU,
	/// data : {gchar*, Icon, double*, CairoDock}
	CAIRO_DOCK_DROP_DATA,
	/// notification appellee lors d'un changement de bureau ou de viewport. data : NULL
	CAIRO_DOCK_DESKTOP_CHANGED,
	/// notification appellee lorsqu'une fenetre est redimensionnee ou deplacee, ou lorsque l'ordre des fenetres change. data : XConfigureEvent ou NULL.
	CAIRO_DOCK_WINDOW_CONFIGURED,
	/// notification appellee lorsque la geometrie du bureau a change (nombre de viewports/bureaux, dimensions). data : NULL
	CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED,
	/// notification appellee lorsque la fenetre active a change. data : Xid
	CAIRO_DOCK_WINDOW_ACTIVATED,
	/// notification appellee lorsque l'utilisateur scrolle sur une icone en maintenant la touche SHIFT ou CTRL enfoncee. data : {Icon, CairoDock, iDirection}
	CAIRO_DOCK_SCROLL_ICON,
	
	CAIRO_DOCK_ENTER_ICON,  // la souris entre sur l'icone
	CAIRO_DOCK_UPDATE_ICON,  // mettre a jour l'icone (animations liees a l'icone, dessin de la surface, etc)
	CAIRO_DOCK_UPDATE_ICON_SLOW,  // idem mais a un rythme plus lent.
	CAIRO_DOCK_PRE_RENDER_ICON,  // rendu de l'arriere-plan de l'icone.
	CAIRO_DOCK_RENDER_ICON,  // rendu de l'icone.
	CAIRO_DOCK_STOP_ICON,  // arret de l'icone (pour suppression par exemple => tout liberer)
	CAIRO_DOCK_ENTER_DOCK,  // la souris entre dans le dock
	CAIRO_DOCK_UPDATE_DOCK,  // mise a jour du dock (animations liees au dock, etc)
	CAIRO_DOCK_UPDATE_DOCK_SLOW,  // idem mais a un rythme plus lent.
	CAIRO_DOCK_RENDER_DOCK,  // rendu du dock.
	CAIRO_DOCK_STOP_DOCK,  // arret du dock (suppression par exemple => tout liberer).
	CAIRO_DOCK_START_DRAG_DATA,  // on entre dans le dock avec un truc a deposer.
	CAIRO_DOCK_REQUEST_ICON_ANIMATION,  // une demande d'animation specifique a ete emise.
	CAIRO_DOCK_MOUSE_MOVED,  // la souris a bouge.
	CAIRO_DOCK_ENTER_DESKLET,  // on entre dans le desklet.
	CAIRO_DOCK_UPDATE_DESKLET,  // le desklet est mis a jour.
	CAIRO_DOCK_UPDATE_DESKLET_SLOW,  // idem mais a un rythme plus lent.
	CAIRO_DOCK_RENDER_DESKLET,  // rendu du desklet.
	CAIRO_DOCK_STOP_DESKLET,  // arret du desklet (suppression par exemple => tout liberer).
	/// notification appellee lorsqu'une icone est inseree dans un dock. data : {Icon, CairoDock}
	CAIRO_DOCK_INSERT_ICON,
	/// notification appellee lorsqu'une icone passe en mode suppression d'un dock. data : {Icon, CairoDock}
	CAIRO_DOCK_REMOVE_ICON,
	CAIRO_DOCK_UPDATE_FLYING_CONTAINER,
	CAIRO_DOCK_RENDER_FLYING_CONTAINER,
	CAIRO_DOCK_UPDATE_DIALOG,
	CAIRO_DOCK_RENDER_DIALOG,
	CAIRO_DOCK_NB_NOTIFICATIONS
	} CairoDockNotificationType;

typedef gboolean (* CairoDockRemoveIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock);
typedef gboolean (* CairoDockClickIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, int iState);
typedef gboolean (* CairoDockDoubleClickIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock);
typedef gboolean (* CairoDockMiddleClickIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock);
typedef gboolean (* CairoDockBuildMenuFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, GtkMenu *pMenu);
typedef gboolean (* CairoDockScrollIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, int iDirection);
typedef gboolean (* CairoDockDropDataFunc) (gpointer pUserData, gchar *cData, Icon *pIcon, double fPosition, CairoDock *pDock);
typedef gboolean (* CairoDockDesktopChangedFunc) (gpointer pUserData);
typedef gboolean (* CairoDockWindowConfiguredFunc) (gpointer pUserData, XConfigureEvent *pXEvent);
typedef gboolean (* CairoDockScreenGeometryAlteredFunc) (gpointer pUserData);
typedef gboolean (* CairoDockWindowActivatedFunc) (gpointer pUserData, Window Xid);

typedef gboolean (* CairoDockEnterIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bStartAnimation);
typedef gboolean (* CairoDockUpdateIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bContinueAnimation);
typedef gboolean (* CairoDockPreRenderIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock);
typedef gboolean (* CairoDockRenderIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext);
typedef gboolean (* CairoDockStopIconFunc) (gpointer pUserData, Icon *pIcon);
typedef gboolean (* CairoDockEnterContainerFunc) (gpointer pUserData, CairoContainer *pContainer, gboolean *bStartAnimation);
typedef gboolean (* CairoDockUpdateContainerFunc) (gpointer pUserData, CairoContainer *pContainer, gboolean *bContinueAnimation);
typedef gboolean (* CairoDockRenderContainerFunc) (gpointer pUserData, CairoContainer *pContainer, cairo_t *pCairoContext);
typedef gboolean (* CairoDockStopContainerFunc) (gpointer pUserData, CairoContainer *pContainer);
typedef gboolean (* CairoDockStartDragDataFunc) (gpointer pUserData, CairoDock *pDock, gboolean *bStartAnimation);
typedef gboolean (* CairoDockAnimateIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gchar *cAnimationName, gint iNbRounds);
typedef gboolean (* CairoDockInsertRemoveIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock);


#define CAIRO_DOCK_RUN_FIRST TRUE
#define CAIRO_DOCK_RUN_AFTER FALSE

#define CAIRO_DOCK_INTERCEPT_NOTIFICATION TRUE
#define CAIRO_DOCK_LET_PASS_NOTIFICATION FALSE


GSList *cairo_dock_get_notifications_list (CairoDockNotificationType iNotifType);

/**
*Enregistre une action a appeler lors d'une notification.
*@param iNotifType type de la notification.
*@param pFunction fonction notifiee.
*@param bRunFirst CAIRO_DOCK_RUN_FIRST pour etre enregistre en 1er, CAIRO_DOCK_RUN_AFTER pour etre enregistreé en dernier.
*/
void cairo_dock_register_notification (CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gboolean bRunFirst, gpointer pUserData);

void cairo_dock_register_notification_on_icon (Icon *pIcon, CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gboolean bRunFirst, gpointer pUserData);

/**
*Enleve une fonction de la liste des fonctions appelees par une notification donnee.
*@param iNotifType type de la notification.
*@param pFunction fonction notifiee.
*/
void cairo_dock_remove_notification_func (CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gpointer pUserData);

void cairo_dock_remove_notification_func_on_icon (Icon *pIcon, CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gpointer pUserData);

/**
*Appelle toutes les fonctions enregistrees pour une notification donnee.
*@param iNotifType type de la notification.
*@param ... donnees passees a la fonction notifiee.
@return TRUE si la notification a ete utilisee par quelqu'un, FALSE si aucune fonction n'est enregistree pour elle.
*/

#define _cairo_dock_notify(pNotificationRecordList, bStop, ...) do {\
	if (pNotificationRecordList == NULL) {\
		FALSE; }\
	else {\
		CairoDockNotificationFunc pFunction;\
		CairoDockNotificationRecord *pNotificationRecord;\
		GSList *pElement = pNotificationRecordList;\
		while (pElement != NULL && ! bStop) {\
			pNotificationRecord = pElement->data;\
			bStop = pNotificationRecord->pFunction (pNotificationRecord->pUserData, ##__VA_ARGS__);\
			pElement = pElement->next; }\
		TRUE; }\
	} while (0)

#define cairo_dock_notify(iNotifType, ...) do {\
	gboolean bStop = FALSE;\
	GSList *pNotificationRecordList = cairo_dock_get_notifications_list (iNotifType);\
	_cairo_dock_notify(pNotificationRecordList, bStop, ##__VA_ARGS__);\
	} while (0)

#define cairo_dock_notify_on_icon(pIcon, iNotifType, ...) do {\
	gboolean bStop = FALSE;\
	GSList *pNotificationRecordList = cairo_dock_get_notifications_list (iNotifType);\
	_cairo_dock_notify(pNotificationRecordList, bStop, ##__VA_ARGS__);\
	if (pIcon && pIcon->pNotificationTab) {\
		GSList *pNotificationRecordList = g_ptr_array_index (pIcon->pNotificationTab, iNotifType);\
		_cairo_dock_notify(pNotificationRecordList, bStop, ##__VA_ARGS__);}\
	} while (0)


void cairo_dock_free_notification_table (GPtrArray *pNotificationsTab);


G_END_DECLS
#endif
