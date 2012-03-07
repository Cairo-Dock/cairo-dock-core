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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cairo.h>

#include <GL/gl.h> 

#include "cairo-dock-notifications.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_compute_icon_area
#include "cairo-dock-dock-facility.h"  // cairo_dock_is_hidden
#include "cairo-dock-module-factory.h"  // cairo_dock_search_container_from_icon
#include "cairo-dock-dock-manager.h"  // cairo_dock_search_dock_from_name
#include "cairo-dock-desklet-factory.h"  // cairo_dock_search_container_from_icon
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-config.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-animations.h"  // cairo_dock_animation_will_be_visible
#include "cairo-dock-X-manager.h"  // g_desktopGeometry
#define _MANAGER_DEF_
#include "cairo-dock-container.h"

// public (manager, config, data)
CairoContainersParam myContainersParam;
CairoContainersManager myContainersMgr;
CairoContainer *g_pPrimaryContainer = NULL;
CairoDockDesktopBackground *g_pFakeTransparencyDesktopBg = NULL;
gboolean g_bUseGlitz = FALSE;

// dependancies
extern CairoDockGLConfig g_openglConfig;
extern gboolean g_bUseOpenGL;
extern CairoDockHidingEffect *g_pHidingBackend;  // cairo_dock_is_hidden
extern CairoDockDesktopGeometry g_desktopGeometry;  // _place_menu_on_icon

// private
static gboolean s_bSticky = TRUE;


void cairo_dock_set_containers_non_sticky (void)
{
	if (g_pPrimaryContainer != NULL)
	{
		cd_warning ("this function has to be called before any container is created.");
		return;
	}
	s_bSticky = FALSE;
}


static gboolean _prevent_delete (GtkWidget *pWidget, GdkEvent *event, gpointer data)
{
	cd_debug ("pas de alt+f4");
	return TRUE;  // on empeche les ALT+F4 malheureux.
}

static void cairo_dock_set_default_rgba_visual (GtkWidget *pWidget)
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

static gboolean _cairo_default_container_animation_loop (CairoContainer *pContainer)
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
		cairo_dock_notify_on_object (pContainer, NOTIFICATION_UPDATE_SLOW, pContainer, &pContainer->bKeepSlowAnimation);
	}
	
	cairo_dock_notify_on_object (pContainer, NOTIFICATION_UPDATE, pContainer, &bContinue);
	
	if (! bContinue && ! pContainer->bKeepSlowAnimation)
	{
		pContainer->iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}

GtkWidget *cairo_dock_init_container_full (CairoContainer *pContainer, gboolean bOpenGLWindow)
{
	GtkWidget* pWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	pContainer->pWidget = pWindow;
	
	if (!pContainer->iface.animation_loop)
		pContainer->iface.animation_loop = _cairo_default_container_animation_loop;
	
	if (g_bUseOpenGL && bOpenGLWindow)
	{
		gldi_glx_init_container (pContainer);
		pContainer->iAnimationDeltaT = myContainersParam.iGLAnimationDeltaT;
	}
	else
	{
		cairo_dock_set_default_rgba_visual (pWindow);
		pContainer->iAnimationDeltaT = myContainersParam.iCairoAnimationDeltaT;
	}
	if (pContainer->iAnimationDeltaT == 0)
		pContainer->iAnimationDeltaT = 30;
	pContainer->fRatio = 1;
	
	g_signal_connect (G_OBJECT (pWindow),
		"delete-event",
		G_CALLBACK (_prevent_delete),
		NULL);
	
	gtk_window_set_default_size (GTK_WINDOW (pWindow), 1, 1);  // this should prevent having grey rectangles during the loading, when the window is mapped and rendered by the WM but not yet by us.
	gtk_widget_set_app_paintable (pWindow, TRUE);
	gtk_window_set_decorated (GTK_WINDOW (pWindow), FALSE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW(pWindow), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pWindow), TRUE);
	if (s_bSticky)
		gtk_window_stick (GTK_WINDOW (pWindow));
	
	// needed since gtk+-3.0 but it's possible that this resize grip has been backported to gtk+-2.0 (e.g. in Ubuntu Natty...)
	#if (GTK_MAJOR_VERSION >= 3 || ENABLE_GTK_GRIP == 1)
	gtk_window_set_has_resize_grip (GTK_WINDOW(pWindow), FALSE);
	#endif
	
	gldi_object_set_manager (GLDI_OBJECT (pContainer), GLDI_MANAGER (&myContainersMgr));  // the implementation of the container will set its manager on top of this one.
	
	if (g_pPrimaryContainer == NULL)
	{
		g_pPrimaryContainer = pContainer;
	}
	return pWindow;
}

void cairo_dock_finish_container (CairoContainer *pContainer)
{
	gldi_glx_finish_container (pContainer);
	
	gtk_widget_destroy (pContainer->pWidget);  // enleve les signaux.
	pContainer->pWidget = NULL;
	if (pContainer->iSidGLAnimation != 0)
	{
		g_source_remove (pContainer->iSidGLAnimation);
		pContainer->iSidGLAnimation = 0;
	}
	cairo_dock_notify_on_object (pContainer, NOTIFICATION_DESTROY, pContainer);
	cairo_dock_clear_notifications_on_object (pContainer);
	
	if (g_pPrimaryContainer == pContainer)
		g_pPrimaryContainer = NULL;
}


void cairo_dock_redraw_container (CairoContainer *pContainer)
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

static inline void _redraw_container_area (CairoContainer *pContainer, GdkRectangle *pArea)
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

void cairo_dock_redraw_container_area (CairoContainer *pContainer, GdkRectangle *pArea)
{
	if (CAIRO_DOCK_IS_DOCK (pContainer) && ! cairo_dock_animation_will_be_visible (CAIRO_DOCK (pContainer)))  // inutile de redessiner.
		return ;
	_redraw_container_area (pContainer, pArea);
}

void cairo_dock_redraw_icon (Icon *icon, CairoContainer *pContainer)
{
	g_return_if_fail (icon != NULL && pContainer != NULL);
	GdkRectangle rect;
	cairo_dock_compute_icon_area (icon, pContainer, &rect);
	
	if (CAIRO_DOCK_IS_DOCK (pContainer) &&
		( (cairo_dock_is_hidden (CAIRO_DOCK (pContainer)) && ! icon->bIsDemandingAttention && ! icon->bAlwaysVisible)
		|| (CAIRO_DOCK (pContainer)->iRefCount != 0 && ! gldi_container_is_visible (pContainer)) ) )  // inutile de redessiner.
		return ;
	_redraw_container_area (pContainer, &rect);
}



static gboolean _cairo_dock_search_icon_in_desklet (CairoDesklet *pDesklet, Icon *icon)
{
	if (pDesklet->icons != NULL)
	{
		Icon *pIcon;
		GList *ic;
		for (ic = pDesklet->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon == icon)
				return TRUE;
		}
	}
	return FALSE;
}
CairoContainer *cairo_dock_search_container_from_icon (Icon *icon)
{
	g_return_val_if_fail (icon != NULL, NULL);
	if (CAIRO_DOCK_IS_APPLET (icon))
	{
		return icon->pModuleInstance->pContainer;
	}
	else if (icon->cParentDockName != NULL)
	{
		return CAIRO_CONTAINER (cairo_dock_search_dock_from_name (icon->cParentDockName));
	}
	else
	{
		CairoDesklet *pDesklet = cairo_dock_foreach_desklet ((CairoDockForeachDeskletFunc)_cairo_dock_search_icon_in_desklet, icon);
		return CAIRO_CONTAINER (pDesklet);
	}
}


void cairo_dock_allow_widget_to_receive_data (GtkWidget *pWidget, GCallback pCallBack, gpointer data)
{
	/*GtkTargetEntry pTargetEntry[6] = {0};
	pTargetEntry[0].target = (gchar*)"text/*";
	pTargetEntry[0].flags = (GtkTargetFlags) 0;
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
	gtk_drag_dest_add_text_targets (pWidget);
	gtk_drag_dest_add_uri_targets (pWidget);
	
	g_signal_connect (G_OBJECT (pWidget),
		"drag_data_received",
		pCallBack,
		data);
}

void gldi_container_disable_drop (CairoContainer *pContainer)
{
	gtk_drag_dest_set_target_list (pContainer->pWidget, NULL);
}

gboolean cairo_dock_string_is_adress (const gchar *cString)
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

void cairo_dock_notify_drop_data (gchar *cReceivedData, Icon *pPointedIcon, double fOrder, CairoContainer *pContainer)
{
	g_return_if_fail (cReceivedData != NULL);
	gchar *cData = NULL;
	
	gchar **cStringList = g_strsplit (cReceivedData, "\n", -1);
	GString *sArg = g_string_new ("");
	int i=0, j;
	while (cStringList[i] != NULL)
	{
		g_string_assign (sArg, cStringList[i]);
		
		if (! cairo_dock_string_is_adress (cStringList[i]))
		{
			j = i + 1;
			while (cStringList[j] != NULL)
			{
				if (cairo_dock_string_is_adress (cStringList[j]))
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
		cairo_dock_notify_on_object (pContainer, NOTIFICATION_DROP_DATA, cData, pPointedIcon, fOrder, pContainer);
	}
	
	g_strfreev (cStringList);
	g_string_free (sArg, TRUE);
}


gboolean cairo_dock_emit_signal_on_container (CairoContainer *pContainer, const gchar *cSignal)
{
	static gboolean bReturn;
	g_signal_emit_by_name (pContainer->pWidget, cSignal, NULL, &bReturn);
	return FALSE;
}
gboolean cairo_dock_emit_leave_signal (CairoContainer *pContainer)
{
	// actualize the coordinates of the pointer, since they are most probably out-dated (because the mouse has left the dock, or because a right-click generates an event with (0;0) coordinates)
	if (pContainer->bIsHorizontal)
		gdk_window_get_pointer (gldi_container_get_gdk_window (pContainer), &pContainer->iMouseX, &pContainer->iMouseY, NULL);
	else
		gdk_window_get_pointer (gldi_container_get_gdk_window (pContainer), &pContainer->iMouseY, &pContainer->iMouseX, NULL);
	return cairo_dock_emit_signal_on_container (pContainer, "leave-notify-event");
}
gboolean cairo_dock_emit_enter_signal (CairoContainer *pContainer)
{
	return cairo_dock_emit_signal_on_container (pContainer, "enter-notify-event");
}


static void _place_menu_on_icon (GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer *data)
{
	*push_in = TRUE;
	Icon *pIcon = data[0];
	CairoContainer *pContainer = data[1];
	int x0 = pContainer->iWindowPositionX + pIcon->fDrawX;
	int y0 = pContainer->iWindowPositionY + pIcon->fDrawY;
	
	int w, h;  // taille menu
	GtkRequisition requisition;
	#if (GTK_MAJOR_VERSION < 3)
	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);
	#else
	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &requisition, NULL);
	#endif
	w = requisition.width;
	h = requisition.height;
	
	//g_print ("%d;%d %dx%d\n", x0, y0, w, h);
	if (pContainer->bIsHorizontal)
	{
		*x = x0;
		if (y0 > g_desktopGeometry.iXScreenHeight[pContainer->bIsHorizontal]/2)  // pContainer->bDirectionUp
			*y = y0 - h;
		else
			*y = y0 + pIcon->fHeight * pIcon->fScale;
	}
	else
	{
		*y = MIN (x0, g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - h);
		if (y0 > g_desktopGeometry.iXScreenHeight[pContainer->bIsHorizontal]/2)  // pContainer->bDirectionUp
			*x = y0 - w;
		else
			*x = y0 + pIcon->fHeight * pIcon->fScale;
	}
}
void cairo_dock_popup_menu_on_icon (GtkWidget *menu, Icon *pIcon, CairoContainer *pContainer)
{
	static gpointer data[2];  // 1 seul menu a la fois, donc on peut la faire statique.
	
	if (menu == NULL)
		return;
	GtkMenuPositionFunc place_menu = NULL;  // if 'place_menu' is NULL, then 'data' is ignored.
	if (pIcon != NULL && pContainer != NULL)
	{
		place_menu = (GtkMenuPositionFunc)_place_menu_on_icon;
		data[0] = pIcon;
		data[1] = pContainer;
	}
	
	if (pContainer->iface.setup_menu)
		pContainer->iface.setup_menu (pContainer, pIcon, menu);
	
	gtk_widget_show_all (GTK_WIDGET (menu));
	
	gtk_menu_popup (GTK_MENU (menu),
		NULL,
		NULL,
		place_menu,
		data,
		1,
		gtk_get_current_event_time ());
}


GtkWidget *cairo_dock_add_in_menu_with_stock_and_data (const gchar *cLabel, const gchar *gtkStock, GCallback pFunction, GtkWidget *pMenu, gpointer pData)
{
	GtkWidget *pMenuItem = gtk_image_menu_item_new_with_label (cLabel);
	if (gtkStock)
	{
		GtkWidget *image = NULL;
		if (*gtkStock == '/')
		{
			GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (gtkStock, 16, 16, NULL);
			image = gtk_image_new_from_pixbuf (pixbuf);
			g_object_unref (pixbuf);
		}
		else
		{
			image = gtk_image_new_from_stock (gtkStock, GTK_ICON_SIZE_MENU);
		}
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (pMenuItem), TRUE);
#endif
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);
	}
	gtk_menu_shell_append  (GTK_MENU_SHELL (pMenu), pMenuItem);
	if (pFunction)
		g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK (pFunction), pData);
	return pMenuItem;
}

GtkWidget *cairo_dock_create_sub_menu (const gchar *cLabel, GtkWidget *pMenu, const gchar *cImage)
{
	GtkWidget *pMenuItem, *image, *pSubMenu = gtk_menu_new ();
	if (cImage == NULL)
	{
		pMenuItem = gtk_image_menu_item_new_with_label (cLabel);
	}
	else
	{
		pMenuItem = gtk_image_menu_item_new_with_label (cLabel);
		if (*cImage == '/')
		{
			GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (cImage, 24, 24, NULL);
			image = gtk_image_new_from_pixbuf (pixbuf);
			g_object_unref (pixbuf);
		}
		else
		{
			image = gtk_image_new_from_stock (cImage, GTK_ICON_SIZE_MENU);
		}
#if (GTK_MAJOR_VERSION > 2 || GTK_MINOR_VERSION >= 16)
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (pMenuItem), TRUE);
#endif
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);
	}
	gtk_menu_shell_append (GTK_MENU_SHELL (pMenu), pMenuItem); 
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenu);
	return pSubMenu; 
}


static GtkWidget *s_pMenu = NULL;  // right-click menu
GtkWidget *cairo_dock_build_menu (Icon *icon, CairoContainer *pContainer)
{
	if (s_pMenu != NULL)
	{
		//g_print ("previous menu still alive\n");
		gtk_widget_destroy (GTK_WIDGET (s_pMenu));  // -> 's_pMenu' becomes NULL thanks to the weak pointer.
	}
	g_return_val_if_fail (pContainer != NULL, NULL);
	
	//\_________________________ On construit le menu.
	GtkWidget *menu = gtk_menu_new ();
	
	//\_________________________ On passe la main a ceux qui veulent y rajouter des choses.
	gboolean bDiscardMenu = FALSE;
	cairo_dock_notify_on_object (pContainer, NOTIFICATION_BUILD_CONTAINER_MENU, icon, pContainer, menu, &bDiscardMenu);
	if (bDiscardMenu)
	{
		gtk_widget_destroy (menu);
		return NULL;
	}
	
	cairo_dock_notify_on_object (pContainer, NOTIFICATION_BUILD_ICON_MENU, icon, pContainer, menu);
	
	s_pMenu = menu;
	g_object_add_weak_pointer (G_OBJECT (menu), (gpointer*)&s_pMenu);  // will nullify 's_pMenu' as soon as the menu is destroyed.
	return menu;
}


GldiShape *gldi_container_create_input_shape (CairoContainer *pContainer, int x, int y, int w, int h)
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


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoContainersParam *pContainersParam)
{
	gboolean bFlushConfFileNeeded = FALSE;
	pContainersParam->bUseFakeTransparency = cairo_dock_get_boolean_key_value (pKeyFile, "System", "fake transparency", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	if (g_bUseOpenGL && ! g_openglConfig.bAlphaAvailable)
		pContainersParam->bUseFakeTransparency = TRUE;
	
	int iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "opengl anim freq", &bFlushConfFileNeeded, 33, NULL, NULL);
	pContainersParam->iGLAnimationDeltaT = 1000. / iRefreshFrequency;
	iRefreshFrequency = cairo_dock_get_integer_key_value (pKeyFile, "System", "cairo anim freq", &bFlushConfFileNeeded, 25, NULL, NULL);
	pContainersParam->iCairoAnimationDeltaT = 1000. / iRefreshFrequency;
	//g_print ("iGLAnimationDeltaT: %d\n", pContainersParam->iGLAnimationDeltaT);
	
	return bFlushConfFileNeeded;
}


  ////////////
 /// LOAD ///
////////////

static void load (void)
{
	if (myContainersParam.bUseFakeTransparency)
	{
		g_pFakeTransparencyDesktopBg = cairo_dock_get_desktop_background (g_bUseOpenGL);
	}
}


  //////////////
 /// RELOAD ///
//////////////

static void _set_below (CairoDock *pDock, gpointer data)
{
	gtk_window_set_keep_below (GTK_WINDOW (pDock->container.pWidget), GPOINTER_TO_INT (data));
}
static void reload (CairoContainersParam *pPrevContainers, CairoContainersParam *pContainers)
{
		//\_______________ Fake Transparency.
	if (pContainers->bUseFakeTransparency && ! pPrevContainers->bUseFakeTransparency)
	{
		cairo_dock_foreach_root_docks ((GFunc)_set_below, GINT_TO_POINTER (TRUE));
		g_pFakeTransparencyDesktopBg = cairo_dock_get_desktop_background (g_bUseOpenGL);
	}
	else if (! pContainers->bUseFakeTransparency && pPrevContainers->bUseFakeTransparency)
	{
		cairo_dock_foreach_root_docks ((GFunc)_set_below, GINT_TO_POINTER (FALSE));
		cairo_dock_destroy_desktop_background (g_pFakeTransparencyDesktopBg);
		g_pFakeTransparencyDesktopBg = NULL;
	}
}


  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	g_pFakeTransparencyDesktopBg = NULL;  // no need to unref it, since everything is going to be unloaded, including the desktop_bg.
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_containers_manager (void)
{
	// Manager
	memset (&myContainersMgr, 0, sizeof (CairoContainersManager));
	myContainersMgr.mgr.cModuleName 	= "Containers";
	myContainersMgr.mgr.init 		= NULL;
	myContainersMgr.mgr.load 		= load;
	myContainersMgr.mgr.unload 		= unload;
	myContainersMgr.mgr.reload 		= (GldiManagerReloadFunc)reload;
	myContainersMgr.mgr.get_config 	= (GldiManagerGetConfigFunc)get_config;
	myContainersMgr.mgr.reset_config = (GldiManagerResetConfigFunc)NULL;
	// Config
	myContainersMgr.mgr.pConfig = (GldiManagerConfigPtr)&myContainersParam;
	myContainersMgr.mgr.iSizeOfConfig = sizeof (CairoContainersParam);
	// data
	myContainersMgr.mgr.pData = (GldiManagerDataPtr)NULL;
	myContainersMgr.mgr.iSizeOfData = 0;
	// signals
	cairo_dock_install_notifications_on_object (&myContainersMgr, NB_NOTIFICATIONS_CONTAINER);
	// register
	gldi_register_manager (GLDI_MANAGER(&myContainersMgr));
}
