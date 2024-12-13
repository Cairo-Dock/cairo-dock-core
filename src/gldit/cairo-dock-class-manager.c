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

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <cairo.h>

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_remove_version_from_string
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_notify_startup
#include "cairo-dock-module-manager.h"  // GldiModule
#include "cairo-dock-module-instance-manager.h"  // GldiModuleInstance
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-indicator-manager.h"  // myIndicatorsParam.bUseClassIndic
#include "cairo-dock-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-desktop-file-db.h"
#include "cairo-dock-class-manager.h"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopEnv g_iDesktopEnv;



/// Definition of a Class of application.
struct _CairoDockClassAppli {
	/// TRUE if the appli must use the icon provided by X instead the one from the theme.
	gboolean bUseXIcon;
	/// TRUE if the appli doesn't group togather with its class.
	gboolean bExpand;
	/// List of the inhibitors of the class.
	GList *pIconsOfClass;
	/// List of the appli icons of this class.
	GList *pAppliOfClass;
	gboolean bSearchedAttributes;
	/// Class of the app as reported by the WM / compositor without parsing or any changes to it
	gchar *cStartupWMClass;
	gchar *cIcon;
	gchar *cName;
	gint iAge;  // age of the first created window of this class
	gchar *cDockName;  // unique name of the class sub-dock
	guint iSidOpeningTimeout;  // timeout to stop the launching, if not stopped by the application before
	gboolean bIsLaunching;  // flag to mark a class as being launched
	gboolean bHasStartupNotify;  // TRUE if the application sends a "remove" event when its launch is complete (not used yet)
	GDesktopAppInfo *app; // desktop file opened by us
};

typedef struct _CairoDockClassAppli CairoDockClassAppli;


static GHashTable *s_hClassTable = NULL;
static GHashTable *s_hAltClass = NULL; // we store alternative class / app-ids here


static void cairo_dock_free_class_appli (CairoDockClassAppli *pClassAppli)
{
	g_list_free (pClassAppli->pIconsOfClass);
	g_list_free (pClassAppli->pAppliOfClass);
	g_free (pClassAppli->cName);
	g_free (pClassAppli->cIcon);
	g_free (pClassAppli->cStartupWMClass);
	if (pClassAppli->iSidOpeningTimeout != 0)
		g_source_remove (pClassAppli->iSidOpeningTimeout);
	if (pClassAppli->app) g_object_unref (pClassAppli->app);
	g_free (pClassAppli);
}

static inline CairoDockClassAppli *_cairo_dock_lookup_class_appli (const gchar *cClass)
{
	gpointer ret = NULL;
	if (cClass)
	{
		ret = g_hash_table_lookup (s_hClassTable, cClass);
		if (!ret) ret = g_hash_table_lookup (s_hAltClass, cClass);
	}
	return ret;
}

static gboolean _on_window_created (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	gldi_class_startup_notify_end (actor->cClass);

	return GLDI_NOTIFICATION_LET_PASS;
}
static gboolean _on_window_activated (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	if (! actor)
		return GLDI_NOTIFICATION_LET_PASS;

	gldi_class_startup_notify_end (actor->cClass);

	return GLDI_NOTIFICATION_LET_PASS;
}
void cairo_dock_initialize_class_manager (void)
{
	gldi_desktop_file_db_init ();
	if (s_hClassTable == NULL)
		s_hClassTable = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			g_free,
			(GDestroyNotify) cairo_dock_free_class_appli);
	if (s_hAltClass == NULL)
		s_hAltClass = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			g_free,
			NULL);
	// register to events to detect the ending of a launching
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_CREATED,
		(GldiNotificationFunc) _on_window_created,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_ACTIVATED,
		(GldiNotificationFunc) _on_window_activated,
		GLDI_RUN_AFTER, NULL);  // some applications don't open a new window, but rather take the focus; 
}


const GList *cairo_dock_list_existing_appli_with_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL ? pClassAppli->pAppliOfClass : NULL);
}

static gboolean _cairo_dock_add_inhibitor_to_class (const gchar *cClass, Icon *pIcon)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, FALSE);

	g_return_val_if_fail (g_list_find (pClassAppli->pIconsOfClass, pIcon) == NULL, TRUE);
	pClassAppli->pIconsOfClass = g_list_prepend (pClassAppli->pIconsOfClass, pIcon);

	return TRUE;
}

CairoDock *cairo_dock_get_class_subdock (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	return gldi_dock_get (pClassAppli->cDockName);
}

CairoDock* cairo_dock_create_class_subdock (const gchar *cClass, CairoDock *pParentDock)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	CairoDock *pDock = gldi_dock_get (pClassAppli->cDockName);
	if (pDock == NULL)  // cDockName not yet defined, or previous class subdock no longer exists
	{
		g_free (pClassAppli->cDockName);
		pClassAppli->cDockName = cairo_dock_get_unique_dock_name (cClass);
		pDock = gldi_subdock_new (pClassAppli->cDockName, NULL, pParentDock, NULL);
	}

	return pDock;
}

static void cairo_dock_destroy_class_subdock (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_if_fail (pClassAppli != NULL);

	CairoDock *pDock = gldi_dock_get (pClassAppli->cDockName);
	if (pDock)
	{
		gldi_object_unref (GLDI_OBJECT(pDock));
	}

	g_free (pClassAppli->cDockName);
	pClassAppli->cDockName = NULL;
}


static gchar *_cairo_dock_register_class_full (const gchar *cDesktopFile, const gchar *cClassName,
	const gchar *cWmClass, gboolean bUseWmClass, CairoDockClassAppli **pClassAppli);

gboolean cairo_dock_add_appli_icon_to_class (Icon *pIcon)
{
	g_return_val_if_fail (CAIRO_DOCK_ICON_TYPE_IS_APPLI (pIcon) && pIcon->pAppli, FALSE);
	cd_debug ("%s (%s)", __func__, pIcon->cClass);

	if (pIcon->cClass == NULL)
	{
		cd_message (" %s doesn't have any class, not good!", pIcon->cName);
		return FALSE;
	}

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	if (pClassAppli == NULL && pIcon->pAppli->cWmClass)
		pClassAppli = _cairo_dock_lookup_class_appli (pIcon->pAppli->cWmClass);
	if (!pClassAppli)
	{
		// not seen before, register this class
		// note: (1) this will always create pClassAppli, even if not found
		// (2) the return value is the same as pIcon->cClass
		char *cClass = _cairo_dock_register_class_full (NULL, pIcon->cClass,
			pIcon->pAppli->cWmClass, FALSE, &pClassAppli);
		if (cClass) g_free (cClass);
		if (!pClassAppli) return FALSE; // should not happen
	}

	if (pIcon->pClassApp) g_object_unref (pIcon->pClassApp);
	pIcon->pClassApp = g_object_ref (pClassAppli->app); // should be not null
	//!! TODO: handle pCustomLauncher here? (or in applications-facility?)
	//!! this is needed for shift + click and similar to use the correct command if 
	//!! multiple icons are added (if only one, the launcher icon will take over)

	if (pClassAppli->pAppliOfClass == NULL)  // the first appli of a class defines the age of the class.
		pClassAppli->iAge = pIcon->pAppli->iAge;

	g_return_val_if_fail (g_list_find (pClassAppli->pAppliOfClass, pIcon) == NULL, TRUE);
	pClassAppli->pAppliOfClass = g_list_prepend (pClassAppli->pAppliOfClass, pIcon);

	return TRUE;
}

gboolean cairo_dock_remove_appli_from_class (Icon *pIcon)
{
	g_return_val_if_fail (pIcon!= NULL, FALSE);
	cd_debug ("%s (%s, %s)", __func__, pIcon->cClass, pIcon->cName);

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_val_if_fail (pClassAppli != NULL, FALSE);

	pClassAppli->pAppliOfClass = g_list_remove (pClassAppli->pAppliOfClass, pIcon);

	return TRUE;
}

gboolean cairo_dock_set_class_use_xicon (const gchar *cClass, gboolean bUseXIcon)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, FALSE);

	if (pClassAppli->bUseXIcon == bUseXIcon)  // nothing to do.
		return FALSE;

	GList *pElement;
	Icon *pAppliIcon;
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL; pElement = pElement->next)
	{
		pAppliIcon = pElement->data;
		if (bUseXIcon)
		{
			cd_message ("%s: take X icon", pAppliIcon->cName);
		}
		else
		{
			cd_message ("%s: doesn't use X icon", pAppliIcon->cName);
		}

		cairo_dock_reload_icon_image (pAppliIcon, cairo_dock_get_icon_container (pAppliIcon));
	}

	return TRUE;
}


static void _cairo_dock_set_same_indicator_on_sub_dock (Icon *pInhibhatorIcon)
{
	CairoDock *pInhibatorDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibhatorIcon));
	if (GLDI_OBJECT_IS_DOCK(pInhibatorDock) && pInhibatorDock->iRefCount > 0)  // the inhibitor is in a sub-dock.
	{
		gboolean bSubDockHasIndicator = FALSE;
		if (pInhibhatorIcon->bHasIndicator)
		{
			bSubDockHasIndicator = TRUE;
		}
		else
		{
			GList* ic;
			Icon *icon;
			for (ic = pInhibatorDock->icons ; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if (icon->bHasIndicator)
				{
					bSubDockHasIndicator = TRUE;
					break;
				}
			}
		}
		CairoDock *pParentDock = NULL;
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pInhibatorDock, &pParentDock);
		if (pPointingIcon != NULL && pPointingIcon->bHasIndicator != bSubDockHasIndicator)
		{
			cd_message ("  for the sub-dock %s : indicator <- %d", pPointingIcon->cName, bSubDockHasIndicator);
			pPointingIcon->bHasIndicator = bSubDockHasIndicator;
			if (pParentDock != NULL)
				cairo_dock_redraw_icon (pPointingIcon);
		}
	}
}

static GldiWindowActor *_gldi_appli_icon_detach_of_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, 0);

	const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
	Icon *pIcon;
	const GList *pElement;
	///gboolean bNeedsRedraw = FALSE;
	CairoDock *pParentDock;
	GldiWindowActor *pFirstFoundActor = NULL;
	for (pElement = pList; pElement != NULL; pElement = pElement->next)
	{
		pIcon = pElement->data;
		pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pIcon));
		if (pParentDock == NULL)  // not in a dock => nothing to do.
			continue;

		cd_debug ("detachment of the icon %s (%p)", pIcon->cName, pFirstFoundActor);
		gldi_icon_detach (pIcon);

		// if the icon was in the class sub-dock, check if it became empty
		if (pParentDock == cairo_dock_get_class_subdock (cClass))  // the icon was in the class sub-dock
		{
			if (pParentDock->icons == NULL)  // and it's now empty -> destroy it (and the class-icon pointing on it as well)
			{
				CairoDock *pMainDock = NULL;
				Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pParentDock, &pMainDock);
				/// TODO: register to the destroy event of the class sub-dock...
				cairo_dock_destroy_class_subdock (cClass);  // destroy it before the class-icon, since it will destroy the sub-dock
				if (pMainDock && CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pPointingIcon))
				{
					gldi_icon_detach (pPointingIcon);
					gldi_object_unref (GLDI_OBJECT(pPointingIcon));
				}
			}
		}

		if (pFirstFoundActor == NULL)  // we grab the 1st app of this class.
		{
			pFirstFoundActor = pIcon->pAppli;
		}
	}
	return pFirstFoundActor;
}
gboolean cairo_dock_inhibite_class (const gchar *cClass, Icon *pInhibitorIcon)
{
	g_return_val_if_fail (cClass != NULL, FALSE);
	cd_message ("%s (%s)", __func__, cClass);

	// add inhibitor to class (first, so that applis can find it and take its surface if necessary)
	if (! _cairo_dock_add_inhibitor_to_class (cClass, pInhibitorIcon))
		return FALSE;

	// set class name on the inhibitor if not already done.
	if (pInhibitorIcon && pInhibitorIcon->cClass != cClass)
	{
		g_free (pInhibitorIcon->cClass);
		pInhibitorIcon->cClass = g_strdup (cClass);
	}

	// if launchers are mixed with applis, steal applis icons.
	if (!myTaskbarParam.bMixLauncherAppli)
		return TRUE;
	GldiWindowActor *pFirstFoundActor = _gldi_appli_icon_detach_of_class (cClass);  // detach existing applis, and then retach them to the inhibitor.
	if (pInhibitorIcon != NULL)
	{
		// inhibitor takes control of the first existing appli of the class.
		gldi_icon_set_appli (pInhibitorIcon, pFirstFoundActor);
		pInhibitorIcon->bHasIndicator = (pFirstFoundActor != NULL);
		_cairo_dock_set_same_indicator_on_sub_dock (pInhibitorIcon);
		
		if (pFirstFoundActor)
		{
			// update the name of the icon
			if (pInhibitorIcon->cInitialName == NULL)
				pInhibitorIcon->cInitialName = pInhibitorIcon->cName;
			else
				g_free (pInhibitorIcon->cName);
			pInhibitorIcon->cName = g_strdup (pFirstFoundActor->cName ? pFirstFoundActor->cName : cClass);
		}

		// other applis icons are retached to the inhibitor.
		const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
		Icon *pIcon;
		const GList *pElement;
		for (pElement = pList; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			cd_debug (" an app is detached (%s)", pIcon->cName);
			if (pIcon->pAppli != pFirstFoundActor && cairo_dock_get_icon_container (pIcon) == NULL)  // detached and has to be re-attached.
				gldi_appli_icon_insert_in_dock (pIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
		}
	}

	return TRUE;
}

gboolean cairo_dock_class_is_inhibited (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->pIconsOfClass != NULL);
}

gboolean cairo_dock_class_is_using_xicon (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	// if pClassAppli == NULL, there is no launcher able to give its icon but we can found one in icons theme of the system.
	return (pClassAppli != NULL && pClassAppli->bUseXIcon);
}

gboolean cairo_dock_class_is_expanded (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->bExpand);
}

gboolean cairo_dock_prevent_inhibited_class (Icon *pIcon)
{
	g_return_val_if_fail (pIcon != NULL, FALSE);
	//g_print ("%s (%s)\n", __func__, pIcon->cClass);

	gboolean bToBeInhibited = FALSE;
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	if (pClassAppli != NULL)
	{
		Icon *pInhibitorIcon;
		GList *pElement;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			if (pInhibitorIcon != NULL)  // an inhibitor is present.
			{
				if (pInhibitorIcon->pAppli == NULL && pInhibitorIcon->pSubDock == NULL)  // this icon inhibits this class but doesn't control any apps yet
				{
					gldi_icon_set_appli (pInhibitorIcon, pIcon->pAppli);
					cd_message (">>> %s will take an indicator during the next redraw ! (pAppli : %p)", pInhibitorIcon->cName, pInhibitorIcon->pAppli);
					pInhibitorIcon->bHasIndicator = TRUE;
					_cairo_dock_set_same_indicator_on_sub_dock (pInhibitorIcon);
				/**}

				if (pInhibitorIcon->pAppli == pIcon->pAppli)  // this icon controls us.
				{*/
					CairoDock *pInhibatorDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibitorIcon));
					//\______________ We place the icon for X.
					if (! bToBeInhibited)  // we put the thumbnail only on the 1st one.
					{
						if (pInhibatorDock != NULL)
						{
							//g_print ("we move the thumbnail on the inhibitor %s\n", pInhibitorIcon->cName);
							gldi_appli_icon_set_geometry_for_window_manager (pInhibitorIcon, pInhibatorDock);
						}
					}
					//\______________ We update inhibitor's label.
					if (pInhibatorDock != NULL && pIcon->cName != NULL)
					{
						if (pInhibitorIcon->cInitialName == NULL)
							pInhibitorIcon->cInitialName = pInhibitorIcon->cName;
						else
							g_free (pInhibitorIcon->cName);
						pInhibitorIcon->cName = NULL;
						gldi_icon_set_name (pInhibitorIcon, pIcon->cName);
					}
				}
				bToBeInhibited = (pInhibitorIcon->pAppli == pIcon->pAppli);
			}
		}
	}
	return bToBeInhibited;
}


static void _cairo_dock_remove_icon_from_class (Icon *pInhibitorIcon)
{
	g_return_if_fail (pInhibitorIcon != NULL);
	cd_message ("%s (%s)", __func__, pInhibitorIcon->cClass);

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pInhibitorIcon->cClass);
	if (pClassAppli != NULL)
	{
		pClassAppli->pIconsOfClass = g_list_remove (pClassAppli->pIconsOfClass, pInhibitorIcon);
	}
}

void cairo_dock_deinhibite_class (const gchar *cClass, Icon *pInhibitorIcon)
{
	cd_message ("%s (%s)", __func__, cClass);
	_cairo_dock_remove_icon_from_class (pInhibitorIcon);

	if (pInhibitorIcon != NULL && pInhibitorIcon->pSubDock != NULL && pInhibitorIcon->pSubDock == cairo_dock_get_class_subdock (cClass))  // the launcher is controlling several appli icons, place them back in the taskbar.
	{
		// first destroy the class sub-dock, so that the appli icons won't go inside again.
		// we empty the sub-dock then destroy it, then re-insert the appli icons
		GList *icons = pInhibitorIcon->pSubDock->icons;
		pInhibitorIcon->pSubDock->icons = NULL;  // empty the sub-dock
		cairo_dock_destroy_class_subdock (cClass);  // destroy the sub-dock without destroying its icons
		pInhibitorIcon->pSubDock = NULL;  // since the inhibitor can already be detached, the sub-dock can't find it

		Icon *pAppliIcon;
		GList *ic;
		for (ic = icons; ic != NULL; ic = ic->next)
		{
			pAppliIcon = ic->data;
			cairo_dock_set_icon_container (pAppliIcon, NULL);  // manually "detach" it
		}

		// then re-insert the appli icons.
		for (ic = icons; ic != NULL; ic = ic->next)
		{
			pAppliIcon = ic->data;
			gldi_appli_icon_insert_in_dock (pAppliIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
		}
		g_list_free (icons);

		cairo_dock_trigger_load_icon_buffers (pInhibitorIcon);  // in case the inhibitor was drawn with an emblem or a stack of the applis
	}

	if (pInhibitorIcon == NULL || pInhibitorIcon->pAppli != NULL)  // the launcher is controlling 1 appli icon, or we deinhibate all the inhibitors.
	{
		const GList *pList = cairo_dock_list_existing_appli_with_class (cClass);
		Icon *pIcon;
		///gboolean bNeedsRedraw = FALSE;
		///CairoDock *pParentDock;
		const GList *pElement;
		for (pElement = pList; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			if (pInhibitorIcon == NULL || pIcon->pAppli == pInhibitorIcon->pAppli)
			{
				cd_message ("re-add the icon previously inhibited (pAppli:%p)", pIcon->pAppli);
				pIcon->fInsertRemoveFactor = 0;
				pIcon->fScale = 1.;
				/**pParentDock = */
				gldi_appli_icon_insert_in_dock (pIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
				///bNeedsRedraw = (pParentDock != NULL && pParentDock->bIsMainDock);
			}
			///cairo_dock_reload_icon_image (pIcon, cairo_dock_get_icon_container (pIcon));  /// question : why should we do that for all icons?...
		}
		///if (bNeedsRedraw)
		///	gtk_widget_queue_draw (g_pMainDock->container.pWidget);  /// pDock->pRenderer->calculate_icons (pDock); ?...
	}
	if (pInhibitorIcon != NULL)
	{
		cd_message (" the inhibitor has lost everything");
		gldi_icon_unset_appli (pInhibitorIcon);
		pInhibitorIcon->bHasIndicator = FALSE;
		g_free (pInhibitorIcon->cClass);
		pInhibitorIcon->cClass = NULL;
		cd_debug ("  no more classes");
	}
}


void gldi_window_detach_from_inhibitors (GldiWindowActor *pAppli)
{
	const gchar *cClass = pAppli->cClass;
	cd_message ("%s (%s)", __func__, cClass);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GldiWindowActor *pNextAppli = NULL;  // next window that will be inhibited.
		gboolean bFirstSearch = TRUE;
		Icon *pSameClassIcon = NULL;
		Icon *pIcon;
		GList *pElement;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pIcon = pElement->data;
			if (pIcon->pAppli == pAppli)  // this inhibitor controls the given window -> make it control another (possibly none).
			{
				// find the next inhibited appli
				if (bFirstSearch)  // we didn't search the next window yet, do it now.
				{
					bFirstSearch = FALSE;
					Icon *pOneIcon;
					GList *ic;
					for (ic = g_list_last (pClassAppli->pAppliOfClass); ic != NULL; ic = ic->prev)  // reverse order, to take the oldest window of this class.
					{
						pOneIcon = ic->data;
						if (pOneIcon != NULL
						&& pOneIcon->pAppli != NULL
						&& pOneIcon->pAppli != pAppli  // not the window we precisely want to avoid
						&& (! myTaskbarParam.bAppliOnCurrentDesktopOnly || gldi_window_is_on_current_desktop (pOneIcon->pAppli)))  // can actually be displayed
						{
							pSameClassIcon = pOneIcon;
							break ;
						}
					}
					pNextAppli = (pSameClassIcon != NULL ? pSameClassIcon->pAppli : NULL);
					if (pSameClassIcon != NULL)  // this icon will be inhibited, we need to detach it if needed
					{
						cd_message ("  it's %s which will replace it", pSameClassIcon->cName);
						gldi_icon_detach (pSameClassIcon);  // it can't be the class sub-dock, because pIcon had the window actor, so it doesn't hold the class sub-dock and the class is not grouped (otherwise they would all be in the class sub-dock).
					}
				}

				// make the icon inhibits the next appli (possibly none)
				gldi_icon_set_appli (pIcon, pNextAppli);
				pIcon->bHasIndicator = (pNextAppli != NULL);
				_cairo_dock_set_same_indicator_on_sub_dock (pIcon);
				if (pNextAppli == NULL)
					gldi_icon_set_name (pIcon, pIcon->cInitialName);
				cd_message (" %s : bHasIndicator <- %d, pAppli <- %p", pIcon->cName, pIcon->bHasIndicator, pIcon->pAppli);

				// redraw
				GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
				if (pContainer)
					gtk_widget_queue_draw (pContainer->pWidget);
			}
		}
	}
}

static void _cairo_dock_remove_all_applis_from_class (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, G_GNUC_UNUSED gpointer data)
{
	g_list_free (pClassAppli->pAppliOfClass);
	pClassAppli->pAppliOfClass = NULL;

	Icon *pInhibitorIcon;
	GList *pElement;
	for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
	{
		pInhibitorIcon = pElement->data;
		pInhibitorIcon->bHasIndicator = FALSE;
		gldi_icon_unset_appli (pInhibitorIcon);
		_cairo_dock_set_same_indicator_on_sub_dock (pInhibitorIcon);
	}
}
void cairo_dock_remove_all_applis_from_class_table (void)  // for the stop_application_manager
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_remove_all_applis_from_class, NULL);
}

void cairo_dock_reset_class_table (void)
{
	g_hash_table_remove_all (s_hClassTable);
	g_hash_table_remove_all (s_hAltClass);
	// gldi_desktop_file_db_stop (); -- TODO: call this when exiting, but not when loading a new theme !!
}


cairo_surface_t *cairo_dock_create_surface_from_class (const gchar *cClass, int iWidth, int iHeight)
{
	cd_debug ("%s (%s)", __func__, cClass);
	// first we try to get an icon from one of the inhibitor.
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		cd_debug ("bUseXIcon:%d", pClassAppli->bUseXIcon);
		if (pClassAppli->bUseXIcon)
			return NULL;

		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;
			cd_debug ("  %s", pInhibitorIcon->cName);
			if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon))
			{
				if (pInhibitorIcon->pSubDock == NULL || myIndicatorsParam.bUseClassIndic)  // in the case where a launcher has more than one instance of its class and which represents the stack, we doesn't take the icon.
				{
					cd_debug ("%s will give its surface", pInhibitorIcon->cName);
					return cairo_dock_duplicate_surface (pInhibitorIcon->image.pSurface,
						pInhibitorIcon->image.iWidth,
						pInhibitorIcon->image.iHeight,
						iWidth,
						iHeight);
				}
				else if (pInhibitorIcon->cFileName != NULL)
				{
					gchar *cIconFilePath = cairo_dock_search_icon_s_path (pInhibitorIcon->cFileName, MAX (iWidth, iHeight));
					if (cIconFilePath != NULL)
					{
						cd_debug ("we replace X icon by %s", cIconFilePath);
						cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
							iWidth,
							iHeight);
						g_free (cIconFilePath);
						if (pSurface)
							return pSurface;
					}
				}
			}
		}
	}

	// if we didn't find one, we use the icon defined in the class.
	if (pClassAppli != NULL && pClassAppli->cIcon != NULL)
	{
		cd_debug ("get the class icon (%s)", pClassAppli->cIcon);
		gchar *cIconFilePath = cairo_dock_search_icon_s_path (pClassAppli->cIcon, MAX (iWidth, iHeight));
		cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
			iWidth,
			iHeight);
		g_free (cIconFilePath);
		if (pSurface)
			return pSurface;
	}
	else
	{
		cd_debug ("no icon for the class %s", cClass);
	}

	// if not found or not defined, try to find an icon based on the name class.
	gchar *cIconFilePath = cairo_dock_search_icon_s_path (cClass, MAX (iWidth, iHeight));
	if (cIconFilePath != NULL)
	{
		cd_debug ("we replace the X icon by %s", cIconFilePath);
		cairo_surface_t *pSurface = cairo_dock_create_surface_from_image_simple (cIconFilePath,
			iWidth,
			iHeight);
		g_free (cIconFilePath);
		if (pSurface)
			return pSurface;
	}

	cd_debug ("class %s will take the X icon", cClass);
	return NULL;
}

/**
void cairo_dock_update_visibility_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli, gboolean bIsHidden)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == pAppli)
			{
				cd_debug (" %s also %s itself", pInhibitorIcon->cName, (bIsHidden ? "hide" : "show"));
				if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon) && myTaskbarParam.fVisibleAppliAlpha != 0)
				{
					pInhibitorIcon->fAlpha = 1;  // we cheat a bit.
					cairo_dock_redraw_icon (pInhibitorIcon);
				}
			}
		}
	}
}

void cairo_dock_update_activity_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == pAppli)
			{
				cd_debug (" %s becomes active too", pInhibitorIcon->cName);
				CairoDock *pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibitorIcon));
				if (pParentDock != NULL)
					gldi_appli_icon_animate_on_active (pInhibitorIcon, pParentDock);
			}
		}
	}
}

void cairo_dock_update_inactivity_on_inhibitors (const gchar *cClass, GldiWindowActor *pAppli)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == pAppli)
			{
				cairo_dock_redraw_icon (pInhibitorIcon);
			}
		}
	}
}

void cairo_dock_update_name_on_inhibitors (const gchar *cClass, GldiWindowActor *actor, gchar *cNewName)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == actor)
			{
				if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon))
				{
					cd_debug (" %s change its name to %s", pInhibitorIcon->cName, cNewName);
					if (pInhibitorIcon->cInitialName == NULL)
					{
						pInhibitorIcon->cInitialName = pInhibitorIcon->cName;
						cd_debug ("pInhibitorIcon->cInitialName <- %s", pInhibitorIcon->cInitialName);
					}
					else
						g_free (pInhibitorIcon->cName);
					pInhibitorIcon->cName = NULL;

					gldi_icon_set_name (pInhibitorIcon, (cNewName != NULL ? cNewName : pInhibitorIcon->cInitialName));
				}
				cairo_dock_redraw_icon (pInhibitorIcon);
			}
		}
	}
}
*/
void gldi_window_foreach_inhibitor (GldiWindowActor *actor, GldiIconRFunc callback, gpointer data)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (actor->cClass);
	if (pClassAppli != NULL)
	{
		Icon *pInhibitorIcon;
		GList *ic;
		for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
		{
			pInhibitorIcon = ic->data;
			if (pInhibitorIcon->pAppli == actor)
			{
				if (! callback (pInhibitorIcon, data))
					break;
			}
		}
	}
}


Icon *cairo_dock_get_classmate (Icon *pIcon)  // gets an icon of the same class, that is inside a dock (or will be for an inhibitor), but not inside the class sub-dock
{
	cd_debug ("%s (%s)", __func__, pIcon->cClass);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	Icon *pFriendIcon = NULL;
	GList *pElement;
	for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
	{
		pFriendIcon = pElement->data;
		/// TODO: this is ugly... maybe an inhibitor shouldn't inhibit when not yet in a dock...
		if (pFriendIcon == NULL || (cairo_dock_get_icon_container(pFriendIcon) == NULL && pFriendIcon->cParentDockName == NULL))  // if not inside a dock (for instance a detached applet, but not a hidden launcher), ignore.
			continue ;
		cd_debug (" friend : %s", pFriendIcon->cName);
		if (pFriendIcon->pAppli != NULL || pFriendIcon->pSubDock != NULL)  // is linked to a window, 1 or several times (window actor or class sub-dock).
			return pFriendIcon;
	}

	GldiContainer *pClassSubDock = CAIRO_CONTAINER(cairo_dock_get_class_subdock (pIcon->cClass));
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL; pElement = pElement->next)
	{
		pFriendIcon = pElement->data;
		if (pFriendIcon == pIcon)  // skip ourselves
			continue ;

		if (cairo_dock_get_icon_container (pFriendIcon) != NULL && cairo_dock_get_icon_container (pFriendIcon) != pClassSubDock)  // inside a dock, but not the class sub-dock
			return pFriendIcon;
	}

	return NULL;
}



gboolean cairo_dock_check_class_subdock_is_empty (CairoDock *pDock, const gchar *cClass)
{
	cd_debug ("%s (%s, %d)", __func__, cClass, g_list_length (pDock->icons));
	if (pDock->iRefCount == 0)
		return FALSE;
	if (pDock->icons == NULL)  // shouldn't happen, handle this case and make some noise.
	{
		cd_warning ("the %s class sub-dock has no element, which is probably an error !\nit will be destroyed.", cClass);
		Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
		cairo_dock_destroy_class_subdock (cClass);
		pFakeClassIcon->pSubDock = NULL;
		if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pFakeClassIcon))
		{
			gldi_icon_detach (pFakeClassIcon);
			gldi_object_unref (GLDI_OBJECT(pFakeClassIcon));
		}
		return TRUE;
	}
	else if (pDock->icons->next == NULL)  // only 1 icon left in the sub-dock -> destroy it.
	{
		cd_debug ("   the sub-dock of the class %s has now only one item in it: will be destroyed", cClass);
		Icon *pLastClassIcon = pDock->icons->data;

		CairoDock *pFakeParentDock = NULL;
		Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeParentDock);
		g_return_val_if_fail (pFakeClassIcon != NULL, TRUE);

		// detach the last icon from the class sub-dock
		gboolean bLastIconIsRemoving = cairo_dock_icon_is_being_removed (pLastClassIcon);  // keep the removing state because when we detach the icon, it returns to normal state.
		gldi_icon_detach (pLastClassIcon);
		pLastClassIcon->fOrder = pFakeClassIcon->fOrder;  // if re-inserted in a dock, insert at the same place

		// destroy the class sub-dock
		cairo_dock_destroy_class_subdock (cClass);
		pFakeClassIcon->pSubDock = NULL;

		if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pFakeClassIcon))  // the class sub-dock is pointed by a class-icon
		{
			// destroy the class-icon
			gldi_icon_detach (pFakeClassIcon);
			gldi_object_unref (GLDI_OBJECT(pFakeClassIcon));

			// re-insert the last icon in place of it, or destroy it if it was being removed
			if (! bLastIconIsRemoving)
			{
				gldi_icon_insert_in_container (pLastClassIcon, CAIRO_CONTAINER(pFakeParentDock), ! CAIRO_DOCK_ANIMATE_ICON);
			}
			else  // the last icon is being removed, no need to re-insert it (e.g. when we close all classes in one it)
			{
				cd_debug ("no need to re-insert the last icon");
				gldi_object_unref (GLDI_OBJECT(pLastClassIcon));
			}
		}
		else  // the class sub-dock is pointed by a launcher/applet
		{
			// re-inhibit the last icon or destroy it if it was being removed
			if (! bLastIconIsRemoving)
			{
				gldi_appli_icon_insert_in_dock (pLastClassIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);  // Note that we could optimize and manually set the appli and the name...
				///cairo_dock_update_name_on_inhibitors (cClass, pLastClassIcon->pAppli, pLastClassIcon->cName);
			}
			else  // the last icon is being removed, no need to re-insert it
			{
				pFakeClassIcon->bHasIndicator = FALSE;
				gldi_object_unref (GLDI_OBJECT(pLastClassIcon));
			}
			cairo_dock_redraw_icon (pFakeClassIcon);
		}
		cd_debug ("no more dock");
		return TRUE;
	}
	return FALSE;
}

static void _cairo_dock_update_class_subdock_name (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, gconstpointer *data)
{
	const CairoDock *pDock = data[0];
	const gchar *cNewName = data[1];
	if (g_strcmp0 (pClassAppli->cDockName, pDock->cDockName) == 0)  // this class uses this dock
	{
		g_free (pClassAppli->cDockName);
		pClassAppli->cDockName = g_strdup (cNewName);
	}
}
void cairo_dock_update_class_subdock_name (const CairoDock *pDock, const gchar *cNewName)  // update the subdock name of the class that holds 'pDock' as its class subdock
{
	gconstpointer data[2] = {pDock, cNewName};
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_update_class_subdock_name, data);
}


static void _cairo_dock_reset_overwrite_exceptions (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, G_GNUC_UNUSED gpointer data)
{
	pClassAppli->bUseXIcon = FALSE;
}
void cairo_dock_set_overwrite_exceptions (const gchar *cExceptions)
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_reset_overwrite_exceptions, NULL);
	if (cExceptions == NULL)
		return;

	gchar **cClassList = g_strsplit (cExceptions, ";", -1);
	if (cClassList == NULL || cClassList[0] == NULL || *cClassList[0] == '\0')
	{
		g_strfreev (cClassList);
		return;
	}
	CairoDockClassAppli *pClassAppli;
	int i;
	for (i = 0; cClassList[i] != NULL; i ++)
	{
		pClassAppli = _cairo_dock_lookup_class_appli (cClassList[i]);
		if (pClassAppli) pClassAppli->bUseXIcon = TRUE;
	}

	g_strfreev (cClassList);
}

static void _cairo_dock_reset_group_exceptions (G_GNUC_UNUSED gchar *cClass, CairoDockClassAppli *pClassAppli, G_GNUC_UNUSED gpointer data)
{
	pClassAppli->bExpand = FALSE;
}
void cairo_dock_set_group_exceptions (const gchar *cExceptions)
{
	g_hash_table_foreach (s_hClassTable, (GHFunc) _cairo_dock_reset_group_exceptions, NULL);
	if (cExceptions == NULL)
		return;

	gchar **cClassList = g_strsplit (cExceptions, ";", -1);
	if (cClassList == NULL || cClassList[0] == NULL || *cClassList[0] == '\0')
	{
		g_strfreev (cClassList);
		return;
	}
	CairoDockClassAppli *pClassAppli;
	int i;
	for (i = 0; cClassList[i] != NULL; i ++)
	{
		pClassAppli = _cairo_dock_lookup_class_appli (cClassList[i]);
		if (pClassAppli) pClassAppli->bExpand = TRUE;
	}

	g_strfreev (cClassList);
}


Icon *cairo_dock_get_prev_next_classmate_icon (Icon *pIcon, gboolean bNext)
{
	cd_debug ("%s (%s, %s)", __func__, pIcon->cClass, pIcon->cName);
	g_return_val_if_fail (pIcon->cClass != NULL, NULL);

	Icon *pActiveIcon = cairo_dock_get_current_active_icon ();
	if (pActiveIcon == NULL || pActiveIcon->cClass == NULL || strcmp (pActiveIcon->cClass, pIcon->cClass) != 0)  // the active window is not from our class, we active the icon given in parameter.
	{
		cd_debug ("Active icon's class: %s", pIcon->cClass);
		return pIcon;
	}

	//\________________ We are looking in the class of the active window and take the next or previous one.
	Icon *pNextIcon = NULL;
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	//\________________ We are looking in icons of apps.
	Icon *pClassmateIcon;
	GList *pElement, *ic;
	for (pElement = pClassAppli->pAppliOfClass; pElement != NULL && pNextIcon == NULL; pElement = pElement->next)
	{
		pClassmateIcon = pElement->data;
		cd_debug (" %s is it active?", pClassmateIcon->cName);
		if (pClassmateIcon->pAppli == pActiveIcon->pAppli)  // the active window.
		{
			cd_debug ("  found an active window (%s; %p)", pClassmateIcon->cName, pClassmateIcon->pAppli);
			if (bNext)  // take the 1st non null after that.
			{
				ic = pElement;
				do
				{
					ic = cairo_dock_get_next_element (ic, pClassAppli->pAppliOfClass);
					if (ic == pElement)
					{
						cd_debug ("  found nothing!");
						break ;
					}
					pClassmateIcon = ic->data;
					if (pClassmateIcon != NULL && pClassmateIcon->pAppli != NULL)
					{
						cd_debug ("  we take this one (%s; %p)", pClassmateIcon->cName, pClassmateIcon->pAppli);
						pNextIcon = pClassmateIcon;
						break ;
					}
				}
				while (1);
			}
			else  // we take the first non null before it.
			{
				ic = pElement;
				do
				{
					ic = cairo_dock_get_previous_element (ic, pClassAppli->pAppliOfClass);
					if (ic == pElement)
						break ;
					pClassmateIcon = ic->data;
					if (pClassmateIcon != NULL && pClassmateIcon->pAppli != NULL)
					{
						pNextIcon = pClassmateIcon;
						break ;
					}
				}
				while (1);
			}
			break ;
		}
	}
	return pNextIcon;
}



Icon *cairo_dock_get_inhibitor (Icon *pIcon, gboolean bOnlyInDock)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	if (pClassAppli != NULL)
	{
		GList *pElement;
		Icon *pInhibitorIcon;
		for (pElement = pClassAppli->pIconsOfClass; pElement != NULL; pElement = pElement->next)
		{
			pInhibitorIcon = pElement->data;

			if (pInhibitorIcon->pAppli == pIcon->pAppli)
			{
				if (! bOnlyInDock || cairo_dock_get_icon_container (pInhibitorIcon) != NULL)
					return pInhibitorIcon;
			}
		}
	}
	return NULL;
}

static inline double _get_previous_order (GList *ic)
{
	if (ic == NULL)
		return 0;
	double fOrder;
	Icon *icon = ic->data;
	Icon *prev_icon = (ic->prev ? ic->prev->data : NULL);
	if (prev_icon != NULL && cairo_dock_get_icon_order (prev_icon) == cairo_dock_get_icon_order (icon))

		fOrder = (icon->fOrder + prev_icon->fOrder) / 2;
	else
		fOrder = icon->fOrder - 1;
	return fOrder;
}
static inline double _get_next_order (GList *ic)
{
	if (ic == NULL)
		return 0;
	double fOrder;
	Icon *icon = ic->data;
	Icon *next_icon = (ic->next ? ic->next->data : NULL);
	if (next_icon != NULL && cairo_dock_get_icon_order (next_icon) == cairo_dock_get_icon_order (icon))
		fOrder = (icon->fOrder + next_icon->fOrder) / 2;
	else
		fOrder = icon->fOrder + 1;
	return fOrder;
}
static inline double _get_first_appli_order (CairoDock *pDock, GList *first_launcher_ic, GList *last_launcher_ic)
{
	double fOrder;
	switch (myTaskbarParam.iIconPlacement)
	{
		case CAIRO_APPLI_BEFORE_FIRST_ICON:
			fOrder = _get_previous_order (pDock->icons);
		break;

		case CAIRO_APPLI_BEFORE_FIRST_LAUNCHER:
			if (first_launcher_ic != NULL)
			{
				//g_print (" go just before the first launcher (%s)\n", ((Icon*)first_launcher_ic->data)->cName);
				fOrder = _get_previous_order (first_launcher_ic);  // 'first_launcher_ic' includes the separators, so we can just take the previous order.
			}
			else  // no launcher, go to the beginning of the dock.
			{
				fOrder = _get_previous_order (pDock->icons);
			}
		break;

		case CAIRO_APPLI_AFTER_ICON:
		{
			Icon *icon;
			GList *ic = NULL;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if ((icon->cDesktopFileName != NULL && g_strcmp0 (icon->cDesktopFileName, myTaskbarParam.cRelativeIconName) == 0)
				|| (icon->pModuleInstance && g_strcmp0 (icon->pModuleInstance->cConfFilePath, myTaskbarParam.cRelativeIconName) == 0))
					break;
			}

			if (ic != NULL)  // icon found
			{
				fOrder = _get_next_order (ic);
				break;
			}  // else don't break, and go to the 'CAIRO_APPLI_AFTER_LAST_LAUNCHER' case, which will be the fallback.
		}
		/* fall through */

		case CAIRO_APPLI_AFTER_LAST_LAUNCHER:
		default:
			if (last_launcher_ic != NULL)
			{
				//g_print (" go just after the last launcher (%s)\n", ((Icon*)last_launcher_ic->data)->cName);
				fOrder = _get_next_order (last_launcher_ic);
			}
			else  // no launcher, go to the beginning of the dock.
			{
				fOrder = _get_previous_order (pDock->icons);
			}
		break;

		case CAIRO_APPLI_AFTER_LAST_ICON:
			fOrder = _get_next_order (g_list_last (pDock->icons));
		break;
	}
	return fOrder;
}
static inline int _get_class_age (CairoDockClassAppli *pClassAppli)
{
	if (pClassAppli->pAppliOfClass == NULL)
		return 0;
	return pClassAppli->iAge;
}
// Set the order of an appli when they are mixed amongst launchers and no class sub-dock exists (because either they are not grouped by class, or just it's the first appli of this class in the dock)
// First try to see if an inhibitor is present in the dock; if not, see if an appli of the same class is present in the dock.
// -> if yes, place it next to it, ordered by age (go to the right until our age is greater)
// -> if no, place it amongst the other appli icons, ordered by age (search the last launcher, skip any automatic separator, and then go to the right until our age is greater or there is no more appli).
void cairo_dock_set_class_order_in_dock (Icon *pIcon, CairoDock *pDock)
{
	//g_print ("%s (%s, %d)\n", __func__, pIcon->cClass, pIcon->iAge);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_if_fail (pClassAppli != NULL);

	// Look for an icon of the same class in the dock, to place ourself relatively to it.
	Icon *pSameClassIcon = NULL;
	GList *same_class_ic = NULL;

	// First look for an inhibitor of this class, preferably a launcher.
	CairoDock *pParentDock;
	Icon *pInhibitorIcon;
	GList *ic;
	for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
	{
		pInhibitorIcon = ic->data;

		pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibitorIcon));
		if (! GLDI_OBJECT_IS_DOCK(pParentDock))  // not inside a dock, for instance a desklet (or a hidden launcher) -> skip
			continue;
		pSameClassIcon = pInhibitorIcon;
		same_class_ic = ic;
		//g_print (" found an inhibitor of this class: %s (%d)\n", pSameClassIcon->cName, pSameClassIcon->iAge);
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pSameClassIcon))  // it's a launcher, we wont't find better -> quit
			break ;
	}

	// if no inhibitor found, look for an appli of this class in the dock.
	if (pSameClassIcon == NULL)
	{
		Icon *pAppliIcon;
		for (ic = g_list_last (pClassAppli->pAppliOfClass); ic != NULL; ic = ic->prev)  // check the older icons first (prepend), because then we'll place ourself to their right.
		{
			pAppliIcon = ic->data;
			if (pAppliIcon == pIcon)  // skip ourself
				continue;
			pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pAppliIcon));
			if (pParentDock != NULL)
			{
				pSameClassIcon = pAppliIcon;
				same_class_ic = ic;
				//g_print (" found an appli of this class: %s (%d)\n", pSameClassIcon->cName, pSameClassIcon->iAge);
				break ;
			}
		}
		pIcon->iGroup = (myTaskbarParam.bSeparateApplis ? CAIRO_DOCK_APPLI : CAIRO_DOCK_LAUNCHER);  // no inhibitor, so we'll go in the taskbar group.
	}
	else  // an inhibitor is present, we'll go next to it, so we'll be in its group.
	{
		pIcon->iGroup = pSameClassIcon->iGroup;
	}

	// if we found one, place next to it, ordered by age amongst the other appli of this class already in the dock.
	if (pSameClassIcon != NULL)
	{
		same_class_ic = g_list_find (pDock->icons, pSameClassIcon);
		g_return_if_fail (same_class_ic != NULL);
		Icon *pNextIcon = NULL;  // the next icon after all the icons of our class, or NULL if we reach the end of the dock.
		for (ic = same_class_ic->next; ic != NULL; ic = ic->next)
		{
			pNextIcon = ic->data;
			//g_print ("  next icon: %s (%d)\n", pNextIcon->cName, pNextIcon->iAge);
			if (!pNextIcon->cClass || strcmp (pNextIcon->cClass, pIcon->cClass) != 0)  // not our class any more, quit.
				break;

			if (pIcon->pAppli->iAge > pNextIcon->pAppli->iAge)  // we are more recent than this icon -> place on its right -> continue
			{
				pSameClassIcon = pNextIcon;  // 'pSameClassIcon' will be the last icon of our class older than us.
				pNextIcon = NULL;
			}
			else  // we are older than it -> go just before it -> quit
			{
				break;
			}
		}
		//g_print (" pNextIcon: %s (%d)\n", pNextIcon?pNextIcon->cName:"none", pNextIcon?pNextIcon->iAge:-1);

		if (pNextIcon != NULL && cairo_dock_get_icon_order (pNextIcon) == cairo_dock_get_icon_order (pSameClassIcon))  // the next icon is in thge09e same group as us: place it between this icon and pSameClassIcon.
			pIcon->fOrder = (pNextIcon->fOrder + pSameClassIcon->fOrder) / 2;
		else  // no icon after our class or in a different grou: we place just after pSameClassIcon.
			pIcon->fOrder = pSameClassIcon->fOrder + 1;

		return;
	}

	// if no icon of our class is present in the dock, place it amongst the other appli icons, after the first appli or after the launchers, and ordered by age.
	// search the last launcher and the first appli.
	Icon *icon;
	Icon *pFirstLauncher = NULL;
	GList *first_appli_ic = NULL, *last_launcher_ic = NULL, *first_launcher_ic = NULL;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)  // launcher, even without class
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon)  // container icon (likely to contain some launchers)
		|| (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && icon->pModuleInstance->pModule->pVisitCard->bActAsLauncher)  // applet acting like a launcher
		/**|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon)*/)  // separator (user or auto).
		{
			// pLastLauncher = icon;
			last_launcher_ic = ic;
			if (pFirstLauncher == NULL)
			{
				pFirstLauncher = icon;
				first_launcher_ic = ic;
			}
		}
		else if ((CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) || CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
		&& ! cairo_dock_class_is_inhibited (icon->cClass))  // an appli not placed next to its inhibitor.
		{
			// pFirstAppli = icon;
			first_appli_ic = ic;
			break ;
		}
	}
	//g_print (" last launcher: %s\n", pLastLauncher?pLastLauncher->cName:"none");
	//g_print (" first appli: %s\n", pFirstAppli?pFirstAppli->cName:"none");

	// place amongst the other applis, or after the last launcher.
	if (first_appli_ic != NULL)  // if an appli exists in the dock, use it as an anchor.
	{
		int iAge = _get_class_age (pClassAppli);  // the age of our class.

		GList *last_appli_ic = NULL;  // last appli whose class is older than ours => we'll go just after.
		for (ic = first_appli_ic; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			if (! CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) && ! CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
				break;

			// get the age of this class (= age of the oldest icon of this class)
			CairoDockClassAppli *pOtherClassAppli = _cairo_dock_lookup_class_appli (icon->cClass);
			if (! pOtherClassAppli || ! pOtherClassAppli->pAppliOfClass)  // should never happen
				continue;

			int iOtherClassAge = _get_class_age (pOtherClassAppli);
			//g_print (" age of class %s: %d\n", icon->cClass, iOtherClassAge);

			// compare to our class.
			if (iOtherClassAge < iAge)  // it's older than our class -> skip this whole class, we'll go after.
			{
				Icon *next_icon;
				while (ic->next != NULL)
				{
					next_icon = ic->next->data;
					if (next_icon->cClass && strcmp (next_icon->cClass, icon->cClass) == 0)  // next icon is of the same class -> skip
						ic = ic->next;
					else
						break;
				}
				last_appli_ic = ic;
			}
			else  // we are older -> discard and quit.
			{
				break;
			}
		}

		if (last_appli_ic == NULL)  // we are the oldest class -> go just before the first appli
		{
			//g_print (" we are the oldest class\n");
			pIcon->fOrder = _get_previous_order (first_appli_ic);
		}
		else  // go just after the last one
		{
			//g_print (" go just after %s\n", ((Icon*)last_appli_ic->data)->cName);
			pIcon->fOrder = _get_next_order (last_appli_ic);
		}
	}
	else  // no appli yet in the dock -> place it at the taskbar position defined in conf.
	{
		pIcon->fOrder = _get_first_appli_order (pDock, first_launcher_ic, last_launcher_ic);
	}
}

void cairo_dock_set_class_order_amongst_applis (Icon *pIcon, CairoDock *pDock)  // set the order of an appli amongst the other applis of a given dock (class sub-dock or main dock).
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (pIcon->cClass);
	g_return_if_fail (pClassAppli != NULL);

	// place the icon amongst the other appli icons of this class, or after the last appli if none.
	if (myTaskbarParam.bSeparateApplis)
		pIcon->iGroup = CAIRO_DOCK_APPLI;
	else
		pIcon->iGroup = CAIRO_DOCK_LAUNCHER;
	Icon *icon;
	GList *ic, *last_ic = NULL, *first_appli_ic = NULL;
	GList *last_launcher_ic = NULL, *first_launcher_ic = NULL;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon))
		{
			if (! first_appli_ic)
				first_appli_ic = ic;
			if (icon->cClass && strcmp (icon->cClass, pIcon->cClass) == 0)  // this icon is in our class.
			{
				if (!icon->pAppli || icon->pAppli->iAge < pIcon->pAppli->iAge)  // it's older than us => we are more recent => go after => continue. (Note: icon->pAppli can be NULL if the icon in the dock is being removed)
				{
					last_ic = ic;  // remember the last item of our class.
				}
				else  // we are older than it => go just before it.
				{
					pIcon->fOrder = _get_previous_order (ic);
					return ;
				}
			}
		}
		else if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)  // launcher, even without class
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon)  // container icon (likely to contain some launchers)
		|| (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && icon->cClass != NULL && icon->pModuleInstance->pModule->pVisitCard->bActAsLauncher)  // applet acting like a launcher
		|| (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon)))  // separator (user or auto).
		{
			last_launcher_ic = ic;
			if (first_launcher_ic == NULL)
			{
				first_launcher_ic = ic;
			}
		}
	}

	if (last_ic != NULL)  // there are some applis of our class, but none are more recent than us, so we are the most recent => go just after the last one we found previously.
	{
		pIcon->fOrder = _get_next_order (last_ic);
	}
	else  // we didn't find a single icon of our class => place amongst the other applis from age.
	{
		if (first_appli_ic != NULL)  // if an appli exists in the dock, use it as an anchor.
		{
			Icon *pOldestAppli = g_list_last (pClassAppli->pAppliOfClass)->data;  // prepend
			int iAge = pOldestAppli->pAppli->iAge;  // the age of our class.

			GList *last_appli_ic = NULL;  // last appli whose class is older than ours => we'll go just after.
			for (ic = first_appli_ic; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				if (! CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) && ! CAIRO_DOCK_IS_MULTI_APPLI (icon))
					break;

				// get the age of this class (= age of the oldest icon of this class)
				CairoDockClassAppli *pOtherClassAppli = _cairo_dock_lookup_class_appli (icon->cClass);
				if (! pOtherClassAppli || ! pOtherClassAppli->pAppliOfClass)  // should never happen
					continue;

				Icon *pOldestAppli = g_list_last (pOtherClassAppli->pAppliOfClass)->data;  // prepend

				// compare to our class.
				if (pOldestAppli->pAppli->iAge < iAge)  // it's older than our class -> skip this whole class, we'll go after.
				{
					while (ic->next != NULL)
					{
						icon = ic->next->data;
						if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon) && icon->cClass && strcmp (icon->cClass, pOldestAppli->cClass) == 0)  // next icon is an appli of the same class -> skip
							ic = ic->next;
						else
							break;
					}
					last_appli_ic = ic;
				}
				else  // we are older -> discard and quit.
				{
					break;
				}
			}

			if (last_appli_ic == NULL)  // we are the oldest class -> go just before the first appli
			{
				pIcon->fOrder = _get_previous_order (first_appli_ic);
			}
			else  // go just after the last one
			{
				pIcon->fOrder = _get_next_order (last_appli_ic);
			}
		}
		else  // no appli, use the defined placement.
		{
			pIcon->fOrder = _get_first_appli_order (pDock, first_launcher_ic, last_launcher_ic);
		}
	}
}


const gchar *cairo_dock_get_class_name (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return pClassAppli ? pClassAppli->cName : NULL;
}
const gchar *cairo_dock_get_class_desktop_file (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli && pClassAppli->app) ?
		g_desktop_app_info_get_filename (pClassAppli->app) : NULL;
}

const gchar *cairo_dock_get_class_icon (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return pClassAppli ? pClassAppli->cIcon : NULL;
}

const gchar *cairo_dock_get_class_wm_class (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);

	if (pClassAppli->cStartupWMClass == NULL)  // if the WMClass has not been retrieved beforehand, do it now
	{
		cd_debug ("retrieve WMClass for %s...", cClass);
		Icon *pIcon;
		GList *ic;
		for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->pAppli && pIcon->pAppli->cWmClass)
			{
				pClassAppli->cStartupWMClass = g_strdup (pIcon->pAppli->cWmClass);
				break;
			}
		}
	}

	return pClassAppli->cStartupWMClass;
}

GDesktopAppInfo *cairo_dock_get_class_app_info (const gchar *cClass)
{
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return pClassAppli ? pClassAppli->app : NULL;
}

const CairoDockImageBuffer *cairo_dock_get_class_image_buffer (const gchar *cClass)
{
	static CairoDockImageBuffer image;
	g_return_val_if_fail (cClass != NULL, NULL);
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	g_return_val_if_fail (pClassAppli != NULL, NULL);
	Icon *pIcon;
	GList *ic;
	for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon) && pIcon->image.pSurface)  // avoid applets
		{
			memcpy (&image, &pIcon->image, sizeof (CairoDockImageBuffer));
			return &image;
		}
	}
	for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->image.pSurface)
		{
			memcpy (&image, &pIcon->image, sizeof (CairoDockImageBuffer));
			return &image;
		}
	}

	return NULL;
}


/**
 * Attempt to find a desktop file for an app given its app-id, class, or desktop file path.
 * 
 * If cDesktopFile is a valid path, try to open it and return the
 * corresponding GDesktopAppInfo. If the return value is non-NULL,
 * it's reference count has been incremented already and the caller
 * must call g_object_unref() on it if it is no longer needed.
 * 
 * Alternatively, search our database of system-wide .desktop files,
 * including workarounds for several known inconsistencies (by adding
 * prefices). All comparisons are case-insensitive.
 * 
 * If bOnlyExact, only the given string is searched (both among filenames and
 * StartupWMClass keys), and heuristics are not attempted.
 */
static GDesktopAppInfo *_search_desktop_file (const gchar *cDesktopFile, gboolean bOnlyExact)
{
	if (cDesktopFile == NULL)
		return NULL;
	gchar *tmp_to_free = NULL;
	gboolean bPath = FALSE;
	if (*cDesktopFile == '/') 
	{
		if (g_file_test (cDesktopFile, G_FILE_TEST_EXISTS))  // it's a path and it exists.
		{
			GDesktopAppInfo *app = g_desktop_app_info_new (cDesktopFile);
			if (app) return app;
		}
		// if not found, try searching based on the basename
		tmp_to_free = g_path_get_basename (cDesktopFile);
		cDesktopFile = tmp_to_free;
		bPath = TRUE;
	}
	
	// remove the .desktop suffix and convert to lower case if it is present
	// note: if cDesktopFile does not end with ".desktop", then it has been
	// processed (and converted to lower case) before
	gchar *suffix = g_strrstr (cDesktopFile, ".desktop");
	if (suffix)
	{
		gchar *to_free2 = tmp_to_free;
		tmp_to_free = g_ascii_strdown (cDesktopFile, suffix - cDesktopFile);
		cDesktopFile = tmp_to_free;
		g_free (to_free2);
	}
	
	// normal case: we have the correct name
	// (if bPath == FALSE, we want to match a class, which can happen based on the .desktop file contents)
	GDesktopAppInfo *app = gldi_desktop_file_db_lookup (cDesktopFile, bPath);
	if (app || bPath || bOnlyExact)
	{
		// if it was found return it; also if the query was an absolute
		// path, we don't try the heuristics (we are only interested if
		// the exact same file is found in some other location)
		g_free (tmp_to_free); // can be NULL
		if (app) g_object_ref (app);
		return app; // can be NULL
	}
	
	// handle potential partial matches and special cases
	// #0: special casing for Gnome Terminal, required at least on Ubuntu 22.04 and 24.04
	// (should be fixed in newer versions, see e.g. here: https://gitlab.gnome.org/GNOME/gnome-terminal/-/issues/8033)
	if (!strcmp (cDesktopFile, "gnome-terminal-server"))
	{
		const char *tmpkey = "org.gnome.terminal";
		app = gldi_desktop_file_db_lookup (tmpkey, TRUE); // we want exact match for org.gnome.terminal.desktop
		if (app)
		{
			g_free (tmp_to_free); // can be NULL
			return g_object_ref (app);
		}
	}
	
	GString *sID = g_string_new (NULL);
	
	/* #1: add common prefices
	 * e.g. org.gnome.Evince.desktop, but app-id is only evince on Ubuntu 22.04 and 24.04
	 * More generally, this can happen with GTK+3 apps that have only "partially" migrated
	 * to using the "new" (reverse DNS style) .desktop format.
	 * See e.g.
	 * https://gitlab.gnome.org/GNOME/gtk/-/issues/2822
	 * https://gitlab.gnome.org/GNOME/gtk/-/issues/2034
	 * https://honk.sigxcpu.org/con/GTK__and_the_application_id.html
	 * https://docs.gtk.org/gtk4/migrating-3to4.html#set-a-proper-application-id
	 */
	const char *prefices[] = {"org.gnome.", "org.kde.", "org.freedesktop.", NULL};
	int j;
	
	for (j = 0; prefices[j]; j++)
	{
		g_string_printf (sID, "%s%s", prefices[j], cDesktopFile);
		app = gldi_desktop_file_db_lookup (sID->str, TRUE); // we want exact match for the file name
		if (app) break;
	}
	
	if (!app)
	{
		// #2: snap "namespaced" names -- these could be anything, we just handle the "common" case where
		// simply the app-id is duplicated (e.g. "firefox_firefox.desktop" as on Ubuntu 22.04 and 24.04)
		g_string_printf (sID, "%s_%s", cDesktopFile, cDesktopFile);
		app = gldi_desktop_file_db_lookup (sID->str, TRUE); // we want exact match for the file name
	}
	
	g_free (tmp_to_free); // can be null
	g_string_free(sID, TRUE);
	if (app) g_object_ref (app);
	return app;
}

gchar *cairo_dock_guess_class (const gchar *cCommand, const gchar *cStartupWMClass)
{
	// Several cases are possible:
	// Exec=toto
	// Exec=toto-1.2
	// Exec=toto -x -y
	// Exec=/path/to/toto -x -y
	// Exec=gksu nautilus /  (or kdesu)
	// Exec=su-to-root -X -c /usr/sbin/synaptic
	// Exec=gksu --description /usr/share/applications/synaptic.desktop /usr/sbin/synaptic
	// Exec=wine "C:\Program Files\Starcraft\Starcraft.exe"
	// Exec=wine "/path/to/prog.exe"
	// Exec=env WINEPREFIX="/home/fab/.wine" wine "C:\Program Files\Starcraft\Starcraft.exe"

	cd_debug ("%s (%s, '%s')", __func__, cCommand, cStartupWMClass);
	gchar *cResult = NULL;
	if (cStartupWMClass == NULL || *cStartupWMClass == '\0' || strcmp (cStartupWMClass, "Wine") == 0)  // special case for wine, because even if the class is defined as "Wine", this information is non-exploitable.
	{
		if (cCommand == NULL || *cCommand == '\0')
			return NULL;
		gchar *cDefaultClass = g_ascii_strdown (cCommand, -1);
		gchar *str;
		const gchar *cClass = cDefaultClass;  // pointer to the current class.

		if (strncmp (cClass, "gksu", 4) == 0 || strncmp (cClass, "kdesu", 5) == 0 || strncmp (cClass, "su-to-root", 10) == 0)  // we take the end
		{
			str = (gchar*)cClass + strlen(cClass) - 1;  // last char.
			while (*str == ' ')  // by security, we remove spaces at the end of the line.
				*(str--) = '\0';
			str = strchr (cClass, ' ');  // first whitespace.
			if (str != NULL)  // we are looking after that.
			{
				while (*str == ' ')
					str ++;
				cClass = str;
			}  // we remove gksu, kdesu, etc..
			if (*cClass == '-')  // if it's an option: we need the last param.
			{
				str = strrchr (cClass, ' ');  // last whitespace.
				if (str != NULL)  // we are looking after that.
					cClass = str + 1;
			}
			else  // we can use the first param
			{
				str = strchr (cClass, ' ');  // first whitespace.
				if (str != NULL)  // we remove everything after that
					*str = '\0';
			}

			str = strrchr (cClass, '/');  // last '/'.
			if (str != NULL)  // remove after that.
				cClass = str + 1;
		}
		else if ((str = g_strstr_len (cClass, -1, "wine ")) != NULL)
		{
			cClass = str;  // class = wine, better than nothing.
			*(str+4) = '\0';
			str += 5;
			while (*str == ' ')  // we remove extra whitespaces.
				str ++;
			// we try to find the executable which is used by wine as res_name.
			gchar *exe = g_strstr_len (str, -1, ".exe");
			if (!exe)
				exe = g_strstr_len (str, -1, ".EXE");
			if (exe)
			{
				*exe = '\0';  // remove the extension.
				gchar *slash = strrchr (str, '\\');
				if (slash)
					cClass = slash+1;
				else
				{
					slash = strrchr (str, '/');
					if (slash)
						cClass = slash+1;
					else
						cClass = str;
				}
			}
			cd_debug ("  special case : wine application => class = '%s'", cClass);
		}
		else
		{
			if (!strncmp (cClass, "env ", 4))
			{
				// e.g. env BAMF_DESKTOP_FILE_HINT=/var/lib/snapd/desktop/applications/firefox_firefox.desktop /snap/bin/firefox %u
				// we want to extract "firefox" from this
				gint argc;
				gchar **argv = NULL;
				gboolean parsed = g_shell_parse_argv (cClass, &argc, &argv, NULL);
				cClass = NULL;
				
				if (parsed)
				{
					int i;
					gboolean have_var = FALSE;
					for (i = 1; i < argc; i++)
					{
						if (strchr (argv[i], '=')) {
							// first argument that is likely actually setting an environment variable
							if (argv[i][0] != '-') have_var = TRUE;
						}
						else if (have_var)
						{
							// first argument which is not an environment variable
							// and likely not a command line option
							g_free (cDefaultClass);
							cDefaultClass = g_strdup (argv[i]);
							cClass = cDefaultClass;
							break;
						}
					}
				}
				
				g_strfreev (argv);
			}
			else if (!strncmp (cClass, "python3 ", 8) || !strncmp (cClass, "python ", 7))
			{
				cClass += 7;
				while (*cClass == ' ') cClass++;
				if (*cClass == 0 || *cClass == '-')
					cClass = NULL; // we cannot parse Python's arguments, do not attempt to do so
			}
			
			// TODO: handle sh -c cases as well?
			
			if (cClass)
			{
				while (*cClass == ' ')  // by security, remove extra whitespaces.
					cClass ++;
				str = strchr (cClass, ' ');  // first whitespace.
				if (str != NULL)  // remove everything after that
					*str = '\0';
				str = strrchr (cClass, '/');  // last '/'.
				if (str != NULL)  // we take after that.
					cClass = str + 1;
				str = strchr (cClass, '.');  // we remove all .xxx otherwise we can't detect the lack of extension when looking for an icon (openoffice.org) or it's a problem when looking for an icon (jbrout.py).
				if (str != NULL && str != cClass)
					*str = '\0';
			}
		}

		if (cClass && *cClass != '\0')
		{
			// handle the cases of programs where command != class.
			if (strncmp (cClass, "oo", 2) == 0)
			{
				if (strcmp (cClass, "ooffice") == 0 || strcmp (cClass, "oowriter") == 0 || strcmp (cClass, "oocalc") == 0 || strcmp (cClass, "oodraw") == 0 || strcmp (cClass, "ooimpress") == 0)  // openoffice poor design: there is no way to bind its windows to the launcher without this trick.
					cClass = "openoffice";
			}
			else if (strncmp (cClass, "libreoffice", 11) == 0)  // libreoffice has different classes according to the launching option (--writer => libreoffice-writer, --calc => libreoffice-calc, etc)
			{
				gchar *str = strchr (cCommand, ' ');
				if (str && *(str+1) == '-')
				{
					g_free (cDefaultClass);
					cDefaultClass = g_strdup_printf ("%s%s", "libreoffice", str+2);
					str = strchr (cDefaultClass, ' ');  // remove the additionnal params of the command.
					if (str)
						*str = '\0';
					cClass = cDefaultClass;  // "libreoffice-writer"
				}
			}
			
			// final result
			cResult = g_strdup (cClass);
		}
		g_free (cDefaultClass);
	}
	else
		cResult = g_ascii_strdown (cStartupWMClass, -1);
	
	if (cResult)
	{
		// remove some suffices that can be problematic: .exe, .py (note: cResult is already lowercase here)
		if (g_str_has_suffix (cResult, ".exe")) cResult[strlen (cResult) - 4] = 0;
		else if (g_str_has_suffix (cResult, ".py")) cResult[strlen (cResult) - 3] = 0;
		cairo_dock_remove_version_from_string (cResult);
	}
	cd_debug (" -> '%s'", cResult);

	return cResult;
}



/** Register an application class from apps installed on the system -- internal version.
* @param cDesktopFile desktop file path or name to look for.
* @param cClassName class name or app-id (from the WM) to look for after preprocessing with
* 	cairo_dock_guess_class () or gldi_window_parse_class () (i.e. should be lowercase and suffices removed).
* @param cWmClass StartupWMClass key from a custom launcher or the class / app-id as reported by the WM
* 	without any processing
* @param bUseWmClass if TRUE, and cWmClass != NULL, cWmClass will also added as a key
* @param pResult store the found / created CairoDockClassAppli here for further processing
* @return the class ID in a newly allocated string (can be used to retrieve class properties later).
* 
* Exactly one of cDesktopFile or cClassName should be supplied. These are processed in the exact same
* way when searching for apps, meaning:
*  - first, it is checked if a registered class exists with either of these or cWmClass as a key
*  - next, they are supplied to _search_desktop_file which tries to find a matching .desktop file
*     using a set of heuristics
* 
* The difference is that if cDesktopFile is given and not found, this function fails: no new class is
* registered and returns NULL. In contrast, if cClassName is given and not found, an new, empty class
* is registered and (a copy of) cClassName is returned. Also, if an app is found, cClassName is always
* added as a key (so if cClassName != NULL, the return value is always a copy of it).
*/
static gchar *_cairo_dock_register_class_full (const gchar *cDesktopFile, const gchar *cClassName,
	const gchar *cWmClass, gboolean bUseWmClass, CairoDockClassAppli **pResult)
{
	g_return_val_if_fail (cDesktopFile != NULL || cClassName != NULL, NULL);
	//g_print ("%s (%s, %s, %s)\n", __func__, cDesktopFile, cClassName, cWmClass);
	if (cDesktopFile && cClassName)
		cd_warning ("both cDesktopFile and cClassName given, only one will be used!");

	//\__________________ if the class is already registered and filled, quit.
	gchar *cClass = cClassName ? g_strdup (cClassName) : NULL;
	CairoDockClassAppli *pClassAppli = NULL;
	
	// note: in many cases, this will be non-NULL (if we encountered cClass before but did not register it)
	pClassAppli = _cairo_dock_lookup_class_appli (cClass?cClass:cDesktopFile);

	if (pClassAppli)  // we already searched this class (we only add to the hash table in this function)
	{
		//g_print ("class %s already known (%s)\n", cClass?cClass:cDesktopFile, pClassAppli->cDesktopFile);
		if (pClassAppli->cStartupWMClass == NULL && cWmClass != NULL)  // if the cStartupWMClass was not stored before, do it now.
			pClassAppli->cStartupWMClass = g_strdup (cWmClass);
		if (bUseWmClass && cWmClass != NULL && !g_hash_table_contains (s_hAltClass, cWmClass))
			g_hash_table_insert (s_hAltClass, g_strdup (cWmClass), pClassAppli);
		//g_print ("%s --> %s\n", cClass, pClassAppli->cStartupWMClass);
		if (pResult) *pResult = pClassAppli;
		return cClass ? cClass : g_strdup (cDesktopFile);
	}
	
	if (cWmClass)
	{
		pClassAppli = _cairo_dock_lookup_class_appli (cWmClass);
		if (pClassAppli)
		{
			if (pResult) *pResult = pClassAppli;
			return g_strdup (cWmClass);
		}
	}

	//\__________________ search the desktop file's path.
	GDesktopAppInfo *app = _search_desktop_file (cDesktopFile?cDesktopFile:cClass, FALSE);
	if (app == NULL && cWmClass != NULL)
		app = _search_desktop_file (cWmClass, TRUE);
	if (app == NULL)  // couldn't find the .desktop
	{
		if (cClass != NULL)  // make a class anyway to store the few info we have.
		{
			pClassAppli = g_new0 (CairoDockClassAppli, 1);
			g_hash_table_insert (s_hClassTable, g_strdup (cClass), pClassAppli);

			if (pClassAppli->cStartupWMClass == NULL && cWmClass != NULL)
				pClassAppli->cStartupWMClass = g_strdup (cWmClass);
			//g_print ("%s ---> %s\n", cClass, pClassAppli->cStartupWMClass);
			pClassAppli->bSearchedAttributes = TRUE;
			if (bUseWmClass && cWmClass != NULL && !g_hash_table_contains (s_hAltClass, cWmClass))
				g_hash_table_insert (s_hAltClass, g_strdup (cWmClass), pClassAppli);
			if (pResult) *pResult = pClassAppli;
		}
		cd_debug ("couldn't find the desktop file %s", cDesktopFile?cDesktopFile:cClass);
		return cClass;  /// can be NULL
	}

	//\__________________ open it. -- TODO: use g_app_info_get_id () instead?
	const gchar *cDesktopFilePath = g_desktop_app_info_get_filename (app);
	cd_debug ("+ parsing class desktop file %s...", cDesktopFilePath);

	//\__________________ guess the class name.
	const gchar *cCommand = g_app_info_get_commandline (G_APP_INFO(app));
	const gchar *cStartupWMClass = g_desktop_app_info_get_startup_wm_class (app);
	if (cStartupWMClass && *cStartupWMClass == '\0')
		cStartupWMClass = NULL;

	/* We have three potential sources for the "class" of an application:
	 * (1) cClassName -- the app-id / class reported for an open app -> cClass variable here
	 * 		(this is already parsed by gldi_window_parse_class () as opposed to the
	 * 		 cWmClass variable which is the "raw" value reported by the WM)
	 * (2) the basename of the desktop file from cDesktopFilePath
	 * 		always available if we are here (either from cDesktopFile
	 * 		or matched above)
	 * (3) the StartupWMClass or Exec key from the desktop file -- note:
	 * 		this is not necessarily unique, multiple desktop files can
	 * 		have the same key or command
	 * Obviously, if we are loading a launcher, (1) is not available.
	 * Note: all of these will be lowercase ((1) and (3) converted when
	 * parsing, (2) converted below).
	 * 
	 * On X11, (1) and (3) should be the same (if (3) is given). On
	 * Wayland, (1) and (2) should be the same. However, there are
	 * exceptions. Also, running XWayland apps can complicate things. In
	 * theory, having three different values is possible if running the
	 * app-id / class does not match the StartupWMClass, but also does
	 * not match the desktop file name because it does not include the
	 * reverse DNS of the publisher, which was however correctly guessed
	 * by us (see _search_desktop_file () above).
	 * We deal with this by storing (1) or (2) as the "class" in
	 * s_hClassTable, and the others in s_hAltClass, so no matter how an
	 * app identifies itself, it will be matched to its icon.
	 * Note: (3) is always put in s_hAltClass since it can be non-unique
	 * and it is ignored if it is already present when registering a new
	 * app that has (1) or (2) not matching a previously registered app. */
	gchar *cDesktopFileID = NULL; // (2)
	gchar *cAltClass2 = NULL; // (3)
	{
		gchar *tmp = g_path_get_basename (cDesktopFilePath);
		gchar *tmp2 = g_ascii_strdown (tmp, -1);
		g_free (tmp);
		if (g_str_has_suffix (tmp2, ".desktop"))
		{
			int len = strlen (tmp2);
			tmp2[len - 8] = 0; // here, len >= 8
		}
		cDesktopFileID = tmp2;
	}
	// potential "class" from the desktop file (3)
	if (cCommand || cStartupWMClass)
		cAltClass2 = cairo_dock_guess_class (cCommand, cStartupWMClass);
	// take care of duplicates
	if (cAltClass2 && !strcmp(cDesktopFileID, cAltClass2))
	{
		g_free (cAltClass2);
		cAltClass2 = NULL;
	}
	if (cClass)
	{
		// cDesktopFileID != NULL here
		if (!strcmp (cClass, cDesktopFileID))
		{
			g_free (cDesktopFileID);
			cDesktopFileID = NULL;
		}
		if (cAltClass2 && !strcmp (cClass, cAltClass2))
		{
			g_free (cAltClass2);
			cAltClass2 = NULL;
		}
	}
	
	//\__________________ make a new class or get the existing one.
	{
		CairoDockClassAppli *pDesktopIDAppli = _cairo_dock_lookup_class_appli (cDesktopFileID);
		
		/*
		 * Note: here cDesktopFileID != NULL, while cClass can be NULL.
		 * If cClass != NULL, it was searched before and not found.
		 * pDesktopIDAppli can be non-NULL if this app was found before; in this
		 * case, it will have bSearchedAttributed == TRUE
		 */
		
		if (pDesktopIDAppli)
		{
			pClassAppli = pDesktopIDAppli;
			if (cClass)
			{
				g_hash_table_insert (s_hAltClass, g_strdup(cClass), pClassAppli);
				g_free (cDesktopFileID);
			}
			else cClass = cDesktopFileID;
		}
		else
		{
			if (!cClass)
			{
				cClass = cDesktopFileID;
				cDesktopFileID = NULL;
			}
			// need to create a new class
			pClassAppli = g_new0 (CairoDockClassAppli, 1);
			g_object_ref (app); // will call g_object_unref() below
			pClassAppli->app = app;
			g_hash_table_insert (s_hClassTable, g_strdup (cClass), pClassAppli);
			if (cDesktopFileID) g_hash_table_insert (s_hAltClass, cDesktopFileID, pClassAppli);
		}
		
		/* here, pClassAppli != NULL
		 * 
		 * We need to check cAltClass2 which can only be stored in s_hAltClass
		 * (it is only potentially added here). If it is found, it is likely a
		 * spurious match for another app that has the same command line or the
		 * same StartupWMClass key, we can ignore it. If it is not found, we
		 * add as a potential alternative ID for this app.
		 */
		if (cAltClass2 && !g_hash_table_contains (s_hAltClass, cAltClass2))
			g_hash_table_insert (s_hAltClass, cAltClass2, pClassAppli);
		else g_free (cAltClass2);
		
		if (bUseWmClass && cWmClass != NULL && !g_hash_table_contains (s_hAltClass, cWmClass))
			g_hash_table_insert (s_hAltClass, g_strdup (cWmClass), pClassAppli);
		// by here, we freed or added as a hash table key both
		// cAltClass2 and cDesktopFileID
	}
	g_object_unref (app); // g_object_ref () was called above if making a new copy
	
	// here, we have a valid pClassAppli, store it if required
	if (pResult) *pResult = pClassAppli;
	
	// we store the WM class (class or app_id as reported by the WM without
	// any processing) if it was not stored before
	// (note: this is always the class we get from the WM and NOT the
	// StartupWMClass in the .desktop file as that might
	// not match what we have in reality)
	if (pClassAppli->cStartupWMClass == NULL && cWmClass != NULL)
		pClassAppli->cStartupWMClass = g_strdup (cWmClass);

	//\__________________ if we already searched and found the attributes beforehand, quit.
	if (pClassAppli->bSearchedAttributes)
	{
		//g_print ("%s ----> %s\n", cClass, pClassAppli->cStartupWMClass);
		return cClass;
	}
	pClassAppli->bSearchedAttributes = TRUE;

	//\__________________ get the attributes.
	pClassAppli->cName = g_strdup (g_app_info_get_name (G_APP_INFO (app)));

	// TODO: use g_app_info_get_icon () instead of this?
	pClassAppli->cIcon = g_desktop_app_info_get_string (app, "Icon");
	if (pClassAppli->cIcon != NULL && *pClassAppli->cIcon != '/')  // remove any extension.
	{
		gchar *str = strrchr (pClassAppli->cIcon, '.');
		if (str && (strcmp (str+1, "png") == 0 || strcmp (str+1, "svg") == 0 || strcmp (str+1, "xpm") == 0))
			*str = '\0';
	}

	cd_debug (" -> class '%s'", cClass);
	return cClass;
}

gchar *cairo_dock_register_class2 (const gchar *cSearchTerm, const gchar *cWmClass, gboolean bCreateAlways)
{
	if (bCreateAlways)
		return _cairo_dock_register_class_full (NULL, cSearchTerm, cWmClass, TRUE, NULL);
	else
		return _cairo_dock_register_class_full (cSearchTerm, NULL, cWmClass, TRUE, NULL);
}

gchar *cairo_dock_register_class (const gchar *cSearchTerm)
{
	return cairo_dock_register_class2 (cSearchTerm, NULL, FALSE);
}


void cairo_dock_set_data_from_class (const gchar *cClass, Icon *pIcon)
{
	g_return_if_fail (cClass != NULL && pIcon != NULL);
	cd_debug ("%s (%s)", __func__, cClass);

	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (pClassAppli == NULL || ! pClassAppli->bSearchedAttributes)
	{
		cd_debug ("no class %s or no attributes", cClass);
		return;
	}

	if (pIcon->cName == NULL)
		pIcon->cName = g_strdup (pClassAppli->cName);

	if (pIcon->cFileName == NULL)
		pIcon->cFileName = g_strdup (pClassAppli->cIcon);
	
	if (pIcon->pClassApp)
	{
		// should not happen, this function is only called for newly created icons
		cd_warning ("app corresponding to this icon was already set!");
		g_object_unref (pIcon->pClassApp);
	}
	pIcon->pClassApp = pClassAppli->app ? g_object_ref (pClassAppli->app) : NULL;
}


static void _gldi_class_appli_startup_notify_end (CairoDockClassAppli *pClassAppli);

static gboolean _stop_opening_timeout (CairoDockClassAppli *pClassAppli)
{
	pClassAppli->iSidOpeningTimeout = 0;
	_gldi_class_appli_startup_notify_end (pClassAppli);
	return FALSE;
}
void gldi_class_startup_notify (Icon *pIcon)
{
	const gchar *cClass = pIcon->cClass;
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (! pClassAppli || pClassAppli->bIsLaunching)
		return;

	// mark the class as launching and set a timeout
	pClassAppli->bIsLaunching = TRUE;
	if (pClassAppli->iSidOpeningTimeout == 0)
		pClassAppli->iSidOpeningTimeout = g_timeout_add_seconds (15,  // 15 seconds, for applications that take a really long time to start
		(GSourceFunc) _stop_opening_timeout, pClassAppli);  // we can give pClassAppli as parameter, as we would remove the timeout if it is destroyed

	// mark the icon as launching (this is just for convenience for the animations)
	gldi_icon_mark_as_launching (pIcon);
}

static void _gldi_class_appli_startup_notify_end (CairoDockClassAppli *pClassAppli)
{
	// unset the icons as launching
	GList* ic;
	Icon *icon;
	for (ic = pClassAppli->pIconsOfClass; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		gldi_icon_stop_marking_as_launching (icon);
	}
	for (ic = pClassAppli->pAppliOfClass; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		gldi_icon_stop_marking_as_launching (icon);
	}
	if (pClassAppli->cDockName != NULL)  // also unset the class-icon, if any
	{
		CairoDock *pClassSubDock = gldi_dock_get (pClassAppli->cDockName);
		Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pClassSubDock, NULL);
		if (pPointingIcon != NULL)
			gldi_icon_stop_marking_as_launching (pPointingIcon);
	}

	// unset the class as launching and stop a timeout
	pClassAppli->bIsLaunching = FALSE;
	if (pClassAppli->iSidOpeningTimeout != 0)
	{
		g_source_remove (pClassAppli->iSidOpeningTimeout);
		pClassAppli->iSidOpeningTimeout = 0;
	}
}

void gldi_class_startup_notify_end (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	if (! pClassAppli || ! pClassAppli->bIsLaunching)
		return;
	_gldi_class_appli_startup_notify_end (pClassAppli);
}


gboolean gldi_class_is_starting (const gchar *cClass)
{
	CairoDockClassAppli *pClassAppli = _cairo_dock_lookup_class_appli (cClass);
	return (pClassAppli != NULL && pClassAppli->iSidOpeningTimeout != 0);
}
