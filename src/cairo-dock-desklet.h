/*
** Login : <ctaf42@gmail.com>
** Started on  Sun Jan 27 18:35:38 2008 Cedric GESTES
** $Id$
**
** Author(s)
**  - Cedric GESTES <ctaf42@gmail.com>
**  - Fabrice REY
**
** Copyright (C) 2008 Cedric GESTES
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __CAIRO_DESKLET_H__
#define  __CAIRO_DESKLET_H__

#include <cairo-dock-struct.h>
G_BEGIN_DECLS

#define CD_NB_ITER_FOR_GRADUATION 10

/**
* Teste si le container est un desklet.
*@param pContainer le container.
*@return TRUE ssi le container a ete declare comme un desklet.
*/
#define CAIRO_DOCK_IS_DESKLET(pContainer) (pContainer != NULL && (pContainer)->iType == CAIRO_DOCK_TYPE_DESKLET)
/**
* Caste un container en desklet.
*@param pContainer le container.
*@return le desklet.
*/
#define CAIRO_DESKLET(pContainer) ((CairoDesklet *)pContainer)

void cairo_dock_load_desklet_buttons (cairo_t *pSourceContext);
void cairo_dock_load_desklet_buttons_texture (void);

gboolean cairo_dock_render_desklet_notification (gpointer pUserData, CairoDesklet *pDesklet);

/**
* Créer un desklet tout simple, sans le placer ni lui définir un moteur de rendu.
*@param pIcon l'icône principale du desklet (jamais testé avec une icône nulle).
*@param pInteractiveWidget le widget d'interaction du desklet, ou NULL si aucun.
*@param bOnWidgetLayer TRUE ssi il faut placer dés maintenant le desklet sur la couche des widgets.
*@return le desklet nouvellement crée.
*/
CairoDesklet *cairo_dock_create_desklet (Icon *pIcon, GtkWidget *pInteractiveWidget, gboolean bOnWidgetLayer);

/**
* Trouve l'icône cliquée dans un desklet, en cherchant parmi l'icône principale et éventuellement la liste des icônes associées.
*@param pDesklet le desklet cliqué.
*@return l'icône cliquée ou NULL si on a cliqué à côté.
*/
Icon *cairo_dock_find_clicked_icon_in_desklet (CairoDesklet *pDesklet);

/**
* Configure entièrement un desklet en fonction de ses paramètres de sauvegarde extraits d'un fichier de conf. Le place, le dimensionne, le garde devant/derrière, vérouille sa position, et le place sur la couche des widgets, et pour finir definit ses decorations.
*@param pDesklet le desklet à placer.
*@param pMinimalConfig la config minimale du desklet, contenant tous les paramètres necessaires.
*/
void cairo_dock_configure_desklet (CairoDesklet *pDesklet, CairoDockMinimalAppletConfig *pMinimalConfig);

/**
* Détache un GtkWidget d'un desklet, le rendant libre d'être inséré autre part.
*@param pDesklet le desklet contenant un widge tinteractif.
*/
void cairo_dock_steal_interactive_widget_from_desklet (CairoDesklet *pDesklet);
/**
* Détruit un desklet, et libère toutes les ressources allouées. Son widget interactif est épargné et peut être re-inséré autre part après l'opération.
*@param pDesklet le desklet à détruire.
*/
void cairo_dock_free_desklet (CairoDesklet *pDesklet);

/**
* Cache un desklet.
*@param pDesklet le desklet à cacher.
*/
void cairo_dock_hide_desklet (CairoDesklet *pDesklet);
/**
* Montre un desklet, lui donnant le focus.
*@param pDesklet le desklet à montrer.
*/
void cairo_dock_show_desklet (CairoDesklet *pDesklet);

/**
* Ajoute un GtkWidget à la fenêtre du desklet. Ce widget ne doit pas appartenir à un autre container. 1 seul widget par desklet, mais si ce widget est un GtkContainer, on peut en mettre plusieurs autres dedans.
*@param pInteractiveWidget le widget à ajouter.
*@param pDesklet le desklet dans lequel le rajouter.
*/
void cairo_dock_add_interactive_widget_to_desklet (GtkWidget *pInteractiveWidget, CairoDesklet *pDesklet);

/**
* Rend tous les desklets visibles si ils étaient cachés, et au premier plan s'ils étaient derrière ou sur la widget layer.
*@param bOnWidgetLayerToo TRUE ssi on veut faire passer tous les desklets de la couche widgets à la couche normale.
*/
void cairo_dock_set_all_desklets_visible (gboolean bOnWidgetLayerToo);
/**
* Remet les desklets dans la position occupée avant l'appel à la fonction précédente.
*/
void cairo_dock_set_desklets_visibility_to_default (void);

/**
* Renvoie le desklet dont la fenetre correspond à la ressource X donnée.
*@param Xid ID du point de vue de X.
*@return le desklet ou NULL si aucun ne correspond.
*/
CairoDesklet *cairo_dock_get_desklet_by_Xid (Window Xid);

/**
* Lance une animation de zoom vers l'avant sur un desklet.
*@param pDesklet le desklet.
*/
void cairo_dock_zoom_out_desklet (CairoDesklet *pDesklet);


void cairo_dock_load_desklet_decorations (CairoDesklet *pDesklet, cairo_t *pSourceContext);


void cairo_dock_free_minimal_config (CairoDockMinimalAppletConfig *pMinimalConfig);

void cairo_dock_reload_desklets_decorations (gboolean bDefaultThemeOnly, cairo_t *pSourceContext);


G_END_DECLS

#endif
