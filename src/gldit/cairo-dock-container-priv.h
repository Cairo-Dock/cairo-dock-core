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


#ifndef __CAIRO_DOCK_CONTAINER_PRIV__
#define  __CAIRO_DOCK_CONTAINER_PRIV__

#include "cairo-dock-container.h"
#include "cairo-dock-manager.h"

G_BEGIN_DECLS

// manager
typedef struct _GldiContainersParam GldiContainersParam;

#ifndef _MANAGER_DEF_
extern GldiContainersParam myContainersParam;
extern GldiManager myContainersMgr;
#endif

#define CD_DOUBLE_CLICK_DELAY 250  // ms

// params
struct _GldiContainersParam{
	//gboolean bUseFakeTransparency;
	gint iGLAnimationDeltaT;
	gint iCairoAnimationDeltaT;
	};

/// Definition of the Container backend. It defines some operations that should be, but are not, provided by GTK.
struct _GldiContainerManagerBackend {
	void (*reserve_space) (GldiContainer *pContainer, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x);
	int (*get_current_desktop_index) (GldiContainer *pContainer);
	void (*move) (GldiContainer *pContainer, int iNumDesktop, int iAbsolutePositionX, int iAbsolutePositionY);
	gboolean (*is_active) (GldiContainer *pContainer);
	void (*present) (GldiContainer *pContainer);
	/// extra functionality for Wayland / gtk_layer_shell
	/// Initialize layer-shell additions -- need to be called before mapping window first.
	/// cNamespace is used for main windows (i.e. not popups) and can be used to identify
	/// the container later (e.g. events received over a Wayland protocol or IPC)
	void (*init_layer) (GldiContainer *pContainer, const gchar *cNamespace);
	/// return if running on Wayland
	gboolean (*is_wayland) ();
	/// Set to keep the container's GtkWindow below or above other windows.
	/// On X11, this calls gtk_window_set_keep_below(); on Wayland, this tries to adjust the
	/// layer the window appears on.
	void (*set_keep_below) (GldiContainer *pContainer, gboolean bKeepBelow);
	/// Move and resize a root dock. On X11, this uses gdk_window_move_resize ().
	/// On Wayland, this uses gdk_window_resize () and layer-shell anchors based on the dock's orientation.
	void (*move_resize_dock) (CairoDock *pDock);
	/// set input shape on a window (Wayland + EGL needs special treatment)
	void (*set_input_shape) (GldiContainer *pContainer, cairo_region_t *pShape);
	/// set the monitor (screen) this container should appear -- required on Wayland
	void (*set_monitor) (GldiContainer *pContainer, int iNumScreen);
	/// recall hidden dock if the mouse is close to a screen edge
	/// update if we need to be looking at the screen edges (for any edge necessary)
	void (*update_polling_screen_edge) ();
	/// determines if it is possible to reserve space for a dock on a given screen with a given orientation; returns TRUE by default
	gboolean (*can_reserve_space) (int iNumScreen, gboolean bDirectionUp, gboolean bIsHorizontal);
	/// update the mouse position based on global coordinates -- only supported on X11
	void (*update_mouse_position) (GldiContainer *pContainer);
	/// backend-specific handling of leave / enter events on a dock
	/// on Wayland, these update iMousePositionType (this is the only place we can do this)
	/// the leave event handler should return if the mouse is really outside the dock
	gboolean (*dock_handle_leave) (CairoDock *pDock, GdkEventCrossing *pEvent);
	void (*dock_handle_enter) (CairoDock *pDock, GdkEventCrossing *pEvent);
	/// check if the mouse is inside the dock (basic case) and update iMousePositionType
	/// only supported on X11
	void (*dock_check_if_mouse_inside_linear) (CairoDock *pDock);
	/// adjust the aimed point (pointed to by a subdock, menu or dialog)
	/// AimedX and AimedY is already set by the caller to a relative position,
	/// only corrections need to be done here
	void (*adjust_aimed_point) (const Icon *pIcon, GtkWidget *pWidget, int w, int h,
		int iMarginPosition, gdouble fAlign, int *iAimedX, int *iAimedY);
};


  /////////////
 // WINDOW //
///////////

void cairo_dock_set_containers_non_sticky (void);

void cairo_dock_enable_containers_opacity (void);

/* Accumulate smooth scroll events and emit signals. */
void gldi_container_handle_scroll (GldiContainer *pContainer, Icon *pIcon, GdkEventScroll* pScroll);

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

/// determines if it is possible to reserve space for a dock on a given screen with a given orientation
gboolean gldi_container_can_reserve_space (int iNumScreen, gboolean bDirectionUp, gboolean bIsHorizontal);

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

/** Make this container a layer-shell surface. This can be used to properly position a dock on the screen on wlroots-based Wayland compositors
*@param pContainer the container
*@param cNamespace optionally the "namespace" to set, essentially similar to an app-id; set to "cairo-dock" if NULL
* 
* See here for more details: https://github.com/swaywm/wlr-protocols/blob/master/unstable/wlr-layer-shell-unstable-v1.xml
* 
* Below functions provide basic functionality to position the dock and place it above / below other windows.
*/
void gldi_container_init_layer (GldiContainer *pContainer, const gchar *cNamespace);

/** Move and resize a root dock. On X11, this uses gdk_window_move_resize ().
* On Wayland, this uses gdk_window_resize () and layer-shell anchors based on the dock's orientation.
* @param pDock the dock to position (must be a root dock)
*/
void gldi_container_move_resize_dock (CairoDock *pDock);

/** Move the dock to the given screen -- only used on Wayland. On X11, this is handled by adding an
* offset based on a global coordinate system in gldi_container_move_resize_dock ().
*@param pContainer the container to move. Must be a root dock.
*@param iNumScreen which screen to move to (offset to the monitors array from the desktop-manager)
*/
void gldi_container_set_screen (GldiContainer* pContainer, int iNumScreen);

void gldi_container_manager_register_backend (GldiContainerManagerBackend *pBackend);


/** Wrapper around gdk_window_move_to_rect() that can be called anytime.
 * 	Originally, gdk_window_move_to_rect() can only be called after
 * 	the container's window has been realized (has been associated with a
 * 	GdkWindow). On the other hand, on Wayland with layer-shell, this needs
 * 	to be set up before the container's window is mapped (it is not possible
 * 	to move a popup after it was mapped).
 *  See https://developer.gnome.org/gdk3/stable/gdk3-Windows.html#gdk-window-move-to-rect
 * 	for the description of the parameters used, except for the anchors which
 *  are interpreted as relative values compared to the width and height of
 *  the corresponding GdkWindow. */
void gldi_container_move_to_rect (GldiContainer *pContainer,
									const GdkRectangle *rect,
									GdkGravity rect_anchor,
									GdkGravity window_anchor,
									GdkAnchorHints anchor_hints,
									gdouble rel_anchor_dx,
									gdouble rel_anchor_dy);

/** Calculate the parameters to pass to gldi_container_move_to_rect() to
 * 	position a child container on the given pContainer, pointing to pPointedIcon.
 * 	This can be used for subdocks, dialogs and menus.
 *  The bSkipLabel parameter controls whether to leave space for an icon's label on a horizontal dock
 *  (should be TRUE for subdocks and dialogs, FALSE for menus). */
void gldi_container_calculate_rect (const GldiContainer* pContainer, const Icon* pPointedIcon,
			GdkRectangle *rect, GdkGravity* rect_anchor, GdkGravity* window_anchor, gboolean bSkipLabel);

/** Calculate the aimed point of sub-containers (menus and dialogs), based on
 * 	relative positioning. This can be used to point an arrow to the corresponding
 * 	icon. Works for menus and dialogs.
 * Parameters:
 * 	pIcon            -- the icon that is pointed by the newly placed container
 *  pWidget          -- GtkWidget of the new container
 * 	w, h             -- with and height of the new container
 * 	iMarginPosition  -- which side the margin (and the arrow) should be: 0: bottom; 1: top; 2: right; 3: left
 * 	iAimedX, iAimedY -- result is stored here:
 *        on Wayland, this is relative to the parent container (if exists, otherwise, relative to pWidget)
 *        on X11, this is in global coordinates
 */
void gldi_container_calculate_aimed_point (const Icon *pIcon, GtkWidget *pWidget, int w, int h,
	int iMarginPosition, gdouble fAlign, int *iAimedX, int *iAimedY);

/** Helper for the above, calculates position along the midpoint of the given edge. */
void gldi_container_calculate_aimed_point_base (int w, int h, int iMarginPosition,
	gdouble fAlign, int *iAimedX, int *iAimedY);

/// update looking at the screen edges (for any edge necessary)
void gldi_container_update_polling_screen_edge (void);
/// check whether we can detect the mouse hitting the screen edges (for the purpose of recalling hidden docks)
gboolean gldi_container_can_poll_screen_edge (void);

/// Set to keep the container's GtkWindow below or above other windows.
/// On X11, this calls gtk_window_set_keep_below(); on Wayland, this tries to adjust the
/// layer the window appears on.
void gldi_container_set_keep_below (GldiContainer *pContainer, gboolean bKeepBelow);


/// extras required for tracking mouse position:
/// backend-specific handling of leave / enter events on a dock
/// on Wayland, these update iMousePositionType (this is the only place we can do this)
/// the leave event handler should return if the mouse is really outside the dock
gboolean gldi_container_dock_handle_leave (CairoDock *pDock, GdkEventCrossing *pEvent);
void gldi_container_dock_handle_enter (CairoDock *pDock, GdkEventCrossing *pEvent);

/// check if the mouse is inside the dock (basic case) and update iMousePositionType
/// only supported on X11
void gldi_container_dock_check_if_mouse_inside_linear (CairoDock *pDock);

/// return whether new code (using gdk_window_move_to_rect () and friends) should be
/// used to position subdocks, menus and dialogs
/// on Wayland, it always returns TRUE, on X11, it is based on the setting in
/// System / X11_new_rendering_code
gboolean gldi_container_use_new_positioning_code ();


  ////////////
 // REDRAW //
////////////

void cairo_dock_set_default_rgba_visual (GtkWidget *pWidget);

/** Enable a Container to accept drag-and-drops.
* @param pContainer a container.
* @param pCallBack the function that will be called when some data is received.
* @param data data passed to the callback.
*/
#define gldi_container_enable_drop(pContainer, pCallBack, data) cairo_dock_allow_widget_to_receive_data (pContainer->pWidget, pCallBack, data)

void gldi_container_disable_drop (GldiContainer *pContainer); // not used at all

/// Get the GdkAtom used internally from dragging icons between docks
GdkAtom gldi_container_icon_dnd_atom (void);

/** Notify everybody that a drop has just occurred.
* @param cReceivedData the dropped data.
* @param pPointedIcon the icon which was pointed when the drop occurred.
* @param fOrder the order of the icon if the drop occurred on it, or LAST_ORDER if the drop occurred between 2 icons.
* @param pContainer the container of the icon
*/
void gldi_container_notify_drop_data (GldiContainer *pContainer, gchar *cReceivedData, Icon *pPointedIcon, double fOrder);


  /////////////////
 // INPUT SHAPE //
/////////////////

cairo_region_t *gldi_container_create_input_shape (GldiContainer *pContainer, int x, int y, int w, int h);

// Note: if gdkwindow->shape == NULL, setting a NULL shape will do nothing
void gldi_container_set_input_shape(GldiContainer *pContainer, cairo_region_t *pShape);


void gldi_register_containers_manager (void);


G_END_DECLS
#endif

