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

#ifndef __CAIRO_DOCK_DOCK_PRIV__
#define  __CAIRO_DOCK_DOCK_PRIV__

#include "cairo-dock-manager.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-container.h"

#include <glib.h>

G_BEGIN_DECLS


// Additional definitions (only used in core)

#define CAIRO_DOCK_INSERT_SEPARATOR TRUE

typedef struct _CairoDockAttr CairoDockAttr;
struct _CairoDockAttr {
	// parent attributes
	GldiContainerAttr cattr;
	const gchar *cDockName;
	const gchar *cRendererName;
	GList *pIconList;
	gboolean bSubDock;
	CairoDock *pParentDock;
};

typedef enum {
	ICON_DEFAULT,  // same as main dock
	ICON_TINY,
	ICON_VERY_SMALL,
	ICON_SMALL,
	ICON_MEDIUM,
	ICON_BIG,
	ICON_HUGE
	} GldiIconSizeEnum;

/// TODO: harmonize the values with the simple config -> make some public functions...
typedef enum {
	ICON_SIZE_TINY = 28,
	ICON_SIZE_VERY_SMALL = 36,
	ICON_SIZE_SMALL = 42,
	ICON_SIZE_MEDIUM = 48,
	ICON_SIZE_BIG = 56,
	ICON_SIZE_HUGE = 64
	} GldiIconSize;


/* Functions defined in cairo-dock-dock-factory.c */

void gldi_dock_init_internals (CairoDock *pDock);

/** Create a new dock of type "sub-dock", and load a given list of icons inside. The list then belongs to the dock, so it must not be freeed after that. The buffers of each icon are loaded, so they just need to have an image filename and a name.
* @param cDockName the name that identifies the dock.
* @param cRendererName name of a renderer. If NULL, the default renderer will be applied.
* @param pParentDock the parent dock.
* @param pIconList a list of icons that will be loaded and inserted into the new dock (optional).
* @return the new dock.
*/
CairoDock *gldi_subdock_new (const gchar *cDockName, const gchar *cRendererName, CairoDock *pParentDock, GList *pIconList);

/* Deplace et redimensionne un dock a ses position et taille attitrees. Ne change pas la zone d'input (cela doit etre fait par ailleurs), et ne la replace pas (cela est fait lors du configure).
*/
void cairo_dock_move_resize_dock (CairoDock *pDock);

/** Remove all icons from a dock (and its sub-docks). If the receiving dock is NULL, the icons are destroyed and removed from the current theme itself.
*@param pDock a dock.
*@param pReceivingDock the dock that will receive the icons, or NULL to destroy and remove the icons.
*/
void cairo_dock_remove_icons_from_dock (CairoDock *pDock, CairoDock *pReceivingDock);

void cairo_dock_set_icon_size_in_dock (CairoDock *pDock, Icon *icon);

void cairo_dock_create_redirect_texture_for_dock (CairoDock *pDock);

/** Attach an applet to a dock. This will prevent the dock from being
*  deleted even if the applet detaches its icon. Should be used for
*  all applets that have this dock set as their parent container.
*@param pDock a dock.
*@param pInstance an instance of a loaded applet
*/
void gldi_dock_attach_applet (CairoDock *pDock, GldiModuleInstance *pInstance);

/** Detach an applet from a dock. The dock will be removed if no applets
*  or icons are left in it.
*@param pDock a dock.
*@param pInstance an instance of a loaded applet to detach
*/
void gldi_dock_detach_applet (CairoDock *pDock, GldiModuleInstance *pInstance);


/* Functions defined in cairo-dock-dock-manager.c */

void cairo_dock_force_docks_above (void);

void cairo_dock_reset_docks_table (void);

void gldi_dock_make_subdock (CairoDock *pDock, CairoDock *pParentDock, const gchar *cRendererName);

/** Get a readable name for a main Dock, suitable for display (like "Bottom dock"). Sub-Docks names are defined by the user, so you can just use \ref gldi_dock_get_name for them.
* @param pDock the dock.
* @return the readable name of the dock, or NULL if not found. Free it when you're done.
*/
gchar *gldi_dock_get_readable_name (CairoDock *pDock);

// renvoie un nom de dock unique; cPrefix peut etre NULL.
gchar *cairo_dock_get_unique_dock_name (const gchar *cPrefix);
gboolean cairo_dock_check_unique_subdock_name (Icon *pIcon);

/** Rename a dock. Update the container's name of all of its icons.
*@param pDock the dock (optional).
*@param cNewName the new name.
*/
void gldi_dock_rename (CairoDock *pDock, const gchar *cNewName);

int cairo_dock_convert_icon_size_to_pixels (GldiIconSizeEnum s, double *fMaxScale, double *fReflectSize, int *iIconGap);

GldiIconSizeEnum cairo_dock_convert_icon_size_to_enum (int iIconSize);

/** Add a config file for a root dock. Does not create the dock (use \ref gldi_dock_new for that). If the config file already exists, it is overwritten (use \ref gldi_dock_get to check if the name is already used).
*@param cDockName name of the dock.
*/
void gldi_dock_add_conf_file_for_name (const gchar *cDockName);

gboolean cairo_dock_entrance_is_allowed (CairoDock *pDock);
void cairo_dock_activate_temporary_auto_hide (CairoDock *pDock);
void cairo_dock_deactivate_temporary_auto_hide (CairoDock *pDock);
#define cairo_dock_is_temporary_hidden(pDock) (pDock)->bTemporaryHidden
void gldi_subdock_synchronize_orientation (CairoDock *pSubDock, CairoDock *pDock, gboolean bUpdateDockSize);

/// Unhide the dock after the given delay, or instantly if iDelay == 0
void cairo_dock_unhide_dock_delayed (CairoDock *pDock, int iDelay);

/** Set the visibility of a root dock. Perform all the necessary actions.
*@param pDock a root dock.
*@param iVisibility its new visibility.
*/
void gldi_dock_set_visibility (CairoDock *pDock, CairoDockVisibility iVisibility);


void gldi_register_docks_manager (void);


/* Functions defined in cairo-dock-dock-facility.c */

/* Demande au WM d'empecher les autres fenetres d'empieter sur l'espace du dock.
* L'espace reserve est pris sur la taille minimale du dock, c'est-a-dire la taille de la zone de rappel si l'auto-hide est active,
* ou la taille du dock au repos sinon.
* @param pDock le dock.
* @param bReserve TRUE pour reserver l'espace, FALSE pour annuler la reservation.
*/
void cairo_dock_reserve_space_for_dock (CairoDock *pDock, gboolean bReserve);

/* Calcule la position d'un dock etant donne ses nouvelles dimensions.
*/
void cairo_dock_get_window_position_at_balance (CairoDock *pDock, int iNewWidth, int iNewHeight, int *iNewPositionX, int *iNewPositionY);

/* Met a jour les zones d'input d'un dock.
*/
void cairo_dock_update_input_shape (CairoDock *pDock);

#define cairo_dock_set_input_shape_active(pDock) do {\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), NULL);\
	if (pDock->fMagnitudeMax == 0.)\
		gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), pDock->pShapeBitmap);\
	else if (pDock->pActiveShapeBitmap != NULL)\
		gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), pDock->pActiveShapeBitmap);\
	} while (0)
#define cairo_dock_set_input_shape_at_rest(pDock) do {\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), NULL);\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), pDock->pShapeBitmap);\
	} while (0)
#define cairo_dock_set_input_shape_hidden(pDock) do {\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), NULL);\
	gldi_container_set_input_shape (CAIRO_CONTAINER (pDock), pDock->pHiddenShapeBitmap);\
	} while (0)


/** Get a list of available docks.
*@param pParentDock excluding this dock if not NULL
*@param pSubDock excluding this dock and its children if not NULL
*@return a list of CairoDock*
*/
GList *cairo_dock_get_available_docks (CairoDock *pParentDock, CairoDock *pSubDock);

/** Get a list of available docks where an user icon can be placed. Its current parent dock is excluded, as well as its sub-dock (if any) and its children.
*@param pIcon the icon
*@return a list of CairoDock*
*/
#define cairo_dock_get_available_docks_for_icon(pIcon) cairo_dock_get_available_docks (CAIRO_DOCK(cairo_dock_get_icon_container(pIcon)), pIcon->pSubDock)

void cairo_dock_stop_marking_icons (CairoDock *pDock);

void cairo_dock_trigger_redraw_subdock_content (CairoDock *pDock);
void cairo_dock_trigger_redraw_subdock_content_on_icon (Icon *icon);

void cairo_dock_redraw_subdock_content (CairoDock *pDock);

void cairo_dock_resize_icon_in_dock (Icon *pIcon, CairoDock *pDock);

void cairo_dock_load_dock_background (CairoDock *pDock);
void cairo_dock_trigger_load_dock_background (CairoDock *pDock);  // peu utile

void cairo_dock_make_preview (CairoDock *pDock, const gchar *cPreviewPath);

G_END_DECLS
#endif

