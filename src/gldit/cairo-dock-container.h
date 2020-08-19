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
#endif
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
*/

// manager
typedef struct _GldiContainersParam GldiContainersParam;
typedef struct _GldiContainerAttr GldiContainerAttr;

#ifndef _MANAGER_DEF_
extern GldiContainersParam myContainersParam;
extern GldiManager myContainersMgr;
extern GldiObjectManager myContainerObjectMgr;
#endif

#define CD_DOUBLE_CLICK_DELAY 250  // ms

// params
struct _GldiContainersParam{
	//gboolean bUseFakeTransparency;
	gint iGLAnimationDeltaT;
	gint iCairoAnimationDeltaT;
	};

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
	/// zoom applied to the container's elements.
	gdouble fRatio;
	/// TRUE if the container has a reflection power.
	gboolean bUseReflect;
	#ifdef HAVE_GLX
	/// OpenGL context.
	GLXContext glContext;
	void *unused;  // keep ABI compatibility
	#elif defined(HAVE_EGL)
	/// OpenGL context.
	EGLContext glContext;
	/// EGL surface.
	EGLSurface eglSurface;
	#endif
	/// whether the GL context is an ortho or a perspective view.
	gboolean bPerspectiveView;
	/// TRUE if a slow animation is running.
	gboolean bKeepSlowAnimation;
	/// counter for the animation loop.
	gint iAnimationStep;
	GldiContainerInterface iface;
	
	gboolean bIgnoreNextReleaseEvent;
	
	void *pMoveToRect; // data for gldi_container_move_to_rect() callback if needed
	gpointer reserved[3];
};


/// Which "layer" a container should appear on. Bottom is below any normal windows, top is above
typedef enum {
	CAIRO_DOCK_LAYER_BOTTOM = 0,
	CAIRO_DOCK_LAYER_TOP
} GldiContainerLayer;

typedef enum {
	CAIRO_DOCK_BOTTOM = 0,
	CAIRO_DOCK_TOP,
	CAIRO_DOCK_RIGHT,
	CAIRO_DOCK_LEFT,
	CAIRO_DOCK_INSIDE_SCREEN,
	CAIRO_DOCK_NB_POSITIONS
	} CairoDockPositionType;

/// Definition of the Container backend. It defines some operations that should be, but are not, provided by GTK.
struct _GldiContainerManagerBackend {
	void (*reserve_space) (GldiContainer *pContainer, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x);
	int (*get_current_desktop_index) (GldiContainer *pContainer);
	void (*move) (GldiContainer *pContainer, int iNumDesktop, int iAbsolutePositionX, int iAbsolutePositionY);
	gboolean (*is_active) (GldiContainer *pContainer);
	void (*present) (GldiContainer *pContainer);
	/// extra functionality for Wayland / gtk_layer_shell
	/// Initialize layer-shell additions -- need to be called before mapping window first
	void (*init_layer) (GldiContainer *pContainer);
	/// Anchor this container to a screen edge (this is used instead of regular positioning)
	void (*set_anchor) (GldiContainer *pContainer, CairoDockPositionType iScreenBorder);
	/// Set on which layer should this container appear
	void (*set_layer) (GldiContainer *pContainer, GldiContainerLayer iLayer);
	gboolean (*is_wayland) ();
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


void cairo_dock_set_containers_non_sticky (void);

void cairo_dock_disable_containers_opacity (void);

#define gldi_container_get_gdk_window(pContainer) gtk_widget_get_window ((pContainer)->pWidget)

#define gldi_container_get_Xid(pContainer) GDK_WINDOW_XID (gldi_container_get_gdk_window(pContainer))

#define gldi_container_is_visible(pContainer) gtk_widget_get_visible ((pContainer)->pWidget)


void gldi_display_get_pointer (int *xptr, int *yptr);

/* NOTE: this does nothing on Wayland (no global mouse position, we need
 * to rely on the motion notify and leave / enter events) */
void gldi_container_update_mouse_position (GldiContainer *pContainer);


/** Reserve a space on the screen for a Container; other windows won't overlap this space when maximised.
*@param pContainer the container
*@param left 
*@param right 
*@param top 
*@param bottom 
*@param left_start_y 
*@param left_end_y 
*@param right_start_y 
*@param right_end_y 
*@param top_start_x 
*@param top_end_x 
*@param bottom_start_x 
*@param bottom_end_x 
*/
void gldi_container_reserve_space (GldiContainer *pContainer, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x);

/** Get the desktop and viewports a Container is placed on.
*@param pContainer the container
*@return an index representing the desktop and viewports.
*/
int gldi_container_get_current_desktop_index (GldiContainer *pContainer);

/** Move a Container to a given desktop, viewport, and position (similar to gtk_window_move except that the position is defined on the whole desktop (made of all viewports); it's only useful if the Container is sticky).
*@param pContainer the container
*@param iNumDesktop desktop number
*@param iAbsolutePositionX horizontal position on the virtual screen
*@param iAbsolutePositionY vertical position on the virtual screen
*/
void gldi_container_move (GldiContainer *pContainer, int iNumDesktop, int iAbsolutePositionX, int iAbsolutePositionY);

/** Tell if a Container is the current active window (similar to gtk_window_is_active but actually works).
*@param pContainer the container
*@return TRUE if the Container is the current active window.
*/
gboolean gldi_container_is_active (GldiContainer *pContainer);

/** Show a Container and make it take the focus  (similar to gtk_window_present, but bypasses the WM focus steal prevention).
*@param pContainer the container
*/
void gldi_container_present (GldiContainer *pContainer);

/** Make this container a layer-shell surface. This can be used to properly position a dock on the screen on wlroots-based Wayland compositors
 *@param pContainer the container
 * 
 * See here for more details: https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-layer-shell-unstable-v1.xml
 * 
 * Below functions provide basic functionality to position the dock and place it above / below other windows.
 */
void gldi_container_init_layer (GldiContainer *pContainer);
/// Anchor this container to a screen edge (this is used instead of regular positioning)
void gldi_container_set_anchor (GldiContainer *pContainer, CairoDockPositionType iScreenBorder);
/// Set on which layer should this container appear
void gldi_container_set_layer (GldiContainer *pContainer, GldiContainerLayer iLayer);
/// determine if the display server is Wayland; this can be used by e.g. positioning
/// code that needs to work differently under Wayland; ideally, code that needs to
/// depend on this could be moved to the backends, but for now, that seems too complicated
gboolean gldi_container_is_wayland_backend ();

void gldi_container_manager_register_backend (GldiContainerManagerBackend *pBackend);


/** Wrapper around gdk_window_move_to_rect() that can be called anytime.
 * 	Originally, gdk_window_move_to_rect() can only be called after
 * 	the container's window has been realized (has been associated with a
 * 	GdkWindow). On the other hand, on Wayland with layer-shell, this needs
 * 	to be set up before the container's window is mapped (it is not possible
 * 	to move a popup after it was mapped).
 *  See https://developer.gnome.org/gdk3/stable/gdk3-Windows.html#gdk-window-move-to-rect
 * 	for the description of the parameters used. */
void gldi_container_move_to_rect (GldiContainer *pContainer,
									const GdkRectangle *rect,
									GdkGravity rect_anchor,
									GdkGravity window_anchor,
									GdkAnchorHints anchor_hints,
									gint rect_anchor_dx,
									gint rect_anchor_dy);

/** Calculate the parameters to pass to gldi_container_move_to_rect() to
 * 	poisition a child container on the given pContainer, pointing to pPointedIcon.
 * 	This can be used for subdocks, dialogs and menus. */
void gldi_container_calculate_rect (const GldiContainer* pContainer, const Icon* pPointedIcon,
			GdkRectangle *rect, GdkGravity* rect_anchor, GdkGravity* window_anchor);

/** Calculate the aimed point of sub-containers (menus and dialogs), based on
 * 	relative positioning. This can be used to point an arrow to the corresponding
 * 	icon. Works for menus (both X11 and Wayland) and dialogs (only Wayland).
 * Parameters:
 * 	pIcon            -- the icon that is pointed by the newly placed container
 * 	w, h             -- with and height of the new container
 * 	iMarginPosition  -- which side the margin (and the arrow) should be: 0: bottom; 1: top; 2: right; 3: left
 * 	iAimedX, iAimedY -- result is stored here; it is always in relative coordinates (to the new container); on X, this should be adjusted by the caller to use global coordinates
 */
void gldi_container_calculate_aimed_point (const Icon* pIcon, int w, int h,
	int iMarginPosition, int* iAimedX, int* iAimedY);


  ////////////
 // REDRAW //
////////////

void cairo_dock_set_default_rgba_visual (GtkWidget *pWidget);

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

/** Enable a Container to accept drag-and-drops.
* @param pContainer a container.
* @param pCallBack the function that will be called when some data is received.
* @param data data passed to the callback.
*/
#define gldi_container_enable_drop(pContainer, pCallBack, data) cairo_dock_allow_widget_to_receive_data (pContainer->pWidget, pCallBack, data)

void gldi_container_disable_drop (GldiContainer *pContainer);

/** Notify everybody that a drop has just occured.
* @param cReceivedData the dropped data.
* @param pPointedIcon the icon which was pointed when the drop occured.
* @param fOrder the order of the icon if the drop occured on it, or LAST_ORDER if the drop occured between 2 icons.
* @param pContainer the container of the icon
*/
void gldi_container_notify_drop_data (GldiContainer *pContainer, gchar *cReceivedData, Icon *pPointedIcon, double fOrder);


gboolean cairo_dock_emit_signal_on_container (GldiContainer *pContainer, const gchar *cSignal);
gboolean cairo_dock_emit_leave_signal (GldiContainer *pContainer);
gboolean cairo_dock_emit_enter_signal (GldiContainer *pContainer);

/** Build the main menu of a Container.
*@param icon the icon that was left-clicked, or NULL if none.
*@param pContainer the container that was left-clicked.
*@return the menu.
*/
GtkWidget *gldi_container_build_menu (GldiContainer *pContainer, Icon *icon);


  /////////////////
 // INPUT SHAPE //
/////////////////

cairo_region_t *gldi_container_create_input_shape (GldiContainer *pContainer, int x, int y, int w, int h);

// Note: if gdkwindow->shape == NULL, setting a NULL shape will do nothing
#define gldi_container_set_input_shape(pContainer, pShape) \
gtk_widget_input_shape_combine_region ((pContainer)->pWidget, pShape)


void gldi_register_containers_manager (void);

G_END_DECLS
#endif
