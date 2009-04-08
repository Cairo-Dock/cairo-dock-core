
#ifndef __CAIRO_DOCK_FILE_MANAGER__
#define  __CAIRO_DOCK_FILE_MANAGER__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-file-manager.h This class handles the integration into the desktop environment, which includes :
*  the VFS (Virtual File System)
*  the various desktop-related tools.
*/

void cairo_dock_fm_register_vfs_backend (CairoDockVFSBackend *pVFSBackend);


GList * cairo_dock_fm_list_directory (const gchar *cURI, CairoDockFMSortType g_fm_iSortType, int iNewIconsType, gboolean bListHiddenFiles, gchar **cFullURI);

gboolean cairo_dock_fm_get_file_info (const gchar *cBaseURI, gchar **cName, gchar **cURI, gchar **cIconName, gboolean *bIsDirectory, int *iVolumeID, double *fOrder, CairoDockFMSortType iSortType);

gboolean cairo_dock_fm_get_file_properties (const gchar *cURI, guint64 *iSize, time_t *iLastModificationTime, gchar **cMimeType, int *iUID, int *iGID, int *iPermissionsMask);

gboolean cairo_dock_fm_launch_uri (const gchar *cURI);

gboolean cairo_dock_fm_add_monitor_full (const gchar *cURI, gboolean bDirectory, const gchar *cMountedURI, CairoDockFMMonitorCallback pCallback, gpointer data);
#define cairo_dock_fm_add_monitor(pIcon) cairo_dock_fm_add_monitor_full (pIcon->cBaseURI, (pIcon->pSubDock != NULL), (pIcon->iVolumeID != 0 ? pIcon->acCommand : NULL), (CairoDockFMMonitorCallback) cairo_dock_fm_action_on_file_event, (gpointer) pIcon)

gboolean cairo_dock_fm_remove_monitor_full (const gchar *cURI, gboolean bDirectory, const gchar *cMountedURI);
#define cairo_dock_fm_remove_monitor(pIcon) cairo_dock_fm_remove_monitor_full (pIcon->cBaseURI, (pIcon->pSubDock != NULL), (pIcon->iVolumeID != 0 ? pIcon->acCommand : NULL))

gboolean cairo_dock_fm_move_file (const gchar *cURI, const gchar *cDirectoryURI);


gboolean cairo_dock_fm_mount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer);
#define cairo_dock_fm_mount(icon, pContainer) cairo_dock_fm_mount_full (icon->acCommand, icon->iVolumeID, cairo_dock_fm_action_after_mounting, icon, pContainer)

gboolean cairo_dock_fm_unmount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer);
#define cairo_dock_fm_unmount(icon, pContainer) cairo_dock_fm_unmount_full (icon->acCommand, icon->iVolumeID, cairo_dock_fm_action_after_mounting, icon, pContainer)

gchar *cairo_dock_fm_is_mounted (const gchar *cURI, gboolean *bIsMounted);

gboolean cairo_dock_fm_can_eject (const gchar *cURI);
gboolean cairo_dock_fm_eject_drive (const gchar *cURI);


gboolean cairo_dock_fm_delete_file (const gchar *cURI);

gboolean cairo_dock_fm_rename_file (const gchar *cOldURI, const gchar *cNewName);

gboolean cairo_dock_fm_move_file (const gchar *cURI, const gchar *cDirectoryURI);

gchar *cairo_dock_fm_get_trash_path (const gchar *cNearURI, gchar **cFileInfoPath);
gchar *cairo_dock_fm_get_desktop_path (void);
gboolean cairo_dock_fm_logout (void);
gboolean cairo_dock_fm_shutdown (void);
gboolean cairo_dock_fm_setup_time (void);
gboolean cairo_dock_fm_show_system_monitor (void);


Icon *cairo_dock_fm_create_icon_from_URI (const gchar *cURI, CairoContainer *pContainer);

void cairo_dock_fm_create_dock_from_directory (Icon *pIcon, CairoDock *pParentDock);


void cairo_dock_fm_manage_event_on_file (CairoDockFMEventType iEventType, const gchar *cURI, Icon *pIcon, CairoDockIconType iTypeOnCreation);
void cairo_dock_fm_action_on_file_event (CairoDockFMEventType iEventType, const gchar *cURI, Icon *pIcon);

void cairo_dock_fm_action_after_mounting (gboolean bMounting, gboolean bSuccess, const gchar *cName, Icon *icon, CairoContainer *pContainer);


gboolean cairo_dock_fm_move_into_directory (const gchar *cURI, Icon *icon, CairoContainer *pContainer);


G_END_DECLS
#endif
