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

#ifndef __CAIRO_DOCK_APPLET_FACILITY__
#define  __CAIRO_DOCK_APPLET_FACILITY__

#include "cairo-dock-struct.h"
#include "cairo-dock-module-factory.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-applet-facility.h A collection of useful macros for applets.
* Macros provides a normalized API that will :
*  - lets you perform complex operations with a minimum amount of code
*  - ensures a bug-free functioning
*  - masks the internal complexity
*  - allows a normalized and easy-to-maintain code amongst all the applets.
*/

/** Apply a surface on a context, with a zoom and a transparency factor. The context is cleared beforehand with the default icon background.
*@param pIconContext the drawing context; is not altered by the function.
*@param pSurface the surface to apply.
*@param fScale zoom fastor.
*@param fAlpha transparency in [0,1].
*@param pIcon the icon.
*@param pContainer the container of the icon.
*/
void cairo_dock_set_icon_surface_full (cairo_t *pIconContext, cairo_surface_t *pSurface, double fScale, double fAlpha, Icon *pIcon, CairoContainer *pContainer);

/** Apply a surface on a context. The context is cleared beforehand with the default icon background..
*@param pIconContext the drawing context; is not altered by the function.
*@param pSurface the surface to apply.
*/
#define cairo_dock_set_icon_surface(pIconContext, pSurface) cairo_dock_set_icon_surface_full (pIconContext, pSurface, 1, 1, NULL, NULL)

/** Draw a bar at the bottom of an Icon, with a gradation from red to green and a given length.
*@param pIconContext the drawing context; is not altered by the function.
*@param fValue the value representing a percentage, in [-1,1]. if negative, the gradation is inverted, and the absolute value is used.
*@param pIcon the icon.
*/
void cairo_dock_draw_bar_on_icon (cairo_t *pIconContext, double fValue, Icon *pIcon);

void cairo_dock_set_icon_surface_with_bar (cairo_t *pIconContext, cairo_surface_t *pSurface, double fValue, Icon *pIcon);

/** Apply a surface on the context of an icon, clearing it beforehand, and adding the reflect.
*@param pIconContext the drawing context; is not altered by the function.
*@param pSurface the surface to apply.
*@param pIcon the icon.
*@param pContainer the container of the icon.
*/
void cairo_dock_set_icon_surface_with_reflect (cairo_t *pIconContext, cairo_surface_t *pSurface, Icon *pIcon, CairoContainer *pContainer);

/** Apply an image on the context of an icon, clearing it beforehand, and adding the reflect.
*@param pIconContext the drawing context; is not altered by the function.
*@param cImagePath name or path to an icon image.
*@param pIcon the icon.
*@param pContainer the container of the icon.
*@return TRUE if everything went smoothly.
*/
gboolean cairo_dock_set_image_on_icon (cairo_t *pIconContext, const gchar *cIconName, Icon *pIcon, CairoContainer *pContainer);

/** Apply an image on the context of an icon, clearing it beforehand, and adding the reflect. The image is searched in any possible locations, and the default image provided is used if the search was fruitless.
*@param pIconContext the drawing context; is not altered by the function.
*@param cImage name of an image to apply on the icon.
*@param pIcon the icon.
*@param pContainer the container of the icon.
*@param cDefaultImagePath path to a default image.
*/
void cairo_dock_set_image_on_icon_with_default (cairo_t *pIconContext, const gchar *cImage, Icon *pIcon, CairoContainer *pContainer, const gchar *cDefaultImagePath);


void cairo_dock_set_hours_minutes_as_quick_info (Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds);
void cairo_dock_set_minutes_secondes_as_quick_info (Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds);

/** Convert a size in bytes into a readable format.
*@param iSizeInBytes size in bytes.
*@return a newly allocated string.
*/
gchar *cairo_dock_get_human_readable_size (long long int iSizeInBytes);
void cairo_dock_set_size_as_quick_info (Icon *pIcon, CairoContainer *pContainer, long long int iSizeInBytes);

/// type of possible display on a Icon.
typedef enum {
	/// don't display anything.
	CAIRO_DOCK_INFO_NONE = 0,
	/// display info on the icon (as quick-info).
	CAIRO_DOCK_INFO_ON_ICON,
	/// display on the label of the icon.
	CAIRO_DOCK_INFO_ON_LABEL,
	CAIRO_DOCK_NB_INFO_DISPLAY
} CairoDockInfoDisplay;


gchar *cairo_dock_get_theme_path_for_module (const gchar *cAppletConfFilePath, GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultThemeName, const gchar *cShareThemesDir, const gchar *cExtraDirName);


/** Play a sound, through Alsa or PulseAudio.
*@param cSoundPath path to an audio file.
*/
void cairo_dock_play_sound (const gchar *cSoundPath);


// should be in gnome-integration if needed...
/* Get the Gnome's version.
*@param iMajor pointer to the major.
*@param iMinor pointer to the minor.
*@param iMicro pointer to the micro.
*/
//void cairo_dock_get_gnome_version (int *iMajor, int *iMinor, int *iMicro);


void cairo_dock_pop_up_about_applet (GtkMenuItem *menu_item, CairoDockModuleInstance *pModuleInstance);

void cairo_dock_open_module_config_on_demand (int iClickedButton, GtkWidget *pInteractiveWidget, CairoDockModuleInstance *pModuleInstance, CairoDialog *pDialog);

void cairo_dock_insert_icons_in_applet (CairoDockModuleInstance *pModuleInstance, GList *pIconsList, const gchar *cDockRenderer, const gchar *cDeskletRenderer, gpointer pDeskletRendererData);

void cairo_dock_insert_icon_in_applet (CairoDockModuleInstance *pInstance, Icon *pOneIcon);

gboolean cairo_dock_detach_icon_from_applet (CairoDockModuleInstance *pModuleInstance, Icon *icon);

gboolean cairo_dock_remove_icon_from_applet (CairoDockModuleInstance *pModuleInstance, Icon *icon);

void cairo_dock_remove_all_icons_from_applet (CairoDockModuleInstance *pModuleInstance);


void cairo_dock_resize_applet (CairoDockModuleInstance *pInstance, int w, int h);


  ////////////
 // CONFIG //
////////////

/** Get the value of a 'boolean' from the conf file.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param bDefaultValue default value if the group/key is not found (typically if the key is new).
*@return a gboolean.
*/
#define CD_CONFIG_GET_BOOLEAN_WITH_DEFAULT(cGroupName, cKeyName, bDefaultValue) cairo_dock_get_boolean_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, bDefaultValue, NULL, NULL)
/** Get the value of a 'boolean' from the conf file, with TRUE as default value.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@return a gboolean.
*/
#define CD_CONFIG_GET_BOOLEAN(cGroupName, cKeyName) CD_CONFIG_GET_BOOLEAN_WITH_DEFAULT (cGroupName, cKeyName, TRUE)

/** Get the value of an 'integer' from the conf file.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param iDefaultValue default value if the group/key is not found (typically if the key is new).
*@return an integer.
*/
#define CD_CONFIG_GET_INTEGER_WITH_DEFAULT(cGroupName, cKeyName, iDefaultValue) cairo_dock_get_integer_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, iDefaultValue, NULL, NULL)
/** Get the value of a 'entier' from the conf file, with 0 as default value.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@return an integer.
*/
#define CD_CONFIG_GET_INTEGER(cGroupName, cKeyName) CD_CONFIG_GET_INTEGER_WITH_DEFAULT (cGroupName, cKeyName, 0)

/** Get the value of a 'double' from the conf file.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param fDefaultValue default value if the group/key is not found (typically if the key is new).
*@return a double.
*/
#define CD_CONFIG_GET_DOUBLE_WITH_DEFAULT(cGroupName, cKeyName, fDefaultValue) cairo_dock_get_double_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, fDefaultValue, NULL, NULL)
/** Get the value of a 'double' from the conf file, with 0. as default value.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@return a double.
*/
#define CD_CONFIG_GET_DOUBLE(cGroupName, cKeyName) CD_CONFIG_GET_DOUBLE_WITH_DEFAULT (cGroupName, cKeyName, 0.)

/** Get the value of an 'integers list' from the conf file.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param iNbElements number of elements to get from the conf file.
*@param iValueBuffer buffer to fill with the values.
*/
#define CD_CONFIG_GET_INTEGER_LIST(cGroupName, cKeyName, iNbElements, iValueBuffer) \
cairo_dock_get_integer_list_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, iValueBuffer, iNbElements, NULL, NULL, NULL)

/** Get the value of a 'string' from the conf file.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param cDefaultValue default value if the group/key is not found (typically if the key is new). can be NULL.
*@return a newly allocated string.
*/
#define CD_CONFIG_GET_STRING_WITH_DEFAULT(cGroupName, cKeyName, cDefaultValue) cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, cDefaultValue, NULL, NULL)
/** Get the value of a 'string' from the conf file, with NULL as default value.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@return a newly allocated string.
*/
#define CD_CONFIG_GET_STRING(cGroupName, cKeyName) CD_CONFIG_GET_STRING_WITH_DEFAULT (cGroupName, cKeyName, NULL)

/** Get the value of a 'file' from the conf file, with NULL as default value. If the value is a file name (not a path), it is supposed to be in the Cairo-Dock's current theme folder. If the value is NULL, the default file is used, taken at the applet's data folder, but the conf file is not updated with this value.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param cDefaultFileName defaul tfile if none is specified in the conf file.
*@return a newly allocated string giving the complete path of the file.
*/
#define CD_CONFIG_GET_FILE_PATH(cGroupName, cKeyName, cDefaultFileName) cairo_dock_get_file_path_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, NULL, NULL, MY_APPLET_SHARE_DATA_DIR, cDefaultFileName)

/** Get the value of a 'strings list' from the conf file.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param length pointer to the number of strings that were extracted from the conf file.
*@param cDefaultValues default value if the group/key is not found (typically if the key is new). It is a string with words separated by ';'. It can be NULL.
*@return a table of strings, to be freeed with 'g_strfreev'.
*/
#define CD_CONFIG_GET_STRING_LIST_WITH_DEFAULT(cGroupName, cKeyName, length, cDefaultValues) cairo_dock_get_string_list_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, length, cDefaultValues, NULL, NULL)
/** Get the value of a 'strings list' from the conf file, with NULL as default value.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param length pointer to the number of strings that were extracted from the conf file.
*@return a table of strings, to be freeed with 'g_strfreev'.
*/
#define CD_CONFIG_GET_STRING_LIST(cGroupName, cKeyName, length) CD_CONFIG_GET_STRING_LIST_WITH_DEFAULT(cGroupName, cKeyName, length, NULL)

/** Get the value of a 'color' in the RGBA format from the conf file.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param pColorBuffer a table of 4 'double' already allocated, that will be filled with the color components.
*@param pDefaultColor default value if the group/key is not found (typically if the key is new). It is a table of 4 'double'. It can be NULL.
*/
#define CD_CONFIG_GET_COLOR_WITH_DEFAULT(cGroupName, cKeyName, pColorBuffer, pDefaultColor) cairo_dock_get_double_list_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, (double*)pColorBuffer, 4, pDefaultColor, NULL, NULL)
/** Get the value of a 'color' in the RGBA format from the conf file, with NULL as default value.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param pColorBuffer a table of 4 'double' already allocated, that will be filled with the color components.
*/
#define CD_CONFIG_GET_COLOR(cGroupName, cKeyName, pColorBuffer) CD_CONFIG_GET_COLOR_WITH_DEFAULT(cGroupName, cKeyName, pColorBuffer, NULL)
/** Get the value of a 'color' in the RGB format from the conf file.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param pColorBuffer a table of 3 'double' already allocated, that will be filled with the color components.
*@param pDefaultColor default value if the group/key is not found (typically if the key is new). It is a table of 3 'double'. It can be NULL.
*/
#define CD_CONFIG_GET_COLOR_RVB_WITH_DEFAULT(cGroupName, cKeyName, pColorBuffer, pDefaultColor) cairo_dock_get_double_list_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, pColorBuffer, 3, pDefaultColor, NULL, NULL)
/** Get the value of a 'color' in the RGB format from the conf file, with NULL as default value.
*@param cGroupName name of the group in the conf file.
*@param cKeyName name of the key in the conf file.
*@param pColorBuffer a table of 3 'double' already allocated, that will be filled with the color components.
*/
#define CD_CONFIG_GET_COLOR_RVB(cGroupName, cKeyName, pColorBuffer) CD_CONFIG_GET_COLOR_RVB_WITH_DEFAULT(cGroupName, cKeyName, pColorBuffer, NULL)

/** Get the complete path of a theme in the conf file.
*@param cGroupName name of the group (in the conf file).
*@param cKeyName name of the key (in the conf file).
*@param cThemeDirName name of the folder containing the local, user, and distant themes.
*@param cDefaultThemeName default value, if the key/group/theme doesn't exist.
*@return Path to the folder of the theme, in a newly allocated string.
*/
#define CD_CONFIG_GET_THEME_PATH(cGroupName, cKeyName, cThemeDirName, cDefaultThemeName) \
	__extension__ ({\
	gchar *_cThemePath = cairo_dock_get_theme_path_for_module (CD_APPLET_MY_CONF_FILE, pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, cDefaultThemeName, MY_APPLET_SHARE_DATA_DIR"/"cThemeDirName, MY_APPLET_USER_DATA_DIR);\
	if (_cThemePath == NULL) {\
		const gchar *_cMessage = _("The theme could not be found; the default theme will be used instead.\n You can change this by opening the configuration of this module. Do you want to do it now?");\
		Icon *_pIcon = cairo_dock_get_dialogless_icon ();\
		gchar *_cQuestion = g_strdup_printf ("%s : %s", myApplet->pModule->pVisitCard->cModuleName, _cMessage);\
		cairo_dock_show_dialog_with_question (_cQuestion, _pIcon, CAIRO_CONTAINER (g_pMainDock), MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE, (CairoDockActionOnAnswerFunc) cairo_dock_open_module_config_on_demand, myApplet, NULL);\
		g_free (_cQuestion); }\
	_cThemePath; })

/** Get the complete path of a Gauge theme in the conf file.
*@param cGroupName name of the group (in the conf file).
*@param cKeyName name of the key (in the conf file).
*/
#define CD_CONFIG_GET_GAUGE_THEME(cGroupName, cKeyName) \
	__extension__ ({\
	gchar *_cThemePath = cairo_dock_get_package_path_for_data_renderer("gauge", CD_APPLET_MY_CONF_FILE, pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, "Turbo-night-fuel");\
	if (_cThemePath == NULL) {\
		const gchar *_cMessage = _("The gauge theme could not be found; a default gauge will be used instead.\nYou can change this by opening the configuration of this module. Do you want to do it now?");\
		Icon *_pIcon = cairo_dock_get_dialogless_icon ();\
		gchar *_cQuestion = g_strdup_printf ("%s : %s", myApplet->pModule->pVisitCard->cModuleName, _cMessage);\
		cairo_dock_show_dialog_with_question (_cQuestion, _pIcon, CAIRO_CONTAINER (g_pMainDock), MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE, (CairoDockActionOnAnswerFunc) cairo_dock_open_module_config_on_demand, myApplet, NULL);\
		g_free (_cQuestion); }\
	_cThemePath; })

/** Rename a group in the conf file, in case you had to change it. Do nothing if the old group no more exists in the conf file.
*@param cGroupName name of the group.
*@param cNewGroupName new name of the group.
*/
#define CD_CONFIG_RENAME_GROUP(cGroupName, cNewGroupName) do {\
	if (cairo_dock_rename_group_in_conf_file (pKeyFile, cGroupName, cNewGroupName))\
		bFlushConfFileNeeded = TRUE; } while (0)

  //////////
 // MENU //
//////////
/** Create and add a sub-menu to a given menu.
*@param cLabel name of the sub-menu.
*@param pMenu GtkWidget of the menu we will add the sub-menu to..
*@param cImage name of an image (can be a path or a GtkStock).
*@return the sub-menu, newly created and attached to the menu.
*/
#define CD_APPLET_ADD_SUB_MENU_WITH_IMAGE(cLabel, pMenu, cImage) \
	cairo_dock_create_sub_menu (cLabel, pMenu, cImage)

/** Create and add a sub-menu to a given menu.
*@param cLabel name of the sub-menu.
*@param pMenu GtkWidget of the menu we will add the sub-menu to..
*@return the sub-menu, newly created and attached to the menu.
*/
#define CD_APPLET_ADD_SUB_MENU(cLabel, pMenu) CD_APPLET_ADD_SUB_MENU_WITH_IMAGE(cLabel, pMenu, NULL)

/** Create and add an entry to a menu, with an icon.
*@param cLabel name of the entry.
*@param gtkStock name of a GTK icon or path to an image.
*@param pCallBack function called when the user selects this entry.
*@param pMenu menu to add the entry to.
*@param pData data passed as parameter of the callback.
*/
#define CD_APPLET_ADD_IN_MENU_WITH_STOCK_AND_DATA(cLabel, gtkStock, pCallBack, pMenu, pData) cairo_dock_add_in_menu_with_stock_and_data (cLabel, gtkStock, G_CALLBACK(pCallBack), pMenu, pData)

/** Create and add an entry to a menu.
*@param cLabel name of the entry.
*@param pCallBack function called when the user selects this entry.
*@param pMenu menu to add the entry to.
*@param pData data passed as parameter of the callback.
*/
#define CD_APPLET_ADD_IN_MENU_WITH_DATA(cLabel, pCallBack, pMenu, pData) CD_APPLET_ADD_IN_MENU_WITH_STOCK_AND_DATA (cLabel, NULL, pCallBack, pMenu, pData)

/** Create and add an entry to a menu. 'myApplet' will be passed to the callback.
*@param cLabel name of the entry.
*@param pCallBack function called when the user selects this entry.
*@param pMenu menu to add the entry to.
*/
#define CD_APPLET_ADD_IN_MENU(cLabel, pCallBack, pMenu) CD_APPLET_ADD_IN_MENU_WITH_DATA(cLabel, pCallBack, pMenu, myApplet)

/** Create and add an entry to a menu, with an icon. 'myApplet' will be passed to the callback.
*@param cLabel name of the entry.
*@param gtkStock name of a GTK icon or path to an image.
*@param pCallBack function called when the user selects this entry.
*@param pMenu menu to add the entry to.
*/
#define CD_APPLET_ADD_IN_MENU_WITH_STOCK(cLabel, gtkStock, pCallBack, pMenu) CD_APPLET_ADD_IN_MENU_WITH_STOCK_AND_DATA(cLabel, gtkStock, pCallBack, pMenu, myApplet)

/** Create and add a separator to a menu.
 */
#define CD_APPLET_ADD_SEPARATOR_IN_MENU(pMenu) do { \
	pMenuItem = gtk_separator_menu_item_new (); \
	gtk_menu_shell_append(GTK_MENU_SHELL (pMenu), pMenuItem); } while (0)

/** Pop-up a menu on the applet's icon.
*@param pMenu menu to show
*/
#define CD_APPLET_POPUP_MENU_ON_MY_ICON(pMenu) cairo_dock_popup_menu_on_icon (pMenu, myIcon, myContainer)

/** Reload the config panel of the applet. This is useful if you have custom widgets inside your conf file, and need to reload them.
*/
#define CD_APPLET_RELOAD_CONFIG_PANEL cairo_dock_reload_current_module_widget (myApplet)
/** Reload the config panel of the applet and jump to the given page. This is useful if you have custom widgets inside your conf file, and need to reload them.
*/
#define CD_APPLET_RELOAD_CONFIG_PANEL_WITH_PAGE(iNumPage) cairo_dock_reload_current_module_widget_full (myApplet, iNumPage)

#define CD_APPLET_GET_CONFIG_PANEL_WIDGET(cGroupName, cKeyName) cairo_dock_get_widget_from_name (myApplet, cGroupName, cKeyName)

#define CD_APPLET_GET_CONFIG_PANEL_GROUP_KEY_WIDGET(cGroupName, cKeyName) cairo_dock_get_group_key_widget_from_name (myApplet, cGroupName, cKeyName)

  /////////////////////////
 // AVAILABLE VARIABLES //
/////////////////////////

//\______________________ init, config, reload.
/** Path of the applet's instance's conf file.
*/
#define CD_APPLET_MY_CONF_FILE myApplet->cConfFilePath
/** Key file of the applet instance, availale during the init, config, and reload.
*/
#define CD_APPLET_MY_KEY_FILE pKeyFile

//\______________________ reload.
/** TRUE if the conf file has changed before the reload.
*/
#define CD_APPLET_MY_CONFIG_CHANGED (pKeyFile != NULL)

/** TRUE if the container type has changed (which can only happen if the config has changed).
*/
#define CD_APPLET_MY_CONTAINER_TYPE_CHANGED (myApplet->pContainer == NULL || myApplet->pContainer->iType != pOldContainer->iType)

/** The previous Container.
*/
#define CD_APPLET_MY_OLD_CONTAINER pOldContainer


//\______________________ clic droit, clic milieu, clic gauche.
/** The clicked Icon.
*/
#define CD_APPLET_CLICKED_ICON pClickedIcon
/** The clicked Container.
*/
#define CD_APPLET_CLICKED_CONTAINER pClickedContainer

//\______________________ clic droit
/**  TRUE if the 'SHIFT' key was pressed during the click.
*/
#define CD_APPLET_SHIFT_CLICK (iButtonState & GDK_SHIFT_MASK)
/**  TRUE if the 'CTRL' key was pressed during the click.
*/
#define CD_APPLET_CTRL_CLICK (iButtonState & GDK_CONTROL_MASK)
/**  TRUE if the 'ALT' key was pressed during the click.
*/
#define CD_APPLET_ALT_CLICK (iButtonState & GDK_MOD1_MASK)

//\______________________ construction du menu.
/** Main menu of the applet.
*/
#define CD_APPLET_MY_MENU pAppletMenu

//\______________________ drop.
/** Data received after a drop occured (string).
*/
#define CD_APPLET_RECEIVED_DATA cReceivedData

//\______________________ scroll
#define CD_APPLET_SCROLL_DIRECTION iDirection
/** TRUE if the user scrolled up.
*/
#define CD_APPLET_SCROLL_UP (CD_APPLET_SCROLL_DIRECTION == GDK_SCROLL_UP)
/** TRUE if the user scrolled down.
*/
#define CD_APPLET_SCROLL_DOWN (CD_APPLET_SCROLL_DIRECTION == GDK_SCROLL_DOWN)


  /////////////////////
 // DRAWING SURFACE //
/////////////////////

/** Redraw the applet's icon (as soon as the main loop is available).
*/
#define CD_APPLET_REDRAW_MY_ICON \
	cairo_dock_redraw_icon (myIcon, myContainer)
/** Redraw the applet's container (as soon as the main loop is available).
*/
#define CAIRO_DOCK_REDRAW_MY_CONTAINER \
	cairo_dock_redraw_container (myContainer)
/** Reload the reflect of the applet's icon (do nothing in OpenGL mode).
*/
#define CD_APPLET_UPDATE_REFLECT_ON_MY_ICON \
	if (myContainer->bUseReflect) cairo_dock_add_reflection_to_icon (myIcon, myContainer)

/** Load an image into a surface, at the same size as the applet's icon. If the image is given by its sole name, it is searched inside the current theme root folder. 
*@param cImagePath path or name of an image.
*@return the newly allocated surface.
*/
#define CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET(cImagePath) \
	cairo_dock_create_surface_from_image_simple (cImagePath, myIcon->fWidth * (myDock ? (1 + myIconsParam.fAmplitude) / myDock->container.fRatio : 1), myIcon->fHeight* (myDock ? (1 + myIconsParam.fAmplitude) / myDock->container.fRatio : 1))

/** Load a user image into a surface, at the same size as the applet's icon, or a default image taken in the installed folder of the applet if the first one is NULL. If the user image is given by its sole name, it is searched inside the current theme root folder.
*@param cUserImageName name or path of an user image.
*@param cDefaultLocalImageName default image
*@return the newly allocated surface.
*/
#define CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET_WITH_DEFAULT(cUserImageName, cDefaultLocalImageName) \
	__extension__ ({\
	cairo_surface_t *pSurface; \
	if (cUserImageName != NULL) \
		pSurface = CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET (cUserImageName); \
	else if (cDefaultLocalImageName != NULL)\
		pSurface = CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET (MY_APPLET_SHARE_DATA_DIR"/"cDefaultLocalImageName); \
	else\
		pSurface = NULL;\
	pSurface; })

/** Apply a surface on the applet's icon, and redraw it.
*@param pSurface the surface to draw on your icon.
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON(pSurface) do { \
	cairo_dock_set_icon_surface_with_reflect (myDrawContext, pSurface, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)

/** Apply a surface on the applet's icon, with a zoom factor and centered, and redraw it.
*@param pSurface the surface to draw on your icon.
*@param fScale zoom factor (at 1 the surface will fill all the icon).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_ZOOM(pSurface, fScale) do { \
	cairo_dock_set_icon_surface_full (myDrawContext, pSurface, fScale, 1., myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)

/** Apply a surface on the applet's icon with a transparency factor, and redraw it.
*@param pSurface the surface to draw on your icon.
*@param fAlpha transparency (in [0,1]).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_ALPHA(pSurface, fAlpha) do { \
	cairo_dock_set_icon_surface_full (myDrawContext, pSurface, 1., fAlpha, myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)

/** Apply a surface on the applet's icon with add a bar at the bottom, and redraw it. The bar is drawn at the bottom of the icon with a gradation from red to green and a given length.
*@param pSurface the surface to draw on your icon.
*@param fValue the value representing a percentage, in [-1,1]. If negative, the gradation is inverted, and the absolute value is used.
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_BAR(pSurface, fValue) do { \
	cairo_dock_set_icon_surface_with_bar (myDrawContext, pSurface, fValue, myIcon); \
	cairo_dock_add_reflection_to_icon (myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)

/** Apply an image on the applet's icon. The image is resized at the same size as the icon. Does not trigger the icon refresh.
*@param cIconName name of an icon or path to an image.
*/
#define CD_APPLET_SET_IMAGE_ON_MY_ICON(cIconName) \
	cairo_dock_set_image_on_icon_with_default (myDrawContext, cIconName, myIcon, myContainer, MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE)

/** Apply an image on the applet's icon, clearing it beforehand, and adding the reflect. The image is searched in any possible locations, and the default image provided is used if the search was fruitless (taken in the installation folder of the applet).
*@param cIconName name of an icon or path to an image.
*@param cDefaultLocalImageName name of an image to use as a fallback (taken in the applet's installation folder).
*/
#define CD_APPLET_SET_USER_IMAGE_ON_MY_ICON(cIconName, cDefaultLocalImageName) \
	cairo_dock_set_image_on_icon_with_default (myDrawContext, cIconName, myIcon, myContainer, MY_APPLET_SHARE_DATA_DIR"/"cDefaultLocalImageName)

/** Apply the default icon on the applet's icon if there is no image yet.
*/
#define CD_APPLET_SET_DEFAULT_IMAGE_ON_MY_ICON_IF_NONE do { \
	if (myIcon->cFileName == NULL) { \
		cairo_dock_set_image_on_icon (myDrawContext, MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE, myIcon, myContainer); } } while (0)


  ///////////
 // LABEL //
///////////
/** Set a new label on the applet's icon.
*@param cIconName the label.
*/
#define CD_APPLET_SET_NAME_FOR_MY_ICON(cIconName) \
	cairo_dock_set_icon_name (cIconName, myIcon, myContainer)
/** Set a new label on the applet's icon.
*@param cIconNameFormat the label, in a 'printf'-like format.
*@param ... values to be written in the string.
*/
#define CD_APPLET_SET_NAME_FOR_MY_ICON_PRINTF(cIconNameFormat, ...) \
	cairo_dock_set_icon_name_printf (myIcon, myContainer, cIconNameFormat, ##__VA_ARGS__)


  ///////////////
 // QUICK-INFO//
///////////////
/** Set a quick-info on the applet's icon.
*@param cQuickInfo the quick-info. This is a small text (a few characters) that is superimposed on the icon.
*/
#define CD_APPLET_SET_QUICK_INFO_ON_MY_ICON(cQuickInfo) \
	cairo_dock_set_quick_info (myIcon, myContainer, cQuickInfo)
/** Set a quick-info on the applet's icon.
*@param cQuickInfoFormat the label, in a 'printf'-like format.
*@param ... values to be written in the string.
*/
#define CD_APPLET_SET_QUICK_INFO_ON_MY_ICON_PRINTF(cQuickInfoFormat, ...) \
	cairo_dock_set_quick_info_printf (myIcon, myContainer, cQuickInfoFormat, ##__VA_ARGS__)

/** Write the time in hours-minutes as a quick-info on the applet's icon.
*@param iTimeInSeconds the time in seconds.
*/
#define CD_APPLET_SET_HOURS_MINUTES_AS_QUICK_INFO(iTimeInSeconds) \
	cairo_dock_set_hours_minutes_as_quick_info (myIcon, myContainer, iTimeInSeconds)
/** Write the time in minutes-secondes as a quick-info on the applet's icon.
*@param iTimeInSeconds the time in seconds.
*/
#define CD_APPLET_SET_MINUTES_SECONDES_AS_QUICK_INFO(iTimeInSeconds) \
	cairo_dock_set_minutes_secondes_as_quick_info (myIcon, myContainer, iTimeInSeconds)
/** Write a size in bytes as a quick-info on the applet's icon.
*@param iSizeInBytes the size in bytes, converted into a readable format.
*/
#define CD_APPLET_SET_SIZE_AS_QUICK_INFO(iSizeInBytes) \
	cairo_dock_set_size_as_quick_info (myIcon, myContainer, iSizeInBytes)


  ///////////////
 // ANIMATION //
///////////////
/** Prevent the applet's icon to be animated when the mouse hovers it (call it once at init).
*/
#define CD_APPLET_SET_STATIC_ICON cairo_dock_set_icon_static (myIcon)

/** Make the applet's icon always visible, even when the dock is hidden.
*/
#define CD_APPLET_SET_ALWAYS_VISIBLE_ICON(bAlwaysVisible) cairo_dock_set_icon_always_visible (myIcon, bAlwaysVisible)

/** Launch an animation on the applet's icon.
*@param cAnimationName name of the animation.
*@param iAnimationLength number of rounds the animation should be played.
*/
#define CD_APPLET_ANIMATE_MY_ICON(cAnimationName, iAnimationLength) \
	cairo_dock_request_icon_animation (myIcon, myContainer, cAnimationName, iAnimationLength)

/** Stop any animation on the applet's icon.
*/
#define CD_APPLET_STOP_ANIMATING_MY_ICON \
	cairo_dock_stop_icon_animation (myIcon)

/** Make applet's icon demanding the attention : it will launch the given animation, and the icon will be visible even if the dock is hidden.
*@param cAnimationName name of the animation.
*@param iAnimationLength number of rounds the animation should be played, or 0 for an endless animation.
*/
#define CD_APPLET_DEMANDS_ATTENTION(cAnimationName, iAnimationLength) \
	do {\
		if (myDock) \
			cairo_dock_request_icon_attention (myIcon, myDock, cAnimationName, iAnimationLength); } while (0)

/** Stop the demand of attention on the applet's icon.
*/
#define CD_APPLET_STOP_DEMANDING_ATTENTION \
	do {\
		if (myDock) \
			cairo_dock_stop_icon_attention (myIcon, myDock); } while (0)


/** Get the dimension allocated to the surface/texture of the applet's icon.
*@param iWidthPtr pointer to the width.
*@param iHeightPtr pointer to the height.
*/
#define CD_APPLET_GET_MY_ICON_EXTENT(iWidthPtr, iHeightPtr) cairo_dock_get_icon_extent (myIcon, iWidthPtr, iHeightPtr)

/** Initiate an OpenGL drawing session on the applet's icon.
*/
#define CD_APPLET_START_DRAWING_MY_ICON cairo_dock_begin_draw_icon (myIcon, myContainer, 0)

/** Initiate an OpenGL drawing session on the applet's icon, or quit the function if failed.
*@param ... value to return in case of failure.
*/
#define CD_APPLET_START_DRAWING_MY_ICON_OR_RETURN(...) \
	if (! cairo_dock_begin_draw_icon (myIcon, myContainer, 0)) \
		return __VA_ARGS__

/** Terminate an OpenGL drawing session on the applet's icon. Does not trigger the icon's redraw.
*/
#define CD_APPLET_FINISH_DRAWING_MY_ICON cairo_dock_end_draw_icon (myIcon, myContainer)


/** Add an overlay from an image on the applet's icon.
 *@param cImageFile an image (if it's not a path, it is searched amongst the current theme's images)
 *@param iPosition position where to display the overlay
 *@return TRUE if the overlay has been successfuly added.
 */
#define CD_APPLET_ADD_OVERLAY_ON_MY_ICON(cImageFile, iPosition) cairo_dock_add_overlay_from_image (myIcon, cImageFile, iPosition)

/** Print an overlay from an image on the applet's icon (it can't be removed without erasing the icon).
 *@param cImageFile an image (if it's not a path, it is searched amongst the current theme's images)
 *@param iPosition position where to display the overlay
 *@return TRUE if the overlay has been successfuly added.
 */
#define CD_APPLET_PRINT_OVERLAY_ON_MY_ICON(cImageFile, iPosition) cairo_dock_print_overlay_on_icon_from_image (myIcon, myContainer, cImageFile, iPosition)

/** Remove an overlay the applet's icon, given its position (there is only one overlay at a given position).
 *@param iPosition position of the overlay
 */
#define CD_APPLET_REMOVE_OVERLAY_ON_MY_ICON(iPosition) cairo_dock_remove_overlay_at_position (myIcon, iPosition)


/** Make an emblem from an image. If the image is given by its sole name, it is looked up inside the root theme folder. Free it with cairo_dock_free_emblem.
*@param cImageFile name of an image file.
*@return a newly allocated CairoEmblem.
*/
#define CD_APPLET_MAKE_EMBLEM(cImageFile) cairo_dock_make_emblem (cImageFile, myIcon)

/** Draw an emblem on the applet's icon. The emblem is drawn directly on the icon, and is erased if the icon is redrawn.
*@param pEmblem an emblem.
*/
#define CD_APPLET_DRAW_EMBLEM_ON_MY_ICON(pEmblem) cairo_dock_draw_emblem_on_icon (pEmblem, myIcon, myContainer)


/** Add a Data Renderer the applet's icon.
*@param pAttr the attributes of the Data Renderer. They allow you to define its properties.
*/
#define CD_APPLET_ADD_DATA_RENDERER_ON_MY_ICON(pAttr) cairo_dock_add_new_data_renderer_on_icon (myIcon, myContainer, pAttr)

/** Reload the Data Renderer of the applet's icon. Pass NULL as the attributes to simply reload the current data renderer without changing any of its parameters. Previous values are kept.
*@param pAttr the attributes of the Data Renderer, or NULL to simply reload the Data Renderer as it it.
*/
#define CD_APPLET_RELOAD_MY_DATA_RENDERER(pAttr) cairo_dock_reload_data_renderer_on_icon (myIcon, myContainer, pAttr)

/** Add new values to the Data Renderer of the applet's icon. Values are a table of 'double', having the same size as defined when the data renderer was created (1 by default). It also triggers the redraw of the icon.
*@param pValues the values, a table of double of the correct size.
*/
#define CD_APPLET_RENDER_NEW_DATA_ON_MY_ICON(pValues) cairo_dock_render_new_data_on_icon (myIcon, myContainer, myDrawContext, pValues)

/** Completely remove the Data Renderer of the applet's icon, including the values associated with.
*/
#define CD_APPLET_REMOVE_MY_DATA_RENDERER cairo_dock_remove_data_renderer_on_icon (myIcon)

/** Refresh the Data Renderer of the applet's icon, to redraw it when the applet's size has changed.
*/
#define CD_APPLET_REFRESH_MY_DATA_RENDERER cairo_dock_refresh_data_renderer (myIcon, myContainer, myDrawContext)


/** Set the history size of the Data Renderer of the applet's icon to the maximum size, that is to say 1 value per pixel.
*/
#define CD_APPLET_SET_MY_DATA_RENDERER_HISTORY_TO_MAX cairo_dock_resize_data_renderer_history (myIcon, myIcon->fWidth)


#define CD_APPLET_GET_MY_ICON_DATA(pIcon) cairo_dock_get_icon_data (pIcon, myApplet)
#define CD_APPLET_GET_MY_CONTAINER_DATA(pContainer) cairo_dock_get_container_data (pContainer, myApplet)
#define CD_APPLET_GET_MY_DOCK_DATA(pDock) CD_APPLET_GET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDock))
#define CD_APPLET_GET_MY_DESKLET_DATA(pDesklet) CD_APPLET_GET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDesklet))

#define CD_APPLET_SET_MY_ICON_DATA(pIcon, pData) cairo_dock_set_icon_data (pIcon, myApplet, pData)
#define CD_APPLET_SET_MY_CONTAINER_DATA(pContainer, pData) cairo_dock_set_container_data (pContainer, myApplet, pData)
#define CD_APPLET_SET_MY_DOCK_DATA(pDock, pData) CD_APPLET_SET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDock), pData)
#define CD_APPLET_SET_MY_DESKLET_DATA(pDesklet, pData) CD_APPLET_SET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDesklet), pData)

#define CD_APPLET_LOAD_LOCAL_TEXTURE(cImageName) cairo_dock_create_texture_from_image (MY_APPLET_SHARE_DATA_DIR"/"cImageName)

#define CD_APPLET_LOAD_TEXTURE_WITH_DEFAULT(cUserImageName, cDefaultLocalImageName) \
	__extension__ ({\
	GLuint iTexture; \
	if (cUserImageName != NULL) \
		iTexture = cairo_dock_create_texture_from_image (cUserImageName); \
	else if (cDefaultLocalImageName != NULL)\
		iTexture = cairo_dock_create_texture_from_image (MY_APPLET_SHARE_DATA_DIR"/"cDefaultLocalImageName); \
	else\
		iTexture = 0;\
	iTexture; })

/** Say if the applet's container currently supports OpenGL.
*/
#define CD_APPLET_MY_CONTAINER_IS_OPENGL (g_bUseOpenGL && ((myDock && myDock->pRenderer->render_opengl) || (myDesklet && myDesklet->pRenderer && myDesklet->pRenderer->render_opengl)))

#define CD_APPLET_SET_TRANSITION_ON_MY_ICON(render_step_cairo, render_step_opengl, bFastPace, iDuration, bRemoveWhenFinished) \
	cairo_dock_set_transition_on_icon (myIcon, myContainer,\
		(CairoDockTransitionRenderFunc) render_step_cairo,\
		(CairoDockTransitionGLRenderFunc) render_step_opengl,\
		bFastPace,\
		iDuration,\
		bRemoveWhenFinished,\
		myApplet,\
		NULL)
#define CD_APPLET_GET_TRANSITION_FRACTION(...) \
	(cairo_dock_has_transition (myIcon) ? cairo_dock_get_transition_fraction (myIcon) : 1.)
#define CD_APPLET_REMOVE_TRANSITION_ON_MY_ICON \
	cairo_dock_remove_transition_on_icon (myIcon)


//\_________________________________ DESKLETS et SOUS-DOCKS

/** Set a renderer to the applet's desklet and create myDrawContext. Call it at the beginning of init and also reload, to take into account the desklet's resizing.
*@param cRendererName name of the renderer.
*@param pConfig configuration data for the renderer, or NULL.
*/
#define CD_APPLET_SET_DESKLET_RENDERER_WITH_DATA(cRendererName, pConfig) do { \
	cairo_dock_set_desklet_renderer_by_name (myDesklet, cRendererName, (CairoDeskletRendererConfigPtr) pConfig); \
	if (myDrawContext) cairo_destroy (myDrawContext);\
	if (myIcon->pIconBuffer != NULL)\
		myDrawContext = cairo_create (myIcon->pIconBuffer);\
	else myDrawContext = NULL; } while (0)

/** Set a renderer to the applet's desklet and create myDrawContext. Call it at the beginning of init and also reload, to take into account the desklet's resizing.
*@param cRendererName name of the renderer.
*/
#define CD_APPLET_SET_DESKLET_RENDERER(cRendererName) CD_APPLET_SET_DESKLET_RENDERER_WITH_DATA (cRendererName, NULL)

/** Prevent the desklet from being rotated. Use it if your desklet has some static GtkWidget inside.
*/
#define CD_APPLET_SET_STATIC_DESKLET cairo_dock_set_static_desklet (myDesklet)

/** Prevent the desklet from being transparent to click. Use it if your desklet has no meaning in being unclickable.
*/
#define CD_APPLET_ALLOW_NO_CLICKABLE_DESKLET cairo_dock_allow_no_clickable_desklet (myDesklet)


/** Delete the list of icons of an applet (keep the subdock in dock mode).
*/
#define CD_APPLET_DELETE_MY_ICONS_LIST cairo_dock_remove_all_icons_from_applet (myApplet)

/** Remove an icon from the list of icons of an applet. The icon is destroyed and should not be used after that.
* @param pIcon the icon to remove.
* @return whether the icon has been removed or not. In any case, the icon is freed.
*/
#define CD_APPLET_REMOVE_ICON_FROM_MY_ICONS_LIST(pIcon) cairo_dock_remove_icon_from_applet (myApplet, pIcon)

/** Detach an icon from the list of icons of an applet. The icon is not destroyed.
* @param pIcon the icon to remove.
* @return whether the icon has been removed or not.
*/
#define CD_APPLET_DETACH_ICON_FROM_MY_ICONS_LIST(pIcon) cairo_dock_detach_icon_from_applet (myApplet, pIcon)

/** Load a list of icons into an applet, with the given renderer for the sub-dock or the desklet. The icons will be loaded automatically in an idle process.
*@param pIconList a list of icons. It will belong to the applet's container after that.
*@param cDockRendererName name of a renderer in case the applet is in dock mode.
*@param cDeskletRendererName name of a renderer in case the applet is in desklet mode.
*@param pDeskletRendererConfig possible configuration parameters for the desklet renderer.
*/
#define CD_APPLET_LOAD_MY_ICONS_LIST(pIconList, cDockRendererName, cDeskletRendererName, pDeskletRendererConfig) do {\
	cairo_dock_insert_icons_in_applet (myApplet, pIconList, cDockRendererName, cDeskletRendererName, pDeskletRendererConfig);\
	if (myDesklet && myIcon->pIconBuffer != NULL && myDrawContext == NULL)\
		myDrawContext = cairo_create (myIcon->pIconBuffer); } while (0)

/** Add an icon into an applet. The view previously set by CD_APPLET_LOAD_MY_ICONS_LIST will be used. The icon will be loaded automatically in an idle process.
*@param pIcon an icon.
*/
#define CD_APPLET_ADD_ICON_IN_MY_ICONS_LIST(pIcon) cairo_dock_insert_icon_in_applet (myApplet, pIcon)

/** Get the list of icons of your applet. It is either the icons of your sub-dock or of your desklet.
*/
#define CD_APPLET_MY_ICONS_LIST (myDock ? (myIcon->pSubDock ? myIcon->pSubDock->icons : NULL) : myDesklet->icons)
/** Get the container of the icons of your applet. It is either your sub-dock or your desklet.
*/
#define CD_APPLET_MY_ICONS_LIST_CONTAINER (myDock && myIcon->pSubDock ? CAIRO_CONTAINER (myIcon->pSubDock) : myContainer)

//\_________________________________ TASKBAR
/** Let your applet control the window of an external program, instead of the Taskbar.
 *\param cApplicationClass the class of the application you wish to control (in lower case), or NULL to stop controling any appli.
*/
#define CD_APPLET_MANAGE_APPLICATION(cApplicationClass) do {\
	if (cairo_dock_strings_differ (myIcon->cClass, (cApplicationClass))) {\
		if (myIcon->cClass != NULL)\
			cairo_dock_deinhibite_class (myIcon->cClass, myIcon);\
		if ((cApplicationClass) != NULL)\
			cairo_dock_inhibite_class ((cApplicationClass), myIcon); } } while (0)

//\_________________________________ INTERNATIONNALISATION
/** Macro for gettext, similar to _() et N_(), but with the domain of the applet. Surround all your strings with this, so that 'xgettext' can find them and automatically include them in the translation files.
*/
#define D_(message) dgettext (MY_APPLET_GETTEXT_DOMAIN, message)
#define _D D_

//\_________________________________ DEBUG
#define CD_APPLET_ENTER g_pCurrentModule = myApplet
#define CD_APPLET_LEAVE(...) do {\
	g_pCurrentModule = NULL;\
	return __VA_ARGS__;} while (0)
#define CD_APPLET_LEAVE_IF_FAIL(condition, ...) do {\
	if (! (condition)) {\
		cd_warning ("condition "#condition" failed");\
		g_pCurrentModule = NULL;\
		return __VA_ARGS__;} } while (0)

#define CD_WARNING(s,...) cd_warning ("%s : "##s, myApplet->pModule->pVisitCard->cModuleName, ##__VA_ARGS__)
#define CD_MESSAGE(s,...) cd_message ("%s : "##s, myApplet->pModule->pVisitCard->cModuleName, ##__VA_ARGS__)
#define CD_DEBUG(s,...) cd_debug ("%s : "##s, myApplet->pModule->pVisitCard->cModuleName, ##__VA_ARGS__)


G_END_DECLS
#endif
