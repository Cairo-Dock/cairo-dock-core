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

gchar *cairo_dock_generate_unique_filename (const gchar *cBaseName, const gchar *cCairoDockDataDir)
{
	int iPrefixNumber = 0;
	GString *sFileName = g_string_new ("");

	do
	{
		iPrefixNumber ++;
		g_string_printf (sFileName, "%s/%02d%s", cCairoDockDataDir, iPrefixNumber, cBaseName);
	} while (iPrefixNumber < 99 && g_file_test (sFileName->str, G_FILE_TEST_EXISTS));

	g_string_free (sFileName, TRUE);
	if (iPrefixNumber == 99)
		return NULL;
	else
		return g_strdup_printf ("%02d%s", iPrefixNumber, cBaseName);
}


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

static gpointer _cairo_dock_launch_threaded (gchar *cCommand)
{
	int r;
	r = system (cCommand);
	if (r != 0)
		cd_warning ("couldn't launch this command (%s)", cCommand);
	g_free (cCommand);
	return NULL;
}
gboolean cairo_dock_launch_command_full (const gchar *cCommand, const gchar *cWorkingDirectory)
{
	g_return_val_if_fail (cCommand != NULL, FALSE);
	cd_debug ("%s (%s , %s)", __func__, cCommand, cWorkingDirectory);
	
	gchar *cBGCommand = NULL;
	if (cCommand[strlen (cCommand)-1] != '&')
		cBGCommand = g_strconcat (cCommand, " &", NULL);
	
	gchar *cCommandFull = NULL;
	if (cWorkingDirectory != NULL)
	{
		cCommandFull = g_strdup_printf ("cd \"%s\" && %s", cWorkingDirectory, cBGCommand ? cBGCommand : cCommand);
		g_free (cBGCommand);
		cBGCommand = NULL;
	}
	else if (cBGCommand != NULL)
	{
		cCommandFull = cBGCommand;
		cBGCommand = NULL;
	}
	
	if (cCommandFull == NULL)
		cCommandFull = g_strdup (cCommand);

	GError *erreur = NULL;
	#if (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 32)
	GThread* pThread = g_thread_create ((GThreadFunc) _cairo_dock_launch_threaded, cCommandFull, FALSE, &erreur);
	#else
	// The name can be useful for discriminating threads in a debugger.
	// Some systems restrict the length of name to 16 bytes. 
	gchar *cThreadName = g_strndup (cCommand, 15);
	GThread* pThread = g_thread_try_new (cThreadName, (GThreadFunc) _cairo_dock_launch_threaded, cCommandFull, &erreur);
	g_thread_unref (pThread);
	g_free (cThreadName);
	#endif
	if (erreur != NULL)
	{
		cd_warning ("couldn't launch this command (%s : %s)", cCommandFull, erreur->message);
		g_error_free (erreur);
		g_free (cCommandFull);
		return FALSE;
	}
	return TRUE;
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
	Display *display = gdk_x11_get_default_xdisplay ();
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
		Display *display = gdk_x11_get_default_xdisplay ();
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
