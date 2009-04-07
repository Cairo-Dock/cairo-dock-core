 /*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cairo.h>

#ifdef HAVE_GLITZ
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include <gtk/gtkgl.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <GL/gl.h> 
#include <GL/glu.h> 
#include <GL/glx.h> 
#include <gdk/x11/gdkglx.h>

#include "cairo-dock-icons.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-container.h"

extern CairoDock *g_pMainDock;

extern gboolean g_bSticky;

extern gboolean g_bUseGlitz;
extern gboolean g_bUseOpenGL;
extern gboolean g_bIndirectRendering;
extern GdkGLConfig* g_pGlConfig;


static void _cairo_dock_on_realize (GtkWidget* pWidget, gpointer data)
{
	if (! g_bUseOpenGL)
		return ;
	
	GdkGLContext* pGlContext = gtk_widget_get_gl_context (pWidget);
	GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (pWidget);
	if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
		return ;
	
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth (1.0f);
	glClearStencil (0);
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint (GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);  // GL_MODULATE / GL_DECAL /  GL_BLEND
	
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MIN_FILTER,
		GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri (GL_TEXTURE_2D,
		GL_TEXTURE_MAG_FILTER,
		GL_LINEAR);
	
	gdk_gl_drawable_gl_end (pGlDrawable);
}

GtkWidget *cairo_dock_create_container_window (void)
{
	GtkWidget* pWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	if (g_bSticky)
		gtk_window_stick (GTK_WINDOW (pWindow));
	gtk_window_set_skip_pager_hint (GTK_WINDOW(pWindow), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pWindow), TRUE);
	
	cairo_dock_set_colormap_for_window (pWindow);
	if (g_bUseOpenGL)
	{
		GdkGLContext *pMainGlContext = gtk_widget_get_gl_context (g_pMainDock ? g_pMainDock->pWidget : NULL);  // NULL si on est en train de creer la fenetre du main dock, ce qui nous convient.
		gtk_widget_set_gl_capability (pWindow,
			g_pGlConfig,
			pMainGlContext,  // on partage les ressources entre les contextes.
			! g_bIndirectRendering,  // TRUE <=> direct connection to the graphics system.
			GDK_GL_RGBA_TYPE);
		g_signal_connect_after (G_OBJECT (pWindow),
			"realize",
			G_CALLBACK (_cairo_dock_on_realize),
			NULL);
	}
	
	gtk_widget_set_app_paintable (pWindow, TRUE);
	gtk_window_set_decorated (GTK_WINDOW (pWindow), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (pWindow), TRUE);
	return pWindow;
}

GtkWidget *cairo_dock_create_container_window_no_opengl (void)
{
	GtkWidget* pWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	if (g_bSticky)
		gtk_window_stick (GTK_WINDOW (pWindow));
	gtk_window_set_skip_pager_hint (GTK_WINDOW(pWindow), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW(pWindow), TRUE);
	
	cairo_dock_set_colormap_for_window (pWindow);
	
	gtk_widget_set_app_paintable (pWindow, TRUE);
	gtk_window_set_decorated (GTK_WINDOW (pWindow), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (pWindow), TRUE);
	return pWindow;
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



void cairo_dock_redraw_icon (Icon *icon, CairoContainer *pContainer)
{
	g_return_if_fail (icon != NULL && pContainer != NULL);
	GdkRectangle rect;
	cairo_dock_compute_icon_area (icon, pContainer, &rect);
	cairo_dock_redraw_container_area (pContainer, &rect);
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

void cairo_dock_redraw_container_area (CairoContainer *pContainer, GdkRectangle *pArea)
{
	g_return_if_fail (pContainer != NULL);
	if (! GTK_WIDGET_VISIBLE (pContainer->pWidget))
		return ;
	if (CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->bAtBottom && (/**CAIRO_DOCK (pContainer)->iRefCount > 0 || */CAIRO_DOCK (pContainer)->bAutoHide))  // inutile de redessiner.
		return ;
	if (pArea->y < 0)
		pArea->y = 0;
	if (pArea->y + pArea->height > pContainer->iHeight)
		pArea->height = pContainer->iHeight - pArea->y;
	//g_print ("rect (%d;%d) (%dx%d)\n", pArea->x, pArea->y, pArea->width, pArea->height);
	if (pArea->width > 0 && pArea->height > 0)
		gdk_window_invalidate_rect (pContainer->pWidget->window, pArea, FALSE);
}




CairoContainer *cairo_dock_search_container_from_icon (Icon *icon)
{
	g_return_val_if_fail (icon != NULL, NULL);
	if (CAIRO_DOCK_IS_APPLET (icon))
		return icon->pModuleInstance->pContainer;
	else
		return CAIRO_CONTAINER (cairo_dock_search_dock_from_name (icon->cParentDockName));
}


void cairo_dock_show_hide_container (CairoContainer *pContainer)
{
	if (pContainer == NULL)
		return;
	if (! GTK_WIDGET_VISIBLE (pContainer->pWidget))
		gtk_window_present (GTK_WINDOW (pContainer->pWidget));
	else
		gtk_widget_hide (pContainer->pWidget);
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

gboolean cairo_dock_string_is_adress (const gchar *cString)
{
	gchar *protocole = g_strstr_len (cString, -1, "://");
	if (protocole == NULL || protocole == cString)
		return FALSE;
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
			g_print (" + adresse\n");
			if (sArg->str[sArg->len-1] == '\r')
			{
				g_print ("retour charriot\n");
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
