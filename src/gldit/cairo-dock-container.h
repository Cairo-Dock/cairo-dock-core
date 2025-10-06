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

#include "gldi-config.h"
#include <glib.h>
#ifdef HAVE_GLX
#include <GL/glx.h>  // GLXContext
#endif
#ifdef HAVE_EGL
#include <EGL/egl.h>  // EGLContext, EGLSurface
#ifdef HAVE_WAYLAND
#include <wayland-egl.h> // wl_egl_window
#endif
#endif
#include "cairo-dock-struct.h"
#include "cairo-dock-object.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-container.h This class defines the Containers, that are classic or hardware accelerated animated windows, and exposes common functions, such as redrawing a part of a container or popping a menu on a container.
*
* A Container is a rectangular on-screen located surface, has the notion of orientation, can hold external datas, monitors the mouse position, and has its own animation loop.
*
* Docks, Desklets, Dialogs, and Flying-containers all derive from Containers.
*
*/


typedef struct _GldiContainerAttr GldiContainerAttr;

#ifndef _MANAGER_DEF_
extern GldiObjectManager myContainerObjectMgr;
#endif

struct _GldiContainerAttr {
	gboolean bNoOpengl;
	gboolean bIsPopup;
};

/// signals
typedef enum {
	/// notification called when the menu is being built on a container. data : {Icon, GldiContainer, GtkMenu, gboolean*}
	NOTIFICATION_BUILD_CONTAINER_MENU = NB_NOTIFICATIONS_OBJECT,
	/// notification called when the menu is being built on an icon (possibly NULL). data : {Icon, GldiContainer, GtkMenu}
	NOTIFICATION_BUILD_ICON_MENU,
	/// notification called when use clicks on an icon data : {Icon, CairoDock, int}
	NOTIFICATION_CLICK_ICON,
	/// notification called when the user double-clicks on an icon. data : {Icon, CairoDock}
	NOTIFICATION_DOUBLE_CLICK_ICON,
	/// notification called when the user middle-clicks on an icon. data : {Icon, CairoDock}
	NOTIFICATION_MIDDLE_CLICK_ICON,
	/// notification called when the user scrolls on a container. data : {Icon, CairoContainer, int iDirection, int bEmulated}
	/// Note: Icon is the icon under the mouse or can be NULL if the mouse is not over any icon. Currently it is only emitted on docks and desklets.
	/// iDirection is either GDK_SCROLL_UP or GDK_SCROLL_DOWN; bEmulated is TRUE if this event is synthetized based on
	/// a series of GDK_SCROLL_SMOOTH events received earlier (so it can be ignored if those were handled)
	NOTIFICATION_SCROLL_ICON,
	/// notification called when the user scrolls on a container and a GDK_SCROLL_SMOOTH event was delivered
	/// data : {Icon, CairoContainer, gdouble delta_x, gdouble delta_y}
	NOTIFICATION_SMOOTH_SCROLL_ICON,
	/// notification called when the mouse enters an icon. data : {Icon, CairoDock, gboolean*}
	NOTIFICATION_ENTER_ICON,
	/// notification called when the mouse enters a dock while dragging an object.
	NOTIFICATION_START_DRAG_DATA,
	/// notification called when something is dropped inside a container. data : {gchar*, Icon, double*, CairoDock}
	/// only called if the below NOTIFICATION_DROP_DATA_SELECTION was not handled
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
	/// notification called when something is dropped, using the original data received
	/// data: GtkSelectionData*, Icon, double*, CairoDock, gboolean* bHandled
	NOTIFICATION_DROP_DATA_SELECTION,
	NB_NOTIFICATIONS_CONTAINER
	} GldiContainerNotifications;


// factory
/// Main orientation of a container.
typedef enum {
	CAIRO_DOCK_VERTICAL = 0,
	CAIRO_DOCK_HORIZONTAL
	} CairoDockTypeHorizontality;

struct _GldiContainerInterface {
	gboolean (*animation_loop) (GldiContainer *pContainer);
	void (*setup_menu) (GldiContainer *pContainer, Icon *pIcon, GtkWidget *pMenu);
	void (*detach_icon) (GldiContainer *pContainer, Icon *pIcon);
	void (*insert_icon) (GldiContainer *pContainer, Icon *pIcon, gboolean bAnimateIcon);
	};

/// Definition of a Container, whom derive Dock, Desklet, Dialog and FlyingContainer. 
struct _GldiContainer {
	/// object.
	GldiObject object;
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
	/// Source ID of the animation loop.
	guint iSidGLAnimation;
	/// interval of time between 2 animation steps.
	gint iAnimationDeltaT;
	/// X position of the mouse in the container's system of reference.
	gint iMouseX;
	/// Y position of the mouse in the container's system of reference.
	gint iMouseY;
	/// accumulate smooth scroll events to emulate fixed steps
	gdouble fSmoothScrollAccum;
	/// time of last smooth scroll event received (to filter potential duplicates)
	guint iLastScrollTime;
	/// zoom applied to the container's elements.
	gdouble fRatio;
	/// TRUE if the container has a reflection power.
	gboolean bUseReflect;
	/// OpenGL context (either a GLXContext or EGLContext -- both are typedef for a pointer).
	void *glContext;
	/// EGL surface (not needed for GLX).
	void *eglSurface;
	/// whether the GL context is an ortho or a perspective view.
	gboolean bPerspectiveView;
	/// TRUE if a slow animation is running.
	gboolean bKeepSlowAnimation;
	/// counter for the animation loop.
	gint iAnimationStep;
	GldiContainerInterface iface;
	
	gboolean bIgnoreNextReleaseEvent;
	
	/// data for the gldi_container_move_to_rect() callback if needed
	void *pMoveToRect;
	/// a wl_egl_window (needed on Wayland + EGL)
	void *eglwindow;
	
	gpointer reserved[2];
};


/// Get the Container part of a pointer.
#define CAIRO_CONTAINER(p) ((GldiContainer *) (p))

/** Say if an object is a Container.
*@param obj the object.
*@return TRUE if the object is a Container.
*/
#define CAIRO_DOCK_IS_CONTAINER(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myContainerObjectMgr)

  /////////////
 // WINDOW //
///////////


#define gldi_container_get_gdk_window(pContainer) gtk_widget_get_window ((pContainer)->pWidget)

#define gldi_container_is_visible(pContainer) gtk_widget_get_visible ((pContainer)->pWidget)

/* NOTE: this does nothing on Wayland (no global mouse position, we need
 * to rely on the motion notify and leave / enter events).
 * Beyond event handling, this is only used in the Toons applet which is not expected to work on Wayland. */
void gldi_container_update_mouse_position (GldiContainer *pContainer);

/** Tell if a Container is the current active window (similar to gtk_window_is_active but actually works).
*@param pContainer the container
*@return TRUE if the Container is the current active window.
*/
gboolean gldi_container_is_active (GldiContainer *pContainer);

/** Show a Container and make it take the focus  (similar to gtk_window_present, but bypasses the WM focus steal prevention).
*@param pContainer the container
*/
void gldi_container_present (GldiContainer *pContainer);

/** Determine if the display server is Wayland; this can be used by e.g. positioning
* code that needs to work differently under Wayland; ideally, code that needs to
* depend on this could be moved to the backends, but for now, that seems too complicated.
*
*@return TRUE if we are running on Wayland.
*/
gboolean gldi_container_is_wayland_backend ();

  ////////////
 // REDRAW //
////////////

/** Clear and trigger the redraw of a Container.
*@param pContainer the Container to redraw.
*/
void cairo_dock_redraw_container (GldiContainer *pContainer);

/** Clear and trigger the redraw of a part of a container.
*@param pContainer the Container to redraw.
*@param pArea the zone to redraw.
*/
void cairo_dock_redraw_container_area (GldiContainer *pContainer, GdkRectangle *pArea);

/** Clear and trigger the redraw of an Icon. The drawing is not done immediately, but when the expose event is received.
*@param icon l'icone a retracer.
*/
void cairo_dock_redraw_icon (Icon *icon);


void cairo_dock_allow_widget_to_receive_data (GtkWidget *pWidget, GCallback pCallBack, gpointer data);

/** Build the main menu of a Container.
*@param icon the icon that was left-clicked, or NULL if none.
*@param pContainer the container that was left-clicked.
*@return the menu.
*/
GtkWidget *gldi_container_build_menu (GldiContainer *pContainer, Icon *icon);

G_END_DECLS
#endif
