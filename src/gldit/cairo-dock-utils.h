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

#ifndef __CAIRO_DOCK_UTILS__
#define  __CAIRO_DOCK_UTILS__
G_BEGIN_DECLS

#include <glib.h>
#include <gio/gdesktopappinfo.h>

/**
*@file cairo-dock-utils.h Some helper functions.
*/

gchar *cairo_dock_cut_string (const gchar *cString, int iNbCaracters);;

/** Remove the version number from a string. Directly modifies the string.
 * @param cString a string.
 * @return TRUE if a version has been removed.
 */
gboolean cairo_dock_remove_version_from_string (gchar *cString);

/** Replace the %20 by normal spaces into the string. The string is directly modified.
*@param cString the string (it can't be a constant string)
*/
void cairo_dock_remove_html_spaces (gchar *cString);

/** Get the 3 version numbers of a string.
*@param cVersionString the string of the form "x.y.z".
*@param iMajorVersion pointer to the major version.
*@param iMinorVersion pointer to the minor version.
*@param iMicroVersion pointer to the micro version.
*/
void cairo_dock_get_version_from_string (const gchar *cVersionString, int *iMajorVersion, int *iMinorVersion, int *iMicroVersion);

/** Say if a string is an adress (file://xxx, http://xxx, ftp://xxx, etc).
* @param cString a string.
* @return TRUE if it's an address.
*/
gboolean cairo_dock_string_is_address (const gchar *cString);
#define cairo_dock_string_is_adress cairo_dock_string_is_address

gboolean cairo_dock_string_contains (const char *cNames, const gchar *cName, const gchar *separators);


gchar *cairo_dock_launch_command_argv_sync_with_stderr (const gchar * const * argv, gboolean bPrintStdErr);
gchar *cairo_dock_launch_command_sync_with_stderr (const gchar *cCommand, gboolean bPrintStdErr);
#define cairo_dock_launch_command_sync(cCommand) cairo_dock_launch_command_sync_with_stderr (cCommand, TRUE)


/** Flags given to cairo_dock_launch_command_argv_full() */
typedef enum {
	GLDI_LAUNCH_DEFAULT = 0,
	/// This is a GUI app, use GdkAppLaunchContext to create an activation token for it
	GLDI_LAUNCH_GUI = 1<<0,
	/// This is a potentially long-lived app, try to put it in a separate process accounting
	/// group with the session manager. Currently, this is only supported on systemd and means
	/// starting the app as a separate, transient service. This has the effect that e.g. resource
	/// use is accounted separately, and the app is not automatically killed if cairo-dock exits.
	GLDI_LAUNCH_SLICE = 1<<1
} GldiLaunchFlags;

gboolean cairo_dock_launch_command_full (const gchar *cCommand, const gchar *cWorkingDirectory, GldiLaunchFlags flags);
#define cairo_dock_launch_command(cCommand) cairo_dock_launch_command_full (cCommand, NULL, GLDI_LAUNCH_DEFAULT)

/** Launch the given command asynchronously (i.e. do not wait for it to exit).
* @param args Argument vector with the executable name and parameters to pass.
* @param cWorkingDirectory working directory to launch the command in.
* @param flags Additional options that determine how to launch the command.
* @return Whether successfully launched the given command. Note: currently, this always returns TRUE if flags includes GLDI_LAUNCH_SLICE;
*  do not rely on its value in this case.
*/
gboolean cairo_dock_launch_command_argv_full (const gchar * const * args, const gchar *cWorkingDirectory, GldiLaunchFlags flags);

/** Launch the given command asynchronously (i.e. do not wait for it to exit).
* @param args Argument vector with the executable name and parameters to pass.
* @param cWorkingDirectory working directory to launch the command in.
* @param flags Additional options that determine how to launch the command.
* @param app_info An optional GAppInfo that corresponds to the app to be launched. Used to generate a startup notify ID
*  (if flags includes GLDI_LAUNCH_GUI) and to identify the app to the system service manager (if flags includes GLDI_LAUNCH_SLICE).
* @return Whether successfully launched the given command. Note: currently, this always returns TRUE if flags includes
*  GLDI_LAUNCH_SLICE; do not rely on its value in this case.
*/
gboolean cairo_dock_launch_command_argv_full2 (const gchar * const * args, const gchar *cWorkingDirectory, GldiLaunchFlags flags,
	GAppInfo *app_info);

#define cairo_dock_launch_command_argv(argv) cairo_dock_launch_command_argv_full (argv, NULL, GLDI_LAUNCH_DEFAULT)
gboolean cairo_dock_launch_command_single (const gchar *cExec);
gboolean cairo_dock_launch_command_single_gui (const gchar *cExec);

/** Get the command to launch the default terminal
 */
const gchar * cairo_dock_get_default_terminal (void);

/* Like g_strcmp0, but saves a function call.
*/
#define gldi_strings_differ(s1, s2) (!s1 ? s2 != NULL : !s2 ? s1 != NULL : strcmp(s1, s2) != 0)
#define cairo_dock_strings_differ gldi_strings_differ


/** Simple "backend" for managing processes launched by us. Mainly needed to put
 * newly launched apps in their own systemd scope / cgroup. */
struct _GldiChildProcessManagerBackend {
	/** Handle a newly launched child process, performing any system-specific setup functions.
	 * This should also eventually call waitpid() or similar to clean up the child process.
	 *
	 *@param id  a non-NULL identifier for the newly launched process, containing only "safe" characters
	 *           (currently this means only characters valid in systemd unit names: ASCII letters, digits, ":", "-", "_", ".", and "\");
	 * 			 does not need to be unique
	 *@param desc  a description suitable to display to the user
	 *@param pid  process id of the newly launched process; the caller should not use waitpid()
	 *            or similar facility to avoid race condition
	 */
	void (*new_app_launched) (const char *id, const char *desc, GPid pid);
	/** Launch a new app based on the given argument vector, performing any system-specific setup necessary.
	 *
	 *@param args  argument vector to use
	 *@param id  a non-NULL identifier for the newly launched process, containing only "safe" characters
	 *           (currently this means only characters valid in systemd unit names: ASCII letters, digits,
	 *            ":", "-", "_", ".", and "\"); does not need to be unique
	 *@param desc  a non-NULL description suitable to display to the user
	 *@param env  an optional vector of environment variables to set for the launched process; each element
	 *            should be in the format of "VAR=value"
	 *@param working_dir  optionally a working directory to set for the newly launched process
	 */
	void (*spawn_app) (const gchar * const *args, const gchar *id, const gchar *desc, const gchar * const *env, const gchar *working_dir);
};
typedef struct _GldiChildProcessManagerBackend GldiChildProcessManagerBackend;

void gldi_register_process_manager_backend (GldiChildProcessManagerBackend *backend);

#include "gldi-config.h"
#ifdef HAVE_X11

gboolean cairo_dock_property_is_present_on_root (const gchar *cPropertyName);  // env-manager

gboolean cairo_dock_check_xrandr (int major, int minor);  // showDesktop

#endif

G_END_DECLS
#endif

