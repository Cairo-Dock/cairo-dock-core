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


#ifndef __CAIRO_DOCK_GUI_FACTORY__
#define  __CAIRO_DOCK_GUI_FACTORY__

#include <gtk/gtk.h>
G_BEGIN_DECLS


/**
*@file cairo-dock-gui-factory.h This class handles the construction of the common widgets used in the conf files.
* 
* A conf file is a common group/key file, with the following syntax :
* \code
* [Group]
* #comment about key1
* key1 = 1
* #comment about key2
* key2 = pouic
* \endcode
* 
* Each key in the conf file has a comment.
* 
* The first character of the comment defines the type of widget. Known types are listed in the CairoDockGUIWidgetType enum.
* 
* A key can be a behaviour key or an appearance key. Appearance keys are keys that defines the look of the appli, they belong to the theme. Behaviour keys are keys that define some configuration parameters, that depends on the user. To mark a key as an apppearance one, suffix the widget character with a '+'. Thus, keys not marked with a '+' won't be loaded when the user loads a theme, except if he forces it.
* 
* After the widget character and its suffix, some widget accept a list of values. For instance, a spinbutton can have a min and a max limits, a list can have pre-defined elements, etc. Such values are set between '[' and ']' brackets, and separated by ';' inside.
* 
* After that, let a blank to start the widget description. It will appear on the left of the widget; description must be short enough to fit the config panel width.
* 
* You can complete this description with a tooltip. To do that, on a new comment line, add some text between '{' and '}' brackets. Tooltips appear above the widget when you let the mouse over it for ~1 second. They can be as long as you want. Use '\n' to insert new lines inside the tooltip.
* 
*/

#define CAIRO_DOCK_GUI_MARGIN 4

/// Types of widgets that Cairo-Dock can automatically build.
typedef enum {
	/// boolean in a button to tick.
	CAIRO_DOCK_WIDGET_CHECK_BUTTON='b',
	/// boolean in a button to tick, that will control the sensitivity of the next widget.
	CAIRO_DOCK_WIDGET_CHECK_CONTROL_BUTTON='B',
	/// integer in a spin button.
	CAIRO_DOCK_WIDGET_SPIN_INTEGER='i',
	/// integer in an horizontal scale.
	CAIRO_DOCK_WIDGET_HSCALE_INTEGER='I',
	/// pair of integers for dimansion WidthxHeight
	CAIRO_DOCK_WIDGET_SIZE_INTEGER='j',
	/// double in a spin button.
	CAIRO_DOCK_WIDGET_SPIN_DOUBLE='f',
	/// 3 doubles with a color selector (RGB).
	CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGB='c',
	/// 4 doubles with a color selector (RGBA).
	CAIRO_DOCK_WIDGET_COLOR_SELECTOR_RGBA='C',
	/// double in an horizontal scale.
	CAIRO_DOCK_WIDGET_HSCALE_DOUBLE='e',
	
	/// list of views.
	CAIRO_DOCK_WIDGET_VIEW_LIST='n',
	/// list of themes in a combo, with preview and readme.
	CAIRO_DOCK_WIDGET_THEME_LIST='h',
	/// list of available animations.
	CAIRO_DOCK_WIDGET_ANIMATION_LIST='a',
	/// list of available dialog decorators.
	CAIRO_DOCK_WIDGET_DIALOG_DECORATOR_LIST='t',
	/// list of available desklet decorations.
	CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST='O',
	/// same but with the 'default' choice too.
	CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST_WITH_DEFAULT='o',
	/// list of existing docks.
	CAIRO_DOCK_WIDGET_DOCK_LIST='d',
	/// list of icons of a dock.
	CAIRO_DOCK_WIDGET_ICONS_LIST='N',
	/// list of installed icon themes.
	CAIRO_DOCK_WIDGET_ICON_THEME_LIST='w',
	/// list of screens
	CAIRO_DOCK_WIDGET_SCREENS_LIST='r',
	/// a button to jump to another module inside the config panel.
	CAIRO_DOCK_WIDGET_JUMP_TO_MODULE='m',
	/// same but only if the module exists.
	CAIRO_DOCK_WIDGET_JUMP_TO_MODULE_IF_EXISTS='M',
	
	/// a button to launch a specific command.
	CAIRO_DOCK_WIDGET_LAUNCH_COMMAND='Z',
	/// a button to launch a specific command with a condition.
	CAIRO_DOCK_WIDGET_LAUNCH_COMMAND_IF_CONDITION='G',
	
	/// a text entry.
	CAIRO_DOCK_WIDGET_STRING_ENTRY='s',
	/// a text entry with a file selector.
	CAIRO_DOCK_WIDGET_FILE_SELECTOR='S',
	/// a text entry with a file selector, files are filtered to only display images.
	CAIRO_DOCK_WIDGET_IMAGE_SELECTOR='g',
	/// a text entry with a folder selector.
	CAIRO_DOCK_WIDGET_FOLDER_SELECTOR='D',
	/// a text entry with a file selector and a 'play' button, for sound files.
	CAIRO_DOCK_WIDGET_SOUND_SELECTOR='u',
	/// a text entry with a shortkey selector.
	CAIRO_DOCK_WIDGET_SHORTKEY_SELECTOR='k',
	/// a text entry with a class selector.
	CAIRO_DOCK_WIDGET_CLASS_SELECTOR='K',
	/// a text entry, where text is hidden and the result is encrypted in the .conf file.
	CAIRO_DOCK_WIDGET_PASSWORD_ENTRY='p',
	/// a font selector button.
	CAIRO_DOCK_WIDGET_FONT_SELECTOR='P',
	
	/// a text list.
	CAIRO_DOCK_WIDGET_LIST='L',
	/// a combo-entry, that is to say a list where one can add a custom choice.
	CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY='E',
	/// a combo where the number of the line is used for the choice.
	CAIRO_DOCK_WIDGET_NUMBERED_LIST='l',
	/// a combo where the number of the line is used for the choice, and for controlling the sensitivity of the widgets below.
	CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST='y',
	/// a combo where the number of the line is used for the choice, and for controlling the sensitivity of the widgets below; controlled widgets are indicated in the list : {entry;index first widget;nb widgets}.
	CAIRO_DOCK_WIDGET_NUMBERED_CONTROL_LIST_SELECTIVE='Y',
	/// a tree view, where lines are numbered and can be moved up and down.
	CAIRO_DOCK_WIDGET_TREE_VIEW_SORT='T',
	/// a tree view, where lines can be added, removed, and moved up and down.
	CAIRO_DOCK_WIDGET_TREE_VIEW_SORT_AND_MODIFY='U',
	/// a tree view, where lines are numbered and can be selected or not.
	CAIRO_DOCK_WIDGET_TREE_VIEW_MULTI_CHOICE='V',
	
	/// an empty GtkContainer, in case you need to build custom widgets.
	CAIRO_DOCK_WIDGET_EMPTY_WIDGET='_',
	/// an empty GtkContainer, the same but using full available space.
	CAIRO_DOCK_WIDGET_EMPTY_FULL='<',
	/// a simple text label.
	CAIRO_DOCK_WIDGET_TEXT_LABEL='>',
	/// a simple text label.
	CAIRO_DOCK_WIDGET_LINK='W',
	/// a label containing the handbook of the applet.
	CAIRO_DOCK_WIDGET_HANDBOOK='A',
	/// an horizontal separator.
	CAIRO_DOCK_WIDGET_SEPARATOR='v',
	/// a frame. The previous frame will be closed.
	CAIRO_DOCK_WIDGET_FRAME='F',
	/// a frame inside an expander. The previous frame will be closed.
	CAIRO_DOCK_WIDGET_EXPANDER='X',
	CAIRO_DOCK_NB_GUI_WIDGETS
	} CairoDockGUIWidgetType;

#define CAIRO_DOCK_WIDGET_CAIRO_ONLY '*'
#define CAIRO_DOCK_WIDGET_OPENGL_ONLY '&'

/// Model used for combo-box and tree-view. CAIRO_DOCK_MODEL_NAME is the name as displayed in the widget, and CAIRO_DOCK_MODEL_RESULT is the resulting string effectively written in the config file.
typedef enum {
	CAIRO_DOCK_MODEL_NAME = 0,  // displayed name
	CAIRO_DOCK_MODEL_RESULT,  // string that will be used for this line
	CAIRO_DOCK_MODEL_DESCRIPTION_FILE,  // readme file
	CAIRO_DOCK_MODEL_IMAGE,  // preview file
	CAIRO_DOCK_MODEL_ACTIVE,  // whether the line is enabled/disabled (checkbox)
	CAIRO_DOCK_MODEL_ORDER,  // used to sort lines
	CAIRO_DOCK_MODEL_ORDER2,  // used to sort lines
	CAIRO_DOCK_MODEL_ICON,  // icon to be displayed
	CAIRO_DOCK_MODEL_STATE,  // used to give a state to the line
	CAIRO_DOCK_MODEL_SIZE,  // size
	CAIRO_DOCK_MODEL_AUTHOR,  // author
	CAIRO_DOCK_MODEL_NB_COLUMNS
	} CairoDockGUIModelColumns;

/// Definition of a widget corresponding to a given (group;key) pair.
struct _CairoDockGroupKeyWidget {
	gchar *cGroupName;
	gchar *cKeyName;
	GSList *pSubWidgetList;
	gchar *cOriginalConfFilePath;
	GtkWidget *pLabel;
	GtkWidget *pKeyBox;
	};


GtkWidget *cairo_dock_gui_make_preview_box (GtkWidget *pMainWindow, GtkWidget *pOneWidget, gboolean bHorizontalPackaging, int iAddInfoBar, const gchar *cInitialDescription, const gchar *cInitialImage, GPtrArray *pDataGarbage);

void _cairo_dock_set_value_in_pair (GtkSpinButton *pSpinButton, gpointer *data);  // exportee pour pouvoir desactiver la callback.

const gchar *cairo_dock_parse_key_comment (gchar *cKeyComment, char *iElementType, guint *iNbElements, gchar ***pAuthorizedValuesList, gboolean *bAligned, const gchar **cTipString);

GtkWidget *cairo_dock_build_group_widget (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath);

GtkWidget *cairo_dock_build_key_file_widget_full (GKeyFile* pKeyFile, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath, GtkWidget *pCurrentNoteBook);

#define cairo_dock_build_key_file_widget(pKeyFile, cGettextDomain, pMainWindow, pWidgetList, pDataGarbage, cOriginalConfFilePath) cairo_dock_build_key_file_widget_full (pKeyFile, cGettextDomain, pMainWindow, pWidgetList, pDataGarbage, cOriginalConfFilePath, NULL)

GtkWidget *cairo_dock_build_conf_file_widget (const gchar *cConfFilePath, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath);


void cairo_dock_update_keyfile_from_widget_list (GKeyFile *pKeyFile, GSList *pWidgetList);


void cairo_dock_free_generated_widget_list (GSList *pWidgetList);


  ///////////////
 // utilities //
///////////////

void cairo_dock_fill_model_with_themes (GtkListStore *pModel, GHashTable *pThemeTable, const gchar *cHint);

void cairo_dock_fill_combo_with_list (GtkWidget *pCombo, GList *pElementList, const gchar *cActiveElement);  // utile pour les applets.

GtkWidget *cairo_dock_gui_make_tree_view (gboolean bGetActiveOnly);

GtkWidget *cairo_dock_gui_make_combo (gboolean bWithEntry);

void cairo_dock_gui_select_in_combo_full (GtkWidget *pOneWidget, const gchar *cValue, gboolean bIsTheme);
#define cairo_dock_gui_select_in_combo(pOneWidget, cValue) cairo_dock_gui_select_in_combo_full (pOneWidget, cValue, FALSE)

gchar **cairo_dock_gui_get_active_rows_in_tree_view (GtkWidget *pOneWidget, gboolean bSelectedRows, gsize *iNbElements);

/** Get a widget from a list of widgets representing a configuration window.
The widgets represent a pair (group,key) as defined in the config file.
@param pWidgetList list of widgets built from the config file
@param cGroupName name of the group the widget belongs to
@param cKeyName name of the key the widget represents
@return the widget asociated with the (group,key) , or NULL if none is found
*/
CairoDockGroupKeyWidget *cairo_dock_gui_find_group_key_widget_in_list (GSList *pWidgetList, const gchar *cGroupName, const gchar *cKeyName);

#define cairo_dock_gui_get_widgets(pGroupKeyWidget) (pGroupKeyWidget)->pSubWidgetList
#define cairo_dock_gui_get_first_widget(pGroupKeyWidget) ((pGroupKeyWidget)->pSubWidgetList ? (pGroupKeyWidget)->pSubWidgetList->data : NULL)
#define cairo_dock_gui_add_widget(pGroupKeyWidget, pOneWidget) (pGroupKeyWidget)->pSubWidgetList = g_slist_append ((pGroupKeyWidget)->pSubWidgetList, pOneWidget)

/** Add a new item in a menu. Adapted from cairo-dock-menu.h and does the same
 *  as gldi_menu_add_item () / cairo_dock_add_in_menu_with_stock_and_data (),
 *  but does not add the custom styling used for menus belonging to docks.
 * @param pMenu the menu
 * @param cLabel the label, or NULL
 * @param cImage the image path or name, or NULL
 * @param pFunction the callback
 * @param pData the data passed to the callback
 * @return the new menu-entry that has been added.
 */
GtkWidget *cairo_dock_gui_menu_item_add (GtkWidget *pMenu, const gchar *cLabel, const gchar *cImage, GCallback pFunction, gpointer pData);

GtkWidget *_gtk_image_new_from_file (const gchar *cIcon, int iSize);

G_END_DECLS
#endif
