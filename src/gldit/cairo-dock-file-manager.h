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

#ifndef __CAIRO_DOCK_FILE_MANAGER__
#define  __CAIRO_DOCK_FILE_MANAGER__

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-file-manager.h This class manages the integration into the desktop environment, which includes :
* - the VFS (Virtual File System)
* - the various desktop-related tools.
*/

// manager
#ifndef _MANAGER_DEF_
extern GldiManager myDesktopEnvMgr;
#endif


/// Type of available Desktop Environments.
typedef enum {
	CAIRO_DOCK_UNKNOWN_ENV=0,
	CAIRO_DOCK_GNOME,
	CAIRO_DOCK_KDE,
	CAIRO_DOCK_XFCE,
	CAIRO_DOCK_NB_DESKTOPS
	} CairoDockDesktopEnv;

/// Type of events that can occur to a file.
typedef enum {
	CAIRO_DOCK_FILE_MODIFIED=0,
	CAIRO_DOCK_FILE_DELETED,
	CAIRO_DOCK_FILE_CREATED,
	CAIRO_DOCK_NB_EVENT_ON_FILES
	} CairoDockFMEventType;

/// Type of sorting available on files.
typedef enum {
	CAIRO_DOCK_FM_SORT_BY_NAME=0,
	CAIRO_DOCK_FM_SORT_BY_DATE,
	CAIRO_DOCK_FM_SORT_BY_SIZE,
	CAIRO_DOCK_FM_SORT_BY_TYPE,
	CAIRO_DOCK_FM_SORT_BY_ACCESS,
	CAIRO_DOCK_NB_SORT_ON_FILE
	} CairoDockFMSortType;

#define CAIRO_DOCK_FM_VFS_ROOT "_vfsroot_"
#define CAIRO_DOCK_FM_NETWORK "_network_"
#define CAIRO_DOCK_FM_TRASH "_trash_"
#define CAIRO_DOCK_FM_DESKTOP "_desktop_"

typedef void (*CairoDockFMGetFileInfoFunc) (const gchar *cBaseURI, gchar **cName, gchar **cURI, gchar **cIconName, gboolean *bIsDirectory, int *iVolumeID, double *fOrder, CairoDockFMSortType iSortType);
typedef void (*CairoDockFMFilePropertiesFunc) (const gchar *cURI, guint64 *iSize, time_t *iLastModificationTime, gchar **cMimeType, int *iUID, int *iGID, int *iPermissionsMask);
typedef GList * (*CairoDockFMListDirectoryFunc) (const gchar *cURI, CairoDockFMSortType g_fm_iSortType, int iNewIconsType, gboolean bListHiddenFiles, int iNbMaxFiles, gchar **cFullURI);
typedef gsize (*CairoDockFMMeasureDirectoryFunc) (const gchar *cURI, gint iCountType, gboolean bRecursive, gint *pCancel);
typedef void (*CairoDockFMLaunchUriFunc) (const gchar *cURI);

typedef gchar * (*CairoDockFMIsMountedFunc) (const gchar *cURI, gboolean *bIsMounted);
typedef gboolean (*CairoDockFMCanEjectFunc) (const gchar *cURI);
typedef gboolean (*CairoDockFMEjectDriveFunc) (const gchar *cURI);

typedef void (*CairoDockFMMountCallback) (gboolean bMounting, gboolean bSuccess, const gchar *cName, const gchar *cUri, gpointer data);
typedef void (*CairoDockFMMountFunc) (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, gpointer user_data);
typedef void (*CairoDockFMUnmountFunc) (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, gpointer user_data);

typedef void (*CairoDockFMMonitorCallback) (CairoDockFMEventType iEventType, const gchar *cURI, gpointer data);
typedef void (*CairoDockFMAddMonitorFunc) (const gchar *cURI, gboolean bDirectory, CairoDockFMMonitorCallback pCallback, gpointer data);
typedef void (*CairoDockFMRemoveMonitorFunc) (const gchar *cURI);

typedef gboolean (*CairoDockFMDeleteFileFunc) (const gchar *cURI, gboolean bNoTrash);
typedef gboolean (*CairoDockFMRenameFileFunc) (const gchar *cOldURI, const gchar *cNewName);
typedef gboolean (*CairoDockFMMoveFileFunc) (const gchar *cURI, const gchar *cDirectoryURI);
typedef gboolean (*CairoDockFMCreateFileFunc) (const gchar *cURI, gboolean bDirectory);
typedef GList * (*CairoDockFMListAppsForFileFunc) (const gchar *cURI);

typedef gchar * (*CairoDockFMGetTrashFunc) (const gchar *cNearURI, gchar **cFileInfoPath);
typedef void (*CairoDockFMEmptyTrashFunc) (void);
typedef gchar * (*CairoDockFMGetDesktopFunc) (void);
typedef void (*CairoDockFMUserActionFunc) (void);

/// Definition of the Desktop Environment backend.
struct _CairoDockDesktopEnvBackend {
	CairoDockFMGetFileInfoFunc 		get_file_info;
	CairoDockFMFilePropertiesFunc 	get_file_properties;
	CairoDockFMListDirectoryFunc 	list_directory;
	CairoDockFMMeasureDirectoryFunc measure_directory;
	CairoDockFMLaunchUriFunc 		launch_uri;
	CairoDockFMIsMountedFunc 		is_mounted;
	CairoDockFMCanEjectFunc 		can_eject;
	CairoDockFMEjectDriveFunc 		eject;
	CairoDockFMMountFunc 			mount;
	CairoDockFMUnmountFunc 			unmount;
	CairoDockFMAddMonitorFunc 		add_monitor;
	CairoDockFMRemoveMonitorFunc 	remove_monitor;
	CairoDockFMDeleteFileFunc 		delete_file;
	CairoDockFMRenameFileFunc 		rename;
	CairoDockFMMoveFileFunc 		move;
	CairoDockFMCreateFileFunc 		create;
	CairoDockFMListAppsForFileFunc 	list_apps_for_file;
	CairoDockFMEmptyTrashFunc		empty_trash;
	CairoDockFMGetTrashFunc 		get_trash_path;
	CairoDockFMGetDesktopFunc 		get_desktop_path;
	CairoDockFMUserActionFunc		logout;
	CairoDockFMUserActionFunc		lock_screen;
	CairoDockFMUserActionFunc		shutdown;
	CairoDockFMUserActionFunc		reboot;
	CairoDockFMUserActionFunc		setup_time;
	CairoDockFMUserActionFunc		show_system_monitor;
};


/** Register a environment backend, overwriting any previous backend.
*/
void cairo_dock_fm_register_vfs_backend (CairoDockDesktopEnvBackend *pVFSBackend);

gboolean cairo_dock_fm_vfs_backend_is_defined (void);

void cairo_dock_fm_force_desktop_env (CairoDockDesktopEnv iForceDesktopEnv);

/** List the content of a directory and turn it into a list of icons.
*/
GList * cairo_dock_fm_list_directory (const gchar *cURI, CairoDockFMSortType g_fm_iSortType, int iNewIconsType, gboolean bListHiddenFiles, int iNbMaxFiles, gchar **cFullURI);

/** Measure a directory (number of files or total size).
*/
gsize cairo_dock_fm_measure_diretory (const gchar *cBaseURI, gint iCountType, gboolean bRecursive, gint *pCancel);

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

/** Remove a monitor on an URI.
*/
gboolean cairo_dock_fm_remove_monitor_full (const gchar *cURI, gboolean bDirectory, const gchar *cMountedURI);

/** Mount a point.
*/
gboolean cairo_dock_fm_mount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, gpointer user_data);

/** Unmount a point.
*/
gboolean cairo_dock_fm_unmount_full (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, gpointer user_data);

/** Say if a point is currently mounted.
*/
gchar *cairo_dock_fm_is_mounted (const gchar *cURI, gboolean *bIsMounted);

/** Say if a point can be ejected (like a CD player).
*/
gboolean cairo_dock_fm_can_eject (const gchar *cURI);

/** Eject a drive, like a CD player.
*/
gboolean cairo_dock_fm_eject_drive (const gchar *cURI);


/** Delete a file.
*/
gboolean cairo_dock_fm_delete_file (const gchar *cURI, gboolean bNoTrash);

/** Rename a file.
*/
gboolean cairo_dock_fm_rename_file (const gchar *cOldURI, const gchar *cNewName);

/** Move a file.
*/
gboolean cairo_dock_fm_move_file (const gchar *cURI, const gchar *cDirectoryURI);

/** Create a new file.
*/
gboolean cairo_dock_fm_create_file (const gchar *cURI, gboolean bDirectory);

/** Get the list of applications that can open a given file. Returns a list of GAppInfo
*/
GList *cairo_dock_fm_list_apps_for_file (const gchar *cURI);

/** Empty the Trash.
*/
gboolean cairo_dock_fm_empty_trash (void);

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
/** Raise the reboot panel.
*/
gboolean cairo_dock_fm_reboot (void);

/** Lock the screen.
*/
gboolean cairo_dock_fm_lock_screen (void);

gboolean cairo_dock_fm_can_setup_time (void);

/** Raise the panel to configure the time.
*/
gboolean cairo_dock_fm_setup_time (void);

/** Raise the default system monitor.
*/
gboolean cairo_dock_fm_show_system_monitor (void);

/** Create an Icon representing a given URI.
*/
Icon *cairo_dock_fm_create_icon_from_URI (const gchar *cURI, GldiContainer *pContainer, CairoDockFMSortType iFileSortType);


gboolean cairo_dock_fm_move_into_directory (const gchar *cURI, Icon *icon, GldiContainer *pContainer);


/** Get the size of a local file.
*@param cFilePath path of a file on the hard disk.
*@return the size of the file, or 0 if it doesn't exist.
*/
int cairo_dock_get_file_size (const gchar *cFilePath);

gboolean cairo_dock_copy_file (const gchar *cFilePath, const gchar *cDestPath);


/** Get process ID given its name
 * @param cProcessName name of the process
 * @return the PID if it exists or -1
 */
int cairo_dock_fm_get_pid (const gchar *cProcessName);

/** Monitor a process. Call a function when the process is no longer running
 * @param cProcessName name(es) of the process(es)
 * @param bCheckSameProcess TRUE to check if first match is running. FALSE to
 *        check every time if this process name is running even if it's not the
 *        same PID.
 * @param pCallback function to call when the process is no longer running
 * @param bAlwaysLaunch TRUE to launch the callback function even if the process
 *        is not running or if there is an error
 * @param pUserData data to pass to pCallback
 * @return FALSE if the process is not running or if there is an error
 */
gboolean cairo_dock_fm_monitor_pid (const gchar *cProcessName, gboolean bCheckSameProcess, GSourceFunc pCallback, gboolean bAlwaysLaunch, gpointer pUserData);

const gchar *cairo_dock_fm_get_desktop_name (void);

void gldi_register_desktop_environment_manager (void);

typedef void (*CairoDockFMOpenedWithCallback) (gpointer ptr);
/** Create a submenu for presenting options to open a file and add it to the given menu.
 * @param pAppList a list with GAppInfo elements to add to the submenu (e.g. the return
 *        value of cairo_dock_fm_list_apps_for_file ()). Owned by the caller, but apps
 *        added to the menu will be refed, so can be freed.
 * @param cPath path of the file to open
 * @param pMenu menu to add a submenu to
 * @param cLabel label of the submenu item
 * @param cImage stock image to use with the label
 * @param pCallback an optional callback function to call when the app was launched (can be NULL)
 * @param user_data data to pass to pCallback
 * @return TRUE if the submenu was successfully created and added to pMenu
 * 
 * Note: the created submenu is managed internally and will be freed when the menu is destroyed. If given,
 * user_data should remain valid while the menu is open. The callback function is only called if the app
 * is launched, so it is possible that it is never called.
 */
gboolean cairo_dock_fm_add_open_with_submenu (GList *pAppList, const gchar *cPath, GtkWidget *pMenu, const gchar *cLabel,
	const gchar *cImage, CairoDockFMOpenedWithCallback pCallback, gpointer user_data);

G_END_DECLS
#endif
