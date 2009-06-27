
#ifndef __CAIRO_DOCK_FILE_MANAGER__
#define  __CAIRO_DOCK_FILE_MANAGER__

#include "cairo-dock-struct.h"
#include "cairo-dock-icons.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-file-manager.h This class handles the integration into the desktop environment, which includes :
* - the VFS (Virtual File System)
* - the various desktop-related tools.
*/

typedef enum {
	CAIRO_DOCK_FILE_MODIFIED=0,
	CAIRO_DOCK_FILE_DELETED,
	CAIRO_DOCK_FILE_CREATED,
	CAIRO_DOCK_NB_EVENT_ON_FILES
	} CairoDockFMEventType;

typedef enum {
	CAIRO_DOCK_FM_SORT_BY_NAME=0,
	CAIRO_DOCK_FM_SORT_BY_DATE,
	CAIRO_DOCK_FM_SORT_BY_SIZE,
	CAIRO_DOCK_FM_SORT_BY_TYPE,
	CAIRO_DOCK_NB_SORT_ON_FILE
	} CairoDockFMSortType;

#define CAIRO_DOCK_FM_VFS_ROOT "_vfsroot_"
#define CAIRO_DOCK_FM_NETWORK "_network_"
#define CAIRO_DOCK_FM_TRASH "_trash_"
#define CAIRO_DOCK_FM_DESKTOP "_desktop_"

typedef void (*CairoDockFMGetFileInfoFunc) (const gchar *cBaseURI, gchar **cName, gchar **cURI, gchar **cIconName, gboolean *bIsDirectory, int *iVolumeID, double *fOrder, CairoDockFMSortType iSortType);
typedef void (*CairoDockFMFilePropertiesFunc) (const gchar *cURI, guint64 *iSize, time_t *iLastModificationTime, gchar **cMimeType, int *iUID, int *iGID, int *iPermissionsMask);
typedef GList * (*CairoDockFMListDirectoryFunc) (const gchar *cURI, CairoDockFMSortType g_fm_iSortType, int iNewIconsType, gboolean bListHiddenFiles, gchar **cFullURI);
typedef void (*CairoDockFMLaunchUriFunc) (const gchar *cURI);

typedef gchar * (*CairoDockFMIsMountedFunc) (const gchar *cURI, gboolean *bIsMounted);
typedef gboolean (*CairoDockFMCanEjectFunc) (const gchar *cURI);
typedef gboolean (*CairoDockFMEjectDriveFunc) (const gchar *cURI);

typedef void (*CairoDockFMMountCallback) (gboolean bMounting, gboolean bSuccess, const gchar *cName, Icon *icon, CairoContainer *pContainer);
typedef void (*CairoDockFMMountFunc) (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer);
typedef void (*CairoDockFMUnmountFunc) (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer);

typedef void (*CairoDockFMMonitorCallback) (CairoDockFMEventType iEventType, const gchar *cURI, gpointer data);
typedef void (*CairoDockFMAddMonitorFunc) (const gchar *cURI, gboolean bDirectory, CairoDockFMMonitorCallback pCallback, gpointer data);
typedef void (*CairoDockFMRemoveMonitorFunc) (const gchar *cURI);

typedef gboolean (*CairoDockFMDeleteFileFunc) (const gchar *cURI);
typedef gboolean (*CairoDockFMRenameFileFunc) (const gchar *cOldURI, const gchar *cNewName);
typedef gboolean (*CairoDockFMMoveFileFunc) (const gchar *cURI, const gchar *cDirectoryURI);

typedef gchar * (*CairoDockFMGetTrashFunc) (const gchar *cNearURI, gchar **cFileInfoPath);
typedef gchar * (*CairoDockFMGetDesktopFunc) (void);
typedef void (*CairoDockFMUserActionFunc) (void);

struct _CairoDockDesktopEnvBackend {
	CairoDockFMGetFileInfoFunc 		get_file_info;
	CairoDockFMFilePropertiesFunc 	get_file_properties;
	CairoDockFMListDirectoryFunc 	list_directory;
	CairoDockFMLaunchUriFunc 		launch_uri;
	CairoDockFMIsMountedFunc 		is_mounted;
	CairoDockFMCanEjectFunc 		can_eject;
	CairoDockFMEjectDriveFunc 		eject;
	CairoDockFMMountFunc 			mount;
	CairoDockFMUnmountFunc 		unmount;
	CairoDockFMAddMonitorFunc 	add_monitor;
	CairoDockFMRemoveMonitorFunc 	remove_monitor;
	CairoDockFMDeleteFileFunc 		delete_file;
	CairoDockFMRenameFileFunc 	rename;
	CairoDockFMMoveFileFunc 		move;
	CairoDockFMGetTrashFunc 		get_trash_path;
	CairoDockFMGetDesktopFunc 	get_desktop_path;
	CairoDockFMUserActionFunc		logout;
	CairoDockFMUserActionFunc		shutdown;
	CairoDockFMUserActionFunc		setup_time;
	CairoDockFMUserActionFunc		show_system_monitor;
};

/** Register a environment backend, overwriting any previous backend.
*/
void cairo_dock_fm_register_vfs_backend (CairoDockDesktopEnvBackend *pVFSBackend);

/** List the content of a directory and turn it into a list of icons.
*/
GList * cairo_dock_fm_list_directory (const gchar *cURI, CairoDockFMSortType g_fm_iSortType, int iNewIconsType, gboolean bListHiddenFiles, gchar **cFullURI);

/** Get the main info to represent a file.
*/
gboolean cairo_dock_fm_get_file_info (const gchar *cBaseURI, gchar **cName, gchar **cURI, gchar **cIconName, gboolean *bIsDirectory, int *iVolumeID, double *fOrder, CairoDockFMSortType iSortType);

/** Get some properties about a file.
*/
gboolean cairo_dock_fm_get_file_properties (const gchar *cURI, guint64 *iSize, time_t *iLastModificationTime, gchar **cMimeType, int *iUID, int *iGID, int *iPermissionsMask);

/** Open a file with the default application.
*/
gboolean cairo_dock_fm_launch_uri (const gchar *cURI);

/** Add a monitor on an URI. It will be called each time a modification occurs on the file.
*/
gboolean cairo_dock_fm_add_monitor_full (const gchar *cURI, gboolean bDirectory, const gchar *cMountedURI, CairoDockFMMonitorCallback pCallback, gpointer data);
/** Add a monitor on an icon representing an URI.
*/
#define cairo_dock_fm_add_monitor(pIcon) cairo_dock_fm_add_monitor_full (pIcon->cBaseURI, (pIcon->pSubDock != NULL), (pIcon->iVolumeID != 0 ? pIcon->acCommand : NULL), (CairoDockFMMonitorCallback) cairo_dock_fm_action_on_file_event, (gpointer) pIcon)

/** Remove a monitor on an URI.
*/
gboolean cairo_dock_fm_remove_monitor_full (const gchar *cURI, gboolean bDirectory, const gchar *cMountedURI);
/** Remove a monitor on an icon representing an URI.
*/
#define cairo_dock_fm_remove_monitor(pIcon) cairo_dock_fm_remove_monitor_full (pIcon->cBaseURI, (pIcon->pSubDock != NULL), (pIcon->iVolumeID != 0 ? pIcon->acCommand : NULL))


/** Mount a point.
*/
gboolean cairo_dock_fm_mount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer);
/** Mount an icon representing an URI.
*/
#define cairo_dock_fm_mount(icon, pContainer) cairo_dock_fm_mount_full (icon->cBaseURI, icon->iVolumeID, cairo_dock_fm_action_after_mounting, icon, pContainer)

/** Unmount a point.
*/
gboolean cairo_dock_fm_unmount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer);
/** Unmount an icon representing an URI.
*/
#define cairo_dock_fm_unmount(icon, pContainer) cairo_dock_fm_unmount_full (icon->cBaseURI, icon->iVolumeID, cairo_dock_fm_action_after_mounting, icon, pContainer)

/** Say if a point is currently mounted.
*/
gchar *cairo_dock_fm_is_mounted (const gchar *cURI, gboolean *bIsMounted);

/** Say if a point can be ejected (like a CD player).
*/
gboolean cairo_dock_fm_can_eject (const gchar *cURI);
/** Eject the point.
*/
gboolean cairo_dock_fm_eject_drive (const gchar *cURI);


/** Delete a file.
*/
gboolean cairo_dock_fm_delete_file (const gchar *cURI);

/** Rename a file.
*/
gboolean cairo_dock_fm_rename_file (const gchar *cOldURI, const gchar *cNewName);

/** Move a file.
*/
gboolean cairo_dock_fm_move_file (const gchar *cURI, const gchar *cDirectoryURI);

/** Get the path to the Trash.
*/
gchar *cairo_dock_fm_get_trash_path (const gchar *cNearURI, gchar **cFileInfoPath);
/** Get the path to the Desktop.
*/
gchar *cairo_dock_fm_get_desktop_path (void);

/** Raise the logout panel.
*/
gboolean cairo_dock_fm_logout (void);
/** Raise the shutdown panel.
*/
gboolean cairo_dock_fm_shutdown (void);

/** Raise the panel to configure the time.
*/
gboolean cairo_dock_fm_setup_time (void);

/** Raise the default system monitor.
*/
gboolean cairo_dock_fm_show_system_monitor (void);

/** Create an Icon representing a given URI.
*/
Icon *cairo_dock_fm_create_icon_from_URI (const gchar *cURI, CairoContainer *pContainer);

/** Create a Dock with the list of icons representing a folder.
*/
void cairo_dock_fm_create_dock_from_directory (Icon *pIcon, CairoDock *pParentDock);


void cairo_dock_fm_manage_event_on_file (CairoDockFMEventType iEventType, const gchar *cURI, Icon *pIcon, CairoDockIconType iTypeOnCreation);
void cairo_dock_fm_action_on_file_event (CairoDockFMEventType iEventType, const gchar *cURI, Icon *pIcon);

void cairo_dock_fm_action_after_mounting (gboolean bMounting, gboolean bSuccess, const gchar *cName, Icon *icon, CairoContainer *pContainer);


gboolean cairo_dock_fm_move_into_directory (const gchar *cURI, Icon *icon, CairoContainer *pContainer);


CairoDockDesktopEnv cairo_dock_guess_environment (void);

/** Get the size of a file.
/*@param cFilePath path of a file on the hard disk.
*@return the size of the file, or 0 if it doesn't exist.
*/
int cairo_dock_get_file_size (const gchar *cFilePath);


G_END_DECLS
#endif
