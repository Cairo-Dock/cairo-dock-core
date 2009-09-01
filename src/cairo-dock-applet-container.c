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

