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

#include "gldi-config.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-container.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-config.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-dialog-factory.h"
#define _MANAGER_DEF_
#include "cairo-dock-dialog-manager.h"

// public (manager, config, data)
CairoDialogsParam myDialogsParam;
CairoDialogsManager myDialogsMgr;

// dependancies
extern CairoDock *g_pMainDock;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern gboolean g_bUseOpenGL;

// private
static GSList *s_pDialogList = NULL;
static cairo_surface_t *s_pButtonOkSurface = NULL;
static cairo_surface_t *s_pButtonCancelSurface = NULL;
static GLuint s_iButtonOkTexture = 0;
static GLuint s_iButtonCancelTexture = 0;
static guint s_iSidReplaceDialogs = 0;

static gboolean _cairo_dock_render_dialog_notification (gpointer data, CairoDialog *pDialog, cairo_t *pCairoContext);
static void cairo_dock_place_dialog (CairoDialog *pDialog, CairoContainer *pContainer);


static inline cairo_surface_t *_cairo_dock_load_button_icon (const gchar *cButtonImage, const gchar *cDefaultButtonImage)
{
	cairo_surface_t *pButtonSurface = NULL;
	if (cButtonImage != NULL)
		pButtonSurface = cairo_dock_create_surface_from_image_simple (cButtonImage,
			myDialogsParam.iDialogButtonWidth,
			myDialogsParam.iDialogButtonHeight);

	if (pButtonSurface == NULL)
	{
		pButtonSurface = cairo_dock_create_surface_from_image_simple (cDefaultButtonImage,
			myDialogsParam.iDialogButtonWidth,
			myDialogsParam.iDialogButtonHeight);
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
void cairo_dock_load_dialog_buttons (gchar *cButtonOkImage, gchar *cButtonCancelImage)
{
	//g_print ("%s (%s ; %s)\n", __func__, cButtonOkImage, cButtonCancelImage);
	if (s_pButtonOkSurface != NULL)
		cairo_surface_destroy (s_pButtonOkSurface);
	s_pButtonOkSurface = _cairo_dock_load_button_icon (cButtonOkImage, GLDI_SHARE_DATA_DIR"/cairo-dock-ok.svg");

	if (s_pButtonCancelSurface != NULL)
		cairo_surface_destroy (s_pButtonCancelSurface);
	s_pButtonCancelSurface = _cairo_dock_load_button_icon (cButtonCancelImage, GLDI_SHARE_DATA_DIR"/cairo-dock-cancel.svg");
	
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
	Icon *pIcon = pDialog->pIcon;
	int iMouseX, iMouseY;
	gdk_window_get_pointer (pDialog->container.pWidget->window, &iMouseX, &iMouseY, NULL);
	
	if (iMouseX > 0 && iMouseX < pDialog->container.iWidth && iMouseY > 0 && iMouseY < pDialog->container.iHeight && pDialog->pInteractiveWidget != NULL)  // en fait on est dedans (peut arriver si le dialogue a un widget a l'interieur).
	{
		if (pIcon != NULL)
		{
			CairoContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
			///if (!pContainer || !pContainer->bInside)  // peut arriver dans le cas d'un dock cache possedant un dialogue. Initialement les 2 se chevauchent, il faut considerer qu'on est hors du dialogue afin de pouvoir le replacer.
			{
				//g_print ("en fait on est dedans\n");
				return FALSE;
			}
			//else
			//	g_print ("leave dialog\n");
		}
	}
	
	//g_print ("leave\n");
	//cd_debug ("outside (%d;%d / %dx%d)", iMouseX, iMouseY, pDialog->container.iWidth, pDialog->container.iHeight);
	pDialog->container.bInside = FALSE;
	
	if (pIcon != NULL /*&& (pEvent->state & GDK_BUTTON1_MASK) == 0*/)
	{
		pDialog->container.iMouseX = pEvent->x_root;
		pDialog->container.iMouseY = pEvent->y_root;
		CairoContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
		//cairo_dock_place_dialog (pDialog, pContainer);
		cairo_dock_set_dialog_orientation (pDialog, pContainer);
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
	int iMinButtonX = .5 * (pDialog->container.iWidth - (n - 1) * CAIRO_DIALOG_BUTTON_GAP - n * myDialogsParam.iDialogButtonWidth);
	for (i = 0; i < pDialog->iNbButtons; i++)
	{
		iButtonX = iMinButtonX + i * (CAIRO_DIALOG_BUTTON_GAP + myDialogsParam.iDialogButtonWidth);
		if (pButton->x >= iButtonX && pButton->x <= iButtonX + myDialogsParam.iDialogButtonWidth && pButton->y >= iButtonY && pButton->y <= iButtonY + myDialogsParam.iDialogButtonHeight)
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
	g_print ("press button on dialog\n");
	if (pButton->time > pDialog->iButtonPressTime)  // it's not a click on the interactive widget that has been passed to the dialog.
	{
		if (pButton->type == GDK_BUTTON_PRESS)
		{
			if (pDialog->pButtons == NULL/** && pDialog->pInteractiveWidget == NULL*/)  // not a dialog that can be closed by a button => we close it here
			{
				if (pDialog->bHideOnClick)
					cairo_dock_hide_dialog (pDialog);
				else
					cairo_dock_dialog_unreference (pDialog);
			}
			else if (pDialog->pButtons != NULL && pButton->button == 1)  // left click on a button.
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
			if (pDialog->pButtons != NULL && pButton->button == 1)
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
	cd_debug ("key pressed on dialog: %d / %d", pKey->state, GDK_CONTROL_MASK | GDK_MOD1_MASK);
	
	if (pKey->type == GDK_KEY_PRESS && ((pKey->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK)) == 0) && pDialog->action_on_answer != NULL)
	{
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
		int iMinButtonX = .5 * (pDialog->container.iWidth - (n - 1) * CAIRO_DIALOG_BUTTON_GAP - n * myDialogsParam.iDialogButtonWidth);
		for (i = 0; i < pDialog->iNbButtons; i++)
		{
			iButtonX = iMinButtonX + i * (CAIRO_DIALOG_BUTTON_GAP + myDialogsParam.iDialogButtonWidth);
			glBindTexture (GL_TEXTURE_2D, pDialog->pButtons[i].iTexture);
			_cairo_dock_apply_current_texture_at_size_with_offset (myDialogsParam.iDialogButtonWidth,
				myDialogsParam.iDialogButtonWidth,
				iButtonX + pDialog->pButtons[i].iOffset + myDialogsParam.iDialogButtonWidth/2,
				pDialog->container.iHeight - (iButtonY + pDialog->pButtons[i].iOffset + myDialogsParam.iDialogButtonWidth/2));			}
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
		x = MAX (0, x - pDialog->iIconOffsetX);
		y = (pDialog->container.bDirectionUp ? pDialog->iTopMargin : pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight)) - pDialog->iIconOffsetX;
		y = MAX (0, y - pDialog->iIconOffsetY);
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
		x = pDialog->iLeftMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN - (pDialog->iIconSize != 0 ? pDialog->iIconOffsetX : 0);
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
		iButtonY = (pDialog->container.bDirectionUp ? pDialog->iTopMargin + pDialog->iMessageHeight + pDialog->iInteractiveHeight + CAIRO_DIALOG_VGAP : pDialog->container.iHeight - pDialog->iTopMargin - pDialog->iButtonsHeight + CAIRO_DIALOG_VGAP);
		int iMinButtonX = .5 * ((pDialog->container.iWidth - pDialog->iLeftMargin - pDialog->iRightMargin) - (n - 1) * CAIRO_DIALOG_BUTTON_GAP - n * myDialogsParam.iDialogButtonWidth) + pDialog->iLeftMargin;
		cairo_surface_t *pButtonSurface;
		for (i = 0; i < pDialog->iNbButtons; i++)
		{
			iButtonX = iMinButtonX + i * (CAIRO_DIALOG_BUTTON_GAP + myDialogsParam.iDialogButtonWidth);
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
			Icon *pIcon = pDialog->pIcon;
			if (pIcon != NULL)
			{
				CairoContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
				//g_print ("leave from container %x\n", pContainer);
				if (pContainer)
					cairo_dock_emit_leave_signal (pContainer);
			}
			cairo_dock_free_dialog (pDialog);
			s_pDialogList = g_slist_remove (s_pDialogList, pDialog);
			cairo_dock_trigger_replace_all_dialogs ();
			
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
		if (pDialog->action_on_answer != NULL)
			pDialog->action_on_answer (-2, pDialog->pInteractiveWidget, pDialog->pUserData, pDialog);  // -2 <=> Escape.
		pDialog->iSidTimer = 0;
		cairo_dock_dialog_unreference (pDialog);  // on pourrait eventuellement faire un fondu avant.
	}
	return FALSE;
}

CairoDialog *cairo_dock_build_dialog (CairoDialogAttribute *pAttribute, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_val_if_fail (pAttribute != NULL, NULL);
	
	if (!pAttribute->bForceAbove)
	{
		Icon *pActiveAppli = cairo_dock_get_current_active_icon ();
		if (pActiveAppli && pActiveAppli->bIsFullScreen && cairo_dock_appli_is_on_current_desktop (pActiveAppli))
		{
			cd_debug ("skip dialog since current fullscreen window would mask it");
			return NULL;
		}
	}
	cd_debug ("%s (%s, %s, %x, %x, %x (%x;%x))", __func__, pAttribute->cText, pAttribute->cImageFilePath, pAttribute->pInteractiveWidget, pAttribute->pActionFunc, pAttribute->pTextDescription, pIcon, pContainer);
	
	//\________________ On cree un nouveau dialogue.
	CairoDialog *pDialog = cairo_dock_new_dialog (pAttribute, pIcon, pContainer);
	if (pIcon && pIcon->pSubDock)  // un sous-dock par-dessus le dialogue est tres genant.
		cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pIcon->pSubDock));
	s_pDialogList = g_slist_prepend (s_pDialogList, pDialog);
	if (pDialog->iNbButtons != 0 && (s_pButtonOkSurface == NULL || s_pButtonCancelSurface == NULL))
		cairo_dock_load_dialog_buttons (myDialogsParam.cButtonOkImage, myDialogsParam.cButtonCancelImage);
	
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
	
	return pDialog;
}



static void _cairo_dock_dialog_find_optimal_placement (CairoDialog *pDialog)
{
	//g_print ("%s (Ybulle:%d; %dx%d)\n", __func__, pDialog->iComputedPositionY, pDialog->iComputedWidth, pDialog->iComputedHeight);
	
	int iZoneXLeft, iZoneXRight, iY, iWidth, iHeight;  // our available zone.
	iWidth = pDialog->iComputedWidth;
	iHeight = pDialog->iComputedHeight;
	iY = pDialog->iComputedPositionY;
	iZoneXLeft = MAX (pDialog->iAimedX - iWidth, 0);
	iZoneXRight = MIN (pDialog->iAimedX + iWidth, g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL]);
	CairoDialog *pDialogOnOurWay;
	int iLimitXLeft = iZoneXLeft;
	int iLimitXRight = iZoneXRight;  // left and right limits due to other dialogs
	int iMinYLimit = (pDialog->container.bDirectionUp ? -1e4 : 1e4);  // closest y limit due to other dialogs.
	int iBottomY, iTopY, iXleft, iXright;  // box of the other dialog.
	gboolean bDialogOnOurWay = FALSE;
	GSList *ic;
	for (ic = s_pDialogList; ic != NULL; ic = ic->next)
	{
		pDialogOnOurWay = ic->data;
		if (pDialogOnOurWay == NULL)
			continue ;
		if (pDialogOnOurWay != pDialog)  // check if this dialog can overlap us.
		{
			if (GTK_WIDGET_VISIBLE (pDialogOnOurWay->container.pWidget) && pDialogOnOurWay->pIcon != NULL)
			{
				iTopY = pDialogOnOurWay->container.iWindowPositionY;
				iBottomY = pDialogOnOurWay->container.iWindowPositionY + pDialogOnOurWay->container.iHeight;
				iXleft = pDialogOnOurWay->container.iWindowPositionX;
				iXright = pDialogOnOurWay->container.iWindowPositionX + pDialogOnOurWay->container.iWidth;
				if ( ((iTopY < iY && iBottomY > iY) || (iTopY >= iY && iTopY < iY + iHeight))
					&& ((iXleft < iZoneXLeft && iXright > iZoneXLeft) || iXleft >= iZoneXLeft && iXleft < iZoneXRight) )  // intersection of the 2 rectangles.
				{
					g_print ("  dialogue genant: \n %d - %d, %d - %d", iTopY, iBottomY, iXleft, iXright);
					if (pDialogOnOurWay->iAimedX < pDialog->iAimedX)  // this dialog is on our left.
						iLimitXLeft = MAX (iLimitXLeft, pDialogOnOurWay->container.iWindowPositionX + pDialogOnOurWay->container.iWidth);
					else
						iLimitXRight = MIN (iLimitXRight, pDialogOnOurWay->container.iWindowPositionX);
					iMinYLimit = (pDialog->container.bDirectionUp ? MAX (iMinYLimit, iTopY) : MIN (iMinYLimit, iBottomY));
					g_print ("  iMinYLimit <- %d\n", iMinYLimit);
					bDialogOnOurWay = TRUE;
				}
			}
		}
	}
	//g_print (" -> [%d ; %d], %d, %d\n", iLimitXLeft, iLimitXRight, iWidth, iMinYLimit);
	if (iLimitXRight - iLimitXLeft >= MIN (g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL], iWidth) || !bDialogOnOurWay)  // there is enough room to place the dialog.
	{
		if (pDialog->bRight)
			pDialog->iComputedPositionX = MAX (0, MIN (pDialog->iAimedX - pDialog->fAlign * iWidth, iLimitXRight - iWidth));
		else
			pDialog->iComputedPositionX = MAX (pDialog->iAimedX - (1. - pDialog->fAlign) * iWidth, iLimitXLeft);
		if (pDialog->container.bDirectionUp && pDialog->iComputedPositionY < 0)
			pDialog->iComputedPositionY = 0;
		else if (!pDialog->container.bDirectionUp && pDialog->iComputedPositionY + iHeight > g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL])
			pDialog->iComputedPositionY = g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] - iHeight;
	}
	else  // not enough room, try again above the closest dialog that was disturbing.
	{
		if (pDialog->container.bDirectionUp)
			pDialog->iComputedPositionY = iMinYLimit - iHeight;
		else
			pDialog->iComputedPositionY = iMinYLimit;
		g_print (" => re-try with y=%d\n", pDialog->iComputedPositionY);
		_cairo_dock_dialog_find_optimal_placement (pDialog);
	}
}

static void cairo_dock_place_dialog (CairoDialog *pDialog, CairoContainer *pContainer)
{
	//g_print ("%s (%x;%d, %s)\n", __func__, pDialog->pIcon, pContainer, pDialog->pIcon?pDialog->pIcon->cParentDockName:"none");
	if (pDialog->container.bInside)
		return;
	
	int w, h;
	GdkGravity iGravity;
	if (pContainer != NULL && pDialog->pIcon != NULL)
	{
		cairo_dock_set_dialog_orientation (pDialog, pContainer);
		
		if (pDialog->bTopBottomDialog)
		{
			pDialog->iComputedPositionY = (pDialog->container.bDirectionUp ? pDialog->iAimedY - pDialog->iComputedHeight : pDialog->iAimedY);
			_cairo_dock_dialog_find_optimal_placement (pDialog);
		}
		else  // dialogue lie a un dock vertical, on ne cherche pas a optimiser le placement.
		{
			int tmp = pDialog->iAimedX;
			pDialog->iAimedX = pDialog->iAimedY;
			pDialog->iAimedY = tmp;
			
			pDialog->iComputedPositionX = (pDialog->bRight ? MAX (0, pDialog->iAimedX - pDialog->container.iWidth) : pDialog->iAimedX);
			pDialog->iComputedPositionY = (pDialog->container.bDirectionUp ? MAX (0, pDialog->iAimedY - pDialog->iComputedHeight) : pDialog->iAimedY + pDialog->iMinBottomGap);  // on place la bulle (et non pas la fenetre) sans faire d'optimisation.
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
		pDialog->iComputedPositionX = (g_pMainDock ? g_pMainDock->iScreenOffsetX : 0) + (g_desktopGeometry.iScreenWidth [CAIRO_DOCK_HORIZONTAL] - pDialog->container.iWidth) / 2;
		pDialog->iComputedPositionY = (g_pMainDock ? g_pMainDock->iScreenOffsetY : 0) + (g_desktopGeometry.iScreenHeight[CAIRO_DOCK_HORIZONTAL] - pDialog->container.iHeight) / 2;
		
		iGravity = GDK_GRAVITY_CENTER;
	}
	
	pDialog->bPositionForced = FALSE;
	///gtk_window_set_gravity (GTK_WINDOW (pDialog->container.pWidget), iGravity);
	//g_print (" => move to (%d;%d) %dx%d , %d\n", pDialog->iComputedPositionX, pDialog->iComputedPositionY, pDialog->iComputedWidth, pDialog->iComputedHeight, iGravity);
	gtk_window_move (GTK_WINDOW (pDialog->container.pWidget),
		pDialog->iComputedPositionX,
		pDialog->iComputedPositionY);
}

void cairo_dock_refresh_all_dialogs (gboolean bReplace)
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
		if (pIcon != NULL && GTK_WIDGET_VISIBLE (pDialog->container.pWidget))  // on ne replace pas les dialogues en cours de destruction ou caches.
		{
			pContainer = cairo_dock_search_container_from_icon (pIcon);
			//if (CAIRO_DOCK_IS_DOCK (pContainer))
			{
				int iAimedX = pDialog->iAimedX;
				int iAimedY = pDialog->iAimedY;
				if (bReplace)
					cairo_dock_place_dialog (pDialog, pContainer);
				else
					cairo_dock_set_dialog_orientation (pDialog, pContainer);
				
				if (iAimedX != pDialog->iAimedX || iAimedY != pDialog->iAimedY)
					gtk_widget_queue_draw (pDialog->container.pWidget);  // on redessine si la pointe change de position.
			}
		}
	}
}

static gboolean _replace_all_dialogs (gpointer data)
{
	cairo_dock_replace_all_dialogs ();
	s_iSidReplaceDialogs = 0;
	return FALSE;
}
void cairo_dock_trigger_replace_all_dialogs (void)
{
	if (s_iSidReplaceDialogs == 0)
	{
		s_iSidReplaceDialogs = g_idle_add ((GSourceFunc)_replace_all_dialogs, NULL);
	}
}



CairoDialog *cairo_dock_show_dialog_full (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	if (pIcon != NULL && cairo_dock_icon_is_being_removed (pIcon))  // icone en cours de suppression.
	{
		cd_debug ("dialog skipped for %s (%.2f)\n", pIcon->cName, pIcon->fInsertRemoveFactor);
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
	
	gchar *cIconPath = g_strdup_printf ("%s/%s", GLDI_SHARE_DATA_DIR, CAIRO_DOCK_ICON);
	cIconPath = g_strdup_printf ("%s/%s", GLDI_SHARE_DATA_DIR, "cairo-dock-animated.xpm");
	
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
	
	cairo_dock_steal_interactive_widget_from_dialog (pDialog);  // le dialogue disparaitra apres cette fonction, mais le widget interactif doit rester.
	
	*iAnswerBuffer = iClickedButton;

	if (g_main_loop_is_running (pBlockingLoop))
		g_main_loop_quit (pBlockingLoop);
}
static gboolean _cairo_dock_dialog_destroyed (GtkWidget *pWidget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	cd_debug ("dialogue detruit, on sort de la boucle\n");
	gtk_window_set_modal (GTK_WINDOW (pWidget), FALSE);  /// utile ?...
	if (g_main_loop_is_running (pBlockingLoop))
		g_main_loop_quit (pBlockingLoop);
	return FALSE;
}
int cairo_dock_show_dialog_and_wait (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget)
{
	int iClickedButton = -3;
	GMainLoop *pBlockingLoop = g_main_loop_new (NULL, FALSE);
	gpointer data[2] = {&iClickedButton, pBlockingLoop};  // inutile d'allouer 'data' puisqu'on va bloquer.

	CairoDialog *pDialog = cairo_dock_show_dialog_full (cText,
		pIcon,
		pContainer,
		0.,
		cIconPath,
		pInteractiveWidget,
		(CairoDockActionOnAnswerFunc)_cairo_dock_get_answer_from_dialog,
		(gpointer) data,
		(GFreeFunc) NULL);
	pDialog->fAppearanceCounter = 1.;
	
	if (pDialog != NULL)
	{
		gtk_window_set_modal (GTK_WINDOW (pDialog->container.pWidget), TRUE);
		g_signal_connect (pDialog->container.pWidget,
			"delete-event",
			G_CALLBACK (_cairo_dock_dialog_destroyed),
			pBlockingLoop);
		
		/**if (myDocksParam.bPopUp && CAIRO_DOCK_IS_DOCK (pContainer))
		{
			cairo_dock_pop_up (CAIRO_DOCK (pContainer));
		}*/
		cd_debug ("debut de boucle bloquante ...\n");
		GDK_THREADS_LEAVE ();
		g_main_loop_run (pBlockingLoop);
		GDK_THREADS_ENTER ();
		cd_debug ("fin de boucle bloquante -> %d\n", iClickedButton);
		/*if (myDocksParam.bPopUp && CAIRO_DOCK_IS_DOCK (pContainer))
			cairo_dock_pop_down (CAIRO_DOCK (pContainer));*/
		if (CAIRO_DOCK_IS_DOCK (pContainer))
		{
			//g_print ("on force a quitter apres le dialogue bloquant\n");
			cairo_dock_emit_leave_signal (pContainer);
		}
	}

	g_main_loop_unref (pBlockingLoop);

	return iClickedButton;
}

gchar *cairo_dock_show_demand_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, const gchar *cInitialAnswer)
{
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog ((cInitialAnswer != NULL ? cInitialAnswer : ""), -1, -1);
	GtkWidget *pWidget = _cairo_dock_make_entry_for_dialog (cInitialAnswer);
	const gchar *cIconPath = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON;

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
	gchar *cIconPath = g_strdup_printf ("%s/%s", GLDI_SHARE_DATA_DIR, CAIRO_DOCK_ICON);

	int iClickedButton = cairo_dock_show_dialog_and_wait (cMessage, pIcon, pContainer, 0, cIconPath, pWidget);
	g_free (cIconPath);

	double fValue = (iClickedButton == 0 || iClickedButton == -1 ? gtk_range_get_value (GTK_RANGE (pWidget)) : -1);
	cd_message ("fValue : %.2f", fValue);

	gtk_widget_destroy (pWidget);
	return fValue;
}

int cairo_dock_ask_question_and_wait (const gchar *cQuestion, Icon *pIcon, CairoContainer *pContainer)
{
	gchar *cIconPath = g_strdup_printf ("%s/%s", GLDI_SHARE_DATA_DIR, CAIRO_DOCK_ICON);  // trouver une icone de question...
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

Icon *cairo_dock_get_dialogless_icon_full (CairoDock *pDock)
{
	if (pDock == NULL)
		pDock = g_pMainDock;
	if (pDock == NULL || pDock->icons == NULL)
		return NULL;

	Icon *pIcon = cairo_dock_get_first_icon_of_type (pDock->icons, CAIRO_DOCK_SEPARATOR12);
	if (pIcon != NULL && ! cairo_dock_icon_has_dialog (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
		return pIcon;
	
	pIcon = cairo_dock_get_first_icon_of_type (pDock->icons, CAIRO_DOCK_SEPARATOR23);
	if (pIcon != NULL && ! cairo_dock_icon_has_dialog (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
		return pIcon;
	
	pIcon = cairo_dock_get_pointed_icon (pDock->icons);
	if (pIcon != NULL && ! CAIRO_DOCK_IS_NORMAL_APPLI (pIcon) && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon) && ! cairo_dock_icon_has_dialog (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
		return pIcon;

	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (! cairo_dock_icon_has_dialog (pIcon) && ! CAIRO_DOCK_IS_NORMAL_APPLI (pIcon) && ! CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon) && pIcon->cParentDockName != NULL && ! cairo_dock_icon_is_being_removed (pIcon))
			return pIcon;
	}
	
	pIcon = cairo_dock_get_first_icon (pDock->icons);
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
		
		cairo_dock_trigger_replace_all_dialogs ();
		
		Icon *pIcon = pDialog->pIcon;
		if (pIcon != NULL)
		{
			CairoContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
			//g_print ("leave from container %x\n", pContainer);
			if (pContainer)
				cairo_dock_emit_leave_signal (pContainer);
		}
	}
}

void cairo_dock_unhide_dialog (CairoDialog *pDialog)
{
	cd_debug ("%s ()", __func__);
	if (! GTK_WIDGET_VISIBLE (pDialog->container.pWidget))
	{
		if (pDialog->pInteractiveWidget != NULL)
			gtk_widget_grab_focus (pDialog->pInteractiveWidget);
		Icon *pIcon = pDialog->pIcon;
		if (pIcon != NULL)
		{
			CairoContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
			cairo_dock_place_dialog (pDialog, pContainer);
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


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoDialogsParam *pDialogs)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pDialogs->cButtonOkImage = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "button_ok image", &bFlushConfFileNeeded, NULL, NULL, NULL);
	pDialogs->cButtonCancelImage = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "button_cancel image", &bFlushConfFileNeeded, NULL, NULL, NULL);

	cairo_dock_get_size_key_value_helper (pKeyFile, "Dialogs", "button ", bFlushConfFileNeeded, pDialogs->iDialogButtonWidth, pDialogs->iDialogButtonHeight);

	double couleur_bulle[4] = {1.0, 1.0, 1.0, 0.7};
	cairo_dock_get_double_list_key_value (pKeyFile, "Dialogs", "background color", &bFlushConfFileNeeded, pDialogs->fDialogColor, 4, couleur_bulle, NULL, NULL);
	pDialogs->iDialogIconSize = MAX (16, cairo_dock_get_integer_key_value (pKeyFile, "Dialogs", "icon size", &bFlushConfFileNeeded, 48, NULL, NULL));
	
	gboolean bCustomFont = cairo_dock_get_boolean_key_value (pKeyFile, "Dialogs", "custom", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	gchar *cFontDescription = (bCustomFont ? cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "message police", &bFlushConfFileNeeded, NULL, "Icons", NULL) : NULL);
	if (cFontDescription == NULL)
		cFontDescription = cairo_dock_get_default_system_font ();
	
	PangoFontDescription *fd = pango_font_description_from_string (cFontDescription);
	pDialogs->dialogTextDescription.cFont = g_strdup (pango_font_description_get_family (fd));
	pDialogs->dialogTextDescription.iSize = pango_font_description_get_size (fd);
	if (!pango_font_description_get_size_is_absolute (fd))
		pDialogs->dialogTextDescription.iSize /= PANGO_SCALE;
	if (pDialogs->dialogTextDescription.iSize == 0)
		pDialogs->dialogTextDescription.iSize = 14;
	if (!bCustomFont)
		pDialogs->dialogTextDescription.iSize *= 1.33;  // c'est pas beau, mais ca evite de casser tous les themes.
	pDialogs->dialogTextDescription.iWeight = pango_font_description_get_weight (fd);
	pDialogs->dialogTextDescription.iStyle = pango_font_description_get_style (fd);
	pDialogs->dialogTextDescription.fMaxRelativeWidth = .5;  // on limite a la moitie de l'ecran.
	
	if (g_key_file_has_key (pKeyFile, "Dialogs", "message size", NULL))  // anciens parametres.
	{
		pDialogs->dialogTextDescription.iSize = g_key_file_get_integer (pKeyFile, "Dialogs", "message size", NULL);
		int iLabelWeight = g_key_file_get_integer (pKeyFile, "Dialogs", "message weight", NULL);
		pDialogs->dialogTextDescription.iWeight = cairo_dock_get_pango_weight_from_1_9 (iLabelWeight);
		gboolean bLabelStyleItalic = g_key_file_get_boolean (pKeyFile, "Dialogs", "message italic", NULL);
		if (bLabelStyleItalic)
			pDialogs->dialogTextDescription.iStyle = PANGO_STYLE_ITALIC;
		else
			pDialogs->dialogTextDescription.iStyle = PANGO_STYLE_NORMAL;
		
		pango_font_description_set_size (fd, pDialogs->dialogTextDescription.iSize * PANGO_SCALE);
		pango_font_description_set_weight (fd, pDialogs->dialogTextDescription.iWeight);
		pango_font_description_set_style (fd, pDialogs->dialogTextDescription.iStyle);
		
		g_free (cFontDescription);
		cFontDescription = pango_font_description_to_string (fd);
		g_key_file_set_string (pKeyFile, "Dialogs", "message police", cFontDescription);
		bFlushConfFileNeeded = TRUE;
	}
	pango_font_description_free (fd);
	g_free (cFontDescription);
	pDialogs->dialogTextDescription.bOutlined = cairo_dock_get_boolean_key_value (pKeyFile, "Dialogs", "outlined", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	pDialogs->dialogTextDescription.iMargin = 0;
	
	double couleur_dtext[3] = {0., 0., 0.};
	cairo_dock_get_double_list_key_value (pKeyFile, "Dialogs", "text color", &bFlushConfFileNeeded, pDialogs->dialogTextDescription.fColorStart, 3, couleur_dtext, NULL, NULL);
	memcpy (&pDialogs->dialogTextDescription.fColorStop, &pDialogs->dialogTextDescription.fColorStart, 3*sizeof (double));
	
	pDialogs->cDecoratorName = cairo_dock_get_string_key_value (pKeyFile, "Dialogs", "decorator", &bFlushConfFileNeeded, "comics", NULL, NULL);

	return bFlushConfFileNeeded;
}


  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (CairoDialogsParam *pDialogs)
{
	g_free (pDialogs->cButtonOkImage);
	g_free (pDialogs->cButtonCancelImage);
	g_free (pDialogs->dialogTextDescription.cFont);
	g_free (pDialogs->cDecoratorName);
}


  //////////////
 /// RELOAD ///
//////////////

static void reload (CairoDialogsParam *pPrevDialogs, CairoDialogsParam *pDialogs)
{
	if (cairo_dock_strings_differ (pPrevDialogs->cButtonOkImage, pDialogs->cButtonOkImage) ||
		cairo_dock_strings_differ (pPrevDialogs->cButtonCancelImage, pDialogs->cButtonCancelImage) ||
		pPrevDialogs->iDialogIconSize != pDialogs->iDialogIconSize)
	{
		cairo_dock_unload_dialog_buttons ();
		cairo_dock_load_dialog_buttons (pDialogs->cButtonOkImage, pDialogs->cButtonCancelImage);
	}
}


  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	cairo_dock_unload_dialog_buttons ();
}


  ////////////
 /// INIT ///
////////////

static void init (void)
{
	cairo_dock_register_notification_on_object (&myDialogsMgr,
		NOTIFICATION_RENDER_DIALOG,
		(CairoDockNotificationFunc) _cairo_dock_render_dialog_notification,
		CAIRO_DOCK_RUN_AFTER, NULL);
}


  ///////////////
 /// MANAGER ///
///////////////

void gldi_register_dialogs_manager (void)
{
	// Manager
	memset (&myDialogsMgr, 0, sizeof (CairoDialogsManager));
	myDialogsMgr.mgr.cModuleName 	= "Dialogs";
	myDialogsMgr.mgr.init 		= init;
	myDialogsMgr.mgr.load 		= NULL;  // data are loaded the first time a dialog is created, to avoid create them for nothing.
	myDialogsMgr.mgr.unload 		= unload;
	myDialogsMgr.mgr.reload 		= (GldiManagerReloadFunc)reload;
	myDialogsMgr.mgr.get_config 	= (GldiManagerGetConfigFunc)get_config;
	myDialogsMgr.mgr.reset_config = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myDialogsParam, 0, sizeof (CairoDialogsParam));
	myDialogsMgr.mgr.pConfig = (GldiManagerConfigPtr*)&myDialogsParam;
	myDialogsMgr.mgr.iSizeOfConfig = sizeof (CairoDialogsParam);
	// data
	myDialogsMgr.mgr.iSizeOfData = 0;
	myDialogsMgr.mgr.pData = (GldiManagerDataPtr*)NULL;
	// signals
	cairo_dock_install_notifications_on_object (&myDialogsMgr, NB_NOTIFICATIONS_DIALOG);
	// register
	gldi_register_manager (GLDI_MANAGER(&myDialogsMgr));
}
