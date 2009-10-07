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

typedef enum {
	CAIRO_DOCK_MAX_SIZE,
	CAIRO_DOCK_NORMAL_SIZE,
	CAIRO_DOCK_MIN_SIZE
	} CairoDockSizeType;

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
	/// function called when the renderer is unset from the dock.
	CairoDockRenderFreeDataFunc free_data;
};

typedef enum {
	CAIRO_DOCK_MOUSE_INSIDE,
	CAIRO_DOCK_MOUSE_ON_THE_EDGE,
	CAIRO_DOCK_MOUSE_OUTSIDE
	} CairoDockMousePositionType;

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
	/// ecart de la fenetre par rapport au bord de l'ecran.
	gint iGapX;
	/// decalage de la fenetre par rapport au point d'alignement sur le bord de l'ecran.
	gint iGapY;
	/// alignment, between 0 and 1, on the screen's edge.
	gdouble fAlign;
	/// whether the dock automatically hides itself or not.
	gboolean bAutoHide;
	/// Horizontal offset of the screen where the dock lives, according to Xinerama.
	gint iScreenOffsetX;
	/// Vertical offset of the screen where the dock lives, according to Xinerama.
	gint iScreenOffsetY;
	
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

	//\_______________ current state
	/// pour faire defiler les icones avec la molette.
	gint iScrollOffset;
	/// indice de calcul du coef multiplicateur de l'amplitude de la sinusoide (entre 0 et CAIRO_DOCK_NB_MAX_ITERATIONS).
	gint iMagnitudeIndex;
	/// (un)folding factor, between 0(unfolded) to 1(folded). It's up to the renderer on how to make use of it.
	gdouble fFoldingFactor;
	/// type d'icone devant eviter la souris, -1 si aucun.
	gint iAvoidingMouseIconType;
	/// marge d'evitement de la souris, en fraction de la largeur d'an icon (entre 0 et 0.5)
	gdouble fAvoidingMouseMargin;
	/// pointeur sur le 1er element de la liste des icones a etre dessine, en partant de la gauche.
	GList *pFirstDrawnElement;
	/// decalage des decorations pour les faire suivre la souris.
	gdouble fDecorationsOffsetX;
	
	/// TRUE if the dock is at rest.
	gboolean bAtBottom;
	/// TRUE if the dock is in high position (maximum zoom).
	gboolean bAtTop;
	/// Whether the dock is in a popped up state or not.
	gboolean bPopped;
	/// TRUE when the menu is visible, to keep the dock on high position.
	gboolean bMenuVisible;
	/// TRUE if the user is dragging something over the dock.
	gboolean bIsDragging;
	/// Backup of the auto-hide state before quick-hide.
	gboolean bAutoHideInitialValue;
	/// TRUE if mouse can't enter into the dock.
	gboolean bEntranceDisabled;
	/// TRUE is the dock is shrinking down.
	gboolean bIsShrinkingDown;
	/// TRUE is the dock is growing up.
	gboolean bIsGrowingUp;
	
	//\_______________ Source ID of events running on the dock.
	/// Source ID of the hiding movement.
	guint iSidMoveDown;
	/// Source ID of the showing movement.
	guint iSidMoveUp;
	/// Source ID for window popping up to the top layer.
	guint iSidPopUp;
	/// Source ID for window popping down to the bottom layer.
	guint iSidPopDown;
	/// Source ID of the timer that delays the "leave" event.
	guint iSidLeaveDemand;
	
	//\_______________ Renderer and fields set by it.
	/// nom de la vue, utile pour (re)charger les fonctions de rendu posterieurement a la creation du dock.
	gchar *cRendererName;
	/// current renderer, never NULL.
	CairoDockRenderer *pRenderer;
	/// data that can be used by the renderer.
	gpointer pRendererData;
	/// Set to TRUE by the renderer if one can drop between 2 icons.
	gboolean bCanDrop;
	/// input zone set by the renderer when the dock is at rest.
	GdkRectangle inputArea;
	/// input shape of the window when the dock is at rest.
	GdkBitmap* pShapeBitmap;
	/// set by the view to say if the mouse is currently on icons, on the egde, or outside of icons.
	CairoDockMousePositionType iMousePositionType;
	/// minimum width of the dock.
	gint iMinDockWidth;
	/// minimum height of the dock.
	gint iMinDockHeight;
	/// maximum width of the dock.
	gint iMaxDockWidth;
	/// maximum height of the dock.
	gint iMaxDockHeight;
	/// width of background decorations.
	gint iDecorationsWidth;
	/// height of background decorations.
	gint iDecorationsHeight;
	/// maximal magnitude of the zoom, between 0 and 1.
	gdouble fMagnitudeMax;
	/// whether the position of icons for the WM is invalid.
	gboolean bWMIconseedsUptade;
	
	/// counter for the fade out effect.
	gint iFadeCounter;
	/// direction of the fade out effect.
	gboolean bFadeInOut;
	/// whether the input shape is set or not.
	gboolean bActive;  // TRUE <=> no input shape.
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


/** Create a new root dock.
* @param cDockName name of the dock, used to identify it quickly. If the name is already used, the corresponding dock is returned.
* @param cRendererName name of a renderer. If NULL, the default renderer will be applied.
* @return the newly allocated dock, to destroy with #cairo_dock_destroy_dock
*/
CairoDock *cairo_dock_create_new_dock (const gchar *cDockName, const gchar *cRendererName);

/*
* Desactive a dock : le rend inoperant, en detruisant tout ce qu'il contient, sauf sa liste d'icones.
* @param pDock the dock.
*/
void cairo_dock_deactivate_one_dock (CairoDock *pDock);

/** Decrease the number of pointing icons, and destroy the dock if no more icons point on it. Also destroy all of its sub-docks, and free any allocated ressources. Do nothing for the main dock, use #cairo_dock_free_all_docks for it.
* @param pDock the dock to be destroyed.
* @param cDockName its name.
* @param ReceivingDock a dock that will get its icons, or NULL to also destroy its icons.
* @param cReceivingDockName the name of the dock that will get its icons, or NULL if none is provided.
*/
void cairo_dock_destroy_dock (CairoDock *pDock, const gchar *cDockName, CairoDock *ReceivingDock, const gchar *cReceivingDockName);

/** Increase by 1 the number of pointing icons. If the dock was a root dock, it becomes a sub-dock.
* @param pDock a dock.
* @param pParentDock its parent dock, if it becomes a sub-dock, otherwise it can be NULL.
*/
void cairo_dock_reference_dock (CairoDock *pDock, CairoDock *pParentDock);

/** Create a new dock of type "sub-dock", and load a given list of icons inside. The list then belongs to the dock, so it must not be freeed after that. The buffers of each icon are loaded, so they just need to have an image filename and a name.
* @param pIconList a list of icons that will be loaded and inserted into the new dock.
* @param cDockName desired name for the new dock.
* @param pParentDock the parent dock.
* @return the newly allocated dock.
*/
CairoDock *cairo_dock_create_subdock_from_scratch (GList *pIconList, gchar *cDockName, CairoDock *pParentDock);

/** Load a set of .desktop files that define icons, and build the corresponding tree of docks.
* All the icons are created and placed inside their dock, which is created if necessary.
* In the end, each dock is computed and placed on the screen.
* @param pMainDock a dock, needed to have an initial cairo context (because icons are created before their container).
* @param cDirectory a folder containing some .desktop files.
*/
void cairo_dock_build_docks_tree_with_desktop_files (CairoDock *pMainDock, gchar *cDirectory);

/** Destroy all docks and all icons contained inside, and free all allocated ressources. Applets and Taskbar are stopped beforehand.
*/
void cairo_dock_free_all_docks (void);


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

/** Insert an icon into a dock, at the position given by its 'fOrder' field.
* Insert an automatic separator if needed. Do nothing if the icon already exists inside the dock.
* @param icon the icon to be inserted. It should have been filled beforehand.
* @param pDock the dock to insert inside.
* @param bUpdateSize TRUE to update the size of the dock after the insertion.
* @param bAnimated TRUE to arm the icon's animation for insertion.
*/
#define cairo_dock_insert_icon_in_dock(icon, pDock, bUpdateSize, bAnimated) cairo_dock_insert_icon_in_dock_full (icon, pDock, bUpdateSize, bAnimated, myIcons.bUseSeparator, NULL)

/** Detach an icon from its dock, removing the unnecessary separators. The icon is not destroyed, and can be directly re-inserted in another container; it keeps its sub-dock, but loose its dialogs. Do nothing if the icon doesn't exist inside the dock.
*@param icon the icon to detach.
*@param pDock the dock containing the icon.
*@param bCheckUnusedSeparator TRUE to check and remove unnecessary separators.
*@return TRUE if the icon has been detached.
*/
gboolean cairo_dock_detach_icon_from_dock (Icon *icon, CairoDock *pDock, gboolean bCheckUnusedSeparator);

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

/** Add automatic separators between the different types of icons inside a dock.
*@param pDock the dock.
*/
void cairo_dock_insert_separators_in_dock (CairoDock *pDock);

/** Add a launcher from a common desktop file : create and add the corresponding .desktop file with the others, load the corresponding icon, and insert it inside a dock with an animtion.
*@param cExternDesktopFileURI path to a desktop file.
*@param pReceivingDock the dock that will hold the new launcher.
*@param fOrder the order of the icon inside the dock.
*/
void cairo_dock_add_new_launcher_by_uri (const gchar *cExternDesktopFileURI, CairoDock *pReceivingDock, double fOrder);


G_END_DECLS
#endif
