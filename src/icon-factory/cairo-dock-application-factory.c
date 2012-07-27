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

#include <math.h>
#include <string.h>
#include <cairo.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>

#include "cairo-dock-icon-factory.h"
#include "cairo-dock-container.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-application-factory.h"

extern CairoDockDesktopGeometry g_desktopGeometry;

static Display *s_XDisplay = NULL;
static Atom s_aNetWmIcon;
static Atom s_aNetWmState;
//static Atom s_aNetWmSkipPager;
static Atom s_aNetWmSkipTaskbar;
static Atom s_aNetWmWindowType;
static Atom s_aNetWmWindowTypeNormal;
static Atom s_aNetWmWindowTypeDialog;
static Atom s_aNetWmWindowTypeDock;
static Atom s_aWmHints;
static Atom s_aNetWmHidden;
static Atom s_aNetWmFullScreen;
static Atom s_aNetWmMaximizedHoriz;
static Atom s_aNetWmMaximizedVert;
static Atom s_aNetWmDemandsAttention;


static void _cairo_dock_initialize_application_factory (void)
{
	s_XDisplay = cairo_dock_get_Xdisplay ();

	s_aNetWmIcon = XInternAtom (s_XDisplay, "_NET_WM_ICON", False);

	s_aNetWmState = XInternAtom (s_XDisplay, "_NET_WM_STATE", False);
	//s_aNetWmSkipPager = XInternAtom (s_XDisplay, "_NET_WM_STATE_SKIP_PAGER", False);
	s_aNetWmSkipTaskbar = XInternAtom (s_XDisplay, "_NET_WM_STATE_SKIP_TASKBAR", False);
	s_aNetWmHidden = XInternAtom (s_XDisplay, "_NET_WM_STATE_HIDDEN", False);

	s_aNetWmWindowType = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE", False);
	s_aNetWmWindowTypeNormal = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	s_aNetWmWindowTypeDialog = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	s_aNetWmWindowTypeDock = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_DOCK", False);
	
	s_aWmHints = XInternAtom (s_XDisplay, "WM_HINTS", False);
	
	s_aNetWmFullScreen = XInternAtom (s_XDisplay, "_NET_WM_STATE_FULLSCREEN", False);
	s_aNetWmMaximizedHoriz = XInternAtom (s_XDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	s_aNetWmMaximizedVert = XInternAtom (s_XDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	s_aNetWmDemandsAttention = XInternAtom (s_XDisplay, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
}

static Window _cairo_dock_get_parent_window (Window Xid)
{
	/// seems like XGetTransientForHint (s_XDisplay, Xid, &xParentWindow) doesn't work ... yet it does exactly the same ...
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	Window *pXBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, XInternAtom (s_XDisplay, "WM_TRANSIENT_FOR", False), 0, G_MAXULONG, False, XA_WINDOW, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXBuffer);

	Window xParentWindow = (iBufferNbElements > 0 && pXBuffer != NULL ? pXBuffer[0] : 0);
	if (pXBuffer != NULL)
		XFree (pXBuffer);
	return xParentWindow;
}

/*static gchar *_cairo_dock_get_appli_command (Window Xid)
{
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pPidBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, XInternAtom (s_XDisplay, "_NET_WM_PID", False), 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pPidBuffer);
	
	gchar *cCommand = NULL;
	if (iBufferNbElements > 0)
	{
		guint iPid = *pPidBuffer;
		gchar *cFilePath = g_strdup_printf ("/proc/%d/cmdline", iPid);  // utiliser /proc/%d/stat pour avoir le nom de fichier de l'executable
		gsize length = 0;
		gchar *cContent = NULL;
		g_file_get_contents (cFilePath,
			&cCommand,  // contient des '\0' entre les arguments.
			&length,
			NULL);
		g_free (cFilePath);
	}
	if (pPidBuffer != NULL)
		XFree (pPidBuffer);
	return cCommand;
}*/

Icon *cairo_dock_new_appli_icon (Window Xid, Window *XParentWindow)
{
	//g_print ("%s (%d)\n", __func__, Xid);
	if (s_XDisplay == NULL)
		_cairo_dock_initialize_application_factory ();
	
	guchar *pNameBuffer = NULL;
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements;
	cairo_surface_t *pNewSurface = NULL;
	double fWidth, fHeight;

	//\__________________ On regarde si on doit l'afficher ou la sauter.
	gboolean bSkip = FALSE, bIsHidden = FALSE, bIsFullScreen = FALSE, bIsMaximized = FALSE, bDemandsAttention = FALSE;
	gulong *pXStateBuffer = NULL;
	iBufferNbElements = 0;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmState, 0, G_MAXULONG, False, XA_ATOM, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXStateBuffer);
	if (iBufferNbElements > 0)
	{
		guint i, iNbMaximizedDimensions = 0;
		for (i = 0; i < iBufferNbElements && ! bSkip; i ++)
		{
			if (pXStateBuffer[i] == s_aNetWmSkipTaskbar)
				bSkip = TRUE;
			else if (pXStateBuffer[i] == s_aNetWmHidden)
				bIsHidden = TRUE;
			else if (pXStateBuffer[i] == s_aNetWmMaximizedVert)
				iNbMaximizedDimensions ++;
			else if (pXStateBuffer[i] == s_aNetWmMaximizedHoriz)
				iNbMaximizedDimensions ++;
			else if (pXStateBuffer[i] == s_aNetWmFullScreen)
				bIsFullScreen = TRUE;
			else if (pXStateBuffer[i] == s_aNetWmDemandsAttention)
				bDemandsAttention = TRUE;
			//else if (pXStateBuffer[i] == s_aNetWmSkipPager)  // contestable ...
			//	bSkip = TRUE;
		}
		bIsMaximized = (iNbMaximizedDimensions == 2);
		//g_print (" -------- bSkip : %d\n",  bSkip);
		XFree (pXStateBuffer);
	}
	if (bSkip)
	{
		cd_debug ("  cette fenetre est timide");
		return NULL;
	}

	//\__________________ On regarde son type.
	gulong *pTypeBuffer = NULL;
	cd_debug (" + nouvelle icone d'appli (%d)", Xid);
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmWindowType, 0, G_MAXULONG, False, XA_ATOM, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pTypeBuffer);
	if (iBufferNbElements != 0)
	{
		gboolean bKeep = FALSE;
		guint i;
		for (i = 0; i < iBufferNbElements; i ++)  // The Client SHOULD specify window types in order of preference (the first being most preferable) but MUST include at least one of the basic window type atoms.
		{
			if (pTypeBuffer[i] == s_aNetWmWindowTypeNormal)  // normal window -> take it
			{
				bKeep = TRUE;
				break;
			}
			if (pTypeBuffer[i] == s_aNetWmWindowTypeDialog)  // dialog -> skip modal dialog, because it's most probably a dialog box (like an open/save dialog)
			{
				Window XMainAppliWindow = _cairo_dock_get_parent_window (Xid);  // maybe we should also get the _NET_WM_STATE_MODAL property, although if a dialog is set modal but not transient, that would probably be an error from the application.
				if (XMainAppliWindow != None && XMainAppliWindow != DefaultRootWindow (s_XDisplay))  // transient dialog, don't keep it, unless it also has the "normal" type further in the buffer.
				{
					cd_debug ("  dialog 'transient for %d' => ignore", XMainAppliWindow);
					if (bDemandsAttention)
						*XParentWindow = XMainAppliWindow;
				}
				else
				{
					bKeep = TRUE;
					break;
				}
			}  // skip any other type (dock, menu, etc)
			else if (pTypeBuffer[i] == s_aNetWmWindowTypeDock)  // workaround for the Unity-panel: if the type 'dock' is present, don't look further (as they add the 'normal' type too, which is non-sense).
			{
				break;
			}
		}
		XFree (pTypeBuffer);
		if (! bKeep)
		{
			cd_debug ("ignore this window");
			return NULL;
		}
	}
	else  // pas de type defini.
	{
		Window XMainAppliWindow = 0;
		XGetTransientForHint (s_XDisplay, Xid, &XMainAppliWindow);
		if (XMainAppliWindow != 0)
		{
			cd_debug ("  transient window => skip it");
			if (bDemandsAttention)
				*XParentWindow = XMainAppliWindow;
			return NULL;
		}
	}
	
	//\__________________ On recupere son nom.
	gchar *cName = cairo_dock_get_xwindow_name (Xid, TRUE);
	cd_debug ("recuperation de '%s' (bIsHidden : %d)", cName, bIsHidden);
	
	//\__________________ On recupere la classe.
	gchar *cClass = NULL, *cWmClass = NULL;
	cClass = cairo_dock_get_xwindow_class (Xid, &cWmClass);
	if (cClass == NULL)
	{
		cd_warning ("this window (%s, %ld)doesn't belong to any class, skip it.", cName, Xid);
		return NULL;
	}
	
	//\__________________ On cree l'icone.
	Icon *icon = cairo_dock_new_icon ();
	icon->iTrueType = CAIRO_DOCK_ICON_TYPE_APPLI;
	icon->iGroup = CAIRO_DOCK_APPLI;
	icon->Xid = Xid;
	
	//\__________________ On renseigne les infos en provenance de X.
	icon->cName = (cName ? cName : g_strdup (cClass));
	icon->cClass = cClass;  // we'll register the class during the loading of the icon, since it can take some time, and we don't really need the class params right now.
	icon->cWmClass = cWmClass;
	icon->bIsHidden = bIsHidden;
	icon->bIsMaximized = bIsMaximized;
	icon->bIsFullScreen = bIsFullScreen;
	icon->bIsDemandingAttention = bDemandsAttention;
	icon->fOrder = CAIRO_DOCK_LAST_ORDER;
	
	icon->iNumDesktop = cairo_dock_get_xwindow_desktop (Xid);
	
	int iLocalPositionX, iLocalPositionY, iWidthExtent, iHeightExtent;
	cairo_dock_get_xwindow_geometry (Xid, &iLocalPositionX, &iLocalPositionY, &iWidthExtent, &iHeightExtent);
	
	icon->iViewPortX = iLocalPositionX / g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] + g_desktopGeometry.iCurrentViewportX;
	icon->iViewPortY = iLocalPositionY / g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] + g_desktopGeometry.iCurrentViewportY;
	
	icon->windowGeometry.x = iLocalPositionX;
	icon->windowGeometry.y = iLocalPositionY;
	icon->windowGeometry.width = iWidthExtent;
	icon->windowGeometry.height = iHeightExtent;
	
	///cairo_dock_set_xwindow_mask (Xid, PropertyChangeMask | StructureNotifyMask);

	return icon;
}
