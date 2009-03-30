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
#include <X11/extensions/shape.h>
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
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-labels.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-factory.h"

extern CairoDock *g_pMainDock;

extern gchar *g_cCurrentLaunchersPath;

extern gboolean g_bKeepAbove;
extern gboolean g_bSkipPager;
extern gboolean g_bSkipTaskbar;
extern gboolean g_bSticky;

extern gboolean g_bUseGlitz;
extern gboolean g_bUseOpenGL;
extern gboolean g_bForceOpenGL;
extern gboolean g_bIndirectRendering;
extern GdkGLConfig* g_pGlConfig;

static void _cairo_dock_on_realize_main_dock (GtkWidget* pWidget, gpointer data)
{
	if (! g_bUseOpenGL)
		return ;
	
	if (! g_bForceOpenGL)
	{
		GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Use OpenGL in Cairo-Dock ?"),
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_YES,
			GTK_RESPONSE_YES,
			GTK_STOCK_NO,
			GTK_RESPONSE_NO,
			NULL);
		GtkWidget *label = gtk_label_new (_("OpenGL allows you to use the hardware acceleration, reducing the CPU load to the minimum.\nIt also allows some pretty visual effects similar to Compiz.\nHowever, some cards and/or their drivers don't fully support it, which may prevent the dock from running correctly.\nDo you want to activate OpenGL ?\n (To not show this dialog, launch the dock from the Application menu,\n  or with the -o option to force OpenGL and -c to force cairo.)"));
		gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), label);
		gtk_widget_show_all (dialog);

		gint iAnswer = gtk_dialog_run (GTK_DIALOG (dialog));
		//int iAnswer = cairo_dock_ask_general_question_and_wait (_("Use OpenGL ?"));
		gtk_widget_destroy (dialog);
		if (iAnswer == GTK_RESPONSE_NO)
		{
			g_bUseOpenGL = FALSE;
			return ;
		}
	}
	
	GdkGLContext* pGlContext = gtk_widget_get_gl_context (pWidget);
	GdkGLDrawable* pGlDrawable = gtk_widget_get_gl_drawable (pWidget);
	if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
		return ;
	
	g_print ("OpenGL version: %s\nOpenGL vendor: %s\nOpenGL renderer: %s\n",
		glGetString (GL_VERSION),
		glGetString (GL_VENDOR),
		glGetString (GL_RENDERER));
	
	gdk_gl_drawable_gl_end (pGlDrawable);
}

CairoDock *cairo_dock_create_new_dock (GdkWindowTypeHint iWmHint, gchar *cDockName, gchar *cRendererName)
{
	cd_message ("%s (%s)", __func__, cDockName);
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
	pDock->fFlatDockWidth = - myIcons.iIconGap;
	pDock->iMouseX = -1; // utile ?
	pDock->iMouseY = -1;
	pDock->fMagnitudeMax = 1.;

	//\__________________ On cree la fenetre GTK.
	GtkWidget *pWindow = cairo_dock_create_container_window ();
	pDock->pWidget = pWindow;

	if (g_bKeepAbove)
		gtk_window_set_keep_above (GTK_WINDOW (pWindow), g_bKeepAbove);
	if (myAccessibility.bPopUp)
		gtk_window_set_keep_below (GTK_WINDOW (pWindow), TRUE);
	if (mySystem.bUseFakeTransparency)
		gtk_window_set_keep_below (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_gravity (GTK_WINDOW (pWindow), GDK_GRAVITY_STATIC);
	gtk_window_set_type_hint (GTK_WINDOW (pWindow), iWmHint);

	gtk_window_set_title (GTK_WINDOW (pWindow), "cairo-dock");  // GTK renseigne la classe avec la meme valeur.
	
	cairo_dock_set_renderer (pDock, cRendererName);

	//\__________________ On connecte les evenements a la fenetre.
	gtk_widget_add_events (pWindow,
		GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
		GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	
	if (pDock->bIsMainDock)
		g_signal_connect_after (G_OBJECT (pWindow),
			"realize",
			G_CALLBACK (_cairo_dock_on_realize_main_dock),
			NULL);
	g_signal_connect (G_OBJECT (pWindow),
		"delete-event",
		G_CALLBACK (cairo_dock_on_delete),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"expose-event",
		G_CALLBACK (cairo_dock_on_expose),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (cairo_dock_on_configure),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"key-release-event",
		G_CALLBACK (cairo_dock_on_key_release),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-press-event",
		G_CALLBACK (cairo_dock_on_button_press),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"button-release-event",
		G_CALLBACK (cairo_dock_on_button_press),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"scroll-event",
		G_CALLBACK (cairo_dock_on_scroll),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"motion-notify-event",
		G_CALLBACK (cairo_dock_on_motion_notify),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"enter-notify-event",
		G_CALLBACK (cairo_dock_on_enter_notify),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"leave-notify-event",
		G_CALLBACK (cairo_dock_on_leave_notify),
		pDock);
	cairo_dock_allow_widget_to_receive_data (pWindow,
		G_CALLBACK (cairo_dock_on_drag_data_received),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag_motion",
		G_CALLBACK (cairo_dock_on_drag_motion),
		pDock);
	g_signal_connect (G_OBJECT (pWindow),
		"drag_leave",
		G_CALLBACK (cairo_dock_on_drag_leave),
		pDock);

	gtk_window_get_size (GTK_WINDOW (pWindow), &pDock->iCurrentWidth, &pDock->iCurrentHeight);  // ca n'est que la taille initiale allouee par GTK.
	gtk_widget_show_all (pWindow);
	gdk_window_set_back_pixmap (pWindow->window, NULL, FALSE);  // vraiment plus rapide ?
	

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
			cd_warning ("failed to create glitz drawable");
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
	{
		if (cairo_dock_get_root_dock_position (cDockName, pDock))
			cairo_dock_place_root_dock (pDock);
	}
	g_print ("redessin force ...\n");
		while (gtk_events_pending ())  // on force le redessin pour eviter les carres gris.
			gtk_main_iteration ();
	g_print ("done.\n");
	
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
	if (pDock->iSidLeaveDemand != 0)
		g_source_remove (pDock->iSidLeaveDemand);
	if (pDock->iSidIconGlide != 0)
		g_source_remove (pDock->iSidIconGlide);
	if (pDock->bIsMainDock && cairo_dock_application_manager_is_running ())
	{
		cairo_dock_pause_application_manager ();  // precaution au cas ou.
	}
	cairo_dock_notify (CAIRO_DOCK_STOP_DOCK, pDock);
	if (pDock->iSidGLAnimation != 0)
		g_source_remove (pDock->iSidGLAnimation);

	g_list_foreach (pDock->icons, (GFunc) _cairo_dock_fm_remove_monitor_on_one_icon, NULL);

	Icon *pPointedIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
	if (pPointedIcon != NULL)
		pPointedIcon->pSubDock = NULL;
	
	if (pDock->pShapeBitmap != NULL)
		g_object_unref ((gpointer) pDock->pShapeBitmap);
	pDock->pShapeBitmap = NULL;
	
	gtk_widget_destroy (pDock->pWidget);
	pDock->pWidget = NULL;

	g_free (pDock->cRendererName);
	pDock->cRendererName = NULL;
}

void cairo_dock_destroy_dock (CairoDock *pDock, const gchar *cDockName, CairoDock *pReceivingDock, gchar *cpReceivingDockName)
{
	g_print ("%s (%s, %d)\n", __func__, cDockName, pDock->iRefCount);
	g_return_if_fail (pDock != NULL);  // && cDockName != NULL
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

			icon->fWidth /= pDock->fRatio;
			icon->fHeight /= pDock->fRatio;
			
			cd_debug (" on re-attribue %s au dock %s", icon->acName, icon->cParentDockName);
			cairo_dock_insert_icon_in_dock (icon, pReceivingDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, myIcons.bUseSeparator);
			
			if (CAIRO_DOCK_IS_APPLET (icon))
			{
				icon->pModuleInstance->pContainer = CAIRO_CONTAINER (pReceivingDock);  // astuce pour ne pas avoir a recharger le fichier de conf ^_^
				icon->pModuleInstance->pDock = pReceivingDock;
				cairo_dock_reload_module_instance (icon->pModuleInstance, FALSE);
			}
			cairo_dock_launch_animation (CAIRO_CONTAINER (pReceivingDock));
		}
	}
	if (pReceivingDock != NULL)
		cairo_dock_update_dock_size (pReceivingDock);

	g_list_free (pIconsList);

	cairo_dock_unregister_dock (cDockName);
	
	if (bModuleWasRemoved)
		cairo_dock_update_conf_file_with_active_modules ();
	
	if (pDock->iRefCount == -1)  // c'etait un dock racine.
		cairo_dock_remove_root_dock_config (cDockName);
	
	g_free (pDock);
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
		pDock->bHorizontalDock = (myViews.bSameHorizontality ? pParentDock->bHorizontalDock : ! pParentDock->bHorizontalDock);
		pDock->bDirectionUp = pParentDock->bDirectionUp;
		if (iScreenBorder != (((! pDock->bHorizontalDock) << 1) | (! pDock->bDirectionUp)))
		{
			cd_message ("changement de position -> %d/%d", pDock->bHorizontalDock, pDock->bDirectionUp);
			cairo_dock_reload_reflects_in_dock (pDock);
		}
		if (g_bKeepAbove)
			gtk_window_set_keep_above (GTK_WINDOW (pDock->pWidget), FALSE);
		if (myAccessibility.bPopUp)
			gtk_window_set_keep_below (GTK_WINDOW (pDock->pWidget), FALSE);
		
		pDock->bAutoHide = FALSE;
		double fPrevRatio = pDock->fRatio;
		pDock->fRatio = MIN (pDock->fRatio, myViews.fSubDockSizeRatio);

		Icon *icon;
		GList *ic;
		pDock->fFlatDockWidth = - myIcons.iIconGap;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			icon = ic->data;
			icon->fWidth *= pDock->fRatio / fPrevRatio;
			icon->fHeight *= pDock->fRatio / fPrevRatio;
			pDock->fFlatDockWidth += icon->fWidth + myIcons.iIconGap;
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
	g_return_val_if_fail (pSubDock != NULL, NULL);
	
	cairo_dock_reference_dock (pSubDock, pParentDock);  // on le fait tout de suite pour avoir la bonne reference avant le 'load'.

	pSubDock->icons = pIconList;
	if (pIconList != NULL)
		cairo_dock_load_buffers_in_one_dock (pSubDock);

	return pSubDock;
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
			{
				cairo_dock_insert_icon_in_dock (icon, pParentDock, ! CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, ! CAIRO_DOCK_INSERT_SEPARATOR);
				/// synchroniser icon->pSubDock avec pParentDock ?...
			}
		}
	} while (1);
	g_dir_close (dir);
	cairo_destroy (pCairoContext);
}

void cairo_dock_free_all_docks (void)
{
	if (g_pMainDock == NULL)
		return ;

	cairo_dock_deactivate_all_modules ();  // y compris les modules qui n'ont pas d'icone.
	
	cairo_dock_reset_class_table ();  // enleve aussi les inhibiteurs.
	cairo_dock_stop_application_manager ();
	cairo_dock_stop_polling_screen_edge ();

	cairo_dock_reset_docks_table ();  // detruit tous les docks, vide la table, et met le main-dock a NULL.
}


void cairo_dock_insert_icon_in_dock_full (Icon *icon, CairoDock *pDock, gboolean bUpdateSize, gboolean bAnimated, gboolean bApplyRatio, gboolean bInsertSeparator, GCompareFunc pCompareFunc)
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
		Icon *pSameTypeIcon = cairo_dock_get_first_icon_of_order (pDock->icons, icon->iType);
		if (pSameTypeIcon == NULL && pDock->icons != NULL)
		{
			bSeparatorNeeded = TRUE;
			cd_debug ("separateur necessaire");
		}
	}

	//\______________ On insere l'icone a sa place dans la liste.
	if (icon->fOrder == CAIRO_DOCK_LAST_ORDER)
	{
		Icon *pLastIcon = cairo_dock_get_last_icon_of_order (pDock->icons, icon->iType);
		if (pLastIcon != NULL)
			icon->fOrder = pLastIcon->fOrder + 1;
		else
			icon->fOrder = 1;
	}
	
	if (pCompareFunc == NULL)
		pCompareFunc = (GCompareFunc) cairo_dock_compare_icons_order;
	pDock->icons = g_list_insert_sorted (pDock->icons,
		icon,
		pCompareFunc);

	if (bApplyRatio)
	{
		icon->fWidth *= pDock->fRatio;
		icon->fHeight *= pDock->fRatio;
	}

	pDock->fFlatDockWidth += myIcons.iIconGap + icon->fWidth;
	pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, icon->fHeight);

	//\______________ On insere un separateur si necessaire.
	if (bSeparatorNeeded)
	{
		int iOrder = cairo_dock_get_icon_order (icon);
		if (iOrder + 1 < CAIRO_DOCK_NB_TYPES)
		{
			Icon *pNextIcon = cairo_dock_get_next_icon (pDock->icons, icon);
			if (pNextIcon != NULL && ((cairo_dock_get_icon_order (pNextIcon) - cairo_dock_get_icon_order (icon)) % 2 == 0) && (cairo_dock_get_icon_order (pNextIcon) != cairo_dock_get_icon_order (icon)))
			{
				int iSeparatorType = iOrder + 1;
				//g_print (" insertion de %s avant %s -> iSeparatorType : %d\n", icon->acName, pNextIcon->acName, iSeparatorType);

				cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
				Icon *pSeparatorIcon = cairo_dock_create_separator_icon (pSourceContext, iSeparatorType, pDock, bApplyRatio);
				if (pSeparatorIcon != NULL)
				{
					pDock->icons = g_list_insert_sorted (pDock->icons,
						pSeparatorIcon,
						(GCompareFunc) cairo_dock_compare_icons_order);
					pDock->fFlatDockWidth += myIcons.iIconGap + pSeparatorIcon->fWidth;
					pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, pSeparatorIcon->fHeight);
				}
				cairo_destroy (pSourceContext);
			}
		}
		if (iOrder > 1)
		{
			Icon *pPrevIcon = cairo_dock_get_previous_icon (pDock->icons, icon);
			if (pPrevIcon != NULL && ((cairo_dock_get_icon_order (pPrevIcon) - cairo_dock_get_icon_order (icon)) % 2 == 0) && (cairo_dock_get_icon_order (pPrevIcon) != cairo_dock_get_icon_order (icon)))
			{
				int iSeparatorType = iOrder - 1;
				//g_print (" insertion de %s (%d) apres %s -> iSeparatorType : %d\n", icon->acName, icon->pModuleInstance != NULL, pPrevIcon->acName, iSeparatorType);

				cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
				Icon *pSeparatorIcon = cairo_dock_create_separator_icon (pSourceContext, iSeparatorType, pDock, bApplyRatio);
				if (pSeparatorIcon != NULL)
				{
					pDock->icons = g_list_insert_sorted (pDock->icons,
						pSeparatorIcon,
						(GCompareFunc) cairo_dock_compare_icons_order);
					pDock->fFlatDockWidth += myIcons.iIconGap + pSeparatorIcon->fWidth;
					pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, pSeparatorIcon->fHeight);
				}
				cairo_destroy (pSourceContext);
			}
		}
	}
	
	//\______________ On effectue les actions demandees.
	if (bAnimated)
	{
		icon->fPersonnalScale = - 0.95;
		cairo_dock_notify (CAIRO_DOCK_INSERT_ICON, icon, pDock);
	}
	if (bUpdateSize)
		cairo_dock_update_dock_size (pDock);

	if (pDock->iRefCount == 0 && myAccessibility.bReserveSpace && bUpdateSize && ! pDock->bAutoHide && (pDock->fFlatDockWidth != iPreviousMinWidth || pDock->iMaxIconHeight != iPreviousMaxIconHeight))
		cairo_dock_reserve_space_for_dock (pDock, TRUE);
}


gboolean cairo_dock_detach_icon_from_dock (Icon *icon, CairoDock *pDock, gboolean bCheckUnusedSeparator)
{
	if (pDock == NULL || g_list_find (pDock->icons, icon) == NULL)  // elle est deja detachee.
		return FALSE;

	cd_message ("%s (%s)", __func__, icon->acName);
	g_free (icon->cParentDockName);
	icon->cParentDockName = NULL;
	
	cairo_dock_stop_icon_animation (icon);
	
	//\___________________ On l'enleve de la liste.
	if (pDock->pFirstDrawnElement != NULL && pDock->pFirstDrawnElement->data == icon)
	{
		if (pDock->pFirstDrawnElement->next != NULL)
			pDock->pFirstDrawnElement = pDock->pFirstDrawnElement->next;
		else
		{
			if (pDock->icons != NULL && pDock->icons->next != NULL)  // la liste n'a pas qu'un seul element.
				pDock->pFirstDrawnElement = pDock->icons;
			else
				pDock->pFirstDrawnElement = NULL;
		}
	}
	pDock->icons = g_list_remove (pDock->icons, icon);
	pDock->fFlatDockWidth -= icon->fWidth + myIcons.iIconGap;

	//\___________________ Cette icone realisait peut-etre le max des hauteurs, comme on l'enleve on recalcule ce max.
	Icon *pOtherIcon;
	GList *ic;
	if (icon->fHeight == pDock->iMaxIconHeight)
	{
		pDock->iMaxIconHeight = 0;
		for (ic = pDock->icons; ic != NULL; ic = ic->next)
		{
			pOtherIcon = ic->data;
			pDock->iMaxIconHeight = MAX (pDock->iMaxIconHeight, pOtherIcon->fHeight);
		}
	}

	//\___________________ On la remet a la taille normale en vue d'une reinsertion quelque part.
	icon->fWidth /= pDock->fRatio;
	icon->fHeight /= pDock->fRatio;

	//\___________________ On enleve le separateur si c'est la derniere icone de son type.
	if (bCheckUnusedSeparator && ! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
	{
		Icon *pSameTypeIcon = cairo_dock_get_first_icon_of_order (pDock->icons, icon->iType);
		if (pSameTypeIcon == NULL)
		{
			Icon * pSeparatorIcon = NULL;
			int iOrder = cairo_dock_get_icon_order (icon);
			//g_print ("iOrder : %d\n", iOrder);
			if (iOrder > 1)  // attention : iType - 1 > 0 si iType = 0, car c'est un unsigned int (enum) !
				pSeparatorIcon = cairo_dock_get_first_icon_of_order (pDock->icons, iOrder - 1);
			if (iOrder + 1 < CAIRO_DOCK_NB_TYPES && pSeparatorIcon == NULL)
				pSeparatorIcon = cairo_dock_get_first_icon_of_order (pDock->icons, iOrder + 1);

			if (pSeparatorIcon != NULL)
			{
				//g_print ("  on enleve un separateur\n");
				cairo_dock_detach_icon_from_dock (pSeparatorIcon, pDock, FALSE);
				cairo_dock_free_icon (pSeparatorIcon);
			}
		}
	}
	return TRUE;
}
static void _cairo_dock_remove_one_icon_from_dock (CairoDock *pDock, Icon *icon, gboolean bCheckUnusedSeparator)
{
	g_return_if_fail (icon != NULL && pDock != NULL);
	//\___________________ On effectue les taches de fermeture de l'icone suivant son type.
	if (icon->acDesktopFileName != NULL)
	{
		gchar *icon_path = g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, icon->acDesktopFileName);
		g_remove (icon_path);
		g_free (icon_path);

		if (CAIRO_DOCK_IS_URI_LAUNCHER (icon))
		{
			cairo_dock_fm_remove_monitor (icon);
		}
	}
	if (CAIRO_DOCK_IS_APPLET (icon))
	{
		cairo_dock_deinstanciate_module (icon->pModuleInstance);  // desactive l'instance du module.
		icon->pModuleInstance = NULL;  // l'instance n'est plus valide apres ca.
	}  // rien a faire pour les separateurs automatiques.

	//\___________________ On detache l'icone du dock.
	cairo_dock_detach_icon_from_dock (icon, pDock, bCheckUnusedSeparator);
	
	if (CAIRO_DOCK_IS_NORMAL_APPLI (icon))
	{
		cairo_dock_unregister_appli (icon);
	}
	
	if (pDock->iRefCount == 0 && myAccessibility.bReserveSpace)  // bIsMainDock
		cairo_dock_reserve_space_for_dock (pDock, TRUE);  // l'espace est reserve sur la taille min, qui a deja ete mise a jour.
}

void cairo_dock_remove_one_icon_from_dock (CairoDock *pDock, Icon *icon)
{
	_cairo_dock_remove_one_icon_from_dock (pDock, icon, FALSE);
}
void cairo_dock_remove_icon_from_dock (CairoDock *pDock, Icon *icon)
{
	_cairo_dock_remove_one_icon_from_dock (pDock, icon, TRUE);
}


void cairo_dock_remove_all_separators (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon;
	GList *ic = pDock->icons, *next_ic;
	while (ic != NULL)
	{
		icon = ic->data;
		next_ic = ic->next;  // si l'icone se fait enlever, on perdrait le fil.
		if (CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
		{
			//g_print ("un separateur en moins (apres %s)\n", ((Icon*)ic->data)->acName);
			cairo_dock_remove_one_icon_from_dock (pDock, icon);
			cairo_dock_free_icon (icon);
		}
		ic = next_ic;
	}
}

void cairo_dock_insert_separators_in_dock (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	Icon *icon, *next_icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (icon))
		{
			if (ic->next != NULL)
			{
				next_icon = ic->next->data;
				if (! CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR (next_icon) && abs (cairo_dock_get_icon_order (icon) - cairo_dock_get_icon_order (next_icon)) > 1)  // icon->iType != next_icon->iType
				{
					int iSeparatorType = myIcons.tIconTypeOrder[next_icon->iType] - 1;
					//g_print ("un separateur entre %s et %s, dans le groupe %d (=%d)\n", icon->acName, next_icon->acName, iSeparatorType, myIcons.tIconTypeOrder[iSeparatorType]);
					cairo_t *pCairoContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDock));
					Icon *pSeparator = cairo_dock_create_separator_icon (pCairoContext, iSeparatorType, pDock, ! CAIRO_DOCK_APPLY_RATIO);
					cairo_destroy (pCairoContext);

					cairo_dock_insert_icon_in_dock (pSeparator, pDock, !CAIRO_DOCK_UPDATE_DOCK_SIZE, ! CAIRO_DOCK_ANIMATE_ICON, CAIRO_DOCK_APPLY_RATIO, ! CAIRO_DOCK_INSERT_SEPARATOR);
				}
			}
		}
	}
}
