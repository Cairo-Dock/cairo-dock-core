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

#include "cairo-dock-icon-facility.h"  // gldi_icon_set_name
#include "cairo-dock-dialog-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-dock-facility.h"  // cairo_dock_update_dock_size
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-indicator-manager.h"  // myIndicatorsParam.bUseClassIndic
#include "cairo-dock-class-icon-manager.h"  // gldi_class_icon_new
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_full
#include "cairo-dock-application-facility.h"

extern CairoDock *g_pMainDock;
extern CairoDockHidingEffect *g_pHidingBackend;  // cairo_dock_is_hidden
extern GldiContainer *g_pPrimaryContainer;

static void _gldi_appli_icon_demands_attention (Icon *icon, CairoDock *pDock, gboolean bForceDemand, Icon *pHiddenIcon)
{
	cd_debug ("%s (%s, force:%d)", __func__, icon->cName, bForceDemand);
	if (CAIRO_DOCK_IS_APPLET (icon))  // on considere qu'une applet prenant le controle d'une icone d'appli dispose de bien meilleurs moyens pour interagir avec l'appli que la barre des taches.
		return ;
	//\____________________ On montre le dialogue.
	if (myTaskbarParam.bDemandsAttentionWithDialog)
	{
		CairoDialog *pDialog;
		if (pHiddenIcon == NULL)
		{
			pDialog = gldi_dialog_show_temporary_with_icon (icon->cName, icon, CAIRO_CONTAINER (pDock), 1000*myTaskbarParam.iDialogDuration, "same icon");
		}
		else
		{
			pDialog = gldi_dialog_show_temporary (pHiddenIcon->cName, icon, CAIRO_CONTAINER (pDock), 1000*myTaskbarParam.iDialogDuration); // mieux vaut montrer pas d'icone dans le dialogue que de montrer une icone qui n'a pas de rapport avec l'appli demandant l'attention.
			g_return_if_fail (pDialog != NULL);
			gldi_dialog_set_icon_surface (pDialog, pHiddenIcon->image.pSurface, pDialog->iIconSize);
		}
		if (pDialog && bForceDemand)
		{
			cd_debug ("force dock and dialog on top");
			if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && pDock->bIsBelow)
				cairo_dock_pop_up (pDock);
			gtk_window_set_keep_above (GTK_WINDOW (pDialog->container.pWidget), TRUE);
			gtk_window_set_type_hint (GTK_WINDOW (pDialog->container.pWidget), GDK_WINDOW_TYPE_HINT_DOCK);  // pour passer devant les fenetres plein ecran; depend du WM.
		}
	}
	//\____________________ On montre l'icone avec une animation.
	if (myTaskbarParam.cAnimationOnDemandsAttention && ! pHiddenIcon)  // on ne l'anime pas si elle n'est pas dans un dock.
	{
		if (pDock->iRefCount == 0)
		{
			if (bForceDemand)
			{
				if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && pDock->bIsBelow)
					cairo_dock_pop_up (pDock);
			}
		}
		/**else if (bForceDemand)
		{
			cd_debug ("force sub-dock to raise\n");
			CairoDock *pParentDock = NULL;
			Icon *pPointedIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
			if (pParentDock)
				cairo_dock_show_subdock (pPointedIcon, pParentDock);
		}*/
		gldi_icon_request_attention (icon, myTaskbarParam.cAnimationOnDemandsAttention, 10000);  // animation de 2-3 heures.
	}
}
void gldi_appli_icon_demands_attention (Icon *icon)
{
	cd_debug ("%s (%s, %p)", __func__, icon->cName, cairo_dock_get_icon_container(icon));
	if (icon->pAppli == gldi_windows_get_active())  // apparemment ce cas existe, et conduit a ne pas pouvoir stopper l'animation de demande d'attention facilement.
	{
		cd_message ("cette fenetre a deja le focus, elle ne peut demander l'attention en plus.");
		return ;
	}
	
	gboolean bForceDemand = (myTaskbarParam.cForceDemandsAttention && icon->cClass && g_strstr_len (myTaskbarParam.cForceDemandsAttention, -1, icon->cClass));
	CairoDock *pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (icon));
	if (pParentDock == NULL)  // appli inhibee ou non affichee.
	{
		Icon *pInhibitorIcon = cairo_dock_get_inhibitor (icon, TRUE);  // on cherche son inhibiteur dans un dock.
		if (pInhibitorIcon != NULL)  // appli inhibee.
		{
			pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibitorIcon));
			if (pParentDock != NULL)  // if the inhibitor is hidden (detached), there is no way to remember what should be its animation, so just forget it (we could fix it when inserting the icon back into a container, by checking if its appli is demanding the attention)
				_gldi_appli_icon_demands_attention (pInhibitorIcon, pParentDock, bForceDemand, NULL);
		}
		else if (bForceDemand)  // appli pas affichee, mais on veut tout de meme etre notifie.
		{
			Icon *pOneIcon = gldi_icons_get_any_without_dialog ();  // on prend une icone dans le main dock.
			if (pOneIcon != NULL)
				_gldi_appli_icon_demands_attention (pOneIcon, g_pMainDock, bForceDemand, icon);
		}
	}
	else  // appli dans un dock.
		_gldi_appli_icon_demands_attention (icon, pParentDock, bForceDemand, NULL);
}

static void _gldi_appli_icon_stop_demanding_attention (Icon *icon, CairoDock *pDock)
{
	if (CAIRO_DOCK_IS_APPLET (icon))  // cf remarque plus haut.
		return ;
	if (myTaskbarParam.bDemandsAttentionWithDialog)
		gldi_dialogs_remove_on_icon (icon);
	if (myTaskbarParam.cAnimationOnDemandsAttention)
	{
		gldi_icon_stop_attention (icon);  // arrete l'animation precedemment lancee par la demande.
		gtk_widget_queue_draw (pDock->container.pWidget);  // optimisation possible : ne redessiner que l'icone en tenant compte de la zone de sa derniere animation (pulse ou rebond).
	}
	if (pDock->iRefCount == 0 && pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && ! pDock->bIsBelow && ! pDock->container.bInside)
		cairo_dock_pop_down (pDock);
}
void gldi_appli_icon_stop_demanding_attention (Icon *icon)
{
	CairoDock *pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (icon));
	if (pParentDock == NULL)  // inhibited
	{
		Icon *pInhibitorIcon = cairo_dock_get_inhibitor (icon, TRUE);
		if (pInhibitorIcon != NULL)
		{
			pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibitorIcon));
			if (pParentDock != NULL)
				_gldi_appli_icon_stop_demanding_attention (pInhibitorIcon, pParentDock);
		}
	}
	else
		_gldi_appli_icon_stop_demanding_attention (icon, pParentDock);
}


void gldi_appli_icon_animate_on_active (Icon *icon, CairoDock *pParentDock)
{
	g_return_if_fail (pParentDock != NULL);
	if (! cairo_dock_icon_is_being_inserted_or_removed (icon))  // sinon on laisse l'animation actuelle.
	{
		if (myTaskbarParam.cAnimationOnActiveWindow)
		{
			if (cairo_dock_animation_will_be_visible (pParentDock) && icon->iAnimationState == CAIRO_DOCK_STATE_REST)
				gldi_icon_request_animation (icon, myTaskbarParam.cAnimationOnActiveWindow, 1);
		}
		else
		{
			cairo_dock_redraw_icon (icon);  // Si pas d'animation, on le fait pour redessiner l'indicateur.
		}
		if (pParentDock->iRefCount != 0)  // l'icone est dans un sous-dock, on veut que l'indicateur soit aussi dessine sur l'icone pointant sur ce sous-dock.
		{
			CairoDock *pMainDock = NULL;
			Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pParentDock, &pMainDock);
			if (pPointingIcon && pMainDock)
			{
				cairo_dock_redraw_icon (pPointingIcon);  // on se contente de redessiner cette icone sans l'animer. Une facon comme une autre de differencier ces 2 cas.
			}
		}
	}
}


// this function is used when we have an appli that is not inhibited. we can place it either in its subdock or in a dock next to an inhibitor or in the main dock amongst the other applis
static CairoDock *_get_parent_dock_for_appli (Icon *icon, CairoDock *pMainDock)
{
	cd_message ("%s (%s)", __func__, icon->cName);
	CairoDock *pParentDock = pMainDock;
	if (CAIRO_DOCK_IS_APPLI (icon) && myTaskbarParam.bGroupAppliByClass && icon->cClass != NULL && ! cairo_dock_class_is_expanded (icon->cClass))  // if this is a valid appli and we want to group the classes.
	{
		Icon *pSameClassIcon = cairo_dock_get_classmate (icon);  // un inhibiteur dans un dock avec appli ou subdock OU une appli de meme classe dans un dock != class-sub-dock.
		if (pSameClassIcon == NULL)  // aucun classmate => elle va dans son class sub-dock ou dans le main dock.
		{
			cd_message ("  no classmate for %s", icon->cClass);
			pParentDock = cairo_dock_get_class_subdock (icon->cClass);
			if (pParentDock == NULL)  // no class sub-dock => go to main dock
				pParentDock = pMainDock;
		}
		else  // on la met dans le sous-dock de sa classe.
		{
			//\____________ create the class sub-dock if necessary
			pParentDock = cairo_dock_get_class_subdock (icon->cClass);
			if (pParentDock == NULL)  // no class sub-dock yet -> create it, and we'll link it to either pSameClassIcon or a class-icon.
			{
				cd_message ("  creation du dock pour la classe %s", icon->cClass);
				pMainDock = CAIRO_DOCK(cairo_dock_get_icon_container (pSameClassIcon));
				pParentDock = cairo_dock_create_class_subdock (icon->cClass, pMainDock);
			}
			else
				cd_message ("  sous-dock de la classe %s existant", icon->cClass);
			
			//\____________ link this sub-dock to the inhibitor, or to a fake appli icon.
			if (GLDI_OBJECT_IS_LAUNCHER_ICON (pSameClassIcon) || GLDI_OBJECT_IS_APPLET_ICON (pSameClassIcon))  // it's an inhibitor
			{
				if (pSameClassIcon->pAppli != NULL)  // actuellement l'inhibiteur inhibe 1 seule appli.
				{
					cd_debug ("actuellement l'inhibiteur inhibe 1 seule appli");
					Icon *pInhibitedIcon = cairo_dock_get_appli_icon (pSameClassIcon->pAppli);  // get the currently inhibited appli-icon; it will go into the class sub-dock with 'icon'
					gldi_icon_unset_appli (pSameClassIcon);  // on lui laisse par contre l'indicateur.
					if (pSameClassIcon->pSubDock == NULL)  // paranoia
					{
						if (pSameClassIcon->cInitialName != NULL)
							gldi_icon_set_name (pSameClassIcon, pSameClassIcon->cInitialName);  // on lui remet son nom de lanceur.
						
						pSameClassIcon->pSubDock = pParentDock;
						
						cairo_dock_redraw_icon (pSameClassIcon);  // on la redessine car elle prend l'indicateur de classe.
					}
					else
						cd_warning ("this launcher (%s) already has a subdock !", pSameClassIcon->cName);
					
					if (pInhibitedIcon != NULL)  // paranoia
					{
						cd_debug (" on insere %s dans le dock de la classe", pInhibitedIcon->cName);
						gldi_icon_insert_in_container (pInhibitedIcon, CAIRO_CONTAINER(pParentDock), ! CAIRO_DOCK_ANIMATE_ICON);
					}
					else
						cd_warning ("couldn't get the appli-icon for '%s' !", pSameClassIcon->cName);
				}
				else if (pSameClassIcon->pSubDock != pParentDock)
					cd_warning ("this inhibitor doesn't hold the class sub-dock !");
			}
			else  // it's an appli in a dock (not the class sub-dock)
			{
				//\______________ make a class-icon that will hold the class sub-dock
				cd_debug (" on cree un fake...");
				CairoDock *pClassMateParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pSameClassIcon));  // c'est en fait le main dock.
				if (!pClassMateParentDock)  // if not yet in its dock (or hidden)
					pClassMateParentDock = gldi_dock_get (pSameClassIcon->cParentDockName);
				Icon *pFakeClassIcon = gldi_class_icon_new (pSameClassIcon, pParentDock);
				
				//\______________ detach the classmate, and put it into the class sub-dock with 'icon'
				cd_debug (" on detache %s pour la passer dans le sous-dock de sa classe", pSameClassIcon->cName);
				gldi_icon_detach (pSameClassIcon);
				gldi_icon_insert_in_container (pSameClassIcon, CAIRO_CONTAINER(pParentDock), ! CAIRO_DOCK_ANIMATE_ICON);
				
				//\______________ put the class-icon in place of the clasmate
				cd_debug (" on lui substitue le fake");
				gldi_icon_insert_in_container (pFakeClassIcon, CAIRO_CONTAINER(pClassMateParentDock), ! CAIRO_DOCK_ANIMATE_ICON);
				
				if (!myIndicatorsParam.bUseClassIndic)
					cairo_dock_trigger_redraw_subdock_content_on_icon (pFakeClassIcon);
			}
		}
	}
	else  /// TODO: look for an inhibitor or a classmate to go in its dock (it's not necessarily the main dock) ...
	{
		pParentDock = pMainDock;
	}
	return pParentDock;
}

CairoDock *gldi_appli_icon_insert_in_dock (Icon *icon, CairoDock *pMainDock, gboolean bAnimate)
{
	if (! myTaskbarParam.bShowAppli)
		return NULL;
	cd_message ("%s (%s, %p)", __func__, icon->cName, icon->pAppli);
	
	if (myTaskbarParam.bAppliOnCurrentDesktopOnly && ! gldi_window_is_on_current_desktop (icon->pAppli))
		return NULL;
	
	//\_________________ On gere ses eventuels inhibiteurs.
	if (myTaskbarParam.bMixLauncherAppli && cairo_dock_prevent_inhibited_class (icon))
	{
		cd_message (" -> se fait inhiber");
		return NULL;
	}
	
	//\_________________ On gere le filtre 'applis minimisees seulement'.
	if (!icon->pAppli->bIsHidden && myTaskbarParam.bHideVisibleApplis)
	{
		gldi_appli_reserve_geometry_for_window_manager (icon->pAppli, icon, pMainDock);  // on reserve la position de l'icone dans le dock pour que l'effet de minimisation puisse viser la bonne position avant que l'icone ne soit effectivement dans le dock.
		return NULL;
	}
	
	//\_________________ On determine dans quel dock l'inserer (cree au besoin).
	CairoDock *pParentDock = _get_parent_dock_for_appli (icon, pMainDock);
	g_return_val_if_fail (pParentDock != NULL, NULL);

	//\_________________ On l'insere dans son dock parent en animant ce dernier eventuellement.
	if (myTaskbarParam.bMixLauncherAppli && pParentDock != cairo_dock_get_class_subdock (icon->cClass))  // this appli is amongst the launchers in the main dock
	{
		cairo_dock_set_class_order_in_dock (icon, pParentDock);
	}
	else  // this appli is either in a different group or in the class sub-dock
	{
		cairo_dock_set_class_order_amongst_applis (icon, pParentDock);
	}
	gldi_icon_insert_in_container (icon, CAIRO_CONTAINER(pParentDock), bAnimate);
	cd_message (" insertion de %s complete (%.2f %.2fx%.2f) dans %s", icon->cName, icon->fInsertRemoveFactor, icon->fWidth, icon->fHeight, gldi_dock_get_name(pParentDock));

	if (bAnimate && cairo_dock_animation_will_be_visible (pParentDock))
	{
		cairo_dock_launch_animation (CAIRO_CONTAINER (pParentDock));
	}
	else
	{
		icon->fInsertRemoveFactor = 0;
		icon->fScale = 1.;
	}

	return pParentDock;
}

CairoDock * gldi_appli_icon_detach (Icon *pIcon)
{
	cd_debug ("%s (%s)", __func__, pIcon->cName);
	CairoDock *pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pIcon));
	if (! GLDI_OBJECT_IS_DOCK (pParentDock))
		return NULL;
	
	gldi_icon_detach (pIcon);
	
	if (pIcon->cClass != NULL && pParentDock == cairo_dock_get_class_subdock (pIcon->cClass))  // is in the sub-dock class -> check if we must destroy it.
	{
		gboolean bEmptyClassSubDock = cairo_dock_check_class_subdock_is_empty (pParentDock, pIcon->cClass);
		if (bEmptyClassSubDock)  // has been destroyed.
			return NULL;
	}
	return pParentDock;
}

#define x_icon_geometry(icon, pDock) (pDock->container.iWindowPositionX + icon->fXAtRest + (pDock->container.iWidth - pDock->iActiveWidth) * pDock->fAlign + (pDock->iActiveWidth - pDock->fFlatDockWidth) / 2)
///#define y_icon_geometry(icon, pDock) (pDock->container.iWindowPositionY + icon->fDrawY - icon->fHeight * myIconsParam.fAmplitude * pDock->fMagnitudeMax)
#define y_icon_geometry(icon, pDock) (pDock->container.iWindowPositionY + icon->fDrawY)
void gldi_appli_icon_set_geometry_for_window_manager (Icon *icon, CairoDock *pDock)
{
	//g_print ("%s (%s)\n", __func__, icon->cName);
	int iX, iY, iWidth, iHeight;
	iX = x_icon_geometry (icon, pDock);
	iY = y_icon_geometry (icon, pDock);  // il faudrait un fYAtRest ...
	//g_print (" -> %d;%d (%.2f)\n", iX - pDock->container.iWindowPositionX, iY - pDock->container.iWindowPositionY, icon->fXAtRest);
	iWidth = icon->fWidth;
	int dh = (icon->image.iWidth - icon->fHeight);
	iHeight = icon->fHeight + 2 * dh;  // on elargit en haut et en bas, pour gerer les cas ou l'icone grossirait vers le haut ou vers le bas.
	
	if (pDock->container.bIsHorizontal)
		gldi_window_set_thumbnail_area (icon->pAppli, pDock->container.pWidget, iX, iY - dh, iWidth, iHeight);
	else
		gldi_window_set_thumbnail_area (icon->pAppli, pDock->container.pWidget, iY - dh, iX, iHeight, iWidth);
}

void gldi_appli_reserve_geometry_for_window_manager (GldiWindowActor *pAppli, Icon *icon, CairoDock *pMainDock)
{
	if (CAIRO_DOCK_IS_APPLI (icon) && cairo_dock_get_icon_container(icon) == NULL)  // a detached appli
	{
		/// TODO: use the same algorithm as the class-manager to find the future position of the icon ...
		
		Icon *pInhibitor = cairo_dock_get_inhibitor (icon, FALSE);  // FALSE <=> meme en-dehors d'un dock
		if (pInhibitor == NULL)  // cette icone n'est pas inhibee, donc se minimisera dans le dock en une nouvelle icone.
		{
			int x, y;
			Icon *pClassmate = cairo_dock_get_classmate (icon);
			CairoDock *pClassmateDock = (pClassmate ? CAIRO_DOCK(cairo_dock_get_icon_container (pClassmate)) : NULL);
			if (myTaskbarParam.bGroupAppliByClass && pClassmate != NULL && pClassmateDock != NULL)  // on va se grouper avec cette icone.
			{
				x = x_icon_geometry (pClassmate, pClassmateDock);
				if (cairo_dock_is_hidden (pMainDock))
				{
					y = (pClassmateDock->container.bDirectionUp ? 0 : gldi_desktop_get_height());
				}
				else
				{
					y = y_icon_geometry (pClassmate, pClassmateDock);
				}
			}
			else if (myTaskbarParam.bMixLauncherAppli && pClassmate != NULL && pClassmateDock != NULL)  // on va se placer a cote.
			{
				x = x_icon_geometry (pClassmate, pClassmateDock) + pClassmate->fWidth/2;
				if (cairo_dock_is_hidden (pClassmateDock))
				{
					y = (pClassmateDock->container.bDirectionUp ? 0 : gldi_desktop_get_height());
				}
				else
				{
					y = y_icon_geometry (pClassmate, pClassmateDock);
				}
			}
			else  // on va se placer a la fin de la barre des taches.
			{
				Icon *pIcon, *pLastLauncher = NULL;
				GList *ic;
				for (ic = pMainDock->icons; ic != NULL; ic = ic->next)
				{
					pIcon = ic->data;
					if (GLDI_OBJECT_IS_LAUNCHER_ICON (pIcon)  // launcher, even without class
					|| GLDI_OBJECT_IS_STACK_ICON (pIcon)  // container icon (likely to contain some launchers)
					|| (GLDI_OBJECT_IS_APPLET_ICON (pIcon) && pIcon->cClass != NULL)  // applet acting like a launcher
					|| (GLDI_OBJECT_IS_SEPARATOR_ICON (pIcon)))  // separator (user or auto).
					{
						pLastLauncher = pIcon;
					}
				}
						
				if (pLastLauncher != NULL)  // on se placera juste apres.
				{
					x = x_icon_geometry (pLastLauncher, pMainDock) + pLastLauncher->fWidth/2;
					if (cairo_dock_is_hidden (pMainDock))
					{
						y = (pMainDock->container.bDirectionUp ? 0 : gldi_desktop_get_height());
					}
					else
					{
						y = y_icon_geometry (pLastLauncher, pMainDock);
					}
				}
				else  // aucune icone avant notre groupe, on sera insere en 1er.
				{
					x = pMainDock->container.iWindowPositionX + 0 + (pMainDock->container.iWidth - pMainDock->fFlatDockWidth) / 2;
					if (cairo_dock_is_hidden (pMainDock))
					{
						y = (pMainDock->container.bDirectionUp ? 0 : gldi_desktop_get_height());
					}
					else
					{
						y = pMainDock->container.iWindowPositionY;
					}
				}
			}
			//g_print (" - %s en (%d;%d)\n", icon->cName, x, y);
			if (pMainDock->container.bIsHorizontal)
				gldi_window_set_minimize_position (pAppli, NULL, x, y);
			else
				gldi_window_set_minimize_position (pAppli, NULL, y, x);
		}
		else
		{
			/// gerer les desklets...
			
		}
	}
}


const CairoDockImageBuffer *gldi_appli_icon_get_image_buffer (Icon *pIcon)
{
	static CairoDockImageBuffer image;
	
	// if the given icon is not loaded
	if (pIcon->image.pSurface == NULL)
	{
		// try to get the image from the class
		const CairoDockImageBuffer *pImageBuffer = cairo_dock_get_class_image_buffer (pIcon->cClass);
		if (pImageBuffer && pImageBuffer->pSurface)
		{
			return pImageBuffer;
		}
		
		// try to load the icon.
		if (g_pMainDock)
		{
			// set a size (we could set any size, but let's set something useful: if the icon is inserted in a dock and is already loaded at the correct size, it won't be loaded again).
			gboolean bNoContainer = FALSE;
			if (pIcon->pContainer == NULL)  // not in a container (=> no size) -> set a size before loading it.
			{
				bNoContainer = TRUE;
				cairo_dock_set_icon_container (pIcon, g_pPrimaryContainer);
				pIcon->fWidth = pIcon->fHeight = 0;  /// useful ?...
				pIcon->iRequestedWidth = pIcon->iRequestedHeight = 0;  // no request
				cairo_dock_set_icon_size_in_dock (g_pMainDock, pIcon);
			}

			// load the icon
			cairo_dock_load_icon_image (pIcon, g_pPrimaryContainer);
			if (bNoContainer)
			{
				cairo_dock_set_icon_container (pIcon, NULL);
			}
		}
	}
	
	// if the given icon is loaded, use its image.
	if (pIcon->image.pSurface != NULL || pIcon->image.iTexture != 0)
	{
		memcpy (&image, &pIcon->image, sizeof (CairoDockImageBuffer));
		return &image;
	}
	else
	{
		return NULL;
	}
}


static gboolean _set_inhibitor_name (Icon *pInhibitorIcon, const gchar *cNewName)
{
	if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon))
	{
		cd_debug (" %s change son nom en %s", pInhibitorIcon->cName, cNewName);
		if (pInhibitorIcon->cInitialName == NULL)
		{
			pInhibitorIcon->cInitialName = pInhibitorIcon->cName;
			cd_debug ("pInhibitorIcon->cInitialName <- %s", pInhibitorIcon->cInitialName);
		}
		else
			g_free (pInhibitorIcon->cName);
		pInhibitorIcon->cName = NULL;
		
		gldi_icon_set_name (pInhibitorIcon, (cNewName != NULL ? cNewName : pInhibitorIcon->cInitialName));
	}
	cairo_dock_redraw_icon (pInhibitorIcon);
	return TRUE;  // continue
}
void gldi_window_inhibitors_set_name (GldiWindowActor *actor, const gchar *cNewName)
{
	gldi_window_foreach_inhibitor (actor, (GldiIconRFunc)_set_inhibitor_name, (gpointer)cNewName);
}


static gboolean _set_active_state (Icon *pInhibitorIcon, gpointer data)
{
	gboolean bActive = GPOINTER_TO_INT(data);
	if (bActive)
	{
		CairoDock *pParentDock = CAIRO_DOCK(cairo_dock_get_icon_container (pInhibitorIcon));
		if (pParentDock != NULL)
			gldi_appli_icon_animate_on_active (pInhibitorIcon, pParentDock);
	}
	else
	{
		cairo_dock_redraw_icon (pInhibitorIcon);
	}
	return TRUE;  // continue
}
void gldi_window_inhibitors_set_active_state (GldiWindowActor *actor, gboolean bActive)
{
	gldi_window_foreach_inhibitor (actor, (GldiIconRFunc)_set_active_state, GINT_TO_POINTER(bActive));
}


static gboolean _set_hidden_state (Icon *pInhibitorIcon, G_GNUC_UNUSED gpointer data)
{
	if (! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pInhibitorIcon) && myTaskbarParam.fVisibleAppliAlpha != 0)
	{
		pInhibitorIcon->fAlpha = 1;  // on triche un peu.
		cairo_dock_redraw_icon (pInhibitorIcon);
	}
	return TRUE;  // continue
}
void gldi_window_inhibitors_set_hidden_state (GldiWindowActor *actor, gboolean bIsHidden)
{
	gldi_window_foreach_inhibitor (actor, (GldiIconRFunc)_set_hidden_state, GINT_TO_POINTER(bIsHidden));
}
