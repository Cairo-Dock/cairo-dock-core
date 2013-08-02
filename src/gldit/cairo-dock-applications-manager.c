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
#include <stdio.h>
#include <stdlib.h>
#define __USE_POSIX
#include <signal.h>

#include <cairo.h>

#include "gldi-config.h"
#include "cairo-dock-icon-manager.h"
#include "cairo-dock-indicator-manager.h"  // myIndicatorsParam.bDrawIndicatorOnAppli
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-animations.h"  // cairo_dock_trigger_icon_removal_from_dock
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-icon-facility.h"  // gldi_icon_set_name
#include "cairo-dock-container.h"
#include "cairo-dock-object.h"
#include "cairo-dock-log.h"
#include "cairo-dock-config.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-desktop-manager.h"  // NOTIFICATION_DESKTOP_CHANGED
#include "cairo-dock-class-manager.h"
#include "cairo-dock-draw-opengl.h"  // cairo_dock_create_texture_from_surface
#include "cairo-dock-keyfile-utilities.h"  // cairo_dock_open_key_file
#include "cairo-dock-application-facility.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-overlay.h"  // cairo_dock_print_overlay_on_icon
#define _MANAGER_DEF_
#include "cairo-dock-applications-manager.h"

#define CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME "default-icon-appli.svg"

// public (manager, config, data)
CairoTaskbarParam myTaskbarParam;
GldiManager myTaskbarMgr;
GldiObjectManager myAppliIconObjectMgr;

// dependancies
extern CairoDock *g_pMainDock;
extern gboolean g_bUseOpenGL;
//extern int g_iDamageEvent;

// private
static GHashTable *s_hAppliIconsTable = NULL;  // table des fenetres affichees dans le dock.
static int s_bAppliManagerIsRunning = FALSE;
static GldiWindowActor *s_pCurrentActiveWindow = NULL;

static void cairo_dock_unregister_appli (Icon *icon);


static Icon * cairo_dock_create_icon_from_window (GldiWindowActor *actor)
{
	if (actor->bDisplayed)
		return (Icon*)gldi_object_new (&myAppliIconObjectMgr, actor);
	else
		return NULL;
}

static Icon *_get_appli_icon (GldiWindowActor *actor)
{
	return g_hash_table_lookup (s_hAppliIconsTable, actor);
}


  ///////////////
 // Callbacks //
///////////////

static gboolean _on_window_created (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	Icon *pIcon = _get_appli_icon (actor);
	g_return_val_if_fail (pIcon == NULL, GLDI_NOTIFICATION_LET_PASS);
	
	// create an appli-icon
	pIcon = cairo_dock_create_icon_from_window (actor);
	if (pIcon != NULL)
	{
		// insert into the dock
		if (myTaskbarParam.bShowAppli)
		{
			cd_message (" insertion de %s ...", pIcon->cName);
			cairo_dock_insert_appli_in_dock (pIcon, g_pMainDock, CAIRO_DOCK_ANIMATE_ICON);
		}
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_destroyed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	cd_debug ("window %s (%p) is destroyed", actor->cName, actor);
	Icon *icon = _get_appli_icon (actor);
	if (icon != NULL)
	{
		if (actor->bDemandsAttention)  // force the stop demanding attention, in case the icon was in a sub-dock (the main icon is also animating).
			cairo_dock_appli_stops_demanding_attention (icon);
		
		CairoDock *pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (icon));
		if (pParentDock != NULL)
		{
			cd_message ("  va etre supprimee");
			cairo_dock_unregister_appli (icon);  // unregister the icon immediately, since it doesn't represent anything any more; it unsets pAppli, so that when the animation is over, the icon will be destroyed.
			
			cairo_dock_trigger_icon_removal_from_dock (icon);
		}
		else  // inhibited or not shown -> destroy it immediately
		{
			cd_message ("  pas dans un container, on la detruit donc immediatement");
			cairo_dock_update_name_on_inhibitors (icon->cClass, actor, NULL);
			gldi_object_unref (GLDI_OBJECT (icon));  // will call cairo_dock_unregister_appli and update the inhibitors.
		}
	}
	
	if (s_pCurrentActiveWindow == actor)
		s_pCurrentActiveWindow = NULL;
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_name_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	Icon *pIcon = _get_appli_icon (actor);
	if (pIcon == NULL)
		return GLDI_NOTIFICATION_LET_PASS;
	
	gldi_icon_set_name (pIcon, actor->cName);
	
	cairo_dock_update_name_on_inhibitors (actor->cClass, actor, pIcon->cName);
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_icon_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	Icon *icon = _get_appli_icon (actor);
	if (icon == NULL)
		return GLDI_NOTIFICATION_LET_PASS;
	
	if (cairo_dock_class_is_using_xicon (icon->cClass) || ! myTaskbarParam.bOverWriteXIcons)
	{
		GldiContainer *pContainer = cairo_dock_get_icon_container (icon);
		if (pContainer != NULL)  // if the icon is not in a container (for instance inhibited), it's no use trying to load its image. It's not even useful to mark it as 'damaged', since anyway it will be loaded when inserted inside a container.
		{
			cairo_dock_load_icon_image (icon, pContainer);
			if (CAIRO_DOCK_IS_DOCK (pContainer))
			{
				CairoDock *pDock = CAIRO_DOCK (pContainer);
				if (pDock->iRefCount != 0)
					cairo_dock_trigger_redraw_subdock_content (pDock);
			}
			cairo_dock_redraw_icon (icon);
		}
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_attention_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	Icon *pIcon = _get_appli_icon (actor);
	if (pIcon == NULL)
		return GLDI_NOTIFICATION_LET_PASS;
	
	if (actor->bDemandsAttention)
	{
		cd_debug ("%s demande votre attention", pIcon->cName);
		if (myTaskbarParam.bDemandsAttentionWithDialog || myTaskbarParam.cAnimationOnDemandsAttention)
		{
			cairo_dock_appli_demands_attention (pIcon);
		}
	}
	else
	{
		cd_debug ("%s se tait", pIcon->cName);
		cairo_dock_appli_stops_demanding_attention (pIcon);
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_size_position_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	Icon *icon = _get_appli_icon (actor);
	if (icon == NULL)
		return GLDI_NOTIFICATION_LET_PASS;
	
	// on regarde si l'appli est sur le viewport courant.
	if (! gldi_window_is_on_current_desktop (actor))  // not on this desktop/viewport any more
	{
		// applis du bureau courant seulement.
		if (myTaskbarParam.bAppliOnCurrentDesktopOnly)
		{
			if (cairo_dock_get_icon_container (icon) != NULL)
			{
				CairoDock *pParentDock = cairo_dock_detach_appli (icon);
				if (pParentDock)
					gtk_widget_queue_draw (pParentDock->container.pWidget);
			}
			else
				gldi_window_detach_from_inhibitors (actor);
		}
	}
	else  // elle est sur le viewport courant.
	{
		// applis du bureau courant seulement.
		if (myTaskbarParam.bAppliOnCurrentDesktopOnly && cairo_dock_get_icon_container (icon) == NULL && myTaskbarParam.bShowAppli)
		{
			//cd_message ("cette fenetre est sur le bureau courant (%d;%d)", x, y);
			cairo_dock_insert_appli_in_dock (icon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);  // the icon might be on this desktop and yet not in a dock (inhibited), in which case this function does nothing.
		}
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_state_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor, gboolean bHiddenChanged, G_GNUC_UNUSED gboolean bMaximizedChanged, G_GNUC_UNUSED gboolean bFullScreenChanged)
{
	Icon *icon = _get_appli_icon (actor);
	if (icon == NULL)
		return GLDI_NOTIFICATION_LET_PASS;
	
	// on gere le cachage/apparition de l'icone (transparence ou miniature, applis minimisees seulement).
	CairoDock *pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (icon));
	if (bHiddenChanged)
	{
		cd_message ("  changement de visibilite -> %d", actor->bIsHidden);
		
		// drawing of hidden appli-icons.
		if (g_bUseOpenGL && myTaskbarParam.iMinimizedWindowRenderType == 2)
		{
			if (pParentDock != NULL)
			{
				cairo_dock_draw_hidden_appli_icon (icon, CAIRO_CONTAINER (pParentDock), TRUE);
			}
		}
		else if (myTaskbarParam.iMinimizedWindowRenderType == 0)
		{
			// transparence sur les inhibiteurs.
			cairo_dock_update_visibility_on_inhibitors (icon->cClass, actor, actor->bIsHidden);
		}
		
		// showing hidden appli-icons only
		if (myTaskbarParam.bHideVisibleApplis && myTaskbarParam.bShowAppli)  // on insere/detache l'icone selon la visibilite de la fenetre, avec une animation.
		{
			if (actor->bIsHidden)  // se cache => on insere son icone.
			{
				cd_message (" => se cache");
				pParentDock = cairo_dock_insert_appli_in_dock (icon, g_pMainDock, CAIRO_DOCK_ANIMATE_ICON);
				if (pParentDock != NULL)
				{
					if (g_bUseOpenGL && myTaskbarParam.iMinimizedWindowRenderType == 2)  // quand on est passe dans ce cas tout a l'heure l'icone n'etait pas encore dans son dock.
						cairo_dock_draw_hidden_appli_icon (icon, CAIRO_CONTAINER (pParentDock), TRUE);
					gtk_widget_queue_draw (pParentDock->container.pWidget);
				}
			}
			else  // se montre => on detache l'icone.
			{
				cd_message (" => re-apparait");
				cairo_dock_trigger_icon_removal_from_dock (icon);
			}
		}
		else if (myTaskbarParam.fVisibleAppliAlpha != 0)  // transparence
		{
			icon->fAlpha = 1;  // on triche un peu.
			if (pParentDock != NULL)
				cairo_dock_redraw_icon (icon);
		}
		
		// miniature (on le fait apres l'avoir inseree/detachee, car comme ca dans le cas ou on l'enleve du dock apres l'avoir deminimisee, l'icone est marquee comme en cours de suppression, et donc on ne recharge pas son icone. Sinon l'image change pendant la transition, ce qui est pas top. Comme ca ne change pas la taille de l'icone dans le dock, on peut faire ca apres l'avoir inseree.
		if (myTaskbarParam.iMinimizedWindowRenderType == 1 && (pParentDock != NULL || myTaskbarParam.bHideVisibleApplis))
		{
			// on redessine avec ou sans la miniature, suivant le nouvel etat.
			cairo_dock_load_icon_image (icon, CAIRO_CONTAINER (pParentDock));
			if (pParentDock)
			{
				cairo_dock_redraw_icon (icon);
				if (pParentDock->iRefCount != 0)  // on prevoit le redessin de l'icone pointant sur le sous-dock.
				{
					cairo_dock_trigger_redraw_subdock_content (pParentDock);
				}
			}
		}
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_window_class_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor, G_GNUC_UNUSED const gchar *cOldClass, G_GNUC_UNUSED const gchar *cOldWmClass)
{
	Icon *icon = _get_appli_icon (actor);
	if (icon == NULL)
		return GLDI_NOTIFICATION_LET_PASS;
	
	// remove the icon from the dock, and then from its class
	CairoDock *pParentDock = NULL;
	if (cairo_dock_get_icon_container (icon) != NULL)  // if in a dock, detach it
		pParentDock = cairo_dock_detach_appli (icon);
	else  // else if inhibited, detach from the inhibitor
		gldi_window_detach_from_inhibitors (actor);
	cairo_dock_remove_appli_from_class (icon);
	
	// set the new class
	g_free (icon->cClass);
	icon->cClass = g_strdup (actor->cClass);
	g_free (icon->cWmClass);
	icon->cWmClass = g_strdup (actor->cWmClass);
	cairo_dock_add_appli_icon_to_class (icon);
	
	// re-insert the icon
	pParentDock = cairo_dock_insert_appli_in_dock (icon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
	if (pParentDock != NULL)
		gtk_widget_queue_draw (pParentDock->container.pWidget);
	
	// reload the icon
	g_strfreev (icon->pMimeTypes);
	icon->pMimeTypes = g_strdupv ((gchar**)cairo_dock_get_class_mimetypes (icon->cClass));
	g_free (icon->cCommand);
	icon->cCommand = g_strdup (cairo_dock_get_class_command (icon->cClass));
	cairo_dock_load_icon_image (icon, CAIRO_CONTAINER (pParentDock));
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static void _hide_show_appli_icons_on_other_desktops (GldiWindowActor *pAppli, Icon *icon, CairoDock *pMainDock)
{
	if (! myTaskbarParam.bHideVisibleApplis || pAppli->bIsHidden)
	{
		cd_debug ("%s (%p)", __func__, pAppli);
		CairoDock *pParentDock = NULL;
		if (gldi_window_is_on_current_desktop (pAppli))
		{
			cd_debug (" => est sur le bureau actuel.");
			if (cairo_dock_get_icon_container(icon) == NULL)
			{
				pParentDock = cairo_dock_insert_appli_in_dock (icon, pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
			}
		}
		else
		{
			cd_debug (" => n'est pas sur le bureau actuel.");
			if (cairo_dock_get_icon_container (icon) != NULL)  // if in a dock, detach it
				pParentDock = cairo_dock_detach_appli (icon);
			else  // else if inhibited, detach from the inhibitor
				gldi_window_detach_from_inhibitors (icon->pAppli);
		}
		if (pParentDock != NULL)
			gtk_widget_queue_draw (pParentDock->container.pWidget);
	}
}

static gboolean _on_window_desktop_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	Icon *icon = _get_appli_icon (actor);
	if (icon == NULL)
		return GLDI_NOTIFICATION_LET_PASS;
	
	// applis du bureau courant seulement.
	if (myTaskbarParam.bAppliOnCurrentDesktopOnly && myTaskbarParam.bShowAppli)
	{
		_hide_show_appli_icons_on_other_desktops (actor, icon, g_pMainDock);  // si elle vient sur notre bureau, elle n'est pas forcement sur le meme viewport, donc il faut le verifier.
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_desktop_changed (G_GNUC_UNUSED gpointer data)
{
	// applis du bureau courant seulement.
	if (myTaskbarParam.bAppliOnCurrentDesktopOnly && myTaskbarParam.bShowAppli)
	{
		g_hash_table_foreach (s_hAppliIconsTable, (GHFunc) _hide_show_appli_icons_on_other_desktops, g_pMainDock);
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_active_window_changed (G_GNUC_UNUSED gpointer data, GldiWindowActor *actor)
{
	// on gere son animation et son indicateur.
	Icon *icon = _get_appli_icon (actor);
	CairoDock *pParentDock = NULL;
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		if (icon->bIsDemandingAttention)  // force the stop demanding attention, as it can happen (for some reason) that the attention state doesn't change when the appli takes the focus.
			cairo_dock_appli_stops_demanding_attention (icon);
		
		pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (icon));
		if (pParentDock == NULL)  // inhibited or not shown
		{
			cairo_dock_update_activity_on_inhibitors (icon->cClass, actor);
		}
		else
		{
			cairo_dock_animate_icon_on_active (icon, pParentDock);
		}
	}
	
	// on enleve l'indicateur sur la precedente appli active.
	Icon *pLastActiveIcon = _get_appli_icon (s_pCurrentActiveWindow);
	if (CAIRO_DOCK_IS_APPLI (pLastActiveIcon))
	{
		CairoDock *pLastActiveParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pLastActiveIcon));
		if (pLastActiveParentDock == NULL)  // inhibited or not shown
		{
			cairo_dock_update_inactivity_on_inhibitors (pLastActiveIcon->cClass, s_pCurrentActiveWindow);
		}
		else
		{
			cairo_dock_redraw_icon (pLastActiveIcon);
			if (pLastActiveParentDock->iRefCount != 0)  // l'icone est dans un sous-dock, comme l'indicateur est aussi dessine sur l'icone pointant sur ce sous-dock, il faut la redessiner sans l'indicateur.
			{
				CairoDock *pMainDock = NULL;
				Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pLastActiveParentDock, &pMainDock);
				if (pPointingIcon && pMainDock)
				{
					cairo_dock_redraw_icon (pPointingIcon);
				}
			}
		}
	}
	s_pCurrentActiveWindow = actor;
	
	return GLDI_NOTIFICATION_LET_PASS;
}


  ///////////////////////////
 // Applis manager : core //
///////////////////////////

static void cairo_dock_register_appli (Icon *icon)
{
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cd_debug ("%s (%p ; %s)", __func__, icon->pAppli, icon->cName);
		// add to table
		g_hash_table_insert (s_hAppliIconsTable, icon->pAppli, icon);
		
		// add to class
		cairo_dock_add_appli_icon_to_class (icon);
	}
}

static void cairo_dock_unregister_appli (Icon *icon)
{
	if (CAIRO_DOCK_IS_APPLI (icon))
	{
		cd_debug ("%s (%p ; %s)", __func__, icon->pAppli, icon->cName);
		// remove from table
		g_hash_table_remove (s_hAppliIconsTable, icon->pAppli);
		
		// remove from class
		cairo_dock_remove_appli_from_class (icon);  // n'efface pas sa classe (on peut en avoir besoin encore).
		gldi_window_detach_from_inhibitors (icon->pAppli);
		
		// unset the appli
		gldi_icon_unset_appli (icon);
	}
}

static void _create_appli_icon (GldiWindowActor *actor, CairoDock *pDock)
{
	Icon *pIcon = cairo_dock_create_icon_from_window (actor);
	if (pIcon != NULL)
	{
		if (myTaskbarParam.bShowAppli && pDock)
		{
			cairo_dock_insert_appli_in_dock (pIcon, g_pMainDock, ! CAIRO_DOCK_ANIMATE_ICON);
		}
	}
}
void cairo_dock_start_applications_manager (CairoDock *pDock)
{
	g_return_if_fail (!s_bAppliManagerIsRunning);
	
	cairo_dock_set_overwrite_exceptions (myTaskbarParam.cOverwriteException);
	cairo_dock_set_group_exceptions (myTaskbarParam.cGroupException);
	
	// create an appli-icon for each window.
	gldi_windows_foreach (FALSE, (GFunc)_create_appli_icon, pDock);  // ordered by creation date; this allows us to set the correct age to the icon, which is constant. On the next updates, the z-order (which is dynamic) will be set.
	
	s_bAppliManagerIsRunning = TRUE;
}

static gboolean _remove_one_appli (G_GNUC_UNUSED GldiWindowActor *pAppli, Icon *pIcon, G_GNUC_UNUSED gpointer data)
{
	if (pIcon == NULL)
		return TRUE;
	if (pIcon->pAppli == NULL)
	{
		g_free (pIcon);
		return TRUE;
	}
	
	cd_debug (" remove %s...", pIcon->cName);
	CairoDock *pDock = CAIRO_DOCK(cairo_dock_get_icon_container (pIcon));
	if (GLDI_OBJECT_IS_DOCK(pDock))
	{
		gldi_icon_detach (pIcon);
		if (pDock->iRefCount != 0)  // this appli-icon is in a sub-dock (above a launcher or a class-icon)
		{
			if (pDock->icons == NULL)  // the sub-dock gets empty -> free it
			{
				CairoDock *pFakeClassParentDock = NULL;
				Icon *pFakeClassIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pFakeClassParentDock);
				if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pFakeClassIcon))  // also remove the fake launcher that was pointing on it
				{
					cd_debug ("on degage le fake qui pointe sur %s", pDock->cDockName);
					pFakeClassIcon->pSubDock = NULL;  // don't free the sub-dock, since we do it below
					gldi_icon_detach (pFakeClassIcon);
					gldi_object_unref (GLDI_OBJECT (pFakeClassIcon));
				}
				gldi_object_unref (GLDI_OBJECT(pDock));
			}
		}
	}
	
	gldi_icon_unset_appli (pIcon);  // on ne veut pas passer dans le 'unregister'
	g_free (pIcon->cClass);  // ni la gestion de la classe.
	pIcon->cClass = NULL;
	gldi_object_unref (GLDI_OBJECT (pIcon));
	return TRUE;
}
static void _cairo_dock_stop_application_manager (void)
{
	s_bAppliManagerIsRunning = FALSE;
	
	cairo_dock_remove_all_applis_from_class_table ();  // enleve aussi les indicateurs.
	
	g_hash_table_foreach_remove (s_hAppliIconsTable, (GHRFunc) _remove_one_appli, NULL);  // libere toutes les icones d'appli.
}



  /////////////////////////////
 // Applis manager : access //
/////////////////////////////

GList *cairo_dock_get_current_applis_list (void)
{
	return g_hash_table_get_values (s_hAppliIconsTable);
}

Icon *cairo_dock_get_current_active_icon (void)
{
	GldiWindowActor *actor = gldi_windows_get_active ();
	return cairo_dock_get_appli_icon (actor);
}

Icon *cairo_dock_get_appli_icon (GldiWindowActor *actor)
{
	if (! actor)
		return NULL;
	return g_hash_table_lookup (s_hAppliIconsTable, actor);
}

static void _for_one_appli_icon (G_GNUC_UNUSED GldiWindowActor *actor, Icon *icon, gpointer *data)
{
	if (! CAIRO_DOCK_IS_APPLI (icon) || cairo_dock_icon_is_being_removed (icon))
		return ;
	CairoDockForeachIconFunc pFunction = data[0];
	gpointer pUserData = data[1];
	
	CairoDock *pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (icon));
	if (! pParentDock)
		pParentDock = g_pMainDock;  // contestable...
	pFunction (icon, CAIRO_CONTAINER (pParentDock), pUserData);
}
void cairo_dock_foreach_appli_icon (CairoDockForeachIconFunc pFunction, gpointer pUserData)
{
	gpointer data[2] = {pFunction, pUserData};
	g_hash_table_foreach (s_hAppliIconsTable, (GHFunc) _for_one_appli_icon, data);
}

void cairo_dock_set_icons_geometry_for_window_manager (CairoDock *pDock)
{
	if (! s_bAppliManagerIsRunning)
		return ;
	//g_print ("%s (main:%d, ref:%d)\n", __func__, pDock->bIsMainDock, pDock->iRefCount);
	
	/*long *data = g_new0 (long, 1+6*g_list_length (pDock->icons));
	int i = 0;*/
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (CAIRO_DOCK_IS_APPLI (icon))
		{
			cairo_dock_set_one_icon_geometry_for_window_manager (icon, pDock);
			/*data[1+6*i+0] = 5;
			data[1+6*i+1] = icon->Xid;
			data[1+6*i+2] = pDock->container.iWindowPositionX + icon->fXAtRest;
			data[1+6*i+3] = 0;
			data[1+6*i+4] = icon->fWidth;
			data[1+6*i+5] = icon->fHeight;
			i ++;*/
		}
	}
	
	/*data[0] = i;
	Atom atom = XInternAtom (gdk_x11_get_default_xdisplay(), "_KDE_WINDOW_PREVIEW", False);
	Window Xid = GDK_WINDOW_XID (pDock->container.pWidget->window);
	XChangeProperty(gdk_x11_get_default_xdisplay(), Xid, atom, atom, 32, PropModeReplace, data, 1+6*i);*/
	
	if (pDock->bIsMainDock && myTaskbarParam.bHideVisibleApplis)  // on complete avec les applis pas dans le dock, pour que l'effet de minimisation pointe (a peu pres) au bon endroit quand on la minimisera.
	{
		g_hash_table_foreach (s_hAppliIconsTable, (GHFunc) cairo_dock_reserve_one_icon_geometry_for_window_manager, pDock);
	}
}


  ////////////////////////////
 // Applis manager : icons //
////////////////////////////

static void _load_appli (Icon *icon)
{
	if (cairo_dock_icon_is_being_removed (icon))
		return ;
	
	//\__________________ register the class to get its attributes, if it was not done yet.
	if (icon->cClass && !icon->pMimeTypes && !icon->cCommand)
	{
		gchar *cClass = cairo_dock_register_class_full (NULL, icon->cClass, icon->cWmClass);
		if (cClass != NULL)
		{
			g_free (cClass);
			icon->cCommand = g_strdup (cairo_dock_get_class_command (icon->cClass));
			icon->pMimeTypes = g_strdupv ((gchar**)cairo_dock_get_class_mimetypes (icon->cClass));
		}
	}
	
	//\__________________ then draw the icon
	int iWidth = cairo_dock_icon_get_allocated_width (icon);
	int iHeight = cairo_dock_icon_get_allocated_height (icon);
	cairo_surface_t *pPrevSurface = icon->image.pSurface;
	GLuint iPrevTexture = icon->image.iTexture;
	icon->image.pSurface = NULL;
	icon->image.iTexture = 0;
	
	// use the thumbnail in the case of a minimized window.
	if (myTaskbarParam.iMinimizedWindowRenderType == 1 && icon->pAppli->bIsHidden)
	{
		// create the thumbnail (window preview).
		if (g_bUseOpenGL)  // in OpenGL, we should be able to use the texture-from-pixmap mechanism
		{
			GLuint iTexture = gldi_window_get_texture (icon->pAppli);
			if (iTexture)
				cairo_dock_load_image_buffer_from_texture (&icon->image, iTexture, iWidth, iHeight);
		}
		if (icon->image.iTexture == 0)  // if not opengl or didn't work, get the content of the pixmap from the X server.
		{
			cairo_surface_t *pThumbnailSurface = gldi_window_get_thumbnail_surface (icon->pAppli, iWidth, iHeight);
			cairo_dock_load_image_buffer_from_surface (&icon->image, pThumbnailSurface, iWidth, iHeight);
		}
		// draw the previous image as an emblem.
		if (icon->image.iTexture != 0 && iPrevTexture != 0)
		{
			cairo_dock_print_overlay_on_icon_from_texture (icon, iPrevTexture, CAIRO_OVERLAY_LOWER_LEFT);
		}
		else if (icon->image.pSurface != NULL && pPrevSurface != NULL)
		{
			cairo_dock_print_overlay_on_icon_from_surface (icon, pPrevSurface, 0, 0, CAIRO_OVERLAY_LOWER_LEFT);
		}
	}
	
	// in other cases (or if the preview couldn't be used)
	if (icon->image.iTexture == 0 && icon->image.pSurface == NULL)
	{
		cairo_surface_t *pSurface = NULL;
		// or use the class icon
		if (myTaskbarParam.bOverWriteXIcons && ! cairo_dock_class_is_using_xicon (icon->cClass))
			pSurface = cairo_dock_create_surface_from_class (icon->cClass, iWidth, iHeight);
		// or use the X icon
		if (pSurface == NULL)
			pSurface = gldi_window_get_icon_surface (icon->pAppli, iWidth, iHeight);
		// or use a default image
		if (pSurface == NULL)  // some applis like xterm don't define any icon, set the default one.
		{
			cd_debug ("%s (%p) doesn't define any icon, we set the default one.", icon->cName, icon->pAppli);
			gchar *cIconPath = cairo_dock_search_image_s_path (CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
			if (cIconPath == NULL)  // image non trouvee.
			{
				cIconPath = g_strdup (GLDI_SHARE_DATA_DIR"/icons/"CAIRO_DOCK_DEFAULT_APPLI_ICON_NAME);
			}
			pSurface = cairo_dock_create_surface_from_image_simple (cIconPath,
				iWidth,
				iHeight);
			g_free (cIconPath);
		}
		if (pSurface != NULL)
			cairo_dock_load_image_buffer_from_surface (&icon->image, pSurface, iWidth, iHeight);
	}
	
	// bent the icon in the case of a minimized window and if defined in the config.
	if (icon->pAppli->bIsHidden && myTaskbarParam.iMinimizedWindowRenderType == 2)
	{
		GldiContainer *pContainer = cairo_dock_get_icon_container (icon);
		if (pContainer)
			cairo_dock_draw_hidden_appli_icon (icon, pContainer, FALSE);
	}
}

static void _show_appli_for_drop (Icon *pIcon)
{
	gldi_window_show (pIcon->pAppli);
}

  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoTaskbarParam *pTaskBar)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	// comportement
	pTaskBar->bShowAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "show applications", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	
	if (pTaskBar->bShowAppli)
	{
		pTaskBar->bAppliOnCurrentDesktopOnly = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "current desktop only", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
		
		pTaskBar->bMixLauncherAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "mix launcher appli", &bFlushConfFileNeeded, TRUE, NULL, NULL);
		pTaskBar->bOpeningAnimation = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "opening animation", &bFlushConfFileNeeded, TRUE, NULL, NULL);

		pTaskBar->bGroupAppliByClass = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "group by class", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
		pTaskBar->cGroupException = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "group exception", &bFlushConfFileNeeded, "pidgin;xchat", NULL, NULL);
		if (pTaskBar->cGroupException)
		{
			int i;
			for (i = 0; pTaskBar->cGroupException[i] != '\0'; i ++)  // on passe tout en minuscule.
				pTaskBar->cGroupException[i] = g_ascii_tolower (pTaskBar->cGroupException[i]);
		}
		
		pTaskBar->bHideVisibleApplis = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "hide visible", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
		
		pTaskBar->iIconPlacement = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "place icons", &bFlushConfFileNeeded, CAIRO_APPLI_AFTER_LAST_LAUNCHER, NULL, NULL);  // after the last launcher by default.
		pTaskBar->cRelativeIconName = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "relative icon", &bFlushConfFileNeeded, NULL, NULL, NULL);
		pTaskBar->bSeparateApplis = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "separate applis", &bFlushConfFileNeeded, TRUE, NULL, NULL);
		
		// representation
		pTaskBar->bOverWriteXIcons = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "overwrite xicon", &bFlushConfFileNeeded, TRUE, NULL, NULL);
		pTaskBar->cOverwriteException = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "overwrite exception", &bFlushConfFileNeeded, "pidgin;xchat", NULL, NULL);
		if (pTaskBar->cOverwriteException)
		{
			int i;
			for (i = 0; pTaskBar->cOverwriteException[i] != '\0'; i ++)
				pTaskBar->cOverwriteException[i] = g_ascii_tolower (pTaskBar->cOverwriteException[i]);
		}
		
		pTaskBar->iMinimizedWindowRenderType = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "minimized", &bFlushConfFileNeeded, -1, NULL, NULL);
		if (pTaskBar->iMinimizedWindowRenderType == -1)  // anciens parametres.
		{
			gboolean bShowThumbnail = g_key_file_get_boolean (pKeyFile, "TaskBar", "window thumbnail", NULL);
			if (bShowThumbnail)
				pTaskBar->iMinimizedWindowRenderType = 1;
			else
				pTaskBar->iMinimizedWindowRenderType = 0;
			g_key_file_set_integer (pKeyFile, "TaskBar", "minimized", pTaskBar->iMinimizedWindowRenderType);
		}
		
		pTaskBar->fVisibleAppliAlpha = MIN (.6, cairo_dock_get_double_key_value (pKeyFile, "TaskBar", "visibility alpha", &bFlushConfFileNeeded, .35, "Applications", NULL));
		
		pTaskBar->iAppliMaxNameLength = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "max name length", &bFlushConfFileNeeded, 25, "Applications", NULL);
		
		// interaction
		pTaskBar->iActionOnMiddleClick = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "action on middle click", &bFlushConfFileNeeded, 1, NULL, NULL);
		pTaskBar->bMinimizeOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "minimize on click", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
		
		pTaskBar->bPresentClassOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "present class on click", &bFlushConfFileNeeded, TRUE, NULL, NULL);
		
		pTaskBar->bDemandsAttentionWithDialog = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "demands attention with dialog", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
		pTaskBar->iDialogDuration = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "duration", &bFlushConfFileNeeded, 2, NULL, NULL);
		pTaskBar->cAnimationOnDemandsAttention = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "animation on demands attention", &bFlushConfFileNeeded, "fire", NULL, NULL);
		gchar *cForceDemandsAttention = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "force demands attention", &bFlushConfFileNeeded, "pidgin;xchat", NULL, NULL);
		if (cForceDemandsAttention != NULL)
		{
			pTaskBar->cForceDemandsAttention = g_ascii_strdown (cForceDemandsAttention, -1);
			g_free (cForceDemandsAttention);
		}
		
		pTaskBar->cAnimationOnActiveWindow = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "animation on active window", &bFlushConfFileNeeded, "wobbly", NULL, NULL);
	}
	return bFlushConfFileNeeded;
}


  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (CairoTaskbarParam *pTaskBar)
{
	g_free (pTaskBar->cAnimationOnActiveWindow);
	g_free (pTaskBar->cAnimationOnDemandsAttention);
	g_free (pTaskBar->cOverwriteException);
	g_free (pTaskBar->cGroupException);
	g_free (pTaskBar->cForceDemandsAttention);
	g_free (pTaskBar->cRelativeIconName);
}

  ////////////
 /// LOAD ///
////////////

/*
static void load (void)
{
	
}
*/

  //////////////
 /// RELOAD ///
//////////////

static void reload (CairoTaskbarParam *pPrevTaskBar, CairoTaskbarParam *pTaskBar)
{
	CairoDock *pDock = g_pMainDock;
	
	if (pPrevTaskBar->bGroupAppliByClass != pTaskBar->bGroupAppliByClass
		|| pPrevTaskBar->bHideVisibleApplis != pTaskBar->bHideVisibleApplis
		|| pPrevTaskBar->bAppliOnCurrentDesktopOnly != pTaskBar->bAppliOnCurrentDesktopOnly
		|| pPrevTaskBar->bMixLauncherAppli != pTaskBar->bMixLauncherAppli
		|| pPrevTaskBar->bOpeningAnimation != pTaskBar->bOpeningAnimation
		|| pPrevTaskBar->bOverWriteXIcons != pTaskBar->bOverWriteXIcons
		|| pPrevTaskBar->iMinimizedWindowRenderType != pTaskBar->iMinimizedWindowRenderType
		|| pPrevTaskBar->iAppliMaxNameLength != pTaskBar->iAppliMaxNameLength
		|| g_strcmp0 (pPrevTaskBar->cGroupException, pTaskBar->cGroupException) != 0
		|| g_strcmp0 (pPrevTaskBar->cOverwriteException, pTaskBar->cOverwriteException) != 0
		|| pPrevTaskBar->bShowAppli != pTaskBar->bShowAppli
		|| pPrevTaskBar->iIconPlacement != pTaskBar->iIconPlacement
		|| pPrevTaskBar->bSeparateApplis != pTaskBar->bSeparateApplis
		|| g_strcmp0 (pPrevTaskBar->cRelativeIconName, pTaskBar->cRelativeIconName) != 0)
	{
		_cairo_dock_stop_application_manager ();
		
		cairo_dock_start_applications_manager (pDock);
		
		gtk_widget_queue_draw (pDock->container.pWidget);  // if no appli-icon has been inserted, but an indicator or a grouped class has been added, we need to draw them now
	}
	else
	{
		gtk_widget_queue_draw (pDock->container.pWidget);  // in case 'fVisibleAlpha' has changed
	}
}


  //////////////
 /// UNLOAD ///
//////////////

static gboolean _remove_appli (G_GNUC_UNUSED GldiWindowActor *actor, Icon *pIcon, G_GNUC_UNUSED gpointer data)
{
	if (pIcon == NULL)
		return TRUE;
	if (pIcon->pAppli == NULL)
	{
		g_free (pIcon);
		return TRUE;
	}
	
	/// TODO here ?...
	///cairo_dock_set_xicon_geometry (pIcon->Xid, 0, 0, 0, 0);  // since we'll not detach the icons one by one, we do it here.
	
	// make it an invalid appli
	gldi_icon_unset_appli (pIcon);  // we don't want to go into the 'unregister'
	g_free (pIcon->cClass);  // nor the class manager, since we reseted it beforehand.
	pIcon->cClass = NULL;
	
	// if not inside a dock, free it, else it will be freeed with the dock.
	if (cairo_dock_get_icon_container (pIcon) == NULL)  // not in a dock.
	{
		gldi_object_unref (GLDI_OBJECT (pIcon));
	}
	
	return TRUE;
}
static void unload (void)
{
	// empty the class table.
	cairo_dock_reset_class_table ();
	
	// empty the applis table.
	g_hash_table_foreach_remove (s_hAppliIconsTable, (GHRFunc) _remove_appli, NULL);
	
	s_bAppliManagerIsRunning = FALSE;
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	s_hAppliIconsTable = g_hash_table_new_full (g_direct_hash,
		g_direct_equal,
		NULL,  // window actor
		NULL);  // appli-icon
	
	cairo_dock_initialize_class_manager ();
	
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_CREATED,
		(GldiNotificationFunc) _on_window_created,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_DESTROYED,
		(GldiNotificationFunc) _on_window_destroyed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_NAME_CHANGED,
		(GldiNotificationFunc) _on_window_name_changed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_ICON_CHANGED,
		(GldiNotificationFunc) _on_window_icon_changed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_ATTENTION_CHANGED,
		(GldiNotificationFunc) _on_window_attention_changed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_SIZE_POSITION_CHANGED,
		(GldiNotificationFunc) _on_window_size_position_changed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_STATE_CHANGED,
		(GldiNotificationFunc) _on_window_state_changed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_CLASS_CHANGED,
		(GldiNotificationFunc) _on_window_class_changed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_DESKTOP_CHANGED,
		(GldiNotificationFunc) _on_window_desktop_changed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myDesktopMgr,
		NOTIFICATION_DESKTOP_CHANGED,
		(GldiNotificationFunc) _on_desktop_changed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myWindowObjectMgr,
		NOTIFICATION_WINDOW_ACTIVATED,
		(GldiNotificationFunc) _on_active_window_changed,
		GLDI_RUN_FIRST, NULL);
}


  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	Icon *icon = (Icon*)obj;
	GldiWindowActor *actor = (GldiWindowActor*)attr;
	
	icon->iGroup = CAIRO_DOCK_APPLI;
	icon->fOrder = CAIRO_DOCK_LAST_ORDER;
	gldi_icon_set_appli (icon, actor);
	
	icon->cName = g_strdup (actor->cName ? actor->cName : actor->cClass);
	icon->cClass = g_strdup (actor->cClass);  // we'll register the class during the loading of the icon, since it can take some time, and we don't really need the class params right now.
	
	icon->iface.load_image           = _load_appli;
	icon->iface.action_on_drag_hover = _show_appli_for_drop;
	
	icon->bHasIndicator = myIndicatorsParam.bDrawIndicatorOnAppli;
	if (myTaskbarParam.bSeparateApplis)
		icon->iGroup = CAIRO_DOCK_APPLI;
	else
		icon->iGroup = CAIRO_DOCK_LAUNCHER;
	
	//\____________ register it.
	cairo_dock_register_appli (icon);
}

static void reset_object (GldiObject *obj)
{
	Icon *pIcon = (Icon*)obj;
	cairo_dock_unregister_appli (pIcon);
}

void gldi_register_applications_manager (void)
{
	// Manager
	memset (&myTaskbarMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myTaskbarMgr), &myManagerObjectMgr, NULL);
	myTaskbarMgr.cModuleName    = "Taskbar";
	// interface
	myTaskbarMgr.init           = init;
	myTaskbarMgr.load           = NULL;  // the manager is started after the launchers&applets have been created, to avoid unecessary computations.
	myTaskbarMgr.unload         = unload;
	myTaskbarMgr.reload         = (GldiManagerReloadFunc)reload;
	myTaskbarMgr.get_config     = (GldiManagerGetConfigFunc)get_config;
	myTaskbarMgr.reset_config   = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myTaskbarParam, 0, sizeof (CairoTaskbarParam));
	myTaskbarMgr.pConfig = (GldiManagerConfigPtr)&myTaskbarParam;
	myTaskbarMgr.iSizeOfConfig = sizeof (CairoTaskbarParam);
	// data
	myTaskbarMgr.pData = (GldiManagerDataPtr)NULL;
	myTaskbarMgr.iSizeOfData = 0;
	
	// Object Manager
	memset (&myAppliIconObjectMgr, 0, sizeof (GldiObjectManager));
	myAppliIconObjectMgr.cName          = "AppliIcon";
	myAppliIconObjectMgr.iObjectSize    = sizeof (Icon);
	// interface
	myAppliIconObjectMgr.init_object    = init_object;
	myAppliIconObjectMgr.reset_object   = reset_object;
	// signals
	gldi_object_install_notifications (GLDI_OBJECT (&myAppliIconObjectMgr), NB_NOTIFICATIONS_TASKBAR);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myAppliIconObjectMgr), &myIconObjectMgr);
}
