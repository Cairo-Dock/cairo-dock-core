/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by ChAnGFu (for any bug report, please mail me to changfu@cairo-dock.org)

*********************************************************************************/
#include <string.h>
#include <stdlib.h>

#include "cairo-dock-log.h"
#include "cairo-dock-applet-container.h"

GList *pAppletContainable = NULL;

CairoDockAppletContenair *cairo_dock_get_applet_contenair_from_name (gchar *cAppletName) {
  cd_debug ("%s : %s", __func__, cAppletName);
  GList *ic;
	CairoDockAppletContenair *applets=NULL;
	for (ic = pAppletContainable; ic != NULL; ic = ic->next) {
		applets = ic->data;
		if (strcmp(applets->cName, cAppletName) == 0)
			return applets;
	}
	return NULL;
}

void cairo_dock_register_my_applet_for_contenair (gchar *cAppletName) {
  cd_debug ("%s : %s", __func__, cAppletName);
  CairoDockAppletContenair *pNewApplet = g_new0 (CairoDockAppletContenair, 1);
  pNewApplet->cName = g_strdup (cAppletName);
  pAppletContainable = g_list_append (pAppletContainable, pNewApplet);
}

gboolean cairo_dock_reset_applet_data_for_contenair (gchar *cAppletName) {
  cd_debug ("%s : %s", __func__, cAppletName);
  GList *ic;
	CairoDockAppletContenair *applets=NULL;
	for (ic = pAppletContainable; ic != NULL; ic = ic->next) {
		applets = ic->data;
		if (strcmp(applets->cName, cAppletName) == 0) {
			g_list_foreach (applets->pData, (GFunc) g_free, NULL);
			return TRUE; //Permet de savoir si notre applet n'a pas disparu de la liste
		}
	}
	return FALSE;
}

gboolean cairo_dock_add_applet_data_for_contenair (gchar *cAppletName, gchar *cData) {
  cd_debug ("%s : %s", __func__, cAppletName);
  CairoDockAppletContenair *applets = cairo_dock_get_applet_contenair_from_name (cAppletName);
  if (applets != NULL) {
    applets->pData = g_list_append (applets->pData, cData);
    return TRUE;
  }
  return FALSE;
}

void cairo_dock_reset_all_data (void) {
  cd_message ("%s : removing all shared information...", __func__);
  GList *ic;
	CairoDockAppletContenair *applets=NULL;
	for (ic = pAppletContainable; ic != NULL; ic = ic->next) {
		applets = ic->data;
		if (strcmp(applets->cName, cAppletName) == 0) {
			g_list_foreach (applets->pData, (GFunc) g_free, NULL);
			g_free (applets);
		}
	}
	g_free (pAppletContainable);
	pAppletContainable = NULL;
}

