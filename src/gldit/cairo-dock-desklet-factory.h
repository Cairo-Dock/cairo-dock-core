/**
* This file is a part of the Cairo-Dock project
* Login : <ctaf42@gmail.com>
* Started on  Sun Jan 27 18:35:38 2008 Cedric GESTES
* $Id$
*
* Author(s)
*  - Cedric GESTES <ctaf42@gmail.com>
*  - Fabrice REY
*
* Copyright : (C) 2008 Cedric GESTES
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

#ifndef __CAIRO_DESKLET_FACTORY_H__
#define  __CAIRO_DESKLET_FACTORY_H__

#include "cairo-dock-struct.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-container.h"
G_BEGIN_DECLS


/**
*@file cairo-dock-desklet-factory.h This class defines the Desklets, that are Widgets placed directly on your desktop.
* A Desklet is a container that holds 1 applet's icon plus an optionnal list of other icons and an optionnal GTK widget, has a decoration, suports several accessibility types (like Compiz Widget Layer), and has a renderer.
* Desklets can be resized or moved directly with the mouse, and can be rotated in the 3 directions of space.
* To actually create or destroy a Desklet, use the Desklet Manager's functoins in \ref cairo-dock-desklet-manager.h.
*/

/// Type of accessibility of a Desklet.
typedef enum {
	/// Normal, like normal window
	CAIRO_DESKLET_NORMAL = 0,
	/// always above
	CAIRO_DESKLET_KEEP_ABOVE,
	/// always below
	CAIRO_DESKLET_KEEP_BELOW,
	/// on the Compiz widget layer
	CAIRO_DESKLET_ON_WIDGET_LAYER,
	/// prevent other windows form overlapping it
	CAIRO_DESKLET_RESERVE_SPACE
	} CairoDeskletVisibility;

/// Decoration of a Desklet.
struct _CairoDeskletDecoration {
	const gchar *cDisplayedName;
	gchar *cBackGroundImagePath;
	gchar *cForeGroundImagePath;
	CairoDockLoadImageModifier iLoadingModifier;
	gdouble fBackGroundAlpha;
	gdouble fForeGroundAlpha;
	gint iLeftMargin;
	gint iTopMargin;
	gint iRightMargin;
	gint iBottomMargin;
	};

typedef struct _CairoDeskletAttr CairoDeskletAttr;

/// Configuration attributes of a Desklet.
struct _CairoDeskletAttr {
	// parent attributes
	GldiContainerAttr cattr;
	gboolean bDeskletUseSize;
	gint iDeskletWidth;
	gint iDeskletHeight;
	gint iDeskletPositionX;
	gint iDeskletPositionY;
	gboolean bPositionLocked;
	gint iRotation;
	gint iDepthRotationY;
	gint iDepthRotationX;
	gchar *cDecorationTheme;
	CairoDeskletDecoration *pUserDecoration;
	CairoDeskletVisibility iVisibility;
	gboolean bOnAllDesktops;
	gint iNumDesktop;
	gboolean bNoInput;
	Icon *pIcon;
} ;

typedef gpointer CairoDeskletRendererDataParameter;
typedef CairoDeskletRendererDataParameter* CairoDeskletRendererDataPtr;
typedef gpointer CairoDeskletRendererConfigParameter;
typedef CairoDeskletRendererConfigParameter* CairoDeskletRendererConfigPtr;
typedef struct {
	gchar *cName;
	CairoDeskletRendererConfigPtr pConfig;
} CairoDeskletRendererPreDefinedConfig;
typedef void (* CairoDeskletRenderFunc) (cairo_t *pCairoContext, CairoDesklet *pDesklet);
typedef void (*CairoDeskletGLRenderFunc) (CairoDesklet *pDesklet);
typedef gpointer (* CairoDeskletConfigureRendererFunc) (CairoDesklet *pDesklet, CairoDeskletRendererConfigPtr pConfig);
typedef void (* CairoDeskletLoadRendererDataFunc) (CairoDesklet *pDesklet);
typedef void (* CairoDeskletUpdateRendererDataFunc) (CairoDesklet *pDesklet, CairoDeskletRendererDataPtr pNewData);
typedef void (* CairoDeskletFreeRendererDataFunc) (CairoDesklet *pDesklet);
typedef void (* CairoDeskletCalculateIconsFunc) (CairoDesklet *pDesklet);
/// Definition of a Desklet's renderer.
struct _CairoDeskletRenderer {
	/// rendering function with libcairo.
	CairoDeskletRenderFunc 				render;
	/// rendering function with OpenGL.
	CairoDeskletGLRenderFunc 			render_opengl;
	/// get the configuration of the renderer from a set of config attributes.
	CairoDeskletConfigureRendererFunc 	configure;
	/// load the internal data of the renderer.
	CairoDeskletLoadRendererDataFunc 	load_data;
	/// free all internal data of the renderer.
	CairoDeskletFreeRendererDataFunc 	free_data;
	/// define the icons' size and load them.
	CairoDeskletCalculateIconsFunc 			calculate_icons;
	/// function called on each iteration of the rendering loop.
	CairoDeskletUpdateRendererDataFunc 	update;
	/// optionnal rendering function with OpenGL that only draws the bounding boxes of the icons (for picking).
	CairoDeskletGLRenderFunc 			render_bounding_box;
	/// An optionnal list of preset configs.
	GList *pPreDefinedConfigList;
};


/// Definition of a Desklet, which derives from a Container.
struct _CairoDesklet {
	//\________________ Core
	// container
	GldiContainer container;
	// Main icon (usually an applet)
	Icon *pIcon;
	// List of sub-icons (possibly NULL)
	GList *icons;
	// Renderer used to draw the desklet
	CairoDeskletRenderer *pRenderer;
	// data used by the renderer
	gpointer pRendererData;
	// The following function outclasses the corresponding function of the renderer. This is useful if you don't want to pick icons but some elements that you draw yourself on the desklet.
	CairoDeskletGLRenderFunc render_bounding_box;
	// ID of the object that was picked in case the previous function is not null.
	GLuint iPickedObject;
	
	//\________________ decorations
	gchar *cDecorationTheme;
	CairoDeskletDecoration *pUserDecoration;
	gint iLeftSurfaceOffset;
	gint iTopSurfaceOffset;
	gint iRightSurfaceOffset;
	gint iBottomSurfaceOffset;
	CairoDockImageBuffer backGroundImageBuffer;
	CairoDockImageBuffer foreGroundImageBuffer;
	
	//\________________ properties.
	gdouble fRotation;  // rotation.
	gdouble fDepthRotationY;
	gdouble fDepthRotationX;
	gboolean bFixedAttitude;
	gboolean bAllowNoClickable;
	gboolean bNoInput;
	GtkWidget *pInteractiveWidget;
	gboolean bPositionLocked;  // TRUE ssi on ne peut pas deplacer le widget a l'aide du simple clic gauche.
	
	//\________________ internal
	gint iSidWritePosition;  // un timer pour retarder l'ecriture dans le fichier lors des deplacements.
	gint iSidWriteSize;  // un timer pour retarder l'ecriture dans le fichier lors des redimensionnements.
	gint iDesiredWidth, iDesiredHeight;  // taille a atteindre (fixee par l'utilisateur dans le.conf)
	gint iKnownWidth, iKnownHeight;  // taille connue par l'applet associee.
	gboolean bSpaceReserved;  // l'espace est actuellement reserve.
	gboolean bAllowMinimize;  // TRUE to allow the desklet to be minimized once. The flag is reseted to FALSE after the desklet has minimized.
	gint iMouseX2d;  // X position of the pointer taking into account the 2D transformations on the desklet (for an opengl renderer, you'll have to use the picking).
	gint iMouseY2d;  // Y position of the pointer taking into account the 2D transformations on the desklet (for an opengl renderer, you'll have to use the picking).
	GTimer *pUnmapTimer;
	gdouble fButtonsAlpha;  // pour le fondu des boutons lors de l'entree dans le desklet.
	gboolean bButtonsApparition;  // si les boutons sont en train d'apparaitre ou de disparaitre.
	gboolean bGrowingUp;  // pour le zoom initial.
	
	//\________________ current state
	gboolean rotatingY;
	gboolean rotatingX;
	gboolean rotating;
	gboolean retaching;  // rattachement au dock.
	gboolean making_transparent;  // no input.
	gboolean moving;  // pour le deplacement manuel de la fenetre.
	gboolean bClicked;
	guint time;  // date du clic.
	
	CairoDeskletVisibility iVisibility;
	gpointer reserved[4];
};

/** Say if an object is a Desklet.
*@param obj the object.
*@return TRUE if the object is a Desklet.
*/
#define GLDI_OBJECT_IS_DESKLET(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myDeskletObjectMgr)
#define CAIRO_DOCK_IS_DESKLET GLDI_OBJECT_IS_DESKLET

/** Cast a Container into a Desklet.
*@param pContainer the container.
*@return the desklet.
*/
#define CAIRO_DESKLET(pContainer) ((CairoDesklet *)pContainer)

#define cairo_dock_desklet_is_free(pDesklet) (! (pDesklet->bPositionLocked || pDesklet->bFixedAttitude))

/** Create a new desklet.
*@param attr the attributes of the desklet
*@return the desklet.
*/
CairoDesklet *gldi_desklet_new (CairoDeskletAttr *attr);

void gldi_desklet_init_internals (CairoDesklet *pDesklet);

/* Apply its settings on a desklet:
* it places it, resizes it, sets up its accessibility, locks its position, and sets up its decorations.
*@param pDesklet the desklet.
*@param pAttribute the attributes to configure the desklet.
*/
void gldi_desklet_configure (CairoDesklet *pDesklet, CairoDeskletAttr *pAttribute);

#define gldi_desklet_set_static(pDesklet) (pDesklet)->bFixedAttitude = TRUE

#define gldi_desklet_allow_no_clickable(pDesklet) (pDesklet)->bAllowNoClickable = TRUE

void gldi_desklet_load_desklet_decorations (CairoDesklet *pDesklet);

void gldi_desklet_decoration_free (CairoDeskletDecoration *pDecoration);


/** Add a GtkWidget to a desklet. Only 1 widget is allowed per desklet, if you need more, you can just use a GtkContainer, and place as many widget as you want inside.
*@param pInteractiveWidget the widget to add.
*@param pDesklet the desklet.
*@param iRightMargin right margin, in pixels, useful to keep a clickable zone on the desklet, or 0 if you don't want a margin.
*/
void gldi_desklet_add_interactive_widget_with_margin (CairoDesklet *pDesklet, GtkWidget *pInteractiveWidget, int iRightMargin);
/** Add a GtkWidget to a desklet. Only 1 widget is allowed per desklet, if you need more, you can just use a GtkContainer, and place as many widget as you want inside.
*@param pInteractiveWidget the widget to add.
*@param pDesklet the desklet.
*/
#define gldi_desklet_add_interactive_widget(pDesklet, pInteractiveWidget) gldi_desklet_add_interactive_widget_with_margin (pDesklet, pInteractiveWidget, 0)
/** Set the right margin of a desklet. This is useful to keep a clickable zone on the desklet when you put a GTK widget inside.
*@param pDesklet the desklet.
*@param iRightMargin right margin, in pixels.
*/
void gldi_desklet_set_margin (CairoDesklet *pDesklet, int iRightMargin);
/** Detach the interactive widget from a desklet. The widget can then be placed anywhere after that. You have to unref it after you placed it into a container, or to destroy it.
*@param pDesklet the desklet with an interactive widget.
*@return the widget.
*/
GtkWidget *gldi_desklet_steal_interactive_widget (CairoDesklet *pDesklet);


/** Hide a desklet.
*@param pDesklet the desklet.
*/
void gldi_desklet_hide (CairoDesklet *pDesklet);
/** Show a desklet, and give it the focus.
*@param pDesklet the desklet.
*/
void gldi_desklet_show (CairoDesklet *pDesklet);

/** Set a desklet's accessibility. For Widget Layer, the WM must support it and the correct rule must be set up in the WM (for instance for Compiz : class=Cairo-dock & type=utility). The function automatically sets up the rule for Compiz (if Dbus is activated).
*@param pDesklet the desklet.
*@param iVisibility the new accessibility.
*@param bSaveState whether to save the new state in the conf file.
*/
void gldi_desklet_set_accessibility (CairoDesklet *pDesklet, CairoDeskletVisibility iVisibility, gboolean bSaveState);

/** Set a desklet sticky (i.e. visible on all desktops), or not. In case the desklet is set unsticky, its current desktop/viewport is saved.
*@param pDesklet the desklet.
*@param bSticky whether the desklet should be sticky or not.
*/
void gldi_desklet_set_sticky (CairoDesklet *pDesklet, gboolean bSticky);

gboolean gldi_desklet_is_sticky (CairoDesklet *pDesklet);

/** Lock the position of a desklet. This makes the desklet impossible to rotate, drag with the mouse, or retach to the dock. The new state is saved in conf.
*@param pDesklet the desklet.
*@param bPositionLocked whether the position should be locked or not.
*/
void gldi_desklet_lock_position (CairoDesklet *pDesklet, gboolean bPositionLocked);


void gldi_desklet_insert_icon (Icon *icon, CairoDesklet *pDesklet);

gboolean gldi_desklet_detach_icon (Icon *icon, CairoDesklet *pDesklet);

G_END_DECLS

#endif
