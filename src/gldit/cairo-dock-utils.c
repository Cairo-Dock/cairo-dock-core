/**
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

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <cairo-dock-log.h>
#include <cairo-dock-file-manager.h>  // g_iDesktopEnv
#include <cairo-dock-utils.h>

extern CairoDockDesktopEnv g_iDesktopEnv;

gchar *cairo_dock_cut_string (const gchar *cString, int iNbCaracters)  // gere l'UTF-8
{
	g_return_val_if_fail (cString != NULL, NULL);
	gchar *cTruncatedName = NULL;
	gsize bytes_read, bytes_written;
	GError *erreur = NULL;
	gchar *cUtf8Name = g_locale_to_utf8 (cString,
		-1,
		&bytes_read,
		&bytes_written,
		&erreur);  // inutile sur Ubuntu, qui est nativement UTF8, mais sur les autres on ne sait pas.
	if (erreur != NULL)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		erreur = NULL;
	}
	if (cUtf8Name == NULL)  // une erreur s'est produite, on tente avec la chaine brute.
		cUtf8Name = g_strdup (cString);
	
	const gchar *cEndValidChain = NULL;
	int iStringLength;
	if (g_utf8_validate (cUtf8Name, -1, &cEndValidChain))
	{
		iStringLength = g_utf8_strlen (cUtf8Name, -1);
		int iNbChars = -1;
		if (iNbCaracters < 0)
		{
			iNbChars = MAX (0, iStringLength + iNbCaracters);
		}
		else if (iStringLength > iNbCaracters)
		{
			iNbChars = iNbCaracters;
		}
		
		if (iNbChars != -1)
		{
			cTruncatedName = g_new0 (gchar, 8 * (iNbChars + 4));  // 8 octets par caractere.
			if (iNbChars != 0)
				g_utf8_strncpy (cTruncatedName, cUtf8Name, iNbChars);
			
			gchar *cTruncature = g_utf8_offset_to_pointer (cTruncatedName, iNbChars);
			*cTruncature = '.';
			*(cTruncature+1) = '.';
			*(cTruncature+2) = '.';
		}
	}
	else
	{
		iStringLength = strlen (cString);
		int iNbChars = -1;
		if (iNbCaracters < 0)
		{
			iNbChars = MAX (0, iStringLength + iNbCaracters);
		}
		else if (iStringLength > iNbCaracters)
		{
			iNbChars = iNbCaracters;
		}
		
		if (iNbChars != -1)
		{
			cTruncatedName = g_new0 (gchar, iNbCaracters + 4);
			if (iNbChars != 0)
				strncpy (cTruncatedName, cString, iNbChars);
			
			cTruncatedName[iNbChars] = '.';
			cTruncatedName[iNbChars+1] = '.';
			cTruncatedName[iNbChars+2] = '.';
		}
	}
	if (cTruncatedName == NULL)
		cTruncatedName = cUtf8Name;
	else
		g_free (cUtf8Name);
	//g_print (" -> etiquette : %s\n", cTruncatedName);
	return cTruncatedName;
}


gboolean cairo_dock_remove_version_from_string (gchar *cString)
{
	if (cString == NULL)
		return FALSE;
	int n = strlen (cString);
	gchar *str = cString + n - 1;
	do
	{
		if (g_ascii_isdigit(*str) || *str == '.')
		{
			str --;
			continue;
		}
		if (*str == '-' || *str == ' ')  // 'Glade-2', 'OpenOffice 3.1'
		{
			*str = '\0';
			return TRUE;
		}
		else
			return FALSE;
	}
	while (str != cString);
	return FALSE;
}


void cairo_dock_remove_html_spaces (gchar *cString)
{
	gchar *str = cString;
	do
	{
		str = g_strstr_len (str, -1, "%20");
		if (str == NULL)
			break ;
		*str = ' ';
		str ++;
		strcpy (str, str+2);
	}
	while (TRUE);
}


void cairo_dock_get_version_from_string (const gchar *cVersionString, int *iMajorVersion, int *iMinorVersion, int *iMicroVersion)
{
	gchar **cVersions = g_strsplit (cVersionString, ".", -1);
	if (cVersions[0] != NULL)
	{
		*iMajorVersion = atoi (cVersions[0]);
		if (cVersions[1] != NULL)
		{
			*iMinorVersion = atoi (cVersions[1]);
			if (cVersions[2] != NULL)
				*iMicroVersion = atoi (cVersions[2]);
		}
	}
	g_strfreev (cVersions);
}


gboolean cairo_dock_string_is_address (const gchar *cString)
{
	gchar *protocole = g_strstr_len (cString, -1, "://");
	if (protocole == NULL || protocole == cString)
	{
		if (strncmp (cString, "www", 3) == 0)
			return TRUE;
		return FALSE;
	}
	const gchar *str = cString;
	while (*str == ' ')
		str ++;
	while (str < protocole)
	{
		if (! g_ascii_isalnum (*str) && *str != '-')  // x-nautilus-desktop://
			return FALSE;
		str ++;
	}
	
	return TRUE;
}


gboolean cairo_dock_string_contains (const char *cNames, const gchar *cName, const gchar *separators)
{
	g_return_val_if_fail (cNames != NULL, FALSE);
	/*
	** Search for cName in the extensions string.  Use of strstr()
	** is not sufficient because extension names can be prefixes of
	** other extension names.  Could use strtok() but the constant
	** string returned by glGetString can be in read-only memory.
	*/
	char *p = (char *) cNames;

	char *end;
	int cNameLen;

	cNameLen = strlen(cName);
	end = p + strlen(p);

	while (p < end)
	{
		int n = strcspn(p, separators);
		if ((cNameLen == n) && (strncmp(cName, p, n) == 0))
		{
			return TRUE;
		}
		p += (n + 1);
	}
	return FALSE;
}

gchar *cairo_dock_launch_command_argv_sync_with_stderr (const gchar * const * argv, gboolean bPrintStdErr)
{
	gchar *standard_output=NULL, *standard_error=NULL;
	gint exit_status=0;
	GError *erreur = NULL;
	// note: g_spawn_sync expects char** as argv, but does not modify it and casts it to const char * const *
	// see here: https://gitlab.gnome.org/GNOME/glib/-/blob/main/glib/gspawn-posix.c?ref_type=heads#L233
	gboolean r = g_spawn_sync (NULL, (gchar**) argv, NULL,
		G_SPAWN_SEARCH_PATH | G_SPAWN_CLOEXEC_PIPES | (bPrintStdErr ? 0 : G_SPAWN_STDERR_TO_DEV_NULL),
		NULL, NULL, &standard_output,
		bPrintStdErr ? &standard_error : NULL,
		&exit_status, &erreur);
	if (erreur != NULL || !r)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		g_free (standard_error);
		g_free (standard_output);
		return NULL;
	}
	if (bPrintStdErr && standard_error != NULL && *standard_error != '\0')
	{
		cd_warning (standard_error);
	}
	g_free (standard_error);
	if (standard_output != NULL && *standard_output == '\0')
	{
		g_free (standard_output);
		return NULL;
	}
	if (standard_output[strlen (standard_output) - 1] == '\n')
		standard_output[strlen (standard_output) - 1] ='\0';
	return standard_output;
}

gchar *cairo_dock_launch_command_sync_with_stderr (const gchar *cCommand, gboolean bPrintStdErr)
{
	gchar *standard_output=NULL, *standard_error=NULL;
	gint exit_status=0;
	GError *erreur = NULL;
	gboolean r = g_spawn_command_line_sync (cCommand,
		&standard_output,
		&standard_error,
		&exit_status,
		&erreur);
	if (erreur != NULL || !r)
	{
		cd_warning (erreur->message);
		g_error_free (erreur);
		g_free (standard_error);
		return NULL;
	}
	if (bPrintStdErr && standard_error != NULL && *standard_error != '\0')
	{
		cd_warning (standard_error);
	}
	g_free (standard_error);
	if (standard_output != NULL && *standard_output == '\0')
	{
		g_free (standard_output);
		return NULL;
	}
	if (standard_output[strlen (standard_output) - 1] == '\n')
		standard_output[strlen (standard_output) - 1] ='\0';
	return standard_output;
}

gboolean cairo_dock_launch_command_printf (const gchar *cCommandFormat, const gchar *cWorkingDirectory, ...)
{
	va_list args;
	va_start (args, cWorkingDirectory);
	gchar *cCommand = g_strdup_vprintf (cCommandFormat, args);
	va_end (args);
	
	gboolean r = cairo_dock_launch_command_full (cCommand, cWorkingDirectory);
	g_free (cCommand);
	
	return r;
}

static void _child_watch_dummy (GPid pid, gint, gpointer)
{
	g_spawn_close_pid (pid); // note: this is a no-op
}
gboolean cairo_dock_launch_command_full (const gchar *cCommand, const gchar *cWorkingDirectory)
{
	g_return_val_if_fail (cCommand != NULL, FALSE);
	cd_debug ("%s (%s , %s)", __func__, cCommand, cWorkingDirectory);
	
	gchar **args = NULL;
	GError *erreur = NULL;
	if (!g_shell_parse_argv (cCommand, NULL, &args, &erreur))
	{
		cd_warning ("couldn't launch this command (%s : %s)", cCommand, erreur->message);
		g_error_free (erreur);
		if (args) g_strfreev (args);
		return FALSE;
	}

	gboolean r = cairo_dock_launch_command_argv_full ((const gchar * const *)args, cWorkingDirectory, FALSE);
	g_strfreev (args);
	return r;
}

gboolean cairo_dock_launch_command_argv_full (const gchar * const * args, const gchar *cWorkingDirectory, gboolean bGraphicalApp)
{
	g_return_val_if_fail (args != NULL && args[0] != NULL, FALSE);
	GError *erreur = NULL;
	GPid pid;
	char **envp = NULL;
	
	if (bGraphicalApp)
	{
		// this is a hack, and is actually ignored at least on Wayland
		GAppInfo *info = g_app_info_create_from_commandline(args[0], args[0], G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION, NULL);
		if (info) {
			GdkAppLaunchContext *ctx = gdk_display_get_app_launch_context(gdk_display_get_default());
			char *startup_id = g_app_launch_context_get_startup_notify_id(G_APP_LAUNCH_CONTEXT(ctx), info, NULL);
			if (startup_id) {
				envp = g_get_environ();
				envp = g_environ_setenv(envp, "DESKTOP_STARTUP_ID", startup_id, TRUE);
				envp = g_environ_setenv(envp, "XDG_ACTIVATION_TOKEN", startup_id, TRUE);
				g_free (startup_id);
			}
			g_object_unref(ctx);
			g_object_unref (info);
		}
	}
	
	// note: args are expected as gchar**, but not modified (casted back to const immediately)
	gboolean ret =  g_spawn_async (cWorkingDirectory, (gchar**)args, envp,
		G_SPAWN_SEARCH_PATH |
		// note: posix_spawn (and clone on Linux) will be used if we include the following
		// two flags, but this requires to later add a child watch and also adds a risk of
		// fd leakage (although it has not been an issue before with system() as well)
		G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_LEAVE_DESCRIPTORS_OPEN |
		G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
		NULL, NULL,
		&pid, &erreur);
	if (!ret)
	{
		cd_warning ("couldn't launch this command (%s : %s)", args[0], erreur->message);
		g_error_free (erreur);
	}
	else g_child_watch_add (pid, _child_watch_dummy, NULL);
	if (envp) g_strfreev (envp);
	return ret;
}

gboolean cairo_dock_launch_command_single (const gchar *cExec)
{
	const gchar * const args[] = {cExec, NULL};
	return cairo_dock_launch_command_argv (args);
}

gboolean cairo_dock_launch_command_single_gui (const gchar *cExec)
{
	const gchar * const args[] = {cExec, NULL};
	return cairo_dock_launch_command_argv_full (args, NULL, TRUE);
}

const gchar * cairo_dock_get_default_terminal (void)
{
	const gchar *cTerm = g_getenv ("COLORTERM");
	if (cTerm != NULL && strlen (cTerm) > 1)  // Filter COLORTERM=1 or COLORTERM=y because we need the name of the terminal
		return cTerm;
	else if (g_iDesktopEnv == CAIRO_DOCK_GNOME)
		return "gnome-terminal";
	else if (g_iDesktopEnv == CAIRO_DOCK_XFCE)
		return "xfce4-terminal";
	else if (g_iDesktopEnv == CAIRO_DOCK_KDE)
		return "konsole";
	else if ((cTerm = g_getenv ("TERM")) != NULL)
		return cTerm;
	else
		return "xterm";
}

gchar * cairo_dock_get_command_with_right_terminal (const gchar *cCommand)
{
	const gchar *cTerm = cairo_dock_get_default_terminal ();
	/* Very very strange, an exception for KDE! :-)
	 * From konsole's man: -e <command> [ arguments ]
	 */
	if (strncmp (cTerm, "konsole", 7) == 0)
		return g_strdup_printf ("%s -e %s", cTerm, cCommand);
	else
		return g_strdup_printf ("%s -e \"%s\"", cTerm, cCommand);
}


#ifdef HAVE_X11

#include <gdk/gdkx.h>  // gdk_x11_get_default_xdisplay
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#ifdef HAVE_XEXTEND
#include <X11/extensions/Xrandr.h>
#endif

gboolean cairo_dock_property_is_present_on_root (const gchar *cPropertyName)
{
	GdkDisplay *gdsp = gdk_display_get_default();
	if (! GDK_IS_X11_DISPLAY(gdsp))
		return FALSE;
	Display *display = GDK_DISPLAY_XDISPLAY (gdsp);
	Atom atom = XInternAtom (display, cPropertyName, False);
	Window root = DefaultRootWindow (display);
	int iNbProperties;
	Atom *pAtomList = XListProperties (display, root, &iNbProperties);
	int i;
	for (i = 0; i < iNbProperties; i ++)
	{
		if (pAtomList[i] == atom)
			break;
	}
	XFree (pAtomList);
	return (i != iNbProperties);
}

gboolean cairo_dock_check_xrandr (int iMajor, int iMinor)
{
	#ifdef HAVE_XEXTEND
	static gboolean s_bChecked = FALSE;
	static int s_iXrandrMajor = 0, s_iXrandrMinor = 0;
	if (!s_bChecked)
	{
		s_bChecked = TRUE;
		GdkDisplay *gdsp = gdk_display_get_default();
		if (! GDK_IS_X11_DISPLAY(gdsp))
			return FALSE;
		Display *display = GDK_DISPLAY_XDISPLAY (gdsp);
		int event_base, error_base;
		if (! XRRQueryExtension (display, &event_base, &error_base))
		{
			cd_warning ("Xrandr extension not available.");
		}
		else
		{
			XRRQueryVersion (display, &s_iXrandrMajor, &s_iXrandrMinor);
		}
	}
	
	return (s_iXrandrMajor > iMajor || (s_iXrandrMajor == iMajor && s_iXrandrMinor >= iMinor));  // if XRandr is not available, the version will stay at 0.0 and therefore the function will always return FALSE.
	#else
	return FALSE;
	#endif
}

#endif
