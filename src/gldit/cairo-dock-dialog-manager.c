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
#include "cairo-dock-desklet-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-dialog-factory.h"
#define _MANAGER_DEF_
#include "cairo-dock-dialog-manager.h"

// public (manager, config, data)
CairoDialogsParam myDialogsParam;
GldiManager myDialogsMgr;
GldiObjectManager myDialogObjectMgr;

// dependancies
extern CairoDock *g_pMainDock;
extern gboolean g_bUseOpenGL;
extern CairoDockHidingEffect *g_pHidingBackend;  // cairo_dock_is_hidden

// private
static GSList *s_pDialogList = NULL;
static cairo_surface_t *s_pButtonOkSurface = NULL;
static cairo_surface_t *s_pButtonCancelSurface = NULL;
static guint s_iSidReplaceDialogs = 0;

static void _set_dialog_orientation (CairoDialog *pDialog, GldiContainer *pContainer);
static void _place_dialog (CairoDialog *pDialog, GldiContainer *pContainer);


static inline cairo_surface_t *_cairo_dock_load_button_icon (const gchar *cButtonImage, const gchar *cDefaultButtonImage)
{
	cairo_surface_t *pButtonSurface = NULL;
	if (cButtonImage != NULL)
	{
		pButtonSurface = cairo_dock_create_surface_from_image_simple (cButtonImage,
			myDialogsParam.iDialogButtonWidth,
			myDialogsParam.iDialogButtonHeight);
	}
	if (pButtonSurface == NULL)
	{
		pButtonSurface = cairo_dock_create_surface_from_image_simple (cDefaultButtonImage,
			myDialogsParam.iDialogButtonWidth,
			myDialogsParam.iDialogButtonHeight);
	}
	return pButtonSurface;
}

static void _load_dialog_buttons (gchar *cButtonOkImage, gchar *cButtonCancelImage)
{
	if (s_pButtonOkSurface != NULL)
		cairo_surface_destroy (s_pButtonOkSurface);
	s_pButtonOkSurface = _cairo_dock_load_button_icon (cButtonOkImage, GLDI_SHARE_DATA_DIR"/icons/cairo-dock-ok.svg");

	if (s_pButtonCancelSurface != NULL)
		cairo_surface_destroy (s_pButtonCancelSurface);
	s_pButtonCancelSurface = _cairo_dock_load_button_icon (cButtonCancelImage, GLDI_SHARE_DATA_DIR"/icons/cairo-dock-cancel.svg");
}

static void _unload_dialog_buttons (void)
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
}


static gboolean on_enter_dialog (G_GNUC_UNUSED GtkWidget* pWidget,
	G_GNUC_UNUSED GdkEventCrossing* pEvent,
	CairoDialog *pDialog)
{
	//cd_debug ("inside");
	pDialog->container.bInside = TRUE;
	return FALSE;
}

static gboolean on_leave_dialog (G_GNUC_UNUSED GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDialog *pDialog)
{
	Icon *pIcon = pDialog->pIcon;
	int iMouseX, iMouseY;
	if (pEvent != NULL)
	{
		iMouseX = pEvent->x_root;
		iMouseY = pEvent->y_root;
	}
	else
	{
		gldi_container_update_mouse_position (CAIRO_CONTAINER (pDialog));
		iMouseX = pDialog->container.iMouseX;
		iMouseY = pDialog->container.iMouseY;
	}
	if (iMouseX > 0 && iMouseX < pDialog->container.iWidth && iMouseY > 0 && iMouseY < pDialog->container.iHeight && pDialog->pInteractiveWidget != NULL)  // en fait on est dedans (peut arriver si le dialogue a un widget a l'interieur).
	{
		if (pIcon != NULL)
		{
			return FALSE;
			/*GldiContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
			///if (!pContainer || !pContainer->bInside)  // peut arriver dans le cas d'un dock cache possedant un dialogue. Initialement les 2 se chevauchent, il faut considerer qu'on est hors du dialogue afin de pouvoir le replacer.
			{
				//g_print ("en fait on est dedans\n");
				return FALSE;
			}
			//else
			//	//g_print ("leave dialog\n");*/
		}
	}
	
	//g_print ("leave\n");
	//cd_debug ("outside (%d;%d / %dx%d)", iMouseX, iMouseY, pDialog->container.iWidth, pDialog->container.iHeight);
	pDialog->container.bInside = FALSE;
	
	if (pIcon != NULL /*&& (pEvent->state & GDK_BUTTON1_MASK) == 0*/)
	{
		pDialog->container.iMouseX = pEvent->x_root;
		pDialog->container.iMouseY = pEvent->y_root;
		GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
		//_place_dialog (pDialog, pContainer);
		_set_dialog_orientation (pDialog, pContainer);
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
	int iMinButtonX = .5 * ((pDialog->container.iWidth - pDialog->iLeftMargin - pDialog->iRightMargin) - (n - 1) * CAIRO_DIALOG_BUTTON_GAP - n * myDialogsParam.iDialogButtonWidth) + pDialog->iLeftMargin;
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

static inline void _answer (CairoDialog *pDialog, int iButton)
{
	pDialog->bInAnswer = TRUE;
	pDialog->action_on_answer (iButton, pDialog->pInteractiveWidget, pDialog->pUserData, pDialog);
	pDialog->bInAnswer = FALSE;
}

static gboolean on_button_press_dialog (G_GNUC_UNUSED GtkWidget* pWidget,
	GdkEventButton* pButton,
	CairoDialog *pDialog)
{
	// g_print ("press button on dialog (%d > %d)\n", pButton->time, pDialog->iButtonPressTime);
	if (pButton->button == 1 && pButton->time > pDialog->iButtonPressTime)  // left-click, and not a click on the interactive widget that has been passed to the dialog.
	{
		// the interactive widget may have holes (for instance, a gtk-calendar); ignore them, otherwise it's really easy to close the dialog unexpectedly.
		if (pDialog->pInteractiveWidget)
		{
			GtkAllocation allocation;
			gtk_widget_get_allocation (pDialog->pInteractiveWidget, &allocation);
			if (pButton->x >= allocation.x && pButton->x <= allocation.x + allocation.width
			&& pButton->y >= allocation.y && pButton->y <= allocation.y + allocation.height)  // the click is inside the widget.
				return FALSE;
		}
		if (pButton->type == GDK_BUTTON_PRESS)
		{
			if (pDialog->pButtons == NULL)  // not a dialog that can be closed by a button => we close it here
			{
				if (pDialog->bHideOnClick)
					gldi_dialog_hide (pDialog);
				else
					gldi_object_unref (GLDI_OBJECT(pDialog));
			}
			else if (pButton->button == 1)  // left click on a button.
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
			if (pDialog->pButtons != NULL && pButton->button == 1)  // release left click with buttons present
			{
				int iButton = _cairo_dock_find_clicked_button_in_dialog (pButton, pDialog);
				cd_debug ("clic on button %d", iButton);
				if (iButton >= 0 && iButton < pDialog->iNbButtons && pDialog->pButtons[iButton].iOffset != 0)
				{
					pDialog->pButtons[iButton].iOffset = 0;
					_answer (pDialog, iButton);
					gtk_widget_queue_draw (pDialog->container.pWidget);  // in case the unref below wouldn't destroy it
					gldi_object_unref (GLDI_OBJECT(pDialog));  // and then destroy the dialog (it might not be destroyed if the ation callback took a ref on it).
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

static gboolean on_key_press_dialog (G_GNUC_UNUSED GtkWidget *pWidget,
	GdkEventKey *pKey,
	CairoDialog *pDialog)
{
	cd_debug ("key pressed on dialog: %d / %d", pKey->state, GDK_CONTROL_MASK | GDK_MOD1_MASK);
	
	if (pKey->type == GDK_KEY_PRESS && ((pKey->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK)) == 0) && pDialog->action_on_answer != NULL)
	{
		switch (pKey->keyval)
		{
			case GLDI_KEY(Return) :
			case GLDI_KEY(KP_Enter) :
				_answer (pDialog, CAIRO_DIALOG_ENTER_KEY);
				gldi_object_unref (GLDI_OBJECT(pDialog));
			break ;
			case GLDI_KEY(Escape) :
				_answer (pDialog, CAIRO_DIALOG_ESCAPE_KEY);
				gldi_object_unref (GLDI_OBJECT(pDialog));
			break ;
		}
	}
	return FALSE;
}

static gboolean _cairo_dock_dialog_auto_delete (CairoDialog *pDialog)
{
	if (pDialog != NULL)
	{
		if (pDialog->action_on_answer != NULL)
			pDialog->action_on_answer (CAIRO_DIALOG_ESCAPE_KEY, pDialog->pInteractiveWidget, pDialog->pUserData, pDialog);
		pDialog->iSidTimer = 0;
		gldi_object_unref (GLDI_OBJECT(pDialog));  // on pourrait eventuellement faire un fondu avant.
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
		_cairo_dock_apply_current_texture_portion_at_size_with_offset (0, 0.,
			1., 1.,
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
		_cairo_dock_apply_current_texture_at_size_with_offset (pDialog->iTextWidth, pDialog->iTextHeight,
			x + pDialog->iTextWidth/2, pDialog->container.iHeight - y - pDialog->iTextHeight/2);
	}
	
	if (pDialog->pButtons != NULL)
	{
		int iButtonX, iButtonY;
		int i, n = pDialog->iNbButtons;
		iButtonY = (pDialog->container.bDirectionUp ? pDialog->iTopMargin + pDialog->iMessageHeight + pDialog->iInteractiveHeight + CAIRO_DIALOG_VGAP : pDialog->container.iHeight - pDialog->iTopMargin - pDialog->iButtonsHeight - CAIRO_DIALOG_VGAP);
		int iMinButtonX = .5 * ((pDialog->container.iWidth - pDialog->iLeftMargin - pDialog->iRightMargin) - (n - 1) * CAIRO_DIALOG_BUTTON_GAP - n * myDialogsParam.iDialogButtonWidth) + pDialog->iLeftMargin;
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
		cairo_set_source_surface (pCairoContext,
			pDialog->pIconBuffer,
			x,
			y);
		_paint_inside_dialog(pCairoContext, fAlpha);
	}
	
	if (pDialog->pTextBuffer != NULL)
	{
		x = pDialog->iLeftMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN - (pDialog->iIconSize != 0 ? pDialog->iIconOffsetX : 0);
		y = (pDialog->container.bDirectionUp ? pDialog->iTopMargin : pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight));
		if (pDialog->iTextHeight < pDialog->iMessageHeight)  // on centre le texte.
			y += (pDialog->iMessageHeight - pDialog->iTextHeight) / 2;
		cairo_set_source_surface (pCairoContext,
			pDialog->pTextBuffer,
			x,
			y);
		_paint_inside_dialog(pCairoContext, fAlpha);
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

static gboolean _cairo_dock_render_dialog_notification (G_GNUC_UNUSED gpointer data, CairoDialog *pDialog, cairo_t *pCairoContext)
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
	return GLDI_NOTIFICATION_LET_PASS;
}


static gboolean _remove_dialog_on_icon (CairoDialog *pDialog, Icon *icon)
{
	if (pDialog->pIcon == icon && ! pDialog->bInAnswer)  // if inside the answer, don't unref, since the dialog will be destroyed after the answer (for instance, can happen with the confirmation dialog of the destruction of an icon)
		gldi_object_unref (GLDI_OBJECT(pDialog));
	return FALSE;
}
void gldi_dialogs_remove_on_icon (Icon *icon)  // gldi_icon_remove_dialog ?...
{
	g_return_if_fail (icon != NULL);
	gldi_dialogs_foreach ((GCompareFunc)_remove_dialog_on_icon, icon);
}


static void _cairo_dock_dialog_find_optimal_placement (CairoDialog *pDialog)
{
	//g_print ("%s (Ybulle:%d; %dx%d)\n", __func__, pDialog->iComputedPositionY, pDialog->iComputedWidth, pDialog->iComputedHeight);
	
	int iZoneXLeft, iZoneXRight, iY, iWidth, iHeight;  // our available zone.
	iWidth = pDialog->iComputedWidth;
	iHeight = pDialog->iComputedHeight;
	iY = pDialog->iComputedPositionY;
	iZoneXLeft = MAX (pDialog->iAimedX - iWidth, 0);
	iZoneXRight = MIN (pDialog->iAimedX + iWidth, gldi_desktop_get_width());
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
			if (pDialogOnOurWay->container.pWidget && gldi_container_is_visible (CAIRO_CONTAINER (pDialogOnOurWay)) && pDialogOnOurWay->pIcon != NULL)
			{
				iTopY = pDialogOnOurWay->container.iWindowPositionY;
				iBottomY = pDialogOnOurWay->container.iWindowPositionY + pDialogOnOurWay->container.iHeight;
				iXleft = pDialogOnOurWay->container.iWindowPositionX;
				iXright = pDialogOnOurWay->container.iWindowPositionX + pDialogOnOurWay->container.iWidth;
				if ( ((iTopY < iY && iBottomY > iY) || (iTopY >= iY && iTopY < iY + iHeight))
					&& ((iXleft < iZoneXLeft && iXright > iZoneXLeft) || (iXleft >= iZoneXLeft && iXleft < iZoneXRight)) )  // intersection of the 2 rectangles.
				{
					cd_debug ("  dialogue genant:  %d - %d, %d - %d", iTopY, iBottomY, iXleft, iXright);
					if (pDialogOnOurWay->iAimedX < pDialog->iAimedX)  // this dialog is on our left.
						iLimitXLeft = MAX (iLimitXLeft, pDialogOnOurWay->container.iWindowPositionX + pDialogOnOurWay->container.iWidth);
					else
						iLimitXRight = MIN (iLimitXRight, pDialogOnOurWay->container.iWindowPositionX);
					iMinYLimit = (pDialog->container.bDirectionUp ? MAX (iMinYLimit, iTopY) : MIN (iMinYLimit, iBottomY));
					cd_debug ("  iMinYLimit <- %d", iMinYLimit);
					bDialogOnOurWay = TRUE;
				}
			}
		}
	}
	//g_print (" -> [%d ; %d], %d, %d\n", iLimitXLeft, iLimitXRight, iWidth, iMinYLimit);
	if (iLimitXRight - iLimitXLeft >= MIN (gldi_desktop_get_width(), iWidth) || !bDialogOnOurWay)  // there is enough room to place the dialog.
	{
		if (pDialog->bRight)
			pDialog->iComputedPositionX = MAX (0, MIN (pDialog->iAimedX - pDialog->fAlign * iWidth, iLimitXRight - iWidth));
		else
			pDialog->iComputedPositionX = MIN (gldi_desktop_get_width() - iWidth, MAX (pDialog->iAimedX - (1. - pDialog->fAlign) * iWidth, iLimitXLeft));
		if (pDialog->container.bDirectionUp && pDialog->iComputedPositionY < 0)
			pDialog->iComputedPositionY = 0;
		else if (!pDialog->container.bDirectionUp && pDialog->iComputedPositionY + iHeight > gldi_desktop_get_height())
			pDialog->iComputedPositionY = gldi_desktop_get_height() - iHeight;
		//g_print (" --> %d\n", pDialog->iComputedPositionX);
	}
	else  // not enough room, try again above the closest dialog that was disturbing.
	{
		if (pDialog->container.bDirectionUp)
			pDialog->iComputedPositionY = iMinYLimit - iHeight;
		else
			pDialog->iComputedPositionY = iMinYLimit;
		cd_debug (" => re-try with y=%d", pDialog->iComputedPositionY);
		_cairo_dock_dialog_find_optimal_placement (pDialog);
	}
}

static void _cairo_dock_dialog_calculate_aimed_point (Icon *pIcon, GldiContainer *pContainer, int *iX, int *iY, gboolean *bRight, gboolean *bIsHorizontal, gboolean *bDirectionUp, double fAlign)
{
	g_return_if_fail (/*pIcon != NULL && */pContainer != NULL);
	//g_print ("%s (%.2f, %.2f)\n", __func__, pIcon?pIcon->fXAtRest:0, pIcon?pIcon->fDrawX:0);
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		if (pDock->iRefCount > 0 && ! gldi_container_is_visible (pContainer))  // sous-dock invisible.
		{
			CairoDock *pParentDock = NULL;
			Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
			_cairo_dock_dialog_calculate_aimed_point (pPointingIcon, CAIRO_CONTAINER (pParentDock), iX, iY, bRight, bIsHorizontal, bDirectionUp, fAlign);
		}
		else  // dock principal ou sous-dock visible.
		{
			*bIsHorizontal = (pContainer->bIsHorizontal == CAIRO_DOCK_HORIZONTAL);
			if (! *bIsHorizontal)
			{
				int *tmp = iX;
				iX = iY;
				iY = tmp;
			}
			int dy;
			if (pDock->iInputState == CAIRO_DOCK_INPUT_ACTIVE)
				dy = pContainer->iHeight - pDock->iActiveHeight;
			else if (cairo_dock_is_hidden (pDock))
				dy = pContainer->iHeight-1;  // on laisse 1 pixels pour pouvoir sortir du dialogue avant de toucher le bord de l'ecran, et ainsi le faire se replacer, lorsqu'on fait apparaitre un dock en auto-hide.
			else
				dy = pContainer->iHeight - pDock->iMinDockHeight;
			if (pContainer->bIsHorizontal)
			{
				*bRight = (pIcon ? pIcon->fXAtRest < pDock->fFlatDockWidth / 2 : TRUE);
				*bDirectionUp = pContainer->bDirectionUp;
				
				if (*bDirectionUp)
					*iY = pContainer->iWindowPositionY + dy;
				else
					*iY = pContainer->iWindowPositionY + pContainer->iHeight - dy;
			}
			else
			{
				*bRight = (pContainer->iWindowPositionY > gldi_desktop_get_width() / 2);  // we don't know if the container is set on a given screen or not, so take the X screen.
				*bDirectionUp = (pIcon ? pIcon->fXAtRest > pDock->fFlatDockWidth / 2 : TRUE);
				*iY = (pContainer->bDirectionUp ?
					pContainer->iWindowPositionY + dy :
					pContainer->iWindowPositionY + pContainer->iHeight - dy);
			}
			
			if (cairo_dock_is_hidden (pDock))
			{
				*iX = pContainer->iWindowPositionX +
					pDock->iMaxDockWidth/2
					- pDock->fFlatDockWidth/2
					+ pIcon->fXAtRest + pIcon->fWidth/2;
					///(pIcon ? (pIcon->fXAtRest + pIcon->fWidth/2) / pDock->fFlatDockWidth * pDock->iMaxDockWidth : 0);
			}
			else
			{
				*iX = pContainer->iWindowPositionX +
					(pIcon ? pIcon->fDrawX + pIcon->fWidth * pIcon->fScale/2 : 0);
			}
		}
	}
	else if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		*bDirectionUp = (pContainer->iWindowPositionY > gldi_desktop_get_height() / 2);
		///*bIsHorizontal = (pContainer->iWindowPositionX > 50 && pContainer->iWindowPositionX + pContainer->iHeight < gldi_desktop_get_width() - 50);
		*bIsHorizontal = TRUE;
		
		if (*bIsHorizontal)
		{
			*bRight = (pContainer->iWindowPositionX + pContainer->iWidth/2 < gldi_desktop_get_width() / 2);
			*iX = pContainer->iWindowPositionX + pContainer->iWidth * (*bRight ? .7 : .3);
			*iY = (*bDirectionUp ? pContainer->iWindowPositionY : pContainer->iWindowPositionY + pContainer->iHeight);
		}
		else
		{
			*bRight = (pContainer->iWindowPositionX < gldi_desktop_get_width() / 2);
			*iY = pContainer->iWindowPositionX + pContainer->iWidth * (*bRight ? 1 : 0);
			*iX =pContainer->iWindowPositionY + pContainer->iHeight / 2;
		}
	}
}
static void _set_dialog_orientation (CairoDialog *pDialog, GldiContainer *pContainer)
{
	if (pContainer != NULL/* && pDialog->pIcon != NULL*/)
	{
		_cairo_dock_dialog_calculate_aimed_point (pDialog->pIcon, pContainer, &pDialog->iAimedX, &pDialog->iAimedY, &pDialog->bRight, &pDialog->bTopBottomDialog, &pDialog->container.bDirectionUp, pDialog->fAlign);
		//g_print ("%s (%d,%d %d %d %d)\n", __func__, pDialog->iAimedX, pDialog->iAimedY, pDialog->bRight, pDialog->bTopBottomDialog, pDialog->container.bDirectionUp);
	}
	else
	{
		pDialog->container.bDirectionUp = TRUE;
	}
}

static void _place_dialog (CairoDialog *pDialog, GldiContainer *pContainer)
{
	//g_print ("%s (%x;%d, %s)\n", __func__, pDialog->pIcon, pContainer, pDialog->pIcon?pDialog->pIcon->cParentDockName:"none");
	if (pDialog->container.bInside && ! (pDialog->pInteractiveWidget || pDialog->action_on_answer))  // in the case of a modal dialog, the dialog takes the dock's events, including the "enter-event" one. So we are inside the dialog as soon as we enter the dock, and consequently, the dialog is not replaced when the dock unhides itself.
		return;
	
	// GdkGravity iGravity;
	if (pContainer != NULL/* && pDialog->pIcon != NULL*/)
	{
		_set_dialog_orientation (pDialog, pContainer);
		
		if (pDialog->bTopBottomDialog)
		{
			pDialog->iComputedPositionY = (pDialog->container.bDirectionUp ? pDialog->iAimedY - pDialog->iComputedHeight : pDialog->iAimedY);
			_cairo_dock_dialog_find_optimal_placement (pDialog);
		}
		else  // dialogue lie a un dock vertical, on ne cherche pas a optimiser le placement.
		{
			/**int tmp = pDialog->iAimedX;
			pDialog->iAimedX = pDialog->iAimedY;
			pDialog->iAimedY = tmp;*/
			
			pDialog->iComputedPositionX = (pDialog->bRight ? MAX (0, pDialog->iAimedX - pDialog->container.iWidth) : pDialog->iAimedX);
			pDialog->iComputedPositionY = (pDialog->container.bDirectionUp ? MAX (0, pDialog->iAimedY - pDialog->iComputedHeight) : pDialog->iAimedY + pDialog->iMinBottomGap);  // on place la bulle (et non pas la fenetre) sans faire d'optimisation.
		}
		
		/*if (pDialog->bRight)
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
		}*/
	}
	else  // dialogue lie a aucun container => au milieu de l'ecran courant.
	{
		pDialog->container.bDirectionUp = TRUE;
		pDialog->iComputedPositionX = (gldi_desktop_get_width() - pDialog->container.iWidth) / 2;  // we don't know if the container is set on a given screen or not, so take the X screen.
		pDialog->iComputedPositionY = (gldi_desktop_get_height() - pDialog->container.iHeight) / 2;
		
		// iGravity = GDK_GRAVITY_CENTER;
	}
	
	pDialog->bPositionForced = FALSE;
	///gtk_window_set_gravity (GTK_WINDOW (pDialog->container.pWidget), iGravity);
	//g_print (" => move to (%d;%d) %dx%d\n", pDialog->iComputedPositionX, pDialog->iComputedPositionY, pDialog->iComputedWidth, pDialog->iComputedHeight);
	gtk_window_move (GTK_WINDOW (pDialog->container.pWidget),
		pDialog->iComputedPositionX,
		pDialog->iComputedPositionY);
}

void _refresh_all_dialogs (gboolean bReplace)
{
	//g_print ("%s ()\n", __func__);
	GSList *ic;
	CairoDialog *pDialog;
	GldiContainer *pContainer;
	Icon *pIcon;

	if (s_pDialogList == NULL)
		return ;

	for (ic = s_pDialogList; ic != NULL; ic = ic->next)
	{
		pDialog = ic->data;
		
		pIcon = pDialog->pIcon;
		if (pIcon != NULL && gldi_container_is_visible (CAIRO_CONTAINER (pDialog)))  // on ne replace pas les dialogues en cours de destruction ou caches.
		{
			pContainer = cairo_dock_get_icon_container (pIcon);
			if (pContainer)
			{
				int iAimedX = pDialog->iAimedX;
				int iAimedY = pDialog->iAimedY;
				if (bReplace)
					_place_dialog (pDialog, pContainer);
				else
					_set_dialog_orientation (pDialog, pContainer);
				
				if (iAimedX != pDialog->iAimedX || iAimedY != pDialog->iAimedY)
					gtk_widget_queue_draw (pDialog->container.pWidget);  // on redessine si la pointe change de position.
			}
		}
	}
}

void gldi_dialogs_refresh_all (void)
{
	_refresh_all_dialogs (FALSE);
}

void gldi_dialogs_replace_all (void)
{
	_refresh_all_dialogs (TRUE);
}


static gboolean _replace_all_dialogs_idle (G_GNUC_UNUSED gpointer data)
{
	gldi_dialogs_replace_all ();
	s_iSidReplaceDialogs = 0;
	return FALSE;
}
static void _trigger_replace_all_dialogs (void)
{
	if (s_iSidReplaceDialogs == 0)
	{
		s_iSidReplaceDialogs = g_idle_add ((GSourceFunc)_replace_all_dialogs_idle, NULL);
	}
}


void gldi_dialog_hide (CairoDialog *pDialog)
{
	cd_debug ("%s ()", __func__);
	if (gldi_container_is_visible (CAIRO_CONTAINER (pDialog)))
	{
		pDialog->bAllowMinimize = TRUE;
		gtk_widget_hide (pDialog->container.pWidget);
		pDialog->container.bInside = FALSE;
		
		_trigger_replace_all_dialogs ();
		
		Icon *pIcon = pDialog->pIcon;
		if (pIcon != NULL)
		{
			GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
			//g_print ("leave from container %x\n", pContainer);
			if (pContainer)
			{
				if (CAIRO_DOCK_IS_DOCK (pContainer) && gtk_window_get_modal (GTK_WINDOW (pDialog->container.pWidget)))
				{
					CAIRO_DOCK (pContainer)->bHasModalWindow = FALSE;
					cairo_dock_emit_leave_signal (pContainer);
				}
			}
			if (pIcon->iHideLabel > 0)
			{
				pIcon->iHideLabel --;
				if (pIcon->iHideLabel == 0 && pContainer)
					gtk_widget_queue_draw (pContainer->pWidget);
			}
		}
	}
}

void gldi_dialog_unhide (CairoDialog *pDialog)
{
	cd_debug ("%s ()", __func__);
	if (! gldi_container_is_visible (CAIRO_CONTAINER (pDialog)))
	{
		if (pDialog->pInteractiveWidget != NULL)
			gtk_widget_grab_focus (pDialog->pInteractiveWidget);
		Icon *pIcon = pDialog->pIcon;
		if (pIcon != NULL)
		{
			GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
			_place_dialog (pDialog, pContainer);
			
			if (CAIRO_DOCK_IS_DOCK (pContainer) && cairo_dock_get_icon_max_scale (pIcon) < 1.01)  // same remark
			{
				if (pIcon->iHideLabel == 0 && pContainer)
					gtk_widget_queue_draw (pContainer->pWidget);
				pIcon->iHideLabel ++;
			}
			if (CAIRO_DOCK_IS_DOCK (pContainer) && gtk_window_get_modal (GTK_WINDOW (pDialog->container.pWidget)))
			{
				CAIRO_DOCK (pContainer)->bHasModalWindow = TRUE;
			}
		}
	}
	pDialog->bPositionForced = FALSE;
	gtk_window_present (GTK_WINDOW (pDialog->container.pWidget));
}

void gldi_dialog_toggle_visibility (CairoDialog *pDialog)
{
	if (gldi_container_is_visible (CAIRO_CONTAINER (pDialog)))
		gldi_dialog_hide (pDialog);
	else
		gldi_dialog_unhide (pDialog);
}

static gboolean on_icon_removed (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon, CairoDock *pDock)
{
	// if an icon is detached from the dock, and is destroyed (for instance, when removing an icon), the icon is detached and then freed;
	// its dialogs (for instance the confirmation dialog) are then destroyed, but since the icon is already detached, the dialog can't unset the 'bHasModalWindow' flag on its previous container.
	// therefore we do it here (plus it's logical to do that whenever an icon is detached. Note: we could handle the case of the icon being rattached to a container while having a modal dialog, to set the 'bHasModalWindow' flag, but I can't imagine a way it would happen).
	if (pIcon && pDock)
	{
		if (pDock->bHasModalWindow)  // check that the icon that is being detached was not carrying a modal dialog.
		{
			GSList *d;
			CairoDialog *pDialog;
			for (d = s_pDialogList; d != NULL; d = d->next)
			{
				pDialog = d->data;
				if (pDialog->pIcon == pIcon && gtk_window_get_modal (GTK_WINDOW (pDialog->container.pWidget)))
				{
					pDock->bHasModalWindow = FALSE;
					cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
					break;  // there can only be 1 modal window at a time.
				}
			}
		}
	}
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean on_icon_destroyed (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon)
{
	gldi_dialogs_remove_on_icon (pIcon);
	return GLDI_NOTIFICATION_LET_PASS;
}

CairoDialog *gldi_dialogs_foreach (GCompareFunc callback, gpointer data)
{
	CairoDialog *pDialog;
	GSList *d, *next_d;
	for (d = s_pDialogList; d != NULL; d = next_d)
	{
		next_d = d->next;  // in case the dialog is destroyed in the callback
		pDialog = d->data;
		if (callback (pDialog, data))
			return pDialog;
	}
	return NULL;
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
	pDialogs->dialogTextDescription.fMaxRelativeWidth = .5;  // limit to half of the screen (the dialog is not placed on a given screen, it can overlap 2 screens, so it's half of the mean screen width)
	
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
		_unload_dialog_buttons ();
		_load_dialog_buttons (pDialogs->cButtonOkImage, pDialogs->cButtonCancelImage);
	}
}

  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	_unload_dialog_buttons ();
}

  ////////////
 /// INIT ///
////////////

static void init (void)
{
	gldi_object_register_notification (&myDialogObjectMgr,
		NOTIFICATION_RENDER,
		(GldiNotificationFunc) _cairo_dock_render_dialog_notification,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDockObjectMgr,
		NOTIFICATION_REMOVE_ICON,
		(GldiNotificationFunc) on_icon_removed,
		GLDI_RUN_AFTER, NULL);
}

  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	CairoDialog *pDialog = (CairoDialog*)obj;
	CairoDialogAttr *pAttribute = (CairoDialogAttr*)attr;
	
	gldi_dialog_init_internals (pDialog, pAttribute);
	
	//\________________ Interactive dialogs are set modal, to be fixed.
	if ((pDialog->pInteractiveWidget || pDialog->pButtons || pAttribute->iTimeLength == 0) && ! pDialog->bNoInput)
	{
		gtk_window_set_modal (GTK_WINDOW (pDialog->container.pWidget), TRUE);  // Note: there is a bug in Ubuntu version of GTK: gtkscrolledwindow in dialog breaks his modality (http://www.gtkforums.com/viewtopic.php?f=3&t=55727, LP: https://bugs.launchpad.net/ubuntu/+source/overlay-scrollbar/+bug/903302).
		GldiContainer *pContainer = pAttribute->pContainer;
		if (CAIRO_DOCK_IS_DOCK (pContainer))
		{
			CAIRO_DOCK (pContainer)->bHasModalWindow = TRUE;
			cairo_dock_emit_enter_signal (pContainer);  // to prevent the dock from hiding. We want to see it while the dialog is visible (a leave event will be emited when it disappears).
		}
	}
	pDialog->bHideOnClick = pAttribute->bHideOnClick;
	
	//\________________ hide sub-dock or label that would overlap it
	Icon *pIcon = pAttribute->pIcon;
	GldiContainer *pContainer = pAttribute->pContainer;
	if (pIcon && pIcon->pSubDock)  // un sous-dock par-dessus le dialogue est tres genant.
		cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pIcon->pSubDock));
	if (pIcon && CAIRO_DOCK_IS_DOCK (pContainer) && cairo_dock_get_icon_max_scale (pIcon) < 1.01)  // view without zoom, the dialog is stuck to the icon, and therefore is under the label, so hide this one.
	{
		if (pIcon->iHideLabel == 0 && pContainer)
			gtk_widget_queue_draw (pContainer->pWidget);
		pIcon->iHideLabel ++;
	}
	
	//\________________ register the dialog
	s_pDialogList = g_slist_prepend (s_pDialogList, pDialog);
	
	//\________________ load the button images
	if (pDialog->iNbButtons != 0 && (s_pButtonOkSurface == NULL || s_pButtonCancelSurface == NULL))
		_load_dialog_buttons (myDialogsParam.cButtonOkImage, myDialogsParam.cButtonCancelImage);
	
	//\________________ On definit son orientation.
	_set_dialog_orientation (pDialog, pContainer);  // renseigne aussi bDirectionUp, bIsHorizontal, et iHeight.
	
	//\________________ on le place parmi les autres.
	_place_dialog (pDialog, pContainer);  // renseigne aussi bDirectionUp, bIsHorizontal, et iHeight.
	
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
		gldi_object_register_notification (pIcon,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) on_icon_destroyed,
			GLDI_RUN_AFTER, NULL);
	}
	
	//\________________ schedule the auto-destruction
	if (pAttribute->iTimeLength != 0)
		pDialog->iSidTimer = g_timeout_add (pAttribute->iTimeLength, (GSourceFunc) _cairo_dock_dialog_auto_delete, (gpointer) pDialog);
}

static void reset_object (GldiObject *obj)
{
	CairoDialog *pDialog = (CairoDialog*)obj;
	
	Icon *pIcon = pDialog->pIcon;
	if (pIcon != NULL)
	{
		// leave from the container if the dialog was modal (because it stole its events)
		GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
		if (pContainer)
		{
			if (CAIRO_DOCK_IS_DOCK (pContainer) && gtk_window_get_modal (GTK_WINDOW (pDialog->container.pWidget)))
			{
				CAIRO_DOCK (pContainer)->bHasModalWindow = FALSE;
				cairo_dock_emit_leave_signal (pContainer);
			}
		}
		
		// unhide the label if we hide it
		if (pIcon->iHideLabel > 0)
		{
			pIcon->iHideLabel --;
			if (pIcon->iHideLabel == 0 && pContainer)
				gtk_widget_queue_draw (pContainer->pWidget);
		}
	}
	
	// stop the timer
	if (pDialog->iSidTimer > 0)
	{
		g_source_remove (pDialog->iSidTimer);
	}
	
	// destroy private data
	if (pDialog->pTextBuffer != NULL)
		cairo_surface_destroy (pDialog->pTextBuffer);
	if (pDialog->pIconBuffer != NULL)
		cairo_surface_destroy (pDialog->pIconBuffer);
	if (pDialog->iIconTexture != 0)
		_cairo_dock_delete_texture (pDialog->iIconTexture);
	if (pDialog->iTextTexture != 0)
		_cairo_dock_delete_texture (pDialog->iTextTexture);
	
	if (pDialog->pButtons != NULL)
	{
		cairo_surface_t *pSurface;
		GLuint iTexture;
		int i;
		for (i = 0; i < pDialog->iNbButtons; i++)
		{
			pSurface = pDialog->pButtons[i].pSurface;
			if (pSurface != NULL)
				cairo_surface_destroy (pSurface);
			iTexture = pDialog->pButtons[i].iTexture;
			if (iTexture != 0)
				_cairo_dock_delete_texture (iTexture);
		}
		g_free (pDialog->pButtons);
	}
	
	if (pDialog->pUnmapTimer != NULL)
		g_timer_destroy (pDialog->pUnmapTimer);
	
	if (pDialog->pShapeBitmap != NULL)
		gldi_shape_destroy (pDialog->pShapeBitmap);
	
	// destroy user data
	if (pDialog->pUserData != NULL && pDialog->pFreeUserDataFunc != NULL)
		pDialog->pFreeUserDataFunc (pDialog->pUserData);
	
	// unregister the dialog
	s_pDialogList = g_slist_remove (s_pDialogList, pDialog);
	
	_trigger_replace_all_dialogs ();
}

void gldi_register_dialogs_manager (void)
{
	// Manager
	memset (&myDialogsMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myDialogsMgr), &myManagerObjectMgr, NULL);
	myDialogsMgr.cModuleName  = "Dialogs";
	// interface
	myDialogsMgr.init         = init;
	myDialogsMgr.load         = NULL;  // data are loaded the first time a dialog is created, to avoid create them for nothing.
	myDialogsMgr.unload       = unload;
	myDialogsMgr.reload       = (GldiManagerReloadFunc)reload;
	myDialogsMgr.get_config   = (GldiManagerGetConfigFunc)get_config;
	myDialogsMgr.reset_config = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myDialogsParam, 0, sizeof (CairoDialogsParam));
	myDialogsMgr.pConfig = (GldiManagerConfigPtr)&myDialogsParam;
	myDialogsMgr.iSizeOfConfig = sizeof (CairoDialogsParam);
	// data
	myDialogsMgr.iSizeOfData = 0;
	myDialogsMgr.pData = (GldiManagerDataPtr)NULL;
	
	// Object Manager
	memset (&myDialogObjectMgr, 0, sizeof (GldiObjectManager));
	myDialogObjectMgr.cName 	= "Dialog";
	myDialogObjectMgr.iObjectSize    = sizeof (CairoDialog);
	// interface
	myDialogObjectMgr.init_object    = init_object;
	myDialogObjectMgr.reset_object   = reset_object;
	// signals
	gldi_object_install_notifications (&myDialogObjectMgr, NB_NOTIFICATIONS_DIALOG);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myDialogObjectMgr), &myContainerObjectMgr);
}
