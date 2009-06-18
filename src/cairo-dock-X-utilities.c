/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef HAVE_XEXTEND
#include <X11/extensions/Xcomposite.h>
//#include <X11/extensions/Xdamage.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/shape.h>
#endif

#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-X-utilities.h"

extern int g_iNbDesktops;
extern int g_iNbViewportX,g_iNbViewportY ;
extern int g_iScreenWidth[2], g_iScreenHeight[2];
extern int g_iXScreenWidth[2], g_iXScreenHeight[2];
extern int g_iScreenOffsetX, g_iScreenOffsetY;
static gboolean s_bUseXComposite = TRUE;
static gboolean s_bUseXTest = TRUE;
static gboolean s_bUseXinerama = TRUE;

//extern int g_iDamageEvent;

static Display *s_XDisplay = NULL;
static Atom s_aNetWmWindowType;
static Atom s_aNetWmWindowTypeNormal;
static Atom s_aNetWmWindowTypeUtility;
static Atom s_aNetWmWindowTypeDock;
static Atom s_aNetWmIconGeometry;
static Atom s_aNetCurrentDesktop;
static Atom s_aNetDesktopViewport;
static Atom s_aNetDesktopGeometry;
static Atom s_aNetNbDesktops;
static Atom s_aRootMapID;

static int _cairo_dock_xerror_handler (Display * pDisplay, XErrorEvent *pXError)
{
	cd_debug ("Erreur (%d, %d, %d) lors d'une requete X sur %d", pXError->error_code, pXError->request_code, pXError->minor_code, pXError->resourceid);
	return 0;
}
void cairo_dock_initialize_X_support (void)
{
	s_XDisplay = XOpenDisplay (0);
	g_return_if_fail (s_XDisplay != NULL);
	
	XSetErrorHandler (_cairo_dock_xerror_handler);
	
	cairo_dock_support_X_extension ();
	
	s_aNetWmWindowType = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE", False);
	s_aNetWmWindowTypeNormal = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	s_aNetWmWindowTypeUtility = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	s_aNetWmWindowTypeDock = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_DOCK", False);
	s_aNetWmIconGeometry = XInternAtom (s_XDisplay, "_NET_WM_ICON_GEOMETRY", False);
	s_aNetCurrentDesktop = XInternAtom (s_XDisplay, "_NET_CURRENT_DESKTOP", False);
	s_aNetDesktopViewport = XInternAtom (s_XDisplay, "_NET_DESKTOP_VIEWPORT", False);
	s_aNetDesktopGeometry = XInternAtom (s_XDisplay, "_NET_DESKTOP_GEOMETRY", False);
	s_aNetNbDesktops = XInternAtom (s_XDisplay, "_NET_NUMBER_OF_DESKTOPS", False);
	s_aRootMapID = XInternAtom (s_XDisplay, "_XROOTPMAP_ID", False);
	
	Screen *XScreen = XDefaultScreenOfDisplay (s_XDisplay);
	g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] = WidthOfScreen (XScreen);
	g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] = HeightOfScreen (XScreen);
	g_iXScreenWidth[CAIRO_DOCK_VERTICAL] = g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
	g_iXScreenHeight[CAIRO_DOCK_VERTICAL] = g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
	
	g_iScreenWidth[CAIRO_DOCK_HORIZONTAL] = WidthOfScreen (XScreen);
	g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] = HeightOfScreen (XScreen);
	g_iScreenWidth[CAIRO_DOCK_VERTICAL] = g_iScreenHeight[CAIRO_DOCK_HORIZONTAL];
	g_iScreenHeight[CAIRO_DOCK_VERTICAL] = g_iScreenWidth[CAIRO_DOCK_HORIZONTAL];
	
	g_iNbDesktops = cairo_dock_get_nb_desktops ();
	cairo_dock_get_nb_viewports (&g_iNbViewportX, &g_iNbViewportY);
	
	cairo_dock_initialize_class_manager ();
	cairo_dock_initialize_application_manager (s_XDisplay);
	cairo_dock_initialize_application_factory (s_XDisplay);
	
	/*GdkWindow * root;
	GdkPixmap* pix = NULL;
	GdkPixbuf* img;
	GdkScreen* screen = gdk_screen_get_default ();
	root = gdk_screen_get_root_window( screen );
	GdkColor desktopBg1;
	desktopBg1.red = 0;
	desktopBg1.green = 0;
	desktopBg1.blue = 65535;
	
	int screenw, screenh;
	int imgw, imgh, x = 0, y = 0;
	screenw = gdk_screen_get_width( screen );
	screenh = gdk_screen_get_height( screen );
	img = gdk_pixbuf_new_from_file_at_scale (
		"/usr/share/backgrounds/space-01.jpg",
		screenw, screenh,
		TRUE, NULL);
	if ( img )
	{
		g_print ("WALLPAPER (%dx%d)\n", screenw, screenh);
		GdkGC * gc;
		pix = gdk_pixmap_new( root, screenw, screenh, -1 );
		imgw = gdk_pixbuf_get_width( img );
		imgh = gdk_pixbuf_get_height( img );
		if ( imgw == screenw )
		{
			//center vertically 
			y = ( screenh - imgh ) / 2;
		}
		else
		{
			// center horizontally 
			x = ( screenw - imgw ) / 2;
		}
		// FIXME: fill the blank area with bg color.
		gc = gdk_gc_new( pix );
		gdk_gc_set_rgb_fg_color( gc, &desktopBg1 );
		gdk_gc_set_rgb_bg_color( gc, &desktopBg1 );
		gdk_gc_set_fill( gc, GDK_SOLID );
		// fill the whole pixmap is not efficient at all!!! 
		gdk_draw_rectangle( pix, gc, TRUE,
			0, 0, screenw, screenh );
		g_object_unref( G_OBJECT( gc ) );
		gdk_draw_pixbuf( pix, NULL, img, 0, 0, x, y,
			imgw, imgh,
			GDK_RGB_DITHER_NONE, 0, 0 );
		gdk_pixbuf_unref( img );
		
		gdk_window_set_back_pixmap( root, pix, FALSE );
		if ( pix )
			g_object_unref( G_OBJECT( pix ) );
		gdk_window_clear( root );
		
		XSetWindowBackgroundPixmap (s_XDisplay, root, pixmap);
	}*/
}

const Display *cairo_dock_get_Xdisplay (void)
{
	return s_XDisplay;
}

guint cairo_dock_get_root_id (void)
{
	return DefaultRootWindow (s_XDisplay);
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




void cairo_dock_set_strut_partial (int Xid, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x)
{
	g_return_if_fail (Xid > 0);

	cd_debug ("%s (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", __func__, left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);
	gulong iGeometryStrut[12] = {left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x};

	XChangeProperty (s_XDisplay,
		Xid,
		XInternAtom (s_XDisplay, "_NET_WM_STRUT_PARTIAL", False),
		XA_CARDINAL, 32, PropModeReplace,
		(guchar *) iGeometryStrut, 12);
	
	Window root = DefaultRootWindow (s_XDisplay);
	cairo_dock_set_xwindow_timestamp (Xid, cairo_dock_get_xwindow_timestamp (root));
}

void cairo_dock_set_xwindow_type_hint (int Xid, const gchar *cWindowTypeName)
{
	g_return_if_fail (Xid > 0);
	
	gulong iWindowType = XInternAtom (s_XDisplay, cWindowTypeName, False);
	cd_debug ("%s (%d, %s=%d)", __func__, Xid, cWindowTypeName, iWindowType);
	
	XChangeProperty (s_XDisplay,
		Xid,
		s_aNetWmWindowType,
		XA_ATOM, 32, PropModeReplace,
		(guchar *) &iWindowType, 1);
}


static inline gboolean _cairo_dock_window_has_type (int Xid, Atom iType)
{
	g_return_val_if_fail (Xid > 0, FALSE);
	
	gboolean bIsType;
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements;
	gulong *pTypeBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmWindowType, 0, G_MAXULONG, False, XA_ATOM, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pTypeBuffer);
	if (iBufferNbElements != 0)
	{
		cd_debug ("%s (%d) -> %d (%d,%d)", __func__, Xid, *pTypeBuffer, s_aNetWmWindowTypeNormal, s_aNetWmWindowTypeUtility);
		bIsType = (*pTypeBuffer == iType);
		XFree (pTypeBuffer);
	}
	else
		bIsType = FALSE;
	return bIsType;
}
gboolean cairo_dock_window_is_utility (int Xid)
{
	return _cairo_dock_window_has_type (Xid, s_aNetWmWindowTypeUtility);
}

gboolean cairo_dock_window_is_dock (int Xid)
{
	return _cairo_dock_window_has_type (Xid, s_aNetWmWindowTypeDock);
}


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

gboolean cairo_dock_update_screen_geometry (void)
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
	if (width_return != g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] || height_return != g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL])  // on n'utilise pas WidthOfScreen() et HeightOfScreen() car leurs valeurs ne sont pas mises a jour immediatement apres les changements de resolution.
	{
		g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] = width_return;
		g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] = height_return;
		g_iXScreenWidth[CAIRO_DOCK_VERTICAL] = g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
		g_iXScreenHeight[CAIRO_DOCK_VERTICAL] = g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
		return TRUE;
	}
	else
		return FALSE;
	/*Atom aNetWorkArea = XInternAtom (s_XDisplay, "_NET_WORKAREA", False);
	iBufferNbElements = 0;
	gulong *pXWorkArea = NULL;
	XGetWindowProperty (s_XDisplay, root, aNetWorkArea, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXWorkArea);
	int i;
	for (i = 0; i < iBufferNbElements/4; i ++)
	{
		cd_message ("work area : (%d;%d) %dx%d\n", pXWorkArea[4*i], pXWorkArea[4*i+1], pXWorkArea[4*i+2], pXWorkArea[4*i+3]);
	}
	XFree (pXWorkArea);*/
}


gboolean cairo_dock_property_is_present_on_root (const gchar *cPropertyName)
{
	g_return_val_if_fail (s_XDisplay != NULL, FALSE);
	Atom atom = XInternAtom (s_XDisplay, cPropertyName, False);
	Window root = DefaultRootWindow (s_XDisplay);
	int iNbProperties;
	Atom *pAtomList = XListProperties (s_XDisplay, root, &iNbProperties);
	int i;
	for (i = 0; i < iNbProperties; i ++)
	{
		if (pAtomList[i] == atom)
			break;
	}
	XFree (pAtomList);
	return (i != iNbProperties);
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
		Screen *scr = XDefaultScreenOfDisplay (s_XDisplay);
		cd_debug ("pVirtualScreenSizeBuffer : %dx%d ; screen : %dx%d", pVirtualScreenSizeBuffer[0], pVirtualScreenSizeBuffer[1], g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL], g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
		*iNbViewportX = pVirtualScreenSizeBuffer[0] / g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
		*iNbViewportY = pVirtualScreenSizeBuffer[1] / g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
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

	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
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
}
void cairo_dock_set_current_viewport (int iViewportNumberX, int iViewportNumberY)
{
	cairo_dock_move_current_viewport_to (iViewportNumberX * g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL], iViewportNumberY * g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL]);
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
}

Pixmap cairo_dock_get_window_background_pixmap (Window Xid)
{
	g_return_val_if_fail (Xid > 0, None);
	cd_debug ("%s (%d)", __func__, Xid);
	
	Pixmap iPixmapID;
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
	cd_debug ("%s (%d) : %ux%ux%u (%d;%d)", __func__, XPixmapID, iWidth, iHeight, iDepth, x, y);

	//\__________________ On recupere le drawable associe.
	GdkDrawable *pGdkDrawable = gdk_xid_table_lookup (XPixmapID);
	if (pGdkDrawable)
		g_object_ref (G_OBJECT (pGdkDrawable));
	else
	{
		cd_debug ("pas d'objet GDK present, on en alloue un nouveau");
		GdkScreen* pScreen = gdk_screen_get_default ();
		pGdkDrawable = gdk_pixmap_foreign_new_for_screen (pScreen, XPixmapID, iWidth, iHeight, iDepth);
	}

	//\__________________ On recupere la colormap.
	GdkColormap* pColormap = gdk_drawable_get_colormap (pGdkDrawable);
	if (pColormap == NULL && gdk_drawable_get_depth (pGdkDrawable) > 1)  // pour les bitmaps, on laisse la colormap a NULL, ils n'en ont pas besoin.
	{
		GdkScreen *pScreen = gdk_drawable_get_screen (GDK_DRAWABLE (pGdkDrawable));
		if (gdk_drawable_get_depth (pGdkDrawable) == 32)
			pColormap = gdk_screen_get_rgba_colormap (pScreen);
		else
			pColormap = gdk_screen_get_rgb_colormap (pScreen);  // au pire on a un colormap nul.
		cd_debug ("  pColormap : %x  (pScreen:%x)", pColormap, pScreen);
	}

	//\__________________ On recupere le buffer dans un GdkPixbuf.
	GdkPixbuf *pIconPixbuf = gdk_pixbuf_get_from_drawable (NULL,
		pGdkDrawable,
		pColormap,
		0,
		0,
		0,
		0,
		iWidth,
		iHeight);
	g_object_unref (G_OBJECT (pGdkDrawable));
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
	xClientMessage.xclient.data.l[0] = iNbViewportX * g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL];
	xClientMessage.xclient.data.l[1] = iNbViewportY * g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL];
	xClientMessage.xclient.data.l[2] = 0;
	xClientMessage.xclient.data.l[3] = 2;
	xClientMessage.xclient.data.l[4] = 0;

	XSendEvent (s_XDisplay,
		root,
		False,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xClientMessage);
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
}


gboolean cairo_dock_support_X_extension (void)
{
#ifdef HAVE_XEXTEND
	int event_base, error_base, major, minor;
	if (! XCompositeQueryExtension (s_XDisplay, &event_base, &error_base))  // on regarde si le serveur X supporte l'extension.
	{
		cd_warning ("XComposite extension not available.");
		s_bUseXComposite = FALSE;
	}
	else
	{
		major = 0, minor = 2;  // La version minimale requise pour avoir XCompositeNameWindowPixmap().
		XCompositeQueryVersion (s_XDisplay, &major, &minor);  // on regarde si on est au moins dans cette version.
		if (! (major > 0 || minor >= 2))
		{
			cd_warning ("XComposite extension too old.");
			s_bUseXComposite = FALSE;
		}
	}
	/*int iDamageError=0;
	if (! XDamageQueryExtension (s_XDisplay, &g_iDamageEvent, &iDamageError))
	{
		cd_warning ("XDamage extension not supported");
		return FALSE;
	}*/
	
	major = 0, minor = 0;
	if (! XTestQueryExtension (s_XDisplay, &event_base, &error_base, &major, &minor))
	{
		cd_warning ("XTest extension not available.");
		s_bUseXTest = FALSE;
	}
	
	if (! XineramaQueryExtension (s_XDisplay, &event_base, &error_base))
	{
		cd_warning ("Xinerama extension not supported");
		s_bUseXinerama = FALSE;
	}
	else
	{
		major = 0, minor = 0;
		if (XineramaQueryVersion (s_XDisplay, &major, &minor) == 0)
		{
			cd_warning ("Xinerama extension too old");
			s_bUseXinerama = FALSE;
		}
	}
	
	return TRUE;
#else
	cd_warning ("The dock was not compiled with the X extensions (XComposite, xinerama, xtst, XDamage, etc).");
	s_bUseXComposite = FALSE;
	s_bUseXTest = FALSE;
	s_bUseXinerama = FALSE;
	return FALSE;
#endif
}

gboolean cairo_dock_xcomposite_is_available (void)
{
	return s_bUseXComposite;
}

gboolean cairo_dock_xtest_is_available (void)
{
	return s_bUseXTest;
}

gboolean cairo_dock_xinerama_is_available (void)
{
	return s_bUseXinerama;
}


void cairo_dock_get_screen_offsets (int iNumScreen)
{
#ifdef HAVE_XEXTEND
	g_return_if_fail (s_bUseXinerama);
	int iNbScreens = 0;
	XineramaScreenInfo *pScreens = XineramaQueryScreens (s_XDisplay, &iNbScreens);
	if (pScreens != NULL)
	{
		if (iNumScreen >= iNbScreens)
		{
			cd_warning ("the number of screen where to place the dock is too big, we'll choose the last one.");
			iNumScreen = iNbScreens - 1;
		}
		g_iScreenOffsetX = pScreens[iNumScreen].x_org;
		g_iScreenOffsetY = pScreens[iNumScreen].y_org;
		g_iScreenWidth[CAIRO_DOCK_HORIZONTAL] = pScreens[iNumScreen].width;
		g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] = pScreens[iNumScreen].height;
		
		//g_iScreenOffsetX = 640;
		//g_iScreenWidth[CAIRO_DOCK_HORIZONTAL] = 640;
		
		g_iScreenWidth[CAIRO_DOCK_VERTICAL] = g_iScreenHeight[CAIRO_DOCK_HORIZONTAL];
		g_iScreenHeight[CAIRO_DOCK_VERTICAL] = g_iScreenWidth[CAIRO_DOCK_HORIZONTAL];
		cd_message (" * screen %d => (%d;%d) %dx%d\n", iNumScreen, g_iScreenOffsetX, g_iScreenOffsetY, g_iScreenWidth[CAIRO_DOCK_HORIZONTAL], g_iScreenHeight[CAIRO_DOCK_HORIZONTAL]);
		
		XFree (pScreens);
	}
	else
	{
		cd_warning ("No screens found from Xinerama, is it really active ?");
		g_iScreenOffsetX = g_iScreenOffsetY = 0;
	}
#else
	cd_warning ("The dock was not compiled with the support of Xinerama.");
#endif
}
