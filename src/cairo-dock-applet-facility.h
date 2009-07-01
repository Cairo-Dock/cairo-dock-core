/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */

#ifndef __CAIRO_DOCK_APPLET_FACILITY__
#define  __CAIRO_DOCK_APPLET_FACILITY__

#include "cairo-dock-struct.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-menu.h"
#include "cairo-dock-applet-factory.h"
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
*@param pContainer the container of the icon.
*/
void cairo_dock_draw_bar_on_icon (cairo_t *pIconContext, double fValue, Icon *pIcon, CairoContainer *pContainer);

void cairo_dock_set_icon_surface_with_bar (cairo_t *pIconContext, cairo_surface_t *pSurface, double fValue, Icon *pIcon, CairoContainer *pContainer);

/** Apply a surface on the context of an icon, clearing it beforehand, and adding the reflect.
*@param pIconContext the drawing context; is not altered by the function.
*@param pSurface the surface to apply.
*@param pIcon the icon.
*@param pContainer the container of the icon.
*/
void cairo_dock_set_icon_surface_with_reflect (cairo_t *pIconContext, cairo_surface_t *pSurface, Icon *pIcon, CairoContainer *pContainer);

/** Apply an image on the context of an icon, clearing it beforehand, and adding the reflect.
*@param pIconContext the drawing context; is not altered by the function.
*@param cImagePath path of an image t apply on the icon.
*@param pIcon the icon.
*@param pContainer the container of the icon.
*/
void cairo_dock_set_image_on_icon (cairo_t *pIconContext, gchar *cImagePath, Icon *pIcon, CairoContainer *pContainer);


void cairo_dock_set_hours_minutes_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds);
void cairo_dock_set_minutes_secondes_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds);

/** Convert a size in bytes into a readable format.
*@param iSizeInBytes size in bytes.
*@return a newly allocated string.
*/
gchar *cairo_dock_get_human_readable_size (long long int iSizeInBytes);
void cairo_dock_set_size_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, long long int iSizeInBytes);

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


gchar *cairo_dock_get_theme_path_for_module (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName, const gchar *cShareThemesDir, const gchar *cExtraDirName);

GtkWidget *cairo_dock_create_sub_menu (const gchar *cLabel, GtkWidget *pMenu, const gchar *cImage);


/** Play a sound, through Alsa or PulseAudio.
*@param cSoundPath path to an audio file.
*/
void cairo_dock_play_sound (const gchar *cSoundPath);


/** Get the Gnome's version.
*@param iMajor pointer to the major.
*@param iMinor pointer to the minor.
*@param iMicro pointer to the micro.
*/
void cairo_dock_get_gnome_version (int *iMajor, int *iMinor, int *iMicro);


void cairo_dock_pop_up_about_applet (GtkMenuItem *menu_item, CairoDockModuleInstance *pModuleInstance);

void cairo_dock_open_module_config_on_demand (int iClickedButton, GtkWidget *pInteractiveWidget, CairoDockModuleInstance *pModuleInstance, CairoDialog *pDialog);



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
/** Get the value of a 'booleen' from the conf file, with TRUE as default value.
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
#define CD_CONFIG_GET_DOUBLE_WITH_DEFAULT(cGroupName, cKeyName, fDefaultValue) cairo_dock_get_double_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, 0., NULL, NULL)
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
	gchar *_cThemePath = cairo_dock_get_theme_path_for_module (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, cDefaultThemeName, MY_APPLET_SHARE_DATA_DIR"/"cThemeDirName, MY_APPLET_USER_DATA_DIR);\
	if (_cThemePath == NULL) {\
		const gchar *_cMessage = _("the theme couldn't be found; the default theme will be used instead.\n You can change this by opening the configuration of this module; do you want to do it now ?");\
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
	gchar *_cThemePath = cairo_dock_get_gauge_key_value(CD_APPLET_MY_CONF_FILE, pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, "turbo-night-fuel");\
	if (_cThemePath == NULL) {\
		const gchar *_cMessage = _("the gauge theme couldn't be found; a default gauge will be used instead.\n You can change this by opening the configuration of this module; do you want to do it now ?");\
		Icon *_pIcon = cairo_dock_get_dialogless_icon ();\
		gchar *_cQuestion = g_strdup_printf ("%s : %s", myApplet->pModule->pVisitCard->cModuleName, _cMessage);\
		cairo_dock_show_dialog_with_question (_cQuestion, _pIcon, CAIRO_CONTAINER (g_pMainDock), MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE, (CairoDockActionOnAnswerFunc) cairo_dock_open_module_config_on_demand, myApplet, NULL);\
		g_free (_cQuestion); }\
	_cThemePath; })


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
#define CD_APPLET_CREATE_AND_ADD_SUB_MENU CD_APPLET_ADD_SUB_MENU

/** Create and add the default sub-menu of an applet to the main menu. This sub-menu is named according to the name of the applet, and is represented by the default icon of the applet.
*@return the sub-menu, newly created and attached to the main menu.
*/
#define CD_APPLET_CREATE_MY_SUB_MENU(...) CD_APPLET_ADD_SUB_MENU_WITH_IMAGE (myApplet->pModule->pVisitCard->cModuleName, CD_APPLET_MY_MENU, MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE)

/** Create and add an entry to a menu.
*@param cLabel name of the entry.
*@param pFunction function called when the user selects this entry.
*@param pMenu menu to add the entry to.
*@param pData data passed as parameter of the callback.
*/
#define CD_APPLET_ADD_IN_MENU_WITH_DATA(cLabel, pFunction, pMenu, pData) do { \
	pMenuItem = gtk_menu_item_new_with_label (cLabel); \
	gtk_menu_shell_append  (GTK_MENU_SHELL (pMenu), pMenuItem); \
	g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK (pFunction), pData); } while (0)

/** Create and add an entry to a menu. 'myApplet' will be passed to the callback.
*@param cLabel name of the entry.
*@param pFunction function called when the user selects this entry.
*@param pMenu menu to add the entry to.
*/
#define CD_APPLET_ADD_IN_MENU(cLabel, pFunction, pMenu) CD_APPLET_ADD_IN_MENU_WITH_DATA(cLabel, pFunction, pMenu, myApplet)

/** Create and add an entry to a menu, with an icon.
*@param cLabel name of the entry.
*@param gtkStock name of a GTK icon or path to an image.
*@param pFunction function called when the user selects this entry.
*@param pMenu menu to add the entry to.
*@param pData data passed as parameter of the callback.
*/
#define CD_APPLET_ADD_IN_MENU_WITH_STOCK_AND_DATA CAIRO_DOCK_ADD_IN_MENU_WITH_STOCK_AND_DATA

/** Create and add an entry to a menu, with an icon. 'myApplet' will be passed to the callback.
*@param cLabel name of the entry.
*@param gtkStock name of a GTK icon or path to an image.
*@param pFunction function called when the user selects this entry.
*@param pMenu menu to add the entry to.
*/
#define CD_APPLET_ADD_IN_MENU_WITH_STOCK(cLabel, gtkStock, pFunction, pMenu) CD_APPLET_ADD_IN_MENU_WITH_STOCK_AND_DATA(cLabel, gtkStock, pFunction, pMenu, myApplet)

/** Create and add a separator to a menu.
 */
#define CD_APPLET_ADD_SEPARATOR_IN_MENU(pMenu) do { \
	pMenuItem = gtk_separator_menu_item_new (); \
	gtk_menu_shell_append(GTK_MENU_SHELL (pMenu), pMenuItem); } while (0)
#define CD_APPLET_ADD_SEPARATOR CD_APPLET_ADD_SEPARATOR_IN_MENU

/** Create and add an entry to a menu for the 'about' function.
*@param pMenu menu to add the entry to.
*/
#define CD_APPLET_ADD_ABOUT_IN_MENU(pMenu) CD_APPLET_ADD_IN_MENU_WITH_STOCK (_("About this applet"), GTK_STOCK_ABOUT, cairo_dock_pop_up_about_applet, pMenu)


  /////////////////////////
 // AVAILABLE VARIABLES //
/////////////////////////

//\______________________ init, config, reload.
/** Path of the applet instance's conf file.
*/
#define CD_APPLET_MY_CONF_FILE myApplet->cConfFilePath
/** Key file of the applet instance, availale during the init, config, and reload.
*/
#define CD_APPLET_MY_KEY_FILE pKeyFile

//\______________________ reload.
/** TRUE if the conf file has changed before the reload.
*/
#define CD_APPLET_MY_CONFIG_CHANGED (pKeyFile != NULL)

/** TRUE if the container type has changed.
*/
#define CD_APPLET_MY_CONTAINER_TYPE_CHANGED (myApplet->pContainer == NULL || myApplet->pContainer->iType != pOldContainer->iType)

/** The previous Container.
*/
#define CD_APPLET_MY_OLD_CONTAINER pOldContainer;


//\______________________ clic droit, clic milieu, clic gauche.
/** The clicked Icon.
*/
#define CD_APPLET_CLICKED_ICON pClickedIcon
/** The clicked Container.
*/
#define CD_APPLET_CLICKED_CONTAINER pClickedContainer

//\______________________ clic droit
/**  TRUE if the 'SHIFT' key was pressed during the click ?
*/
#define CD_APPLET_SHIFT_CLICK (iButtonState & GDK_SHIFT_MASK)
/**  TRUE if the 'CTRL' key was pressed during the click ?
*/
#define CD_APPLET_CTRL_CLICK (iButtonState & GDK_CONTROL_MASK)
/**  TRUE if the 'ALT' key was pressed during the click ?
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
/** TRUE if the users scrolled up.
*/
#define CD_APPLET_SCROLL_UP (CD_APPLET_SCROLL_DIRECTION == GDK_SCROLL_UP)
/** TRUE if the users scrolled down.
*/
#define CD_APPLET_SCROLL_DOWN (CD_APPLET_SCROLL_DIRECTION == GDK_SCROLL_DOWN)


  /////////////////////
 // DRAWING SURFACE //
/////////////////////

/** Redraw the applet's icon (as soon as the main loop is available).
*/
#define CD_APPLET_REDRAW_MY_ICON \
	cairo_dock_redraw_icon (myIcon, myContainer)
#define CAIRO_DOCK_REDRAW_MY_CONTAINER \
	cairo_dock_redraw_container (myContainer)
/** Reload the reflect of the applet's icon (useless in OpenGL mode).
*/
#define CD_APPLET_UPDATE_REFLECT_ON_MY_ICON \
	if (myContainer->bUseReflect) cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer)

/**
*Charge une image dans une surface, aux dimensions of the icon of l'applet.
*@param cImagePath chemin du fichier of l'image.
*@return la surface nouvellement creee.
*/
#define CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET(cImagePath) \
	cairo_dock_create_surface_for_icon (cImagePath, myDrawContext, myIcon->fWidth * (myDock ? (1 + g_fAmplitude) / myDock->fRatio : 1), myIcon->fHeight* (myDock ? (1 + g_fAmplitude) / myDock->fRatio : 1))
/**
*Charge une image utilisateur dans une surface, aux dimensions of the icon of l'applet, ou une image par defaut si la premiere est NULL.
*@param cUserImageName nom of l'image utilisateur.
*@param cDefaultLocalImageName icone par defaut
*@return la surface nouvellement creee.
*/
#define CD_APPLET_LOAD_USER_SURFACE_FOR_MY_APPLET(cUserImageName, cDefaultLocalImageName) \
	__extension__ ({\
	gchar *_cImagePath; \
	if (cUserImageName != NULL) \
		_cImagePath = cairo_dock_generate_file_path (cUserImageName); \
	else if (cDefaultLocalImageName != NULL)\
		_cImagePath = g_strdup_printf ("%s/%s", MY_APPLET_SHARE_DATA_DIR, cDefaultLocalImageName); \
	else\
		_cImagePath = NULL;\
	cairo_surface_t *pSurface = CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET (_cImagePath); \
	g_free (_cImagePath);\
	pSurface; })


/**
*Applique une surface existante sur le contexte of dessin of l'applet, et la rafraichit.
*@param pSurface la surface cairo a dessiner.
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON(pSurface) do { \
	cairo_dock_set_icon_surface_with_reflect (myDrawContext, pSurface, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)
/**
*Applique une surface existante sur le contexte of dessin of l'applet en la zoomant, et la redessine.
*@param pSurface la surface cairo a dessiner.
*@param fScale le facteur of zoom (a 1 la surface remplit toute the icon).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_ZOOM(pSurface, fScale) do { \
	cairo_dock_set_icon_surface_full (myDrawContext, pSurface, fScale, 1., myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)
/**
*Applique une surface existante sur le contexte of dessin of l'applet with un facteur of transparence, et la rafraichit.
*@param pSurface la surface cairo a dessiner.
*@param fAlpha la transparence (dans [0 , 1]).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_ALPHA(pSurface, fAlpha) do { \
	cairo_dock_set_icon_surface_full (myDrawContext, pSurface, 1., fAlpha, myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)
/**
*Applique une surface existante sur le contexte of dessin of l'applet et ajoute une barre a sa base, et la rafraichit.
*@param pSurface la surface cairo a dessiner.
*@param fValue la valeur en fraction of la valeur max (donc dans [0 , 1]).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_BAR(pSurface, fValue) do { \
	cairo_dock_set_icon_surface_with_bar (myDrawContext, pSurface, fValue, myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)

/**
*Applique une image definie par son chemin sur le contexte of dessin of l'applet, mais ne la rafraichit pas. L'image est redimensionnee aux dimensions of the icon.
*@param cImagePath chemin du fichier of l'image.
*/
#define CD_APPLET_SET_IMAGE_ON_MY_ICON(cImagePath) do { \
	if (cImagePath != myIcon->acFileName) \
	{ \
		g_free (myIcon->acFileName); \
		myIcon->acFileName = g_strdup (cImagePath); \
	} \
	cairo_dock_set_image_on_icon (myDrawContext, cImagePath, myIcon, myContainer); } while (0)

/**
*Idem que precedemment mais l'image est definie par son nom localement au repertoire d'installation of l'applet
*@param cImageName nom du fichier of l'image 
*/
#define CD_APPLET_SET_LOCAL_IMAGE_ON_MY_ICON(cImageName) do { \
	gchar *_cImageFilePath = g_strconcat (MY_APPLET_SHARE_DATA_DIR, "/", cImageName, NULL); \
	CD_APPLET_SET_IMAGE_ON_MY_ICON (_cImageFilePath); \
	g_free (_cImageFilePath); } while (0)

/**
*Idem que precedemment mais l'image est definie soit relativement au repertoire utilisateur, soit relativement au repertoire d'installation of l'applet si la 1ere est NULL.
*@param cUserImageName nom du fichier of l'image cote utilisateur.
*@param cDefaultLocalImageName image locale par defaut cote installation.
*/
#define CD_APPLET_SET_USER_IMAGE_ON_MY_ICON(cUserImageName, cDefaultLocalImageName) do { \
	gchar *cImagePath; \
	if (cUserImageName != NULL) \
		cImagePath = cairo_dock_generate_file_path (cUserImageName); \
	else \
		cImagePath = g_strdup_printf ("%s/%s", MY_APPLET_SHARE_DATA_DIR, cDefaultLocalImageName); \
	CD_APPLET_SET_IMAGE_ON_MY_ICON (cImagePath); \
	g_free (cImagePath); } while (0)

#define CD_APPLET_SET_DEFAULT_IMAGE_ON_MY_ICON_IF_NONE do { \
	if (myIcon->acFileName == NULL) { \
		CD_APPLET_SET_LOCAL_IMAGE_ON_MY_ICON (MY_APPLET_ICON_FILE); } } while (0)

/**
*Applique une surface existante sur le contexte of dessin of l'applet, et la redessine. La surface est redimensionnee aux dimensions of the icon.
*@param pSurface
*@param fScale
*/
#define CD_APPLET_SET_ZOOMED_SURFACE_ON_MY_ICON(pSurface, fScale) do { \
	cairo_dock_set_icon_surface_with_zoom (myDrawContext, pSurface, fScale, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)


  /////////////
 /// LABEL ///
/////////////
/**
*Remplace l'etiquette of the icon of l'applet par une nouvelle.
*@param cIconName la nouvelle etiquette.
*/
#define CD_APPLET_SET_NAME_FOR_MY_ICON(cIconName) \
	cairo_dock_set_icon_name (myDrawContext, cIconName, myIcon, myContainer)
/**
*Remplace l'etiquette of the icon of l'applet par une nouvelle.
*@param cIconNameFormat la nouvelle etiquette au format 'printf'.
*/
#define CD_APPLET_SET_NAME_FOR_MY_ICON_PRINTF(cIconNameFormat, ...) \
	cairo_dock_set_icon_name_full (myDrawContext, myIcon, myContainer, cIconNameFormat, ##__VA_ARGS__)


  /////////////////
 /// QUICK-INFO///
/////////////////
/**
*Ecris une info-rapide sur the icon of l'applet.
*@param cQuickInfo l'info-rapide. Ce doit etre une chaine of caracteres particulièrement petite, representant une info concise, puisque ecrite directement sur the icon.
*/
#define CD_APPLET_SET_QUICK_INFO_ON_MY_ICON(cQuickInfo) \
	cairo_dock_set_quick_info (myDrawContext, cQuickInfo, myIcon, myDock ? (1 + myIcons.fAmplitude) / 1 : 1)
/**
*Ecris une info-rapide sur the icon of l'applet.
*@param cQuickInfoFormat l'info-rapide, au format 'printf'. Ce doit etre une chaine of caracteres particulièrement petite, representant une info concise, puisque ecrite directement sur the icon.
*/
#define CD_APPLET_SET_QUICK_INFO_ON_MY_ICON_PRINTF(cQuickInfoFormat, ...) \
	cairo_dock_set_quick_info_full (myDrawContext, myIcon, myContainer, cQuickInfoFormat, ##__VA_ARGS__)

/** Write the time in hours-minutes as a quick-info on the applet's icon.
*@param iTimeInSeconds the time in seconds.
*/
#define CD_APPLET_SET_HOURS_MINUTES_AS_QUICK_INFO(iTimeInSeconds) \
	cairo_dock_set_hours_minutes_as_quick_info (myDrawContext, myIcon, myContainer, iTimeInSeconds)
/** Write the time in minutes-secondes as a quick-info on the applet's icon.
*@param iTimeInSeconds the time in seconds.
*/
#define CD_APPLET_SET_MINUTES_SECONDES_AS_QUICK_INFO(iTimeInSeconds) \
	cairo_dock_set_minutes_secondes_as_quick_info (myDrawContext, myIcon, myContainer, iTimeInSeconds)
/** Write a size in bytes as a quick-info on the applet's icon.
*@param iSizeInBytes the size in bytes, converted into a readable format.
*/
#define CD_APPLET_SET_SIZE_AS_QUICK_INFO(iSizeInBytes) \
	cairo_dock_set_size_as_quick_info (myDrawContext, myIcon, myContainer, iSizeInBytes)


  ///////////////
 // ANIMATION //
///////////////
/** Prevent the applet's icon to be animated when the mouse hovers it (call it once at init).
*/
#define CD_APPLET_SET_STATIC_ICON cairo_dock_set_icon_static (myIcon)

/** Request an animation on applet's icon.
*@param cAnimationName name of the animation.
*@param iAnimationLength number of rounds the animation should be played.
*/
#define CD_APPLET_ANIMATE_MY_ICON(cAnimationName, iAnimationLength) \
	cairo_dock_request_icon_animation (myIcon, myDock, cAnimationName, iAnimationLength)

/** Get the dimension allocated to the surface/texture of the applet's icon.
*/
#define CD_APPLET_GET_MY_ICON_EXTENT(iWidthPtr, iHeightPtr) cairo_dock_get_icon_extent (myIcon, myContainer, iWidthPtr, iHeightPtr)


/** Initiate an OpenGL drawing session on the applet's icon.
*/
#define CD_APPLET_START_DRAWING_MY_ICON cairo_dock_begin_draw_icon (myIcon, myContainer)

/** Initiate an OpenGL drawing session on the applet's icon, or quit the function if failed.
*@param ... value to return in case of failure.
*/
#define CD_APPLET_START_DRAWING_MY_ICON_OR_RETURN(...) \
	if (! cairo_dock_begin_draw_icon (myIcon, myContainer)) \
		return __VA_ARGS__

/** Terminate an OpenGL drawing session on the applet's icon. Does not trigger the icon's redraw.
*/
#define CD_APPLET_FINISH_DRAWING_MY_ICON cairo_dock_end_draw_icon (myIcon, myContainer)

/** Add a Data Renderer the applet's icon.
*/
#define CD_APPLET_ADD_DATA_RENDERER_ON_MY_ICON(pAttr) cairo_dock_add_new_data_renderer_on_icon (myIcon, myContainer, myDrawContext, pAttr)
/** Reload the Data Renderer of the applet's icon. Pass NULL as the attributes to simply reload the current data renderer without changing any of its parameters. Previous values are kept.
*/
#define CD_APPLET_RELOAD_MY_DATA_RENDERER(pAttr) cairo_dock_reload_data_renderer_on_icon (myIcon, myContainer, myDrawContext, pAttr)
/** Add new values to the Data Renderer of the applet's icon. Values are a table of 'double', having the same size as defined when the data renderer was created (1 by default).
*/
#define CD_APPLET_RENDER_NEW_DATA_ON_MY_ICON(pValues) cairo_dock_render_new_data_on_icon (myIcon, myContainer, myDrawContext, pValues)
/** Completely remove the Data Renderer of the applet's icon, including the values associated with.
*/
#define CD_APPLET_REMOVE_MY_DATA_RENDERER cairo_dock_remove_data_renderer_on_icon (myIcon)

///__________________ deprecated ___________________
#define CD_APPLET_RENDER_GAUGE(pGauge, fValue) cairo_dock_render_gauge (myDrawContext, myContainer, myIcon, pGauge, fValue)
#define CD_APPLET_RENDER_GAUGE_MULTI_VALUE(pGauge, pValueList) cairo_dock_render_gauge_multi_value (myDrawContext, myContainer, myIcon, pGauge, pValueList)
#define CD_APPLET_RENDER_GRAPH(pGraph) cairo_dock_render_graph (myDrawContext, myContainer, myIcon, pGraph);
#define CD_APPLET_RENDER_GRAPH_NEW_VALUE(pGraph, fValue) do { \
	cairo_dock_update_graph (pGraph, fValue); \
	CD_APPLET_RENDER_GRAPH (pGraph); } while (0)
#define CD_APPLET_RENDER_GRAPH_NEW_VALUES(pGraph, fValue, fValue2) do { \
	cairo_dock_update_double_graph (pGraph, fValue, fValue2); \
	CD_APPLET_RENDER_GRAPH (pGraph); } while (0)
///__________________ end of deprecated ____________

#define CD_APPLET_GET_MY_ICON_DATA(pIcon) cairo_dock_get_icon_data (pIcon, myApplet)
#define CD_APPLET_GET_MY_CONTAINER_DATA(pContainer) cairo_dock_get_container_data (pContainer, myApplet)
#define CD_APPLET_GET_MY_DOCK_DATA(pDock) CD_APPLET_GET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDock))
#define CD_APPLET_GET_MY_DESKLET_DATA(pDesklet) CD_APPLET_GET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDesklet))

#define CD_APPLET_SET_MY_ICON_DATA(pIcon, pData) cairo_dock_set_icon_data (pIcon, myApplet, pData)
#define CD_APPLET_SET_MY_CONTAINER_DATA(pContainer, pData) cairo_dock_set_container_data (pContainer, myApplet, pData)
#define CD_APPLET_SET_MY_DOCK_DATA(pDock, pData) CD_APPLET_SET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDock), pData)
#define CD_APPLET_SET_MY_DESKLET_DATA(pDesklet, pData) CD_APPLET_SET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDesklet), pData)

#define CD_APPLET_LOAD_LOCAL_TEXTURE(cImageName) cairo_dock_load_local_texture (cImageName, MY_APPLET_SHARE_DATA_DIR)

#define CD_APPLET_MY_CONTAINER_IS_OPENGL (g_bUseOpenGL && ((myDock && myDock->render_opengl) || (myDesklet && myDesklet->pRenderer && myDesklet->pRenderer->render_opengl)))

#define CD_APPLET_SET_TRANSITION_ON_MY_ICON(render_step_cairo, render_step_opengl, bFastPace, iDuration, bRemoveWhenFinished) \
	cairo_dock_set_transition_on_icon (myIcon, myContainer, myDrawContext,\
		(CairoDockTransitionRenderFunc) render_step_cairo,\
		(CairoDockTransitionGLRenderFunc) render_step_opengl,\
		bFastPace,\
		iDuration,\
		bRemoveWhenFinished,\
		myApplet)
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
	cairo_dock_set_desklet_renderer_by_name (myDesklet, cRendererName, NULL, CAIRO_DOCK_LOAD_ICONS_FOR_DESKLET, (CairoDeskletRendererConfigPtr) pConfig); \
	myDrawContext = cairo_create (myIcon->pIconBuffer); } while (0)
/** Set a renderer to the applet's desklet and create myDrawContext. Call it at the beginning of init and also reload, to take into account the desklet's resizing.
*@param cRendererName name of the renderer.
*/
#define CD_APPLET_SET_DESKLET_RENDERER(cRendererName) CD_APPLET_SET_DESKLET_RENDERER_WITH_DATA (cRendererName, NULL)

/** Prevent the desklet from being rotated. Use it if your desklet has some static GtkWidget inside.
*/
#define CD_APPLET_SET_STATIC_DESKLET cairo_dock_set_static_desklet (myDesklet)

#define CD_APPLET_CREATE_MY_SUBDOCK(pIconsList, cRenderer) do { \
	if (myIcon->acName == NULL) { \
		CD_APPLET_SET_NAME_FOR_MY_ICON (myApplet->pModule->pVisitCard->cModuleName); } \
	if (cairo_dock_check_unique_subdock_name (myIcon)) { \
		CD_APPLET_SET_NAME_FOR_MY_ICON (myIcon->acName); } \
	myIcon->pSubDock = cairo_dock_create_subdock_from_scratch (pIconsList, myIcon->acName, myDock); \
	cairo_dock_set_renderer (myIcon->pSubDock, cRenderer); \
	cairo_dock_update_dock_size (myIcon->pSubDock); } while (0)

#define CD_APPLET_DESTROY_MY_SUBDOCK do { \
	cairo_dock_destroy_dock (myIcon->pSubDock, myIcon->acName, NULL, NULL); \
	myIcon->pSubDock = NULL; } while (0)

#define CD_APPLET_LOAD_ICONS_IN_MY_SUBDOCK(pIconsList) do { \
	if (myIcon->acName == NULL) { \
		CD_APPLET_SET_NAME_FOR_MY_ICON (myIcon->pModuleInstance->pModule->pVisitCard->cModuleName); }\
	else { \
		Icon *icon;\
		GList *ic;\
		for (ic = pIconsList; ic != NULL; ic = ic->next) {\
			icon = ic->data;\
			if (icon->cParentDockName == NULL)\
				icon->cParentDockName = g_strdup (myIcon->acName); } } \
	myIcon->pSubDock->icons = pIconsList; \
	myIcon->pSubDock->pFirstDrawnElement = pIconsList; \
	cairo_dock_load_buffers_in_one_dock (myIcon->pSubDock); \
	cairo_dock_update_dock_size (myIcon->pSubDock); } while (0)

/** Delete the list of icons of an applet
*/
#define CD_APPLET_DELETE_MY_ICONS_LIST do {\
	if (myDesklet && myDesklet->icons != NULL) {\
		g_list_foreach (myDesklet->icons, (GFunc) cairo_dock_free_icon, NULL);\
		g_list_free (myDesklet->icons);\
		myDesklet->icons = NULL; }\
	if (myIcon->pSubDock != NULL) {\
		if (myDesklet) {\
			CD_APPLET_DESTROY_MY_SUBDOCK; }\
		else {\
			g_list_foreach (myIcon->pSubDock->icons, (GFunc) cairo_dock_free_icon, NULL);\
			g_list_free (myIcon->pSubDock->icons);\
			myIcon->pSubDock->icons = NULL;\
			myIcon->pSubDock->pFirstDrawnElement = NULL; } } } while (0)

/** Load a list of icons into an applet, with the given renderer for the sub-dock or the desklet).
*/
#define CD_APPLET_LOAD_MY_ICONS_LIST(pIconList, cDockRendererName, cDeskletRendererName, pDeskletRendererConfig) do {\
	if (myDock) {\
		if (myIcon->pSubDock == NULL) {\
			if (pIconList != NULL) {\
				CD_APPLET_CREATE_MY_SUBDOCK (pIconList, cDockRendererName); } }\
		else {\
			if (pIconList == NULL) {\
				CD_APPLET_DESTROY_MY_SUBDOCK; }\
			else {\
				cairo_dock_set_renderer (myIcon->pSubDock, cDockRendererName); \
				CD_APPLET_LOAD_ICONS_IN_MY_SUBDOCK (pIconList); } } }\
	else {\
		if (myIcon->pSubDock != NULL) {\
			CD_APPLET_DESTROY_MY_SUBDOCK; }\
		myDesklet->icons = pIconList;\
		CD_APPLET_SET_DESKLET_RENDERER_WITH_DATA (cDeskletRendererName, pDeskletRendererConfig);\
		CAIRO_DOCK_REDRAW_MY_CONTAINER;\
	} } while (0)

/** Gets the list of icons of your applet. It is either the icons of your sub-dock or of your desklet.
*/
#define CD_APPLET_MY_ICONS_LIST (myDock ? (myIcon->pSubDock ? myIcon->pSubDock->icons : NULL) : myDesklet->icons)
/** Gets the container of the icons of your applet. It is either your sub-dock or your desklet.
*/
#define CD_APPLET_MY_ICONS_LIST_CONTAINER (myDock ? CAIRO_CONTAINER (myIcon->pSubDock) : CAIRO_CONTAINER (myDesklet))


//\_________________________________ TASKBAR
/** Lets your applet control the window of an external program, instead of the Taskbar.
 *\param cApplicationClass the class of the application you wish to control.
 *\param bStealTaskBarIcon TRUE to manage the application, FALSE to stop managing it.
*/
#define CD_APPLET_MANAGE_APPLICATION(cApplicationClass, bStealTaskBarIcon) do {\
	if (myIcon->cClass != NULL && (cairo_dock_strings_differ (myIcon->cClass, cApplicationClass) || ! bStealTaskBarIcon))\
		cairo_dock_deinhibate_class (cApplicationClass, myIcon);\
	if (myIcon->cClass == NULL && bStealTaskBarIcon)\
		cairo_dock_inhibate_class (cApplicationClass, myIcon); } while (0)

//\_________________________________ INTERNATIONNALISATION
/**
*Macro for gettext, similar to _() et N_(), but with the domaine of the applet. Surround all your strings with this, so that 'xgettext' can find them and automatically include them in the translation files.
*/
#define D_(message) dgettext (MY_APPLET_GETTEXT_DOMAIN, message)
#define _D D_

G_END_DECLS
#endif
