
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


/// Types of widgets that Cairo-Dock can automatically build.
typedef enum {
	/// boolean in a button to tick.
	CAIRO_DOCK_WIDGET_CHECK_BUTTON='b',
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
	/// same but with a combo-entry to let the user enter any text.
	CAIRO_DOCK_WIDGET_THEME_LIST_ENTRY='H',
	/// list of user dock themes, with a check button to select several themes.
	CAIRO_DOCK_WIDGET_USER_THEME_SELECTOR='x',
	/// list of dock themes, sortable by name, rating, and sobriety.
	CAIRO_DOCK_WIDGET_THEME_SELECTOR='R',
	/// list of available animations.
	CAIRO_DOCK_WIDGET_ANIMATION_LIST='a',
	/// list of available dialog decorators.
	CAIRO_DOCK_WIDGET_DIALOG_DECORATOR_LIST='t',
	/// list of available desklet decorations.
	CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST='O',
	/// same but with the 'default' choice too.
	CAIRO_DOCK_WIDGET_DESKLET_DECORATION_LIST_WITH_DEFAULT='o',
	/// list of gauges themes.
	CAIRO_DOCK_WIDGET_GAUGE_LIST='g',
	/// list of existing docks.
	CAIRO_DOCK_WIDGET_DOCK_LIST='d',
	/// a button to jump to another module inside the config panel.
	CAIRO_DOCK_WIDGET_JUMP_TO_MODULE='m',
	/// same but only if the module exists.
	CAIRO_DOCK_WIDGET_JUMP_TO_MODULE_IF_EXISTS='M',
	
	/// a text entry.
	CAIRO_DOCK_WIDGET_STRING_ENTRY='s',
	/// a text entry with a file selector.
	CAIRO_DOCK_WIDGET_FILE_SELECTOR='S',
	/// a text entry with a folder selector.
	CAIRO_DOCK_WIDGET_FOLDER_SELECTOR='D',
	/// a text entry with a file selector and a 'play' button, for sound files.
	CAIRO_DOCK_WIDGET_SOUND_SELECTOR='u',
	/// a text entry with a font selector.
	CAIRO_DOCK_WIDGET_FONT_SELECTOR='P',
	/// a text entry with a shortkey selector.
	CAIRO_DOCK_WIDGET_SHORTKEY_SELECTOR='k',
	/// a text entry, where text is hidden and the result is encrypted in the .conf file.
	CAIRO_DOCK_WIDGET_PASSWORD_ENTRY='p',
	
	/// a text list.
	CAIRO_DOCK_WIDGET_LIST='L',
	/// a combo-entry, that is to say a list where one can add a custom choice.
	CAIRO_DOCK_WIDGET_LIST_WITH_ENTRY='E',
	/// a combo where the number of the line is used for the choice.
	CAIRO_DOCK_WIDGET_NUMBERED_LIST='l',
	/// a tree view, where lines are numbered and can be moved up and down.
	CAIRO_DOCK_WIDGET_TREE_VIEW_SORT='T',
	/// a tree view, where lines can be added, removed, and moved up and down.
	CAIRO_DOCK_WIDGET_TREE_VIEW_SORT_AND_MODIFY='U',
	/// a tree view, where lines are numbered and can be selected or not.
	CAIRO_DOCK_WIDGET_TREE_VIEW_MULTI_CHOICE='V',
	
	/// an empty GtkContainer, to use by applets that want to build custom widgets.
	CAIRO_DOCK_WIDGET_EMPTY_WIDGET='_',
	/// a simple text label.
	CAIRO_DOCK_WIDGET_TEXT_LABEL='>',
	/// an horizontal separator.
	CAIRO_DOCK_WIDGET_SEPARATOR='v',
	/// a frame. The previous frame will be closed.
	CAIRO_DOCK_WIDGET_FRAME='F',
	/// a frame inside an expander. The previous frame will be closed.
	CAIRO_DOCK_WIDGET_EXPANDER='X',
	CAIRO_DOCK_NB_GUI_WIDGETS
	} CairoDockGUIWidgetType;
	

void cairo_dock_build_renderer_list_for_gui (GHashTable *pHashTable);
void cairo_dock_build_desklet_decorations_list_for_gui (GHashTable *pHashTable);
void cairo_dock_build_desklet_decorations_list_for_applet_gui (GHashTable *pHashTable);
void cairo_dock_build_animations_list_for_gui (GHashTable *pHashTable);
void cairo_dock_build_dialog_decorator_list_for_gui (GHashTable *pHashTable);


gchar *cairo_dock_parse_key_comment (gchar *cKeyComment, char *iElementType, guint *iNbElements, gchar ***pAuthorizedValuesList, gboolean *bAligned, gchar **cTipString);

GtkWidget *cairo_dock_build_group_widget (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath);

GtkWidget *cairo_dock_build_key_file_widget (GKeyFile* pKeyFile, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath);
GtkWidget *cairo_dock_build_conf_file_widget (const gchar *cConfFilePath, const gchar *cGettextDomain, GtkWidget *pMainWindow, GSList **pWidgetList, GPtrArray *pDataGarbage, const gchar *cOriginalConfFilePath);


void cairo_dock_update_keyfile_from_widget_list (GKeyFile *pKeyFile, GSList *pWidgetList);


void cairo_dock_free_generated_widget_list (GSList *pWidgetList);


GSList *cairo_dock_find_widgets_from_name (GSList *pWidgetList, const gchar *cGroupName, const gchar *cKeyName);
GtkWidget *cairo_dock_find_widget_from_name (GSList *pWidgetList, const gchar *cGroupName, const gchar *cKeyName);

void cairo_dock_fill_combo_with_themes (GtkWidget *pCombo, GHashTable *pThemeTable, gchar *cActiveTheme);
void cairo_dock_fill_combo_with_list (GtkWidget *pCombo, GList *pElementList, const gchar *cActiveElement);


G_END_DECLS
#endif
