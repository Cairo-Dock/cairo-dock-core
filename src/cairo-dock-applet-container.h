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

#ifndef __CAIRO_DOCK_APPLET_CONTAINER__
#define  __CAIRO_DOCK_APPLET_CONTAINER__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

typedef _CairoDockAppletContenair CairoDockAppletContenair;

struct _CairoDockAppletContenair {
  gchar *cName;
  GList *pData;
};

/**
*Renvoie un pointeur vers un "Applet donnant ses informations""
*@param cAppletName nom de l'applet (tel qu'il a été enregistré); n'est pas altere par la fonction.
*/
CairoDockAppletContenair *cairo_dock_get_applet_contenair_from_name (gchar *cAppletName);

/**
*Enregistre notre applet comme partagant ses informations.
*@param cAppletName nom de l'applet; n'est pas altere par la fonction.
*/
void cairo_dock_register_my_applet_for_contenair (gchar *cAppletName);

/**
*Supprime de la mémoire toutes les informations.
*@param cAppletName nom de l'applet (tel qu'il a été enregistré); n'est pas altere par la fonction.
*retourn TRUE si l'action s'est bien passé sinon FALSE.
*/
gboolean cairo_dock_reset_applet_data_for_contenair (gchar *cAppletName);

/**
*Ajoute une information a la GList de l'applet.
*@param cAppletName nom de l'applet (tel qu'il a été enregistré); n'est pas altere par la fonction.
*@param cData informations a sauvegarder; n'est pas altere par la fonction.
*retourn TRUE si l'action s'est bien passé sinon FALSE.
*/
gboolean cairo_dock_add_applet_data_for_contenair (gchar *cAppletName, gchar *cData);

/**
*Supprime tout.
*/
void cairo_dock_reset_all_data (void);

G_END_DECLS
#endif
