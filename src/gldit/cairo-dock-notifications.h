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

#ifndef __CAIRO_DOCK_NOTIFICATIONS__
#define  __CAIRO_DOCK_NOTIFICATIONS__

#include <glib.h>
#include "cairo-dock-struct.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-notifications.h This class defines the notification system. Each time an event occurs (like an icon being clicked), Cairo-Dock broadcasts the corresponding notification. Anybody that has registered to it will be called then.
Common objects that hold notifications are Managers, Icons, and Containers (and their derivatives Docks, Desklets, Dialogs, Flyings).
*/

/// Generic prototype of a notification callback.
typedef gboolean (* CairoDockNotificationFunc) (gpointer pUserData, ...);

typedef struct {
	CairoDockNotificationFunc pFunction;
	gpointer pUserData;
	} CairoDockNotificationRecord;

typedef guint CairoDockNotificationType;

/// Use this in \ref cairo_dock_register_notification_on_object to be called before the dock.
#define CAIRO_DOCK_RUN_FIRST TRUE
/// Use this in \ref cairo_dock_register_notification_on_object to be called after the dock.
#define CAIRO_DOCK_RUN_AFTER FALSE

/// Return this in your callback to prevent the other callbacks from being called after you.
#define CAIRO_DOCK_INTERCEPT_NOTIFICATION TRUE
/// Return this in your callback to let pass the notification to the other callbacks after you.
#define CAIRO_DOCK_LET_PASS_NOTIFICATION FALSE


void cairo_dock_free_notification_table (GPtrArray *pNotificationsTab);

#define cairo_dock_install_notifications_on_object(pObject, iNbNotifs) do {\
	GPtrArray *pNotificationsTab = ((GldiObject*)pObject)->pNotificationsTab;\
	if (pNotificationsTab == NULL) {\
		pNotificationsTab = g_ptr_array_new ();\
		((GldiObject*)pObject)->pNotificationsTab = pNotificationsTab; }\
	if (pNotificationsTab->len < iNbNotifs)\
		g_ptr_array_set_size (pNotificationsTab, iNbNotifs); } while (0)

#define cairo_dock_clear_notifications_on_object(pObject) do {\
	GPtrArray *pNotificationsTab = ((GldiObject*)pObject)->pNotificationsTab;\
	cairo_dock_free_notification_table (pNotificationsTab);\
	((GldiObject*)pObject)->pNotificationsTab = NULL; } while (0)

/** Register an action to be called when a given notification is broadcasted from a given object.
*@param pObject the object (Icon, Container, Manager).
*@param iNotifType type of the notification.
*@param pFunction callback.
*@param bRunFirst CAIRO_DOCK_RUN_FIRST to be called before Cairo-Dock, CAIRO_DOCK_RUN_AFTER to be called after.
*@param pUserData data to be passed as the first parameter of the callback.
*/
void cairo_dock_register_notification_on_object (gpointer pObject, CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gboolean bRunFirst, gpointer pUserData);

/** Remove a callback from the list of callbacks of a given object for a given notification and a given data.
Note: it is safe to remove the callback when it is called, but not another one.
*@param pObject the object (Icon, Container, Manager) for which the action has been registered.
*@param iNotifType type of the notification.
*@param pFunction callback.
*@param pUserData data that was registerd with the callback.
*/
void cairo_dock_remove_notification_func_on_object (gpointer pObject, CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gpointer pUserData);


#define _notify(pNotificationRecordList, bStop, ...) do {\
	CairoDockNotificationRecord *pNotificationRecord;\
	GSList *pElement = pNotificationRecordList, *pNextElement;\
	while (pElement != NULL && ! bStop) {\
		pNotificationRecord = pElement->data;\
		pNextElement = pElement->next;\
		bStop = pNotificationRecord->pFunction (pNotificationRecord->pUserData, ##__VA_ARGS__);\
		pElement = pNextElement; }\
	} while (0)

#define _notify_on_object(pObject, iNotifType, ...) \
	__extension__ ({\
	gboolean _stop = FALSE;\
	GPtrArray *pNotificationsTab = (pObject)->pNotificationsTab;\
	if (pNotificationsTab && iNotifType < pNotificationsTab->len) {\
		GSList *pNotificationRecordList = g_ptr_array_index (pNotificationsTab, iNotifType);\
		_notify (pNotificationRecordList, _stop, ##__VA_ARGS__);} \
	else {_stop = TRUE;}\
	_stop; })

/** Broadcast a notification on a given object, and on all its managers.
*@param pObject the object (Icon, Container, Manager, ...).
*@param iNotifType type of the notification.
*@param ... parameters to be passed to the callbacks that have registered to this notification.
*/
#define cairo_dock_notify_on_object(pObject, iNotifType, ...) \
	__extension__ ({\
	gboolean _bStop = FALSE;\
	GldiObject *_obj = (GldiObject*)pObject;\
	while (_obj && !_bStop) {\
		_bStop = _notify_on_object (_obj, iNotifType, ##__VA_ARGS__);\
		_obj = GLDI_OBJECT (_obj->mgr); }\
	})

G_END_DECLS
#endif
