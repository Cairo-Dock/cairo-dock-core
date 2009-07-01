
#ifndef __CAIRO_DOCK_GUI_FACTORY__
#define  __CAIRO_DOCK_GUI_FACTORY__

#include <gtk/gtk.h>
G_BEGIN_DECLS


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
	/// double  in a spin button.
	CAIRO_DOCK_WIDGET_SPIN_DOUBLE='f',
	/// 3 or 4 doubles with a color selector.
	CAIRO_DOCK_WIDGET_COLOR_SELECTOR='c',
	/// double in an horizontal scale.
	CAIRO_DOCK_WIDGET_HSCALE_DOUBLE='e',
	/// list of views.
	CAIRO_DOCK_WIDGET_VIEW_COMBO='n',
	/// list of themes in a combo, with preview and readme.
	CAIRO_DOCK_WIDGET_THEME_COMBO='h',
	/// same but with a combo-entry to let the user enter any text.
	CAIRO_DOCK_WIDGET_THEME_COMBO_ENTRY='H',
	/// list of user dock themes, with a check button to select several themes.
	CAIRO_DOCK_WIDGET_USER_THEME_TREE_VIEW='x',
	/// list of available animations.
	CAIRO_DOCK_WIDGET_ANIMATION_COMBO='a',
	/// list of available dialog decorators.
	CAIRO_DOCK_WIDGET_DIALOG_DECORATOR_COMBO='t',
	/// list of available desklet decorations.
	CAIRO_DOCK_WIDGET_DESKLET_DECORATION_COMBO='O',
	/// same but with the 'default' choice too.
	CAIRO_DOCK_WIDGET_DESKLET_DECORATION_COMBO_WITH_DEFAULT='o',
	/// list of gauges themes.
	CAIRO_DOCK_WIDGET_GAUGE_COMBO='g',
	/// list of existing docks.
	CAIRO_DOCK_WIDGET_DOCK_COMBO='d',
	/// a button to jump to another module inside the config panel.
	CAIRO_DOCK_WIDGET_JUMP_TO_MODULE='m',
	/// same but only if the module exists.
	CAIRO_DOCK_WIDGET_JUMP_TO_MODULE_IF_EXISTS='M',
	/// an empty GtkContainer, to use by applets that want to build custom widgets.
	CAIRO_DOCK_WIDGET_EMPTY_WIDGET='_',
	/// a simple text label.
	CAIRO_DOCK_WIDGET_TEXT_LABEL='>',
	/// a text entry or combo.
	CAIRO_DOCK_WIDGET_STRING_ENTRY_OR_COMBO='s',
	/// a text entry with a file selector.
	CAIRO_DOCK_WIDGET_FILE_SELECTOR='S',
	/// a text entry with a file selector and a 'play' button, for sound files.
	CAIRO_DOCK_WIDGET_SOUND_SELECTOR='u',
	/// a text entry with a folder selector.
	CAIRO_DOCK_WIDGET_FOLDER_SELECTOR='D',
	/// a tree view, where lines can be added, removed, and moved up and down.
	CAIRO_DOCK_WIDGET_TREE_VIEW_CONSTANT='T',
	/// a combo-entry, that is to say a list where one can add a custom choice.
	CAIRO_DOCK_WIDGET_COMBO_ENTRY='E',
	/// a como with a preview and a readme.
	CAIRO_DOCK_WIDGET_COMBO_WITH_README_AND_PREVIEW='R',
	/// a text entry with a font selector.
	CAIRO_DOCK_WIDGET_FONT_SELECTOR='P',
	/// a combo where the number of the line is used for the choice.
	CAIRO_DOCK_WIDGET_NUMBERED_COMBO='r',
	/// a text entry with a shortkey selector.
	CAIRO_DOCK_WIDGET_SHORTKEY_SELECTOR='k',
	/// a text entry, where text is hidden and the result is encrypted in the .conf file.
	CAIRO_DOCK_WIDGET_PASSWORD_ENTRY='p',
	/// a frame. can't be embedded into another frame.
	CAIRO_DOCK_WIDGET_FRAME='F',
	/// a frame inside an expander. can't be embedded into another frame.
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
