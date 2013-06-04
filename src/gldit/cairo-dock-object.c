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

#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-object.h"

/* obj -> mgr0 -> mgr1 -> ... -> mgrN
 * new_dialog -> new_object (mgr, attr) -> mgr->top_parent->init(attr) -> mgr->parent->init(attr) -> mgr->init(attr) --> notif
 * unref_object -> ref-- -> notif -> mgr->destroy -> mgr->parent->destroy -> mgr->top_parent->destroy --> free
 * GLDI_OBJECT_IS_xxx obj->mgr == pMgr || mgr->parent->mrg == pMgr || ...
 * */

void gldi_object_set_manager (GldiObject *pObject, GldiManager *pMgr)
{
	pObject->mgr = pMgr;
	pObject->mgrs = g_list_copy (pMgr->object.mgrs);
	pObject->mgrs = g_list_append (pObject->mgrs, pMgr);
	gldi_object_install_notifications (pObject, pMgr->object.pNotificationsTab->len);
}
void gldi_object_init (GldiObject *obj, GldiManager *pMgr, gpointer attr)
{
	obj->ref = 1;
	// set the manager
	gldi_object_set_manager (obj, pMgr);
	
	// init the object
	GList *m;
	for (m = obj->mgrs; m != NULL; m = m->next)
	{
		pMgr = m->data;
		if (pMgr->init_object)
			pMgr->init_object (obj, attr);
	}
	// emit a notification
	gldi_object_notify (obj, NOTIFICATION_NEW, obj);
}

GldiObject *gldi_object_new (GldiManager *pMgr, gpointer attr)
{
	GldiObject *obj = g_malloc0 (pMgr->iObjectSize);
	gldi_object_init (obj, pMgr, attr);
	return obj;
}

void gldi_object_ref (GldiObject *pObject)
{
	g_return_if_fail (pObject != NULL && pObject->ref > 0);
	pObject->ref ++;
}

void gldi_object_unref (GldiObject *pObject)
{
	if (pObject == NULL)
		return;
	pObject->ref --;
	if (pObject->ref == 0)  // so if it was already 0, don't do anything
	{
		// emit a notification
		gldi_object_notify (pObject, NOTIFICATION_DESTROY, pObject);
		
		// reset the object
		GldiManager *pMgr = pObject->mgr;
		while (pMgr)
		{
			if (pMgr->reset_object)
				pMgr->reset_object (pObject);
			pMgr = pMgr->object.mgr;
		}
		
		// clear notifications
		GPtrArray *pNotificationsTab = pObject->pNotificationsTab;
		guint i;
		for (i = 0; i < pNotificationsTab->len; i ++)
		{
			GSList *pNotificationRecordList = g_ptr_array_index (pNotificationsTab, i);
			g_slist_foreach (pNotificationRecordList, (GFunc)g_free, NULL);
			g_slist_free (pNotificationRecordList);
		}
		g_ptr_array_free (pNotificationsTab, TRUE);
		
		// free memory
		g_free (pObject);
	}
}

void gldi_object_delete (GldiObject *pObject)
{
	if (pObject == NULL)
		return;
	
	//\_________________ delete the object from the current theme
	gboolean r = TRUE;
	GldiManager *pMgr = pObject->mgr;
	while (pMgr)
	{
		if (pMgr->delete_object)
			r = pMgr->delete_object (pObject);
		if (!r)
			return;
		pMgr = pMgr->object.mgr;
	}
	
	//\_________________ destroy the object
	gldi_object_unref (pObject);
}

gboolean gldi_object_is_manager_child (GldiObject *pObject, GldiManager *pMgr)
{
	while (pObject)
	{
		if (pObject->mgr == pMgr)
			return TRUE;
		pObject = GLDI_OBJECT (pObject->mgr);
	}
	return FALSE;
}

#define GLDI_OBJECT_IS_DOCK(obj) gldi_object_is_manager_child (obj, &myDocksMgr)



void gldi_object_register_notification (gpointer pObject, GldiNotificationType iNotifType, GldiNotificationFunc pFunction, gboolean bRunFirst, gpointer pUserData)
{
	g_return_if_fail (pObject != NULL);
	// grab the notifications tab
	GPtrArray *pNotificationsTab = GLDI_OBJECT(pObject)->pNotificationsTab;
	if (!pNotificationsTab || pNotificationsTab->len < iNotifType)
	{
		cd_warning ("someone tried to register to an inexisting notification (%d) on an object of type '%s'", iNotifType, GLDI_OBJECT(pObject)->mgr?GLDI_OBJECT(pObject)->mgr->cModuleName:"manager");
		return ;  // don't try to create/resize the notifications tab, since noone will emit this notification.
	}
	
	// add a record
	GldiNotificationRecord *pNotificationRecord = g_new (GldiNotificationRecord, 1);
	pNotificationRecord->pFunction = pFunction;
	pNotificationRecord->pUserData = pUserData;
	
	GSList *pNotificationRecordList = g_ptr_array_index (pNotificationsTab, iNotifType);
	pNotificationsTab->pdata[iNotifType] = (bRunFirst ? g_slist_prepend : g_slist_append) (pNotificationRecordList, pNotificationRecord);
}


void gldi_object_remove_notification (gpointer pObject, GldiNotificationType iNotifType, GldiNotificationFunc pFunction, gpointer pUserData)
{
	g_return_if_fail (pObject != NULL);
	// grab the notifications tab
	GPtrArray *pNotificationsTab = GLDI_OBJECT(pObject)->pNotificationsTab;
	
	// remove the record
	GSList *pNotificationRecordList = g_ptr_array_index (pNotificationsTab, iNotifType);
	GldiNotificationRecord *pNotificationRecord;
	GSList *nr;
	for (nr = pNotificationRecordList; nr != NULL; nr = nr->next)
	{
		pNotificationRecord = nr->data;
		if (pNotificationRecord->pFunction == pFunction && pNotificationRecord->pUserData == pUserData)
		{
			pNotificationsTab->pdata[iNotifType] = g_slist_delete_link (pNotificationRecordList, nr);
			g_free (pNotificationRecord);
			break;
		}
	}
}
