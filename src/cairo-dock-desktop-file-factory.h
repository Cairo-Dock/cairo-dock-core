
#ifndef __CAIRO_DOCK_DESKTOP_FILE_FACTORY__
#define  __CAIRO_DOCK_DESKTOP_FILE_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS


void cairo_dock_remove_html_spaces (gchar *cString);

gchar *cairo_dock_generate_desktop_file_for_launcher (const gchar *cDesktopURI, const gchar *cDockName, double fOrder, GError **erreur);
gchar *cairo_dock_generate_desktop_file_for_edition (CairoDockNewLauncherType iNewLauncherType, const gchar *cDockName, double fOrder, GError **erreur);
gchar *cairo_dock_generate_desktop_file_for_file (const gchar *cURI, const gchar *cDockName, double fOrder, GError **erreur);

gchar *cairo_dock_add_desktop_file_from_uri_full (const gchar *cURI, const gchar *cDockName, double fOrder, CairoDockNewLauncherType iNewLauncherType, CairoDock *pDock, GError **erreur);
#define cairo_dock_add_desktop_file_from_uri(cURI, cDockName, fOrder, pDock, erreur) cairo_dock_add_desktop_file_from_uri_full (cURI, cDockName, fOrder, CAIRO_DOCK_LAUNCHER_FROM_DESKTOP_FILE, pDock, erreur)
#define cairo_dock_add_desktop_file_for_container(cDockName, fOrder, pDock, erreur) cairo_dock_add_desktop_file_from_uri_full (NULL, cDockName, fOrder, CAIRO_DOCK_LAUNCHER_FOR_CONTAINER, pDock, erreur)
#define cairo_dock_add_desktop_file_for_separator(cDockName, fOrder, pDock, erreur) cairo_dock_add_desktop_file_from_uri_full (NULL, cDockName, fOrder, CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR, pDock, erreur)

gchar *cairo_dock_generate_desktop_filename (gchar *cBaseName, gchar *cCairoDockDataDir);


void cairo_dock_update_launcher_desktop_file (gchar *cDesktopFilePath, CairoDockNewLauncherType iLauncherType);


gchar *cairo_dock_get_launcher_template_conf_file (CairoDockNewLauncherType iNewLauncherType);


G_END_DECLS
#endif

