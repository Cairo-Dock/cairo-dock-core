
#ifndef __CAIRO_DOCK_NOTIFICATIONS__
#define  __CAIRO_DOCK_NOTIFICATIONS__

#include <glib.h>
#include "cairo-dock-struct.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-notifications.h This class defines the notification system. Each time an event occurs (like an icon being clicked), Cairo-Dock broadcasts the corresponding notification. Anybody that has registered to it will be called then.
*/

/// Generic prototype of a notification callback.
typedef gboolean (* CairoDockNotificationFunc) (gpointer pUserData, ...);

typedef struct {
	CairoDockNotificationFunc pFunction;
	gpointer pUserData;
	} CairoDockNotificationRecord;

/// The list of all notifications.
typedef enum {
	/// notification called when use clicks on an icon data : {Icon, CairoDock, int}
	CAIRO_DOCK_CLICK_ICON=0,
	/// notification called when the user double-clicks on an icon. data : {Icon, CairoDock}
	CAIRO_DOCK_DOUBLE_CLICK_ICON,
	/// notification called when the user middle-clicks on an icon. data : {Icon, CairoDock}
	CAIRO_DOCK_MIDDLE_CLICK_ICON,
	/// notification called when the user scrolls on an icon. data : {Icon, CairoDock, int}
	CAIRO_DOCK_SCROLL_ICON,
	/// notification called when the menu is being built. data : {Icon, CairoDock, GtkMenu}
	CAIRO_DOCK_BUILD_MENU,
	/// notification called when something is dropped inside a container. data : {gchar*, Icon, double*, CairoDock}
	CAIRO_DOCK_START_DRAG_DATA,
	/// notification called when someone asks for an animation for a given icon.
	CAIRO_DOCK_DROP_DATA,
	/// notification called when the mouse has moved inside a container.
	CAIRO_DOCK_MOUSE_MOVED,
	/// notification called when a key is pressed in a container that has the focus.
	CAIRO_DOCK_KEY_PRESSED,
	
	/// notification called when the user switches to another desktop/viewport. data : NULL
	CAIRO_DOCK_DESKTOP_CHANGED,
	/// notification called when a window is resized or moved, or when the z-order of windows has changed. data : XConfigureEvent or NULL.
	CAIRO_DOCK_WINDOW_CONFIGURED,
	/// notification called when the geometry of the desktop has changed (number of viewports/desktops, dimensions). data : NULL
	CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED,
	/// notification called when the active window has changed. data : Window* or NULL
	CAIRO_DOCK_WINDOW_ACTIVATED,
	/// notification called when the state of the keyboard has changed.
	CAIRO_DOCK_KBD_STATE_CHANGED,
	
	/// notification called when an icon has just been inserted into a dock. data : {Icon, CairoDock}
	CAIRO_DOCK_INSERT_ICON,
	/// notification called when an icon is going to be removed from a dock. data : {Icon, CairoDock}
	CAIRO_DOCK_REMOVE_ICON,
	/// notification called when the mouse enters a dock while dragging an object.
	CAIRO_DOCK_REQUEST_ICON_ANIMATION,
	
	/// notification called when the mouse enters an icon. data : {Icon, CairoDock, gboolean*}
	CAIRO_DOCK_ENTER_ICON,
	/// notification called when an icon is updated in the fast rendering loop.
	CAIRO_DOCK_UPDATE_ICON,
	/// notification called when an icon is updated in the slow rendering loop.
	CAIRO_DOCK_UPDATE_ICON_SLOW,
	/// notification called when the background of an icon is rendered.
	CAIRO_DOCK_PRE_RENDER_ICON,
	/// notification called when an icon is rendered.
	CAIRO_DOCK_RENDER_ICON,
	/// notification called when an icon is stopped, for instance before it is removed.
	CAIRO_DOCK_STOP_ICON,
	
	/// notification called when the mouse enters a dock.
	CAIRO_DOCK_ENTER_DOCK,
	/// notification called when a dock is updated in the fast rendering loop.
	CAIRO_DOCK_UPDATE_DOCK,
	/// notification called when when a dock is updated in the slow rendering loop.
	CAIRO_DOCK_UPDATE_DOCK_SLOW,
	/// notification called when a dock is rendered.
	CAIRO_DOCK_RENDER_DOCK,
	/// notification called when a dock is stopped, for instance before it is destroyed.
	CAIRO_DOCK_STOP_DOCK,
	
	/// notification called when the mouse enters a desklet.
	CAIRO_DOCK_ENTER_DESKLET,
	/// notification called when a desklet is updated in the fast rendering loop.
	CAIRO_DOCK_UPDATE_DESKLET,
	/// notification called when a desklet is updated in the slow rendering loop.
	CAIRO_DOCK_UPDATE_DESKLET_SLOW,
	/// notification called when a desklet is rendered.
	CAIRO_DOCK_RENDER_DESKLET,
	/// notification called when a desklet is stopped, for instance before it is destroyed.
	CAIRO_DOCK_STOP_DESKLET,
	
	/// notification called when a FlyingContainer is updated in the fast rendering loop.
	CAIRO_DOCK_UPDATE_FLYING_CONTAINER,
	/// notification called when a FlyingContainer is rendered.
	CAIRO_DOCK_RENDER_FLYING_CONTAINER,
	
	/// notification called when a Dialog is updated in the fast rendering loop.
	CAIRO_DOCK_UPDATE_DIALOG,
	/// notification called when a Dialog is rendered.
	CAIRO_DOCK_RENDER_DIALOG,
	
	/// notification called for the fast rendering loop on a default container.
	CAIRO_DOCK_UPDATE_DEFAULT_CONTAINER,
	/// notification called for the slow rendering loop on a default container.
	CAIRO_DOCK_UPDATE_DEFAULT_CONTAINER_SLOW,
	/// notification called when a default container is rendered.
	CAIRO_DOCK_RENDER_DEFAULT_CONTAINER,
	CAIRO_DOCK_NB_NOTIFICATIONS
	} CairoDockNotificationType;

/// prototype of the callback to the CAIRO_DOCK_CLICK_ICON notification.
typedef gboolean (* CairoDockClickIconFunc) (gpointer pUserData, Icon *pIcon, CairoContainer *pContainer, int iState);
/// prototype of the callback to the CAIRO_DOCK_DOUBLE_CLICK_ICON notification.
typedef gboolean (* CairoDockDoubleClickIconFunc) (gpointer pUserData, Icon *pIcon, CairoContainer *pContainer);
/// prototype of the callback to the CAIRO_DOCK_MIDDLE_CLICK_ICON notification.
typedef gboolean (* CairoDockMiddleClickIconFunc) (gpointer pUserData, Icon *pIcon, CairoContainer *pContainer);
/// prototype of the callback to the CAIRO_DOCK_SCROLL_ICON notification.
typedef gboolean (* CairoDockScrollIconFunc) (gpointer pUserData, Icon *pIcon, CairoContainer *pContainer, int iDirection);
/// prototype of the callback to the CAIRO_DOCK_BUILD_MENU notification.
typedef gboolean (* CairoDockBuildMenuFunc) (gpointer pUserData, Icon *pIcon, CairoContainer *pContainer, GtkMenu *pMenu);
/// prototype of the callback to the CAIRO_DOCK_START_DRAG_DATA notification.
typedef gboolean (* CairoDockStartDragDataFunc) (gpointer pUserData, CairoContainer *pContainer, gboolean *bStartAnimation);
/// prototype of the callback to the CAIRO_DOCK_DROP_DATA notification.
typedef gboolean (* CairoDockDropDataFunc) (gpointer pUserData, gchar *cData, Icon *pIcon, double fPosition, CairoContainer *pContainer);
/// prototype of the callback to the CAIRO_DOCK_MOUSE_MOVED notification.
typedef gboolean (* CairoDockMouseMovedFunc) (gpointer pUserData, CairoContainer *pContainer, gboolean *bStartAnimation);
/// prototype of the callback to the CAIRO_DOCK_KEY_PRESSED notification.
typedef gboolean (* CairoDockKeyPressedFunc) (gpointer pUserData, CairoDock *pDock, int keyval, int state, gchar *string);

/// prototype of the callback to the CAIRO_DOCK_DESKTOP_CHANGED notification.
typedef gboolean (* CairoDockDesktopChangedFunc) (gpointer pUserData);
/// prototype of the callback to the CAIRO_DOCK_WINDOW_CONFIGURED notification.
typedef gboolean (* CairoDockWindowConfiguredFunc) (gpointer pUserData, XConfigureEvent *pXEvent);
/// prototype of the callback to the CAIRO_DOCK_SCREEN_GEOMETRY_ALTERED notification.
typedef gboolean (* CairoDockScreenGeometryAlteredFunc) (gpointer pUserData);
/// prototype of the callback to the CAIRO_DOCK_WINDOW_ACTIVATED notification.
typedef gboolean (* CairoDockWindowActivatedFunc) (gpointer pUserData, Window *Xid);
/// prototype of the callback to the CAIRO_DOCK_KBD_STATE_CHANGED notification.
typedef gboolean (* CairoDockKeyboardStateChangedFunc) (gpointer pUserData, Window *Xid);

/// prototype of the callback to the CAIRO_DOCK_INSERT_ICON and CAIRO_DOCK_REMOVE_ICON notifications.
typedef gboolean (* CairoDockInsertRemoveIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock);
/// prototype of the callback to the CAIRO_DOCK_REQUEST_ICON_ANIMATION notification.
typedef gboolean (* CairoDockRequestAnimationFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gchar *cAnimationName, gint iNbRounds);

/// prototype of the callback to the CAIRO_DOCK_ENTER_ICON notification.
typedef gboolean (* CairoDockEnterIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bStartAnimation);
/// prototype of the callback to the CAIRO_DOCK_UPDATE_ICON and CAIRO_DOCK_UPDATE_ICON_SLOW notifications.
typedef gboolean (* CairoDockUpdateIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bContinueAnimation);

/// prototype of the callback to the CAIRO_DOCK_PRE_RENDER_ICON notification.
typedef gboolean (* CairoDockPreRenderIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock);
/// prototype of the callback to the CAIRO_DOCK_RENDER_ICON notification.
typedef gboolean (* CairoDockRenderIconFunc) (gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bHasBeenRendered, cairo_t *pCairoContext);
/// prototype of the callback to the CAIRO_DOCK_STOP_ICON notification.
typedef gboolean (* CairoDockStopIconFunc) (gpointer pUserData, Icon *pIcon);

/// prototype of the callback to the CAIRO_DOCK_ENTER_DOCK and CAIRO_DOCK_ENTER_DESKLET notifications.
typedef gboolean (* CairoDockEnterContainerFunc) (gpointer pUserData, CairoContainer *pContainer, gboolean *bStartAnimation);
/// prototype of the callback to the CAIRO_DOCK_UPDATE_DOCK, CAIRO_DOCK_UPDATE_DOCK_SLOW, CAIRO_DOCK_UPDATE_DESKLET, CAIRO_DOCK_UPDATE_DESKLET_SLOW, CAIRO_DOCK_UPDATE_DIALOG and CAIRO_DOCK_UPDATE_FLYING_CONTAINER notifications.
typedef gboolean (* CairoDockUpdateContainerFunc) (gpointer pUserData, CairoContainer *pContainer, gboolean *bContinueAnimation);
/// prototype of the callback to the CAIRO_DOCK_RENDER_DOCK, CAIRO_DOCK_RENDER_DESKLET, CAIRO_DOCK_RENDER_DIALOG, CAIRO_DOCK_RENDER_FLYING_CONTAINER notifications. 'pCairoContext' is NULL for an OpenGL rendering.
typedef gboolean (* CairoDockRenderContainerFunc) (gpointer pUserData, CairoContainer *pContainer, cairo_t *pCairoContext);
/// prototype of the callback to the CAIRO_DOCK_STOP_DOCK and CAIRO_DOCK_STOP_DESKLET notifications.
typedef gboolean (* CairoDockStopContainerFunc) (gpointer pUserData, CairoContainer *pContainer);


/// Use this in \ref cairo_dock_register_notification to be called before the dock.
#define CAIRO_DOCK_RUN_FIRST TRUE
/// Use this in \ref cairo_dock_register_notification to be called after the dock.
#define CAIRO_DOCK_RUN_AFTER FALSE

/// Return this in your callback to prevent the other callbacks from being called after you.
#define CAIRO_DOCK_INTERCEPT_NOTIFICATION TRUE
/// Return this in your callback to let pass the notification to the other callbacks after you.
#define CAIRO_DOCK_LET_PASS_NOTIFICATION FALSE


GSList *cairo_dock_get_notifications_list (CairoDockNotificationType iNotifType);

/** Register an action to be called when a given notification is raised.
*@param iNotifType type of the notification.
*@param pFunction callback.
*@param bRunFirst CAIRO_DOCK_RUN_FIRST to be called before Cairo-Dock, CAIRO_DOCK_RUN_AFTER to be called after.
*@param pUserData data to be passed as the first parameter of the callback.
*/
void cairo_dock_register_notification (CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gboolean bRunFirst, gpointer pUserData);

void cairo_dock_register_notification_on_icon (Icon *pIcon, CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gboolean bRunFirst, gpointer pUserData);

void cairo_dock_register_notification_on_container (CairoContainer *pContainer, CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gboolean bRunFirst, gpointer pUserData);

/** Remove a callback from the list list of callbacks for a given notification and a given data.
*@param iNotifType type of the notification.
*@param pFunction callback.
*@param pUserData data that was registerd with the callback.
*/
void cairo_dock_remove_notification_func (CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gpointer pUserData);

void cairo_dock_remove_notification_func_on_icon (Icon *pIcon, CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gpointer pUserData);

void cairo_dock_remove_notification_func_on_container (CairoContainer *pContainer, CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gpointer pUserData);


#define _cairo_dock_notify(pNotificationRecordList, bStop, ...) do {\
	if (pNotificationRecordList != NULL) {\
		CairoDockNotificationRecord *pNotificationRecord;\
		GSList *pElement = pNotificationRecordList, *pNextElement;\
		while (pElement != NULL && ! bStop) {\
			pNotificationRecord = pElement->data;\
			pNextElement = pElement->next;\
			bStop = pNotificationRecord->pFunction (pNotificationRecord->pUserData, ##__VA_ARGS__);\
			pElement = pNextElement; } }\
	} while (0)

/** Broadcast a notification.
*@param iNotifType type of the notification.
*@param ... parameters to be passed to the callbacks that has registerd to this notification.
*/
#define cairo_dock_notify(iNotifType, ...) do {\
	gboolean bStop = FALSE;\
	GSList *pNotificationRecordList = cairo_dock_get_notifications_list (iNotifType);\
	_cairo_dock_notify(pNotificationRecordList, bStop, ##__VA_ARGS__);\
	} while (0)

#define cairo_dock_notify_on_icon(pIcon, iNotifType, ...) do {\
	gboolean bStop = FALSE;\
	GSList *pNotificationRecordList = cairo_dock_get_notifications_list (iNotifType);\
	_cairo_dock_notify(pNotificationRecordList, bStop, ##__VA_ARGS__);\
	if (pIcon && pIcon->pNotificationsTab) {\
		GSList *pNotificationRecordList = g_ptr_array_index (pIcon->pNotificationsTab, iNotifType);\
		_cairo_dock_notify(pNotificationRecordList, bStop, ##__VA_ARGS__);}\
	} while (0)

#define cairo_dock_notify_on_container(pContainer, iNotifType, ...) do {\
	gboolean bStop = FALSE;\
	GSList *pNotificationRecordList = cairo_dock_get_notifications_list (iNotifType);\
	_cairo_dock_notify(pNotificationRecordList, bStop, ##__VA_ARGS__);\
	if (pContainer && pContainer->pNotificationsTab) {\
		GSList *pNotificationRecordList = g_ptr_array_index (pContainer->pNotificationsTab, iNotifType);\
		_cairo_dock_notify(pNotificationRecordList, bStop, ##__VA_ARGS__);}\
	} while (0)


void cairo_dock_free_notification_table (GPtrArray *pNotificationsTab);


G_END_DECLS
#endif
