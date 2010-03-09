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

#include "cairo-dock-icons.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-log.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-application-factory.h"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopGeometry g_desktopGeometry;

static Display *s_XDisplay = NULL;
static Atom s_aNetWmIcon;
static Atom s_aNetWmState;
static Atom s_aNetWmSkipPager;
static Atom s_aNetWmSkipTaskbar;
static Atom s_aNetWmWindowType;
static Atom s_aNetWmWindowTypeNormal;
static Atom s_aNetWmWindowTypeDialog;
static Atom s_aWmHints;
static Atom s_aNetWmHidden;
static Atom s_aNetWmFullScreen;
static Atom s_aNetWmMaximizedHoriz;
static Atom s_aNetWmMaximizedVert;
static Atom s_aNetWmDemandsAttention;


void cairo_dock_initialize_application_factory (Display *pXDisplay)
{
	s_XDisplay = pXDisplay;
	g_return_if_fail (s_XDisplay != NULL);

	s_aNetWmIcon = XInternAtom (s_XDisplay, "_NET_WM_ICON", False);

	s_aNetWmState = XInternAtom (s_XDisplay, "_NET_WM_STATE", False);
	s_aNetWmSkipPager = XInternAtom (s_XDisplay, "_NET_WM_STATE_SKIP_PAGER", False);
	s_aNetWmSkipTaskbar = XInternAtom (s_XDisplay, "_NET_WM_STATE_SKIP_TASKBAR", False);
	s_aNetWmHidden = XInternAtom (s_XDisplay, "_NET_WM_STATE_HIDDEN", False);

	s_aNetWmWindowType = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE", False);
	s_aNetWmWindowTypeNormal = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	s_aNetWmWindowTypeDialog = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	
	s_aWmHints = XInternAtom (s_XDisplay, "WM_HINTS", False);
	
	s_aNetWmFullScreen = XInternAtom (s_XDisplay, "_NET_WM_STATE_FULLSCREEN", False);
	s_aNetWmMaximizedHoriz = XInternAtom (s_XDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	s_aNetWmMaximizedVert = XInternAtom (s_XDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	s_aNetWmDemandsAttention = XInternAtom (s_XDisplay, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
}

static Window _cairo_dock_get_parent_window (Window Xid)
{
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
Icon * cairo_dock_new_appli_icon (Window Xid, Window *XParentWindow)
{
	//g_print ("%s (%d)\n", __func__, Xid);
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
		if (*pTypeBuffer != s_aNetWmWindowTypeNormal)  // pas une fenetre normale.
		{
			if (*pTypeBuffer == s_aNetWmWindowTypeDialog)  // on saute si c'est un dialogue modal.
			{
				/*Window iPropWindow;
				XGetTransientForHint (s_XDisplay, Xid, &iPropWindow);
				g_print ("%s\n", gdk_x11_get_xatom_name (iPropWindow));*/
				Window XMainAppliWindow = _cairo_dock_get_parent_window (Xid);
				if (XMainAppliWindow != 0)
				{
					cd_debug ("  dialogue 'transient for %d' => on ignore", XMainAppliWindow);
					if (bDemandsAttention)
						*XParentWindow = XMainAppliWindow;
					XFree (pTypeBuffer);
					return NULL;
				}
			}
			else  // autre type : on saute.
			{
				cd_debug ("type indesirable (%d)", *pTypeBuffer);
				XFree (pTypeBuffer);
				return NULL;
			}
		}
		XFree (pTypeBuffer);
	}
	else  // pas de type defini.
	{
		Window XMainAppliWindow = 0;
		XGetTransientForHint (s_XDisplay, Xid, &XMainAppliWindow);
		if (XMainAppliWindow != 0)
		{
			cd_debug ("  fenetre modale => on saute.");
			if (bDemandsAttention)
				*XParentWindow = XMainAppliWindow;
			return NULL;
		}
	}
	
	//\__________________ On recupere son nom.
	gchar *cName = cairo_dock_get_xwindow_name (Xid, TRUE);
	cd_debug ("recuperation de '%s' (bIsHidden : %d)", cName, bIsHidden);
	
	//\__________________ On recupere la classe.
	XClassHint *pClassHint = XAllocClassHint ();
	gchar *cClass = NULL;
	if (XGetClassHint (s_XDisplay, Xid, pClassHint) != 0)
	{
		cd_debug ("  res_name : %s(%x); res_class : %s(%x)", pClassHint->res_name, pClassHint->res_name, pClassHint->res_class, pClassHint->res_class);
		if (pClassHint->res_class && strcmp (pClassHint->res_class, "Wine") == 0 && pClassHint->res_name && g_str_has_suffix (pClassHint->res_name, ".exe"))
		{
			cd_debug ("  wine application detected, changing the class '%s' to '%s'", pClassHint->res_class, pClassHint->res_name);
			cClass = g_ascii_strdown (pClassHint->res_name, -1);
		}
		else if (*pClassHint->res_class == '/' && g_str_has_suffix (pClassHint->res_class, ".exe"))  // cas des applications Mono telles que tomboy ...
		{
			gchar *str = strrchr (pClassHint->res_class, '/');
			if (str)
				str ++;
			else
				str = pClassHint->res_class;
			cClass = g_ascii_strdown (str, -1);
			cClass[strlen (cClass) - 4] = '\0';
		}
		else
			cClass = g_ascii_strdown (pClassHint->res_class, -1);  // on la passe en minuscule, car certaines applis ont la bonne idee de donner des classes avec une majuscule ou non suivant les fenetres.
		
		cairo_dock_remove_version_from_string (cClass);  // on enleve les numeros de version (Openoffice.org-3.1)
		
		XFree (pClassHint->res_name);
		XFree (pClassHint->res_class);
	}
	else
	{
		cd_warning ("this window doesn't belong to any class, skip it.");
	}
	XFree (pClassHint);
	
	
	//\__________________ On cree l'icone.
	Icon *icon = g_new0 (Icon, 1);
	icon->iType = CAIRO_DOCK_APPLI;
	icon->Xid = Xid;
	
	//\__________________ On renseigne les infos en provenance de X.
	icon->cName = (cName ? cName : g_strdup (cClass));
	icon->cClass = cClass;
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
		
	cairo_dock_set_xwindow_mask (Xid, PropertyChangeMask | StructureNotifyMask);

	return icon;
}
