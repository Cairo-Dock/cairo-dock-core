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

#ifndef __CAIRO_DOCK_ICON_FACTORY__
#define  __CAIRO_DOCK_ICON_FACTORY__

#include <glib.h>
#include <gio/gdesktopappinfo.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-object.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-icon-factory.h This class defines the items contained in containers : Icons.
* An icon can either be:
* - a launcher (it has a command, a class, and possible an X window ID)
* - an appli (it has a X window ID and a class, no command)
* - an applet (it has a module instance and no command, possibly a class)
* - a container (it has a sub-dock and no class nor command)
* - a class icon (it has a bsub-dock and a class, but no command nor X ID)
* - a separator (it has nothing)
* 
* The class defines the methods used to create a generic Icon and to load its various buffers.
* Specialized Icons are created by the corresponding factory.
*/

/// Available groups of icons.
typedef enum {
	CAIRO_DOCK_LAUNCHER = 0,  // launchers and applets, and applis if mixed
	CAIRO_DOCK_SEPARATOR12,
	CAIRO_DOCK_APPLI,
	CAIRO_DOCK_NB_GROUPS
	} CairoDockIconGroup;

/// Animation state of an icon, sorted by priority.
typedef enum {
	CAIRO_DOCK_STATE_REST = 0,
	CAIRO_DOCK_STATE_MOUSE_HOVERED,
	CAIRO_DOCK_STATE_CLICKED,
	CAIRO_DOCK_STATE_AVOID_MOUSE,
	CAIRO_DOCK_STATE_FOLLOW_MOUSE,
	CAIRO_DOCK_STATE_REMOVE_INSERT,
	CAIRO_DOCK_NB_STATES
	} CairoDockAnimationState;

/// Icon's interface
struct _IconInterface {
	/// function that loads the icon surface (and optionnally texture).
	void (*load_image) (Icon *icon);
	/// function called when the user drag something over the icon for more than 500ms.
	void (*action_on_drag_hover) (Icon *icon);
	};

/// Definition of an Icon.
struct _Icon {
	//\____________ Definition.
	/// object
	GldiObject object;
	gpointer pDataSlot[CAIRO_DOCK_NB_DATA_SLOT];
	/// group of the icon.
	CairoDockIconGroup iGroup;
	/// interface
	IconInterface iface;
	GldiContainer *pContainer;  // container where the icon is currently.
	
	//\____________ properties.
	// generic.
	/// Name of the icon.
	gchar *cName;
	/// Short info displayed on the icon (few characters).
	gchar *cQuickInfo;
	/// name or path of an image displayed on the icon.
	gchar *cFileName;
	/// Class of application the icon will be bound to.
	gchar *cClass;
	/// name of the dock the icon belongs to (NULL means it's not currently inside a dock).
	gchar *cParentDockName;
	/// Sub-dock the icon is pointing to.
	CairoDock *pSubDock;
	/// Order of the icon amongst the other icons of its group.
	gdouble fOrder;
	
	gint iSpecificDesktop;
	gint iSubdockViewType;
	/// a hint to indicate the icon should be kept static (no animation like bouncing).
	gboolean bStatic;
	/// a flag that allows the icon to be always visible, even when the dock is hidden.
	gboolean bAlwaysVisible;
	gboolean bIsDemandingAttention;
	gboolean bHasHiddenBg;
	GldiColor *pHiddenBgColor;  // NULL to use the default color
	gboolean bIgnoreQuicklist;  // TRUE to not display the Ubuntu's quicklist of the class
	gboolean bHasIndicator;
	
	// Launcher.
	gchar *cDesktopFileName;  // nom (et non pas chemin) du fichier .desktop
	gchar *cCommand; // used by plugins to store a custom command or other data; NOT used by core
	gchar *cBaseURI; // used by shortcuts
	gint iVolumeID;
	gchar *cInitialName; // original name replaced by e.g. the actual app title matched to a launcher
	// GAppInfo(s) that are used to launch the app corresponding to this icon (if it is a launcher or appli)
	// These are only set on creation and do not change during the lifetime of the icon.
	GldiAppInfo *pAppInfo; // app info from a .desktop file installed on the system
	GDesktopAppInfo *pCustomLauncher; // GAppInfo with custom launch command (if set)
	
	// Appli.
	GldiWindowActor *pAppli;
	
	// Applet.
	GldiModuleInstance *pModuleInstance;
	GldiModuleInstance *pAppletOwner;
	
	//\____________ Buffers.
	gdouble fWidth, fHeight;  // size at rest in the container (including ratio and orientation).
	gint iRequestedWidth, iRequestedHeight;  // buffer image size that can be requested (surface/texture size)
	gint iRequestedDisplayWidth, iRequestedDisplayHeight;  // icon size that can be requested (as displayed in the dock)
	gint iAllocatedWidth, iAllocatedHeight;  // buffer image size actually allocated (surface/texture size)
	CairoDockImageBuffer image; // the image of the icon
	CairoDockImageBuffer label; // the label above the icon
	GList *pOverlays;  // a list of CairoOverlay
	CairoDataRenderer *pDataRenderer;
	CairoDockTransition *pTransition;
	
	//\____________ Drawing parameters.
	gdouble fXMin, fXMax;  // Abscisse extremale gauche/droite que the icon atteindra (variable avec la vague).
	gdouble fXAtRest;  // Abscisse de the icon au repos.
	gdouble fPhase;  // Phase de the icon (entre -pi et piconi).
	gdouble fX, fY;  // Abscisse/Ordonnee temporaire du bord haut-gauche de l'image de the icon.
	
	gdouble fScale;
	gdouble fDrawX, fDrawY;
	gdouble fWidthFactor, fHeightFactor;
	gdouble fAlpha;
	gdouble fDeltaYReflection;  // Decalage en ordonnees du reflet (rebond).
	gdouble fOrientation;  // par rapport a la verticale Oz
	gint iRotationX;  // Rotation autour de l'axe Ox
	gint iRotationY;  // Rotation autour de l'axe Oy
	gdouble fReflectShading;
	
	gdouble fGlideOffset;  // decalage pour le glissement des icons.
	gint iGlideDirection;  // direction dans laquelle glisse the icon.
	gdouble fGlideScale;  // echelle d'adaptation au glissement.
	
	CairoDockAnimationState iAnimationState;
	/// Whether the icon is currently pointed or not.
	gboolean bPointed;
	gdouble fInsertRemoveFactor;
	gboolean bDamaged;  // TRUE when the icon couldn't draw its surface, because the Gl context was not yet ready.
	gboolean bNeedApplyBackground;
	
	//\____________ Other dynamic parameters.
	guint iSidRedrawSubdockContent;
	guint iSidLoadImage;
	guint iSidDoubleClickDelay;
	gint iNbDoubleClickListeners;
	gint iHideLabel;
	gint iThumbnailX, iThumbnailY;  // X icon geometry for apps
	gint iThumbnailWidth, iThumbnailHeight;
	
	gboolean bIsLaunching;  // a mere recopy of gldi_class_is_starting()
	gpointer reserved[4];
};

typedef void (*CairoIconContainerLoadFunc) (void);
typedef void (*CairoIconContainerUnloadFunc) (void);
typedef void (*CairoIconContainerRenderFunc) (Icon *pIcon, GldiContainer *pContainer, int w, int h, cairo_t *pCairoContext);
typedef void (*CairoIconContainerRenderOpenGLFunc) (Icon *pIcon, GldiContainer *pContainer, int w, int h);

/// Definition of an Icon container (= an icon holding a sub-dock) renderer.
struct _CairoIconContainerRenderer {
	CairoIconContainerLoadFunc load;
	CairoIconContainerUnloadFunc unload;
	CairoIconContainerRenderFunc render;
	CairoIconContainerRenderOpenGLFunc render_opengl;
};

/** Say if an object is an Icon.
*@param obj the object.
*@return TRUE if the object is an icon.
*/
#define CAIRO_DOCK_IS_ICON(obj) gldi_object_is_manager_child (obj, &myIconObjectMgr)

#define CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER GLDI_OBJECT_IS_LAUNCHER_ICON
#define CAIRO_DOCK_ICON_TYPE_IS_CONTAINER GLDI_OBJECT_IS_STACK_ICON
#define CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR GLDI_OBJECT_IS_SEPARATOR_ICON
#define CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER GLDI_OBJECT_IS_CLASS_ICON
#define CAIRO_DOCK_ICON_TYPE_IS_APPLI GLDI_OBJECT_IS_APPLI_ICON
#define CAIRO_DOCK_ICON_TYPE_IS_APPLET GLDI_OBJECT_IS_APPLET_ICON

/** TRUE if the icon holds a window.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_APPLI(icon) (icon != NULL && (icon)->pAppli != NULL)

/** TRUE if the icon holds an instance of a module.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_APPLET(icon) (icon != NULL && (icon)->pModuleInstance != NULL)

/** TRUE if the icon is an icon pointing on the sub-dock of a class.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_MULTI_APPLI(icon) (\
	(  CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)\
	|| CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon)\
	|| (CAIRO_DOCK_ICON_TYPE_IS_APPLET (icon) && icon->cClass != NULL) )\
	&& icon->pSubDock != NULL)

/** TRUE if the icon is an automatic separator.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR(icon) (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) && (icon)->cDesktopFileName == NULL)

/** TRUE if the icon is a separator added by the user.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_USER_SEPARATOR(icon) (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) && (icon)->cDesktopFileName != NULL)
/**
*TRUE if the icon is an icon d'appli only.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_NORMAL_APPLI(icon) (CAIRO_DOCK_IS_APPLI (icon) && CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon))
/**
*TRUE if the icon is an icon d'applet detachable en desklet.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_DETACHABLE_APPLET(icon) (CAIRO_DOCK_IS_APPLET (icon) && ((icon)->pModuleInstance->pModule->pVisitCard->iContainerType & CAIRO_DOCK_MODULE_CAN_DESKLET))


/** Create an empty icon.
*@return the newly allocated icon object.
*/
Icon *gldi_icon_new (void);


/** Create an Icon that will behave like a launcher. It's especially useful for applets that want to fill a sub-dock or a desklet (the icon is not loaded by the function). Be careful that the strings are not duplicated. Therefore, you must use g_strdup() if you want to set a constant string; and must not free the strings after calling this function.
* @param cName label of the icon
* @param cFileName name of an image
* @param cCommand a command, or NULL
* @param cQuickInfo a quick-info, or NULL
* @param fOrder order of the icon in its container.
* @return the newly created icon.
*/
Icon * cairo_dock_create_dummy_launcher (gchar *cName, gchar *cFileName, gchar *cCommand, gchar *cQuickInfo, double fOrder);


/* Cree la surface de reflection d'une icone (pour cairo).
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*/
void cairo_dock_add_reflection_to_icon (Icon *pIcon, GldiContainer *pContainer);


gboolean cairo_dock_apply_icon_background_opengl (Icon *icon);

/**Fill the image buffer (surface & texture) of a given icon, according to its type. Set its size if necessary, and fills the reflection buffer for cairo.
*@param icon the icon.
*@param pContainer its container.
*/
void cairo_dock_load_icon_image (Icon *icon, GldiContainer *pContainer);
#define cairo_dock_reload_icon_image cairo_dock_load_icon_image

/**Fill the label buffer (surface & texture) of a given icon, according to a text description.
*@param icon the icon.
*/
void cairo_dock_load_icon_text (Icon *icon);

/**Fill the quick-info buffer (surface & texture) of a given icon, according to a text description.
*@param icon the icon.
*/
void cairo_dock_load_icon_quickinfo (Icon *icon);

/** Fill all the buffers (surfaces & textures) of a given icon, according to its type. Set its size accordingly, and fills the reflection buffer for cairo. Label and quick-info are loaded with the current global text description.
*@param pIcon the icon.
*@param pContainer its container.
*/
void cairo_dock_load_icon_buffers (Icon *pIcon, GldiContainer *pContainer);

void cairo_dock_trigger_load_icon_buffers (Icon *pIcon);


void cairo_dock_draw_subdock_content_on_icon (Icon *pIcon, CairoDock *pDock);

#define cairo_dock_set_subdock_content_renderer(pIcon, view) (pIcon)->iSubdockViewType = view


void gldi_icon_detach (Icon *pIcon);

void gldi_icon_insert_in_container (Icon *pIcon, GldiContainer *pContainer, gboolean bAnimateIcon);


G_END_DECLS
#endif
