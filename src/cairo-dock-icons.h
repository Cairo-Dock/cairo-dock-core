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


#ifndef __CAIRO_DOCK_ICONS__
#define  __CAIRO_DOCK_ICONS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-modules.h"
G_BEGIN_DECLS


/**
*@file cairo-dock-icons.h This class defines the items contained in containers : Icons.
* An Icon is an union of 3 parts : a launcher, an appli, and an applet. It can be all of them at the same time.
* - a launcher is any icon having a command or pointing to a sub-dock.
* - an appli is an icon representing an X window.
* - an applet is an icon holding a module instance.
* - an icon being none of them is a separator.
*
* The type of an icon is dynamic (for instance a launcher can become an application).
*
* Icons are sorted by the (group, order) pair; the group is also the initial type of the icon.
*/

/// Definition if the groups of icons.
typedef enum {
	CAIRO_DOCK_LAUNCHER = 0,
	CAIRO_DOCK_SEPARATOR12,
	CAIRO_DOCK_APPLI,
	CAIRO_DOCK_SEPARATOR23,
	CAIRO_DOCK_APPLET,
	CAIRO_DOCK_NB_TYPES
	} CairoDockIconType;

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

/// Definition of an Icon.
struct _Icon {
	//\____________ Definition.
	/// group of the icon.
	CairoDockIconType iType;
	GPtrArray *pNotificationsTab;
	gpointer pDataSlot[CAIRO_DOCK_NB_DATA_SLOT];
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
	gboolean bStatic;
	
	CairoDataRenderer *pDataRenderer;
	CairoDockTransition *pTransition;
	
	CairoDockAnimationState iAnimationState;
	/// Whether the icon is currently pointed or not.
	gboolean bPointed;
	gdouble fInsertRemoveFactor;
	
	//\____________ Launcher.
	gchar *cDesktopFileName;  // nom (et non pas chemin) du fichier .desktop
	gchar *cCommand;
	gchar *cWorkingDirectory;
	gchar *cBaseURI;
	gint iVolumeID;
	
	//\____________ Appli.
	Window Xid;
	gboolean bIsHidden;
	gboolean bIsFullScreen;
	gboolean bIsMaximized;
	gboolean bIsDemandingAttention;
	gboolean bHasIndicator;
	GtkAllocation windowGeometry;
	gint iNumDesktop;
	gint iViewPortX, iViewPortY;
	gint iStackOrder;
	gint iLastCheckTime;
	gchar *cInitialName;
	gchar *cLastAttentionDemand;
	Pixmap iBackingPixmap;
	//Damage iDamageHandle;
	
	//\____________ Applet.
	CairoDockModuleInstance *pModuleInstance;
	
	//\____________ Buffers.
	gdouble fWidth, fHeight;
	cairo_surface_t* pIconBuffer;
	GLuint iIconTexture;
	cairo_surface_t* pReflectionBuffer;
	
	gint iTextWidth, iTextHeight;
	cairo_surface_t* pTextBuffer;
	GLuint iLabelTexture;
	
	gint iQuickInfoWidth, iQuickInfoHeight;
	cairo_surface_t* pQuickInfoBuffer;
	GLuint iQuickInfoTexture;
	
	//\____________ Parametres de dessin, definis par la vue/les animations.
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
	
	gboolean bBeingRemovedByCairo;  // devrait etre dans pDataSlot...
	
	guint iSidRedrawSubdockContent;
};

/// Definition of a function that runs through all icons.
typedef void (* CairoDockForeachIconFunc) (Icon *icon, CairoContainer *pContainer, gpointer data);


/** TRUE if the icon is a launcher (a real launcher with a command or an URI, or a container icon, or even a fake launcher).
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_LAUNCHER(icon) (icon != NULL && ((icon)->cCommand != NULL || (icon)->cBaseURI != NULL || ((icon)->pSubDock != NULL && (icon)->pModuleInstance == NULL)))

/** TRUE if the icon is an icon of appli.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_APPLI(icon) (icon != NULL && (icon)->Xid != 0)

/** TRUE if the icon is an icon of applet.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_APPLET(icon) (icon != NULL && (icon)->pModuleInstance != NULL)

/** TRUE if the icon is an icon of separator.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_SEPARATOR(icon) (icon != NULL && (icon)->pModuleInstance == NULL && (icon)->Xid == 0 && (icon)->pSubDock == NULL && (icon)->cCommand == NULL && (icon)->cBaseURI == NULL)

/** TRUE if the icon is an icon of launcher launching something.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_NORMAL_LAUNCHER(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && (icon)->cCommand != NULL)

/** TRUE if the icon is an icon of launcher defined by a .desktop file.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_STORED_LAUNCHER(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && (icon)->cDesktopFileName != NULL)

/** TRUE if the icon is an icon representing an URI.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_URI_LAUNCHER(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && (icon)->cBaseURI != NULL)

/** TRUE if the icon is a fake icon (for class groupment).
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_FAKE_LAUNCHER(icon) (icon != NULL && (icon)->cCommand == NULL && (icon)->cClass != NULL && (icon)->pModuleInstance == NULL && (icon)->pSubDock != NULL && (icon)->cDesktopFileName == NULL)

/** TRUE if the icon is a container icon.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_CONTAINER_LAUNCHER(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && (icon)->cCommand == NULL && (icon)->cBaseURI == NULL && icon->cClass == NULL)

/** TRUE if the icon is an icon pointing on the sub-dock of a class.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_MULTI_APPLI(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && icon->pSubDock != NULL && icon->pSubDock->icons != NULL && icon->cClass != NULL)

/** TRUE if the icon is an icon of automatic separator.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR(icon) (CAIRO_DOCK_IS_SEPARATOR (icon) && (icon)->cDesktopFileName == NULL)

/** TRUE if the icon is an icon of separator added by the user.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_USER_SEPARATOR(icon) (CAIRO_DOCK_IS_SEPARATOR (icon) && (icon)->cDesktopFileName != NULL)
/**
*TRUE if the icon is an icon d'appli seulement.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_NORMAL_APPLI(icon) (CAIRO_DOCK_IS_APPLI (icon) && (icon)->cDesktopFileName == NULL && (icon)->pModuleInstance == NULL)
/**
*TRUE if the icon is an icon d'applet detachable en desklet.
*@param icon an icon.
*/
#define CAIRO_DOCK_IS_DETACHABLE_APPLET(icon) (CAIRO_DOCK_IS_APPLET (icon) && (icon)->pModuleInstance->bCanDetach)


/** Destroy an icon, freeing all allocated ressources. Ths sub-dock is not unreferenced, this must be done beforehand. It also deactivate the icon before (dialog, class, Xid, module) if necessary.
*@param icon the icon to free.
*/
void cairo_dock_free_icon (Icon *icon);


/** Say whether an icon is currently being inserted.
*/
#define cairo_dock_icon_is_being_inserted(icon) ((icon)->fInsertRemoveFactor < 0)
/** Say whether an icon is currently being removed.
*/
#define cairo_dock_icon_is_being_removed(icon) ((icon)->fInsertRemoveFactor > 0)
#define cairo_dock_icon_is_being_inserted_or_removed(icon) ((icon)->fInsertRemoveFactor != 0)


#define cairo_dock_get_group_order(iType) (iType < CAIRO_DOCK_NB_TYPES ? myIcons.tIconTypeOrder[iType] : iType)
/** Get the group order of an icon. 3 groups are available by default : launchers, applis, and applets, and each group has an order.
*/
#define cairo_dock_get_icon_order(icon) cairo_dock_get_group_order ((icon)->iType)

/** Get the type of an icon according to its content (launcher, appli, applet). This can be different from its group.
*@param icon the icon.
*/
CairoDockIconType cairo_dock_get_icon_type (Icon *icon);
/** Compare 2 icons with the order relation on (group order, icon order).
*@param icon1 an icon.
*@param icon2 another icon.
*@return -1 if icon1 < icon2, 1 if icon1 > icon2, 0 if icon1 = icon2.
*/
int cairo_dock_compare_icons_order (Icon *icon1, Icon *icon2);

/** Compare 2 icons with the order relation on the name (case unsensitive alphabetical order).
*@param icon1 an icon.
*@param icon2 another icon.
*@return -1 if icon1 < icon2, 1 if icon1 > icon2, 0 if icon1 = icon2.
*/
int cairo_dock_compare_icons_name (Icon *icon1, Icon *icon2);

/**
*Compare 2 icons with the order relation on the extension of their URIs (case unsensitive alphabetical order).
*@param icon1 an icon.
*@param icon2 another icon.
*@return -1 if icon1 < icon2, 1 if icon1 > icon2, 0 if icon1 = icon2.
*/
int cairo_dock_compare_icons_extension (Icon *icon1, Icon *icon2);

/** Sort a list with the order relation on (group order, icon order).
*@param pIconList a list of icons.
*@return the sorted list. Elements are the same as the initial list, only their order has changed.
*/
GList *cairo_dock_sort_icons_by_order (GList *pIconList);

/** Sort a list with the alphabetical order on the icons' name.
*@param pIconList a list of icons.
*@return the sorted list. Elements are the same as the initial list, only their order has changed. Icon's orders are updated to reflect the new order.
*/
GList *cairo_dock_sort_icons_by_name (GList *pIconList);

/** Get the first icon of a list of icons.
*@param pIconList a list of icons.
*@return the first icon, or NULL if the list is empty.
*/
Icon *cairo_dock_get_first_icon (GList *pIconList);

/** Get the last icon of a list of icons.
*@param pIconList a list of icons.
*@return the last icon, or NULL if the list is empty.
*/
Icon *cairo_dock_get_last_icon (GList *pIconList);

/*
*Renvoie la 1ere icon a etre dessinee dans un dock (qui n'est pas forcement la 1ere icon de la liste, si l'utilisateur a scrolle).
*@param pDock le dock.
*@return la 1ere icon a etre dessinee, ou NULL si la liste is vide.
*/
Icon *cairo_dock_get_first_drawn_icon (CairoDock *pDock);

/*
*Renvoie la derniere icon a etre dessinee dans un dock (qui n'est pas forcement la derniere icon de la liste, si l'utilisateur a scrolle).
*@param pDock le dock.
*@return la derniere icon a etre dessinee, ou NULL si la liste is vide.
*/
Icon *cairo_dock_get_last_drawn_icon (CairoDock *pDock);

/** Get the first icon of a given group.
*@param pIconList a list of icons.
*@param iType the group of icon.
*@return the first found icon with this group, or NULL if none matches.
*/
Icon *cairo_dock_get_first_icon_of_group (GList *pIconList, CairoDockIconType iType);
#define cairo_dock_get_first_icon_of_type cairo_dock_get_first_icon_of_group

/** Get the last icon of a given group.
*@param pIconList a list of icons.
*@param iType the group of icon.
*@return the last found icon with this group, or NULL if none matches.
*/
Icon *cairo_dock_get_last_icon_of_group (GList *pIconList, CairoDockIconType iType);
#define cairo_dock_get_last_icon_of_type cairo_dock_get_last_icon_of_group

/** Get the first icon whose group has the same order as a given one.
*@param pIconList a list of icons.
*@param iType a type of icon.
*@return the first found icon, or NULL if none matches.
*/
Icon* cairo_dock_get_first_icon_of_order (GList *pIconList, CairoDockIconType iType);

/** Get the last icon whose group has the same order as a given one.
*@param pIconList a list of icons.
*@param iType a type of icon.
*@return the last found icon, or NULL if none matches.
*/
Icon* cairo_dock_get_last_icon_of_order (GList *pIconList, CairoDockIconType iType);

Icon* cairo_dock_get_last_icon_until_order (GList *pIconList, CairoDockIconType iType);

/** Get the currently pointed icon in a list of icons.
*@param pIconList a list of icons.
*@return the icon whose field 'bPointed' is TRUE, or NULL if none is pointed.
*/
Icon *cairo_dock_get_pointed_icon (GList *pIconList);

/** Get the icon next to a given one. The cost is O(n).
*@param pIconList a list of icons.
*@param pIcon an icon in the list.
*@return the icon whose left neighboor is pIcon, or NULL if the list is empty or if pIcon is the last icon.
*/
Icon *cairo_dock_get_next_icon (GList *pIconList, Icon *pIcon);

/** Get the icon previous to a given one. The cost is O(n).
*@param pIconList a list of icons.
*@param pIcon an icon in the list.
*@return the icon whose right neighboor is pIcon, or NULL if the list is empty or if pIcon is the first icon.
*/
Icon *cairo_dock_get_previous_icon (GList *pIconList, Icon *pIcon);

/** Get the next element in a list, looping if necessary..
*@param ic the current element.
*@param list a list.
*@return the next element, or the first element of the list if 'ic' is the last one.
*/
#define cairo_dock_get_next_element(ic, list) (ic == NULL || ic->next == NULL ? list : ic->next)

/** Get the previous element in a list, looping if necessary..
*@param ic the current element.
*@param list a list.
*@return the previous element, or the last element of the list if 'ic' is the first one.
*/
#define cairo_dock_get_previous_element(ic, list) (ic == NULL || ic->prev == NULL ? g_list_last (list) : ic->prev)

/** Search an icon with a given command in a list of icons.
*@param pIconList a list of icons.
*@param cCommand the command.
*@return the first icon whose field 'cCommand' is identical to the given command, or NULL if no icon matches.
*/
Icon *cairo_dock_get_icon_with_command (GList *pIconList, const gchar *cCommand);

/** Search an icon with a given URI in a list of icons.
*@param pIconList a list of icons.
*@param cBaseURI the URI.
*@return the first icon whose field 'cURI' is identical to the given URI, or NULL if no icon matches.
*/
Icon *cairo_dock_get_icon_with_base_uri (GList *pIconList, const gchar *cBaseURI);

/** Search an icon with a given name in a list of icons.
*@param pIconList a list of icons.
*@param cName the name.
*@return the first icon whose field 'cName' is identical to the given name, or NULL if no icon matches.
*/
Icon *cairo_dock_get_icon_with_name (GList *pIconList, const gchar *cName);

/** Search the icon pointing on a given sub-dock in a list of icons.
*@param pIconList a list of icons.
*@param pSubDock a sub-dock.
*@return the first icon whose field 'pSubDock' is equal to the given sub-dock, or NULL if no icon matches.
*/
Icon *cairo_dock_get_icon_with_subdock (GList *pIconList, CairoDock *pSubDock);

/** Search the icon of a given module in a list of icons.
*@param pIconList a list of icons.
*@param pModule the module.
*@return the first icon which has an instance of the given module, or NULL if no icon matches.
*/
Icon *cairo_dock_get_icon_with_module (GList *pIconList, CairoDockModule *pModule);

#define cairo_dock_get_first_launcher(pIconList) cairo_dock_get_first_icon_of_group (pIconList, CAIRO_DOCK_LAUNCHER)
#define cairo_dock_get_last_launcher(pIconList) cairo_dock_get_last_icon_of_group (pIconList, CAIRO_DOCK_LAUNCHER)
#define cairo_dock_get_first_appli(pIconList) cairo_dock_get_first_icon_of_group (pIconList, CAIRO_DOCK_APPLI)
#define cairo_dock_get_last_appli(pIconList) cairo_dock_get_last_icon_of_group (pIconList, CAIRO_DOCK_APPLI)

/** Get the dimension allocated to the surface/texture of an icon.
@param pIcon the icon.
@param pContainer its container.
@param iWidth pointer to the width.
@param iHeight pointer to the height.
*/
void cairo_dock_get_icon_extent (Icon *pIcon, CairoContainer *pContainer, int *iWidth, int *iHeight);

/** Get the current size of an icon as it is seen on the screen (taking into account the zoom and the ratio).
@param pIcon the icon
@param pContainer its container
@param fSizeX pointer to the X size (horizontal)
@param fSizeY pointer to the Y size (vertical)
*/
void cairo_dock_get_current_icon_size (Icon *pIcon, CairoContainer *pContainer, double *fSizeX, double *fSizeY);

/** Get the total zone used by an icon on its container (taking into account reflect, gap to reflect, zoom and stretching).
@param icon the icon
@param pContainer its container
@param pArea a rectangle filled with the zone used by the icon on its container.
*/
void cairo_dock_compute_icon_area (Icon *icon, CairoContainer *pContainer, GdkRectangle *pArea);



void cairo_dock_normalize_icons_order (GList *pIconList, CairoDockIconType iType);

void cairo_dock_move_icon_after_icon (CairoDock *pDock, Icon *icon1, Icon *icon2);

/** Run an action on all the icons of a given group. The action can even destroy or remove the icon from the list.
*@param pIconList a list of icons.
*@param iType the group.
*@param pFuntion the callback.
*@param data data passed as a parameter of the callback.
*@return the first automatic separator with another group, or NULL if there is none.
*/
Icon *cairo_dock_foreach_icons_of_type (GList *pIconList, CairoDockIconType iType, CairoDockForeachIconFunc pFuntion, gpointer data);

/** Update the container's name of an icon with the name of a dock. In the case of a launcher or an applet, the conf file is updated too.
*@param icon the icon.
*@param cNewParentDockName the name of its new dock.
*/
void cairo_dock_update_icon_s_container_name (Icon *icon, const gchar *cNewParentDockName);

/** Make an icon static. Static icon are not animated when mouse hovers them.
*@param icon the icon.
*/
#define cairo_dock_set_icon_static(icon) ((icon)->bStatic = TRUE)

/** Set the label of an icon. If it has a sub-dock, it is renamed (the name is possibly altered to stay unique). The label buffer is updated too.
*@param pSourceContext a drawing context; is not altered by the function.
*@param cIconName the new label of the icon. You can even pass pIcon->cName.
*@param pIcon the icon.
*@param pContainer the container of the icon.
*/
void cairo_dock_set_icon_name (cairo_t *pSourceContext, const gchar *cIconName, Icon *pIcon, CairoContainer *pContainer);

/** Same as above, but takes a printf-like format string.
*@param pSourceContext a drawing context; is not altered by the function.
*@param pIcon the icon.
*@param pContainer the container of the icon.
*@param cIconNameFormat the new label of the icon, in a 'printf' way.
*@param ... data to be inserted into the string.
*/
void cairo_dock_set_icon_name_full (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconNameFormat, ...);

/** Set the quick-info of an icon. This is a small text (a few characters) that is superimposed on the icon.
*@param pSourceContext a drawing context; is not altered by the function.
*@param pIcon the icon.
*@param pContainer the container of the icon.
*@param cQuickInfo the text of the quick-info.
*/
void cairo_dock_set_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, const gchar *cQuickInfo);

/** Same as above, but takes a printf-like format string.
*@param pSourceContext a drawing context; is not altered by the function.
*@param pIcon the icon.
*@param pContainer the container of the icon.
*@param cQuickInfoFormat the text of the quick-info, in a 'printf' way.
*@param ... data to be inserted into the string.
*/
void cairo_dock_set_quick_info_full (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, const gchar *cQuickInfoFormat, ...);

/** Clear the quick-info of an icon.
*@param pIcon the icon.
*/
#define cairo_dock_remove_quick_info(pIcon) cairo_dock_set_quick_info (NULL, pIcon, NULL, NULL)

/** Updates which launcher is attached/detached to/from the dock, according to the current desktop.
*@param pDock the dock whose icons are to be considered.
*/
void cairo_dock_hide_show_launchers_on_other_desktops (CairoDock *pDock);

G_END_DECLS
#endif

