
#ifndef __CAIRO_DOCK_CONTAINER__
#define  __CAIRO_DOCK_CONTAINER__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

  //////////////
 /// WINDOW ///
//////////////
/** Cree une fenetre GTK adaptee a un CairoContainer (notamment transparente).
*@return la fenetre GTK nouvellement cree.
*/
GtkWidget *cairo_dock_create_container_window (void);

GtkWidget *cairo_dock_create_container_window_no_opengl (void);

/** Applique la colormap de l'ecran a une fenetre GTK, lui ajoutant la transparence.
*@param pWidget
*/
void cairo_dock_set_colormap_for_window (GtkWidget *pWidget);
/** Applique la colormap de l'ecran a un container, lui ajoutant la transparence, et active Glitz si possible.
* @param pContainer le container.
*/
void cairo_dock_set_colormap (CairoContainer *pContainer);

  //////////////
 /// REDRAW ///
//////////////
/** Efface et programme le redessin d'une seule icone. Un ExposeEvent sera genere, qui appellera la fonction de trace optimise de la vue courante si cette derniere en possede une. Le trace ne se fait donc pas immediatement, mais est mis en attente et effectue lorsqu'on revient a la main loop.
*@param icon l'icone a retracer.
*@param pContainer le container de l'icone.
*/
void cairo_dock_redraw_icon (Icon *icon, CairoContainer *pContainer);
#define cairo_dock_redraw_my_icon cairo_dock_redraw_icon
/** Efface et programme le redessin complet d'un container.
*@param pContainer le container a redessiner.
*/
void cairo_dock_redraw_container (CairoContainer *pContainer);
/** Efface et programme le redessin d'une partie d'un container.
*@param pContainer le container a redessiner.
*@pArea zone a redessiner.
*/
void cairo_dock_redraw_container_area (CairoContainer *pContainer, GdkRectangle *pArea);


/** Cherche le container contenant l'icone donnee (dock ou desklet dans le cas d'une applet).
* @param icon l'icone.
* @return le container contenant cette icone, ou NULL si l'icone est un fantome.
*/
CairoContainer *cairo_dock_search_container_from_icon (Icon *icon);


void cairo_dock_show_hide_container (CairoContainer *pContainer);


/** Autorise un widget a accepter les glisse-deposes.
* @param pWidget un widget.
* @param pCallBack la fonction qui sera appelee lors d'une reception de donnee.
* @param data donnees passees en entree de la callback.
*/
void cairo_dock_allow_widget_to_receive_data (GtkWidget *pWidget, GCallback pCallBack, gpointer data);

/**  Dis si une chaine est une addresse (file://xxx, http://xxx, ftp://xxx, etc).
* @param cString une chaine de caracteres.
*/
gboolean cairo_dock_string_is_adress (const gchar *cString);

/** Notifie tout le monde qu'un drop vient d'avoir lieu.
* @param cReceivedData les donnees recues.
* @param pPointedIcon l'icone pointee lors du drop
* @param fOrder l'ordre, egal a l'ordre de l'icone si on a droppe dessus, ou LAST_ORDER si on a droppe entre 2 icones.
* @param pContainer le container de l'icone
*/
void cairo_dock_notify_drop_data (gchar *cReceivedData, Icon *pPointedIcon, double fOrder, CairoContainer *pContainer);


/** Retourne le zoom max des icones contenues dans un conteneur donne.
* @param pContainer le container.
* @return le facteur d'echelle max.
*/
#define cairo_dock_get_max_scale(pContainer) (CAIRO_DOCK_IS_DOCK (pContainer) ? (1 + myIcons.fAmplitude) : 1)


G_END_DECLS
#endif
