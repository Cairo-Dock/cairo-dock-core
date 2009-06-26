
#ifndef __CAIRO_DOCK_LAUNCHER_FACTORY__
#define  __CAIRO_DOCK_LAUNCHER_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-launcher-factory.h This class handles the creation of launcher icons, from the desktop files contained into the 'launchers' floder.
*/

/** Search the path of an icon into the defined folders/icons themes. It handles the '~' caracter.
 * @param cFileName name of the icon file.
 */
gchar *cairo_dock_search_icon_s_path (const gchar *cFileName);


/** Read a desktop file and fetch all its data into an Icon.
 * @param cDesktopFileName name or path of a desktop file. If it's a simple name, it will be taken in the "launchers" folder of the current theme.
 * @param icon the Icon to fill.
*/
void cairo_dock_load_icon_info_from_desktop_file (const gchar *cDesktopFileName, Icon *icon);


/** Create an Icon from a given desktop file, and fill its buffers. The resulting icon can directly be used inside a container. Class inhibating is handled.
*/
Icon * cairo_dock_create_icon_from_desktop_file (const gchar *cDesktopFileName, cairo_t *pSourceContext);

/** Reload an Icon from a given desktop file, and fill its buffers. It doesn't handle the side-effects like modifying the class, the sub-dock's view, the container, etc.
*/
void cairo_dock_reload_icon_from_desktop_file (const gchar *cDesktopFileName, cairo_t *pSourceContext, Icon *icon);


G_END_DECLS
#endif
