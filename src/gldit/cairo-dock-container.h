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


#ifndef __CAIRO_DOCK_CONTAINER__
#define  __CAIRO_DOCK_CONTAINER__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


/**
*@file cairo-dock-container.h This class defines the Containers, that are classic or hardware accelerated animated windows, and exposes common functions, such as redrawing a part of a container or popping a menu on a container.
*
* A Container is a rectangular on-screen located surface, has the notion of orientation, can hold external datas, monitors the mouse position, and has its own animation loop.
*
* Docks, Desklets, Dialogs, and Flying-containers all derive from Containers.
*
* If you write a new type of container, you must call \ref cairo_dock_init_container when you create it and \ref cairo_dock_finish_container when you destroy it.
*/

/// Main orientation of a container.
typedef enum {
	CAIRO_DOCK_VERTICAL = 0,
	CAIRO_DOCK_HORIZONTAL
	} CairoDockTypeHorizontality;

/// Types of available containers.
typedef enum {
	CAIRO_DOCK_TYPE_DOCK = 0,
	CAIRO_DOCK_TYPE_DESKLET,
	CAIRO_DOCK_TYPE_DIALOG,
	CAIRO_DOCK_TYPE_FLYING_CONTAINER,
	CAIRO_DOCK_NB_CONTAINER_TYPES
	} CairoDockTypeContainer;

struct _CairoContainerInterface {
	void (*set_icon_size) (CairoContainer *pContainer, Icon *icon);
	};

/// Definition of a Container, whom derive Dock, Desklet, Dialog and FlyingContainer. 
struct _CairoContainer {
	/// type of container.
	CairoDockTypeContainer iType;
	/// list of available notifications.
	GPtrArray *pNotificationsTab;
	/// External data.
	gpointer pDataSlot[CAIRO_DOCK_NB_DATA_SLOT];
	/// window of the container.
	GtkWidget *pWidget;
	/// size of the container.
	gint iWidth, iHeight;
	/// position of the container.
	gint iWindowPositionX, iWindowPositionY;
	/// TURE is the mouse is inside the container (including the possible sub-widgets).
	gboolean bInside;
	/// TRUE if the container is horizontal, FALSE if vertical
	CairoDockTypeHorizontality bIsHorizontal;
	/// TRUE if the container is oriented upwards, FALSE if downwards.
	gboolean bDirectionUp;
#ifdef HAVE_GLITZ
	glitz_drawable_format_t *pDrawFormat;
	glitz_drawable_t* pGlitzDrawable;
	glitz_format_t* pGlitzFormat;
#else
	gpointer padding[3];
#endif
	/// Source ID of the animation loop.
	guint iSidGLAnimation;
	/// interval of time between 2 animation steps.
	gint iAnimationDeltaT;
	/// X position of the mouse in the container's system of reference.
	gint iMouseX;
	/// Y position of the mouse in the container's system of reference.
	gint iMouseY;
	/// zoom applied to the container's elements.
	gdouble fRatio;
	/// TRUE if the container has a reflection power.
	gboolean bUseReflect;
	/// OpenGL context.
	GLXContext glContext;
	/// whether the GL context is an ortho or a perspective view.
	gboolean bPerspectiveView;
	/// TRUE if a slow animation is running.
	gboolean bKeepSlowAnimation;
	/// counter for the animation loop.
	gint iAnimationStep;
	CairoContainerInterface iface;
};

/// Get the Container part of a pointer.
#define CAIRO_CONTAINER(p) ((CairoContainer *) (p))

  /////////////
 // WINDOW //
///////////

void cairo_dock_set_containers_non_sticky (void);

GtkWidget *cairo_dock_init_container_full (CairoContainer *pContainer, gboolean bOpenGLWindow);

/** Initialize a Container : create a GTK window with transparency and OpenGL support. To be called when you create a new container.
*@return the newly allocated GTK window.
*/
#define cairo_dock_init_container(pContainer) cairo_dock_init_container_full (pContainer, TRUE)
/** Same as above, but with no OpenGL support.
 */
#define cairo_dock_init_container_no_opengl(pContainer) cairo_dock_init_container_full (pContainer, FALSE)

/** Finish a Container. To be called before you free it.
*@param pContainer a Container.
*/
void cairo_dock_finish_container (CairoContainer *pContainer);

void cairo_dock_set_colormap_for_window (GtkWidget *pWidget);

  ////////////
 // REDRAW //
////////////

/** Clear and trigger the redraw of a Container.
*@param pContainer the Container to redraw.
*/
void cairo_dock_redraw_container (CairoContainer *pContainer);

/** Clear and trigger the redraw of a part of a container.
*@param pContainer the Container to redraw.
*@param pArea the zone to redraw.
*/
void cairo_dock_redraw_container_area (CairoContainer *pContainer, GdkRectangle *pArea);

/** Clear and trigger the redraw of an Icon. The drawing is not done immediately, but when the expose event is received.
*@param icon l'icone a retracer.
*@param pContainer le container de l'icone.
*/
void cairo_dock_redraw_icon (Icon *icon, CairoContainer *pContainer);



/** Search for the Container of a given Icon (dock or desklet in the case of an applet).
* @param icon the icon.
* @return the container contening this icon, or NULL if the icon is nowhere.
*/
CairoContainer *cairo_dock_search_container_from_icon (Icon *icon);


/** Let a widget accepts drag-and-drops.
* @param pWidget a widget.
* @param pCallBack the function that will be called when some data is received.
* @param data data passed to the callback.
*/
void cairo_dock_allow_widget_to_receive_data (GtkWidget *pWidget, GCallback pCallBack, gpointer data);

void cairo_dock_disallow_widget_to_receive_data (GtkWidget *pWidget);

/** Say if a string is an adress (file://xxx, http://xxx, ftp://xxx, etc).
* @param cString a string.
*/
gboolean cairo_dock_string_is_adress (const gchar *cString);

/** Notify everybody that a drop has just occured.
* @param cReceivedData the dropped data.
* @param pPointedIcon the icon which was pointed when the drop occured.
* @param fOrder the order of the icon if the drop occured on it, or LAST_ORDER if the drop occured between 2 icons.
* @param pContainer the container of the icon
*/
void cairo_dock_notify_drop_data (gchar *cReceivedData, Icon *pPointedIcon, double fOrder, CairoContainer *pContainer);


/** Get the maximum zoom of the icons inside a given container.
* @param pContainer the container.
* @return the maximum scale factor.
*/
#define cairo_dock_get_max_scale(pContainer) (CAIRO_DOCK_IS_DOCK (pContainer) ? (1 + myIcons.fAmplitude) : 1)


gboolean cairo_dock_emit_signal_on_container (CairoContainer *pContainer, const gchar *cSignal);
gboolean cairo_dock_emit_leave_signal (CairoContainer *pContainer);
gboolean cairo_dock_emit_enter_signal (CairoContainer *pContainer);

/** Pop-up a menu on an icon. The menu is placed so that it touches the icon, without overlapping it. If the icon is NULL, it will be placed it at the mouse's position. In the case of a dock, it prevents this one from shrinking down.
*@param menu the menu.
*@param pIcon the icon, or NULL.
*@param pContainer the container that was clicked.
*/
void cairo_dock_popup_menu_on_icon (GtkWidget *menu, Icon *pIcon, CairoContainer *pContainer);

/** Pop-up a menu on a container. In the case of a dock, it prevents this one from shrinking down.
*@param menu the menu.
*@param pContainer the container that was clicked.
*/
#define cairo_dock_popup_menu_on_container(menu, pContainer) cairo_dock_popup_menu_on_icon (menu, NULL, pContainer)

/** Add an entry to a given menu.
*@param cLabel label of the entry
*@param gtkStock a GTK stock or a path to an image
*@param pFunction callback
*@param pMenu the menu to insert the entry in
*@param pData data to feed the callback with
*/
GtkWidget *cairo_dock_add_in_menu_with_stock_and_data (const gchar *cLabel, const gchar *gtkStock, GFunc pFunction, GtkWidget *pMenu, gpointer pData);

/** Build the main menu of a Container.
*@param icon the icon that was left-clicked, or NULL if none.
*@param pContainer the container that was left-clicked.
*@return the menu.
*/
GtkWidget *cairo_dock_build_menu (Icon *icon, CairoContainer *pContainer);


G_END_DECLS
#endif
