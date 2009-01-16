
/******************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <math.h>
#include <string.h>
#include <cairo.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>
#ifdef HAVE_XEXTEND
#include <X11/extensions/Xcomposite.h>
//#include <X11/extensions/Xdamage.h>
#endif

#include "cairo-dock-load.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-taskbar.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-applet-facility.h"
#include "cairo-dock-application-factory.h"


extern gboolean g_bEasterEggs;

static GHashTable *s_hAppliTable = NULL;  // table des PID connus de cairo-dock (affichees ou non dans le dock).
static Display *s_XDisplay = NULL;
static Atom s_aNetWmIcon;
static Atom s_aNetWmState;
static Atom s_aNetWmSkipPager;
static Atom s_aNetWmSkipTaskbar;
static Atom s_aNetWmPid;
static Atom s_aNetWmWindowType;
static Atom s_aNetWmWindowTypeNormal;
static Atom s_aNetWmWindowTypeDialog;
static Atom s_aNetWmName;
static Atom s_aUtf8String;
static Atom s_aWmName;
static Atom s_aString;
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

	s_hAppliTable = g_hash_table_new_full (g_int_hash,
		g_int_equal,
		g_free,
		NULL);
	
	s_aNetWmIcon = XInternAtom (s_XDisplay, "_NET_WM_ICON", False);

	s_aNetWmState = XInternAtom (s_XDisplay, "_NET_WM_STATE", False);
	s_aNetWmSkipPager = XInternAtom (s_XDisplay, "_NET_WM_STATE_SKIP_PAGER", False);
	s_aNetWmSkipTaskbar = XInternAtom (s_XDisplay, "_NET_WM_STATE_SKIP_TASKBAR", False);
	s_aNetWmHidden = XInternAtom (s_XDisplay, "_NET_WM_STATE_HIDDEN", False);

	s_aNetWmPid = XInternAtom (s_XDisplay, "_NET_WM_PID", False);

	s_aNetWmWindowType = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE", False);
	s_aNetWmWindowTypeNormal = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	s_aNetWmWindowTypeDialog = XInternAtom (s_XDisplay, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	
	s_aNetWmName = XInternAtom (s_XDisplay, "_NET_WM_NAME", False);
	s_aUtf8String = XInternAtom (s_XDisplay, "UTF8_STRING", False);
	s_aWmName = XInternAtom (s_XDisplay, "WM_NAME", False);
	s_aString = XInternAtom (s_XDisplay, "STRING", False);

	s_aWmHints = XInternAtom (s_XDisplay, "WM_HINTS", False);
	
	s_aNetWmFullScreen = XInternAtom (s_XDisplay, "_NET_WM_STATE_FULLSCREEN", False);
	s_aNetWmMaximizedHoriz = XInternAtom (s_XDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	s_aNetWmMaximizedVert = XInternAtom (s_XDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	s_aNetWmDemandsAttention = XInternAtom (s_XDisplay, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
}

void cairo_dock_unregister_pid (Icon *icon)
{
	if (myTaskBar.bUniquePid && CAIRO_DOCK_IS_APPLI (icon) && icon->iPid != 0)
	{
		g_hash_table_remove (s_hAppliTable, &icon->iPid);
	}
}

cairo_surface_t *cairo_dock_create_surface_from_xpixmap (Pixmap Xid, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight)
{
	g_return_val_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS && Xid > 0, NULL);
	GdkPixbuf *pPixbuf = cairo_dock_get_pixbuf_from_pixmap (Xid, TRUE);
	if (pPixbuf == NULL)
	{
		cd_warning ("This pixmap is undefined. It can happen for exemple for a window that is in a minimized state when the dock is launching.");
		return NULL;
	}
	g_print ("window pixmap : %dx%d\n", gdk_pixbuf_get_width (pPixbuf), gdk_pixbuf_get_height (pPixbuf));
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_pixbuf (pPixbuf,
		pSourceContext,
		fMaxScale,
		myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI],
		myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI],
		FALSE,
		fWidth,
		fHeight,
		NULL, NULL);
	g_object_unref (pPixbuf);
	return pSurface;
}

cairo_surface_t *cairo_dock_create_surface_from_xwindow (Window Xid, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight)
{
	g_return_val_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements = 0;
	gulong *pXIconBuffer = NULL;
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmIcon, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pXIconBuffer);

	if (iBufferNbElements > 2)
	{
		cairo_surface_t *pNewSurface = cairo_dock_create_surface_from_xicon_buffer (pXIconBuffer, iBufferNbElements, pSourceContext, fMaxScale, fWidth, fHeight);
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
			cairo_surface_t *pNewSurface = cairo_dock_create_surface_from_pixbuf (pIconPixbuf,
				pSourceContext,
				fMaxScale,
				myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLI],
				myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLI],
				FALSE,
				fWidth,
				fHeight,
				NULL, NULL);

			g_object_unref (pIconPixbuf);
			return pNewSurface;
		}
		return NULL;
	}
}


CairoDock *cairo_dock_manage_appli_class (Icon *icon, CairoDock *pMainDock)
{
	cd_message ("%s (%s)", __func__, icon->acName);
	CairoDock *pParentDock = pMainDock;
	g_free (icon->cParentDockName);
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskBar.bGroupAppliByClass && icon->cClass != NULL)
	{
		Icon *pSameClassIcon = cairo_dock_get_classmate (icon);
		//if (pSameClassIcon != NULL)
		//	g_print ("class-mate : %s (%s)\n", pSameClassIcon->acName, pSameClassIcon->cParentDockName);
		//pSameClassIcon = cairo_dock_get_icon_with_class (pMainDock->icons, icon->cClass);
		if (pSameClassIcon == NULL/** || pSameClassIcon == icon || pSameClassIcon->cParentDockName == NULL*/)  // aucun classmate => elle va dans le main dock.
		{
			cd_message ("  classe %s encore vide", icon->cClass);
			icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);
			CairoDock *pClassDock = cairo_dock_search_dock_from_name (icon->cClass);
			if (pClassDock != NULL)
			{
				if (icon->pSubDock == NULL)
				{
					///icon->pSubDock = pClassDock;
					///cd_warning ("on lie de force le sous-dock de la classe %s a l'icone %s", icon->cClass, icon->acName);
				}
				else if (pClassDock != icon->pSubDock)
					cd_warning ("le sous-dock de la classe %s est orphelin  (%s a deja un sous-dock) !", icon->cClass, icon->acName);
			}
		}
		else  // on la met dans le sous-dock de sa classe.
		{
			icon->cParentDockName = g_strdup (icon->cClass);

			//\____________ On cree ce sous-dock si necessaire.
			pParentDock = cairo_dock_search_dock_from_name (icon->cClass);
			if (pParentDock == NULL)  // alors il faut creer le sous-dock, et on decide de l'associer a pSameClassIcon.
			{
				cd_message ("  creation du dock pour la classe %s", icon->cClass);
				pParentDock = cairo_dock_create_subdock_for_class_appli (icon->cClass, pMainDock);
			}
			else
			{
				cd_message ("  sous-dock de la classe %s existant", icon->cClass);
			}
			
			if (! g_bEasterEggs)
			{
				//\____________ On l'associe au classmate.
				if (pSameClassIcon->pSubDock != NULL && pSameClassIcon->pSubDock != pParentDock)
				{
					cd_warning ("this appli (%s) already has a subdock, but it is not the class's subdock => we'll add its classmate in the main dock");
					
				}
				else if (pSameClassIcon->pSubDock == NULL)
					pSameClassIcon->pSubDock = pParentDock;
			}
			else
			{
				if (CAIRO_DOCK_IS_LAUNCHER (pSameClassIcon) || CAIRO_DOCK_IS_APPLET (pSameClassIcon))  // c'est un inhibiteur; on place juste l'icone dans le sous-dock de sa classe.
				{
					if (pSameClassIcon->pSubDock != NULL && pSameClassIcon->pSubDock != pParentDock)
					{
						cd_warning ("icon %s alreay owns a subdock, it can't owns the class subdock '%s'", pSameClassIcon->acName, icon->cClass);
						if (pParentDock->icons == NULL)
							cairo_dock_destroy_dock (pParentDock, icon->cClass, NULL, NULL);
						pParentDock = pMainDock;
						icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);
					}
					else
					{
						pSameClassIcon->pSubDock = pParentDock;
						Icon *pInhibatedIcon = cairo_dock_get_icon_with_Xid (pSameClassIcon->Xid);
						pSameClassIcon->Xid = 0;
						if (pInhibatedIcon != NULL)
						{
							cairo_dock_insert_icon_in_dock (pInhibatedIcon, pParentDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, FALSE);
						}
					}
				}
				else if (pParentDock->icons == NULL)  // le sous-dock de cette classe est nouveau, on deplace le classmate dans le sous-dock, et on met un fake a sa place pour pointer dessus.
				{
					g_print ("nouveau sous-dock de la classe %s\n", pSameClassIcon->cClass);
					Icon *pFakeClassIcon = g_new0 (Icon, 1);
					pFakeClassIcon->acName = g_strdup (pSameClassIcon->cClass);
					pFakeClassIcon->cClass = g_strdup (pSameClassIcon->cClass);
					pFakeClassIcon->iType = pSameClassIcon->iType;
					pFakeClassIcon->fOrder = pSameClassIcon->fOrder;
					pFakeClassIcon->cParentDockName = g_strdup (pSameClassIcon->cParentDockName);
					pFakeClassIcon->fWidth = pSameClassIcon->fWidth;
					pFakeClassIcon->fHeight = pSameClassIcon->fHeight;
					pFakeClassIcon->fXMax = pSameClassIcon->fXMax;
					pFakeClassIcon->fXMin = pSameClassIcon->fXMin;
					pFakeClassIcon->fXAtRest = pSameClassIcon->fXAtRest;
					pFakeClassIcon->pSubDock = pParentDock;  // grace a cela ce sera un lanceur.
					
					CairoDock *pClassMateParentDock = cairo_dock_search_dock_from_name (pSameClassIcon->cParentDockName);
					pFakeClassIcon->Xid = pSameClassIcon->Xid;
					cairo_dock_load_one_icon_from_scratch (pFakeClassIcon, CAIRO_CONTAINER (pClassMateParentDock));  // iBackingPixmap est nul donc on n'aura pas de miniature.
					pFakeClassIcon->Xid = 0;
					/// dessiner une embleme ?...
					
					g_print ("on detache %s pour la passer dans le sous-dock de sa classe\n", pSameClassIcon->acName);
					cairo_dock_detach_icon_from_dock (pSameClassIcon, pClassMateParentDock, FALSE);
					g_free (pSameClassIcon->cParentDockName);
					pSameClassIcon->cParentDockName = g_strdup (pSameClassIcon->cClass);
					cairo_dock_insert_icon_in_dock (pSameClassIcon, pParentDock, CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, FALSE);
				}
			}
		}
	}
	else
		icon->cParentDockName = g_strdup (CAIRO_DOCK_MAIN_DOCK_NAME);

	return pParentDock;
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
Icon * cairo_dock_create_icon_from_xwindow (cairo_t *pSourceContext, Window Xid, CairoDock *pDock)
{
	//g_print ("%s (%d)\n", __func__, Xid);
	guchar *pNameBuffer = NULL;
	gulong *pPidBuffer = NULL;
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
		int i, iNbMaximizedDimensions = 0;
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
	//else
	//	cd_message ("pas d'etat defini, donc on continue\n");
	if (bSkip)
	{
		cd_debug ("  cette fenetre est timide");
		return NULL;
	}

	//\__________________ On recupere son PID si on est en mode "PID unique".
	if (myTaskBar.bUniquePid)
	{
		iBufferNbElements = 0;
		XGetWindowProperty (s_XDisplay, Xid, s_aNetWmPid, 0, G_MAXULONG, False, XA_CARDINAL, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pPidBuffer);
		if (iBufferNbElements > 0)
		{
			//g_print (" +++ PID %d\n", *pPidBuffer);

			Icon *pIcon = g_hash_table_lookup (s_hAppliTable, pPidBuffer);
			if (pIcon != NULL)  // si c'est une fenetre d'une appli deja referencee, on ne rajoute pas d'icones.
			{
				XFree (pPidBuffer);
				return NULL;
			}
		}
		else
		{
			//g_print ("pas de PID defini -> elle degage\n");
			return NULL;
		}
	}

	//\__________________ On regarde son type.
	gulong *pTypeBuffer = NULL;
	cd_debug (" + nouvelle icone d'appli (%d)", Xid);
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmWindowType, 0, G_MAXULONG, False, XA_ATOM, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, (guchar **)&pTypeBuffer);
	if (iBufferNbElements != 0)
	{
		if (*pTypeBuffer != s_aNetWmWindowTypeNormal)
		{
			if (*pTypeBuffer == s_aNetWmWindowTypeDialog)
			{
				/*Window iPropWindow;
				XGetTransientForHint (s_XDisplay, Xid, &iPropWindow);
				g_print ("%s\n", gdk_x11_get_xatom_name (iPropWindow));*/
				Window XMainAppliWindow = _cairo_dock_get_parent_window (Xid);
				if (XMainAppliWindow != 0)
				{
					cd_debug ("  dialogue 'transient for' => on ignore");
					if (bDemandsAttention && (myTaskBar.bDemandsAttentionWithDialog || myTaskBar.cAnimationOnDemandsAttention))
					{
						Icon *pParentIcon = cairo_dock_get_icon_with_Xid (XMainAppliWindow);
						if (pParentIcon != NULL)
						{
							g_print ("%s requiert votre attention indirectement !\n", pParentIcon->acName);
							cairo_dock_appli_demands_attention (pParentIcon);
						}
						else
							g_print ("ce dialogue est bien bruyant ! (%d)\n", XMainAppliWindow);
					}
					XFree (pTypeBuffer);
					return NULL;  // inutile de rajouter le PID ici, c'est le meme que la fenetre principale.
				}
				//g_print ("dialogue autorise\n");
			}
			else
			{
				//g_print ("type indesirable\n");
				XFree (pTypeBuffer);
				if (myTaskBar.bUniquePid)
					g_hash_table_insert (s_hAppliTable, pPidBuffer, NULL);  // On rajoute son PID meme si c'est une appli qu'on n'affichera pas.
				return NULL;
			}
		}
		XFree (pTypeBuffer);
	}
	else
	{
		Window XMainAppliWindow = 0;
		XGetTransientForHint (s_XDisplay, Xid, &XMainAppliWindow);
		if (XMainAppliWindow != 0)
		{
			cd_debug ("  fenetre modale => on saute.");
			if (bDemandsAttention && (myTaskBar.bDemandsAttentionWithDialog || myTaskBar.cAnimationOnDemandsAttention))
			{
				Icon *pParentIcon = cairo_dock_get_icon_with_Xid (XMainAppliWindow);
				if (pParentIcon != NULL)
				{
					g_print ("%s requiert votre attention indirectement !\n", pParentIcon->acName);
					cairo_dock_appli_demands_attention (pParentIcon);
				}
				else
					g_print ("ce dialogue est bien bruyant ! (%d)\n", XMainAppliWindow);
			}
			return NULL;  // meme remarque.
		}
		//else
		//	cd_message (" pas de type defini -> on suppose que son type est 'normal'\n");
	}
	
	
	//\__________________ On recupere son nom.
	XGetWindowProperty (s_XDisplay, Xid, s_aNetWmName, 0, G_MAXULONG, False, s_aUtf8String, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, &pNameBuffer);
	if (iBufferNbElements == 0)
	{
		XGetWindowProperty (s_XDisplay, Xid, s_aWmName, 0, G_MAXULONG, False, s_aString, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, &pNameBuffer);
	}
	if (iBufferNbElements == 0)
	{
		//g_print ("pas de nom, elle degage\n");
		if (myTaskBar.bUniquePid)
			g_hash_table_insert (s_hAppliTable, pPidBuffer, NULL);  // On rajoute son PID meme si c'est une appli qu'on n'affichera pas.
		return NULL;
	}
	cd_debug ("recuperation de '%s' (bIsHidden : %d)", pNameBuffer, bIsHidden);
	
	
	//\__________________ On recupere la classe.
	XClassHint *pClassHint = XAllocClassHint ();
	gchar *cClass = NULL;
	if (XGetClassHint (s_XDisplay, Xid, pClassHint) != 0)
	{
		cd_debug ("  res_name : %s(%x); res_class : %s(%x)", pClassHint->res_name, pClassHint->res_name, pClassHint->res_class, pClassHint->res_class);
		cClass = g_ascii_strdown (pClassHint->res_class, -1);  // on la passe en minuscule, car certaines applis ont la bonne idee de donner des classes avec une majuscule ou non suivant les fenetres. Il reste le cas des applis telles que Glade2 ('Glade' et 'Glade-2' ...)
		XFree (pClassHint->res_name);
		XFree (pClassHint->res_class);
		//g_print (".\n");
	}
	XFree (pClassHint);
	
	
	//\__________________ On cree, on remplit l'icone, et on l'enregistre, par contre elle sera inseree plus tard.
	Icon *icon = g_new0 (Icon, 1);
	icon->acName = g_strdup ((gchar *)pNameBuffer);
	if (myTaskBar.bUniquePid)
		icon->iPid = *pPidBuffer;
	icon->Xid = Xid;
	icon->cClass = cClass;
	Icon * pLastAppli = cairo_dock_get_last_appli (pDock->icons);
	icon->fOrder = (pLastAppli != NULL ? pLastAppli->fOrder + 1 : 1);
	icon->iType = CAIRO_DOCK_APPLI;
	icon->bIsHidden = bIsHidden;
	icon->bIsMaximized = bIsMaximized;
	icon->bIsFullScreen = bIsFullScreen;
	icon->bIsDemandingAttention = bDemandsAttention;
	
	cairo_dock_get_window_geometry (Xid,
		&icon->windowGeometry.x,
		&icon->windowGeometry.y,
		&icon->windowGeometry.width,
		&icon->windowGeometry.height);
	#ifdef HAVE_XEXTEND
	if (myTaskBar.bShowThumbnail)
	{
		icon->iBackingPixmap = XCompositeNameWindowPixmap (s_XDisplay, Xid);
		/*icon->iDamageHandle = XDamageCreate (s_XDisplay, Xid, XDamageReportNonEmpty);  // XDamageReportRawRectangles
		g_print ("backing pixmap : %d ; iDamageHandle : %d\n", icon->iBackingPixmap, icon->iDamageHandle);*/
	}
	#endif
	
	cairo_dock_fill_icon_buffers_for_dock (icon, pSourceContext, pDock);
	
	if (myTaskBar.bUniquePid)
		g_hash_table_insert (s_hAppliTable, pPidBuffer, icon);
	cairo_dock_register_appli (icon);
	XFree (pNameBuffer);
	
	cairo_dock_set_window_mask (Xid, PropertyChangeMask | StructureNotifyMask);

	return icon;
}



void cairo_dock_Xproperty_changed (Icon *icon, Atom aProperty, int iState, CairoDock *pDock)
{
	//g_print ("%s (%s, %s)\n", __func__, icon->acName, gdk_x11_get_xatom_name (aProperty));
	Atom aReturnedType = 0;
	int aReturnedFormat = 0;
	unsigned long iLeftBytes, iBufferNbElements=0;
	cairo_t* pCairoContext;
	
	if (iState == PropertyNewValue && (aProperty == s_aNetWmName || aProperty == s_aWmName))
	{
		//g_print ("chgt de nom (%d)\n", aProperty);
		guchar *pNameBuffer = NULL;
		XGetWindowProperty (s_XDisplay, icon->Xid, s_aNetWmName, 0, G_MAXULONG, False, s_aUtf8String, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, &pNameBuffer);  // on cherche en priorite le nom en UTF8, car on est notifie des 2, mais il vaut mieux eviter le WM_NAME qui, ne l'etant pas, contient des caracteres bizarres qu'on ne peut pas convertir avec g_locale_to_utf8, puisque notre locale _est_ UTF8.
		if (iBufferNbElements == 0 && aProperty == s_aWmName)
			XGetWindowProperty (s_XDisplay, icon->Xid, aProperty, 0, G_MAXULONG, False, s_aString, &aReturnedType, &aReturnedFormat, &iBufferNbElements, &iLeftBytes, &pNameBuffer);
		if (iBufferNbElements > 0)
		{
			if (icon->acName == NULL || strcmp (icon->acName, (gchar *)pNameBuffer) != 0)
			{
				g_free (icon->acName);
				icon->acName = g_strdup ((gchar *)pNameBuffer);
				XFree (pNameBuffer);

				pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
				cairo_dock_fill_one_text_buffer (icon, pCairoContext, &myLabels.iconTextDescription, (mySystem.bTextAlwaysHorizontal ? CAIRO_DOCK_HORIZONTAL : pDock->bHorizontalDock), pDock->bDirectionUp);
				cairo_destroy (pCairoContext);
				
				cairo_dock_update_name_on_inhibators (icon->cClass, icon->Xid, icon->acName);
			}
		}
	}
	else if (iState == PropertyNewValue && aProperty == s_aNetWmIcon)
	{
		//g_print ("%s change son icone\n", icon->acName);
		if (cairo_dock_class_is_using_xicon (icon->cClass) || ! myTaskBar.bOverWriteXIcons)
		{
			pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
			icon->fWidth /= pDock->fRatio;
			icon->fHeight /= pDock->fRatio;
			cairo_dock_fill_one_icon_buffer (icon, pCairoContext, 1 + myIcons.fAmplitude, pDock->bHorizontalDock, TRUE, pDock->bDirectionUp);
			icon->fWidth *= pDock->fRatio;
			icon->fHeight *= pDock->fRatio;
			cairo_destroy (pCairoContext);
			cairo_dock_redraw_my_icon (icon, CAIRO_CONTAINER (pDock));
		}
	}
	else if (aProperty == s_aWmHints)
	{
		XWMHints *pWMHints = XGetWMHints (s_XDisplay, icon->Xid);
		if (pWMHints != NULL)
		{
			if ((pWMHints->flags & XUrgencyHint) && (myTaskBar.bDemandsAttentionWithDialog || myTaskBar.cAnimationOnDemandsAttention))
			{
				if (iState == PropertyNewValue)
				{
					g_print ("%s vous interpelle !\n", icon->acName);
					cairo_dock_appli_demands_attention (icon);
				}
				else if (iState == PropertyDelete)
				{
					g_print ("%s arrette de vous interpeler.\n", icon->acName);
					cairo_dock_appli_stops_demanding_attention (icon);
				}
				else
					cd_warning ("  etat du changement d'urgence inconnu sur %s !", icon->acName);
			}
			if (iState == PropertyNewValue && (pWMHints->flags & (IconPixmapHint | IconMaskHint | IconWindowHint)))
			{
				//g_print ("%s change son icone\n", icon->acName);
				if (cairo_dock_class_is_using_xicon (icon->cClass) || ! myTaskBar.bOverWriteXIcons)
				{
					pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
					icon->fWidth /= pDock->fRatio;
					icon->fHeight /= pDock->fRatio;
					cairo_dock_fill_one_icon_buffer (icon, pCairoContext, 1 + myIcons.fAmplitude, pDock->bHorizontalDock, TRUE, pDock->bDirectionUp);
					icon->fWidth *= pDock->fRatio;
					icon->fHeight *= pDock->fRatio;
					cairo_destroy (pCairoContext);
					cairo_dock_redraw_my_icon (icon, CAIRO_CONTAINER (pDock));
				}
			}
		}
	}
}


static void _cairo_dock_appli_demands_attention (Icon *icon, CairoDock *pDock)
{
	cd_debug ("%s (%s)\n", __func__, icon->acName);
	icon->bIsDemandingAttention = TRUE;
	if (myTaskBar.bDemandsAttentionWithDialog)
		cairo_dock_show_temporary_dialog_with_icon (icon->acName, icon, CAIRO_CONTAINER (pDock), myTaskBar.iDialogDuration, "same icon");
	if (myTaskBar.cAnimationOnDemandsAttention)
	{
		cairo_dock_request_icon_animation (icon, pDock, myTaskBar.cAnimationOnDemandsAttention, 10);
	}
}
void cairo_dock_appli_demands_attention (Icon *icon)
{
	CairoDock *pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);
	if (pParentDock == NULL)  // appli inhibee.
	{
		Icon *pInhibitorIcon = cairo_dock_get_classmate (icon);
		if (pInhibitorIcon != NULL)
		{
			pParentDock = cairo_dock_search_dock_from_name (pInhibitorIcon->cParentDockName);
			if (pParentDock != NULL)
				_cairo_dock_appli_demands_attention (pInhibitorIcon, pParentDock);
		}
	}
	else
		_cairo_dock_appli_demands_attention (icon, pParentDock);
}

static void _cairo_dock_appli_stops_demanding_attention (Icon *icon)
{
	icon->bIsDemandingAttention = FALSE;
	if (myTaskBar.bDemandsAttentionWithDialog)
		cairo_dock_remove_dialog_if_any (icon);
	cairo_dock_notify (CAIRO_DOCK_STOP_ICON, icon);  // arrete son animation quelqu'elle soit.
}
void cairo_dock_appli_stops_demanding_attention (Icon *icon)
{
	if (icon->cParentDockName == NULL)
	{
		Icon *pInhibitorIcon = cairo_dock_get_classmate (icon);
		if (pInhibitorIcon != NULL)
			_cairo_dock_appli_stops_demanding_attention (pInhibitorIcon);
	}
	_cairo_dock_appli_stops_demanding_attention (icon);
}

