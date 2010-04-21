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
#include <string.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>

#include <gtk/gtkgl.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <gdk/x11/gdkglx.h>

#include "../config.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-dialogs.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-dialog-factory.h"
#include "cairo-dock-dialog-manager.h"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern gboolean g_bSticky;
extern gboolean g_bKeepAbove;
extern gboolean g_bUseOpenGL;

static GSList *s_pDialogList = NULL;
static cairo_surface_t *s_pButtonOkSurface = NULL;
static cairo_surface_t *s_pButtonCancelSurface = NULL;
static GLuint s_iButtonOkTexture = 0;
static GLuint s_iButtonCancelTexture = 0;

static gboolean _cairo_dock_render_dialog_notification (gpointer data, CairoDialog *pDialog, cairo_t *pCairoContext);
static void cairo_dock_place_dialog (CairoDialog *pDialog, CairoContainer *pContainer);

void cairo_dock_init_dialog_manager (void)
{
	cairo_dock_register_notification (CAIRO_DOCK_RENDER_DIALOG,
		(CairoDockNotificationFunc) _cairo_dock_render_dialog_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
}

static inline cairo_surface_t *_cairo_dock_load_button_icon (const gchar *cButtonImage, const gchar *cDefaultButtonImage)
{
	//g_print ("%s (%d ; %d)\n", __func__, myDialogs.iDialogButtonWidth, myDialogs.iDialogButtonHeight);
	cairo_surface_t *pButtonSurface = cairo_dock_create_surface_from_image_simple (cButtonImage,
		myDialogs.iDialogButtonWidth,
		myDialogs.iDialogButtonHeight);

	if (pButtonSurface == NULL)
	{
		gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, cDefaultButtonImage);
		//g_print ("  on charge %s par defaut\n", cIconPath);
		pButtonSurface = cairo_dock_create_surface_from_image_simple (cIconPath,
			myDialogs.iDialogButtonWidth,
			myDialogs.iDialogButtonHeight);
		g_free (cIconPath);
	}

	return pButtonSurface;
}
#define _load_button_texture(iTexture, pSurface) do {\
	if (iTexture != 0)\
		_cairo_dock_delete_texture (iTexture);\
	if (pSurface != NULL)\
		iTexture = cairo_dock_create_texture_from_surface (pSurface);\
	else\
		iTexture = 0; } while (0)
void cairo_dock_load_dialog_buttons (CairoContainer *pContainer, gchar *cButtonOkImage, gchar *cButtonCancelImage)
{
	//g_print ("%s (%s ; %s)\n", __func__, cButtonOkImage, cButtonCancelImage);
	if (s_pButtonOkSurface != NULL)
		cairo_surface_destroy (s_pButtonOkSurface);
	s_pButtonOkSurface = _cairo_dock_load_button_icon (cButtonOkImage, "cairo-dock-ok.svg");

	if (s_pButtonCancelSurface != NULL)
		cairo_surface_destroy (s_pButtonCancelSurface);
	s_pButtonCancelSurface = _cairo_dock_load_button_icon (cButtonCancelImage, "cairo-dock-cancel.svg");
	
	if (0 && g_bUseOpenGL)
	{
		_load_button_texture (s_iButtonOkTexture, s_pButtonOkSurface);
		_load_button_texture (s_iButtonCancelTexture, s_pButtonCancelSurface);
	}
}

void cairo_dock_unload_dialog_buttons (void)
{
	if (s_pButtonOkSurface != NULL)
	{
		cairo_surface_destroy (s_pButtonOkSurface);
		s_pButtonOkSurface = NULL;
	}
	if (s_pButtonCancelSurface != NULL)
	{
		cairo_surface_destroy (s_pButtonCancelSurface);
		s_pButtonCancelSurface = NULL;
	}
	if (s_iButtonOkTexture != 0)
	{
		_cairo_dock_delete_texture (s_iButtonOkTexture);
		s_iButtonOkTexture = 0;
	}
	if (s_iButtonCancelTexture != 0)
	{
		_cairo_dock_delete_texture (s_iButtonCancelTexture);
		s_iButtonCancelTexture = 0;
	}
}


static gboolean on_enter_dialog (GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDialog *pDialog)
{
	//cd_debug ("inside");
	pDialog->container.bInside = TRUE;
	return FALSE;
}

static gboolean on_leave_dialog (GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDialog *pDialog)
{
	int iMouseX, iMouseY;
	gdk_window_get_pointer (pDialog->container.pWidget->window, &iMouseX, &iMouseY, NULL);
	if (iMouseX > 0 && iMouseX < pDialog->container.iWidth && iMouseY > 0 && iMouseY < pDialog->container.iHeight)
	{
		cd_debug ("en fait on est dedans");
		return FALSE;
	}
	
	//g_print ("leave\n");
	//cd_debug ("outside (%d;%d / %dx%d)", iMouseX, iMouseY, pDialog->container.iWidth, pDialog->container.iHeight);
	pDialog->container.bInside = FALSE;
	Icon *pIcon = pDialog->pIcon;
	if (pIcon != NULL /*&& (pEvent->state & GDK_BUTTON1_MASK) == 0*/)
	{
		pDialog->container.iMouseX = pEvent->x_root;
		pDialog->container.iMouseY = pEvent->y_root;
		CairoContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
		cairo_dock_place_dialog (pDialog, pContainer);
	}

	return FALSE;
}

static int _cairo_dock_find_clicked_button_in_dialog (GdkEventButton* pButton, CairoDialog *pDialog)
{
	int iButtonX, iButtonY;
	int i, n = pDialog->iNbButtons;
	iButtonY = (pDialog->container.bDirectionUp ?
		pDialog->iTopMargin + pDialog->iMessageHeight + pDialog->iInteractiveHeight + CAIRO_DIALOG_VGAP :
		pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iButtonsHeight));
	int iMinButtonX = .5 * (pDialog->container.iWidth - (n - 1) * CAIRO_DIALOG_BUTTON_GAP - n * myDialogs.iDialogButtonWidth);
	for (i = 0; i < pDialog->iNbButtons; i++)
	{
		iButtonX = iMinButtonX + i * (CAIRO_DIALOG_BUTTON_GAP + myDialogs.iDialogButtonWidth);
		if (pButton->x >= iButtonX && pButton->x <= iButtonX + myDialogs.iDialogButtonWidth && pButton->y >= iButtonY && pButton->y <= iButtonY + myDialogs.iDialogButtonHeight)
		{
			return i;
		}
	}
	return -1;
}

static gboolean on_button_press_dialog (GtkWidget* pWidget,
	GdkEventButton* pButton,
	CairoDialog *pDialog)
{
	if (pButton->button == 1)  // clique gauche.
	{
		if (pButton->type == GDK_BUTTON_PRESS)
		{
			if (pDialog->pButtons == NULL && pDialog->pInteractiveWidget == NULL)  // ce n'est pas un dialogue interactif.
			{
				cairo_dock_dialog_unreference (pDialog);
			}
			else if (pDialog->pButtons != NULL)
			{
				int iButton = _cairo_dock_find_clicked_button_in_dialog (pButton, pDialog);
				if (iButton >= 0 && iButton < pDialog->iNbButtons)
				{
					pDialog->pButtons[iButton].iOffset = CAIRO_DIALOG_BUTTON_OFFSET;
					gtk_widget_queue_draw (pDialog->container.pWidget);
				}
			}
		}
		else if (pButton->type == GDK_BUTTON_RELEASE)
		{
			if (pDialog->pButtons != NULL)
			{
				int iButton = _cairo_dock_find_clicked_button_in_dialog (pButton, pDialog);
				cd_debug ("clic on button %d", iButton);
				if (iButton >= 0 && iButton < pDialog->iNbButtons && pDialog->pButtons[iButton].iOffset != 0)
				{
					cd_debug (" -> action !");
					pDialog->pButtons[iButton].iOffset = 0;
					pDialog->action_on_answer (iButton, pDialog->pInteractiveWidget, pDialog->pUserData, pDialog);
					gtk_widget_queue_draw (pDialog->container.pWidget);  // au cas ou le unref ci-dessous ne le detruirait pas.
					cairo_dock_dialog_unreference (pDialog);
				}
				else
				{
					int i;
					for (i = 0; i < pDialog->iNbButtons; i++)
						pDialog->pButtons[i].iOffset = 0;
					gtk_widget_queue_draw (pDialog->container.pWidget);
				}
			}
		}
	}

	return FALSE;
}

static gboolean on_key_press_dialog (GtkWidget *pWidget,
	GdkEventKey *pKey,
	CairoDialog *pDialog)
{
	cd_debug ("key pressed");
	
	if (pKey->type == GDK_KEY_PRESS && pDialog->action_on_answer != NULL)
	{
		GdkEventScroll dummyScroll;
		int iX, iY;
		switch (pKey->keyval)
		{
			case GDK_Return :
				pDialog->action_on_answer (-1, pDialog->pInteractiveWidget, pDialog->pUserData, pDialog);
				cairo_dock_dialog_unreference (pDialog);
			break ;
			case GDK_Escape :
				pDialog->action_on_answer (-2, pDialog->pInteractiveWidget, pDialog->pUserData, pDialog);
				cairo_dock_dialog_unreference (pDialog);
			break ;
		}
	}
	return FALSE;
}

static void _cairo_dock_draw_inside_dialog_opengl (CairoDialog *pDialog, double fAlpha)
{
	_cairo_dock_enable_texture ();
	_cairo_dock_set_blend_alpha ();
	_cairo_dock_set_alpha (fAlpha);
	
	double x, y;
	if (pDialog->iIconTexture != 0)
	{
		x = pDialog->iLeftMargin;
		y = (pDialog->container.bDirectionUp ? pDialog->iTopMargin : pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight));
		
		glBindTexture (GL_TEXTURE_2D, pDialog->iIconTexture);
		_cairo_dock_apply_current_texture_portion_at_size_with_offset (1.*pDialog->iCurrentFrame/pDialog->iNbFrames, 0.,
			1. / pDialog->iNbFrames, 1.,
			pDialog->iIconSize, pDialog->iIconSize,
			x + pDialog->iIconSize/2, pDialog->container.iHeight - y - pDialog->iIconSize/2);
	}
	
	if (pDialog->iTextTexture != 0)
	{
		x = pDialog->iLeftMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN;
		y = (pDialog->container.bDirectionUp ? pDialog->iTopMargin : pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight));
		if (pDialog->iTextHeight < pDialog->iMessageHeight)  // on centre le texte.
			y += (pDialog->iMessageHeight - pDialog->iTextHeight) / 2;
		
		glBindTexture (GL_TEXTURE_2D, pDialog->iTextTexture);
		if (pDialog->iMaxTextWidth != 0 && pDialog->iTextWidth > pDialog->iMaxTextWidth)
		{
			double w = MIN (pDialog->iMaxTextWidth, pDialog->iTextWidth - pDialog->iCurrentTextOffset);  // taille de la portion de texte a gauche.
			_cairo_dock_apply_current_texture_portion_at_size_with_offset (1.*pDialog->iCurrentTextOffset/pDialog->iTextWidth, 0.,
				1.*(pDialog->iTextWidth - pDialog->iCurrentTextOffset) / pDialog->iTextWidth, 1.,
				w, pDialog->iTextHeight,
				x + w/2, pDialog->container.iHeight - y - pDialog->iTextHeight/2);
			
			if (pDialog->iTextWidth - pDialog->iCurrentTextOffset < pDialog->iMaxTextWidth)
			{
				w = pDialog->iMaxTextWidth - (pDialog->iTextWidth - pDialog->iCurrentTextOffset);  // taille de la portion de texte a droite.
				_cairo_dock_apply_current_texture_portion_at_size_with_offset (0., 0.,
					w / pDialog->iTextWidth, 1.,
					w, pDialog->iTextHeight,
					x + pDialog->iMaxTextWidth - w/2, pDialog->container.iHeight - y - pDialog->iTextHeight/2);
			}
		}
		else
		{
			_cairo_dock_apply_current_texture_at_size_with_offset (pDialog->iTextWidth, pDialog->iTextHeight,
				x + pDialog->iTextWidth/2, pDialog->container.iHeight - y - pDialog->iTextHeight/2);
		}
	}
	
	if (pDialog->pButtons != NULL)
	{
		int iButtonX, iButtonY;
		int i, n = pDialog->iNbButtons;
		iButtonY = (pDialog->container.bDirectionUp ? pDialog->iTopMargin + pDialog->iMessageHeight + pDialog->iInteractiveHeight + CAIRO_DIALOG_VGAP : pDialog->container.iHeight - pDialog->iTopMargin - pDialog->iButtonsHeight - CAIRO_DIALOG_VGAP);
		int iMinButtonX = .5 * (pDialog->container.iWidth - (n - 1) * CAIRO_DIALOG_BUTTON_GAP - n * myDialogs.iDialogButtonWidth);
		for (i = 0; i < pDialog->iNbButtons; i++)
		{
			iButtonX = iMinButtonX + i * (CAIRO_DIALOG_BUTTON_GAP + myDialogs.iDialogButtonWidth);
			glBindTexture (GL_TEXTURE_2D, pDialog->pButtons[i].iTexture);
			_cairo_dock_apply_current_texture_at_size_with_offset (myDialogs.iDialogButtonWidth,
				myDialogs.iDialogButtonWidth,
				iButtonX + pDialog->pButtons[i].iOffset + myDialogs.iDialogButtonWidth/2,
				pDialog->container.iHeight - (iButtonY + pDialog->pButtons[i].iOffset + myDialogs.iDialogButtonWidth/2));			}
	}
	
	if (pDialog->pRenderer != NULL && pDialog->pRenderer->render_opengl)
		pDialog->pRenderer->render_opengl (pDialog, fAlpha);
}

#define _paint_inside_dialog(pCairoContext, fAlpha) do { \
	if (fAlpha != 0) \
		cairo_paint_with_alpha (pCairoContext, fAlpha); \
	else \
		cairo_paint (pCairoContext); } while (0)
static void _cairo_dock_draw_inside_dialog (cairo_t *pCairoContext, CairoDialog *pDialog, double fAlpha)
{
	double x, y;
	if (pDialog->pIconBuffer != NULL)
	{
		x = pDialog->iLeftMargin;
		y = (pDialog->container.bDirectionUp ? pDialog->iTopMargin : pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight));
		if (pDialog->iNbFrames > 1)
		{
			cairo_save (pCairoContext);
			cairo_rectangle (pCairoContext,
				x,
				y,
				pDialog->iIconSize,
				pDialog->iIconSize);
			cairo_clip (pCairoContext);
		}
		
		cairo_set_source_surface (pCairoContext,
			pDialog->pIconBuffer,
			x - (pDialog->iCurrentFrame * pDialog->iIconSize),
			y);
		_paint_inside_dialog(pCairoContext, fAlpha);
		if (pDialog->iNbFrames > 1)
			cairo_restore (pCairoContext);
	}
	
	if (pDialog->pTextBuffer != NULL)
	{
		x = pDialog->iLeftMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN;
		y = (pDialog->container.bDirectionUp ? pDialog->iTopMargin : pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight));
		if (pDialog->iTextHeight < pDialog->iMessageHeight)  // on centre le texte.
			y += (pDialog->iMessageHeight - pDialog->iTextHeight) / 2;
		if (pDialog->iMaxTextWidth != 0 && pDialog->iTextWidth > pDialog->iMaxTextWidth)
		{
			cairo_save (pCairoContext);
			cairo_rectangle (pCairoContext,
				x,
				y,
				pDialog->iMaxTextWidth,
				pDialog->iTextHeight);
			cairo_clip (pCairoContext);
		}
		cairo_set_source_surface (pCairoContext,
			pDialog->pTextBuffer,
			x - pDialog->iCurrentTextOffset,
			y);
		_paint_inside_dialog(pCairoContext, fAlpha);
		
		if (pDialog->iMaxTextWidth != 0 && pDialog->iTextWidth > pDialog->iMaxTextWidth)
		{
			cairo_set_source_surface (pCairoContext,
				pDialog->pTextBuffer,
				x - pDialog->iCurrentTextOffset + pDialog->iTextWidth + 10,
				y);
			_paint_inside_dialog (pCairoContext, fAlpha);
			cairo_restore (pCairoContext);
		}
	}
	
	if (pDialog->pButtons != NULL)
	{
		int iButtonX, iButtonY;
		int i, n = pDialog->iNbButtons;
		iButtonY = (pDialog->container.bDirectionUp ? pDialog->iTopMargin + pDialog->iMessageHeight + pDialog->iInteractiveHeight + CAIRO_DIALOG_VGAP : pDialog->container.iHeight - pDialog->iTopMargin - pDialog->iButtonsHeight - CAIRO_DIALOG_VGAP);
		int iMinButtonX = .5 * (pDialog->container.iWidth - (n - 1) * CAIRO_DIALOG_BUTTON_GAP - n * myDialogs.iDialogButtonWidth);
		cairo_surface_t *pButtonSurface;
		for (i = 0; i < pDialog->iNbButtons; i++)
		{
			iButtonX = iMinButtonX + i * (CAIRO_DIALOG_BUTTON_GAP + myDialogs.iDialogButtonWidth);
			if (pDialog->pButtons[i].pSurface != NULL)
				pButtonSurface = pDialog->pButtons[i].pSurface;
			else if (pDialog->pButtons[i].iDefaultType == 1)
				pButtonSurface = s_pButtonOkSurface;
			else
				pButtonSurface = s_pButtonCancelSurface;
			cairo_set_source_surface (pCairoContext,
				pButtonSurface,
				iButtonX + pDialog->pButtons[i].iOffset,
				iButtonY + pDialog->pButtons[i].iOffset);
			_paint_inside_dialog(pCairoContext, fAlpha);
		}
	}
	
	if (pDialog->pRenderer != NULL)
		pDialog->pRenderer->render (pCairoContext, pDialog, fAlpha);
}

static gboolean _cairo_dock_render_dialog_notification (gpointer data, CairoDialog *pDialog, cairo_t *pCairoContext)
{
	if (pCairoContext == NULL)
	{
		_cairo_dock_draw_inside_dialog_opengl (pDialog, 0.);
		if (pDialog->container.bUseReflect)
		{
			glTranslatef (0.,
				pDialog->container.iHeight - 2* (pDialog->iTopMargin + pDialog->iBubbleHeight),
				0.);
			glScalef (1., -1., 1.);
			_cairo_dock_draw_inside_dialog_opengl (pDialog, pDialog->container.fRatio);
		}
	}
	else
	{
		_cairo_dock_draw_inside_dialog (pCairoContext, pDialog, 0.);
		
		if (pDialog->container.bUseReflect)
		{
			cairo_save (pCairoContext);
			cairo_rectangle (pCairoContext,
				0.,
				pDialog->iTopMargin + pDialog->iBubbleHeight,
				pDialog->iBubbleWidth,
				pDialog->iBottomMargin);
			//g_print( "pDialog->iBottomMargin:%d\n", pDialog->iBottomMargin);
			cairo_clip (pCairoContext);
	
			cairo_translate (pCairoContext,
				0.,
				2* (pDialog->iTopMargin + pDialog->iBubbleHeight));
			cairo_scale (pCairoContext, 1., -1.);
			_cairo_dock_draw_inside_dialog (pCairoContext, pDialog, pDialog->container.fRatio);
		}
	}
	return CAIRO_DOCK_LET_PASS_NOTIFICATION;
}



gboolean cairo_dock_dialog_reference (CairoDialog *pDialog)
{
	if (pDialog != NULL && pDialog->iRefCount > 0)
	{
		pDialog->iRefCount ++;
		return TRUE;  // on peut l'utiliser.
	}
	return FALSE;
}

gboolean cairo_dock_dialog_unreference (CairoDialog *pDialog)
{
	//g_print ("%s (%d)\n", __func__, pDialog->iRefCount);
	if (pDialog != NULL && pDialog->iRefCount > 0)
	{
		pDialog->iRefCount --;
		if (pDialog->iRefCount == 0)  // devient nul.
		{
			cairo_dock_free_dialog (pDialog);
			s_pDialogList = g_slist_remove (s_pDialogList, pDialog);
			cairo_dock_replace_all_dialogs ();
			return TRUE;
		}
		else
			return FALSE;  // il n'est pas mort.
	}
	return TRUE;
}


gboolean cairo_dock_remove_dialog_if_any_full (Icon *icon, gboolean bAll)
{
	g_return_val_if_fail (icon != NULL, FALSE);
	cd_debug ("%s (%s)", __func__, (icon?icon->cName : "nobody"));
	CairoDialog *pDialog;
	GSList *dl, *next_dl;
	gboolean bDialogRemoved = FALSE;

	if (s_pDialogList == NULL)
		return FALSE;

	dl = s_pDialogList;
	do
	{
		next_dl = dl->next;  // si on enleve l'element courant, on perd 'dl'.
		pDialog = dl->data;
		if (pDialog->pIcon == icon && (bAll || pDialog->action_on_answer == NULL))
		{
			cairo_dock_dialog_unreference (pDialog);
			bDialogRemoved = TRUE;
		}
		dl = next_dl;
	} while (dl != NULL);

	return bDialogRemoved;
}


static gboolean _cairo_dock_dialog_auto_delete (CairoDialog *pDialog)
{
	if (pDialog != NULL)
	{
		pDialog->iSidTimer = 0;
		cairo_dock_dialog_unreference (pDialog);  // on pourrait eventuellement faire un fondu avant.
	}
	return FALSE;
}

CairoDialog *cairo_dock_build_dialog (CairoDialogAttribute *pAttribute, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_val_if_fail (pAttribute != NULL, NULL);
	
	Icon *pActiveAppli = cairo_dock_get_current_active_icon ();
	if (pActiveAppli && pActiveAppli->bIsFullScreen && cairo_dock_appli_is_on_current_desktop (pActiveAppli))
	///if (cairo_dock_search_window_covering_dock (g_pMainDock, FALSE, TRUE) != NULL)
	{
		cd_debug ("skip dialog since a fullscreen window would mask it");
		return NULL;
	}
	cd_debug ("%s (%s, %s, %x, %x, %x (%x;%x))", __func__, pAttribute->cText, pAttribute->cImageFilePath, pAttribute->pInteractiveWidget, pAttribute->pActionFunc, pAttribute->pTextDescription, pIcon, pContainer);
	
	//\________________ On cree un nouveau dialogue.
	CairoDialog *pDialog = cairo_dock_new_dialog (pAttribute, pIcon, pContainer);
	s_pDialogList = g_slist_prepend (s_pDialogList, pDialog);
	if (pDialog->iNbButtons != 0 && (s_pButtonOkSurface == NULL || s_pButtonCancelSurface == NULL))
		cairo_dock_load_dialog_buttons (CAIRO_CONTAINER (pDialog), myDialogs.cButtonOkImage, myDialogs.cButtonCancelImage);
	
	//\________________ on le place parmi les autres.
	cairo_dock_place_dialog (pDialog, pContainer);  // renseigne aussi bDirectionUp, bIsHorizontal, et iHeight.
	
	//\________________ On connecte les signaux utiles.
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"button-press-event",
		G_CALLBACK (on_button_press_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"button-release-event",
		G_CALLBACK (on_button_press_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"key-press-event",
		G_CALLBACK (on_key_press_dialog),
		pDialog);
	if (pIcon != NULL)  // on inhibe le deplacement du dialogue lorsque l'utilisateur est dedans.
	{
		g_signal_connect (G_OBJECT (pDialog->container.pWidget),
			"enter-notify-event",
			G_CALLBACK (on_enter_dialog),
			pDialog);
		g_signal_connect (G_OBJECT (pDialog->container.pWidget),
			"leave-notify-event",
			G_CALLBACK (on_leave_dialog),
			pDialog);
	}
	
	if (pAttribute->iTimeLength != 0)
		pDialog->iSidTimer = g_timeout_add (pAttribute->iTimeLength, (GSourceFunc) _cairo_dock_dialog_auto_delete, (gpointer) pDialog);
	
	if (pIcon && pIcon->cParentDockName != NULL)
		cairo_dock_dialog_window_created ();
	
	return pDialog;
}



static void _cairo_dock_dialog_find_optimal_placement (CairoDialog *pDialog)
{
	//g_print ("%s (Ybulle:%d; width:%d)\n", __func__, pDialog->container.iWindowPositionY, pDialog->container.iWidth);
	g_return_if_fail (pDialog->container.iWindowPositionY > 0);

	Icon *icon;
	CairoDialog *pDialogOnOurWay;

	double fXLeft = 0, fXRight = g_desktopGeometry.iXScreenWidth[pDialog->container.bIsHorizontal];
	if (pDialog->bRight)
	{
		fXLeft = -1e4;
		fXRight = MAX (g_desktopGeometry.iXScreenWidth[pDialog->container.bIsHorizontal], pDialog->iAimedX + pDialog->iMinFrameWidth + pDialog->iRightMargin + 1);  // 2*CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE
	}
	else
	{
		fXLeft = MIN (0, pDialog->iAimedX - (pDialog->iMinFrameWidth + pDialog->iLeftMargin + 1));  // 2*CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE
		fXRight = 1e4;
	}
	gboolean bCollision = FALSE;
	double fNextYStep = (pDialog->container.bDirectionUp ? -1e4 : 1e4);
	int iYInf, iYSup;
	GSList *ic;
	for (ic = s_pDialogList; ic != NULL; ic = ic->next)
	{
		pDialogOnOurWay = ic->data;
		if (pDialogOnOurWay == NULL)
			continue ;
		if (pDialogOnOurWay != pDialog)
		{
			if (GTK_WIDGET_VISIBLE (pDialogOnOurWay->container.pWidget) && pDialogOnOurWay->pIcon != NULL)
			{
				iYInf = (pDialog->container.bDirectionUp ? pDialogOnOurWay->container.iWindowPositionY : pDialogOnOurWay->container.iWindowPositionY + pDialogOnOurWay->container.iHeight - (pDialogOnOurWay->iBubbleHeight + 0));
				iYSup = (pDialog->container.bDirectionUp ? pDialogOnOurWay->container.iWindowPositionY + pDialogOnOurWay->iBubbleHeight + 0 : pDialogOnOurWay->container.iWindowPositionY + pDialogOnOurWay->container.iHeight);
				if (iYInf < pDialog->container.iWindowPositionY + pDialog->iBubbleHeight + 0 && iYSup > pDialog->container.iWindowPositionY)
				{
					//g_print ("pDialogOnOurWay : %d - %d ; pDialog : %d - %d\n", iYInf, iYSup, pDialog->container.iWindowPositionY, pDialog->container.iWindowPositionY + (pDialog->iBubbleHeight + 2 * myBackground.iDockLineWidth));
					if (pDialogOnOurWay->iAimedX < pDialog->iAimedX)
						fXLeft = MAX (fXLeft, pDialogOnOurWay->container.iWindowPositionX + pDialogOnOurWay->container.iWidth);
					else
						fXRight = MIN (fXRight, pDialogOnOurWay->container.iWindowPositionX);
					bCollision = TRUE;
					fNextYStep = (pDialog->container.bDirectionUp ? MAX (fNextYStep, iYInf) : MIN (fNextYStep, iYSup));
				}
			}
		}
	}
	//g_print (" -> [%.2f ; %.2f]\n", fXLeft, fXRight);

	if ((fXRight - fXLeft > pDialog->container.iWidth) && (
		(pDialog->bRight && fXLeft + pDialog->iLeftMargin < pDialog->iAimedX && fXRight > pDialog->iAimedX + pDialog->iMinFrameWidth + pDialog->iRightMargin)
		||
		(! pDialog->bRight && fXLeft < pDialog->iAimedX - pDialog->iMinFrameWidth - pDialog->iLeftMargin && fXRight > pDialog->iAimedX + pDialog->iRightMargin)
		))
	{
		if (pDialog->bRight)
			pDialog->container.iWindowPositionX = MIN (pDialog->iAimedX - pDialog->fAlign * pDialog->iBubbleWidth - pDialog->iLeftMargin, fXRight - pDialog->container.iWidth);
		else
			pDialog->container.iWindowPositionX = MAX (pDialog->iAimedX - pDialog->iRightMargin - pDialog->container.iWidth + pDialog->fAlign * pDialog->iBubbleWidth, fXLeft);  /// pDialog->iBubbleWidth (?)
	}
	else
	{
		//g_print (" * Aim : (%d ; %d) ; Width : %d\n", pDialog->iAimedX, pDialog->iAimedY, pDialog->container.iWidth);
		pDialog->container.iWindowPositionY = fNextYStep - (pDialog->container.bDirectionUp ? pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin : 0);
		_cairo_dock_dialog_find_optimal_placement (pDialog);
	}
}

static void cairo_dock_place_dialog (CairoDialog *pDialog, CairoContainer *pContainer)
{
	//g_print ("%s (%x;%d, %s)\n", __func__, pDialog->pIcon, pContainer, pDialog->pIcon?pDialog->pIcon->cParentDockName:"none");
	int w, h;
	GdkGravity iGravity;
	int iPrevPositionX = pDialog->container.iWindowPositionX, iPrevPositionY = pDialog->container.iWindowPositionY;
	if (pContainer != NULL && pDialog->pIcon != NULL)
	{
		cairo_dock_set_dialog_orientation (pDialog, pContainer);
		
		if (pDialog->container.bIsHorizontal)
		{
			if (! pDialog->container.bInside)
			{
				pDialog->container.iWindowPositionY = (pDialog->container.bDirectionUp ? pDialog->iAimedY - (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iMinBottomGap) : pDialog->iAimedY + pDialog->iMinBottomGap);  // on place la bulle d'abord sans prendre en compte la pointe.
				_cairo_dock_dialog_find_optimal_placement (pDialog);
			}
			else if (! pDialog->container.bDirectionUp)
				pDialog->container.iWindowPositionY += pDialog->iDistanceToDock;
		}
		else
		{
			int tmp = pDialog->iAimedX;
			pDialog->iAimedX = pDialog->iAimedY;
			pDialog->iAimedY = tmp;
			if (! pDialog->container.bInside)
			{
				pDialog->container.iWindowPositionX = (pDialog->bRight ? pDialog->iAimedX - pDialog->fAlign * pDialog->iBubbleWidth : pDialog->iAimedX - pDialog->container.iWidth + pDialog->fAlign * pDialog->iBubbleWidth);
				pDialog->container.iWindowPositionY = (pDialog->container.bDirectionUp ? pDialog->iAimedY - (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iMinBottomGap) : pDialog->iAimedY + pDialog->iMinBottomGap);  // on place la bulle (et non pas la fenetre) sans faire d'optimisation.
			}
		}
		//g_print (" => position : (%d;%d)\n", pDialog->container.iWindowPositionX, pDialog->container.iWindowPositionY);
		int iOldDistance = pDialog->iDistanceToDock;
		pDialog->iDistanceToDock = (pDialog->container.bDirectionUp ? pDialog->iAimedY - pDialog->container.iWindowPositionY - (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin) : pDialog->container.iWindowPositionY - pDialog->iAimedY);
		if (! pDialog->container.bDirectionUp)  // iPositionY est encore la position du coin haut gauche de la bulle et non de la fenetre.
			pDialog->container.iWindowPositionY = pDialog->iAimedY;
		
		if (pDialog->iDistanceToDock != iOldDistance && pDialog->pTipWidget != NULL)
		{
			//g_print ("  On change la taille de la pointe a : %d pixels ( -> %d)\n", pDialog->iDistanceToDock, pDialog->iMessageHeight + pDialog->iInteractiveHeight +pDialog->iButtonsHeight + pDialog->iDistanceToDock);
			gtk_widget_set (pDialog->pTipWidget, "height-request", MAX (0, pDialog->iDistanceToDock + pDialog->iBottomMargin), NULL);
		}
		if (pDialog->bRight)
		{
			if (pDialog->container.bDirectionUp)
				iGravity = GDK_GRAVITY_SOUTH_WEST;
			else
				iGravity = GDK_GRAVITY_NORTH_WEST;
		}
		else
		{
			if (pDialog->container.bDirectionUp)
				iGravity = GDK_GRAVITY_SOUTH_EAST;
			else
				iGravity = GDK_GRAVITY_NORTH_EAST;
		}
	}
	else  // dialogue lie a aucun container => au milieu de l'ecran courant.
	{
		pDialog->container.bDirectionUp = TRUE;
		pDialog->container.iWindowPositionX = (g_pMainDock ? g_pMainDock->iScreenOffsetX : 0) + (g_desktopGeometry.iScreenWidth [CAIRO_DOCK_HORIZONTAL] - pDialog->container.iWidth) / 2;
		pDialog->container.iWindowPositionY = (g_pMainDock ? g_pMainDock->iScreenOffsetY : 0) + (g_desktopGeometry.iScreenHeight[CAIRO_DOCK_HORIZONTAL] - pDialog->container.iHeight) / 2;
		pDialog->container.iHeight = pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin;
		//g_print (" au milieu de l'ecran (%d;%d) %dx%d\n", pDialog->container.iWindowPositionX, pDialog->container.iWindowPositionY, pDialog->container.iWidth, pDialog->container.iHeight);
		pDialog->iDistanceToDock = 0;
		iGravity = GDK_GRAVITY_CENTER;
	}
	
	w = pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin;
	h = pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iDistanceToDock;
	
	pDialog->bPositionForced = FALSE;
	gtk_window_set_gravity (GTK_WINDOW (pDialog->container.pWidget), iGravity);
	//g_print (" => move to (%d;%d) %dx%d , %d\n", pDialog->container.iWindowPositionX, pDialog->container.iWindowPositionY, w, h, iGravity);
	gdk_window_move_resize (GDK_WINDOW (pDialog->container.pWidget->window),
		pDialog->container.iWindowPositionX,
		pDialog->container.iWindowPositionY,
		w,
		h);
}


void cairo_dock_replace_all_dialogs (void)
{
	//g_print ("%s ()\n", __func__);
	GSList *ic;
	CairoDialog *pDialog;
	CairoContainer *pContainer;
	Icon *pIcon;

	if (s_pDialogList == NULL)
		return ;

	for (ic = s_pDialogList; ic != NULL; ic = ic->next)
	{
		pDialog = ic->data;
		
		pIcon = pDialog->pIcon;
		if (pIcon != NULL && GTK_WIDGET_VISIBLE (pDialog->container.pWidget)) // on ne replace pas les dialogues en cours de destruction ou caches.
		{
			pContainer = cairo_dock_search_container_from_icon (pIcon);
			//if (CAIRO_DOCK_IS_DOCK (pContainer))
			{
				int iPositionX = pDialog->container.iWindowPositionX;
				int iPositionY = pDialog->container.iWindowPositionY;
				int iAimedX = pDialog->iAimedX;
				int iAimedY = pDialog->iAimedY;
				cairo_dock_place_dialog (pDialog, pContainer);
				
				if (iPositionX != pDialog->container.iWindowPositionX || iPositionY != pDialog->container.iWindowPositionY || iAimedX != pDialog->iAimedX || iAimedY != pDialog->iAimedY)
					gtk_widget_queue_draw (pDialog->container.pWidget);  // on redessine meme si la position n'a pas changee, car la pointe, elle, change.
			}
		}
	}
}


CairoDialog *cairo_dock_show_dialog_full (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	if (pIcon != NULL && cairo_dock_icon_is_being_removed (pIcon))  // icone en cours de suppression.
	{
		g_print ("dialog skipped for %s (%.2f)\n", pIcon->cName, pIcon->fInsertRemoveFactor);
		return NULL;
	}
	
	CairoDialogAttribute attr;
	memset (&attr, 0, sizeof (CairoDialogAttribute));
	attr.cText = (gchar *)cText;
	attr.cImageFilePath = (gchar *)cIconPath;
	attr.pInteractiveWidget = pInteractiveWidget;
	attr.pActionFunc = pActionFunc;
	attr.pUserData = data;
	attr.pFreeDataFunc = pFreeDataFunc;
	attr.iTimeLength = (int) fTimeLength;
	if (pActionFunc != NULL)
	{
		const gchar *cButtons[3] = {"ok", "cancel", NULL};
		attr.cButtonsImage = cButtons;
	}
	
	CairoDialog *pDialog = cairo_dock_build_dialog (&attr, pIcon, pContainer);
	return pDialog;
}



CairoDialog *cairo_dock_show_temporary_dialog_with_icon_printf (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, ...)
{
	g_return_val_if_fail (cText != NULL, NULL);
	va_list args;
	va_start (args, cIconPath);
	gchar *cFullText = g_strdup_vprintf (cText, args);
	CairoDialog *pDialog = cairo_dock_show_dialog_full (cFullText, pIcon, pContainer, fTimeLength, cIconPath, NULL, NULL, NULL, NULL);
	g_free (cFullText);
	va_end (args);
	return pDialog;
}

CairoDialog *cairo_dock_show_temporary_dialog_with_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath)
{
	g_return_val_if_fail (cText != NULL, NULL);
	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, fTimeLength, cIconPath, NULL, NULL, NULL, NULL);
}

CairoDialog *cairo_dock_show_temporary_dialog (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength)
{
	g_return_val_if_fail (cText != NULL, NULL);
	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, fTimeLength, NULL, NULL, NULL, NULL, NULL);
}

CairoDialog *cairo_dock_show_temporary_dialog_with_default_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength)
{
	g_return_val_if_fail (cText != NULL, NULL);
	
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);
	cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, "cairo-dock-animated.xpm");
	
	CairoDialogAttribute attr;
	memset (&attr, 0, sizeof (CairoDialogAttribute));
	attr.cText = cText;
	attr.cImageFilePath = cIconPath;
	attr.iNbFrames = 12;
	attr.iIconSize = 32;
	attr.iTimeLength = (int) fTimeLength;
	CairoDialog *pDialog = cairo_dock_build_dialog (&attr, pIcon, pContainer);
	
	g_free (cIconPath);
	return pDialog;
}



static inline GtkWidget *_cairo_dock_make_entry_for_dialog (const gchar *cTextForEntry)
{
	GtkWidget *pWidget = gtk_entry_new ();
	gtk_entry_set_has_frame (GTK_ENTRY (pWidget), FALSE);
	gtk_widget_set (pWidget, "width-request", CAIRO_DIALOG_MIN_ENTRY_WIDTH, NULL);
	if (cTextForEntry != NULL)
		gtk_entry_set_text (GTK_ENTRY (pWidget), cTextForEntry);
	return pWidget;
}
static inline GtkWidget *_cairo_dock_make_hscale_for_dialog (double fValueForHScale, double fMaxValueForHScale)
{
	GtkWidget *pWidget = gtk_hscale_new_with_range (0, fMaxValueForHScale, fMaxValueForHScale / 100.);
	gtk_scale_set_digits (GTK_SCALE (pWidget), 2);
	gtk_range_set_value (GTK_RANGE (pWidget), fValueForHScale);

	gtk_widget_set (pWidget, "width-request", CAIRO_DIALOG_MIN_SCALE_WIDTH, NULL);
	return pWidget;
}

CairoDialog *cairo_dock_show_dialog_with_question (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconPath, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, 0, cIconPath, NULL, pActionFunc, data, pFreeDataFunc);
}

CairoDialog *cairo_dock_show_dialog_with_entry (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconPath, const gchar *cTextForEntry, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (cTextForEntry, -1, -1);
	GtkWidget *pWidget = _cairo_dock_make_entry_for_dialog (cTextForEntry);

	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, 0, cIconPath, pWidget, pActionFunc, data, pFreeDataFunc);
}

CairoDialog *cairo_dock_show_dialog_with_value (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconPath, double fValue, double fMaxValue, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	fValue = MAX (0., fValue);
	fValue = MIN (fMaxValue, fValue);
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (NULL, fValue, 1.);
	GtkWidget *pWidget = _cairo_dock_make_hscale_for_dialog (fValue, fMaxValue);

	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, 0, cIconPath, pWidget, pActionFunc, data, pFreeDataFunc);
}



static void _cairo_dock_get_answer_from_dialog (int iClickedButton, GtkWidget *pInteractiveWidget, gpointer *data, CairoDialog *pDialog)
{
	cd_message ("%s (%d)", __func__, iClickedButton);
	int *iAnswerBuffer = data[0];
	GMainLoop *pBlockingLoop = data[1];
	GtkWidget *pWidgetCatcher = data[2];
	if (pInteractiveWidget != NULL)
		gtk_widget_reparent (pInteractiveWidget, pWidgetCatcher);  // j'ai rien trouve de mieux pour empecher que le 'pInteractiveWidget' ne soit pas detruit avec le dialogue apres l'appel de la callback (g_object_ref ne marche pas).

	*iAnswerBuffer = iClickedButton;

	if (g_main_loop_is_running (pBlockingLoop))
		g_main_loop_quit (pBlockingLoop);
}
static gboolean _cairo_dock_dialog_destroyed (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	g_print ("dialogue detruit, on sort de la boucle\n");
	gtk_window_set_modal (GTK_WINDOW (pWidget), FALSE);
	if (g_main_loop_is_running (pBlockingLoop))
		g_main_loop_quit (pBlockingLoop);
	return FALSE;
}
int cairo_dock_show_dialog_and_wait (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget)
{
	static GtkWidget *pWidgetCatcher = NULL;  // voir l'astuce plus haut.
	int iClickedButton = -3;
	GMainLoop *pBlockingLoop = g_main_loop_new (NULL, FALSE);
	if (pWidgetCatcher == NULL)
		pWidgetCatcher = gtk_hbox_new (0, FALSE);
	gpointer data[3] = {&iClickedButton, pBlockingLoop, pWidgetCatcher};  // inutile d'allouer 'data' puisqu'on va bloquer.

	CairoDialog *pDialog = cairo_dock_show_dialog_full (cText,
		pIcon,
		pContainer,
		0.,
		cIconPath,
		pInteractiveWidget,
		(CairoDockActionOnAnswerFunc)_cairo_dock_get_answer_from_dialog,
		(gpointer) data,
		(GFreeFunc) NULL);

	if (pDialog != NULL)
	{
		gtk_window_set_modal (GTK_WINDOW (pDialog->container.pWidget), TRUE);
		g_signal_connect (pDialog->container.pWidget,
			"delete-event",
			G_CALLBACK (_cairo_dock_dialog_destroyed),
			pBlockingLoop);
		
		if (myAccessibility.bPopUp && CAIRO_DOCK_IS_DOCK (pContainer))
		{
			cairo_dock_pop_up (CAIRO_DOCK (pContainer));
		}
		g_print ("debut de boucle bloquante ...\n");
		GDK_THREADS_LEAVE ();
		g_main_loop_run (pBlockingLoop);
		GDK_THREADS_ENTER ();
		g_print ("fin de boucle bloquante -> %d\n", iClickedButton);
		if (myAccessibility.bPopUp && CAIRO_DOCK_IS_DOCK (pContainer))
			cairo_dock_pop_down (CAIRO_DOCK (pContainer));
		if (CAIRO_DOCK_IS_DOCK (pContainer)/* && ! pDock->container.bInside*/)
		{
			cd_message ("on force a quitter");
			CairoDock *pDock = CAIRO_DOCK (pContainer);
			pDock->container.bInside = TRUE;
			///pDock->bAtBottom = FALSE;
			cairo_dock_emit_leave_signal (pDock);
			/*cairo_dock_on_leave_notify (pDock->container.pWidget,
				NULL,
				pDock);*/
		}
	}

	g_main_loop_unref (pBlockingLoop);

	return iClickedButton;
}

gchar *cairo_dock_show_demand_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, const gchar *cInitialAnswer)
{
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog ((cInitialAnswer != NULL ? cInitialAnswer : ""), -1, -1);
	GtkWidget *pWidget = _cairo_dock_make_entry_for_dialog (cInitialAnswer);
	const gchar *cIconPath = CAIRO_DOCK_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON;

	int iClickedButton = cairo_dock_show_dialog_and_wait (cMessage, pIcon, pContainer, 0, cIconPath, pWidget);

	gchar *cAnswer = (iClickedButton == 0 || iClickedButton == -1 ? g_strdup (gtk_entry_get_text (GTK_ENTRY (pWidget))) : NULL);
	cd_message ("cAnswer : %s", cAnswer);

	gtk_widget_destroy (pWidget);
	return cAnswer;
}

double cairo_dock_show_value_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, double fInitialValue, double fMaxValue)
{
	fInitialValue = MAX (0, fInitialValue);
	fInitialValue = MIN (fMaxValue, fInitialValue);
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (NULL, fInitialValue, fMaxValue);
	GtkWidget *pWidget = _cairo_dock_make_hscale_for_dialog (fInitialValue, fMaxValue);
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);

	int iClickedButton = cairo_dock_show_dialog_and_wait (cMessage, pIcon, pContainer, 0, cIconPath, pWidget);
	g_free (cIconPath);

	double fValue = (iClickedButton == 0 || iClickedButton == -1 ? gtk_range_get_value (GTK_RANGE (pWidget)) : -1);
	cd_message ("fValue : %.2f", fValue);

	gtk_widget_destroy (pWidget);
	return fValue;
}

int cairo_dock_ask_question_and_wait (const gchar *cQuestion, Icon *pIcon, CairoContainer *pContainer)
{
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);  // trouver une icone de question...
	int iClickedButton = cairo_dock_show_dialog_and_wait (cQuestion, pIcon, pContainer, 0, cIconPath, NULL);
	g_free (cIconPath);

	return (iClickedButton == 0 || iClickedButton == -1 ? GTK_RESPONSE_YES : GTK_RESPONSE_NO);
}



gboolean cairo_dock_icon_has_dialog (Icon *pIcon)
{
	GSList *ic;
	CairoDialog *pDialog;
	for (ic = s_pDialogList; ic != NULL; ic = ic->next)
	{
		pDialog = ic->data;
		if (pDialog->pIcon == pIcon)
			break ;
	}

	return (ic != NULL);
}

Icon *cairo_dock_get_dialogless_icon (void)
{
	if (g_pMainDock == NULL || g_pMainDock->icons == NULL)
		return NULL;

	Icon *pIcon = cairo_dock_get_first_icon_of_type (g_pMainDock->icons, CAIRO_DOCK_SEPARATOR12);
	if (pIcon != NULL && ! cairo_dock_icon_has_dialog (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
		return pIcon;
	
	pIcon = cairo_dock_get_first_icon_of_type (g_pMainDock->icons, CAIRO_DOCK_SEPARATOR23);
	if (pIcon != NULL && ! cairo_dock_icon_has_dialog (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
		return pIcon;
	
	pIcon = cairo_dock_get_pointed_icon (g_pMainDock->icons);
	if (pIcon != NULL && ! CAIRO_DOCK_IS_NORMAL_APPLI (pIcon) && ! CAIRO_DOCK_IS_APPLET (pIcon) && ! cairo_dock_icon_has_dialog (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
		return pIcon;

	GList *ic;
	for (ic = g_pMainDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (! cairo_dock_icon_has_dialog (pIcon) && ! CAIRO_DOCK_IS_NORMAL_APPLI (pIcon) && ! CAIRO_DOCK_IS_APPLET (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
			return pIcon;
	}
	
	pIcon = cairo_dock_get_first_icon (g_pMainDock->icons);
	return pIcon;
}

CairoDialog *cairo_dock_show_general_message (const gchar *cMessage, double fTimeLength)
{
	Icon *pIcon = cairo_dock_get_dialogless_icon ();
	return cairo_dock_show_temporary_dialog (cMessage, pIcon, CAIRO_CONTAINER (g_pMainDock), fTimeLength);
}

int cairo_dock_ask_general_question_and_wait (const gchar *cQuestion)
{
	Icon *pIcon = cairo_dock_get_dialogless_icon ();
	return cairo_dock_ask_question_and_wait (cQuestion, pIcon, CAIRO_CONTAINER (g_pMainDock));
}



void cairo_dock_hide_dialog (CairoDialog *pDialog)
{
	cd_debug ("%s ()", __func__);
	if (GTK_WIDGET_VISIBLE (pDialog->container.pWidget))
	{
		pDialog->bAllowMinimize = TRUE;
		gtk_widget_hide (pDialog->container.pWidget);
		pDialog->container.bInside = FALSE;
		cairo_dock_replace_all_dialogs ();
		if (pDialog->pIcon && pDialog->pIcon->cParentDockName != NULL)
			cairo_dock_dialog_window_destroyed ();
	}
}

void cairo_dock_unhide_dialog (CairoDialog *pDialog)
{
	g_print ("%s ()\n", __func__);
	if (! GTK_WIDGET_VISIBLE (pDialog->container.pWidget))
	{
		if (pDialog->pInteractiveWidget != NULL)
			gtk_widget_grab_focus (pDialog->pInteractiveWidget);
		Icon *pIcon = pDialog->pIcon;
		if (pIcon != NULL)
		{
			CairoContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
			cairo_dock_place_dialog (pDialog, pContainer);
			if (pIcon->cParentDockName != NULL)
				cairo_dock_dialog_window_created ();
		}
	}
	pDialog->bPositionForced = FALSE;
	gtk_window_present (GTK_WINDOW (pDialog->container.pWidget));
}

void cairo_dock_toggle_dialog_visibility (CairoDialog *pDialog)
{
	if (GTK_WIDGET_VISIBLE (pDialog->container.pWidget))
		cairo_dock_hide_dialog (pDialog);
	else
		cairo_dock_unhide_dialog (pDialog);
}
