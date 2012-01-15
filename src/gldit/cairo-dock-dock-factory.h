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
#include "cairo-dock-desktop-file-factory.h"
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

#define CAIRO_DOCK_UPDATE_DOCK_SIZE TRUE
#define CAIRO_DOCK_ANIMATE_ICON TRUE
#define CAIRO_DOCK_INSERT_SEPARATOR TRUE

typedef void (*CairoDockComputeSizeFunc) (CairoDock *pDock);
typedef Icon * (*CairoDockCalculateIconsFunc) (CairoDock *pDock);
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
	CairoContainer container;
	/// the list of icons.
	GList* icons;
	/// Set to TRUE for the main dock (the first to be created, and the one containing the taskbar).
	gboolean bIsMainDock;
	/// number of icons pointing on the dock (0 means it is a root dock, >0 a sub-dock).
	gint iRefCount;
	
	//\_______________ Config parameters.
	gint iGapX;  // ecart de la fenetre par rapport au bord de l'ecran.
	gint iGapY;  // decalage de la fenetre par rapport au point d'alignement sur le bord de l'ecran.
	gdouble fAlign;  // alignment, between 0 and 1, on the screen's edge.
	/// visibility.
	CairoDockVisibility iVisibility;
	/// Horizontal offset of the screen where the dock lives, according to Xinerama.
	gint iScreenOffsetX;
	/// Vertical offset of the screen where the dock lives, according to Xinerama.
	gint iScreenOffsetY;
	/// number of the screen the dock is placed on (Xinerama).
	gint iNumScreen;
	
	/// maximum height of the icons.
	gdouble iMaxIconHeight;
	/// width of the dock, only taking into account an alignment of the icons.
	gdouble fFlatDockWidth;
	
	gint iMaxLabelWidth;
	gint iMinLeftMargin;
	gint iMinRightMargin;
	gint iMaxLeftMargin;
	gint iMaxRightMargin;
	gint iLeftMargin;
	gint iRightMargin;
	
	//\_______________ current state of the dock.
	gboolean bAutoHide;  // auto-hide activated.
	gint iMagnitudeIndex;  // indice de calcul du coef multiplicateur de l'amplitude de la sinusoide (entre 0 et CAIRO_DOCK_NB_MAX_ITERATIONS).
	/// (un)folding factor, between 0(unfolded) to 1(folded). It's up to the renderer on how to make use of it.
	gdouble fFoldingFactor;
	gint iAvoidingMouseIconType;// type d'icone devant eviter la souris, -1 si aucun.
	gdouble fAvoidingMouseMargin;// marge d'evitement de la souris, en fraction de la largeur d'an icon (entre 0 et 0.5)
	GList *pFirstDrawnElement;// pointeur sur le 1er element de la liste des icones a etre dessine, en partant de la gauche.
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
	/// whether the menu is visible (to keep the dock on high position).
	gboolean bMenuVisible;
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
	GldiShape* pShapeBitmap;
	/// input shape of the window when the dock is hidden.
	GldiShape* pHiddenShapeBitmap;
	/// input shape of the window when the dock is active (NULL to cover all dock).
	GldiShape* pActiveShapeBitmap;
	
	GLuint iRedirectedTexture;
	GLuint iFboId;
	
	//\_______________ background.
	/// whether the dock should use the global background parameters.
	gboolean bGlobalBg;
	/// path to an image, or NULL
	gchar *cBgImagePath;
	/// whether to repeat the image as a pattern, or to stretch it to fill the dock.
	gboolean bBgImageRepeat;
	/// first color of the gradation
	gdouble fBgColorBright[4];
	/// second color of the gradation
	gdouble fBgColorDark[4];
	/// Background image buffer of the dock.
	CairoDockImageBuffer backgroundBuffer;
	gboolean bExtendedMode;
	gint iOffsetForExtend;
	
	/// icon size, as specified in the config of the dock
	gint iIconSize;
	guint iSidUpdateDockSize;
	gint reserved[2];
};


/** Say if a Container is a Dock.
* @param pContainer the container.
* @return TRUE if the container is a Dock.
*/
#define CAIRO_DOCK_IS_DOCK(pContainer) (pContainer != NULL && ((CairoContainer*)pContainer)->iType == CAIRO_DOCK_TYPE_DOCK)

/** Cast a Container into a Dock.
* @param pDock the container to consider as a dock.
* @return the dock.
*/
#define CAIRO_DOCK(pDock) ((CairoDock *)pDock)


CairoDock *cairo_dock_new_dock (void);

void cairo_dock_free_dock (CairoDock *pDock);

void cairo_dock_make_sub_dock (CairoDock *pDock, CairoDock *pParentDock, const gchar *cRendererName);

/** Insert an icon into a dock.
* Do nothing if the icon already exists inside the dock.
* @param icon the icon to be inserted. It should have been filled beforehand.
* @param pDock the dock to insert inside.
* @param bUpdateSize TRUE to update the size of the dock after the insertion.
* @param bAnimated TRUE to arm the icon's animation for insertion.
* @param bInsertSeparator TRUE to insert an automatic separator if needed.
* @param pCompareFunc a sorting function to place the new icon amongst the others, or NULL to sort by group/order.
*/
void cairo_dock_insert_icon_in_dock_full (Icon *icon, CairoDock *pDock, gboolean bUpdateSize, gboolean bAnimated, gboolean bInsertSeparator, GCompareFunc pCompareFunc);

/** Insert an icon into a dock.
* Insert an automatic separator if needed. Do nothing if the icon already exists inside the dock.
* @param icon the icon to be inserted. It should have been filled beforehand.
* @param pDock the dock to insert inside.
* @param bUpdateSize TRUE to update the size of the dock after the insertion.
* @param bAnimated TRUE to arm the icon's animation for insertion.
*/
#define cairo_dock_insert_icon_in_dock(icon, pDock, bUpdateSize, bAnimated) cairo_dock_insert_icon_in_dock_full (icon, pDock, bUpdateSize, bAnimated, TRUE, NULL)

/** Detach an icon from its dock. The icon is not destroyed, and can be directly re-inserted in another container; it keeps its sub-dock, but looses its dialogs. Do nothing if the icon doesn't exist inside the dock.
*@param icon the icon to detach.
*@param pDock the dock containing the icon.
*@param bCheckUnusedSeparator TRUE to check and remove unnecessary separators.
*@return TRUE if the icon has been detached.
*/
gboolean cairo_dock_detach_icon_from_dock_full (Icon *icon, CairoDock *pDock, gboolean bCheckUnusedSeparator);
#define cairo_dock_detach_icon_from_dock(icon, pDock) cairo_dock_detach_icon_from_dock_full (icon, pDock, TRUE)

void cairo_dock_remove_icon_from_dock_full (CairoDock *pDock, Icon *icon, gboolean bCheckUnusedSeparator);

/** Completely remove an icon from the dock, that is to say detach the icon, and remove all links with Cairo-Dock : its .desktop is deleted, its module is deactivated, and its Xid is removed from the Taskbar (its class is handled too).
* Unnecessary separators are not tested.
* The icon is not yet destroyed, but looses its sub-dock in case of a container launcher.
*@param pDock the dock containing the icon, or NULL if the icon is already detached.
*@param icon the icon to be removed.
*/
#define cairo_dock_remove_one_icon_from_dock(pDock, icon) cairo_dock_remove_icon_from_dock_full (pDock, icon, FALSE)

/** Completely remove an icon from the dock, that is to say detach the icon, and  remove all links with Cairo-Dock : its .desktop is deleted, its module is deactivated, and its Xid is removed from the Taskbar (its class is handled too).
* Unnecessary separators are removed as well.
* The icon is not yet destroyed, but looses its sub-dock in case of a container launcher.
*@param pDock the dock containing the icon, or NULL if the icon is already detached.
*@param icon the icon to be removed.
*/
#define cairo_dock_remove_icon_from_dock(pDock, icon) cairo_dock_remove_icon_from_dock_full (pDock, icon, TRUE)

/** Remove and destroy all automatic separators inside a dock.
*@param pDock the dock.
*/
void cairo_dock_remove_automatic_separators (CairoDock *pDock);

/** Add automatic separators between the different groups of icons inside a dock.
*@param pDock the dock.
*/
void cairo_dock_insert_automatic_separators_in_dock (CairoDock *pDock);

Icon *cairo_dock_add_new_launcher_by_uri_or_type (const gchar *cExternDesktopFileURI, CairoDockDesktopFileType iType, CairoDock *pReceivingDock, double fOrder);

/** Add a launcher from a common desktop file : create and add the corresponding .desktop file with the others, load the corresponding icon, and insert it inside a dock with an animtion.
*@param cExternDesktopFileURI path to a desktop file.
*@param pReceivingDock the dock that will hold the new launcher.
*@param fOrder the order of the icon inside the dock.
*@return the newly created Icon corresponding to the file, or NULL if an error occured.
*/
#define cairo_dock_add_new_launcher_by_uri(cExternDesktopFileURI, pReceivingDock, fOrder) cairo_dock_add_new_launcher_by_uri_or_type (cExternDesktopFileURI, CAIRO_DOCK_DESKTOP_FILE_FOR_LAUNCHER, pReceivingDock, fOrder)

/** Add an empty default launcher of a given type : create and add the corresponding .desktop file with the others, load the corresponding icon, and insert it inside a dock with an animtion. The launcher is then suitable for being edited by the user to add real properties.
*@param iType type of the launcher.
*@param pReceivingDock the dock that will hold the new launcher.
*@param fOrder the order of the icon inside the dock.
*@param iGroup the group it will belong to
*@return the newly created Icon corresponding to the type, or NULL if an error occured.
*/
#define cairo_dock_add_new_launcher_by_type(iType, pReceivingDock, fOrder) cairo_dock_add_new_launcher_by_uri_or_type (NULL, iType, pReceivingDock, fOrder)

/** Remove all icons from a dock (and its sub-docks). If the receiving dock is NULL, the icons are destroyed and removed from the current theme itself.
*@param pDock a dock.
*@param pReceivingDock the dock that will receive the icons, or NULL to destroy and remove the icons.
*@param cReceivingDockName name of the receiving dock.
*/
void cairo_dock_remove_icons_from_dock (CairoDock *pDock, CairoDock *pReceivingDock, const gchar *cReceivingDockName);


void cairo_dock_create_redirect_texture_for_dock (CairoDock *pDock);

G_END_DECLS
#endif
