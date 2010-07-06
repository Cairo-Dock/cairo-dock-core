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

#ifdef HAVE_GLITZ
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include <gtk/gtkgl.h>
#include <GL/gl.h> 

#include "cairo-dock-icons.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-container.h"

gboolean g_bSticky = TRUE;
gboolean g_bUseGlitz = FALSE;

CairoContainer *g_pPrimaryContainer = NULL;
extern gboolean g_bUseOpenGL;


static gboolean _cairo_dock_on_delete (GtkWidget *pWidget, GdkEvent *event, gpointer data)
{
	cd_debug ("pas de alt+f4");
	return TRUE;  // on empeche les ALT+F4 malheureux.
}

GtkWidget *cairo_dock_init_container_full (CairoContainer *pContainer, gboolean bOpenGLWindow)
{
	GtkWidget* pWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	if (g_bSticky)
		gtk_window_stick (GTK_WINDOW (pWindow));
	gtk_window_set_skip_pager_hint (GTK_WINDOW(pWindow), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pWindow), TRUE);
	
	cairo_dock_set_colormap_for_window (pWindow);
	if (g_bUseOpenGL && bOpenGLWindow)
	{
		cairo_dock_set_gl_capabilities (pWindow);
	}
	
	g_signal_connect (G_OBJECT (pWindow),
		"delete-event",
		G_CALLBACK (_cairo_dock_on_delete),
		NULL);
	
	gtk_widget_set_app_paintable (pWindow, TRUE);
	gtk_window_set_decorated (GTK_WINDOW (pWindow), FALSE);
	///gtk_window_set_resizable (GTK_WINDOW (pWindow), TRUE);  // vrai par defaut.
	
	if (g_pPrimaryContainer == NULL)
	{
		g_pPrimaryContainer = pContainer;
	}
	pContainer->pWidget = pWindow;
	return pWindow;
}

void cairo_dock_finish_container (CairoContainer *pContainer)
{
	gtk_widget_destroy (pContainer->pWidget);  // enleve les signaux.
	pContainer->pWidget = NULL;
	if (pContainer->iSidGLAnimation != 0)
	{
		g_source_remove (pContainer->iSidGLAnimation);
		pContainer->iSidGLAnimation = 0;
	}
	if (g_pPrimaryContainer == pContainer)
		g_pPrimaryContainer = NULL;
}

void cairo_dock_set_colormap_for_window (GtkWidget *pWidget)
{
	GdkScreen* pScreen = gtk_widget_get_screen (pWidget);
	GdkColormap* pColormap = gdk_screen_get_rgba_colormap (pScreen);
	if (!pColormap)
		pColormap = gdk_screen_get_rgb_colormap (pScreen);
	
	/// est-ce que ca vaut le coup de plutot faire ca avec le visual obtenu pour l'openGL ?...
	//GdkVisual *visual = gdkx_visual_get (pVisInfo->visualid);
	//pColormap = gdk_colormap_new (visual, TRUE);

	gtk_widget_set_colormap (pWidget, pColormap);
}

void cairo_dock_set_colormap (CairoContainer *pContainer)
{
	GdkColormap* pColormap;
#ifdef HAVE_GLITZ
	if (g_bUseGlitz)
	{
		glitz_drawable_format_t templ, *format;
		unsigned long	    mask = GLITZ_FORMAT_DOUBLEBUFFER_MASK;
		XVisualInfo		    *vinfo = NULL;
		int			    screen = 0;
		GdkVisual		    *visual;
		GdkDisplay		    *gdkdisplay;
		Display		    *xdisplay;

		templ.doublebuffer = 1;
		gdkdisplay = gtk_widget_get_display (pContainer->pWidget);
		xdisplay   = gdk_x11_display_get_xdisplay (gdkdisplay);

		int i = 0;
		do
		{
			format = glitz_glx_find_window_format (xdisplay,
				screen,
				mask,
				&templ,
				i++);
			if (format)
			{
				vinfo = glitz_glx_get_visual_info_from_format (xdisplay,
					screen,
					format);
				if (vinfo->depth == 32)
				{
					pContainer->pDrawFormat = format;
					break;
				}
				else if (!pContainer->pDrawFormat)
				{
					pContainer->pDrawFormat = format;
				}
			}
		} while (format);

		if (! pContainer->pDrawFormat)
		{
			cd_warning ("no double buffered GLX visual");
		}
		else
		{
			vinfo = glitz_glx_get_visual_info_from_format (xdisplay,
				screen,
				pContainer->pDrawFormat);

			visual = gdkx_visual_get (vinfo->visualid);
			pColormap = gdk_colormap_new (visual, TRUE);

			gtk_widget_set_colormap (pContainer->pWidget, pColormap);
			gtk_widget_set_double_buffered (pContainer->pWidget, FALSE);
			return ;
		}
	}
#endif
	
	cairo_dock_set_colormap_for_window (pContainer->pWidget);
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
	if (! GTK_WIDGET_VISIBLE (pContainer->pWidget))
		return ;
	
	if (pArea->y < 0)
		pArea->y = 0;
	if (pContainer->bIsHorizontal && pArea->y + pArea->height > pContainer->iHeight)
		pArea->height = pContainer->iHeight - pArea->y;
	else if (! pContainer->bIsHorizontal && pArea->x + pArea->width > pContainer->iHeight)
		pArea->width = pContainer->iHeight - pArea->x;
	
	if (pArea->width > 0 && pArea->height > 0)
		gdk_window_invalidate_rect (pContainer->pWidget->window, pArea, FALSE);
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
	
	if (CAIRO_DOCK_IS_DOCK (pContainer) && (cairo_dock_is_hidden (CAIRO_DOCK (pContainer)) && ! icon->bIsDemandingAttention && ! icon->bAlwaysVisible) || (CAIRO_DOCK (pContainer)->iRefCount != 0 && ! GTK_WIDGET_VISIBLE (pContainer->pWidget)))  // inutile de redessiner.
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

void cairo_dock_disallow_widget_to_receive_data (GtkWidget *pWidget)
{
	gtk_drag_dest_set_target_list (pWidget, NULL);
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
		cairo_dock_notify (CAIRO_DOCK_DROP_DATA, cData, pPointedIcon, fOrder, pContainer);
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
	return cairo_dock_emit_signal_on_container (pContainer, "leave-notify-event");
}
gboolean cairo_dock_emit_enter_signal (CairoContainer *pContainer)
{
	return cairo_dock_emit_signal_on_container (pContainer, "enter-notify-event");
}


static void _cairo_dock_delete_menu (GtkMenuShell *menu, CairoDock *pDock)
{
	g_return_if_fail (CAIRO_DOCK_IS_DOCK (pDock));
	pDock->bMenuVisible = FALSE;
	
	cd_message ("on force a quitter");
	pDock->container.bInside = TRUE;
	///pDock->bAtBottom = FALSE;
	cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
	/*cairo_dock_on_leave_notify (pDock->container.pWidget,
		NULL,
		pDock);*/
}
void cairo_dock_popup_menu_on_container (GtkWidget *menu, CairoContainer *pContainer)
{
	if (menu == NULL)
		return;
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		if (g_signal_handler_find (menu,
			G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
			0,
			0,
			NULL,
			_cairo_dock_delete_menu,
			pContainer) == 0)  // on evite de connecter 2 fois ce signal, donc la fonction est appelable plusieurs fois.
		{
			g_signal_connect (G_OBJECT (menu),
				"deactivate",
				G_CALLBACK (_cairo_dock_delete_menu),
				pContainer);
		}
		CAIRO_DOCK (pContainer)->bMenuVisible = TRUE;
	}
	
	gtk_widget_show_all (GTK_WIDGET (menu));

	gtk_menu_popup (GTK_MENU (menu),
		NULL,
		NULL,
		NULL,
		NULL,
		1,
		gtk_get_current_event_time ());
}


GtkWidget *cairo_dock_add_in_menu_with_stock_and_data (const gchar *cLabel, const gchar *gtkStock, GFunc pFunction, GtkWidget *pMenu, gpointer pData)
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
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);
	}
	gtk_menu_shell_append  (GTK_MENU_SHELL (pMenu), pMenuItem);
	if (pFunction)
		g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK (pFunction), pData);
	return pMenuItem;
}


GtkWidget *cairo_dock_build_menu (Icon *icon, CairoContainer *pContainer)
{
	static GtkWidget *s_pMenu = NULL;
	if (s_pMenu != NULL)
	{
		gtk_widget_destroy (GTK_WIDGET (s_pMenu));
		s_pMenu = NULL;
	}
	g_return_val_if_fail (pContainer != NULL, NULL);
	
	//\_________________________ On construit le menu.
	GtkWidget *menu = gtk_menu_new ();
	
	//\_________________________ On passe la main a ceux qui veulent y rajouter des choses.
	gboolean bDiscardMenu = FALSE;
	cairo_dock_notify (CAIRO_DOCK_BUILD_CONTAINER_MENU, icon, pContainer, menu, &bDiscardMenu);
	if (bDiscardMenu)
	{
		gtk_widget_destroy (menu);
		return NULL;
	}
	
	cairo_dock_notify (CAIRO_DOCK_BUILD_ICON_MENU, icon, pContainer, menu);
	s_pMenu = menu;
	return menu;
}
