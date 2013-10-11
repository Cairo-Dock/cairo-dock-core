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
#include <math.h>

#include <cairo.h>
#include <gtk/gtk.h>
#include <GL/gl.h>
#if GTK_CHECK_VERSION (3, 10, 0)
#include "gtk3imagemenuitem.h"
#endif

#include "cairo-dock-icon-facility.h"  // cairo_dock_compute_icon_area
#include "cairo-dock-dock-facility.h"  // cairo_dock_is_hidden
#include "cairo-dock-dock-manager.h"  // gldi_dock_get
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-config.h"
#include "cairo-dock-utils.h"  // cairo_dock_string_is_address
#include "cairo-dock-windows-manager.h"  // gldi_windows_get_active
#include "cairo-dock-opengl.h"
#include "cairo-dock-animations.h"  // cairo_dock_animation_will_be_visible
#include "cairo-dock-desktop-manager.h"  // gldi_desktop_get_width
#include "cairo-dock-menu.h"  // gldi_menu_new
#define _MANAGER_DEF_
#include "cairo-dock-container.h"

// public (manager, config, data)
GldiContainersParam myContainersParam;
GldiManager myContainersMgr;
GldiObjectManager myContainerObjectMgr;
GldiContainer *g_pPrimaryContainer = NULL;
GldiDesktopBackground *g_pFakeTransparencyDesktopBg = NULL;

// dependancies
extern CairoDockGLConfig g_openglConfig;
extern gboolean g_bUseOpenGL;
extern CairoDockHidingEffect *g_pHidingBackend;  // cairo_dock_is_hidden
extern CairoDock *g_pMainDock;  // for the default dock visibility when composite goes off->on

// private
static gboolean s_bSticky = TRUE;
static gboolean s_bInitialOpacity0 = TRUE;  // set initial window opacity to 0, to avoid grey rectangles.
static gboolean s_bNoComposite = FALSE;
static GldiContainerManagerBackend s_backend;


void cairo_dock_set_containers_non_sticky (void)
{
	if (g_pPrimaryContainer != NULL)
	{
		cd_warning ("this function has to be called before any container is created.");
		return;
	}
	s_bSticky = FALSE;
}

void cairo_dock_disable_containers_opacity (void)
{
	if (g_pPrimaryContainer != NULL)
	{
		cd_warning ("this function has to be called before any container is created.");
		return;
	}
	s_bInitialOpacity0 = FALSE;
}


static gboolean _prevent_delete (G_GNUC_UNUSED GtkWidget *pWidget, G_GNUC_UNUSED GdkEvent *event, G_GNUC_UNUSED gpointer data)
{
	cd_debug ("No alt+f4");
	return TRUE;  // on empeche les ALT+F4 malheureux.
}

void cairo_dock_set_default_rgba_visual (GtkWidget *pWidget)
{
	GdkScreen* pScreen = gtk_widget_get_screen (pWidget);
	
	#if (GTK_MAJOR_VERSION < 3)
	GdkColormap* pColormap = gdk_screen_get_rgba_colormap (pScreen);
	if (!pColormap)
		pColormap = gdk_screen_get_rgb_colormap (pScreen);
	
	gtk_widget_set_colormap (pWidget, pColormap);
	#else
	GdkVisual *pGdkVisual = gdk_screen_get_rgba_visual (pScreen);
	if (pGdkVisual == NULL)
		pGdkVisual = gdk_screen_get_system_visual (pScreen);
	
	gtk_widget_set_visual (pWidget, pGdkVisual);
	#endif
}

static gboolean _cairo_default_container_animation_loop (GldiContainer *pContainer)
{
	gboolean bContinue = FALSE;
	
	gboolean bUpdateSlowAnimation = FALSE;
	pContainer->iAnimationStep ++;
	if (pContainer->iAnimationStep * pContainer->iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pContainer->iAnimationStep = 0;
		pContainer->bKeepSlowAnimation = FALSE;
	}
	
	if (bUpdateSlowAnimation)
	{
		gldi_object_notify (pContainer, NOTIFICATION_UPDATE_SLOW, pContainer, &pContainer->bKeepSlowAnimation);
	}
	
	gldi_object_notify (pContainer, NOTIFICATION_UPDATE, pContainer, &bContinue);
	
	if (! bContinue && ! pContainer->bKeepSlowAnimation)
	{
		pContainer->iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}

static gboolean _set_opacity (GtkWidget *pWidget,
#if (GTK_MAJOR_VERSION < 3)
	G_GNUC_UNUSED GdkEventExpose *pExpose,
#else
	G_GNUC_UNUSED cairo_t *ctx,
#endif
	GldiContainer *pContainer)
{
	if (pContainer->iWidth != 1 ||pContainer->iHeight != 1)
	{
		g_signal_handlers_disconnect_by_func (pWidget, _set_opacity, pContainer);  // we'll never need to pass here any more, so simply disconnect ourselves.
		//g_print ("____OPACITY 1 (%dx%d)\n", pContainer->iWidth, pContainer->iHeight);
		#if GTK_CHECK_VERSION (3, 8, 0)
		gtk_widget_set_opacity (pWidget, 1.);
		#else
		gtk_window_set_opacity (GTK_WINDOW (pWidget), 1.);
		#endif
	}
	return FALSE ;
}

static void _remove_background (G_GNUC_UNUSED GtkWidget *pWidget, GldiContainer *pContainer)
{
	#if (GTK_MAJOR_VERSION < 3)
	gdk_window_set_back_pixmap (gldi_container_get_gdk_window (pContainer), NULL, FALSE);
	#else
	gdk_window_set_background_pattern (gldi_container_get_gdk_window (pContainer), NULL);  // window must be realized (shown)
	#endif
}

void cairo_dock_redraw_container (GldiContainer *pContainer)
{
	g_return_if_fail (pContainer != NULL);
	GdkRectangle rect = {0, 0, pContainer->iWidth, pContainer->iHeight};
	if (! pContainer->bIsHorizontal)
	{
		rect.width = pContainer->iHeight;
		rect.height = pContainer->iWidth;
	}
	cairo_dock_redraw_container_area (pContainer, &rect);
}

static inline void _redraw_container_area (GldiContainer *pContainer, GdkRectangle *pArea)
{
	g_return_if_fail (pContainer != NULL);
	if (! gldi_container_is_visible (pContainer))
		return ;
	
	if (pArea->y < 0)
		pArea->y = 0;
	if (pContainer->bIsHorizontal && pArea->y + pArea->height > pContainer->iHeight)
		pArea->height = pContainer->iHeight - pArea->y;
	else if (! pContainer->bIsHorizontal && pArea->x + pArea->width > pContainer->iHeight)
		pArea->width = pContainer->iHeight - pArea->x;
	
	if (pArea->width > 0 && pArea->height > 0)
		gdk_window_invalidate_rect (gldi_container_get_gdk_window (pContainer), pArea, FALSE);
}

void cairo_dock_redraw_container_area (GldiContainer *pContainer, GdkRectangle *pArea)
{
	if (CAIRO_DOCK_IS_DOCK (pContainer) && ! cairo_dock_animation_will_be_visible (CAIRO_DOCK (pContainer)))  // inutile de redessiner.
		return ;
	_redraw_container_area (pContainer, pArea);
}

void cairo_dock_redraw_icon (Icon *icon)
{
	g_return_if_fail (icon != NULL);
	GldiContainer *pContainer = cairo_dock_get_icon_container (icon);
	g_return_if_fail (pContainer != NULL);
	GdkRectangle rect;
	cairo_dock_compute_icon_area (icon, pContainer, &rect);
	
	if (CAIRO_DOCK_IS_DOCK (pContainer) &&
		( (cairo_dock_is_hidden (CAIRO_DOCK (pContainer)) && ! icon->bIsDemandingAttention && ! icon->bAlwaysVisible)
		|| (CAIRO_DOCK (pContainer)->iRefCount != 0 && ! gldi_container_is_visible (pContainer)) ) )  // inutile de redessiner.
		return ;
	_redraw_container_area (pContainer, &rect);
}


void cairo_dock_allow_widget_to_receive_data (GtkWidget *pWidget, GCallback pCallBack, gpointer data)
{
	// /*GtkTargetEntry pTargetEntry[6] = {0};
	// pTargetEntry[0].target = (gchar*)"text/*";
	/* pTargetEntry[0].flags = (GtkTargetFlags) 0;
	pTargetEntry[0].info = 0;
	pTargetEntry[1].target = (gchar*)"text/uri-list";
	pTargetEntry[2].target = (gchar*)"text/plain";
	pTargetEntry[3].target = (gchar*)"text/plain;charset=UTF-8";
	pTargetEntry[4].target = (gchar*)"text/directory";
	pTargetEntry[5].target = (gchar*)"text/html";
	gtk_drag_dest_set (pWidget,
		GTK_DEST_DEFAULT_DROP | GTK_DEST_DEFAULT_MOTION,  // GTK_DEST_DEFAULT_HIGHLIGHT ne rend pas joli je trouve.
		pTargetEntry,
		6,
		GDK_ACTION_COPY | GDK_ACTION_MOVE);  // le 'GDK_ACTION_MOVE' c'est pour KDE.*/
	gtk_drag_dest_set (pWidget,
		GTK_DEST_DEFAULT_DROP | GTK_DEST_DEFAULT_MOTION,  // GTK_DEST_DEFAULT_HIGHLIGHT ne rend pas joli je trouve.
		NULL,
		0,
		GDK_ACTION_COPY | GDK_ACTION_MOVE);  // le 'GDK_ACTION_MOVE' c'est pour KDE.
	gtk_drag_dest_add_uri_targets (pWidget);
	gtk_drag_dest_add_text_targets (pWidget);
	
	g_signal_connect (G_OBJECT (pWidget),
		"drag_data_received",
		pCallBack,
		data);
}

void gldi_container_disable_drop (GldiContainer *pContainer)
{
	gtk_drag_dest_set_target_list (pContainer->pWidget, NULL);
}

void gldi_container_notify_drop_data (GldiContainer *pContainer, gchar *cReceivedData, Icon *pPointedIcon, double fOrder)
{
	g_return_if_fail (cReceivedData != NULL);
	gchar *cData = NULL;
	
	gchar **cStringList = g_strsplit (cReceivedData, "\n", -1);
	GString *sArg = g_string_new ("");
	int i=0, j;
	while (cStringList[i] != NULL)
	{
		g_string_assign (sArg, cStringList[i]);
		
		if (! cairo_dock_string_is_address (cStringList[i]))
		{
			j = i + 1;
			while (cStringList[j] != NULL)
			{
				if (cairo_dock_string_is_address (cStringList[j]))
					break ;
				g_string_append_printf (sArg, "\n%s", cStringList[j]);
				j ++;
			}
			i = j;
		}
		else
		{
			cd_debug (" + adresse");
			if (sArg->str[sArg->len-1] == '\r')
			{
				cd_debug ("retour charriot");
				sArg->str[sArg->len-1] = '\0';
			}
			i ++;
		}
		
		cData = sArg->str;
		cd_debug (" notification de drop '%s'", cData);
		gldi_object_notify (pContainer, NOTIFICATION_DROP_DATA, cData, pPointedIcon, fOrder, pContainer);
	}
	
	g_strfreev (cStringList);
	g_string_free (sArg, TRUE);
}


void gldi_container_reserve_space (GldiContainer *pContainer, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x)
{
	if (s_backend.reserve_space)
		s_backend.reserve_space (pContainer, left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x);
}

int gldi_container_get_current_desktop_index (GldiContainer *pContainer)
{
	if (s_backend.get_current_desktop_index)
		return s_backend.get_current_desktop_index (pContainer);
	return 0;
}

void gldi_container_move (GldiContainer *pContainer, int iNumDesktop, int iAbsolutePositionX, int iAbsolutePositionY)
{
	if (s_backend.move)
		s_backend.move (pContainer, iNumDesktop, iAbsolutePositionX, iAbsolutePositionY);
}

gboolean gldi_container_is_active (GldiContainer *pContainer)
{
	if (s_backend.is_active)
		return s_backend.is_active (pContainer);
	return FALSE;
}

void gldi_container_present (GldiContainer *pContainer)
{
	if (s_backend.present)
		s_backend.present (pContainer);
}

void gldi_container_manager_register_backend (GldiContainerManagerBackend *pBackend)
{
	gpointer *ptr = (gpointer*)&s_backend;
	gpointer *src = (gpointer*)pBackend;
	gpointer *src_end = (gpointer*)(pBackend + 1);
	while (src != src_end)
	{
		if (*src != NULL)
			*ptr = *src;
		src ++;
		ptr ++;
	}
}


gboolean cairo_dock_emit_signal_on_container (GldiContainer *pContainer, const gchar *cSignal)
{
	static gboolean bReturn;
	g_signal_emit_by_name (pContainer->pWidget, cSignal, NULL, &bReturn);
	return FALSE;
}
gboolean cairo_dock_emit_leave_signal (GldiContainer *pContainer)
{
	// actualize the coordinates of the pointer, since they are most probably out-dated (because the mouse has left the dock, or because a right-click generates an event with (0;0) coordinates)
	gldi_container_update_mouse_position (pContainer);
	return cairo_dock_emit_signal_on_container (pContainer, "leave-notify-event");
}
gboolean cairo_dock_emit_enter_signal (GldiContainer *pContainer)
{
	return cairo_dock_emit_signal_on_container (pContainer, "enter-notify-event");
}


static GtkWidget *s_pMenu = NULL;  // right-click menu
GtkWidget *gldi_container_build_menu (GldiContainer *pContainer, Icon *icon)
{
	if (s_pMenu != NULL)
	{
		//g_print ("previous menu still alive\n");
		gtk_widget_destroy (GTK_WIDGET (s_pMenu));  // -> 's_pMenu' becomes NULL thanks to the weak pointer.
	}
	g_return_val_if_fail (pContainer != NULL, NULL);
	
	//\_________________________ On construit le menu.
	GtkWidget *menu = gldi_menu_new (icon);
	
	//\_________________________ On passe la main a ceux qui veulent y rajouter des choses.
	gboolean bDiscardMenu = FALSE;
	gldi_object_notify (pContainer, NOTIFICATION_BUILD_CONTAINER_MENU, icon, pContainer, menu, &bDiscardMenu);
	if (bDiscardMenu)
	{
		gtk_widget_destroy (menu);
		return NULL;
	}
	
	gldi_object_notify (pContainer, NOTIFICATION_BUILD_ICON_MENU, icon, pContainer, menu);
	
	s_pMenu = menu;
	g_object_add_weak_pointer (G_OBJECT (menu), (gpointer*)&s_pMenu);  // will nullify 's_pMenu' as soon as the menu is destroyed.
	return menu;
}


GldiShape *gldi_container_create_input_shape (GldiContainer *pContainer, int x, int y, int w, int h)
{
	if (pContainer->iWidth == 0 || pContainer->iHeight == 0)  // very unlikely to happen, but anyway avoid this case.
		return NULL;
	
	#if (GTK_MAJOR_VERSION < 3)
	int W, H;
	if (pContainer->bIsHorizontal)
	{
		W = pContainer->iWidth;
		H = pContainer->iHeight;
	}
	else
	{
		W = pContainer->iHeight;
		H = pContainer->iWidth;
	}
	GdkBitmap *pShapeBitmap = (GdkBitmap*) gdk_pixmap_new (NULL,
		W,
		H,
		1);

	cairo_t *pCairoContext = gdk_cairo_create (pShapeBitmap);
	g_return_val_if_fail (pCairoContext != NULL, NULL);  // if no context, abort (https://bugs.launchpad.net/cairo-dock-plug-ins/+bug/861725)
	cairo_set_source_rgba (pCairoContext, 0.0f, 0.0f, 0.0f, 0.0f);
	cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pCairoContext);
	
	cairo_set_source_rgba (pCairoContext, 1., 1., 1., 1.);
	cairo_rectangle (pCairoContext,
		x,
		y,
		w,
		h);
	cairo_fill (pCairoContext);
	cairo_destroy (pCairoContext);
	#else
	cairo_rectangle_int_t rect = {x, y, w, h};
	cairo_region_t *pShapeBitmap = cairo_region_create_rectangle (&rect);  // for a more complex shape, we would need to draw it on a cairo_surface_t, and then make it a region with gdk_cairo_region_from_surface().
	#endif
	return pShapeBitmap;
}


  ////////////
 /// INIT ///
////////////

static CairoDockVisibility s_iPrevVisibility = CAIRO_DOCK_NB_VISI;
static void _set_visibility (CairoDock *pDock, gpointer data)
{
	gldi_dock_set_visibility (pDock, GPOINTER_TO_INT (data));
}
static void _enable_fake_transparency (void)
{
	g_pFakeTransparencyDesktopBg = gldi_desktop_background_get (g_bUseOpenGL);
	s_bNoComposite = TRUE;
	s_iPrevVisibility = g_pMainDock->iVisibility;
	gldi_docks_foreach_root ((GFunc)_set_visibility, GINT_TO_POINTER (CAIRO_DOCK_VISI_KEEP_BELOW));  // set the visibility to 'keep below'; that's the best compromise between accessibility and visual annoyance.
}
static void _on_composited_changed (GdkScreen *pScreen, G_GNUC_UNUSED gpointer data)
{
	if (!gdk_screen_is_composited (pScreen) || (g_bUseOpenGL && ! g_openglConfig.bAlphaAvailable))
	{
		_enable_fake_transparency ();
	}
	else  // composite is now ON => disable fake transparency
	{
		gldi_desktop_background_destroy (g_pFakeTransparencyDesktopBg);
		s_bNoComposite = FALSE;
		g_pFakeTransparencyDesktopBg = NULL;
		if (s_iPrevVisibility < CAIRO_DOCK_NB_VISI)
			gldi_docks_foreach_root ((GFunc)_set_visibility, GINT_TO_POINTER (s_iPrevVisibility));  // restore the previous visibility.
	}
}
static gboolean _check_composite_delayed (G_GNUC_UNUSED gpointer data)
{
	GdkScreen *pScreen = gdk_screen_get_default ();
	if (!gdk_screen_is_composited (pScreen) || (g_bUseOpenGL && ! g_openglConfig.bAlphaAvailable))  // no composite available -> load the desktop background
	{
		cd_message ("Composite is not available");
		/**g_pFakeTransparencyDesktopBg = gldi_desktop_background_get (g_bUseOpenGL);  // we don't modify the visibility on startup; if it's the first launch, the user has to notice the problem. and if it's not, just respect his configuration.
		s_bNoComposite = TRUE;*/
		_enable_fake_transparency ();  // modify the visibility even on startup, because there is no configuration that is really usable except for 'keep-below'
	}
	g_signal_connect (pScreen, "composited-changed", G_CALLBACK (_on_composited_changed), NULL);
	return FALSE;
}
static void init (void)
{
	g_timeout_add_seconds (4, _check_composite_delayed, NULL);  // we don't want to be annoyed by the activation of the composite on startup
}

  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, GldiContainersParam *pContainersParam)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	int iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "opengl anim freq", &bFlushConfFileNeeded, 33, NULL, NULL);
	pContainersParam->iGLAnimationDeltaT = 1000. / iRefreshFrequency;
	
	iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "cairo anim freq", &bFlushConfFileNeeded, 25, NULL, NULL);
	pContainersParam->iCairoAnimationDeltaT = 1000. / iRefreshFrequency;
	
	return bFlushConfFileNeeded;
}

  ////////////
 /// LOAD ///
////////////

static void load (void)
{
	if (s_bNoComposite)
	{
		g_pFakeTransparencyDesktopBg = gldi_desktop_background_get (g_bUseOpenGL);
	}
}

  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	gldi_desktop_background_destroy (g_pFakeTransparencyDesktopBg);  // destroy it, since it will be unloaded anyway by the desktop-manager
	g_pFakeTransparencyDesktopBg = NULL;
}

  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	GldiContainer *pContainer = (GldiContainer*)obj;
	GldiContainerAttr *cattr = (GldiContainerAttr*)attr;
	
	pContainer->iface.animation_loop = _cairo_default_container_animation_loop;
	pContainer->fRatio = 1;
	pContainer->bIsHorizontal = TRUE;
	pContainer->bDirectionUp = TRUE;
	
	// create a window
	GtkWidget* pWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	pContainer->pWidget = pWindow;
	gtk_window_set_default_size (GTK_WINDOW (pWindow), 1, 1);  // this should prevent having grey rectangles during the loading, when the window is mapped and rendered by the WM but not yet by us.
	gtk_window_resize (GTK_WINDOW (pWindow), 1, 1);
	gtk_widget_set_app_paintable (pWindow, TRUE);
	gtk_window_set_decorated (GTK_WINDOW (pWindow), FALSE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW(pWindow), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pWindow), TRUE);
	if (s_bSticky)
		gtk_window_stick (GTK_WINDOW (pWindow));
	g_signal_connect (G_OBJECT (pWindow),
		"delete-event",
		G_CALLBACK (_prevent_delete),
		NULL);
	gtk_window_get_size (GTK_WINDOW (pWindow), &pContainer->iWidth, &pContainer->iHeight);  // it's only the initial size allocated by GTK.
	
	// set an RGBA visual for cairo or opengl
	if (g_bUseOpenGL && ! cattr->bNoOpengl)
	{
		gldi_gl_container_init (pContainer);
		pContainer->iAnimationDeltaT = myContainersParam.iGLAnimationDeltaT;
	}
	else
	{
		cairo_dock_set_default_rgba_visual (pWindow);
		pContainer->iAnimationDeltaT = myContainersParam.iCairoAnimationDeltaT;
	}
	if (pContainer->iAnimationDeltaT == 0)
		pContainer->iAnimationDeltaT = 30;
	
	// set the opacity to 0 to avoid seeing grey rectangles until the window is ready to be painted by us.
	if (s_bInitialOpacity0)
	{
		#if GTK_CHECK_VERSION (3, 8, 0)
		gtk_widget_set_opacity (pWindow, 0.);
		#else
		gtk_window_set_opacity (GTK_WINDOW (pWindow), 0.);
		#endif
		g_signal_connect (G_OBJECT (pWindow),
			#if (GTK_MAJOR_VERSION < 3)
			"expose-event",
			#else
			"draw",
			#endif
			G_CALLBACK (_set_opacity),
			pContainer);  // the callback will be removed once it has done its job.
	}
	g_signal_connect (G_OBJECT (pWindow),
		"realize",
		G_CALLBACK (_remove_background),
		pContainer);
	
	// remove the resize grip added by gtk3 (it's also possible that this grip has been backported to gtk+-2.0 (e.g. in Ubuntu Natty...))
	#if (GTK_MAJOR_VERSION >= 3 || ENABLE_GTK_GRIP == 1)
	gtk_window_set_has_resize_grip (GTK_WINDOW(pWindow), FALSE);
	#endif
	
	// make it the primary container if it's the first
	if (g_pPrimaryContainer == NULL)
		g_pPrimaryContainer = pContainer;
}

static void reset_object (GldiObject *obj)
{
	GldiContainer *pContainer = (GldiContainer*)obj;
	
	// destroy the opengl context
	gldi_gl_container_finish (pContainer);
	
	// destroy the window (will remove all signals)
	gtk_widget_destroy (pContainer->pWidget);
	pContainer->pWidget = NULL;
	
	// stop the animation loop
	if (pContainer->iSidGLAnimation != 0)
	{
		g_source_remove (pContainer->iSidGLAnimation);
		pContainer->iSidGLAnimation = 0;
	}
	
	if (g_pPrimaryContainer == pContainer)
		g_pPrimaryContainer = NULL;
}

void gldi_register_containers_manager (void)
{
	// Manager
	memset (&myContainersMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myContainersMgr), &myManagerObjectMgr, NULL);
	myContainersMgr.cModuleName  = "Containers";
	// interface
	myContainersMgr.init         = init;
	myContainersMgr.load         = load;
	myContainersMgr.unload       = unload;
	myContainersMgr.reload       = (GldiManagerReloadFunc)NULL;
	myContainersMgr.get_config   = (GldiManagerGetConfigFunc)get_config;
	myContainersMgr.reset_config = (GldiManagerResetConfigFunc)NULL;
	// Config
	myContainersMgr.pConfig = (GldiManagerConfigPtr)&myContainersParam;
	myContainersMgr.iSizeOfConfig = sizeof (GldiContainersParam);
	// data
	myContainersMgr.pData = (GldiManagerDataPtr)NULL;
	myContainersMgr.iSizeOfData = 0;
	
	// Object Manager
	memset (&myContainerObjectMgr, 0, sizeof (GldiObjectManager));
	myContainerObjectMgr.cName        = "Container";
	myContainerObjectMgr.iObjectSize  = sizeof (GldiContainer);
	// interface
	myContainerObjectMgr.init_object  = init_object;
	myContainerObjectMgr.reset_object = reset_object;
	// signals
	gldi_object_install_notifications (&myContainerObjectMgr, NB_NOTIFICATIONS_CONTAINER);
}
