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


#ifndef __CAIRO_DOCK_FACTORY__
#define  __CAIRO_DOCK_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-style-facility.h"  // GldiColor
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-container.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-dock-factory.h This class defines the Docks, and gives the way to create, destroy, and fill them.
*
* A dock is a container that holds a set of icons and a renderer (also known as view).
*
* It has the ability to be placed anywhere on the screen edges and to resize itself automatically to fit the screen's size.
*
* It supports internal dragging of its icons with the mouse, and dragging of itself with alt+mouse.
*
* A dock can be either a main-dock (not linked to any icon) or a sub-dock (linked to an icon of another dock), and there can be as many docks of each sort as you want.
*/

typedef enum {
	CAIRO_DOCK_BOTTOM = 0,
	CAIRO_DOCK_TOP,
	CAIRO_DOCK_RIGHT,
	CAIRO_DOCK_LEFT,
	CAIRO_DOCK_INSIDE_SCREEN,
	CAIRO_DOCK_NB_POSITIONS
	} CairoDockPositionType;

#define CAIRO_DOCK_ANIMATE_ICON TRUE 

typedef void (*CairoDockComputeSizeFunc) (CairoDock *pDock);
typedef Icon* (*CairoDockCalculateIconsFunc) (CairoDock *pDock);
typedef void (*CairoDockRenderFunc) (cairo_t *pCairoContext, CairoDock *pDock);
typedef void (*CairoDockRenderOptimizedFunc) (cairo_t *pCairoContext, CairoDock *pDock, GdkRectangle *pArea);
typedef void (*CairoDockSetSubDockPositionFunc) (Icon *pPointedIcon, CairoDock *pParentDock);
typedef void (*CairoDockGLRenderFunc) (CairoDock *pDock);
typedef void (*CairoDockRenderFreeDataFunc) (CairoDock *pDock);
typedef void (*CairoDockSetInputShapeFunc) (CairoDock *pDock);
typedef void (*CairoDockSetIconSizeFunc) (Icon *pIcon, CairoDock *pDock);

/// Dock's renderer, also known as 'view'.
struct _CairoDockRenderer {
	/// function that computes the sizes of a dock.
	CairoDockComputeSizeFunc compute_size;
	/// function that computes all the icons' parameters.
	CairoDockCalculateIconsFunc calculate_icons;
	/// rendering function (cairo)
	CairoDockRenderFunc render;
	/// optimized rendering function (cairo) that only redraw a part of the dock.
	CairoDockRenderOptimizedFunc render_optimized;
	/// rendering function (OpenGL, optionnal).
	CairoDockGLRenderFunc render_opengl;
	/// function that computes the position of the dock when it's a sub-dock.
	CairoDockSetSubDockPositionFunc set_subdock_position;
	/// function called when the renderer is unset from the dock.
	CairoDockRenderFreeDataFunc free_data;
	/// function called when the input zones are defined.
	CairoDockSetInputShapeFunc update_input_shape;
	/// function called to define the size of an icon, or NULL to let the container handles that.
	CairoDockSetIconSizeFunc set_icon_size;
	/// TRUE if the view uses the OpenGL stencil buffer.
	gboolean bUseStencil;
	/// TRUE is the view uses reflects.
	gboolean bUseReflect;
	/// name displayed in the GUI (translated).
	const gchar *cDisplayedName;
	/// path to a readme file that gives a short description of the view.
	gchar *cReadmeFilePath;
	/// path to a preview image.
	gchar *cPreviewFilePath;
};

typedef enum {
	CAIRO_DOCK_MOUSE_INSIDE,
	CAIRO_DOCK_MOUSE_ON_THE_EDGE,
	CAIRO_DOCK_MOUSE_OUTSIDE
	} CairoDockMousePositionType;

typedef enum {
	CAIRO_DOCK_INPUT_ACTIVE,
	CAIRO_DOCK_INPUT_AT_REST,
	CAIRO_DOCK_INPUT_HIDDEN
	} CairoDockInputState;

typedef enum {
	CAIRO_DOCK_VISI_KEEP_ABOVE=0,
	CAIRO_DOCK_VISI_RESERVE,
	CAIRO_DOCK_VISI_KEEP_BELOW,
	CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP,
	CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY,
	CAIRO_DOCK_VISI_AUTO_HIDE,
	CAIRO_DOCK_VISI_SHORTKEY,
	CAIRO_DOCK_NB_VISI
	} CairoDockVisibility;

/// Definition of a Dock, which derives from a Container.
struct _CairoDock {
	/// container.
	GldiContainer container;
	/// the list of icons.
	GList* icons;
	/// Set to TRUE for the main dock (the first to be created, and the one containing the taskbar).
	gboolean bIsMainDock;
	/// number of icons pointing on the dock (0 means it is a root dock, >0 a sub-dock).
	gint iRefCount;
	/// unique name of the dock
	gchar *cDockName;
	
	//\_______________ Config parameters.
	// position
	gint iGapX;  // ecart de la fenetre par rapport au bord de l'ecran.
	gint iGapY;  // decalage de la fenetre par rapport au point d'alignement sur le bord de l'ecran.
	gdouble fAlign;  // alignment, between 0 and 1, on the screen's edge.
	/// visibility.
	CairoDockVisibility iVisibility;
	/// number of the screen the dock is actually placed on (between 0 and g_desktopGeometry.iNbScreens - 1)
	/// might differ from iScreenReq if the requested screen is not available; filled out when updating the dock's position
	gint iNumScreen;
	/// number of the screen requested by the user
	gint iScreenReq;
	/// name of the screen requested by the user -- TODO: not used yet, should implement saving screen names !
	gchar *cScreenReq;
	
	// icons
	/// icon size, as specified in the config of the dock
	gint iIconSize;
	/// whether the dock should use the global icons size parameters.
	gboolean bGlobalIconSize;
	// background
	/// whether the dock should use the global background parameters.
	gboolean bGlobalBg;
	/// path to an image, or NULL
	gchar *cBgImagePath;
	/// whether to repeat the image as a pattern, or to stretch it to fill the dock.
	gboolean bBgImageRepeat;
	/// first color of the gradation
	GldiColor fBgColorBright;
	/// second color of the gradation
	GldiColor fBgColorDark;
	/// Background image buffer of the dock.
	CairoDockImageBuffer backgroundBuffer;
	gboolean bExtendedMode;
	
	//\_______________ current state of the dock.
	gboolean bAutoHide;  // auto-hide activated.
	gint iMagnitudeIndex;  // indice de calcul du coef multiplicateur de l'amplitude de la sinusoide (entre 0 et CAIRO_DOCK_NB_MAX_ITERATIONS).
	/// (un)folding factor, between 0(unfolded) to 1(folded). It's up to the renderer on how to make use of it.
	gdouble fFoldingFactor;
	gint iAvoidingMouseIconType;// type d'icone devant eviter la souris, -1 si aucun.
	gdouble fAvoidingMouseMargin;// marge d'evitement de la souris, en fraction de la largeur d'an icon (entre 0 et 0.5)
	gdouble fDecorationsOffsetX;// decalage des decorations pour les faire suivre la souris.
	// counter for the fade out effect.
	gint iFadeCounter;
	// direction of the fade out effect.
	gboolean bFadeInOut;
	/// counter for auto-hide.
	gdouble fHideOffset;
	/// counter for the post-hiding animation for icons always visible.
	gdouble fPostHideOffset;
	
	/// Whether the dock is in a popped up state or not.
	gboolean bIsBelow;
	/// TRUE if the dock has a modal window (menu, dialog, etc), that will block it.
	gint bHasModalWindow;
	/// whether the user is dragging something over the dock.
	gboolean bIsDragging;
	/// Backup of the auto-hide state before quick-hide.
	gboolean bTemporaryHidden;
	/// whether mouse can't enter into the dock.
	gboolean bEntranceDisabled;
	/// whether the dock is shrinking down.
	gboolean bIsShrinkingDown;
	/// whether the dock is growing up.
	gboolean bIsGrowingUp;
	/// whether the dock is hiding.
	gboolean bIsHiding;
	/// whether the dock is showing.
	gboolean bIsShowing;
	/// whether an icon is being dragged away from the dock
	gboolean bIconIsFlyingAway;
	/// whether icons in the dock can be dragged with the mouse (inside and outside of the dock).
	gboolean bPreventDraggingIcons;
	gboolean bWMIconsNeedUpdate;
	
	/// maximum height of the icons.
	gdouble iMaxIconHeight;
	/// width of the dock, only taking into account an alignment of the icons.
	gdouble fFlatDockWidth;
	gint iOffsetForExtend;
	gint iMaxLabelWidth;
	gint iMinLeftMargin;
	gint iMinRightMargin;
	gint iMaxLeftMargin;
	gint iMaxRightMargin;
	gint iLeftMargin;
	gint iRightMargin;
	
	//\_______________ Source ID of events running on the dock.
	/// Source ID for window resizing.
	guint iSidMoveResize;
	/// Source ID for window popping down to the bottom layer.
	guint iSidUnhideDelayed;
	/// Source ID of the timer that delays the "leave" event.
	guint iSidLeaveDemand;
	/// Source ID for pending update of WM icons geometry.
	guint iSidUpdateWMIcons;
	/// Source ID for hiding back the dock.
	guint iSidHideBack;
	/// Source ID for loading the background.
	guint iSidLoadBg;
	/// Source ID to destroy an empty main dock.
	guint iSidDestroyEmptyDock;
	/// Source ID for shrinking down the dock after a mouse event.
	guint iSidTestMouseOutside;
	/// Source ID for updating the dock's size and icons layout.
	guint iSidUpdateDockSize;
	
	//\_______________ Renderer and fields set by it.
	// nom de la vue, utile pour (re)charger les fonctions de rendu posterieurement a la creation du dock.
	gchar *cRendererName;
	/// current renderer, never NULL.
	CairoDockRenderer *pRenderer;
	/// data that can be used by the renderer.
	gpointer pRendererData;
	/// Set to TRUE by the renderer if one can drop between 2 icons.
	gboolean bCanDrop;
	/// set by the view to say if the mouse is currently on icons, on the egde, or outside of icons.
	CairoDockMousePositionType iMousePositionType;
	/// width of the dock at rest.
	gint iMinDockWidth;
	/// height of the dock at rest.
	gint iMinDockHeight;
	/// maximum width of the dock.
	gint iMaxDockWidth;
	/// maximum height of the dock.
	gint iMaxDockHeight;
	/// width of background decorations, set by the renderer.
	gint iDecorationsWidth;
	/// height of background decorations, set by the renderer.
	gint iDecorationsHeight;
	/// maximal magnitude of the zoom, between 0 and 1.
	gdouble fMagnitudeMax;
	/// width of the active zone of the dock.
	gint iActiveWidth;
	/// height of the active zone of the dock.
	gint iActiveHeight;
	
	//\_______________ input shape.
	/// state of the input shape (active, at rest, hidden).
	CairoDockInputState iInputState;
	/// input shape of the window when the dock is at rest.
	cairo_region_t* pShapeBitmap;
	/// input shape of the window when the dock is hidden.
	cairo_region_t* pHiddenShapeBitmap;
	/// input shape of the window when the dock is active (NULL to cover all dock).
	cairo_region_t* pActiveShapeBitmap;
	
	//\_______________ OpenGL.
	GLuint iRedirectedTexture;
	GLuint iFboId;
	gboolean bNeedSizeUpdate; // set to TRUE whether we could not do size-related updated in the configure handler
	/// last buffer scale factor set by us
	gint iScaleFactor;
	
	/// any data necessary for the dock visibility backend to work -- managed by the backend
	gpointer pVisibilityData;
	
	/// List of applets that are currently connected to this dock. This prevents the dock being deleted as long
	/// as it is not empty. Only modified by module-instance-manager.
	/// It is necessary to keep this separately from icons, as some applets might (temporarily or permanently)
	/// remove their icon even while keeping their association with the dock, resulting in a crash if the dock
	/// is then subsequently freed; e.g. Cairo-Penguin or Status-Notifier.
	GList *applets;
	
	gpointer reserved[2];
};


/** Say if an object is a Dock.
*@param obj the object.
*@return TRUE if the object is a Dock.
*/
#define GLDI_OBJECT_IS_DOCK(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myDockObjectMgr)
#define CAIRO_DOCK_IS_DOCK GLDI_OBJECT_IS_DOCK

/** Cast a Container into a Dock.
* @param p the container to consider as a dock.
* @return the dock.
*/
#define CAIRO_DOCK(p) ((CairoDock *)p)


void cairo_dock_freeze_docks (gboolean bFreeze);


/** Create a new root dock.
*@param cDockName the name that identifies the dock
*@return the new dock.
*/
CairoDock *gldi_dock_new (const gchar *cDockName);

/** Notify pDock that the mouse has possibly left it, just as if it
*  received the "leave-notify-event" signal from GTK. Use this e.g.
*  when a subdock, dialog or menu is closed and the dock should shrink
*  down if the mouse is not over it.
*@param pDock a dock.
*/
void gldi_dock_leave_synthetic (CairoDock *pDock);

/** Notify pDock that the mouse has possibly entered it, just as if it
*  received the "enter-notify-event" signal from GTK. Use this e.g.
*  when a dialog is shown to prevent the dock from hiding.
*@param pDock a dock.
*/
void gldi_dock_enter_synthetic (CairoDock *pDock);

G_END_DECLS
#endif
