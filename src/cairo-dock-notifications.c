/******************************************************************************

This file is a part of the cairo-dock program, 
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include "cairo-dock-notifications.h"

static GPtrArray *s_pNotificationsTab = NULL;


void cairo_dock_register_notification (CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gboolean bRunFirst, gpointer pUserData)
{
	if (s_pNotificationsTab == NULL)
	{
		s_pNotificationsTab = g_ptr_array_new ();  // un 'g_ptr_array_sized_new' ne met pas a 0 les pointeurs, et laisse 'len' a 0.
		g_ptr_array_set_size (s_pNotificationsTab, CAIRO_DOCK_NB_NOTIFICATIONS);
	}
	g_return_if_fail (iNotifType < s_pNotificationsTab->len);
	
	CairoDockNotificationRecord *pNotificationRecord = g_new (CairoDockNotificationRecord, 1);
	pNotificationRecord->pFunction = pFunction;
	pNotificationRecord->pUserData = pUserData;
	
	GSList *pNotificationRecordList = g_ptr_array_index (s_pNotificationsTab, iNotifType);
	if (bRunFirst)
		s_pNotificationsTab->pdata[iNotifType] = g_slist_prepend (pNotificationRecordList, pNotificationRecord);
	else
		s_pNotificationsTab->pdata[iNotifType] = g_slist_append (pNotificationRecordList, pNotificationRecord);
}

void cairo_dock_remove_notification_func (CairoDockNotificationType iNotifType, CairoDockNotificationFunc pFunction, gpointer pUserData)
{
	if (s_pNotificationsTab != NULL)
	{
		GSList *pNotificationRecordList = g_ptr_array_index (s_pNotificationsTab, iNotifType);
		CairoDockNotificationRecord *pNotificationRecord;
		GSList *pElement;
		for (pElement = pNotificationRecordList; pElement != NULL; pElement = pElement->next)
		{
			pNotificationRecord = pElement->data;
			if (pNotificationRecord->pFunction == pFunction && pNotificationRecord->pUserData == pUserData)
			{
				s_pNotificationsTab->pdata[iNotifType] = g_slist_delete_link (pNotificationRecordList, pElement);
				g_free (pNotificationRecord);
			}
		}
	}
}

GSList *cairo_dock_get_notifications_list (CairoDockNotificationType iNotifType)
{
	if (s_pNotificationsTab == NULL || iNotifType >= s_pNotificationsTab->len)
		return NULL;
	
	return g_ptr_array_index (s_pNotificationsTab, iNotifType);
}


static void cairo_dock_register_notifications (gboolean bRunFirst, int iFirstNotifType, va_list args)
{
	CairoDockNotificationType iNotifType = iFirstNotifType;
	CairoDockNotificationFunc pFunction;
	gpointer pUserData;
	
	while (iNotifType != -1)
	{
		//g_print ("%s () : %d\n", __func__, iNotifType);
		
		pFunction = va_arg (args, CairoDockNotificationFunc);
		if (pFunction == NULL)  // ne devrait pas arriver.
			break;
		pUserData = va_arg (args, gpointer);
		
		cairo_dock_register_notification (iNotifType, pFunction, bRunFirst, pUserData);
		
		iNotifType = va_arg (args, CairoDockNotificationType);
	}
}

void cairo_dock_register_first_notifications (int iFirstNotifType, ...)
{
	va_list args;
	va_start (args, iFirstNotifType);
	cairo_dock_register_notifications (CAIRO_DOCK_RUN_FIRST, iFirstNotifType, args);
	va_end (args);
}

void cairo_dock_register_last_notifications (int iFirstNotifType, ...)
{
	va_list args;
	va_start (args, iFirstNotifType);
	cairo_dock_register_notifications (CAIRO_DOCK_RUN_AFTER, iFirstNotifType, args);
	va_end (args);
}

void cairo_dock_remove_notification_funcs (int iFirstNotifType, ...)
{
	va_list args;
	va_start (args, iFirstNotifType);
	
	CairoDockNotificationType iNotifType = iFirstNotifType;
	CairoDockNotificationFunc pFunction;
	gpointer pUserData;
	
	while (iNotifType != -1)
	{
		pFunction= va_arg (args, CairoDockNotificationFunc);
		if (pFunction == NULL)  // ne devrait pas arriver.
			break;
		
		pUserData = va_arg (args, gpointer);
		
		cairo_dock_remove_notification_func (iNotifType, pFunction, pUserData);
		
		iNotifType = va_arg (args, CairoDockNotificationType);
	}
	
	va_end (args);
}
