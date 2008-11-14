 /*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cairo.h>
#include <pango/pango.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#ifdef HAVE_GLITZ
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include <gtk/gtkgl.h>
#include <X11/extensions/Xrender.h> 
#include <GL/gl.h> 
#include <GL/glu.h> 
#include <GL/glx.h> 
#include <gdk/x11/gdkglx.h>


#include "cairo-dock-draw.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-load.h"
#include "cairo-dock-config.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-separator-factory.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-file-manager.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-dock-factory.h"

extern int g_iWmHint;
extern CairoDock *g_pMainDock;
extern gboolean g_bSameHorizontality;
extern double g_fSubDockSizeRatio;
extern gboolean g_bReserveSpace;
extern gboolean g_bAnimateOnAutoHide;
extern double g_fUnfoldAcceleration;

extern int g_iScreenWidth[2], g_iScreenHeight[2];
extern int g_iMaxAuthorizedWidth;
extern gint g_iDockLineWidth;
extern int g_iIconGap;
extern double g_fAmplitude;

extern CairoDockLabelDescription g_iconTextDescription;
extern gboolean g_bTextAlwaysHorizontal;

extern gboolean g_bUseSeparator;

extern gchar *g_cCurrentLaunchersPath;


extern int g_tIconTypeOrder[CAIRO_DOCK_NB_TYPES];
extern gchar *g_cConfFile;

extern gboolean g_bKeepAbove;
extern gboolean g_bPopUp;
extern gboolean g_bSkipPager;
extern gboolean g_bSkipTaskbar;
extern gboolean g_bSticky;

extern gboolean g_bUseGlitz;
extern gboolean g_bUseOpenGL;
extern gboolean g_bIndirectRendering;
extern GdkGLConfig* g_pGlConfig;

CairoDock *cairo_dock_create_new_dock (GdkWindowTypeHint iWmHint, gchar *cDockName, gchar *cRendererName)
{
	//static pouet = 0;
	//g_print ("%s ()\n", __func__);
	g_return_val_if_fail (cDockName != NULL, NULL);
	
	//\__________________ On enregistre un nouveau dock.
	CairoDock *pDock = g_new0 (CairoDock, 1);
	pDock->iType = CAIRO_DOCK_TYPE_DOCK;
	CairoDock *pInsertedDock = cairo_dock_register_dock (cDockName, pDock);  // determine au passage si c'est le MainDock.
	if (pInsertedDock != pDock)  // un autre dock de ce nom existe deja.
	{
		g_free (pDock);
		return pInsertedDock;
	}
	
	pDock->bAtBottom = TRUE;
	pDock->iRefCount = 0;  // c'est un dock racine par defaut.
	pDock->fRatio = 1.;
	pDock->iAvoidingMouseIconType = -1;
	pDock->fFlatDockWidth = - g_iIconGap;
	pDock->iMouseX = -1; // utile ?
	pDock->iMouseY = -1;
	pDock->fMagnitudeMax = 1.;

	//\__________________ On cree la fenetre GTK.
	GtkWidget* pWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	pDock->pWidget = pWindow;

	if (g_bSticky)
		gtk_window_stick (GTK_WINDOW (pWindow));
	if (g_bKeepAbove)
		gtk_window_set_keep_above (GTK_WINDOW (pWindow), g_bKeepAbove);
	if (g_bPopUp)
		gtk_window_set_keep_below (GTK_WINDOW (pWindow), g_bPopUp);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (pWindow), g_bSkipPager);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (pWindow), g_bSkipTaskbar);
	gtk_window_set_gravity (GTK_WINDOW (pWindow), GDK_GRAVITY_STATIC);

	gtk_window_set_type_hint (GTK_WINDOW (pWindow), iWmHint);

	
	if (g_bUseOpenGL && g_pGlConfig == NULL)  // taken from a MacSlow's exemple.
	{
		VisualID xvisualid;
	
		GdkDisplay	   *gdkdisplay;
		Display	   *XDisplay;
		Window	   xid;
		GdkVisual		    *visual;

		gdkdisplay = gdk_display_get_default ();
		XDisplay   = gdk_x11_display_get_xdisplay (gdkdisplay);
		xid = gdk_x11_drawable_get_xid (GDK_DRAWABLE (pDock->pWidget->window));
		Window root = XRootWindow(XDisplay, 0);
		
		XWindowAttributes attrib;
		XVisualInfo templ;
		XVisualInfo *visinfo;
		int nvisinfo, defaultDepth, value;
		
		
		GLXFBConfig*	     pFBConfigs; 
		XRenderPictFormat*   pPictFormat = NULL; 
		int doubleBufferAttributes[] = { 
			GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, 
			GLX_RENDER_TYPE,   GLX_RGBA_BIT,
			GLX_DOUBLEBUFFER,  True, 
			GLX_RED_SIZE,      1, 
			GLX_GREEN_SIZE,    1, 
			GLX_BLUE_SIZE,     1, 
			GLX_ALPHA_SIZE,    1, 
			GLX_DEPTH_SIZE,    1, 
			None}; 
		
		int i, iNumOfFBConfigs = 0;
		pFBConfigs = glXChooseFBConfig (XDisplay, 
			DefaultScreen (XDisplay), 
			doubleBufferAttributes, 
			&iNumOfFBConfigs); 
		
		if (pFBConfigs == NULL)
		{
			cd_warning ("Argl, we could not get an ARGB-visual!");
		}
		
		gboolean bKeepGoing=FALSE;
		//GLXFBConfig	     renderFBConfig; 
		XVisualInfo*	     pVisInfo = NULL; 
		for (i = 0; i < iNumOfFBConfigs; i++) 
		{ 
			pVisInfo = glXGetVisualFromFBConfig (XDisplay, pFBConfigs[i]); 
			if (!pVisInfo) 
				continue; 
	
			pPictFormat = XRenderFindVisualFormat (XDisplay, pVisInfo->visual); 
			if (!pPictFormat)
			{
				XFree (pVisInfo);
				continue; 
			}
			
			if (pPictFormat->direct.alphaMask > 0) 
			{ 
				cd_message ("Strike, found a GLX visual with alpha-support!");
				//pVisInfo = glXGetVisualFromFBConfig (XDisplay, pFBConfigs[i]); 
				//renderFBConfig = pFBConfigs[i]; 
				bKeepGoing = True; 
				break; 
			} 
	
			XFree (pVisInfo); 
		} 
		
		if (bKeepGoing)
		{
			//visual = gdkx_visual_get (pVisInfo->visualid);
			//pColormap = gdk_colormap_new (visual, TRUE);
			g_pGlConfig = gdk_x11_gl_config_new_from_visualid (pVisInfo->visualid);
			XFree (pVisInfo);
		}
	}
	
	cairo_dock_set_colormap (CAIRO_CONTAINER (pDock));
	
	if (g_bUseOpenGL)
	{
		GdkGLContext *pMainGlContext = (pDock->bIsMainDock ? NULL : gtk_widget_get_gl_context (g_pMainDock->pWidget));
		gtk_widget_set_gl_capability (pWindow,
			g_pGlConfig,
			pMainGlContext,  // on partage les ressources entre les contextes.
			! g_bIndirectRendering,  // TRUE <=> direct connection to the graphics system.
			GDK_GL_RGBA_TYPE);
		
		g_signal_connect_after (G_OBJECT (pWindow),
			"realize",
			G_CALLBACK (on_realize),
			pDock);
	}

	gtk_widget_set_app_paintable (pWindow, TRUE);
	gtk_window_set_decorated (GTK_WINDOW (pWindow), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_title (GTK_WINDOW (pWindow), "cairo-dock");  // GTK renseigne la classe avec la meme valeur.
	
	cairo_dock_set_renderer (pDock, cRendererName);

	//\__________________ On connecte les evenements a la fenetre.
	gtk_widget_add_events (pWindow,
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
		GDK_KEY_PRESS_MASK |
		//GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK |
		GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	
	g_signal_connect (G_OBJECT (pWindow),
		"delete-event",
		G_CALLBACK (on_delete),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"expose-event",
		G_CALLBACK (on_expose),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (on_configure),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"key-press-event",
		G_CALLBACK (on_key_press),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"key-release-event",
		G_CALLBACK (on_key_release),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-press-event",
		G_CALLBACK (on_button_press2),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-release-event",
		G_CALLBACK (on_button_press2),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"scroll-event",
		G_CALLBACK (on_scroll),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"motion-notify-event",
		G_CALLBACK (on_motion_notify2),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"enter-notify-event",
		G_CALLBACK (on_enter_notify2),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"leave-notify-event",
		G_CALLBACK (on_leave_notify2),
		pDock);
	cairo_dock_allow_widget_to_receive_data (pWindow, G_CALLBACK (on_drag_data_received), pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag_motion",
		G_CALLBACK (on_drag_motion),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag_leave",
		G_CALLBACK (on_drag_leave),
		pDock);
	/*g_signal_connect (G_OBJECT (pWindow),
		"selection_request_event",
		G_CALLBACK (on_selection_request_event),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"selection_notify_event",
		G_CALLBACK (on_selection_notify_event),
		pDock);*/

	
	gtk_window_get_size (GTK_WINDOW (pWindow), &pDock->iCurrentWidth, &pDock->iCurrentHeight);  // ca n'est que la taille initiale allouee par GTK.
	gtk_widget_show_all (pWindow);
	gdk_window_set_back_pixmap (pWindow->window, NULL, FALSE);  // utile ?
	
	/*if (!pouet)
	{
		pouet = 1;
		GdkAtom selection = gdk_atom_intern ("_NET_SYSTEM_TRAY_S0", FALSE);
		gboolean bOwnerOK = gtk_selection_owner_set (pWindow,
			selection,
			GDK_CURRENT_TIME);
		cd_message ("bOwnerOK : %d\n", bOwnerOK);

		gtk_selection_add_target (pWindow,
			selection,
			GDK_SELECTION_TYPE_ATOM,
			1);
		gtk_selection_add_target (pWindow,
			selection,
			GDK_SELECTION_TYPE_BITMAP,
			2);
		gtk_selection_add_target (pWindow,
			selection,
			GDK_SELECTION_TYPE_DRAWABLE,
			3);
		gtk_selection_add_target (pWindow,
			selection,
			GDK_SELECTION_TYPE_INTEGER,
			4);
		gtk_selection_add_target (pWindow,
			selection,
			GDK_SELECTION_TYPE_STRING,
			4);
		gtk_selection_add_target (pWindow,
			selection,
			GDK_SELECTION_TYPE_WINDOW,
			4);
		gtk_selection_add_target (pWindow,
			selection,
			GDK_SELECTION_TYPE_PIXMAP,
			4);

		GdkAtom message_type = gdk_atom_intern ("_NET_SYSTEM_TRAY_OPCODE", False);
		gtk_selection_add_target (pWindow,
			selection,
			message_type,
			5);
		message_type = gdk_atom_intern ("_NET_SYSTEM_TRAY_MESSAGE_DATA", False );
		gtk_selection_add_target (pWindow,
			selection,
			message_type,
			5);

		g_signal_connect (G_OBJECT (pWindow),
			"selection_get",
			G_CALLBACK (on_selection_get),
			pDock);
		g_signal_connect (G_OBJECT (pWindow),
			"selection_received",
			G_CALLBACK (on_selection_received),
			pDock);
		g_signal_connect (G_OBJECT (pWindow),
			"selection_clear_event",
			G_CALLBACK (on_selection_clear_event),
			pDock);
	}*/

#ifdef HAVE_GLITZ
	if (g_bUseGlitz && pDock->pDrawFormat != NULL)
	{
		glitz_format_t templ;
		GdkDisplay	   *gdkdisplay;
		Display	   *XDisplay;
		Window	   xid;

		gdkdisplay = gdk_display_get_default ();
		XDisplay   = gdk_x11_display_get_xdisplay (gdkdisplay);
		xid = gdk_x11_drawable_get_xid (GDK_DRAWABLE (pWindow->window));
		pDock->pGlitzDrawable = glitz_glx_create_drawable_for_window (XDisplay,
			0,
			pDock->pDrawFormat,
			xid,
			pDock->iCurrentWidth,
			pDock->iCurrentHeight);
		if (! pDock->pGlitzDrawable)
		{
			cd_warning ("Attention : failed to create glitz drawable");
		}
		else
		{
			templ.color        = pDock->pDrawFormat->color;
			templ.color.fourcc = GLITZ_FOURCC_RGB;
			pDock->pGlitzFormat = glitz_find_format (pDock->pGlitzDrawable,
				GLITZ_FORMAT_RED_SIZE_MASK   |
				GLITZ_FORMAT_GREEN_SIZE_MASK |
				GLITZ_FORMAT_BLUE_SIZE_MASK  |
				GLITZ_FORMAT_ALPHA_SIZE_MASK |
				GLITZ_FORMAT_FOURCC_MASK,
				&templ,
				0);
			if (! pDock->pGlitzFormat)
			{
				cd_warning ("couldn't find glitz surface format");
			}
		}
	}
#endif
	
	if (! pDock->bIsMainDock)
		cairo_dock_get_root_dock_position (cDockName, pDock);
	
	//if (! g_bUseGlitz)
		while (gtk_events_pending ())  // on force le redessin pour eviter les carre gris.
			gtk_main_iteration ();
	
	return pDock;
}

static void _cairo_dock_fm_remove_monitor_on_one_icon (Icon *icon, gpointer data)
{
	if (CAIRO_DOCK_IS_URI_LAUNCHER (icon))
		cairo_dock_fm_remove_monitor (icon);
}
void cairo_dock_deactivate_one_dock (CairoDock *pDock)
{
	if (pDock->iSidMoveDown != 0)
		g_source_remove (pDock->iSidMoveDown);
	if (pDock->iSidMoveUp != 0)
		g_source_remove (pDock->iSidMoveUp);
	if (pDock->iSidPopDown != 0)
		g_source_remove (pDock->iSidPopDown);
	if (pDock->iSidPopUp != 0)
		g_source_remove (pDock->iSidPopUp);
	if (pDock->iSidGrowUp != 0)
		g_source_remove (pDock->iSidGrowUp);
	if (pDock->iSidShrinkDown != 0)
		g_source_remove (pDock->iSidShrinkDown);
	if (pDock->iSidLeaveDemand != 0)
		g_source_remove (pDock->iSidLeaveDemand);
	if (pDock->bIsMainDock && cairo_dock_application_manager_is_running ())
	{
		cairo_dock_pause_application_manager ();  // precaution au cas ou.
	}
	cairo_dock_notify (CAIRO_DOCK_STOP_DOCK, pDock);
	if (pDock->iSidGLAnimation != 0)
		g_source_remove (pDock->iSidGLAnimation);

	g_list_foreach (pDock->icons, (GFunc) _cairo_dock_fm_remove_monitor_on_one_icon, NULL);

	Icon *pPointedIcon;
	if ((pPointedIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL)) != NULL)
	{
		pPointedIcon->pSubDock = NULL;
	}

	gtk_widget_destroy (pDock->pWidget);
	pDock->pWidget = NULL;

	g_free (pDock->cRendererName);
	pDock->cRendererName = NULL;
}

void cairo_dock_free_dock (CairoDock *pDock)
{
	cairo_dock_deactivate_one_dock (pDock);

	g_list_foreach (pDock->icons, (GFunc) cairo_dock_free_icon, NULL);
	g_list_free (pDock->icons);

	g_free (pDock);
}

void cairo_dock_destroy_dock (CairoDock *pDock, const gchar *cDockName, CairoDock *pReceivingDock, gchar *cpReceivingDockName)
{
	g_print ("%s (%s, %d)\n", __func__, cDockName, pDock->iRefCount);
	g_return_if_fail (pDock != NULL && cDockName != NULL);
	if (pDock->bIsMainDock)  // utiliser cairo_dock_free_all_docks ().
		return;
	pDock->iRefCount --;
	if (pDock->iRefCount > 0)
		return ;

	cairo_dock_deactivate_one_dock (pDock);

	gboolean bModuleWasRemoved = FALSE;
	GList *pIconsList = pDock->icons;
	pDock->icons = NULL;
	Icon *icon;
	GList *ic;
	gchar *cDesktopFilePath;
	for (ic = pIconsList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;

		if (icon->pSubDock != NULL && pReceivingDock == NULL)
		{
			cairo_dock_destroy_dock (icon->pSubDock, icon->acName, NULL, NULL);
			icon->pSubDock = NULL;
		}

		if (pReceivingDock == NULL || cpReceivingDockName == NULL)  // alors on les jete.
		{
			if (CAIRO_DOCK_IS_NORMAL_LAUNCHER (icon))  // icon->acDesktopFileName != NULL
			{
				cDesktopFilePath = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->acDesktopFileName);
				g_remove (cDesktopFilePath);
				g_free (cDesktopFilePath);
			}
			else if (CAIRO_DOCK_IS_APPLET (icon))  // on decide de les remettre dans le dock principal la prochaine fois qu'ils seront actives.
			{
				cairo_dock_update_icon_s_container_name (icon, CAIRO_DOCK_MAIN_DOCK_NAME);
				bModuleWasRemoved = TRUE;
			}
			cairo_dock_free_icon (icon);
		}
		else  // on les re-attribue au dock receveur.
		{
			cairo_dock_update_icon_s_container_name (icon, cpReceivingDockName);

			///if (pDock->iRefCount > 0)
			{
				icon->fWidth /= pDock->fRatio;  /// g_fSubDockSizeRatio
				icon->fHeight /= pDock->fRatio;
			}
			cd_debug (" on re-attribue %s au dock %s", icon->acName, icon->cParentDockName);
			cairo_dock_insert_icon_in_dock (icon, pReceivingDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, g_bUseSeparator);
			
			if (CAIRO_DOCK_IS_APPLET (icon))
			{
				icon->pModuleInstance->pContainer = CAIRO_CONTAINER (pReceivingDock);  // astuce pour ne pas avoir a recharger le fichier de conf ^_^
				icon->pModuleInstance->pDock = pReceivingDock;
				cairo_dock_reload_module_instance (icon->pModuleInstance, FALSE);
			}
		}
	}
	if (pReceivingDock != NULL)
		cairo_dock_update_dock_size (pReceivingDock);

	g_list_free (pIconsList);

	cairo_dock_unregister_dock (cDockName);
	
	if (bModuleWasRemoved)
		cairo_dock_update_conf_file_with_active_modules2 (NULL, g_cConfFile);
	
	if (pDock->iRefCount == -1)  // c'etait un dock racine.
		cairo_dock_remove_root_dock_config (cDockName);
	
	g_free (pDock);
}

void cairo_dock_reload_reflects_in_dock (CairoDock *pDock)
{
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
	double fMaxScale = cairo_dock_get_max_scale (CAIRO_CONTAINER (pDock));
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pReflectionBuffer != NULL || icon->pFullIconBuffer != NULL)
		{
			cairo_surface_destroy (icon->pReflectionBuffer);
			icon->pReflectionBuffer = NULL;
			cairo_surface_destroy (icon->pFullIconBuffer);
			icon->pFullIconBuffer = NULL;
			cairo_dock_load_reflect_on_icon (icon, pCairoContext, fMaxScale, pDock->bHorizontalDock, pDock->bDirectionUp);
		}
	}
	cairo_destroy (pCairoContext);
}
void cairo_dock_reference_dock (CairoDock *pDock, CairoDock *pParentDock)
{
	pDock->iRefCount ++;
	if (pDock->iRefCount == 1)  // il devient un sous-dock.
	{
		if (pParentDock == NULL)
			pParentDock = g_pMainDock;
		CairoDockPositionType iScreenBorder = ((! pDock->bHorizontalDock) << 1) | (! pDock->bDirectionUp);
		cd_message ("position : %d/%d", pDock->bHorizontalDock, pDock->bDirectionUp);
		pDock->bHorizontalDock = (g_bSameHorizontality ? pParentDock->bHorizontalDock : ! pParentDock->bHorizontalDock);
		pDock->bDirectionUp = pParentDock->bDirectionUp;
		if (iScreenBorder != (((! pDock->bHorizontalDock) << 1) | (! pDock->bDirectionUp)))
		{
			cd_message ("changement de position -> %d/%d", pDock->bHorizontalDock, pDock->bDirectionUp);
			cairo_dock_reload_reflects_in_dock (pDock);
		}
		if (g_bKeepAbove)
			gtk_window_set_keep_above (GTK_WINDOW (pDock->pWidget), FALSE);
		if (g_bPopUp)
			gtk_window_set_keep_below (GTK_WINDOW (pDock->pWidget), FALSE);
		
		pDock->bAutoHide = FALSE;
		double fPrevRatio = pDock->fRatio;
		pDock->fRatio = MIN (pDock->fRatio, g_fSubDockSizeRatio);

		Icon *icon;
		GList *ic;
		pDock->fFlatDockWidth = - g_iIconGap;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			icon->fWidth *= pDock->fRatio / fPrevRatio;
			icon->fHeight *= pDock->fRatio / fPrevRatio;
			pDock->fFlatDockWidth += icon->fWidth + g_iIconGap;

			if (! g_bSameHorizontality)
			{
				cairo_t* pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
				cairo_dock_fill_one_text_buffer (icon, pSourceContext, &g_iconTextDescription, (g_bTextAlwaysHorizontal ? CAIRO_DOCK_HORIZONTAL : pDock->bHorizontalDock), pDock->bDirectionUp);
				cairo_destroy (pSourceContext);
			}
		}
		pDock->iMaxIconHeight *= pDock->fRatio / fPrevRatio;

		cairo_dock_set_default_renderer (pDock);

		gtk_widget_hide (pDock->pWidget);
		cairo_dock_update_dock_size (pDock);
		
		const gchar *cDockName = cairo_dock_search_dock_name (pDock);
		cairo_dock_remove_root_dock_config (cDockName);
	}
}

CairoDock *cairo_dock_create_subdock_from_scratch_with_type (GList *pIconList, gchar *cDockName, GdkWindowTypeHint iWindowTypeHint, CairoDock *pParentDock)
{
	CairoDock *pSubDock = cairo_dock_create_new_dock (iWindowTypeHint, cDockName, NULL);
	cairo_dock_reference_dock (pSubDock, pParentDock);  // on le fait tout de suite pour avoir la bonne reference avant le 'load'.

	pSubDock->icons = pIconList;
	if (pIconList != NULL)
		cairo_dock_load_buffers_in_one_dock (pSubDock);

	/*while (gtk_events_pending ())
		gtk_main_iteration ();
	gtk_widget_hide (pSubDock->pWidget);*/

	return pSubDock;
}

static void _cairo_dock_update_child_dock_size (gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (! pDock->bIsMainDock)
	{
		cd_message ("  %s (%s)", __func__, cDockName);
		cairo_dock_update_dock_size (pDock);
		///pDock->iMouseX = -1; // utile ?
		///pDock->iMouseY = -1;
		pDock->calculate_icons (pDock);
		gtk_window_present (GTK_WINDOW (pDock->pWidget));
		while (gtk_events_pending ())
			gtk_main_iteration ();
		if (pDock->iRefCount > 0)
			gtk_widget_hide (pDock->pWidget);
		else
			gtk_window_move (GTK_WINDOW (pDock->pWidget), 500, 500);  // sinon ils n'apparaisesent pas.
	}
}
void cairo_dock_build_docks_tree_with_desktop_files (CairoDock *pMainDock, gchar *cDirectory)
{
	cd_message ("%s (%s)", __func__, cDirectory);
	GDir *dir = g_dir_open (cDirectory, 0, NULL);
	g_return_if_fail (dir != NULL);

	Icon* icon;
	const gchar *cFileName;
	CairoDock *pParentDock;
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pMainDock));

	do
	{
		cFileName = g_dir_read_name (dir);
		if (cFileName == NULL)
			break ;

		if (g_str_has_suffix (cFileName, ".desktop"))
		{
			icon = cairo_dock_create_icon_from_desktop_file (cFileName, pCairoContext);
			g_return_if_fail (icon->cParentDockName != NULL);

			pParentDock = cairo_dock_search_dock_from_name (icon->cParentDockName);

			if (pParentDock != NULL)  // a priori toujours vrai.
				cairo_dock_insert_icon_in_dock (icon, pParentDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, ! CAIRO_DOCK_INSERT_SEPARATOR);
		}
	} while (1);
	g_dir_close (dir);
	cairo_destroy (pCairoContext);
	//g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_update_child_dock_size, NULL);  // on mettra a jour la taille du dock principal apres y avoir insere les applis/applets, car pour l'instant les docks fils n'en ont pas.
}

void cairo_dock_free_all_docks (void)
{
	if (g_pMainDock == NULL)
		return ;

	cairo_dock_deactivate_all_modules ();  // y compris les modules qui n'ont pas d'icone.
	
	cairo_dock_reset_class_table ();  // enleve aussi les inhibiteurs.
	cairo_dock_stop_application_manager ();
	cairo_dock_stop_polling_screen_edge ();

	cairo_dock_reset_docks_table ();
}



void cairo_dock_update_dock_size (CairoDock *pDock)  // iMaxIconHeight et fFlatDockWidth doivent avoir ete mis a jour au prealable.
{
	pDock->calculate_max_dock_size (pDock);
	
	int n = 0;
	do
	{
		double fPrevRatio = pDock->fRatio;
		cd_debug ("  %s (%d / %d)", __func__, (int)pDock->iMaxDockWidth, g_iMaxAuthorizedWidth);
		if (pDock->iMaxDockWidth > g_iMaxAuthorizedWidth)  // g_iScreenWidth[pDock->bHorizontalDock]
		{
			pDock->fRatio *= 1. * g_iMaxAuthorizedWidth / pDock->iMaxDockWidth;
		}
		else
		{
			double fMaxRatio = (pDock->iRefCount == 0 ? 1 : g_fSubDockSizeRatio);
			if (pDock->fRatio < fMaxRatio)
			{
				pDock->fRatio *= 1. * g_iMaxAuthorizedWidth / pDock->iMaxDockWidth;
				pDock->fRatio = MIN (pDock->fRatio, fMaxRatio);
			}
			else
				pDock->fRatio = fMaxRatio;
		}
		
		if (pDock->iMaxDockHeight > g_iScreenHeight[pDock->bHorizontalDock])
		{
			pDock->fRatio = MIN (pDock->fRatio, fPrevRatio * g_iScreenHeight[pDock->bHorizontalDock] / pDock->iMaxDockHeight);
		}
		
		if (fPrevRatio != pDock->fRatio)
		{
			cd_debug ("  -> changement du ratio : %.3f -> %.3f (%d, %d try)", fPrevRatio, pDock->fRatio, pDock->iRefCount, n);
			Icon *icon;
			GList *ic;
			pDock->fFlatDockWidth = -g_iIconGap;
			for (ic = pDock->icons; ic != NULL; ic = ic->next)
			{
				icon = ic->data;
				icon->fWidth *= pDock->fRatio / fPrevRatio;
				icon->fHeight *= pDock->fRatio / fPrevRatio;
				pDock->fFlatDockWidth += icon->fWidth + g_iIconGap;
			}
			pDock->iMaxIconHeight *= pDock->fRatio / fPrevRatio;
			
			pDock->calculate_max_dock_size (pDock);
		}
		
		n ++;
	} while ((pDock->iMaxDockWidth > g_iMaxAuthorizedWidth || pDock->iMaxDockHeight > g_iScreenHeight[pDock->bHorizontalDock]) && n < 3);
	
	if (! pDock->bInside && (pDock->bAutoHide && pDock->iRefCount == 0))
		return;
	else if (GTK_WIDGET_VISIBLE (pDock->pWidget))
	{
		int iNewWidth, iNewHeight;
		cairo_dock_get_window_position_and_geometry_at_balance (pDock, (pDock->bInside || pDock->iSidShrinkDown > 0 ? CAIRO_DOCK_MAX_SIZE : CAIRO_DOCK_NORMAL_SIZE), &iNewWidth, &iNewHeight);  // inutile de recalculer Y mais bon...

		if (pDock->bHorizontalDock)
		{
			if (pDock->iCurrentWidth != iNewWidth || pDock->iCurrentHeight != iNewHeight)
				gdk_window_move_resize (pDock->pWidget->window,
					pDock->iWindowPositionX,
					pDock->iWindowPositionY,
					iNewWidth,
					iNewHeight);
		}
		else
		{
			if (pDock->iCurrentWidth != iNewHeight || pDock->iCurrentHeight != iNewWidth)
				gdk_window_move_resize (pDock->pWidget->window,
					pDock->iWindowPositionY,
					pDock->iWindowPositionX,
					iNewHeight,
					iNewWidth);
		}
	}
	
	cairo_dock_set_icons_geometry_for_window_manager (pDock);

	cairo_dock_update_background_decorations_if_necessary (pDock, pDock->iDecorationsWidth, pDock->iDecorationsHeight);
}

void cairo_dock_insert_icon_in_dock (Icon *icon, CairoDock *pDock, gboolean bUpdateSize, gboolean bAnimated, gboolean bApplyRatio, gboolean bInsertSeparator)
{
	g_return_if_fail (icon != NULL);
	if (g_list_find (pDock->icons, icon) != NULL)  // elle est deja dans ce dock.
		return ;

	int iPreviousMinWidth = pDock->fFlatDockWidth;
	int iPreviousMaxIconHeight = pDock->iMaxIconHeight;

	//\______________ On regarde si on doit inserer un separateur.
	gboolean bSeparatorNeeded = FALSE;
	if (bInsertSeparator && ! CAIRO_DOCK_IS_SEPARATOR (icon))
	{
		Icon *pSameTypeIcon = cairo_dock_get_first_icon_of_type (pDock->icons, icon->iType);
		if (pSameTypeIcon == NULL && pDock->icons != NULL)
		{
			bSeparatorNeeded = TRUE;
			cd_message ("separateur necessaire");
		}
	}

	//\______________ On insere l'icone a sa place dans la liste.
	if (icon->fOrder == CAIRO_DOCK_LAST_ORDER)
	{
		Icon *pLastIcon = cairo_dock_get_last_icon_of_type (pDock->icons, icon->iType);
		if (pLastIcon != NULL)
			icon->fOrder = pLastIcon->fOrder + 1;
		else
			icon->fOrder = 1;
	}

	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon,
		(GCompareFunc) cairo_dock_compare_icons_order);

	if (bApplyRatio)
	{
		icon->fWidth *= pDock->fRatio;
		icon->fHeight *= pDock->fRatio;
	}
	//g_print (" +size <- %.2fx%.2f\n", icon->fWidth, icon->fHeight);

	if (! g_bSameHorizontality)
	{
		cairo_t* pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
		cairo_dock_fill_one_text_buffer (icon, pSourceContext, &g_iconTextDescription, (g_bTextAlwaysHorizontal ? CAIRO_DOCK_HORIZONTAL : pDock->bHorizontalDock), pDock->bDirectionUp);
		cairo_destroy (pSourceContext);
	}

	pDock->fFlatDockWidth += g_iIconGap + icon->fWidth;
	pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);

	//\______________ On insere un separateur si necessaire.
	if (bSeparatorNeeded)
	{
		int iOrder = cairo_dock_get_icon_order (icon);
		if (iOrder + 1 < CAIRO_DOCK_NB_TYPES)
		{
			Icon *pNextIcon = cairo_dock_get_next_icon (pDock->icons, icon);
			if (pNextIcon != NULL && ((pNextIcon->iType - icon->iType) % 2 == 0))
			{
				int iSeparatorType = iOrder + 1;
				cd_message (" insertion de %s -> iSeparatorType : %d", icon->acName, iSeparatorType);

				cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
				Icon *pSeparatorIcon = cairo_dock_create_separator_icon (pSourceContext, iSeparatorType, pDock, bApplyRatio);
				if (pSeparatorIcon != NULL)
				{
					pDock->icons = g_list_insert_sorted (pDock->icons,
						pSeparatorIcon,
						(GCompareFunc) cairo_dock_compare_icons_order);
					pDock->fFlatDockWidth += g_iIconGap + pSeparatorIcon->fWidth;
					pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, pSeparatorIcon->fHeight);
				}
				cairo_destroy (pSourceContext);
			}
		}
		if (iOrder > 1)
		{
			Icon *pPrevIcon = cairo_dock_get_previous_icon (pDock->icons, icon);
			if (pPrevIcon != NULL && ((pPrevIcon->iType - icon->iType) % 2 == 0))
			{
				int iSeparatorType = iOrder - 1;
				cd_message (" insertion de %s -> iSeparatorType : %d", icon->acName, iSeparatorType);

				cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
				Icon *pSeparatorIcon = cairo_dock_create_separator_icon (pSourceContext, iSeparatorType, pDock, bApplyRatio);
				if (pSeparatorIcon != NULL)
				{
					pDock->icons = g_list_insert_sorted (pDock->icons,
						pSeparatorIcon,
						(GCompareFunc) cairo_dock_compare_icons_order);
					pDock->fFlatDockWidth += g_iIconGap + pSeparatorIcon->fWidth;
					pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, pSeparatorIcon->fHeight);
				}
				cairo_destroy (pSourceContext);
			}
		}
	}
	
	//\______________ On effectue les actions demandees.
	if (bAnimated)
		icon->fPersonnalScale = - 0.95;

	if (bUpdateSize)
		cairo_dock_update_dock_size (pDock);

	if (pDock->bIsMainDock && g_bReserveSpace && bUpdateSize && ! pDock->bAutoHide && (pDock->fFlatDockWidth != iPreviousMinWidth || pDock->iMaxIconHeight != iPreviousMaxIconHeight))  // && ! pDock->bInside
		cairo_dock_reserve_space_for_dock (pDock, TRUE);
}



void cairo_dock_reserve_space_for_dock (CairoDock *pDock, gboolean bReserve)
{
	Window Xid = GDK_WINDOW_XID (pDock->pWidget->window);
	int left=0, right=0, top=0, bottom=0;
	int left_start_y=0, left_end_y=0, right_start_y=0, right_end_y=0, top_start_x=0, top_end_x=0, bottom_start_x=0, bottom_end_x=0;
	int iHeight, iWidth;

	if (bReserve)
	{
		int iWindowPositionX = pDock->iWindowPositionX, iWindowPositionY = pDock->iWindowPositionY;
		cairo_dock_get_window_position_and_geometry_at_balance (pDock, (pDock->bAutoHide ? CAIRO_DOCK_MIN_SIZE : CAIRO_DOCK_NORMAL_SIZE), &iWidth, &iHeight);
		if (pDock->bDirectionUp)
		{
			if (pDock->bHorizontalDock)
			{
				bottom = iHeight + pDock->iGapY;
				bottom_start_x = pDock->iWindowPositionX;
				bottom_end_x = pDock->iWindowPositionX + iWidth;

			}
			else
			{
				right = iHeight + pDock->iGapY;
				right_start_y = pDock->iWindowPositionX;
				right_end_y = pDock->iWindowPositionX + iWidth;
			}
		}
		else
		{

			if (pDock->bHorizontalDock)
			{
				top = iHeight + pDock->iGapY;
				top_start_x = pDock->iWindowPositionX;
				top_end_x = pDock->iWindowPositionX + iWidth;
			}
			else
			{
				left = iHeight + pDock->iGapY;
				left_start_y = pDock->iWindowPositionX;
				left_end_y = pDock->iWindowPositionX + iWidth;
			}
		}
		pDock->iWindowPositionX = iWindowPositionX;
		pDock->iWindowPositionY = iWindowPositionY;
	}

	cairo_dock_set_strut_partial (Xid, left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);

	if ((bReserve && ! pDock->bDirectionUp) || (g_iWmHint == GDK_WINDOW_TYPE_HINT_DOCK))  // merci a Robrob pour le patch !
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_DOCK");  // gtk_window_set_type_hint ne marche que sur une fenetre avant de la rendre visible !
	else if (g_iWmHint == GDK_WINDOW_TYPE_HINT_NORMAL)
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_NORMAL");  // idem.
	else if (g_iWmHint == GDK_WINDOW_TYPE_HINT_TOOLBAR)
		cairo_dock_set_xwindow_type_hint (Xid, "_NET_WM_WINDOW_TYPE_TOOLBAR");  // idem.
}


void cairo_dock_place_root_dock (CairoDock *pDock)
{
	int iNewWidth, iNewHeight;
	if (pDock->bAutoHide && pDock->iRefCount == 0)
	{
		cairo_dock_get_window_position_and_geometry_at_balance (pDock, CAIRO_DOCK_MIN_SIZE, &iNewWidth, &iNewHeight);
		pDock->fFoldingFactor = (g_bAnimateOnAutoHide ? g_fUnfoldAcceleration : 0);
	}
	else
	{
		pDock->fFoldingFactor = 0;
		cairo_dock_get_window_position_and_geometry_at_balance (pDock, CAIRO_DOCK_NORMAL_SIZE, &iNewWidth, &iNewHeight);
	}

	if (pDock->bHorizontalDock)
		gdk_window_move_resize (pDock->pWidget->window,
			pDock->iWindowPositionX,
			pDock->iWindowPositionY,
			iNewWidth,
			iNewHeight);
	else
		gdk_window_move_resize (pDock->pWidget->window,
			pDock->iWindowPositionY,
			pDock->iWindowPositionX,
			iNewHeight,
			iNewWidth);
}

void cairo_dock_prevent_dock_from_out_of_screen (CairoDock *pDock)
{
	int x, y;  // position du point invariant du dock.
	x = pDock->iWindowPositionX +  pDock->iCurrentWidth * pDock->fAlign;
	y = (pDock->bDirectionUp ? pDock->iWindowPositionY + pDock->iCurrentHeight : pDock->iWindowPositionY);
	cd_message ("%s (%d;%d)", __func__, x, y);
	
	pDock->iGapX = x - g_iScreenWidth[pDock->bHorizontalDock] * pDock->fAlign;
	pDock->iGapY = (pDock->bDirectionUp ? g_iScreenHeight[pDock->bHorizontalDock] - y : y);
	cd_message (" -> (%d;%d)", pDock->iGapX, pDock->iGapY);
	
	if (pDock->iGapX < - g_iScreenWidth[pDock->bHorizontalDock]/2)
		pDock->iGapX = - g_iScreenWidth[pDock->bHorizontalDock]/2;
	if (pDock->iGapX > g_iScreenWidth[pDock->bHorizontalDock]/2)
		pDock->iGapX = g_iScreenWidth[pDock->bHorizontalDock]/2;
	if (pDock->iGapY < 0)
		pDock->iGapY = 0;
	if (pDock->iGapY > g_iScreenHeight[pDock->bHorizontalDock])
		pDock->iGapY = g_iScreenHeight[pDock->bHorizontalDock];
}


void cairo_dock_allow_widget_to_receive_data (GtkWidget *pWidget, GCallback pCallBack, gpointer data)
{
	GtkTargetEntry *pTargetEntry = g_new0 (GtkTargetEntry, 6);
	pTargetEntry[0].target = "text/*";
	pTargetEntry[0].flags = (GtkTargetFlags) 0;
	pTargetEntry[0].info = 0;
	pTargetEntry[1].target = "text/uri-list";
	pTargetEntry[2].target = "text/plain";
	pTargetEntry[3].target = "text/plain;charset=UTF-8";
	pTargetEntry[4].target = "text/directory";
	pTargetEntry[5].target = "text/html";
	gtk_drag_dest_set (pWidget,
		GTK_DEST_DEFAULT_DROP | GTK_DEST_DEFAULT_MOTION,  // GTK_DEST_DEFAULT_HIGHLIGHT ne rend pas joli je trouve.
		pTargetEntry,
		6,
		GDK_ACTION_COPY | GDK_ACTION_MOVE);  // le 'GDK_ACTION_MOVE' c'est pour KDE.
	g_free (pTargetEntry);

	g_signal_connect (G_OBJECT (pWidget),
		"drag_data_received",
		pCallBack,
		data);
}

void cairo_dock_notify_drop_data (gchar *cReceivedData, Icon *pPointedIcon, double fOrder, CairoContainer *pContainer)
{
	g_return_if_fail (cReceivedData != NULL);
	gchar *cData = NULL;
	gchar *str;
	do
	{
		cData = cReceivedData;
		if (*cReceivedData == '\0')
			break ;
		
		str = strchr (cReceivedData, '\n');
		if (str != NULL)
		{
			*str = '\0';
			if (str != cReceivedData && *(str - 1) == '\r')
				*(str - 1) = '\0';
			cReceivedData = str + 1;
		}
		
		cd_debug (" notification de drop '%s'", cData);
		
		cairo_dock_notify (CAIRO_DOCK_DROP_DATA, cData, pPointedIcon, fOrder, pContainer);
	} while (str != NULL);
}
