
#ifndef __CAIRO_DOCK_GUI_FILTER__
#define  __CAIRO_DOCK_GUI_FILTER__

#include <gtk/gtk.h>
G_BEGIN_DECLS


void cairo_dock_apply_filter_on_group_widget (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther, GSList *pWidgetList);

void cairo_dock_apply_filter_on_group_list (gchar **pKeyWords, gboolean bAllWords, gboolean bSearchInToolTip, gboolean bHighLightText, gboolean bHideOther, GList *pGroupDescriptionList);


G_END_DECLS
#endif
