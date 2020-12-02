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

#include "gldi-config.h"
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "gldi-config.h"
#ifdef HAVE_XEXTEND
#include <X11/extensions/Xcomposite.h>
//#include <X11/extensions/Xdamage.h>
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>  // Note: Xinerama is deprecated by XRandr >= 1.3
#endif
#include <X11/extensions/Xrandr.h>
#endif

#include "cairo-dock-log.h"
#include "cairo-dock-utils.h"  // cairo_dock_remove_version_from_string, cairo_dock_check_xrandr
#include "cairo-dock-surface-factory.h"  // cairo_dock_create_surface_from_xicon_buffer
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-opengl.h"  // for texture_from_pixmap
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-windows-manager.h" // gldi_window_parse_class

#include <cairo/cairo-xlib.h>  // needed for cairo_xlib_surface_create

extern gboolean g_bEasterEggs;
extern CairoDockGLConfig g_openglConfig;

static gboolean s_bUseXComposite = TRUE;
static gboolean s_bUseXinerama = TRUE;
static gboolean s_bUseXrandr = TRUE;
//extern int g_iDamageEvent;

static Display *s_XDisplay = NULL;
// Atoms pour le bureau
static Atom s_aNetWmWindowType;
static Atom s_aNetWmWindowTypeNormal;
static Atom s_aNetWmWindowTypeDialog;
static Atom s_aNetWmWindowTypeDock;
static Atom s_aNetWmIconGeometry;
static Atom s_aNetCurrentDesktop;
static Atom s_aNetDesktopViewport;
static Atom s_aNetDesktopGeometry;
static Atom s_aNetNbDesktops;
static Atom s_aNetDesktopNames;
static Atom s_aRootMapID;
// Atoms pour les fenetres
static Atom s_aNetClientList;  // z-order
static Atom s_aNetClientListStacking;  // age-order
static Atom s_aNetActiveWindow;
static Atom s_aNetWmState;
static Atom s_aNetWmBelow;
static Atom s_aNetWmAbove;
static Atom s_aNetWmSticky;
static Atom s_aNetWmHidden;
static Atom s_aNetWmFullScreen;
static Atom s_aNetWmSkipTaskbar;
static Atom s_aNetWmMaximizedHoriz;
static Atom s_aNetWmMaximizedVert;
static Atom s_aNetWmDemandsAttention;
static Atom s_aNetWMAllowedActions;
static Atom s_aNetWMActionMinimize;
static Atom s_aNetWMActionMaximizeHorz;
static Atom s_aNetWMActionMaximizeVert;
static Atom s_aNetWMActionClose;
static Atom s_aNetWmDesktop;
static Atom s_aNetWmIcon;
static Atom s_aNetWmName;
static Atom s_aWmName;
static Atom s_aUtf8String;
static Atom s_aString;
static unsigned char error_code = Success;

static GtkAllocation *_get_screens_geometry (int *pNbScreens);

static gboolean cairo_dock_support_X_extension (void);

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
} MotifWmHints, MwmHints;
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MOTIF_WM_HINTS_ELEMENTS 5
#define PROP_MWM_HINTS_ELEMENTS PROP_MOTIF_WM_HINTS_ELEMENTS


static int _cairo_dock_xerror_handler (G_GNUC_UNUSED Display * pDisplay, XErrorEvent *pXError)
{
	//g_print ("Error (%d, %d, %d) during an X request on %d\n", pXError->error_code, pXError->request_code, pXError->minor_code, pXError->resourceid);
	error_code = pXError->error_code;
	return 0;
}
Display *cairo_dock_initialize_X_desktop_support (void)
{
	if (s_XDisplay != NULL)
		return s_XDisplay;
	s_XDisplay = XOpenDisplay (0);
	g_return_val_if_fail (s_XDisplay != NULL, NULL);
	
	XSetErrorHandler (_cairo_dock_xerror_handler);
	
	cairo_dock_support_X_extension ();
	
	s_aNetWmWindowType          = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE", False);
    s_aNetWmWindowTypeNormal    = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    s_aNetWmWindowTypeDialog    = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    s_aNetWmWindowTypeDock      = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_DOCK", False);
    s_aNetWmIconGeometry        = XInternAtom (s_XDisplay, "_NET_WM_ICON_GEOMETRY", False);
    s_aNetCurrentDesktop        = XInternAtom (s_XDisplay, "_NET_CURRENT_DESKTOP", False);
    s_aNetDesktopViewport       = XInternAtom (s_XDisplay, "_NET_DESKTOP_VIEWPORT", False);
    s_aNetDesktopGeometry       = XInternAtom (s_XDisplay, "_NET_DESKTOP_GEOMETRY", False);
    s_aNetNbDesktops            = XInternAtom (s_XDisplay, "_NET_NUMBER_OF_DESKTOPS", False);
    s_aNetDesktopNames          = XInternAtom (s_XDisplay, "_NET_DESKTOP_NAMES", False);
    s_aRootMapID                = XInternAtom (s_XDisplay, "_XROOTPMAP_ID", False);
    
    s_aNetClientListStacking    = XInternAtom (s_XDisplay, "_NET_CLIENT_LIST_STACKING", False);
    s_aNetClientList            = XInternAtom (s_XDisplay, "_NET_CLIENT_LIST", False);
    s_aNetActiveWindow          = XInternAtom (s_XDisplay, "_NET_ACTIVE_WINDOW", False);
    s_aNetWmState               = XInternAtom (s_XDisplay, "_NET_WM_STATE", False);
    s_aNetWmFullScreen          = XInternAtom (s_XDisplay, "_NET_WM_STATE_FULLSCREEN", False);
    s_aNetWmAbove               = XInternAtom (s_XDisplay, "_NET_WM_STATE_ABOVE", False);
    s_aNetWmBelow               = XInternAtom (s_XDisplay, "_NET_WM_STATE_BELOW", False);
    s_aNetWmSticky              = XInternAtom (s_XDisplay, "_NET_WM_STATE_STICKY", False);
    s_aNetWmHidden              = XInternAtom (s_XDisplay, "_NET_WM_STATE_HIDDEN", False);
    s_aNetWmSkipTaskbar         = XInternAtom (s_XDisplay, "_NET_WM_STATE_SKIP_TASKBAR", False);
    s_aNetWmMaximizedHoriz      = XInternAtom (s_XDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    s_aNetWmMaximizedVert       = XInternAtom (s_XDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    s_aNetWmDemandsAttention    = XInternAtom (s_XDisplay, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
    s_aNetWMAllowedActions      = XInternAtom (s_XDisplay, "_NET_WM_ALLOWED_ACTIONS", False);
    s_aNetWMActionMinimize      = XInternAtom (s_XDisplay, "_NET_WM_ACTION_MINIMIZE", False);
    s_aNetWMActionMaximizeHorz  = XInternAtom (s_XDisplay, "_NET_WM_ACTION_MAXIMIZE_HORZ", False);
    s_aNetWMActionMaximizeVert  = XInternAtom (s_XDisplay, "_NET_WM_ACTION_MAXIMIZE_VERT", False);
    s_aNetWMActionClose         = XInternAtom (s_XDisplay, "_NET_WM_ACTION_CLOSE", False);
    s_aNetWmDesktop             = XInternAtom (s_XDisplay, "_NET_WM_DESKTOP", False);
    s_aNetWmIcon                = XInternAtom (s_XDisplay, "_NET_WM_ICON", False);
    s_aNetWmName                = XInternAtom (s_XDisplay, "_NET_WM_NAME", False);
    s_aWmName                   = XInternAtom (s_XDisplay, "WM_NAME", False);
    s_aUtf8String               = XInternAtom (s_XDisplay, "UTF8_STRING", False);
    s_aString                   = XInternAtom (s_XDisplay, "STRING", False);
	
	Screen *XScreen = XDefaultScreenOfDisplay (s_XDisplay);
	
	g_desktopGeometry.Xscreen.width = WidthOfScreen (XScreen); // x and y are nul.
	g_desktopGeometry.Xscreen.height = HeightOfScreen (XScreen);
	
	g_desktopGeometry.pScreens = _get_screens_geometry (&g_desktopGeometry.iNbScreens);
	
	return s_XDisplay;
}


Display *cairo_dock_get_X_display (void)
{
	return s_XDisplay;
}

void cairo_dock_reset_X_error_code (void)
{
	error_code = Success;
}

unsigned char cairo_dock_get_X_error_code (void)
{
	return error_code;
}

static GtkAllocation *_get_screens_geometry (int *pNbScreens)
{
	GtkAllocation *pScreens = NULL;
	GtkAllocation *pScreen;
	int iNbScreens = 0;
	/*Unit Tests
	iNbScreens = 2;
	pScreens = g_new0 (GtkAllocation, iNbScreens);
	pScreens[0].x = 0;
	pScreens[0].y = 0;
	pScreens[0].width = 1000;
	pScreens[0].height = 1050;
	pScreens[1].x = 1000;
	pScreens[1].y = 0;
	pScreens[1].width = 680;
	pScreens[1].height = 1050;
	*pNbScreens = iNbScreens;
	return pScreens;*/
	
	#ifdef HAVE_XEXTEND
	if (s_bUseXrandr)  // we place Xrandr first to get more tests :) (and also because it will deprecate Xinerama).
	{
		cd_debug ("Using Xrandr to determine the screen's position and size ...");
		XRRScreenResources *res = XRRGetScreenResources (s_XDisplay, DefaultRootWindow (s_XDisplay));  // Xrandr >= 1.3
		if (res != NULL)
		{
			int n = res->ncrtc;
			cd_debug (" number of screen(s): %d", n);
			pScreens = g_new0 (GtkAllocation, n);
			int i;
			for (i = 0; i < n; i++)
			{
				XRRCrtcInfo *info = XRRGetCrtcInfo (s_XDisplay, res, res->crtcs[i]);
				if (info == NULL)
				{
					cd_warning ("This screen (%d) has no info, skip it.", i);
					continue;
				}
				
				if (info->width == 0 || info->height == 0)
				{
					cd_debug ("This screen (%d) has a null dimensions, skip it.", i);  // seems normal behaviour of xrandr, so no warning
					XRRFreeCrtcInfo (info);
					continue;
				}
				
				pScreen = &pScreens[iNbScreens];
				pScreen->x = info->x;
				pScreen->y = info->y;
				pScreen->width = info->width;
				pScreen->height = info->height;
				cd_message (" * screen %d(%d) => (%d;%d) %dx%d", iNbScreens, i, pScreen->x, pScreen->y, pScreen->width, pScreen->height);
				
				XRRFreeCrtcInfo (info);
				iNbScreens ++;
			}
			XRRFreeScreenResources (res);
		}
		else
			cd_warning ("No screen found from Xrandr, is it really active ?");
	}
	
	#ifdef HAVE_XINERAMA
	if (iNbScreens == 0 && s_bUseXinerama && XineramaIsActive (s_XDisplay))
	{
		cd_debug ("Using Xinerama to determine the screen's position and size ...");
		int n;
		XineramaScreenInfo *scr = XineramaQueryScreens (s_XDisplay, &n);
		if (scr != NULL)
		{
			cd_debug (" number of screen(s): %d", n);
			pScreens = g_new0 (GtkAllocation, n);
			int i;
			for (i = 0; i < n; i++)
			{
				pScreen = &pScreens[i];
				pScreen->x = scr[i].x_org;
				pScreen->y = scr[i].y_org;
				pScreen->width = scr[i].width;
				pScreen->height = scr[i].height;
				cd_message (" * screen %d(%d) => (%d;%d) %dx%d", iNbScreens, i, pScreen->x, pScreen->y, pScreen->width, pScreen->height);
				
				iNbScreens ++;
			}
			XFree (scr);
		}
		else
			cd_warning ("No screen found from Xinerama, is it really active ?");
	}
	#endif  // HAVE_XINERAMA
	#endif  // HAVE_XEXTEND
	
	if (iNbScreens == 0)
	{
		#ifdef HAVE_XEXTEND
		cd_warning ("Xrandr and Xinerama are not available, assume there is only 1 screen.");
		#else
		cd_warning ("The dock was not compiled with the support of Xinerama/Xrandr, assume there is only 1 screen.");
		#endif
		
		iNbScreens = 1;
		pScreens = g_new0 (GtkAllocation, iNbScreens);
		pScreen = &pScreens[0];
		pScreen->x = 0;
		pScreen->y = 0;
		pScreen->width = gldi_desktop_get_width();
		pScreen->height = gldi_desktop_get_height();
	}
	
	/*Window root = DefaultRootWindow (s_XDisplay);
	Atom aNetWorkArea = XInternAtom (s_XDisplay, "_NET_WORKAREA", False);
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXWorkArea = NULL;
	XGetWindowProperty (s_XDisplay, root, aNetWorkArea, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXWorkArea);
	int i;
	for (i = 0; i < iBufferNbElements/4; i ++)
	{
		// g_print ("work area : (%d;%d) %dx%d\n", pXWorkArea[4*i], pXWorkArea[4*i+1], pXWorkArea[4*i+2], pXWorkArea[4*i+3]);
	}
	XFree (pXWorkArea);*/
	
	*pNbScreens = iNbScreens;
	return pScreens;
}

gboolean cairo_dock_update_screen_geometry (void)
{
	// get the geometry of the root window (the virtual X screen) from the server.
	Window root = DefaultRootWindow (s_XDisplay);
	Window root_return;
	int x_return=1, y_return=1;
	unsigned int width_return, height_return, border_width_return, depth_return;
	XGetGeometry (s_XDisplay, root,
		&root_return,
		&x_return, &y_return,
		&width_return, &height_return,
		&border_width_return, &depth_return);
	cd_debug (">>>>>   screen resolution: %dx%d -> %dx%d", gldi_desktop_get_width(), gldi_desktop_get_height(), width_return, height_return);
	gboolean bNewSize = FALSE;
	if ((int)width_return != gldi_desktop_get_width() || (int)height_return != gldi_desktop_get_height())  // on n'utilise pas WidthOfScreen() et HeightOfScreen() car leurs valeurs ne sont pas mises a jour immediatement apres les changements de resolution.
	{
		g_desktopGeometry.Xscreen.width = width_return;
		g_desktopGeometry.Xscreen.height = height_return;
		cd_debug ("new screen size : %dx%d", gldi_desktop_get_width(), gldi_desktop_get_height());
		bNewSize = TRUE;
	}
	
	// get the size and position of each screen (they could have changed even though the X screen has not changed, for instance if you swap 2 screens).
	GtkAllocation *pScreens = g_desktopGeometry.pScreens;
	int iNbScreens = g_desktopGeometry.iNbScreens;
	g_desktopGeometry.pScreens = _get_screens_geometry (&g_desktopGeometry.iNbScreens);
	
	if (! bNewSize)  // if the X screen has not changed, check if real screens have changed.
	{
		bNewSize = (iNbScreens != g_desktopGeometry.iNbScreens);
		if (! bNewSize)
		{
			int i;
			for (i = 0; i < MIN (iNbScreens, g_desktopGeometry.iNbScreens); i ++)
			{
				if (memcmp (&pScreens[i], &g_desktopGeometry.pScreens[i], sizeof (GtkAllocation)) != 0)
				{
					bNewSize = TRUE;
					break;
				}
			}
		}
	}
	
	g_free (pScreens);
	return bNewSize;
}


gchar **cairo_dock_get_desktops_names (void)
{
	gchar **cNames = NULL;
	Window root = DefaultRootWindow (s_XDisplay);
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gchar *names = NULL;
	XGetWindowProperty (s_XDisplay, root, s_aNetDesktopNames, 0, G_MAXULONG, False, s_aUtf8String, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&names);
	
	if (iBufferNbElements > 0)
	{
		gchar *str = names;
		int n = 0;
		while (str < names + iBufferNbElements)
		{
			str = strchr (str, '\0');
			str ++;
			n ++;
		}
		
		cNames = g_new0 (gchar*, n+1);  // NULL-terminated
		int i = 0;
		str = names;
		while (str < names + iBufferNbElements)
		{
			cNames[i] = g_strdup (str);
			str = strchr (str, '\0');
			str ++;
			i ++;
		}
	}
	return cNames;
}

void cairo_dock_set_desktops_names (gchar **cNames)
{
	if (cNames == NULL)
		return;
	
	int i, n = 0;
	for (i = 0; cNames[i] != NULL; i ++)
		n += strlen (cNames[i]) + 1;  // strlen doesn't count the terminating null byte
	
	gchar *names = g_new0 (gchar, n);  // we can't use g_strjoinv as the separator is '\0'
	gchar *str = names;
	for (i = 0; cNames[i] != NULL; i ++)
	{
		strcpy (str, cNames[i]);
		str += strlen (cNames[i]) + 1;
	}
	
	Window root = DefaultRootWindow (s_XDisplay);
	XChangeProperty (s_XDisplay,
			root,
			s_aNetDesktopNames,
			s_aUtf8String, 8, PropModeReplace,
			(guchar *)names, n);
	g_free (names);
}

int cairo_dock_get_current_desktop (void)
{
	Window root = DefaultRootWindow (s_XDisplay);
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXDesktopNumberBuffer = NULL;
	XGetWindowProperty (s_XDisplay, root, s_aNetCurrentDesktop, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXDesktopNumberBuffer);

	int iDesktopNumber;
	if (iBufferNbElements > 0)
		iDesktopNumber = *pXDesktopNumberBuffer;
	else
		iDesktopNumber = 0;
	
	XFree (pXDesktopNumberBuffer);
	return iDesktopNumber;
}

void cairo_dock_get_current_viewport (int *iCurrentViewPortX, int *iCurrentViewPortY)
{
	Window root = DefaultRootWindow (s_XDisplay);
	
	Window root_return;
	int x_return=1, y_return=1;
	unsigned int width_return, height_return, border_width_return, depth_return;
	XGetGeometry (s_XDisplay, root,
		&root_return,
		&x_return, &y_return,
		&width_return, &height_return,
		&border_width_return, &depth_return);
	*iCurrentViewPortX = x_return;
	*iCurrentViewPortY = y_return;
	
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pViewportsXY = NULL;
	XGetWindowProperty (s_XDisplay, root, s_aNetDesktopViewport, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pViewportsXY);
	if (iBufferNbElements > 0)
	{
		*iCurrentViewPortX = pViewportsXY[0];
		*iCurrentViewPortY = pViewportsXY[1];
		XFree (pViewportsXY);
	}
	
}

int cairo_dock_get_nb_desktops (void)
{
	Window root = DefaultRootWindow (s_XDisplay);
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXDesktopNumberBuffer = NULL;
	XGetWindowProperty (s_XDisplay, root, s_aNetNbDesktops, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXDesktopNumberBuffer);
	
	int iNumberOfDesktops;
	if (iBufferNbElements > 0)
		iNumberOfDesktops = *pXDesktopNumberBuffer;
	else
		iNumberOfDesktops = 0;
	
	return iNumberOfDesktops;
}

void cairo_dock_get_nb_viewports (int *iNbViewportX, int *iNbViewportY)
{
	Window root = DefaultRootWindow (s_XDisplay);
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pVirtualScreenSizeBuffer = NULL;
	XGetWindowProperty (s_XDisplay, root, s_aNetDesktopGeometry, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pVirtualScreenSizeBuffer);
	if (iBufferNbElements > 0)
	{
		cd_debug ("pVirtualScreenSizeBuffer : %dx%d ; screen : %dx%d", pVirtualScreenSizeBuffer[0], pVirtualScreenSizeBuffer[1], gldi_desktop_get_width(), gldi_desktop_get_height());
		*iNbViewportX = pVirtualScreenSizeBuffer[0] / gldi_desktop_get_width();
		*iNbViewportY = pVirtualScreenSizeBuffer[1] / gldi_desktop_get_height();
		XFree (pVirtualScreenSizeBuffer);
	}
}



gboolean cairo_dock_desktop_is_visible (void)
{
	Atom aNetShowingDesktop = XInternAtom (s_XDisplay, "_NET_SHOWING_DESKTOP", False);
	gulong iLeftBytes, iBufferNbElements = 0;
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	gulong *pXBuffer = NULL;
	Window root = DefaultRootWindow (s_XDisplay);
	XGetWindowProperty (s_XDisplay, root, aNetShowingDesktop, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXBuffer);

	gboolean bDesktopIsShown = (iBufferNbElements > 0 && pXBuffer != NULL ? *pXBuffer : FALSE);
	XFree (pXBuffer);
	return bDesktopIsShown;
}

void cairo_dock_show_hide_desktop (gboolean bShow)
{
	XEvent xClientMessage;
	Window root = DefaultRootWindow (s_XDisplay);

	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = root;
	xClientMessage.xclient.message_type = XInternAtom (s_XDisplay, "_NET_SHOWING_DESKTOP", False);
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = bShow;
	xClientMessage.xclient.data.l[1] = 0;
	xClientMessage.xclient.data.l[2] = 0;
	xClientMessage.xclient.data.l[3] = 2;
	xClientMessage.xclient.data.l[4] = 0;
	
	cd_debug ("%s (%d)", __func__, bShow);
	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
	XFlush (s_XDisplay);
}

static void cairo_dock_move_current_viewport_to (int iDesktopViewportX, int iDesktopViewportY)
{
	XEvent xClientMessage;
	Window root = DefaultRootWindow (s_XDisplay);
	
	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = root;
	xClientMessage.xclient.message_type = s_aNetDesktopViewport;
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = iDesktopViewportX;
	xClientMessage.xclient.data.l[1] = iDesktopViewportY;
	xClientMessage.xclient.data.l[2] = 0;
	xClientMessage.xclient.data.l[3] = 0;
	xClientMessage.xclient.data.l[4] = 0;
	
	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
	XFlush (s_XDisplay);
}
void cairo_dock_set_current_viewport (int iViewportNumberX, int iViewportNumberY)
{
	cairo_dock_move_current_viewport_to (iViewportNumberX * gldi_desktop_get_width(), iViewportNumberY * gldi_desktop_get_height());
}
void cairo_dock_set_current_desktop (int iDesktopNumber)
{
	Window root = DefaultRootWindow (s_XDisplay);
	int iTimeStamp = cairo_dock_get_xwindow_timestamp (root);
	XEvent xClientMessage;
	
	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = root;
	xClientMessage.xclient.message_type = s_aNetCurrentDesktop;
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = iDesktopNumber;
	xClientMessage.xclient.data.l[1] = iTimeStamp;
	xClientMessage.xclient.data.l[2] = 0;
	xClientMessage.xclient.data.l[3] = 0;
	xClientMessage.xclient.data.l[4] = 0;

	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
	XFlush (s_XDisplay);
}

Pixmap cairo_dock_get_window_background_pixmap (Window Xid)
{
	g_return_val_if_fail (Xid > 0, None);
	//cd_debug ("%s (%d)", __func__, Xid);
	
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements;
	Pixmap *pPixmapIdBuffer = NULL;
	Pixmap iBgPixmapID = 0;
	XGetWindowProperty (s_XDisplay, Xid, s_aRootMapID, 0, G_MAXULONG, False, XA_PIXMAP, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pPixmapIdBuffer);
	if (iBufferNbElements != 0)
	{
		iBgPixmapID = *pPixmapIdBuffer;
		XFree (pPixmapIdBuffer);
	}
	else
		iBgPixmapID = None;
	cd_debug (" => rootmapid : %d", iBgPixmapID);
	return iBgPixmapID;
}

GdkPixbuf *cairo_dock_get_pixbuf_from_pixmap (int XPixmapID, gboolean bAddAlpha)  // cette fonction est inspiree par celle de libwnck.
{
	//\__________________ On recupere la taille telle qu'elle est actuellement sur le serveur X.
	Window root;  // inutile.
	int x, y;  // inutile.
	guint border_width;  // inutile.
	guint iWidth, iHeight, iDepth;
	if (! XGetGeometry (s_XDisplay,
		XPixmapID, &root, &x, &y,
		&iWidth, &iHeight, &border_width, &iDepth))
		return NULL;
	//g_print ("%s (%d) : %ux%ux%u (%d;%d)\n", __func__, XPixmapID, iWidth, iHeight, iDepth, x, y);

	//\__________________ On recupere le drawable associe.
	cairo_surface_t *surface = cairo_xlib_surface_create (s_XDisplay,
		XPixmapID,
		DefaultVisual(s_XDisplay, 0),
		iWidth,
		iHeight);
	GdkPixbuf *pIconPixbuf = gdk_pixbuf_get_from_surface(surface,
		0,
		0,
		iWidth,
		iHeight);
	cairo_surface_destroy(surface);
	g_return_val_if_fail (pIconPixbuf != NULL, NULL);

	//\__________________ On lui ajoute un canal alpha si necessaire.
	if (! gdk_pixbuf_get_has_alpha (pIconPixbuf) && bAddAlpha)
	{
		cd_debug ("  on lui ajoute de la transparence");
		GdkPixbuf *tmp_pixbuf = gdk_pixbuf_add_alpha (pIconPixbuf, FALSE, 255, 255, 255);
		g_object_unref (pIconPixbuf);
		pIconPixbuf = tmp_pixbuf;
	}
	return pIconPixbuf;
}


void cairo_dock_set_nb_viewports (int iNbViewportX, int iNbViewportY)
{
	XEvent xClientMessage;
	Window root = DefaultRootWindow (s_XDisplay);
	
	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = root;
	xClientMessage.xclient.message_type = s_aNetDesktopGeometry;
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = iNbViewportX * gldi_desktop_get_width();
	xClientMessage.xclient.data.l[1] = iNbViewportY * gldi_desktop_get_height();
	xClientMessage.xclient.data.l[2] = 0;
	xClientMessage.xclient.data.l[3] = 2;
	xClientMessage.xclient.data.l[4] = 0;

	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
	XFlush (s_XDisplay);
}

void cairo_dock_set_nb_desktops (gulong iNbDesktops)
{
	XEvent xClientMessage;
	Window root = DefaultRootWindow (s_XDisplay);
	
	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = root;
	xClientMessage.xclient.message_type = s_aNetNbDesktops;
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = iNbDesktops;
	xClientMessage.xclient.data.l[1] = 0;
	xClientMessage.xclient.data.l[2] = 0;
	xClientMessage.xclient.data.l[3] = 2;
	xClientMessage.xclient.data.l[4] = 0;

	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
	XFlush (s_XDisplay);
}


static gboolean cairo_dock_support_X_extension (void)
{
#ifdef HAVE_XEXTEND
	// check for XComposite >= 0.2
	int event_base, error_base, major, minor;
	if (! XCompositeQueryExtension (s_XDisplay, &event_base, &error_base))  // on regarde si le serveur X supporte l'extension.
	{
		cd_warning ("XComposite extension not available.");
		s_bUseXComposite = FALSE;
	}
	else
	{
		major = 0, minor = 0;
		XCompositeQueryVersion (s_XDisplay, &major, &minor);
		if (! (major > 0 || minor >= 2))  // 0.2 is required to have XCompositeNameWindowPixmap().
		{
			cd_warning ("XComposite extension is too old (%d.%d)", major, minor);
			s_bUseXComposite = FALSE;
		}
	}
	/*int iDamageError=0;
	if (! XDamageQueryExtension (s_XDisplay, &g_iDamageEvent, &iDamageError))
	{
		cd_warning ("XDamage extension not supported");
		return FALSE;
	}*/
	
	// check for Xinerama
	#ifdef HAVE_XINERAMA
	if (! XineramaQueryExtension (s_XDisplay, &event_base, &error_base))
	{
		cd_warning ("Xinerama extension not supported");
		s_bUseXinerama = FALSE;
	}
	#else
	s_bUseXinerama = FALSE;
	#endif
	
	// check for Xrandr >= 1.3
	s_bUseXrandr = cairo_dock_check_xrandr (1, 3);
	
	return TRUE;
#else
	cd_warning ("The dock was not compiled with the X extensions (XComposite, Xinerama, Xtest, Xrandr, etc).");
	s_bUseXComposite = FALSE;
	s_bUseXinerama = FALSE;
	s_bUseXrandr = FALSE;
	return FALSE;
#endif
}

gboolean cairo_dock_xcomposite_is_available (void)
{
	return s_bUseXComposite;
}


void cairo_dock_set_xwindow_timestamp (Window Xid, gulong iTimeStamp)
{
	g_return_if_fail (Xid > 0);
	Atom aNetWmUserTime = XInternAtom (s_XDisplay, "_NET_WM_USER_TIME", False);
	XChangeProperty (s_XDisplay,
		Xid,
		aNetWmUserTime,
		XA_CARDINAL, 32, PropModeReplace,
		(guchar *)&iTimeStamp, 1);
}

void cairo_dock_set_strut_partial (Window Xid, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x)
{
	g_return_if_fail (Xid > 0);
	//g_print ("%s (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", __func__, left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);
	gulong iGeometryStrut[12] = {left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x};

	XChangeProperty (s_XDisplay,
		Xid,
		XInternAtom (s_XDisplay, "_NET_WM_STRUT_PARTIAL", False),
		XA_CARDINAL, 32, PropModeReplace,
		(guchar *) iGeometryStrut, 12);
	
	Window root = DefaultRootWindow (s_XDisplay);
	cairo_dock_set_xwindow_timestamp (Xid, cairo_dock_get_xwindow_timestamp (root));
}

void cairo_dock_set_xwindow_mask (Window Xid, long iMask)
{
	//StructureNotifyMask | /*ResizeRedirectMask*/
	//SubstructureRedirectMask |
	//SubstructureNotifyMask |  // place sur le root, donne les evenements Map, Unmap, Destroy, Create de chaque fenetre.
	//PropertyChangeMask
	XSelectInput (s_XDisplay, Xid, iMask);  // c'est le 'event_mask' d'un XSetWindowAttributes.
}

/*void cairo_dock_set_xwindow_type_hint (int Xid, const gchar *cWindowTypeName)
{
	g_return_if_fail (Xid > 0);
	
	gulong iWindowType = XInternAtom (s_XDisplay, cWindowTypeName, False);
	cd_debug ("%s (%d, %s=%d)", __func__, Xid, cWindowTypeName, iWindowType);
	
	XChangeProperty (s_XDisplay,
		Xid,
		s_aNetWmWindowType,
		XA_ATOM, 32, PropModeReplace,
		(guchar *) &iWindowType, 1);
}*/


void cairo_dock_set_xicon_geometry (int Xid, int iX, int iY, int iWidth, int iHeight)
{
	g_return_if_fail (Xid > 0);

	gulong iIconGeometry[4] = {iX, iY, iWidth, iHeight};
	
	if (iWidth == 0 || iHeight == 0)
		XDeleteProperty (s_XDisplay,
			Xid,
			s_aNetWmIconGeometry);
	else
		XChangeProperty (s_XDisplay,
			Xid,
			s_aNetWmIconGeometry,
			XA_CARDINAL, 32, PropModeReplace,
			(guchar *) iIconGeometry, 4);
}



void cairo_dock_close_xwindow (Window Xid)
{
	//g_print ("%s (%d)\n", __func__, Xid);
	g_return_if_fail (Xid > 0);
	
	XEvent xClientMessage;
	
	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = Xid;
	xClientMessage.xclient.message_type = XInternAtom (s_XDisplay, "_NET_CLOSE_WINDOW", False);
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = cairo_dock_get_xwindow_timestamp (Xid);  // timestamp
	xClientMessage.xclient.data.l[1] = 2;  // 2 <=> pagers and other Clients that represent direct user actions.
	xClientMessage.xclient.data.l[2] = 0;
	xClientMessage.xclient.data.l[3] = 0;
	xClientMessage.xclient.data.l[4] = 0;

	Window root = DefaultRootWindow (s_XDisplay);
	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
	XFlush (s_XDisplay);
	//cairo_dock_set_xwindow_timestamp (Xid, cairo_dock_get_xwindow_timestamp (root));
}

void cairo_dock_kill_xwindow (Window Xid)
{
	g_return_if_fail (Xid > 0);
	XKillClient (s_XDisplay, Xid);
	XFlush (s_XDisplay);
}

void cairo_dock_show_xwindow (Window Xid)
{
	g_return_if_fail (Xid > 0);
	XEvent xClientMessage;
	Window root = DefaultRootWindow (s_XDisplay);

	//\______________ On se deplace sur le bureau de la fenetre a afficher (autrement Metacity deplacera la fenetre sur le bureau actuel).
	int iDesktopNumber = cairo_dock_get_xwindow_desktop (Xid);
	gboolean bIsSticky = cairo_dock_xwindow_is_sticky (Xid);
	if (iDesktopNumber >= 0 && !bIsSticky)  // don't move if the window is sticky, since it is on every desktops
		cairo_dock_set_current_desktop (iDesktopNumber);

	//\______________ On active la fenetre.
	//XMapRaised (s_XDisplay, Xid);  // on la mappe, pour les cas ou elle etait en zone de notification. Malheuresement, la zone de notif de gnome est bugguee, et reduit la fenetre aussitot qu'on l'a mappee :-(
	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = Xid;
	xClientMessage.xclient.message_type = s_aNetActiveWindow;
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = 2;  // source indication
	xClientMessage.xclient.data.l[1] = 0;  // timestamp
	xClientMessage.xclient.data.l[2] = 0;  // requestor's currently active window, 0 if none
	xClientMessage.xclient.data.l[3] = 0;
	xClientMessage.xclient.data.l[4] = 0;
	
	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
	XFlush (s_XDisplay);
}

void cairo_dock_minimize_xwindow (Window Xid)
{
	g_return_if_fail (Xid > 0);
	XIconifyWindow (s_XDisplay, Xid, DefaultScreen (s_XDisplay));
	XFlush (s_XDisplay);
}

void cairo_dock_lower_xwindow (Window Xid)
{
	g_return_if_fail (Xid > 0);
	XLowerWindow (s_XDisplay, Xid);
	XFlush (s_XDisplay);
}



static void _cairo_dock_change_window_state (Window Xid, gulong iNewValue, Atom iProperty1, Atom iProperty2)
{
	g_return_if_fail (Xid > 0);
	XEvent xClientMessage;

	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = Xid;
	xClientMessage.xclient.message_type = s_aNetWmState;
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = iNewValue;
	xClientMessage.xclient.data.l[1] = iProperty1;
	xClientMessage.xclient.data.l[2] = iProperty2;
	xClientMessage.xclient.data.l[3] = 2;
	xClientMessage.xclient.data.l[4] = 0;

	Window root = DefaultRootWindow (s_XDisplay);
	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);

	cairo_dock_set_xwindow_timestamp (Xid, cairo_dock_get_xwindow_timestamp (root));
	XFlush (s_XDisplay);
}
void cairo_dock_maximize_xwindow (Window Xid, gboolean bMaximize)
{
	_cairo_dock_change_window_state (Xid, bMaximize, s_aNetWmMaximizedVert, s_aNetWmMaximizedHoriz);
}

void cairo_dock_set_xwindow_fullscreen (Window Xid, gboolean bFullScreen)
{
	_cairo_dock_change_window_state (Xid, bFullScreen, s_aNetWmFullScreen, 0);
}

void cairo_dock_set_xwindow_above (Window Xid, gboolean bAbove)
{
	_cairo_dock_change_window_state (Xid, bAbove, s_aNetWmAbove, 0);
}

void cairo_dock_set_xwindow_sticky (Window Xid, gboolean bSticky)
{
	_cairo_dock_change_window_state (Xid, bSticky, s_aNetWmSticky, 0);
}


void cairo_dock_move_xwindow_to_absolute_position (Window Xid, int iDesktopNumber, int iPositionX, int iPositionY)  // dans le referentiel du viewport courant.
{
	g_return_if_fail (Xid > 0);
	XEvent xClientMessage;

	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = Xid;
	xClientMessage.xclient.message_type = XInternAtom (s_XDisplay, "_NET_WM_DESKTOP", False);
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = iDesktopNumber;
	xClientMessage.xclient.data.l[1] = 2;
	xClientMessage.xclient.data.l[2] = 0;
	xClientMessage.xclient.data.l[3] = 0;
	xClientMessage.xclient.data.l[4] = 0;

	Window root = DefaultRootWindow (s_XDisplay);
	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
	
	xClientMessage.xclient.type = ClientMessage;
	xClientMessage.xclient.serial = 0;
	xClientMessage.xclient.send_event = True;
	xClientMessage.xclient.display = s_XDisplay;
	xClientMessage.xclient.window = Xid;
	xClientMessage.xclient.message_type = XInternAtom (s_XDisplay, "_NET_MOVERESIZE_WINDOW", False);
	xClientMessage.xclient.format = 32;
	xClientMessage.xclient.data.l[0] = StaticGravity | (1 << 8) | (1 << 9) | (0 << 10) | (0 << 11);
	xClientMessage.xclient.data.l[1] =  iPositionX;  // coordonnees dans le referentiel du viewport desire.
	xClientMessage.xclient.data.l[2] =  iPositionY;
	xClientMessage.xclient.data.l[3] = 0;
	xClientMessage.xclient.data.l[4] = 0;
	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
	XFlush (s_XDisplay);

	//cairo_dock_set_xwindow_timestamp (Xid, cairo_dock_get_xwindow_timestamp (root));
}

static void cairo_dock_get_xwindow_position_on_its_viewport (Window Xid, int *iRelativePositionX, int *iRelativePositionY)
{
	int iLocalPositionX, iLocalPositionY, iWidthExtent=1, iHeightExtent=1;  // we don't care wbout the size
	cairo_dock_get_xwindow_geometry (Xid, &iLocalPositionX, &iLocalPositionY, &iWidthExtent, &iHeightExtent);
	
	while (iLocalPositionX < 0)  // on passe au referentiel du viewport de la fenetre; inutile de connaitre sa position, puisqu'ils ont tous la meme taille.
		iLocalPositionX += gldi_desktop_get_width();
	while (iLocalPositionX >= gldi_desktop_get_width())
		iLocalPositionX -= gldi_desktop_get_width();
	while (iLocalPositionY < 0)
		iLocalPositionY += gldi_desktop_get_height();
	while (iLocalPositionY >= gldi_desktop_get_height())
		iLocalPositionY -= gldi_desktop_get_height();
	
	*iRelativePositionX = iLocalPositionX;
	*iRelativePositionY = iLocalPositionY;
	//cd_debug ("position relative : (%d;%d) taille : %dx%d", *iRelativePositionX, *iRelativePositionY, iWidthExtent, iHeightExtent);
}
void cairo_dock_move_xwindow_to_nth_desktop (Window Xid, int iDesktopNumber, int iDesktopViewportX, int iDesktopViewportY)
{
	g_return_if_fail (Xid > 0);
	int iRelativePositionX, iRelativePositionY;
	cairo_dock_get_xwindow_position_on_its_viewport (Xid, &iRelativePositionX, &iRelativePositionY);
	
	cairo_dock_move_xwindow_to_absolute_position (Xid, iDesktopNumber, iDesktopViewportX + iRelativePositionX, iDesktopViewportY + iRelativePositionY);
}

void cairo_dock_set_xwindow_border (Window Xid, gboolean bWithBorder)
{
	MwmHints mwmhints;
	Atom prop;
	memset(&mwmhints, 0, sizeof(mwmhints));
	prop = XInternAtom(s_XDisplay, "_MOTIF_WM_HINTS", False);
	mwmhints.flags = MWM_HINTS_DECORATIONS;
	mwmhints.decorations = bWithBorder;
	XChangeProperty (s_XDisplay, Xid, prop,
		prop, 32, PropModeReplace,
		(unsigned char *) &mwmhints,
		PROP_MWM_HINTS_ELEMENTS);
}


gulong cairo_dock_get_xwindow_timestamp (Window Xid)
{
	g_return_val_if_fail (Xid > 0, 0);
	Atom aNetWmUserTime = XInternAtom (s_XDisplay, "_NET_WM_USER_TIME", False);
	gulong iLeftBytes, iBufferNbElements = 0;
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	gulong *pTimeBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, aNetWmUserTime, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pTimeBuffer);
	gulong iTimeStamp = 0;
	if (iBufferNbElements > 0)
		iTimeStamp = *pTimeBuffer;
	XFree (pTimeBuffer);
	return iTimeStamp;
}

gchar *cairo_dock_get_xwindow_name (Window Xid, gboolean bSearchWmName)
{
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements=0;
	guchar *pNameBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmName, 0, G_MAXULONG, False, s_aUtf8String, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, &pNameBuffer);  // on cherche en priorite le nom en UTF8, car on est notifie des 2, mais il vaut mieux eviter le WM_NAME qui, ne l'etant pas, contient des caracteres bizarres qu'on ne peut pas convertir avec g_locale_to_utf8, puisque notre locale _est_ UTF8.
	if (iBufferNbElements == 0 && bSearchWmName)
		XGetWindowProperty (s_XDisplay, Xid, s_aWmName, 0, G_MAXULONG, False, s_aString, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, &pNameBuffer);
	
	gchar *cName = NULL;
	if (iBufferNbElements > 0)
	{
		cName = g_strdup ((gchar *)pNameBuffer);
		XFree (pNameBuffer);
	}
	return cName;
}

gchar *cairo_dock_get_xwindow_class (Window Xid, gchar **cWMClass)
{
	XClassHint *pClassHint = XAllocClassHint ();
	gchar *cClass = NULL;
	if (XGetClassHint (s_XDisplay, Xid, pClassHint) != 0 && pClassHint->res_class)
	{
		cClass = gldi_window_parse_class(pClassHint->res_class, pClassHint->res_name);
		if (cWMClass) *cWMClass = g_strdup (pClassHint->res_class);
		XFree (pClassHint->res_name);
		XFree (pClassHint->res_class);
		XFree (pClassHint);
	}
	return cClass;
}

gboolean cairo_dock_xwindow_is_maximized (Window Xid)
{
	g_return_val_if_fail (Xid > 0, FALSE);

	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXStateBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmState, 0, G_MAXULONG, False, XA_ATOM, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXStateBuffer);
	int iIsMaximized = 0;
	if (iBufferNbElements > 0)
	{
		guint i;
		for (i = 0; i < iBufferNbElements && iIsMaximized < 2; i ++)
		{
			if (pXStateBuffer[i] == s_aNetWmMaximizedVert)
				iIsMaximized ++;
			if (pXStateBuffer[i] == s_aNetWmMaximizedHoriz)
				iIsMaximized ++;
		}
	}
	XFree (pXStateBuffer);

	return (iIsMaximized == 2);
}

static gboolean _cairo_dock_window_is_in_state (Window Xid, Atom iState)
{
	g_return_val_if_fail (Xid > 0, FALSE);
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXStateBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmState, 0, G_MAXULONG, False, XA_ATOM, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXStateBuffer);

	gboolean bIsInState = FALSE;
	if (iBufferNbElements > 0)
	{
		guint i;
		for (i = 0; i < iBufferNbElements; i ++)
		{
			if (pXStateBuffer[i] == iState)
			{
				bIsInState = TRUE;
				break;
			}
		}
	}

	XFree (pXStateBuffer);
	return bIsInState;
}

gboolean cairo_dock_xwindow_is_fullscreen (Window Xid)
{
	return _cairo_dock_window_is_in_state (Xid, s_aNetWmFullScreen);
}

gboolean cairo_dock_xwindow_skip_taskbar (Window Xid)
{
	return _cairo_dock_window_is_in_state (Xid, s_aNetWmSkipTaskbar);
}

gboolean cairo_dock_xwindow_is_sticky (Window Xid)
{
	return _cairo_dock_window_is_in_state (Xid, s_aNetWmSticky);
}

void cairo_dock_xwindow_is_above_or_below (Window Xid, gboolean *bIsAbove, gboolean *bIsBelow)
{
	g_return_if_fail (Xid > 0);
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXStateBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmState, 0, G_MAXULONG, False, XA_ATOM, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXStateBuffer);

	if (iBufferNbElements > 0)
	{
		guint i;
		//g_print ("iBufferNbElements : %d (%d;%d)\n", iBufferNbElements, s_aNetWmAbove, s_aNetWmBelow);
		for (i = 0; i < iBufferNbElements; i ++)
		{
			//g_print (" - %d\n", pXStateBuffer[i]);
			if (pXStateBuffer[i] == s_aNetWmAbove)
			{
				*bIsAbove = TRUE;
				*bIsBelow = FALSE;
				break;
			}
			else if (pXStateBuffer[i] == s_aNetWmBelow)
			{
				*bIsAbove = FALSE;
				*bIsBelow = TRUE;
				break;
			}
		}
	}

	XFree (pXStateBuffer);
}

gboolean cairo_dock_xwindow_is_fullscreen_or_hidden_or_maximized (Window Xid, gboolean *bIsFullScreen, gboolean *bIsHidden, gboolean *bIsMaximized, gboolean *bDemandsAttention, gboolean *bIsSticky)
{
	g_return_val_if_fail (Xid > 0, FALSE);
	//cd_debug ("%s (%d)", __func__, Xid);
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXStateBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmState, 0, G_MAXULONG, False, XA_ATOM, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXStateBuffer);
	
	gboolean bValid = TRUE;
	*bIsFullScreen = FALSE;
	*bIsHidden = FALSE;
	*bIsMaximized = FALSE;
	if (bDemandsAttention != NULL)
		*bDemandsAttention = FALSE;
	if (bIsSticky != NULL)
		*bIsSticky = FALSE;
	if (iBufferNbElements > 0)
	{
		guint i, iNbMaximizedDimensions = 0;
		for (i = 0; i < iBufferNbElements; i ++)
		{
			if (pXStateBuffer[i] == s_aNetWmFullScreen)
			{
				*bIsFullScreen = TRUE;
			}
			else if (pXStateBuffer[i] == s_aNetWmHidden)
			{
				*bIsHidden = TRUE;
			}
			else if (pXStateBuffer[i] == s_aNetWmMaximizedVert)
			{
				iNbMaximizedDimensions ++;
				if (iNbMaximizedDimensions == 2)
					*bIsMaximized = TRUE;
			}
			else if (pXStateBuffer[i] == s_aNetWmMaximizedHoriz)
			{
				iNbMaximizedDimensions ++;
				if (iNbMaximizedDimensions == 2)
					*bIsMaximized = TRUE;
			}
			else if (pXStateBuffer[i] == s_aNetWmDemandsAttention && bDemandsAttention != NULL)
			{
				*bDemandsAttention = TRUE;
			}
			else if (pXStateBuffer[i] == s_aNetWmSticky && bIsSticky != NULL)
			{
				*bIsSticky = TRUE;
			}
			
			else if (pXStateBuffer[i] == s_aNetWmSkipTaskbar)
			{
				cd_debug ("this appli should not be in taskbar anymore");
				bValid = FALSE;
			}
		}
	}
	
	XFree (pXStateBuffer);
	return bValid;
}

void cairo_dock_xwindow_can_minimize_maximized_close (Window Xid, gboolean *bCanMinimize, gboolean *bCanMaximize, gboolean *bCanClose)
{
	g_return_if_fail (Xid > 0);

	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXStateBuffer = NULL;
	
	XGetWindowProperty (s_XDisplay,
		Xid, s_aNetWMAllowedActions, 0, G_MAXULONG, False, XA_ATOM,
		&aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXStateBuffer);

	*bCanMinimize = FALSE;
	*bCanMaximize = FALSE;
	*bCanClose = FALSE;

	if (iBufferNbElements > 0)
	{
		guint i;
		for (i = 0; i < iBufferNbElements; i ++)
		{
			if (pXStateBuffer[i] == s_aNetWMActionMinimize)
			{
				*bCanMinimize = TRUE;
			}
			else if (pXStateBuffer[i] == s_aNetWMActionMaximizeHorz || pXStateBuffer[i] == s_aNetWMActionMaximizeVert)
			{
				*bCanMaximize = TRUE;
			}
			else if (pXStateBuffer[i] == s_aNetWMActionClose)
			{
				*bCanClose = TRUE;
			}
		}
	}

	XFree (pXStateBuffer);
}


int cairo_dock_get_xwindow_desktop (Window Xid)
{
	int iDesktopNumber;
	gulong iLeftBytes, iBufferNbElements = 0;
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	gulong *pBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmDesktop, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pBuffer);
	if (iBufferNbElements > 0)
		iDesktopNumber = *pBuffer;
	else
		iDesktopNumber = 0;
	
	XFree (pBuffer);
	return iDesktopNumber;
}

void cairo_dock_get_xwindow_geometry (Window Xid, int *iLocalPositionX, int *iLocalPositionY, int *iWidthExtent, int *iHeightExtent)  // renvoie les coordonnees du coin haut gauche dans le referentiel du viewport actuel. // sous KDE, x et y sont toujours nuls ! (meme avec XGetWindowAttributes).
{
	// get the geometry from X.
	unsigned int width_return=0, height_return=0;
	if (*iWidthExtent == 0 || *iHeightExtent == 0)  // if the size is already set, don't update it.
	{
		Window root_return;
		int x_return=1, y_return=1;
		unsigned int border_width_return, depth_return;
		XGetGeometry (s_XDisplay, Xid,
			&root_return,
			&x_return, &y_return,
			&width_return, &height_return,
			&border_width_return, &depth_return);  // renvoie les coordonnees du coin haut gauche dans le referentiel du viewport actuel.
		*iWidthExtent = width_return;
		*iHeightExtent = height_return;
	}
	
	// make another round trip to the server to query the coordinates of the window relatively to the root window (which basically gives us the (x,y) of the window); we need to do this to workaround a strange X bug: x_return and y_return are wrong (0,0, modulo the borders) (on Ubuntu 11.10 + Compiz 0.9/Metacity, not on Debian 6 + Compiz 0.8).
	Window root = DefaultRootWindow (s_XDisplay);
	int dest_x_return, dest_y_return;
	Window child_return;
	XTranslateCoordinates (s_XDisplay, Xid, root, 0, 0, &dest_x_return, &dest_y_return, &child_return);  // translate into the coordinate space of the root window. we need to do this, because (x_return,;y_return) is always (0;0)
	
	// take into account the window borders
	int left=0, right=0, top=0, bottom=0;
	gulong iLeftBytes, iBufferNbElements = 0;
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	gulong *pBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, XInternAtom (s_XDisplay, "_NET_FRAME_EXTENTS", False), 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pBuffer);
	if (iBufferNbElements > 3)
	{
		left=pBuffer[0], right=pBuffer[1], top=pBuffer[2], bottom=pBuffer[3];
	}
	if (pBuffer)
		XFree (pBuffer);
	
	*iLocalPositionX = dest_x_return - left;
	*iLocalPositionY = dest_y_return - top;
	*iWidthExtent += left + right;
	*iHeightExtent += top + bottom;
}


Window *cairo_dock_get_windows_list (gulong *iNbWindows, gboolean bStackOrder)
{
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	Window *XidList = NULL;

	Window root = DefaultRootWindow (s_XDisplay);
	gulong iLeftBytes;
	XGetWindowProperty (s_XDisplay, root, (bStackOrder ? s_aNetClientListStacking : s_aNetClientList), 0, G_MAXLONG, False, XA_WINDOW, &aReturnedType, &aReturnedFormat, iNbWindows, &iLeftBytes, (guchar **)&XidList);
	return XidList;
}

Window cairo_dock_get_active_xwindow (void)
{
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	Window *pXBuffer = NULL;
	Window root = DefaultRootWindow (s_XDisplay);
	XGetWindowProperty (s_XDisplay, root, s_aNetActiveWindow, 0, G_MAXULONG, False, XA_WINDOW, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXBuffer);

	Window xActiveWindow = (iBufferNbElements > 0 && pXBuffer != NULL ? pXBuffer[0] : 0);
	XFree (pXBuffer);
	return xActiveWindow;
}



cairo_surface_t *cairo_dock_create_surface_from_xwindow (Window Xid, int iWidth, int iHeight)
{
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXIconBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmIcon, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXIconBuffer);

	if (iBufferNbElements > 2)
	{
		cairo_surface_t *pNewSurface = cairo_dock_create_surface_from_xicon_buffer (pXIconBuffer,
			iBufferNbElements,
			iWidth,
			iHeight);
		XFree (pXIconBuffer);
		return pNewSurface;
	}
	else  // sinon on tente avec l'icone eventuellement presente dans les WMHints.
	{
		XWMHints *pWMHints = XGetWMHints (s_XDisplay, Xid);
		if (pWMHints == NULL)
		{
			cd_debug ("  aucun WMHints");
			return NULL;
		}
		//\__________________ On recupere les donnees dans un  pixbuf.
		GdkPixbuf *pIconPixbuf = NULL;
		if (pWMHints->flags & IconWindowHint)
		{
			Window XIconID = pWMHints->icon_window;
			cd_debug ("  pas de _NET_WM_ICON, mais une fenetre (ID:%d)", XIconID);
			Pixmap iPixmap = cairo_dock_get_window_background_pixmap (XIconID);
			pIconPixbuf = cairo_dock_get_pixbuf_from_pixmap (iPixmap, TRUE);  /// A valider ...
		}
		else if (pWMHints->flags & IconPixmapHint)
		{
			cd_debug ("  pas de _NET_WM_ICON, mais un pixmap");
			Pixmap XPixmapID = pWMHints->icon_pixmap;
			pIconPixbuf = cairo_dock_get_pixbuf_from_pixmap (XPixmapID, TRUE);

			//\____________________ On lui applique le masque de transparence s'il existe.
			if (pWMHints->flags & IconMaskHint)
			{
				Pixmap XPixmapMaskID = pWMHints->icon_mask;
				GdkPixbuf *pMaskPixbuf = cairo_dock_get_pixbuf_from_pixmap (XPixmapMaskID, FALSE);

				int iNbChannels = gdk_pixbuf_get_n_channels (pIconPixbuf);
				int iRowstride = gdk_pixbuf_get_rowstride (pIconPixbuf);
				guchar *p, *pixels = gdk_pixbuf_get_pixels (pIconPixbuf);

				int iNbChannelsMask = gdk_pixbuf_get_n_channels (pMaskPixbuf);
				int iRowstrideMask = gdk_pixbuf_get_rowstride (pMaskPixbuf);
				guchar *q, *pixelsMask = gdk_pixbuf_get_pixels (pMaskPixbuf);

				int w = MIN (gdk_pixbuf_get_width (pIconPixbuf), gdk_pixbuf_get_width (pMaskPixbuf));
				int h = MIN (gdk_pixbuf_get_height (pIconPixbuf), gdk_pixbuf_get_height (pMaskPixbuf));
				int x, y;
				for (y = 0; y < h; y ++)
				{
					for (x = 0; x < w; x ++)
					{
						p = pixels + y * iRowstride + x * iNbChannels;
						q = pixelsMask + y * iRowstrideMask + x * iNbChannelsMask;
						if (q[0] == 0)
							p[3] = 0;
						else
							p[3] = 255;
					}
				}

				g_object_unref (pMaskPixbuf);
			}
		}
		XFree (pWMHints);

		//\____________________ On cree la surface.
		if (pIconPixbuf != NULL)
		{
			double fWidth, fHeight;
			cairo_surface_t *pNewSurface = cairo_dock_create_surface_from_pixbuf (pIconPixbuf,
				1.,
				iWidth,
				iHeight,
				CAIRO_DOCK_KEEP_RATIO | CAIRO_DOCK_FILL_SPACE,
				&fWidth,
				&fHeight,
				NULL, NULL);

			g_object_unref (pIconPixbuf);
			return pNewSurface;
		}
		return NULL;
	}
}

cairo_surface_t *cairo_dock_create_surface_from_xpixmap (Pixmap Xid, int iWidth, int iHeight)
{
	g_return_val_if_fail (Xid > 0, NULL);
	GdkPixbuf *pPixbuf = cairo_dock_get_pixbuf_from_pixmap (Xid, TRUE);
	if (pPixbuf == NULL)
	{
		cd_warning ("No thumbnail available.\nEither the WM doesn't support this functionnality, or the window was minimized when the dock has been launched.");
		return NULL;
	}
	
	cd_debug ("window pixmap : %dx%d", gdk_pixbuf_get_width (pPixbuf), gdk_pixbuf_get_height (pPixbuf));
	double fWidth, fHeight;
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_pixbuf (pPixbuf,
		1.,
		iWidth, iHeight,
		CAIRO_DOCK_KEEP_RATIO | CAIRO_DOCK_FILL_SPACE,  // on conserve le ratio de la fenetre, tout en gardant la taille habituelle des icones d'appli.
		&fWidth, &fHeight,
		NULL, NULL);
	g_object_unref (pPixbuf);
	return pSurface;
}

//typedef void (*GLXBindTexImageProc) (Display *display, GLXDrawable drawable, int buffer, int *attribList);
//typedef void (*GLXReleaseTexImageProc) (Display *display, GLXDrawable drawable, int buffer);

// Bind redirected window to texture:
GLuint cairo_dock_texture_from_pixmap (Window Xid, Pixmap iBackingPixmap)
{
	#ifdef HAVE_GLX
	if (!g_bEasterEggs)
		return 0;  /// works for some windows (gnome-terminal) but not for all ... still need to figure why.
	
	if (!iBackingPixmap || ! g_openglConfig.bTextureFromPixmapAvailable)
		return 0;
	
	Display *display = s_XDisplay;
	XWindowAttributes attrib;
	XGetWindowAttributes (display, Xid, &attrib);
	
	VisualID visualid = XVisualIDFromVisual (attrib.visual);
	
	int nfbconfigs;
	int screen = 0;
	GLXFBConfig *fbconfigs = glXGetFBConfigs (display, screen, &nfbconfigs);
	
	GLfloat top=0., bottom=0.;
	XVisualInfo *visinfo;
	int value;
	int i;
	for (i = 0; i < nfbconfigs; i++)
	{
		visinfo = glXGetVisualFromFBConfig (display, fbconfigs[i]);
		if (!visinfo || visinfo->visualid != visualid)
			continue;
	
		glXGetFBConfigAttrib (display, fbconfigs[i], GLX_DRAWABLE_TYPE, &value);
		if (!(value & GLX_PIXMAP_BIT))
			continue;
	
		glXGetFBConfigAttrib (display, fbconfigs[i],
			GLX_BIND_TO_TEXTURE_TARGETS_EXT,
			&value);
		if (!(value & GLX_TEXTURE_2D_BIT_EXT))
			continue;
		
		glXGetFBConfigAttrib (display, fbconfigs[i],
			GLX_BIND_TO_TEXTURE_RGBA_EXT,
			&value);
		if (value == FALSE)
		{
			glXGetFBConfigAttrib (display, fbconfigs[i],
				GLX_BIND_TO_TEXTURE_RGB_EXT,
				&value);
			if (value == FALSE)
				continue;
		}
		
		glXGetFBConfigAttrib (display, fbconfigs[i],
			GLX_Y_INVERTED_EXT,
			&value);
		if (value == TRUE)
		{
			top = 0.0f;
			bottom = 1.0f;
		}
		else
		{
			top = 1.0f;
			bottom = 0.0f;
		}
		
		break;
	}
	
	if (i == nfbconfigs)
	{
		cd_warning ("No FB Config found");
		return 0;
	}
	
	int pixmapAttribs[5] = { GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
		GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
		None };
	GLXPixmap glxpixmap = glXCreatePixmap (display, fbconfigs[i], iBackingPixmap, pixmapAttribs);
	g_return_val_if_fail (glxpixmap != 0, 0);
	
	GLuint texture;
	glEnable (GL_TEXTURE_2D);
	glGenTextures (1, &texture);
	glBindTexture (GL_TEXTURE_2D, texture);
	
	g_openglConfig.bindTexImage (display, glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
	
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MIN_FILTER,
		g_bEasterEggs ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	if (g_bEasterEggs)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	// draw using iBackingPixmap as texture
	glBegin (GL_QUADS);
	
	glTexCoord2d (0.0f, bottom);
	glVertex2d (0.0f, 0.0f);
	
	glTexCoord2d (0.0f, top);
	glVertex2d (0.0f, attrib.height);
	
	glTexCoord2d (1.0f, top);
	glVertex2d (attrib.width, attrib.height);
	
	glTexCoord2d (1.0f, bottom);
	glVertex2d (attrib.width, 0.0f);
	
	glEnd ();
	glDisable (GL_TEXTURE_2D);
	
	g_openglConfig.releaseTexImage (display, glxpixmap, GLX_FRONT_LEFT_EXT);
	glXDestroyGLXPixmap (display, glxpixmap);
	return texture;
	
	#else
	(void)Xid;  // avoid unused warning
	(void)iBackingPixmap;
	return 0;
	#endif
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

gboolean cairo_dock_get_xwindow_type (Window Xid, Window *pTransientFor)
{
	gboolean bKeep = FALSE;  // we only want to know if we can display this window in the dock or not, so a boolean is enough.
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements;
	gulong *pTypeBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmWindowType, 0, G_MAXULONG, False, XA_ATOM, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pTypeBuffer);
	if (iBufferNbElements != 0)
	{
		guint i;
		for (i = 0; i < iBufferNbElements; i ++)  // The Client SHOULD specify window types in order of preference (the first being most preferable) but MUST include at least one of the basic window type atoms.
		{
			if (pTypeBuffer[i] == s_aNetWmWindowTypeNormal)  // normal window -> take it
			{
				bKeep = TRUE;
				break;
			}
			if (pTypeBuffer[i] == s_aNetWmWindowTypeDialog)  // dialog -> skip modal dialog, because we can't act on it independantly from the parent window (it's most probably a dialog box like an open/save dialog)
			{
				XGetTransientForHint (s_XDisplay, Xid, pTransientFor);  // maybe we should also get the _NET_WM_STATE_MODAL property, although if a dialog is set modal but not transient, that would probably be an error from the application.
				if (*pTransientFor == None)
				{
					bKeep = TRUE;
					break;
				}  // else it's a transient dialog, don't keep it, unless it also has the "normal" type further in the buffer.
			}  // skip any other type (dock, menu, etc)
			else if (pTypeBuffer[i] == s_aNetWmWindowTypeDock)  // workaround for the Unity-panel: if the type 'dock' is present, don't look further (as they add the 'normal' type too, which is non-sense).
			{
				break;
			}
		}
		XFree (pTypeBuffer);
	}
	else  // no type, take it by default, unless it's transient.
	{
		XGetTransientForHint (s_XDisplay, Xid, pTransientFor);
		bKeep = (*pTransientFor == None);
	}
	return bKeep;
}

#endif
