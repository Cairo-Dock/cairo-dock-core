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
#include "cairo-dock-manager.h"
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

typedef struct _CairoContainersParam CairoContainersParam;
typedef struct _CairoContainersManager CairoContainersManager;

#ifndef _MANAGER_DEF_
extern CairoContainersParam myContainersParam;
extern CairoContainersManager myContainersMgr;
#endif

#define CD_DOUBLE_CLICK_DELAY 250  // ms


// params
struct _CairoContainersParam{
	gboolean bUseFakeTransparency;
	gint iGLAnimationDeltaT;
	gint iCairoAnimationDeltaT;
	};

// manager
struct _CairoContainersManager {
	GldiManager mgr;
	} ;

/// signals
typedef enum {
	/// notification called when the menu is being built on a container. data : {Icon, CairoContainer, GtkMenu, gboolean*}
	NOTIFICATION_BUILD_CONTAINER_MENU = NB_NOTIFICATIONS_OBJECT,
	/// notification called when the menu is being built on an icon (possibly NULL). data : {Icon, CairoContainer, GtkMenu}
	NOTIFICATION_BUILD_ICON_MENU,
	/// notification called when use clicks on an icon data : {Icon, CairoDock, int}
	NOTIFICATION_CLICK_ICON,
	/// notification called when the user double-clicks on an icon. data : {Icon, CairoDock}
	NOTIFICATION_DOUBLE_CLICK_ICON,
	/// notification called when the user middle-clicks on an icon. data : {Icon, CairoDock}
	NOTIFICATION_MIDDLE_CLICK_ICON,
	/// notification called when the user scrolls on an icon. data : {Icon, CairoDock, int}
	NOTIFICATION_SCROLL_ICON,
	/// notification called when the mouse enters an icon. data : {Icon, CairoDock, gboolean*}
	NOTIFICATION_ENTER_ICON,
	/// notification called when the mouse enters a dock while dragging an object.
	NOTIFICATION_START_DRAG_DATA,
	/// notification called when something is dropped inside a container. data : {gchar*, Icon, double*, CairoDock}
	NOTIFICATION_DROP_DATA,
	/// notification called when the mouse has moved inside a container.
	NOTIFICATION_MOUSE_MOVED,
	/// notification called when a key is pressed in a container that has the focus.
	NOTIFICATION_KEY_PRESSED,
	/// notification called for the fast rendering loop on a container.
	NOTIFICATION_UPDATE,
	/// notification called for the slow rendering loop on a container.
	NOTIFICATION_UPDATE_SLOW,
	/// notification called when a container is rendered.
	NOTIFICATION_RENDER,
	NB_NOTIFICATIONS_CONTAINER
	} CairoContainerNotifications;


// factory
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
	gboolean (*animation_loop) (CairoContainer *pContainer);
	};

/// Definition of a Container, whom derive Dock, Desklet, Dialog and FlyingContainer. 
struct _CairoContainer {
	/// object.
	GldiObject object;
	/// External data.
	gpointer pDataSlot[CAIRO_DOCK_NB_DATA_SLOT];
	/// type of container.
	CairoDockTypeContainer iType;
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
	
	gboolean bIgnoreNextReleaseEvent;
	gpointer reserved[4];
};

/// Get the Container part of a pointer.
#define CAIRO_CONTAINER(p) ((CairoContainer *) (p))


  /////////////
 // WINDOW //
///////////

void cairo_dock_set_containers_non_sticky (void);

GtkWidget *cairo_dock_init_container_full (CairoContainer *pContainer, gboolean bOpenGLWindow);

/** Initialize a Container : create a GTK window with transparency and OpenGL support. To be called when you create a new container.
*@param pContainer a Container.
*@return the newly allocated GTK window.
*/
#define cairo_dock_init_container(pContainer) cairo_dock_init_container_full (pContainer, TRUE)

/** Same as above, but with no OpenGL support.
*@param pContainer a Container.
*/
#define cairo_dock_init_container_no_opengl(pContainer) cairo_dock_init_container_full (pContainer, FALSE)

/** Finish a Container. To be called before you free it.
*@param pContainer a Container.
*/
void cairo_dock_finish_container (CairoContainer *pContainer);


#if (GTK_MAJOR_VERSION < 3 && GTK_MINOR_VERSION < 14)
#define gldi_container_get_gdk_window(pContainer) (pContainer)->pWidget->window
#else
#define gldi_container_get_gdk_window(pContainer) gtk_widget_get_window ((pContainer)->pWidget)
#endif

#define gldi_container_get_Xid(pContainer) GDK_WINDOW_XID (gldi_container_get_gdk_window(pContainer))

#if (GTK_MAJOR_VERSION < 3 && GTK_MINOR_VERSION < 18)
#define gldi_container_is_visible(pContainer) GTK_WIDGET_VISIBLE ((pContainer)->pWidget)
#else
#define gldi_container_is_visible(pContainer) gtk_widget_get_visible ((pContainer)->pWidget)
#endif

#if (GTK_MAJOR_VERSION < 3)
#define GLDI_KEY(x) GDK_##x
#else
#define GLDI_KEY(x) GDK_KEY_##x
#endif

#define gldi_container_update_mouse_position(pContainer) \
	if ((pContainer)->bIsHorizontal) \
		gdk_window_get_pointer (gldi_container_get_gdk_window (pContainer), &pContainer->iMouseX, &pContainer->iMouseY, NULL); \
	else \
		gdk_window_get_pointer (gldi_container_get_gdk_window (pContainer), &pContainer->iMouseY, &pContainer->iMouseX, NULL);

#if (GTK_MAJOR_VERSION < 3)
#define _gtk_hbox_new(m) gtk_hbox_new (FALSE, m)
#define _gtk_vbox_new(m) gtk_vbox_new (FALSE, m)
#else
#define _gtk_hbox_new(m) gtk_box_new (GTK_ORIENTATION_HORIZONTAL, m)
#define _gtk_vbox_new(m) gtk_box_new (GTK_ORIENTATION_VERTICAL, m)
#endif

gboolean cairo_dock_emit_signal_on_container (CairoContainer *pContainer, const gchar *cSignal);
gboolean cairo_dock_emit_leave_signal (CairoContainer *pContainer);
gboolean cairo_dock_emit_enter_signal (CairoContainer *pContainer);


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


void cairo_dock_allow_widget_to_receive_data (GtkWidget *pWidget, GCallback pCallBack, gpointer data);

/** Enable a Container to accept drag-and-drops.
* @param pContainer a container.
* @param pCallBack the function that will be called when some data is received.
* @param data data passed to the callback.
*/
#define gldi_container_enable_drop(pContainer, pCallBack, data) cairo_dock_allow_widget_to_receive_data (pContainer->pWidget, pCallBack, data)

void gldi_container_disable_drop (CairoContainer *pContainer);

/** Say if a string is an adress (file://xxx, http://xxx, ftp://xxx, etc).
* @param cString a string.
* @return TRUE if it's an address.
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
#define cairo_dock_get_max_scale(pContainer) (CAIRO_DOCK_IS_DOCK (pContainer) ? (1 + myIconsParam.fAmplitude) : 1)


  //////////
 // MENU //
//////////

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
* @return the new menu-entry that has been added.
*/
GtkWidget *cairo_dock_add_in_menu_with_stock_and_data (const gchar *cLabel, const gchar *gtkStock, GCallback pFunction, GtkWidget *pMenu, gpointer pData);

/** Add sub-menu to a given menu.
*@param cLabel label of the sub-menu
*@param pMenu the menu to insert the entry in
*@param cImage a GTK stock or a path to an image
* @return the new sub-menu that has been added.
*/
GtkWidget *cairo_dock_create_sub_menu (const gchar *cLabel, GtkWidget *pMenu, const gchar *cImage);


/** Build the main menu of a Container.
*@param icon the icon that was left-clicked, or NULL if none.
*@param pContainer the container that was left-clicked.
*@return the menu.
*/
GtkWidget *cairo_dock_build_menu (Icon *icon, CairoContainer *pContainer);


  /////////////////
 // INPUT SHAPE //
/////////////////

GldiShape *gldi_container_create_input_shape (CairoContainer *pContainer, int x, int y, int w, int h);

#if (GTK_MAJOR_VERSION < 3)
#define gldi_container_set_input_shape(pContainer, pShape) \
gtk_widget_input_shape_combine_mask ((pContainer)->pWidget, pShape, 0, 0)
#else // GTK3, cairo_region_t
#define gldi_container_set_input_shape(pContainer, pShape) \
gtk_widget_input_shape_combine_region ((pContainer)->pWidget, pShape)
#endif

#if (GTK_MAJOR_VERSION < 3)
#define gldi_shape_destroy(pShape) g_object_unref (pShape)
#else
#define gldi_shape_destroy(pShape) cairo_region_destroy (pShape)
#endif


void gldi_register_containers_manager (void);

G_END_DECLS
#endif
