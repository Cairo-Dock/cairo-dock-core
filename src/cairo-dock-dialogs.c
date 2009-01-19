/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>

#include "cairo-dock-icons.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-hidden-dock.h"
#include "cairo-dock-internal-dialogs.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-dialogs.h"

extern CairoDock *g_pMainDock;
extern gint g_iXScreenWidth[2], g_iXScreenHeight[2];
extern gint g_iScreenWidth[2], g_iScreenHeight[2];
extern int g_iScreenOffsetX, g_iScreenOffsetY;
extern gboolean g_bSticky;
extern gboolean g_bKeepAbove;

static GSList *s_pDialogList = NULL;
static cairo_surface_t *s_pButtonOkSurface = NULL;
static cairo_surface_t *s_pButtonCancelSurface = NULL;

#define CAIRO_DIALOG_MIN_SIZE 20
#define CAIRO_DIALOG_TEXT_MARGIN 3
#define CAIRO_DIALOG_MIN_ENTRY_WIDTH 150
///#define CAIRO_DIALOG_MAX_ENTRY_WIDTH 300
#define CAIRO_DIALOG_MIN_SCALE_WIDTH 150
#define CAIRO_DIALOG_BUTTON_OFFSET 3
#define CAIRO_DIALOG_VGAP 4
#define CAIRO_DIALOG_BUTTON_GAP 16

gboolean on_enter_dialog (GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDialog *pDialog)
{
	cd_debug ("inside");
	pDialog->bInside = TRUE;
	return FALSE;
}

static gboolean on_leave_dialog (GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDialog *pDialog)
{
	//cd_debug ("%s (%d/%d)", __func__, pDialog->iButtonOkOffset, pDialog->iButtonCancelOffset);

	/*while (gtk_events_pending ())
		gtk_main_iteration ();
	cd_message ("fin d'attente, bInside : %d\n", pDialog->bInside);*/
	int iMouseX, iMouseY;
	gdk_window_get_pointer (pDialog->pWidget->window, &iMouseX, &iMouseY, NULL);
	if (iMouseX > 0 && iMouseX < pDialog->iWidth && iMouseY > 0 && iMouseY < pDialog->iHeight)
	{
		cd_debug ("en fait on est dedans");
		return FALSE;
	}

	//cd_debug ("outside (%d;%d / %dx%d)", iMouseX, iMouseY, pDialog->iWidth, pDialog->iHeight);
	pDialog->bInside = FALSE;
	Icon *pIcon = pDialog->pIcon;
	if (pIcon != NULL /*&& (pEvent->state & GDK_BUTTON1_MASK) == 0*/)
	{
		pDialog->iPositionX = pEvent->x_root;
		pDialog->iPositionY = pEvent->y_root;
		CairoContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
		cairo_dock_place_dialog (pDialog, pContainer);
		///gtk_widget_queue_draw (pDialog->pWidget);
	}

	return FALSE;
}

static int _cairo_dock_find_clicked_button_in_dialog (GdkEventButton* pButton, CairoDialog *pDialog)
{
	GtkRequisition requisition = {0, 0};
	if (pDialog->pInteractiveWidget != NULL)
		gtk_widget_size_request (pDialog->pInteractiveWidget, &requisition);

	int iButtonX = .5*pDialog->iWidth - myDialogs.iDialogButtonWidth - .5*CAIRO_DIALOG_BUTTON_GAP;
	int iButtonY = (pDialog->bDirectionUp ? 
		pDialog->iTopMargin + pDialog->iMessageHeight + pDialog->iInteractiveHeight + CAIRO_DIALOG_VGAP :
		pDialog->iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight - pDialog->iButtonsHeight - CAIRO_DIALOG_VGAP));
	
	//g_print ("clic (%d;%d) bouton Ok (%d;%d)\n", (int) pButton->x, (int) pButton->y, iButtonX, iButtonY);
	if (pButton->x >= iButtonX && pButton->x <= iButtonX + myDialogs.iDialogButtonWidth && pButton->y >= iButtonY && pButton->y <= iButtonY + myDialogs.iDialogButtonHeight)
	{
		return GTK_BUTTONS_OK;
	}
	else
	{
		iButtonX = .5*pDialog->iWidth + .5*CAIRO_DIALOG_BUTTON_GAP;
		//g_print ("clic (%d;%d) bouton Cancel (%d;%d)\n", (int) pButton->x, (int) pButton->y, iButtonX, iButtonY);
		if (pButton->x >= iButtonX && pButton->x <= iButtonX + myDialogs.iDialogButtonWidth && pButton->y >= iButtonY && pButton->y <= iButtonY + myDialogs.iDialogButtonHeight)
		{
			return GTK_BUTTONS_CANCEL;
		}
	}
	return GTK_BUTTONS_NONE;
}

static gboolean on_button_press_dialog (GtkWidget* pWidget,
	GdkEventButton* pButton,
	CairoDialog *pDialog)
{
	if (pButton->button == 1)  // clique gauche.
	{
		if (pButton->type == GDK_BUTTON_PRESS)
		{
			if (pDialog->iButtonsType == GTK_BUTTONS_NONE && pDialog->pInteractiveWidget == NULL)  // ce n'est pas un dialogue interactif.
			{
				cairo_dock_dialog_unreference (pDialog);
			}
			else if (pDialog->iButtonsType != GTK_BUTTONS_NONE)
			{
				int iButton = _cairo_dock_find_clicked_button_in_dialog (pButton, pDialog);
				if (iButton == GTK_BUTTONS_OK)
				{
					pDialog->iButtonOkOffset = CAIRO_DIALOG_BUTTON_OFFSET;
					gtk_widget_queue_draw (pDialog->pWidget);
				}
				else if (iButton == GTK_BUTTONS_CANCEL)
				{
					pDialog->iButtonCancelOffset = CAIRO_DIALOG_BUTTON_OFFSET;
					gtk_widget_queue_draw (pDialog->pWidget);
				}
			}
		}
		else if (pButton->type == GDK_BUTTON_RELEASE)
		{
			//g_print ("release\n");
			if (pDialog->iButtonsType != GTK_BUTTONS_NONE)
			{
				int iButton = _cairo_dock_find_clicked_button_in_dialog (pButton, pDialog);
				if (pDialog->iButtonOkOffset != 0)
				{
					pDialog->iButtonOkOffset = 0;
					gtk_widget_queue_draw (pDialog->pWidget);
					if (iButton == GTK_BUTTONS_OK)
					{
						pDialog->action_on_answer (GTK_RESPONSE_OK, pDialog->pInteractiveWidget, pDialog->pUserData);
						cairo_dock_dialog_unreference (pDialog);
					}
				}
				else if (pDialog->iButtonCancelOffset != 0)
				{
					pDialog->iButtonCancelOffset = 0;
					gtk_widget_queue_draw (pDialog->pWidget);
					if (iButton == GTK_BUTTONS_CANCEL)
					{
						pDialog->action_on_answer (GTK_RESPONSE_CANCEL, pDialog->pInteractiveWidget, pDialog->pUserData);
						cairo_dock_dialog_unreference (pDialog);
					}
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
	if (pDialog->iButtonsType == GTK_BUTTONS_NONE)
	{
		return FALSE;
	}
	if (pKey->type == GDK_KEY_PRESS)
	{
		GdkEventScroll dummyScroll;
		int iX, iY;
		switch (pKey->keyval)
		{
			case GDK_Return :
				pDialog->action_on_answer (GTK_RESPONSE_OK, pDialog->pInteractiveWidget, pDialog->pUserData);
				cairo_dock_dialog_unreference (pDialog);
			break ;
			case GDK_Escape :
				pDialog->action_on_answer (GTK_RESPONSE_CANCEL, pDialog->pInteractiveWidget, pDialog->pUserData);
				cairo_dock_dialog_unreference (pDialog);
			break ;
		}
	}
	return FALSE;
}


#define _drawn_text_width(pDialog) (pDialog->iMaxTextWidth != 0 && pDialog->iTextWidth > pDialog->iMaxTextWidth ? pDialog->iMaxTextWidth : pDialog->iTextWidth)
static gboolean on_expose_dialog (GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	CairoDialog *pDialog)
{
	//cd_message ("%s (%dx%d)", __func__, pDialog->iWidth, pDialog->iHeight);
	int x, y;
	cairo_t *pCairoContext;
	
	if (pExpose->area.x != 0 || pExpose->area.y != 0)
	{
		if (pDialog->pIconBuffer != NULL)
		{
			x = pDialog->iLeftMargin;
			y = (pDialog->bDirectionUp ? pDialog->iTopMargin : pDialog->iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight));
			if (pExpose->area.x >= x &&
				pExpose->area.x + pExpose->area.width <= x + pDialog->iIconSize && 
				pExpose->area.y >= y &&
				pExpose->area.y + pExpose->area.height <= y + pDialog->iIconSize)
			{
				//cd_debug ("icon redraw");
				pCairoContext = cairo_dock_create_drawing_context_on_area (CAIRO_CONTAINER (pDialog), &pExpose->area, myDialogs.fDialogColor);
				cairo_set_source_surface (pCairoContext,
					pDialog->pIconBuffer,
					x - (pDialog->iCurrentFrame * pDialog->iIconSize),
					y);
				cairo_paint (pCairoContext);
				cairo_destroy (pCairoContext);
				return FALSE;
			}
		}

		if (pDialog->pTextBuffer != NULL)
		{
			x = pDialog->iLeftMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN;
			y = (pDialog->bDirectionUp ? pDialog->iTopMargin : pDialog->iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight));
			if (pExpose->area.x >= x &&
				pExpose->area.x + pExpose->area.width <= x + _drawn_text_width (pDialog) && 
				pExpose->area.y >=  y&&
				pExpose->area.y + pExpose->area.height <= y + pDialog->iTextHeight)
			{
				//cd_debug ("text redraw");
				pCairoContext = cairo_dock_create_drawing_context_on_area (CAIRO_CONTAINER (pDialog), &pExpose->area, myDialogs.fDialogColor);
				if (pDialog->iTextHeight < pDialog->iMessageHeight)  // on centre le texte.
					y += (pDialog->iMessageHeight - pDialog->iTextHeight) / 2;
				cairo_set_source_surface (pCairoContext,
					pDialog->pTextBuffer,
					x - pDialog->iCurrentTextOffset,
					y);
				cairo_paint (pCairoContext);
				
				if (pDialog->iMaxTextWidth != 0 && pDialog->iTextWidth > pDialog->iMaxTextWidth)
				{
					cairo_set_source_surface (pCairoContext,
						pDialog->pTextBuffer,
						x - pDialog->iCurrentTextOffset + pDialog->iTextWidth + 10,
						y);
					cairo_paint (pCairoContext);
					cairo_restore (pCairoContext);
				}
				
				cairo_destroy (pCairoContext);
				return FALSE;
			}
		}
	}
	pCairoContext = cairo_dock_create_drawing_context (CAIRO_CONTAINER (pDialog));
	//cd_debug ("redraw");
	
	if (pDialog->pDecorator != NULL)
	{
		cairo_save (pCairoContext);
		pDialog->pDecorator->render (pCairoContext, pDialog);
		cairo_restore (pCairoContext);
	}

	if (pDialog->pIconBuffer != NULL)
	{
		x = pDialog->iLeftMargin;
		y = (pDialog->bDirectionUp ? pDialog->iTopMargin : pDialog->iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight));
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
			x,
			y);
		cairo_paint (pCairoContext);
		if (pDialog->iNbFrames > 1)
			cairo_restore (pCairoContext);
	}
	
	if (pDialog->pTextBuffer != NULL)
	{
		x = pDialog->iLeftMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN;
		y = (pDialog->bDirectionUp ? pDialog->iTopMargin : pDialog->iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight));
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
		cairo_paint (pCairoContext);
		
		if (pDialog->iMaxTextWidth != 0 && pDialog->iTextWidth > pDialog->iMaxTextWidth)
		{
			cairo_set_source_surface (pCairoContext,
				pDialog->pTextBuffer,
				x - pDialog->iCurrentTextOffset + pDialog->iTextWidth + 10,
				y);
			cairo_paint (pCairoContext);
			cairo_restore (pCairoContext);
		}
	}
	
	if (pDialog->iButtonsType != GTK_BUTTONS_NONE)
	{
		/*GtkRequisition requisition = {0, 0};
		if (pDialog->pInteractiveWidget != NULL)
			gtk_widget_size_request (pDialog->pInteractiveWidget, &requisition);
		cd_message (" pInteractiveWidget : %dx%d", requisition.width, requisition.height);*/

		int iButtonY = (pDialog->bDirectionUp ? pDialog->iTopMargin + pDialog->iMessageHeight + pDialog->iInteractiveHeight + CAIRO_DIALOG_VGAP : pDialog->iHeight - pDialog->iTopMargin - pDialog->iButtonsHeight - CAIRO_DIALOG_VGAP);  // requisition.height
		//g_print (" -> iButtonY : %d\n", iButtonY);

		cairo_set_source_surface (pCairoContext,
				s_pButtonOkSurface,
				.5*pDialog->iWidth - myDialogs.iDialogButtonWidth - .5*CAIRO_DIALOG_BUTTON_GAP + pDialog->iButtonOkOffset,
				iButtonY + pDialog->iButtonOkOffset);
		cairo_paint (pCairoContext);

		cairo_set_source_surface (pCairoContext,
				s_pButtonCancelSurface,
				.5*pDialog->iWidth + .5*CAIRO_DIALOG_BUTTON_GAP + pDialog->iButtonCancelOffset,
				iButtonY + pDialog->iButtonCancelOffset);
		cairo_paint (pCairoContext);
	}
	
	if (pDialog->pRenderer != NULL)
		pDialog->pRenderer->render (pCairoContext, pDialog);

	cairo_destroy (pCairoContext);
	return FALSE;
}


static gboolean on_configure_dialog (GtkWidget* pWidget,
	GdkEventConfigure* pEvent,
	CairoDialog *pDialog)
{
	g_print ("%s (%dx%d)\n", __func__, pEvent->width, pEvent->height);
	if (pEvent->width == CAIRO_DIALOG_MIN_SIZE && pEvent->height == CAIRO_DIALOG_MIN_SIZE)
		return FALSE;
	
	int iWidth = pDialog->iWidth, iHeight = pDialog->iHeight;
	//\____________ On recupere la taille du widget interactif qui a pu avoir change.
	if (pDialog->pInteractiveWidget != NULL)
	{
		GtkRequisition requisition;
		gtk_widget_size_request (pDialog->pInteractiveWidget, &requisition);
		pDialog->iInteractiveWidth = requisition.width;
		pDialog->iInteractiveHeight = requisition.height;
		g_print ("  pInteractiveWidget : %dx%d\n", pDialog->iInteractiveWidth, pDialog->iInteractiveHeight);

		pDialog->iBubbleWidth = MAX (pDialog->iMessageWidth, MAX (pDialog->iInteractiveWidth, pDialog->iButtonsWidth));
		pDialog->iBubbleHeight = pDialog->iMessageHeight + pDialog->iInteractiveHeight + pDialog->iButtonsHeight;
		g_print (" -> iBubbleWidth: %d , iBubbleHeight : %d\n", pDialog->iBubbleWidth, pDialog->iBubbleHeight);
		cairo_dock_compute_dialog_sizes (pDialog);
	}

	if ((iWidth != pEvent->width || iHeight != pEvent->height))
	{
		if ((pEvent->width != CAIRO_DIALOG_MIN_SIZE || pEvent->height != CAIRO_DIALOG_MIN_SIZE) && (pEvent->width < iWidth || pEvent->height < iHeight))
		{
			g_print ("non, on a dit %dx%d !\n", pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin,
				pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iDistanceToDock);
			gtk_window_resize (GTK_WINDOW (pDialog->pWidget),
				pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin,
				pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iDistanceToDock);
		}
		pDialog->iWidth = pEvent->width;
		pDialog->iHeight = pEvent->height;

		if (pDialog->pIcon != NULL && ! pDialog->bInside)
		{
			//g_print ("configure (%d) => place\n", pDialog->bInside);
			CairoContainer *pContainer = cairo_dock_search_container_from_icon (pDialog->pIcon);
			///gboolean bInside = pDialog->bInside;
			///pDialog->bInside = FALSE;
			cairo_dock_place_dialog (pDialog, pContainer);
			///pDialog->bInside = bInside;
		}
	}
	gtk_widget_queue_draw (pDialog->pWidget);  // les widgets internes peuvent avoir changer de taille sans que le dialogue n'en ait change, il faut donc redessiner tout le temps.

	return FALSE;
}

static cairo_surface_t *_cairo_dock_load_button_icon (cairo_t *pCairoContext, gchar *cButtonImage, gchar *cDefaultButtonImage)
{
	//g_print ("%s (%d ; %d)\n", __func__, myDialogs.iDialogButtonWidth, myDialogs.iDialogButtonHeight);
	cairo_surface_t *pButtonSurface = cairo_dock_load_image_for_icon (pCairoContext,
		cButtonImage,
		myDialogs.iDialogButtonWidth,
		myDialogs.iDialogButtonHeight);

	if (pButtonSurface == NULL)
	{
		gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, cDefaultButtonImage);
		//g_print ("  on charge %s par defaut\n", cIconPath);
		pButtonSurface = cairo_dock_load_image_for_icon (pCairoContext,
			cIconPath,
			myDialogs.iDialogButtonWidth,
			myDialogs.iDialogButtonHeight);
		g_free (cIconPath);
	}

	return pButtonSurface;
}
void cairo_dock_load_dialog_buttons (CairoContainer *pContainer, gchar *cButtonOkImage, gchar *cButtonCancelImage)
{
	//g_print ("%s (%s ; %s)\n", __func__, cButtonOkImage, cButtonCancelImage);
	cairo_t *pCairoContext = cairo_dock_create_context_from_window (pContainer);

	if (s_pButtonOkSurface != NULL)
	{
		cairo_surface_destroy (s_pButtonOkSurface);
		s_pButtonOkSurface = NULL;
	}
	s_pButtonOkSurface = _cairo_dock_load_button_icon (pCairoContext, cButtonOkImage, "cairo-dock-ok.svg");

	if (s_pButtonCancelSurface != NULL)
	{
		cairo_surface_destroy (s_pButtonCancelSurface);
		s_pButtonCancelSurface = NULL;
	}
	s_pButtonCancelSurface = _cairo_dock_load_button_icon (pCairoContext, cButtonCancelImage, "cairo-dock-cancel.svg");

	cairo_destroy (pCairoContext);
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

static void _cairo_dock_isolate_dialog (CairoDialog *pDialog)
{
	cd_debug ("");
	if (pDialog == NULL)
		return ;

	if (pDialog->iSidTimer > 0)
	{
		g_source_remove (pDialog->iSidTimer);
		pDialog->iSidTimer = 0;
	}
	if (pDialog->iSidAnimateIcon > 0)
	{
		g_source_remove (pDialog->iSidAnimateIcon);
		pDialog->iSidAnimateIcon = 0;
	}
	if (pDialog->iSidAnimateText > 0)
	{
		g_source_remove (pDialog->iSidAnimateText);
		pDialog->iSidAnimateText = 0;
	}

	g_signal_handlers_disconnect_by_func (pDialog->pWidget, on_expose_dialog, NULL);
	g_signal_handlers_disconnect_by_func (pDialog->pWidget, on_button_press_dialog, NULL);
	g_signal_handlers_disconnect_by_func (pDialog->pWidget, on_configure_dialog, NULL);
	g_signal_handlers_disconnect_by_func (pDialog->pWidget, on_enter_dialog, NULL);
	g_signal_handlers_disconnect_by_func (pDialog->pWidget, on_leave_dialog, NULL);

	pDialog->iButtonsType = GTK_BUTTONS_NONE;
	pDialog->action_on_answer = NULL;

	pDialog->pIcon = NULL;
}
gboolean cairo_dock_dialog_unreference (CairoDialog *pDialog)
{
	//g_print ("%s (%d)\n", __func__, pDialog->iRefCount);
	if (pDialog != NULL && pDialog->iRefCount > 0)
	{
		pDialog->iRefCount --;
		if (pDialog->iRefCount == 0)  // devient nul.
		{
			_cairo_dock_isolate_dialog (pDialog);
			cairo_dock_free_dialog (pDialog);
			return TRUE;
		}
		else
			return FALSE;  // il n'est pas mort.
	}
	return TRUE;
}


void cairo_dock_free_dialog (CairoDialog *pDialog)
{
	if (pDialog == NULL)
		return ;

	cd_debug ("");
	s_pDialogList = g_slist_remove (s_pDialogList, pDialog);

	if (pDialog->pTextBuffer != NULL)
	{
		cairo_surface_destroy (pDialog->pTextBuffer);
	}
	if (pDialog->pIconBuffer != NULL)
	{
		cairo_surface_destroy (pDialog->pIconBuffer);
	}

	gtk_widget_destroy (pDialog->pWidget);  // detruit aussi le widget interactif.
	pDialog->pWidget = NULL;

	if (pDialog->pUserData != NULL && pDialog->pFreeUserDataFunc != NULL)
		pDialog->pFreeUserDataFunc (pDialog->pUserData);
	
	g_free (pDialog);
}

gboolean cairo_dock_remove_dialog_if_any (Icon *icon)
{
	g_return_val_if_fail (icon != NULL, FALSE);
	CairoDialog *pDialog;
	GSList *ic;
	gboolean bDialogRemoved = FALSE;

	if (s_pDialogList == NULL)
		return FALSE;

	ic = s_pDialogList;
	do
	{
		if (ic->next == NULL)
			break;

		pDialog = ic->next->data;  // on ne peut pas enlever l'element courant, sinon on perd 'ic'.
		if (pDialog->pIcon == icon)
		{
			cairo_dock_dialog_unreference (pDialog);
			bDialogRemoved = TRUE;
		}
		else
		{
			ic = ic->next;
		}
	} while (TRUE);

	pDialog = s_pDialogList->data;
	if (pDialog != NULL && pDialog->pIcon == icon)
	{
		cairo_dock_dialog_unreference (pDialog);
		bDialogRemoved = TRUE;
	}

	return bDialogRemoved;
}

static GtkWidget *_cairo_dock_make_entry_for_dialog (const gchar *cTextForEntry)
{
	GtkWidget *pWidget = gtk_entry_new ();
	gtk_entry_set_has_frame (GTK_ENTRY (pWidget), FALSE);
	gtk_widget_set (pWidget, "width-request", CAIRO_DIALOG_MIN_ENTRY_WIDTH, NULL);
	if (cTextForEntry != NULL)
		gtk_entry_set_text (GTK_ENTRY (pWidget), cTextForEntry);
	return pWidget;
}
static GtkWidget *_cairo_dock_make_hscale_for_dialog (double fValueForHScale, double fMaxValueForHScale)
{
	GtkWidget *pWidget = gtk_hscale_new_with_range (0, fMaxValueForHScale, fMaxValueForHScale / 100.);
	gtk_scale_set_digits (GTK_SCALE (pWidget), 2);
	gtk_range_set_value (GTK_RANGE (pWidget), fValueForHScale);

	gtk_widget_set (pWidget, "width-request", CAIRO_DIALOG_MIN_SCALE_WIDTH, NULL);
	return pWidget;
}
/**GtkWidget *cairo_dock_build_common_interactive_widget_for_dialog (const gchar *cInitialAnswer, double fValueForHScale, double fMaxValueForHScale)
{
	int iBoxWidth = 0, iBoxHeight = 0;
	GtkWidget *pWidget = NULL;
	if (cInitialAnswer != NULL)  // presence d'une GtkEntry.
	{
		pWidget = gtk_entry_new ();
		gtk_entry_set_has_frame (GTK_ENTRY (pWidget), FALSE);

		gtk_entry_set_text (GTK_ENTRY (pWidget), "|_°");  // ces caracteres donnent presque surement la hauteur max.
		PangoLayout *pLayout = gtk_entry_get_layout (GTK_ENTRY (pWidget));
		PangoRectangle ink, log;
		pango_layout_get_pixel_extents (pLayout, &ink, &log);

		int iEntryWidth = MIN (CAIRO_DIALOG_MAX_ENTRY_WIDTH, MAX (ink.width+2, CAIRO_DIALOG_MIN_ENTRY_WIDTH));
		gtk_widget_set (pWidget, "width-request", iEntryWidth, NULL);
		gtk_widget_set (pWidget, "height-request", ink.height+2, NULL);

		iBoxWidth = MAX (iBoxWidth, iEntryWidth);
		cd_debug ("iEntryWidth : %d", iEntryWidth);
		iBoxHeight += ink.height;

		gtk_entry_set_text (GTK_ENTRY (pWidget), cInitialAnswer);
	}
	else if (fMaxValueForHScale > 0 && fValueForHScale >= 0 && fValueForHScale <= fMaxValueForHScale)
	{
		pWidget = gtk_hscale_new_with_range (0, fMaxValueForHScale, fMaxValueForHScale / 100);
		gtk_scale_set_digits (GTK_SCALE (pWidget), 2);
		gtk_range_set_value (GTK_RANGE (pWidget), fValueForHScale);

		gtk_widget_set (pWidget, "width-request", 150, NULL);
	}
	return pWidget;
}*/
GtkWidget *cairo_dock_add_dialog_internal_box (CairoDialog *pDialog, int iWidth, int iHeight, gboolean bCanResize)
{
	GtkWidget *pBox = gtk_hbox_new (0, FALSE);
	if (iWidth != 0 && iHeight != 0)
		gtk_widget_set (pBox, "height-request", iHeight, "width-request", iWidth, NULL);
	else if (iWidth != 0)
			gtk_widget_set (pBox, "width-request", iWidth, NULL);
	else if (iHeight != 0)
			gtk_widget_set (pBox, "height-request", iHeight, NULL);
	gtk_box_pack_start (GTK_BOX (pDialog->pWidgetLayout),
		pBox,
		bCanResize,
		bCanResize,
		0);
	return pBox;
}

static CairoDialog *_cairo_dock_create_new_dialog (void)
{
	//\________________ On cree un dialogue qu'on insere immediatement dans la liste.
	CairoDialog *pDialog = g_new0 (CairoDialog, 1);
	pDialog->iType = CAIRO_DOCK_TYPE_DIALOG;
	pDialog->iRefCount = 1;
	pDialog->fRatio = 1.;
	s_pDialogList = g_slist_prepend (s_pDialogList, pDialog);

	//\________________ On construit la fenetre du dialogue.
	//gboolean bInteractiveWindow = (iButtonsType != GTK_BUTTONS_NONE || pInteractiveWidget != NULL);  // il y'aura des boutons ou un widget interactif, donc la fenetre doit pouvoir recevoir les evenements utilisateur.
	//GtkWidget* pWindow = gtk_window_new (bInteractiveWindow ? GTK_WINDOW_TOPLEVEL : GTK_WINDOW_POPUP);  // les popus ne prennent pas le focus. En fait, ils ne sont meme pas controles par le WM.
	GtkWidget* pWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	pDialog->pWidget = pWindow;

	if (g_bSticky)
		gtk_window_stick (GTK_WINDOW (pWindow));
	gtk_window_set_keep_above (GTK_WINDOW (pWindow), g_bKeepAbove || myAccessibility.bPopUp);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_gravity (GTK_WINDOW (pWindow), GDK_GRAVITY_STATIC);
	/*if (bInteractiveWindow)
		GTK_WIDGET_SET_FLAGS (pWindow, GTK_CAN_FOCUS);  // a priori inutile mais bon.
	else
		GTK_WIDGET_UNSET_FLAGS (pWindow, GTK_CAN_FOCUS);  // pareil, mais bon on ne sait jamais avec ces WM.*/

	cairo_dock_set_colormap_for_window (pWindow);

	gtk_widget_set_app_paintable (pWindow, TRUE);
	gtk_window_set_decorated (GTK_WINDOW (pWindow), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_title (GTK_WINDOW (pWindow), "cairo-dock-dialog");

	gtk_widget_add_events (pWindow, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_window_resize (GTK_WINDOW (pWindow), CAIRO_DIALOG_MIN_SIZE, CAIRO_DIALOG_MIN_SIZE);
	gtk_widget_show_all (pWindow);
	
	return pDialog;
}

static cairo_surface_t *_cairo_dock_create_dialog_text_surface (const gchar *cText, CairoDockLabelDescription *pTextDescription, cairo_t *pSourceContext, int *iTextWidth, int *iTextHeight)
{
	if (cText == NULL)
		return NULL;
	g_print ("%x;%x\n", pTextDescription, pSourceContext);
	double fTextXOffset, fTextYOffset;
	cairo_surface_t *pTextBuffer = cairo_dock_create_surface_from_text_full (cText,
		pSourceContext,
		(pTextDescription ? pTextDescription : &myDialogs.dialogTextDescription),
		1.,
		0,
		iTextWidth,
		iTextHeight,
		&fTextXOffset,
		&fTextYOffset);

	/**PangoRectangle ink, log;
	PangoLayout *pLayout = pango_cairo_create_layout (pSourceContext);

	PangoFontDescription *pDesc = pango_font_description_new ();
	pango_font_description_set_absolute_size (pDesc, myDialogs.dialogTextDescription.iSize * PANGO_SCALE);
	pango_font_description_set_family_static (pDesc, myDialogs.dialogTextDescription.cFont);
	pango_font_description_set_weight (pDesc, myDialogs.dialogTextDescription.iWeight);
	pango_font_description_set_style (pDesc, myDialogs.dialogTextDescription.iStyle);
	pango_layout_set_font_description (pLayout, pDesc);
	pango_font_description_free (pDesc);

	pango_layout_set_text (pLayout, cText, -1);

	pango_layout_get_pixel_extents (pLayout, &ink, &log);

	*iTextWidth = ink.width;
	*iTextHeight = ink.height;

	cairo_surface_t *pTextBuffer = cairo_surface_create_similar (cairo_get_target (pSourceContext),
		CAIRO_CONTENT_COLOR_ALPHA,
		ink.width,
		ink.height);
	cairo_t* pSurfaceContext = cairo_create (pTextBuffer);
	
	cairo_translate (pSurfaceContext, -ink.x, -ink.y);
	cairo_set_source_rgba (pSurfaceContext, myDialogs.dialogTextDescription.fColorStart[0], myDialogs.dialogTextDescription.fColorStart[1], myDialogs.dialogTextDescription.fColorStart[2], 1.);
	pango_cairo_show_layout (pSurfaceContext, pLayout);
	g_object_unref (pLayout);

	cairo_destroy (pSurfaceContext);*/
	return pTextBuffer;
}

static cairo_surface_t *_cairo_dock_create_dialog_icon_surface (const gchar *cImageFilePath, int iNbFrames, cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, int iDesiredSize, int *iIconSize)
{
	if (cImageFilePath == NULL)
		return NULL;
	if (iDesiredSize == 0)
		iDesiredSize = myDialogs.iDialogIconSize;
	cairo_surface_t *pIconBuffer = NULL;
	if (strcmp (cImageFilePath, "same icon") == 0 && pIcon != NULL && pContainer != NULL)
	{
		if (pContainer == NULL)
			pContainer = cairo_dock_search_container_from_icon (pIcon);
		double fMaxScale = (pContainer != NULL ? cairo_dock_get_max_scale (pContainer) : 1.);
		pIconBuffer = cairo_dock_duplicate_surface (pIcon->pIconBuffer,
			pSourceContext,
			pIcon->fWidth * fMaxScale, pIcon->fHeight * fMaxScale,
			iDesiredSize, iDesiredSize);
	}
	else
	{
		//pIconBuffer = cairo_dock_load_image_for_square_icon (pSourceContext, cImageFilePath, myDialogs.iDialogIconSize);
		double fImageWidth = iNbFrames * iDesiredSize, fImageHeight = iDesiredSize;
		pIconBuffer = cairo_dock_load_image (pSourceContext, cImageFilePath,
			&fImageWidth, &fImageHeight,
			0., 1., FALSE);
	}
	if (pIconBuffer != NULL)
		*iIconSize = iDesiredSize;
	return pIconBuffer;
}

static gboolean _cairo_dock_animate_dialog_icon (CairoDialog *pDialog)
{
	pDialog->iCurrentFrame ++;
	if (pDialog->iCurrentFrame == pDialog->iNbFrames)
			pDialog->iCurrentFrame = 0;
	cairo_dock_damage_icon_dialog (pDialog);
	return TRUE;
}
static gboolean _cairo_dock_animate_dialog_text (CairoDialog *pDialog)
{
	if (pDialog->iTextWidth <= pDialog->iMaxTextWidth)
		return FALSE;
	pDialog->iCurrentTextOffset += 3;
	if (pDialog->iCurrentTextOffset >= pDialog->iTextWidth)
			pDialog->iCurrentTextOffset -= pDialog->iTextWidth;
	cairo_dock_damage_text_dialog (pDialog);
	return TRUE;
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
//CairoDialog *cairo_dock_build_dialog (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cImageFilePath, GtkWidget *pInteractiveWidget, GtkButtonsType iButtonsType, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
CairoDialog *cairo_dock_build_dialog (CairoDialogAttribute *pAttribute, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_val_if_fail (pAttribute != NULL, NULL);
	g_print ("%s (%s, %s, %x, %d, %x)\n", __func__, pAttribute->cText, pAttribute->cImageFilePath, pAttribute->pInteractiveWidget, pAttribute->iButtonsType, pAttribute->pTextDescription);
	
	//\________________ On cree un nouveau dialogue.
	CairoDialog *pDialog = _cairo_dock_create_new_dialog ();
	pDialog->pIcon = pIcon;
	cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDialog));
	
	//\________________ On cree la surface du message.
	if (pAttribute->cText != NULL)
	{
		pDialog->iMaxTextWidth = pAttribute->iMaxTextWidth;
		pDialog->pTextBuffer = _cairo_dock_create_dialog_text_surface (pAttribute->cText,
			pAttribute->pTextDescription, pSourceContext,
			&pDialog->iTextWidth, &pDialog->iTextHeight);
		if (pDialog->iMaxTextWidth > 0 && pDialog->pTextBuffer != NULL && pDialog->iTextWidth > pDialog->iMaxTextWidth)
		{
			pDialog->iSidAnimateText = g_timeout_add (200, (GSourceFunc) _cairo_dock_animate_dialog_text, (gpointer) pDialog);  // multiple du timeout de l'icone animee.
		}
	}

	//\________________ On cree la surface de l'icone a afficher sur le cote.
	if (pAttribute->cImageFilePath != NULL)
	{
		pDialog->iNbFrames = (pAttribute->iNbFrames > 0 ? pAttribute->iNbFrames : 1);
		pDialog->pIconBuffer = _cairo_dock_create_dialog_icon_surface (pAttribute->cImageFilePath, pDialog->iNbFrames, pSourceContext, pIcon, pContainer, pAttribute->iIconSize, &pDialog->iIconSize);
		if (pDialog->pIconBuffer != NULL && pDialog->iNbFrames > 1)
		{
			pDialog->iSidAnimateIcon = g_timeout_add (100, (GSourceFunc) _cairo_dock_animate_dialog_icon, (gpointer) pDialog);
		}
	}

	//\________________ On prend en compte le widget interactif.
	if (pAttribute->pInteractiveWidget != NULL)
	{
		pDialog->pInteractiveWidget = pAttribute->pInteractiveWidget;
		
		GtkRequisition requisition;
		gtk_widget_size_request (pAttribute->pInteractiveWidget, &requisition);
		pDialog->iInteractiveWidth = requisition.width;
		pDialog->iInteractiveHeight = requisition.height;
	}
	
	//\________________ On prend en compte les boutons.
	pDialog->iButtonsType = pAttribute->iButtonsType;
	if (pAttribute->pActionFunc == NULL)  // pas d'action, pas de bouton.
		pDialog->iButtonsType = GTK_BUTTONS_NONE;
	
	if (pDialog->iButtonsType != GTK_BUTTONS_NONE)
	{
		pDialog->action_on_answer = pAttribute->pActionFunc;
		pDialog->pUserData = pAttribute->pUserData;
		pDialog->pFreeUserDataFunc = pAttribute->pFreeDataFunc;
		
		if (s_pButtonOkSurface == NULL || s_pButtonCancelSurface == NULL)
			cairo_dock_load_dialog_buttons (CAIRO_CONTAINER (pDialog), myDialogs.cButtonOkImage, myDialogs.cButtonCancelImage);
	}

	//\________________ On lui affecte un decorateur.
	cairo_dock_set_dialog_decorator_by_name (pDialog, (pAttribute->cDecoratorName ? pAttribute->cDecoratorName : myDialogs.cDecoratorName));
	if (pDialog->pDecorator != NULL)
		pDialog->pDecorator->set_size (pDialog);
	
	//\________________ Maintenant qu'on connait tout, on calcule les tailles des divers elements.
	cairo_dock_compute_dialog_sizes (pDialog);
	
	//\________________ On reserve l'espace pour les decorations.
	GtkWidget *pMainHBox = gtk_hbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (pDialog->pWidget), pMainHBox);
	pDialog->pLeftPaddingBox = gtk_vbox_new (0, FALSE);
	gtk_widget_set (pDialog->pLeftPaddingBox, "width-request", pDialog->iLeftMargin, NULL);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pDialog->pLeftPaddingBox,
		FALSE,
		FALSE,
		0);
	
	pDialog->pWidgetLayout = gtk_vbox_new (0, FALSE);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pDialog->pWidgetLayout,
		FALSE,
		FALSE,
		0);
	
	pDialog->pRightPaddingBox = gtk_vbox_new (0, FALSE);
	gtk_widget_set (pDialog->pRightPaddingBox, "width-request", pDialog->iRightMargin, NULL);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pDialog->pRightPaddingBox,
		FALSE,
		FALSE,
		0);
	
	//\________________ On reserve l'espace pour les elements.
	if (pDialog->iMessageWidth != 0 && pDialog->iMessageHeight != 0)
	{
		pDialog->pMessageWidget = cairo_dock_add_dialog_internal_box (pDialog, pDialog->iMessageWidth, pDialog->iMessageHeight, FALSE);
	}
	if (pDialog->pInteractiveWidget != NULL)
	{
		cd_debug (" ref = %d", pAttribute->pInteractiveWidget->object.parent_instance.ref_count);
		gtk_box_pack_start (GTK_BOX (pDialog->pWidgetLayout),
			pDialog->pInteractiveWidget,
			FALSE,
			FALSE,
			0);
		cd_debug (" pack -> ref = %d", pAttribute->pInteractiveWidget->object.parent_instance.ref_count);
	}
	if (pDialog->iButtonsType != GTK_BUTTONS_NONE)
	{
		pDialog->pButtonsWidget = cairo_dock_add_dialog_internal_box (pDialog, pDialog->iButtonsWidth, pDialog->iButtonsHeight, FALSE);
	}
	pDialog->pTipWidget = cairo_dock_add_dialog_internal_box (pDialog, 0, pDialog->iMinBottomGap + pDialog->iBottomMargin, TRUE);
	
	gtk_widget_show_all (pDialog->pWidget);
	
	//\________________ On connecte les signaux utiles.
	g_signal_connect (G_OBJECT (pDialog->pWidget),
		"expose-event",
		G_CALLBACK (on_expose_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->pWidget),
		"configure-event",
		G_CALLBACK (on_configure_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->pWidget),
		"button-press-event",
		G_CALLBACK (on_button_press_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->pWidget),
		"button-release-event",
		G_CALLBACK (on_button_press_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->pWidget),
		"key-press-event",
		G_CALLBACK (on_key_press_dialog),
		pDialog);
	if (pIcon != NULL)  // on inhibe le deplacement du dialogue lorsque l'utilisateur est dedans.
	{
		g_signal_connect (G_OBJECT (pDialog->pWidget),
			"enter-notify-event",
			G_CALLBACK (on_enter_dialog),
			pDialog);
		g_signal_connect (G_OBJECT (pDialog->pWidget),
			"leave-notify-event",
			G_CALLBACK (on_leave_dialog),
			pDialog);
	}

	cairo_dock_place_dialog (pDialog, pContainer);  // renseigne aussi bDirectionUp, bIsHorizontal, et iHeight.
	
	cairo_destroy (pSourceContext);
	
	if (pAttribute->iTimeLength != 0)
		pDialog->iSidTimer = g_timeout_add (pAttribute->iTimeLength, (GSourceFunc) _cairo_dock_dialog_auto_delete, (gpointer) pDialog);

	return pDialog;
}


void cairo_dock_dialog_calculate_aimed_point (Icon *pIcon, CairoContainer *pContainer, int *iX, int *iY, gboolean *bRight, CairoDockTypeHorizontality *bIsHorizontal, gboolean *bDirectionUp)
{
	g_return_if_fail (pIcon != NULL && pContainer != NULL);
	//g_print ("%s (%.2f, %.2f)\n", __func__, pIcon->fXAtRest, pIcon->fDrawX);
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		if (pDock->iRefCount == 0 && pDock->bAtBottom)  // un dock principal au repos.
		{
			*bIsHorizontal = (pDock->bHorizontalDock == CAIRO_DOCK_HORIZONTAL);
			if (pDock->bHorizontalDock)
			{
				*bRight = (pIcon->fXAtRest > pDock->fFlatDockWidth / 2);
				*bDirectionUp = (pDock->iWindowPositionY > g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
				*iY = (*bDirectionUp ? pDock->iWindowPositionY : pDock->iWindowPositionY + pDock->iCurrentHeight);
			}
			else
			{
				*bRight = (pDock->iWindowPositionY < g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] / 2);
				*bDirectionUp = (pIcon->fXAtRest > pDock->fFlatDockWidth / 2);
				*iY = (! (*bRight) ? pDock->iWindowPositionY : pDock->iWindowPositionY + pDock->iCurrentHeight);
			}
	
			if (pDock->bAutoHide)
			{
				*iX = pDock->iWindowPositionX + (pIcon->fXAtRest + pIcon->fWidth * (*bRight ? .7 : .3)) / pDock->fFlatDockWidth * myHiddenDock.iVisibleZoneWidth;
				cd_debug ("placement sur un dock cache -> %d", *iX);
			}
			else
			{
				*iX = pDock->iWindowPositionX + pIcon->fDrawX + pIcon->fWidth * pIcon->fScale * pIcon->fWidthFactor / 2 + pIcon->fWidth * (*bRight ? .2 : - .2);
			}
		}
		else if (pDock->iRefCount > 0 && ! GTK_WIDGET_VISIBLE (pDock->pWidget))  // sous-dock invisible.  // pDock->bAtBottom
		{
			CairoDock *pParentDock = NULL;
			Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
			cairo_dock_dialog_calculate_aimed_point (pPointingIcon, CAIRO_CONTAINER (pParentDock), iX, iY, bRight, bIsHorizontal, bDirectionUp);
		}
		else  // dock actif.
		{
			*bIsHorizontal = (pDock->bHorizontalDock == CAIRO_DOCK_HORIZONTAL);
			if (pDock->bHorizontalDock)
			{
				*bRight = (pIcon->fXAtRest > pDock->fFlatDockWidth / 2);
				*bDirectionUp = (pDock->iWindowPositionY > g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
				*iY = (*bDirectionUp ? pDock->iWindowPositionY : pDock->iWindowPositionY + pDock->iCurrentHeight);
			}
			else
			{
				*bRight = (pDock->iWindowPositionY < g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] / 2);
				*bDirectionUp = (pIcon->fXAtRest > pDock->fFlatDockWidth / 2);
				*iY = (! (*bRight) ? pDock->iWindowPositionY : pDock->iWindowPositionY + pDock->iCurrentHeight);
			}
			*iX = pDock->iWindowPositionX + pIcon->fDrawX + pIcon->fWidth * pIcon->fScale * pIcon->fWidthFactor / 2 + pIcon->fWidth * (*bRight ? .2 : - .2);
		}
	}
	else if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
		*bDirectionUp = (pDesklet->iWindowPositionY > g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
		*bIsHorizontal = (pDesklet->iWindowPositionX > 50 && pDesklet->iWindowPositionX + pDesklet->iHeight < g_iXScreenWidth[CAIRO_DOCK_HORIZONTAL] - 50);
		
		if (*bIsHorizontal)
		{
			*bRight = (pDesklet->iWindowPositionX > g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
			*iX = pDesklet->iWindowPositionX + pDesklet->iWidth * (*bRight ? .7 : .3);
			*iY = (*bDirectionUp ? pDesklet->iWindowPositionY : pDesklet->iWindowPositionY + pDesklet->iHeight);
		}
		else
		{
			*bRight = (pDesklet->iWindowPositionX < g_iXScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
			*iY = pDesklet->iWindowPositionX + pDesklet->iWidth * (*bRight ? 1 : 0);
			*iX =pDesklet->iWindowPositionY + pDesklet->iHeight / 2;
		}
	}
}


static void _cairo_dock_dialog_find_optimal_placement (CairoDialog *pDialog)
{
	//g_print ("%s (Ybulle:%d; width:%d)\n", __func__, pDialog->iPositionY, pDialog->iWidth);
	g_return_if_fail (pDialog->iPositionY > 0);

	Icon *icon;
	CairoDialog *pDialogOnOurWay;

	double fXLeft = 0, fXRight = g_iXScreenWidth[pDialog->bIsHorizontal];
	if (pDialog->bRight)
	{
		fXLeft = -1e4;
		fXRight = MAX (g_iXScreenWidth[pDialog->bIsHorizontal], pDialog->iAimedX + pDialog->iMinFrameWidth + pDialog->iRightMargin + 1);  // 2*CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE
	}
	else
	{
		fXLeft = MIN (0, pDialog->iAimedX - (pDialog->iMinFrameWidth + pDialog->iLeftMargin + 1));  // 2*CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE
		fXRight = 1e4;
	}
	gboolean bCollision = FALSE;
	double fNextYStep = (pDialog->bDirectionUp ? -1e4 : 1e4);
	int iYInf, iYSup;
	GSList *ic;
	for (ic = s_pDialogList; ic != NULL; ic = ic->next)
	{
		pDialogOnOurWay = ic->data;
		if (pDialogOnOurWay == NULL)
			continue ;
		if (pDialogOnOurWay != pDialog)
		{
			if (GTK_WIDGET_VISIBLE (pDialogOnOurWay->pWidget) && pDialogOnOurWay->pIcon != NULL)
			{
				iYInf = (pDialog->bDirectionUp ? pDialogOnOurWay->iPositionY : pDialogOnOurWay->iPositionY + pDialogOnOurWay->iHeight - (pDialogOnOurWay->iBubbleHeight + 0));
				iYSup = (pDialog->bDirectionUp ? pDialogOnOurWay->iPositionY + pDialogOnOurWay->iBubbleHeight + 0 : pDialogOnOurWay->iPositionY + pDialogOnOurWay->iHeight);
				if (iYInf < pDialog->iPositionY + pDialog->iBubbleHeight + 0 && iYSup > pDialog->iPositionY)
				{
					//g_print ("pDialogOnOurWay : %d - %d ; pDialog : %d - %d\n", iYInf, iYSup, pDialog->iPositionY, pDialog->iPositionY + (pDialog->iBubbleHeight + 2 * myBackground.iDockLineWidth));
					if (pDialogOnOurWay->iAimedX < pDialog->iAimedX)
						fXLeft = MAX (fXLeft, pDialogOnOurWay->iPositionX + pDialogOnOurWay->iWidth);
					else
						fXRight = MIN (fXRight, pDialogOnOurWay->iPositionX);
					bCollision = TRUE;
					fNextYStep = (pDialog->bDirectionUp ? MAX (fNextYStep, iYInf) : MIN (fNextYStep, iYSup));
				}
			}
		}
	}
	//g_print (" -> [%.2f ; %.2f]\n", fXLeft, fXRight);

	if ((fXRight - fXLeft > pDialog->iWidth) && (
		(pDialog->bRight && fXLeft + pDialog->iLeftMargin < pDialog->iAimedX && fXRight > pDialog->iAimedX + pDialog->iMinFrameWidth + pDialog->iRightMargin)
		||
		(! pDialog->bRight && fXLeft < pDialog->iAimedX - pDialog->iMinFrameWidth - pDialog->iLeftMargin && fXRight > pDialog->iAimedX + pDialog->iRightMargin)
		))
	{
		if (pDialog->bRight)
			pDialog->iPositionX = MIN (pDialog->iAimedX - pDialog->fAlign * pDialog->iBubbleWidth - pDialog->iLeftMargin, fXRight - pDialog->iWidth);
		else
			pDialog->iPositionX = MAX (pDialog->iAimedX - pDialog->iRightMargin - pDialog->iWidth + pDialog->fAlign * pDialog->iBubbleWidth, fXLeft);  /// pDialog->iBubbleWidth (?)
	}
	else
	{
		//g_print (" * Aim : (%d ; %d) ; Width : %d\n", pDialog->iAimedX, pDialog->iAimedY, pDialog->iWidth);
		pDialog->iPositionY = fNextYStep - (pDialog->bDirectionUp ? pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin : 0);
		_cairo_dock_dialog_find_optimal_placement (pDialog);
	}
}

void cairo_dock_place_dialog (CairoDialog *pDialog, CairoContainer *pContainer)
{
	//g_print ("%s ()\n", __func__);
	int iPrevPositionX = pDialog->iPositionX, iPrevPositionY = pDialog->iPositionY;
	if (pContainer != NULL && pDialog->pIcon != NULL)
	{
		cairo_dock_dialog_calculate_aimed_point (pDialog->pIcon, pContainer, &pDialog->iAimedX, &pDialog->iAimedY, &pDialog->bRight, &pDialog->bIsHorizontal, &pDialog->bDirectionUp);	
		g_print (" Aim (%d;%d) / %d,%d,%d\n", pDialog->iAimedX, pDialog->iAimedY, pDialog->bIsHorizontal, pDialog->bDirectionUp, pDialog->bInside);
		
		if (pDialog->bIsHorizontal)
		{
			if (! pDialog->bInside)
			{
				pDialog->iPositionY = (pDialog->bDirectionUp ? pDialog->iAimedY - (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iMinBottomGap) : pDialog->iAimedY + pDialog->iMinBottomGap);  // on place la bulle d'abord sans prendre en compte la pointe.
				_cairo_dock_dialog_find_optimal_placement (pDialog);
			}
			else if (! pDialog->bDirectionUp)
				pDialog->iPositionY += pDialog->iDistanceToDock;
		}
		else
		{
			int tmp = pDialog->iAimedX;
			pDialog->iAimedX = pDialog->iAimedY;
			pDialog->iAimedY = tmp;
			if (! pDialog->bInside)
			{
				pDialog->iPositionX = (pDialog->bRight ? pDialog->iAimedX - pDialog->fAlign * pDialog->iBubbleWidth : pDialog->iAimedX - pDialog->iWidth + pDialog->fAlign * pDialog->iBubbleWidth);
				pDialog->iPositionY = (pDialog->bDirectionUp ? pDialog->iAimedY - (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iMinBottomGap) : pDialog->iAimedY + pDialog->iMinBottomGap);  // on place la bulle (et non pas la fenetre) sans faire d'optimisation.
			}
		}
		g_print (" => position : (%d;%d)\n", pDialog->iPositionX, pDialog->iPositionY);
		int iOldDistance = pDialog->iDistanceToDock;
		pDialog->iDistanceToDock = (pDialog->bDirectionUp ? pDialog->iAimedY - pDialog->iPositionY - (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin) : pDialog->iPositionY - pDialog->iAimedY);
		if (! pDialog->bDirectionUp)  // iPositionY est encore la position du coin haut gauche de la bulle et non de la fenetre.
			pDialog->iPositionY = pDialog->iAimedY;
		
		if (pDialog->iDistanceToDock != iOldDistance)
		{
			g_print ("  On change la taille de la pointe a : %d pixels ( -> %d)\n", pDialog->iDistanceToDock, pDialog->iMessageHeight + pDialog->iInteractiveHeight +pDialog->iButtonsHeight + pDialog->iDistanceToDock);
			gtk_widget_set (pDialog->pTipWidget, "height-request", MAX (0, pDialog->iDistanceToDock + pDialog->iBottomMargin), NULL);

			if ((iOldDistance == 0) || (pDialog->iDistanceToDock < iOldDistance))
			{
				g_print ("    cela reduit la fenetre a %dx%d\n", pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin, pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iDistanceToDock);
				gtk_window_resize (GTK_WINDOW (pDialog->pWidget),
					pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin,
					pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iDistanceToDock);
			}
		}
	}
	else  // au milieu de l'ecran courant.
	{
		pDialog->bDirectionUp = TRUE;
		pDialog->iPositionX = g_iScreenOffsetX + (g_iScreenWidth [CAIRO_DOCK_HORIZONTAL] - pDialog->iWidth) / 2;
		pDialog->iPositionY = g_iScreenOffsetY + (g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] - pDialog->iHeight) / 2;
		pDialog->iHeight = pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin;
		gtk_window_resize (GTK_WINDOW (pDialog->pWidget),
			pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin,
			pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin);
	}

	if (iPrevPositionX != pDialog->iPositionX || iPrevPositionY != pDialog->iPositionY)
	{
		g_print (" => move to (%d;%d) %dx%d\n", pDialog->iPositionX, pDialog->iPositionY, pDialog->iWidth, pDialog->iHeight);
		gtk_window_move (GTK_WINDOW (pDialog->pWidget),
			pDialog->iPositionX,
			pDialog->iPositionY);
	}
}

void cairo_dock_compute_dialog_sizes (CairoDialog *pDialog)
{
	pDialog->iMessageWidth = pDialog->iIconSize + _drawn_text_width (pDialog) + (pDialog->iTextWidth != 0 ? 2 : 0) * CAIRO_DIALOG_TEXT_MARGIN;  // icone + marge + texte + marge.
	pDialog->iMessageHeight = MAX (pDialog->iIconSize, pDialog->iTextHeight) + (pDialog->pInteractiveWidget != NULL ? CAIRO_DIALOG_VGAP : 0);  // (icone/texte + marge) + widget + (marge + boutons) + pointe.
	
	if (pDialog->iButtonsType != GTK_BUTTONS_NONE)
	{
		pDialog->iButtonsWidth = 2 * myDialogs.iDialogButtonWidth + CAIRO_DIALOG_BUTTON_GAP + 2 * CAIRO_DIALOG_TEXT_MARGIN;  // marge + bouton1 + ecart + bouton2 + marge.
		pDialog->iButtonsHeight = CAIRO_DIALOG_VGAP + myDialogs.iDialogButtonHeight;  // il y'a toujours quelque chose au-dessus (texte et/out widget)
	}
	
	pDialog->iBubbleWidth = MAX (pDialog->iInteractiveWidth, MAX (pDialog->iButtonsWidth, MAX (pDialog->iMessageWidth, pDialog->iMinFrameWidth)));
	pDialog->iBubbleHeight = pDialog->iMessageHeight + pDialog->iInteractiveHeight + pDialog->iButtonsHeight;
	if (pDialog->iBubbleWidth == 0)  // precaution.
		pDialog->iBubbleWidth = 2 * myBackground.iDockRadius + myBackground.iDockLineWidth;
	if (pDialog->iBubbleHeight == 0)
		pDialog->iBubbleHeight = 2 * myBackground.iDockRadius + myBackground.iDockLineWidth;
	
	pDialog->iWidth = pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin;
	pDialog->iHeight = pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iMinBottomGap;  // resultat temporaire, sans la pointe.
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
		if (pIcon != NULL && GTK_WIDGET_VISIBLE (pDialog->pWidget)) // on ne replace pas les dialogues en cours de destruction ou caches.
		{
			pContainer = cairo_dock_search_container_from_icon (pIcon);
			if (CAIRO_DOCK_IS_DOCK (pContainer))
			{
				int iPositionX = pDialog->iPositionX;
				int iPositionY = pDialog->iPositionY;
				int iAimedX = pDialog->iAimedX;
				int iAimedY = pDialog->iAimedY;
				cairo_dock_place_dialog (pDialog, pContainer);
				
				if (iPositionX != pDialog->iPositionX || iPositionY != pDialog->iPositionY || iAimedX != pDialog->iAimedX || iAimedY != pDialog->iAimedY)
					gtk_widget_queue_draw (pDialog->pWidget);  // on redessine meme si la position n'a pas changee, car la pointe, elle, change.
			}
		}
	}
}


CairoDialog *cairo_dock_show_dialog_full (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, gchar *cIconPath, GtkButtonsType iButtonsType, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	if (pIcon != NULL && pIcon->fPersonnalScale > 0)  // icone en cours de suppression.
		return NULL;
	
	CairoDialogAttribute attr;
	memset (&attr, 0, sizeof (CairoDialogAttribute));
	attr.cText = cText;
	attr.cImageFilePath = cIconPath;
	attr.pInteractiveWidget = pInteractiveWidget;
	attr.iButtonsType = iButtonsType;
	attr.pActionFunc = pActionFunc;
	attr.pUserData = data;
	attr.pFreeDataFunc = pFreeDataFunc;
	attr.iTimeLength = (int) fTimeLength;
	
	CairoDialog *pDialog = cairo_dock_build_dialog (&attr, pIcon, pContainer);
	return pDialog;
}



CairoDialog *cairo_dock_show_temporary_dialog_with_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, gchar *cIconPath, ...)
{
	va_list args;
	va_start (args, cIconPath);
	gchar *cFullText = g_strdup_vprintf (cText, args);
	CairoDialog *pDialog = cairo_dock_show_dialog_full (cFullText, pIcon, pContainer, fTimeLength, cIconPath, GTK_BUTTONS_NONE, NULL, NULL, NULL, NULL);
	g_free (cFullText);
	va_end (args);
	return pDialog;
}

CairoDialog *cairo_dock_show_temporary_dialog (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, ...)
{
	va_list args;
	va_start (args, fTimeLength);
	gchar *cFullText = g_strdup_vprintf (cText, args);
	CairoDialog *pDialog = cairo_dock_show_dialog_full (cFullText, pIcon, pContainer, fTimeLength, NULL, GTK_BUTTONS_NONE, NULL, NULL, NULL, NULL);
	g_free (cFullText);
	va_end (args);
	return pDialog;
}

CairoDialog *cairo_dock_show_temporary_dialog_with_default_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, ...)
{
	va_list args;
	va_start (args, fTimeLength);
	gchar *cFullText = g_strdup_vprintf (cText, args);
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);
	cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, "cairo-dock-animated.xpm");
	CairoDialogAttribute attr;
	memset (&attr, 0, sizeof (CairoDialogAttribute));
	attr.cText = cFullText;
	attr.cImageFilePath = cIconPath;
	attr.iNbFrames = 12;
	attr.iIconSize = 32;
	attr.iTimeLength = (int) fTimeLength;
	//CairoDialog *pDialog = cairo_dock_show_dialog_full (cFullText, pIcon, pContainer, fTimeLength, cIconPath, GTK_BUTTONS_NONE, NULL, NULL, NULL, NULL);
	CairoDialog *pDialog = cairo_dock_build_dialog (&attr, pIcon, pContainer);
	g_free (cIconPath);
	g_free (cFullText);
	va_end (args);
	return pDialog;
}



CairoDialog *cairo_dock_show_dialog_with_question (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, 0, cIconPath, GTK_BUTTONS_YES_NO, NULL, pActionFunc, data, pFreeDataFunc);
}

CairoDialog *cairo_dock_show_dialog_with_entry (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, const gchar *cTextForEntry, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (cTextForEntry, -1, -1);
	GtkWidget *pWidget = _cairo_dock_make_entry_for_dialog (cTextForEntry);

	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, 0, cIconPath, GTK_BUTTONS_OK_CANCEL, pWidget, pActionFunc, data, pFreeDataFunc);
}

CairoDialog *cairo_dock_show_dialog_with_value (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, double fValue, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	fValue = MAX (0., fValue);
	fValue = MIN (1., fValue);
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (NULL, fValue, 1.);
	GtkWidget *pWidget = _cairo_dock_make_hscale_for_dialog (fValue, 1.);

	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, 0, cIconPath, GTK_BUTTONS_OK_CANCEL, pWidget, pActionFunc, data, pFreeDataFunc);
}




static void _cairo_dock_get_answer_from_dialog (int iAnswer, GtkWidget *pInteractiveWidget, gpointer *data)
{
	cd_message ("%s (%d)", __func__, iAnswer);
	int *iAnswerBuffer = data[0];
	GMainLoop *pBlockingLoop = data[1];
	GtkWidget *pWidgetCatcher = data[2];
	if (pInteractiveWidget != NULL)
		gtk_widget_reparent (pInteractiveWidget, pWidgetCatcher);  // j'ai rien trouve de mieux pour empecher que le 'pInteractiveWidget' ne soit pas detruit avec le dialogue apres l'appel de la callback (g_object_ref ne marche pas).

	*iAnswerBuffer = iAnswer;

	if (g_main_loop_is_running (pBlockingLoop))
		g_main_loop_quit (pBlockingLoop);
}
static gboolean _cairo_dock_dialog_destroyed (GtkWidget *widget, GdkEvent *event, GMainLoop *pBlockingLoop)
{
	cd_message ("dialogue detruit, on sort de la boucle");
	if (g_main_loop_is_running (pBlockingLoop))
		g_main_loop_quit (pBlockingLoop);
	return FALSE;
}
int cairo_dock_show_dialog_and_wait (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, gchar *cIconPath, GtkButtonsType iButtonsType, GtkWidget *pInteractiveWidget)
{
	static GtkWidget *pWidgetCatcher = NULL;  // voir l'astuce plus haut.
	int iAnswer = GTK_RESPONSE_NONE;
	GMainLoop *pBlockingLoop = g_main_loop_new (NULL, FALSE);
	if (pWidgetCatcher == NULL)
		pWidgetCatcher = gtk_hbox_new (0, FALSE);
	gpointer data[3] = {&iAnswer, pBlockingLoop, pWidgetCatcher};  // inutile d'allouer 'data' puisqu'on va bloquer.

	CairoDialog *pDialog = cairo_dock_show_dialog_full (cText,
		pIcon,
		pContainer,
		0.,
		cIconPath,
		iButtonsType,
		pInteractiveWidget,
		(CairoDockActionOnAnswerFunc)_cairo_dock_get_answer_from_dialog,
		(gpointer) data,
		(GFreeFunc) NULL);

	if (pDialog != NULL)
	{
		gtk_window_set_modal (GTK_WINDOW (pDialog->pWidget), TRUE);
		g_signal_connect (pDialog->pWidget,
			"delete-event",
			G_CALLBACK (_cairo_dock_dialog_destroyed),
			pBlockingLoop);

		//g_print ("debut de boucle bloquante ...\n");
		GDK_THREADS_LEAVE ();
		g_main_loop_run (pBlockingLoop);
		GDK_THREADS_ENTER ();
		//g_print ("fin de boucle bloquante -> %d\n", iAnswer);
	}

	g_main_loop_unref (pBlockingLoop);

	return iAnswer;
}

gchar *cairo_dock_show_demand_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, const gchar *cInitialAnswer)
{
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog ((cInitialAnswer != NULL ? cInitialAnswer : ""), -1, -1);
	GtkWidget *pWidget = _cairo_dock_make_entry_for_dialog (cInitialAnswer);
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);

	int iAnswer = cairo_dock_show_dialog_and_wait (cMessage, pIcon, pContainer, 0, cIconPath, GTK_BUTTONS_OK_CANCEL, pWidget);
	g_free (cIconPath);

	gchar *cAnswer = (iAnswer == GTK_RESPONSE_OK ? g_strdup (gtk_entry_get_text (GTK_ENTRY (pWidget))) : NULL);
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

	int iAnswer = cairo_dock_show_dialog_and_wait (cMessage, pIcon, pContainer, 0, cIconPath, GTK_BUTTONS_OK_CANCEL, pWidget);
	g_free (cIconPath);

	double fValue = (iAnswer == GTK_RESPONSE_OK ? gtk_range_get_value (GTK_RANGE (pWidget)) : -1);
	cd_message ("fValue : %.2f", fValue);

	gtk_widget_destroy (pWidget);
	return fValue;
}

int cairo_dock_ask_question_and_wait (const gchar *cQuestion, Icon *pIcon, CairoContainer *pContainer)
{
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);  // trouver une icone de question...
	int iAnswer = cairo_dock_show_dialog_and_wait (cQuestion, pIcon, pContainer, 0, cIconPath, GTK_BUTTONS_YES_NO, NULL);
	g_free (cIconPath);

	return (iAnswer == GTK_RESPONSE_OK ? GTK_RESPONSE_YES : GTK_RESPONSE_NO);
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
	if (pIcon == NULL)
	{
		pIcon = cairo_dock_get_first_icon_of_type (g_pMainDock->icons, CAIRO_DOCK_SEPARATOR23);
		if (pIcon == NULL)
		{
			pIcon = cairo_dock_get_pointed_icon (g_pMainDock->icons);
			if (pIcon == NULL || CAIRO_DOCK_IS_NORMAL_APPLI (pIcon))
			{
				GList *ic;
				for (ic = g_pMainDock->icons; ic != NULL; ic = ic->next)
				{
					pIcon = ic->data;
					if (! cairo_dock_icon_has_dialog (pIcon) && ! CAIRO_DOCK_IS_NORMAL_APPLI (pIcon))
						break;
				}
			}
		}
	}
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
	gtk_widget_hide (pDialog->pWidget);
	pDialog->bInside = FALSE;
}

void cairo_dock_unhide_dialog (CairoDialog *pDialog)
{
	if (! GTK_WIDGET_VISIBLE (pDialog->pWidget))
	{
		gtk_window_present (GTK_WINDOW (pDialog->pWidget));
		Icon *pIcon = pDialog->pIcon;
		if (pIcon != NULL)
		{
			CairoContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
			cairo_dock_place_dialog (pDialog, pContainer);
		}
	}
}


GtkWidget *cairo_dock_steal_widget_from_its_container (GtkWidget *pWidget)
{
	/*static GtkWidget *pWidgetCatcher = NULL;
	if (pWidgetCatcher == NULL)
		pWidgetCatcher = gtk_hbox_new (0, FALSE);*/
	g_return_val_if_fail (pWidget != NULL, NULL);
	GtkWidget *pContainer = gtk_widget_get_parent (pWidget);
	if (pWidget != NULL && pContainer != NULL)
	{
		/*gtk_widget_reparent (pWidget, pWidgetCatcher);
		cd_message ("reparent -> ref = %d\n", pWidget->object.parent_instance.ref_count);
		gtk_object_ref (GTK_OBJECT (pWidget));
		gtk_object_ref (GTK_OBJECT (pWidget));
		gtk_widget_unparent (pWidget);
		gtk_object_unref (GTK_OBJECT (pWidget));
		cd_message ("unparent -> ref = %d\n", pWidget->object.parent_instance.ref_count);*/
		cd_debug (" ref : %d", pWidget->object.parent_instance.ref_count);
		gtk_object_ref (GTK_OBJECT (pWidget));
		gtk_container_remove (GTK_CONTAINER (pContainer), pWidget);
		cd_debug (" -> %d", pWidget->object.parent_instance.ref_count);
	}
	return pWidget;
}



void cairo_dock_set_new_dialog_text_surface (CairoDialog *pDialog, cairo_surface_t *pNewTextSurface, int iNewTextWidth, int iNewTextHeight)
{
	int iPrevMessageWidth = pDialog->iMessageWidth;
	int iPrevMessageHeight = pDialog->iMessageHeight;

	cairo_surface_destroy (pDialog->pTextBuffer);
	pDialog->pTextBuffer = pNewTextSurface;
	
	pDialog->iTextWidth = iNewTextWidth;
	pDialog->iTextHeight = iNewTextHeight;
	cairo_dock_compute_dialog_sizes (pDialog);

	if (pDialog->iMessageWidth != iPrevMessageWidth || pDialog->iMessageHeight != iPrevMessageHeight)
	{
		gtk_widget_set (pDialog->pMessageWidget, "width-request", pDialog->iMessageWidth, "height-request", pDialog->iMessageHeight, NULL);
		if (pDialog->pIcon != NULL)
		{
			CairoContainer *pContainer = cairo_dock_search_container_from_icon (pDialog->pIcon);
			cairo_dock_place_dialog (pDialog, pContainer);
		}
		gtk_widget_queue_draw (pDialog->pWidget);
	}
	else
	{
		cairo_dock_damage_text_dialog (pDialog);
	}

	if (pDialog->iMaxTextWidth > 0 && pDialog->iSidAnimateText == 0 && pDialog->pTextBuffer != NULL && pDialog->iTextWidth > pDialog->iMaxTextWidth)
	{
		pDialog->iSidAnimateText = g_timeout_add (200, (GSourceFunc) _cairo_dock_animate_dialog_text, (gpointer) pDialog);  // multiple du timeout de l'icone animee.
	}
}

void cairo_dock_set_new_dialog_icon_surface (CairoDialog *pDialog, cairo_surface_t *pNewIconSurface, int iNewIconSize)
{
	int iPrevMessageWidth = pDialog->iMessageWidth;
	int iPrevMessageHeight = pDialog->iMessageHeight;

	cairo_surface_destroy (pDialog->pIconBuffer);
	pDialog->pIconBuffer = pNewIconSurface;
	
	pDialog->iIconSize = iNewIconSize;
	cairo_dock_compute_dialog_sizes (pDialog);

	if (pDialog->iMessageWidth != iPrevMessageWidth || pDialog->iMessageHeight != iPrevMessageHeight)
	{
		gtk_widget_set (pDialog->pMessageWidget, "width-request", pDialog->iMessageWidth, "height-request", pDialog->iMessageHeight, NULL);
		if (pDialog->pIcon != NULL)
		{
			CairoContainer *pContainer = cairo_dock_search_container_from_icon (pDialog->pIcon);
			cairo_dock_place_dialog (pDialog, pContainer);
		}
		gtk_widget_queue_draw (pDialog->pWidget);
	}
	else
	{
		cairo_dock_damage_icon_dialog (pDialog);
	}
}


void cairo_dock_set_dialog_message (CairoDialog *pDialog, const gchar *cMessage)
{
	cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDialog));

	int iNewTextWidth=0, iNewTextHeight=0;
	cairo_surface_t *pNewTextSurface = _cairo_dock_create_dialog_text_surface (cMessage, NULL, pSourceContext, &iNewTextWidth, &iNewTextHeight);
	
	cairo_dock_set_new_dialog_text_surface (pDialog, pNewTextSurface, iNewTextWidth, iNewTextHeight);

	cairo_destroy (pSourceContext);
}

void cairo_dock_set_dialog_icon (CairoDialog *pDialog, const gchar *cImageFilePath)
{
	cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDialog));

	cairo_surface_t *pNewIconSurface = cairo_dock_create_surface_for_square_icon (cImageFilePath, pSourceContext, myDialogs.iDialogIconSize);
	int iNewIconSize = (pNewIconSurface != NULL ? myDialogs.iDialogIconSize : 0);
			
	cairo_dock_set_new_dialog_icon_surface (pDialog, pNewIconSurface, iNewIconSize);

	cairo_destroy (pSourceContext);
}


void cairo_dock_damage_icon_dialog (CairoDialog *pDialog)
{
	gtk_widget_queue_draw_area (pDialog->pWidget,
		pDialog->iLeftMargin,
		(pDialog->bDirectionUp ? 
			pDialog->iTopMargin :
			pDialog->iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight)),
		pDialog->iIconSize,
		pDialog->iIconSize);
}

void cairo_dock_damage_text_dialog (CairoDialog *pDialog)
{
	gtk_widget_queue_draw_area (pDialog->pWidget,
		pDialog->iLeftMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN,
		(pDialog->bDirectionUp ? 
			pDialog->iTopMargin :
			pDialog->iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight)),
		_drawn_text_width (pDialog),
		pDialog->iTextHeight);
}

void cairo_dock_damage_interactive_widget_dialog (CairoDialog *pDialog)
{
	gtk_widget_queue_draw_area (pDialog->pWidget,
		pDialog->iLeftMargin,
		(pDialog->bDirectionUp ? 
			pDialog->iTopMargin + pDialog->iMessageHeight :
			pDialog->iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight) + pDialog->iMessageHeight),
		pDialog->iInteractiveWidth,
		pDialog->iInteractiveHeight);
}



// Futur applet dialog-rendering

#define CAIRO_DIALOG_MIN_GAP 20
#define CAIRO_DIALOG_TIP_ROUNDING_MARGIN 12
#define CAIRO_DIALOG_TIP_MARGIN 25
#define CAIRO_DIALOG_TIP_BASE 25

void cairo_dock_set_frame_size_comics (CairoDialog *pDialog)
{
	double fRadius = myDialogs.iCornerRadius;
	double fLineWidth = myDialogs.iLineWidth;
	int iMargin = .5 * fLineWidth + (1. - sqrt (2) / 2) * fRadius;
	pDialog->iRightMargin = iMargin;
	pDialog->iLeftMargin = iMargin;
	pDialog->iTopMargin = iMargin;
	pDialog->iBottomMargin = iMargin;
	pDialog->iMinBottomGap = CAIRO_DIALOG_MIN_GAP;
	pDialog->iMinFrameWidth = iMargin + CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_ROUNDING_MARGIN + CAIRO_DIALOG_TIP_BASE + iMargin;  // dans l'ordre.
	pDialog->fAlign = 0.;  // la pointe colle au bord du dialogue.
}

void cairo_dock_draw_decorations_comics (cairo_t *pCairoContext, CairoDialog *pDialog)
{
	double fLineWidth = myDialogs.iLineWidth;
	double fRadius = myDialogs.iCornerRadius;
	
	double fGapFromDock = pDialog->iDistanceToDock + .5 * fLineWidth;
	double cos_gamma = 1 / sqrt (1. + 1. * (CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE) / fGapFromDock * (CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE) / fGapFromDock);
	double cos_theta = 1 / sqrt (1. + 1. * CAIRO_DIALOG_TIP_MARGIN / fGapFromDock * CAIRO_DIALOG_TIP_MARGIN / fGapFromDock);
	double fTipHeight = fGapFromDock / (1. + fLineWidth / 2. / CAIRO_DIALOG_TIP_BASE * (1./cos_gamma + 1./cos_theta));
	//g_print ("TipHeight <- %d\n", (int)fTipHeight);

	double fOffsetX	= fRadius +	fLineWidth / 2;
	double fOffsetY	= (pDialog->bDirectionUp ? fLineWidth / 2 : pDialog->iHeight - fLineWidth / 2);
	int	sens = (pDialog->bDirectionUp ?	1 :	-1);
	cairo_move_to (pCairoContext, fOffsetX, fOffsetY);
	//g_print ("  fOffsetX : %.2f; fOffsetY	: %.2f\n", fOffsetX, fOffsetY);
	int	iWidth = pDialog->iWidth;

	cairo_rel_line_to (pCairoContext, iWidth - (2 *	fRadius + fLineWidth), 0);
	// Coin	haut droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		fRadius, 0,
		fRadius, sens *	fRadius);
	cairo_rel_line_to (pCairoContext, 0, sens *	(pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin - (2 * fRadius + fLineWidth)));
	// Coin	bas	droit.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0, sens	* fRadius,
		-fRadius, sens * fRadius);
	// La pointe.
	double fDeltaMargin;
	if (pDialog->bRight)
	{
		fDeltaMargin = MAX (0, pDialog->iAimedX	- pDialog->iPositionX -	fRadius	- fLineWidth / 2);
		//g_print ("fDeltaMargin : %.2f\n",	fDeltaMargin);
		cairo_rel_line_to (pCairoContext, -iWidth +	fDeltaMargin + fLineWidth +	2 * fRadius + CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE + CAIRO_DIALOG_TIP_ROUNDING_MARGIN ,	0);	
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			- CAIRO_DIALOG_TIP_ROUNDING_MARGIN,	0,
			- (CAIRO_DIALOG_TIP_ROUNDING_MARGIN	+ CAIRO_DIALOG_TIP_MARGIN +	CAIRO_DIALOG_TIP_BASE),	sens * fTipHeight);
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			CAIRO_DIALOG_TIP_MARGIN, - sens	* fTipHeight,
			CAIRO_DIALOG_TIP_MARGIN	- CAIRO_DIALOG_TIP_ROUNDING_MARGIN,	- sens * fTipHeight);
		cairo_rel_line_to (pCairoContext, -	CAIRO_DIALOG_TIP_MARGIN	- fDeltaMargin + CAIRO_DIALOG_TIP_ROUNDING_MARGIN, 0);
	}
	else
	{
		fDeltaMargin = MAX (0, MIN (- CAIRO_DIALOG_TIP_MARGIN -	CAIRO_DIALOG_TIP_ROUNDING_MARGIN - CAIRO_DIALOG_TIP_BASE - fRadius - fLineWidth / 2, pDialog->iPositionX - pDialog->iAimedX	- fRadius -	fLineWidth / 2)	+ pDialog->iWidth);
		//g_print ("fDeltaMargin : %.2f	/ %d\n", fDeltaMargin, pDialog->iWidth);
		cairo_rel_line_to (pCairoContext, -	(CAIRO_DIALOG_TIP_MARGIN + fDeltaMargin) + CAIRO_DIALOG_TIP_ROUNDING_MARGIN, 0);
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			-CAIRO_DIALOG_TIP_ROUNDING_MARGIN, 0,
			CAIRO_DIALOG_TIP_MARGIN	- CAIRO_DIALOG_TIP_ROUNDING_MARGIN,	sens * fTipHeight);
		cairo_rel_curve_to (pCairoContext,
			0, 0,
			- (CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE), - sens	* fTipHeight,
			- (CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE)	- CAIRO_DIALOG_TIP_ROUNDING_MARGIN,	- sens * fTipHeight);
		cairo_rel_line_to (pCairoContext, -iWidth +	fDeltaMargin + fLineWidth +	2 *	fRadius	+ CAIRO_DIALOG_TIP_MARGIN +	CAIRO_DIALOG_TIP_BASE +	CAIRO_DIALOG_TIP_ROUNDING_MARGIN, 0);
	}

	// Coin	bas	gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		-fRadius, 0,
		-fRadius, -sens	* fRadius);
	cairo_rel_line_to (pCairoContext, 0, - sens * (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin - (2 * fRadius + fLineWidth)));
	// Coin	haut gauche.
	cairo_rel_curve_to (pCairoContext,
		0, 0,
		0, -sens * fRadius,
		fRadius, -sens * fRadius);
	if (fRadius	< 1)
		cairo_close_path (pCairoContext);

	cairo_set_source_rgba (pCairoContext, myDialogs.fDialogColor[0], myDialogs.fDialogColor[1],	myDialogs.fDialogColor[2], myDialogs.fDialogColor[3]);
	cairo_fill_preserve (pCairoContext);

	cairo_set_line_width (pCairoContext, fLineWidth);
	cairo_set_source_rgba (pCairoContext, myDialogs.fLineColor[0], myDialogs.fLineColor[1], myDialogs.fLineColor[2], myDialogs.fLineColor[3]);
	cairo_stroke (pCairoContext);
}



void cairo_dock_set_frame_size_modern (CairoDialog *pDialog)
{
	double fRadius = myDialogs.iCornerRadius;
	double fLineWidth = myDialogs.iLineWidth;
	int iMargin = .5 * fLineWidth + .5 * fRadius;
	pDialog->iRightMargin = iMargin;
	pDialog->iLeftMargin = iMargin;
	pDialog->iTopMargin = iMargin;
	pDialog->iBottomMargin = iMargin;
	pDialog->iMinBottomGap = 30;
	pDialog->iMinFrameWidth = CAIRO_DIALOG_TIP_BASE + CAIRO_DIALOG_TIP_MARGIN + 2 * iMargin;
	pDialog->fAlign = .33;
}

void cairo_dock_draw_decorations_modern (cairo_t *pCairoContext, CairoDialog *pDialog)
{
	double fLineWidth = myDialogs.iLineWidth;
	double fRadius = MIN (myDialogs.iCornerRadius, pDialog->iBubbleHeight/2);
	int sens = (pDialog->bDirectionUp ?	1 :	-1);
	int sens2 = (pDialog->bRight ? 1 :	-1);
	
	//\_________________ On part du haut.
	double fOffsetX = (pDialog->bRight ? fLineWidth/2 : pDialog->iWidth - fLineWidth/2);
	double fOffsetY = (pDialog->bDirectionUp ? 0. : pDialog->iHeight);
	cairo_move_to (pCairoContext, fOffsetX, fOffsetY);
	
	//\_________________ On remplit le fond.
	cairo_rel_line_to (pCairoContext,
		0.,
		sens * (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin - fRadius));
	cairo_rel_line_to (pCairoContext,
		sens2 * fRadius,
		sens * fRadius);
	cairo_rel_line_to (pCairoContext,
		sens2 * pDialog->iBubbleWidth,
		0.);
	cairo_set_line_width (pCairoContext, fLineWidth);
	cairo_set_source_rgba (pCairoContext, myDialogs.fLineColor[0], myDialogs.fLineColor[1], myDialogs.fLineColor[2], myDialogs.fLineColor[3]);
	
	cairo_rel_line_to (pCairoContext,
		0.,
		- sens * (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin - fRadius));
	cairo_rel_line_to (pCairoContext,
		- sens2 * fRadius,
		- sens * fRadius);
	cairo_close_path (pCairoContext);
	cairo_set_source_rgba (pCairoContext, myDialogs.fDialogColor[0], myDialogs.fDialogColor[1],	myDialogs.fDialogColor[2], myDialogs.fDialogColor[3]);
	cairo_fill (pCairoContext);
	
	//\_________________ On trace le cadre.
	cairo_move_to (pCairoContext, fOffsetX, fOffsetY);
	cairo_rel_line_to (pCairoContext,
		0.,
		sens * (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin - fRadius));
	cairo_rel_line_to (pCairoContext,
		sens2 * fRadius,
		sens * fRadius);
	cairo_rel_line_to (pCairoContext,
		sens2 * pDialog->iBubbleWidth,
		0.);
	cairo_set_line_width (pCairoContext, fLineWidth);
	cairo_set_source_rgba (pCairoContext, myDialogs.fLineColor[0], myDialogs.fLineColor[1], myDialogs.fLineColor[2], myDialogs.fLineColor[3]);
	cairo_stroke (pCairoContext);
	
	//\_________________ On part du haut, petit cote.
	fOffsetX = (pDialog->bRight ? fRadius + fLineWidth/2 : pDialog->iWidth - fRadius - fLineWidth/2);
	fOffsetY = (pDialog->bDirectionUp ? pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin : pDialog->iHeight - (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin));
	
	//\_________________ On trace la pointe.
	cairo_set_line_width (pCairoContext, 1.);
	cairo_set_source_rgba (pCairoContext, myDialogs.fLineColor[0], myDialogs.fLineColor[1], myDialogs.fLineColor[2], myDialogs.fLineColor[3]);
	int i, h = pDialog->iHeight - (pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin);
	double w1 = MAX (0, pDialog->iAimedX - pDialog->iPositionX - (pDialog->bRight ? fOffsetX : 0));
	double w2 = MAX (0, pDialog->iPositionX + pDialog->iWidth - pDialog->iAimedX - (pDialog->bRight ? 0 : fRadius + fLineWidth/2));
	g_print ("%.2f,%.2f ; %d + %d > %d\n", w1, w2, pDialog->iPositionX, pDialog->iWidth, pDialog->iAimedX);
	double x, y, w;
	for (i = 0; i < h; i += 4)
	{
		y = fOffsetY + sens * i;
		x = fOffsetX + 1. * sens2 * i / h * (pDialog->bRight ? w1 : w2);
		w = (w1 + w2) * (h - i) / h;
		cairo_move_to (pCairoContext, x, y);
		cairo_rel_line_to (pCairoContext,
			sens2 * w,
			0.);
		cairo_stroke (pCairoContext);
	}
	if (h-i > 1)
	{
		y = fOffsetY + sens * h;
		x = fOffsetX + 1. * sens2 * (pDialog->bRight ? w1 : w2);
		w = MIN (w/2, 15);
		cairo_move_to (pCairoContext, x, y);
		cairo_rel_line_to (pCairoContext,
			sens2 * w,
			0.);
		cairo_stroke (pCairoContext);
	}
}

