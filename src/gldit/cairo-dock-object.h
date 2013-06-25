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

#ifndef __CAIRO_DOCK_OBJECT__
#define  __CAIRO_DOCK_OBJECT__

#include <glib.h>
#include "cairo-dock-struct.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-object.h This class defines the Objects, the base class of libgldi.
* Every element in this library is an Object.
* An object is defined by an ObjectManager, which defines its capabilitie and signals.
* 
* Any object is created with \ref gldi_object_new and destroyed with \ref gldi_object_unref.
* An object can be deleted from the current theme with \ref gldi_object_delete.
* An object can be reloaded with \ref gldi_object_reload.
* 
* You can listen for notifications on an object with \ref gldi_object_register_notification and stop listening with \ref gldi_object_remove_notification.
* To listen for notifications on any object of a given type, simply register yourself on its ObjectManager.
*/

struct _GldiObject {
	gint ref;
	GPtrArray *pNotificationsTab;
	GldiObjectManager *mgr;
	GList *mgrs;  // sorted in reverse order
};


struct _GldiObjectManager {
	GldiObject object;
	const gchar *cName;
	gint iObjectSize;
	void (*init_object) (GldiObject *pObject, gpointer attr);
	void (*reset_object) (GldiObject *pObject);
	gboolean (*delete_object) (GldiObject *pObject);
	GKeyFile* (*reload_object) (GldiObject *pObject, gboolean bReloadConf, GKeyFile *pKeyFile);
};


/// signals (any object has at least these ones)
typedef enum {
	/// notification called when an object has been created. data : the object
	NOTIFICATION_NEW,
	/// notification called when the object is going to be destroyed. data : the object
	NOTIFICATION_DESTROY,
	NB_NOTIFICATIONS_OBJECT
	} GldiObjectNotifications;

#define GLDI_OBJECT(p) ((GldiObject*)(p))


void gldi_object_init (GldiObject *obj, GldiObjectManager *pMgr, gpointer attr);

/** Create a new object.
 * @param pMgr the ObjectManager
 * @param attr the attributes of the object
 * @return the new object, with a reference of 1; use \ref gldi_object_unref to destroy it
 */
GldiObject *gldi_object_new (GldiObjectManager *pMgr, gpointer attr);

/** Take a reference on an object.
 * @param pObject the Object
 */
void gldi_object_ref (GldiObject *pObject);

/** Drop your reference on an object. If it's the last reference, the object is destroyed, otherwise nothing happen.
 * @param pObject the Object
 */
void gldi_object_unref (GldiObject *pObject);

/** Delete an object from the current theme. The object is unref'd, and won't be created again on next startup.
 * @param pObject the Object
 */
void gldi_object_delete (GldiObject *pObject);

/** Reload an object.
 * @param pObject the Object
 * @param bReloadConfig TRUE to read its config file again (if the object has one)
 */
void gldi_object_reload (GldiObject *obj, gboolean bReloadConfig);


void gldi_object_set_manager (GldiObject *pObject, GldiObjectManager *pMgr);

gboolean gldi_object_is_manager_child (GldiObject *pObject, GldiObjectManager *pMgr);

#define gldi_object_get_type(obj) (GLDI_OBJECT(obj)->mgr && GLDI_OBJECT(obj)->ref > 0 ? GLDI_OBJECT(obj)->mgr->cName : "ObjectManager")


/// Generic prototype of a notification callback.
typedef gboolean (* GldiNotificationFunc) (gpointer pUserData, ...);

typedef struct {
	GldiNotificationFunc pFunction;
	gpointer pUserData;
	} GldiNotificationRecord;

typedef guint GldiNotificationType;

/// Use this in \ref gldi_object_register_notification to be called before the dock.
#define GLDI_RUN_FIRST TRUE
/// Use this in \ref gldi_object_register_notification to be called after the dock.
#define GLDI_RUN_AFTER FALSE

/// Return this in your callback to prevent the other callbacks from being called after you.
#define GLDI_NOTIFICATION_INTERCEPT TRUE
/// Return this in your callback to let pass the notification to the other callbacks after you.
#define GLDI_NOTIFICATION_LET_PASS FALSE


#define gldi_object_install_notifications(pObject, iNbNotifs) do {\
	GPtrArray *pNotificationsTab = (GLDI_OBJECT(pObject))->pNotificationsTab;\
	if (pNotificationsTab == NULL) {\
		pNotificationsTab = g_ptr_array_new ();\
		(GLDI_OBJECT(pObject))->pNotificationsTab = pNotificationsTab; }\
	if (pNotificationsTab->len < iNbNotifs)\
		g_ptr_array_set_size (pNotificationsTab, iNbNotifs); } while (0)

/** Register an action to be called when a given notification is broadcasted from a given object.
*@param pObject the object (Icon, Container, Manager).
*@param iNotifType type of the notification.
*@param pFunction callback.
*@param bRunFirst CAIRO_DOCK_RUN_FIRST to be called before Cairo-Dock, CAIRO_DOCK_RUN_AFTER to be called after.
*@param pUserData data to be passed as the first parameter of the callback.
*/
void gldi_object_register_notification (gpointer pObject, GldiNotificationType iNotifType, GldiNotificationFunc pFunction, gboolean bRunFirst, gpointer pUserData);

/** Remove a callback from the list of callbacks of a given object for a given notification and a given data.
Note: it is safe to remove the callback when it is called, but not another one.
*@param pObject the object (Icon, Container, Manager) for which the action has been registered.
*@param iNotifType type of the notification.
*@param pFunction callback.
*@param pUserData data that was registerd with the callback.
*/
void gldi_object_remove_notification (gpointer pObject, GldiNotificationType iNotifType, GldiNotificationFunc pFunction, gpointer pUserData);


#define __notify(pNotificationRecordList, bStop, ...) do {\
	GldiNotificationRecord *pNotificationRecord;\
	GSList *pElement = pNotificationRecordList, *pNextElement;\
	while (pElement != NULL && ! bStop) {\
		pNotificationRecord = pElement->data;\
		pNextElement = pElement->next;\
		bStop = pNotificationRecord->pFunction (pNotificationRecord->pUserData, ##__VA_ARGS__);\
		pElement = pNextElement; }\
	} while (0)

#define __notify_on_object(pObject, iNotifType, ...) \
	__extension__ ({\
	gboolean _stop = FALSE;\
	GPtrArray *pNotificationsTab = (pObject)->pNotificationsTab;\
	if (pNotificationsTab && iNotifType < pNotificationsTab->len) {\
		GSList *pNotificationRecordList = g_ptr_array_index (pNotificationsTab, iNotifType);\
		__notify (pNotificationRecordList, _stop, ##__VA_ARGS__);} \
	else {_stop = TRUE;}\
	_stop; })

/** Broadcast a notification on a given object, and on all its managers.
*@param pObject the object (Icon, Container, Manager, ...).
*@param iNotifType type of the notification.
*@param ... parameters to be passed to the callbacks that have registered to this notification.
*/
#define gldi_object_notify(pObject, iNotifType, ...) \
	__extension__ ({\
	gboolean _bStop = FALSE;\
	GldiObject *_obj = GLDI_OBJECT (pObject);\
	while (_obj && !_bStop) {\
		_bStop = __notify_on_object (_obj, iNotifType, ##__VA_ARGS__);\
		_obj = GLDI_OBJECT (_obj->mgr); }\
	})


G_END_DECLS
#endif
