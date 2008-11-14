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
#include "cairo-dock-dialogs.h"

static GSList *s_pDialogList = NULL;
static GStaticRWLock s_mDialogsMutex = G_STATIC_RW_LOCK_INIT;

extern CairoDock *g_pMainDock;
extern gint g_iScreenWidth[2], g_iScreenHeight[2];
extern gboolean g_bSticky;
extern gboolean g_bKeepAbove;
extern gboolean g_bPopUp;
extern int g_iVisibleZoneWidth, g_iVisibleZoneHeight;

extern int g_iDockLineWidth;
extern int g_iDockRadius;
extern double g_fLineColor[4];
extern double g_fAmplitude;

extern int g_iDialogButtonWidth;
extern int g_iDialogButtonHeight;
extern double g_fDialogColor[4];
extern int g_iDialogIconSize;

extern CairoDockLabelDescription g_dialogTextDescription;

static cairo_surface_t *s_pButtonOkSurface = NULL;
static cairo_surface_t *s_pButtonCancelSurface = NULL;

#define CAIRO_DIALOG_DEFAULT_GAP 20
#define CAIRO_DIALOG_TEXT_MARGIN 0
#define CAIRO_DIALOG_TIP_ROUNDING_MARGIN 12
#define CAIRO_DIALOG_TIP_MARGIN 25
#define CAIRO_DIALOG_TIP_BASE 25

#define CAIRO_DIALOG_MIN_ENTRY_WIDTH 120
#define CAIRO_DIALOG_MAX_ENTRY_WIDTH 300
#define CAIRO_DIALOG_BUTTON_OFFSET 3
#define CAIRO_DIALOG_HGAP 2
#define CAIRO_DIALOG_VGAP 4
#define CAIRO_DIALOG_BUTTON_GAP 16

// fRadius + .5*fLineWidth
#define dx(fRadius, fLineWidth) ceil (fLineWidth + (1. - sqrt (2) / 2) * MAX (0, fRadius - .5*fLineWidth))
// fLineWidth
#define dy dx

gboolean on_enter_dialog (GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDialog *pDialog)
{
	if (! cairo_dock_dialog_reference (pDialog))
		return FALSE;
	cd_debug ("inside");
	pDialog->bInside = TRUE;
	cairo_dock_dialog_unreference (pDialog);
	return FALSE;
}

static gboolean on_leave_dialog (GtkWidget* pWidget,
	GdkEventCrossing* pEvent,
	CairoDialog *pDialog)
{
	if (! cairo_dock_dialog_reference (pDialog))
		return FALSE;

	//cd_debug ("%s (%d/%d)", __func__, pDialog->iButtonOkOffset, pDialog->iButtonCancelOffset);

	/*while (gtk_events_pending ())
		gtk_main_iteration ();
	cd_message ("fin d'attente, bInside : %d\n", pDialog->bInside);*/
	int iMouseX, iMouseY;
	gdk_window_get_pointer (pDialog->pWidget->window, &iMouseX, &iMouseY, NULL);
	if (iMouseX > 0 && iMouseX < pDialog->iWidth && iMouseY > 0 && iMouseY < pDialog->iHeight)
	{
		cd_debug ("en fait on est dedans");
		cairo_dock_dialog_unreference (pDialog);
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

	cairo_dock_dialog_unreference (pDialog);
	return FALSE;
}

static int _cairo_dock_find_clicked_button_in_dialog (GdkEventButton* pButton, CairoDialog *pDialog)
{
	GtkRequisition requisition = {0, 0};
	if (pDialog->pInteractiveWidget != NULL)
		gtk_widget_size_request (pDialog->pInteractiveWidget, &requisition);

	int iButtonX = .5*pDialog->iWidth - g_iDialogButtonWidth - .5*CAIRO_DIALOG_BUTTON_GAP;
	///int iButtonY = g_iDockLineWidth + pDialog->iMessageHeight + requisition.height + CAIRO_DIALOG_VGAP;
	int iButtonY = pDialog->iMargin + pDialog->iMessageHeight + pDialog->iInteractiveHeight + 0*CAIRO_DIALOG_VGAP;
	if (! pDialog->bDirectionUp)
		iButtonY +=  pDialog->iHeight - (pDialog->iBubbleHeight + pDialog->iMargin);

	//g_print ("clic (%d;%d) bouton Ok (%d;%d)\n", (int) pButton->x, (int) pButton->y, iButtonX, iButtonY);
	if (pButton->x >= iButtonX && pButton->x <= iButtonX + g_iDialogButtonWidth && pButton->y >= iButtonY && pButton->y <= iButtonY + g_iDialogButtonHeight)
	{
		return GTK_BUTTONS_OK;
	}
	else
	{
		iButtonX = .5*pDialog->iWidth + .5*CAIRO_DIALOG_BUTTON_GAP;
		//g_print ("clic (%d;%d) bouton Cancel (%d;%d)\n", (int) pButton->x, (int) pButton->y, iButtonX, iButtonY);
		if (pButton->x >= iButtonX && pButton->x <= iButtonX + g_iDialogButtonWidth && pButton->y >= iButtonY && pButton->y <= iButtonY + g_iDialogButtonHeight)
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
	if (! cairo_dock_dialog_reference (pDialog))
		return FALSE;

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

	cairo_dock_dialog_unreference (pDialog);
	return FALSE;
}
gboolean on_key_press_dialog (GtkWidget *pWidget,
	GdkEventKey *pKey,
	CairoDialog *pDialog)
{
	cd_debug ("key pressed");
	if (! cairo_dock_dialog_reference (pDialog))
		return FALSE;
	if (pDialog->iButtonsType == GTK_BUTTONS_NONE)
	{
		cairo_dock_dialog_unreference (pDialog);
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
	cairo_dock_dialog_unreference (pDialog);
	return FALSE;
}
static gboolean on_expose_dialog (GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	CairoDialog *pDialog)
{
	//cd_message ("%s (%dx%d)", __func__, pDialog->iWidth, pDialog->iHeight);
	if (! cairo_dock_dialog_reference (pDialog))
		return FALSE;

	double fLineWidth = g_iDockLineWidth;
	double fRadius = pDialog->fRadius;

	cairo_t *pCairoContext;
	
	if (pExpose->area.x >= pDialog->iMargin + CAIRO_DIALOG_TEXT_MARGIN && pExpose->area.y >= pDialog->iMargin + CAIRO_DIALOG_TEXT_MARGIN && pExpose->area.width <= pDialog->iIconSize && pExpose->area.height <= pDialog->iIconSize)  // redessin de l'icone.
	{
		pCairoContext = cairo_dock_create_drawing_context_on_area (CAIRO_CONTAINER (pDialog), &pExpose->area, g_fDialogColor);
		
		cairo_set_source_surface (pCairoContext, pDialog->pIconBuffer,
			pDialog->iMargin + CAIRO_DIALOG_TEXT_MARGIN,
			pDialog->iMargin + CAIRO_DIALOG_TEXT_MARGIN);
		cairo_paint (pCairoContext);
	}
	else if (pExpose->area.x >= pDialog->iMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN && pExpose->area.y <= pDialog->iMessageHeight && pExpose->area.width <= pDialog->iMessageWidth && pExpose->area.height <= pDialog->iMessageHeight)  // redessin du texte.
	{
		pCairoContext = cairo_dock_create_drawing_context_on_area (CAIRO_CONTAINER (pDialog), &pExpose->area, g_fDialogColor);
		
		cairo_set_source_surface (pCairoContext, pDialog->pTextBuffer,
			pDialog->iMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN,
			pDialog->iMargin + CAIRO_DIALOG_TEXT_MARGIN + (pDialog->iIconSize > pDialog->iTextHeight ? (pDialog->iIconSize - pDialog->iTextHeight) / 2: 0));
		cairo_paint (pCairoContext);
	}
	else
	{
		if (pDialog->iMessageHeight != 0 && pExpose->area.y >= pDialog->iMessageHeight)  // redessin du widget interactif.
		{
			pCairoContext = cairo_dock_create_drawing_context_on_area (CAIRO_CONTAINER (pDialog), &pExpose->area, g_fDialogColor);
		}
		else
		{
			if (pExpose->area.x > 0 || pExpose->area.y > 0)
				pCairoContext = cairo_dock_create_drawing_context_on_area (CAIRO_CONTAINER (pDialog), &pExpose->area, NULL);
			else
				pCairoContext = cairo_dock_create_drawing_context (CAIRO_CONTAINER (pDialog));
			
			if (pDialog->iWidth == 20 && pDialog->iHeight == 20)
			{
				//cd_debug ("dialogue incomplet");
				cairo_destroy (pCairoContext);
				cairo_dock_dialog_unreference (pDialog);
				return FALSE;
			}
		
			cairo_save (pCairoContext);
			double fOffsetX = fRadius + fLineWidth / 2;
			double fOffsetY = (pDialog->bDirectionUp ? fLineWidth / 2 : pDialog->iHeight - .5*fLineWidth);
			int sens = (pDialog->bDirectionUp ? 1 : -1);
			cairo_move_to (pCairoContext, fOffsetX, fOffsetY);
			//g_print ("  fOffsetX : %.2f; fOffsetY : %.2f\n", fOffsetX, fOffsetY);
			int iWidth = pDialog->iWidth;
		
			cairo_rel_line_to (pCairoContext, iWidth - (2 * fRadius + fLineWidth), 0);
			// Coin haut droit.
			cairo_rel_curve_to (pCairoContext,
				0, 0,
				fRadius, 0,
				fRadius, sens * fRadius);
			cairo_rel_line_to (pCairoContext, 0, sens * (pDialog->iBubbleHeight + 2 * pDialog->iMargin - 2 * fRadius));
			// Coin bas droit.
			cairo_rel_curve_to (pCairoContext,
				0, 0,
				0, sens * fRadius,
				-fRadius, sens * fRadius);
			// La pointe.
			double fDeltaMargin;
			if (pDialog->bRight)
			{
				fDeltaMargin = MAX (0, pDialog->iAimedX - pDialog->iPositionX - fRadius - fLineWidth / 2);
				//g_print ("fDeltaMargin : %.2f\n", fDeltaMargin);
				cairo_rel_line_to (pCairoContext, -iWidth + fDeltaMargin + fLineWidth + 2. * fRadius + CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE + CAIRO_DIALOG_TIP_ROUNDING_MARGIN , 0); 
				cairo_rel_curve_to (pCairoContext,
					0, 0,
					- CAIRO_DIALOG_TIP_ROUNDING_MARGIN, 0,
					- (CAIRO_DIALOG_TIP_ROUNDING_MARGIN + CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE), sens * pDialog->fTipHeight);
				cairo_rel_curve_to (pCairoContext,
					0, 0,
					CAIRO_DIALOG_TIP_MARGIN, - sens * pDialog->fTipHeight,
					CAIRO_DIALOG_TIP_MARGIN - CAIRO_DIALOG_TIP_ROUNDING_MARGIN, - sens * pDialog->fTipHeight);
				cairo_rel_line_to (pCairoContext, - CAIRO_DIALOG_TIP_MARGIN - fDeltaMargin + CAIRO_DIALOG_TIP_ROUNDING_MARGIN, 0);
			}
			else
			{
				fDeltaMargin = MAX (0, MIN (- CAIRO_DIALOG_TIP_MARGIN - CAIRO_DIALOG_TIP_ROUNDING_MARGIN - CAIRO_DIALOG_TIP_BASE - fRadius - fLineWidth / 2, pDialog->iPositionX - pDialog->iAimedX - fRadius - fLineWidth / 2) + pDialog->iWidth);
				//g_print ("fDeltaMargin : %.2f / %d\n", fDeltaMargin, pDialog->iWidth);
				cairo_rel_line_to (pCairoContext, - (CAIRO_DIALOG_TIP_MARGIN + fDeltaMargin) + CAIRO_DIALOG_TIP_ROUNDING_MARGIN, 0);
				cairo_rel_curve_to (pCairoContext,
					0, 0,
					-CAIRO_DIALOG_TIP_ROUNDING_MARGIN, 0,
					CAIRO_DIALOG_TIP_MARGIN - CAIRO_DIALOG_TIP_ROUNDING_MARGIN, sens * pDialog->fTipHeight);
				cairo_rel_curve_to (pCairoContext,
					0, 0,
					- (CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE), - sens * pDialog->fTipHeight,
					- (CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE) - CAIRO_DIALOG_TIP_ROUNDING_MARGIN, - sens * pDialog->fTipHeight);
				cairo_rel_line_to (pCairoContext, -iWidth + fDeltaMargin + fLineWidth + 2 * fRadius + CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE + CAIRO_DIALOG_TIP_ROUNDING_MARGIN, 0);
			}
		
			// Coin bas gauche.
			cairo_rel_curve_to (pCairoContext,
				0, 0,
				-fRadius, 0,
				-fRadius, -sens * fRadius);
			cairo_rel_line_to (pCairoContext, 0, sens * (- pDialog->iBubbleHeight - 2 * pDialog->iMargin + fRadius * 2));
			// Coin haut gauche.
			cairo_rel_curve_to (pCairoContext,
				0, 0,
				0, -sens * fRadius,
				fRadius, -sens * fRadius);
			if (fRadius < 1)
				cairo_close_path (pCairoContext);
		
			cairo_set_source_rgba (pCairoContext, g_fDialogColor[0], g_fDialogColor[1], g_fDialogColor[2], g_fDialogColor[3]);
			cairo_fill_preserve (pCairoContext);
		
			cairo_set_line_width (pCairoContext, fLineWidth);
			cairo_set_source_rgba (pCairoContext, g_fLineColor[0], g_fLineColor[1], g_fLineColor[2], g_fLineColor[3]);
			cairo_stroke (pCairoContext);
			cairo_restore (pCairoContext);  // retour au contexte initial.
		
			cairo_save (pCairoContext);
			fOffsetX = pDialog->iMargin + CAIRO_DIALOG_TEXT_MARGIN;
			fOffsetY = (pDialog->bDirectionUp ? pDialog->iMargin : pDialog->iHeight - pDialog->iMargin - pDialog->iBubbleHeight) + CAIRO_DIALOG_TEXT_MARGIN;  // pDialog->fTipHeight + pDialog->iMargin
			cairo_translate (pCairoContext, fOffsetX, fOffsetY);
			///cairo_move_to (pCairoContext, fOffsetX, fOffsetY);
			
			if (pDialog->pIconBuffer != NULL)
			{
				///cairo_set_source_surface (pCairoContext, pDialog->pTextBuffer, fOffsetX + CAIRO_DIALOG_TEXT_MARGIN, fOffsetY + CAIRO_DIALOG_TEXT_MARGIN);
				cairo_set_source_surface (pCairoContext, pDialog->pIconBuffer, 0., 0.);
				cairo_paint (pCairoContext);
				cairo_translate (pCairoContext, pDialog->iIconSize, (pDialog->iIconSize > pDialog->iTextHeight ? (pDialog->iIconSize - pDialog->iTextHeight) / 2: 0));
			}
			if (pDialog->pTextBuffer != NULL)
			{
				///cairo_set_source_surface (pCairoContext, pDialog->pTextBuffer, fOffsetX + CAIRO_DIALOG_TEXT_MARGIN, fOffsetY + CAIRO_DIALOG_TEXT_MARGIN);
				cairo_set_source_surface (pCairoContext, pDialog->pTextBuffer, 0., 0.);
				cairo_paint (pCairoContext);
			}
			cairo_restore (pCairoContext);  // retour au contexte initial.
		
			if (pDialog->iButtonsType != GTK_BUTTONS_NONE)
			{
				GtkRequisition requisition = {0, 0};
				if (pDialog->pInteractiveWidget != NULL)
					gtk_widget_size_request (pDialog->pInteractiveWidget, &requisition);
				//cd_message (" pInteractiveWidget : %dx%d", requisition.width, requisition.height);
		
				int iButtonY = pDialog->iMargin + pDialog->iMessageHeight + pDialog->iInteractiveHeight + 0*CAIRO_DIALOG_VGAP;  // requisition.height
				if (! pDialog->bDirectionUp)
					iButtonY +=  pDialog->iHeight - (pDialog->iBubbleHeight + pDialog->iMargin);
				//g_print (" -> iButtonY : %d\n", iButtonY);
		
				cairo_set_source_surface (pCairoContext, s_pButtonOkSurface, .5*pDialog->iWidth - g_iDialogButtonWidth - .5*CAIRO_DIALOG_BUTTON_GAP + pDialog->iButtonOkOffset, iButtonY + pDialog->iButtonOkOffset);
				cairo_paint (pCairoContext);
		
				cairo_set_source_surface (pCairoContext, s_pButtonCancelSurface, .5*pDialog->iWidth + .5*CAIRO_DIALOG_BUTTON_GAP + pDialog->iButtonCancelOffset, iButtonY + pDialog->iButtonCancelOffset);
				cairo_paint (pCairoContext);
			}
		}
		
		if (pDialog->pRenderer != NULL)  // un moteur de rendu specifique a ete fourni pour le widget interactif.
		{
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_OVER);
			if (pDialog->pRenderer->render != NULL)
				pDialog->pRenderer->render (pCairoContext, pDialog);
		}
	}
	
	
	cairo_destroy (pCairoContext);
	cairo_dock_dialog_unreference (pDialog);
	//g_print ("fin du dessin\n");
	return FALSE;
}


static gboolean on_configure_dialog (GtkWidget* pWidget,
	GdkEventConfigure* pEvent,
	CairoDialog *pDialog)
{
	//g_print ("%s (%dx%d)\n", __func__, pEvent->width, pEvent->height);
	if (! cairo_dock_dialog_reference (pDialog))
		return FALSE;

	//\____________ On recupere la taille du widget interactif qui a pu avoir change.
	if (pDialog->pInteractiveWidget != NULL)
	{
		GtkRequisition requisition;
		gtk_widget_size_request (pDialog->pInteractiveWidget, &requisition);
		pDialog->iInteractiveWidth = requisition.width;
		pDialog->iInteractiveHeight = requisition.height;
		//g_print ("  pInteractiveWidget : %dx%d\n", pDialog->iInteractiveWidth, pDialog->iInteractiveHeight);

		pDialog->iBubbleWidth = MAX (pDialog->iMessageWidth, MAX (pDialog->iInteractiveWidth, pDialog->iBubbleWidth));
		pDialog->iBubbleHeight = pDialog->iMessageHeight + pDialog->iInteractiveHeight + pDialog->iButtonsHeight;
		//g_print (" -> iBubbleWidth: %d , iBubbleHeight : %d\n", pDialog->iBubbleWidth, pDialog->iBubbleHeight);
	}

	if ((pDialog->iWidth != pEvent->width || pDialog->iHeight != pEvent->height))
	{
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

	cairo_dock_dialog_unreference (pDialog);
	return FALSE;
}

static cairo_surface_t *_cairo_dock_load_button_icon (cairo_t *pCairoContext, gchar *cButtonImage, gchar *cDefaultButtonImage)
{
	//g_print ("%s (%d ; %d)\n", __func__, g_iDialogButtonWidth, g_iDialogButtonHeight);
	cairo_surface_t *pButtonSurface = cairo_dock_load_image_for_icon (pCairoContext,
		cButtonImage,
		g_iDialogButtonWidth,
		g_iDialogButtonHeight);

	if (pButtonSurface == NULL)
	{
		gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, cDefaultButtonImage);
		//g_print ("  on charge %s par defaut\n", cIconPath);
		pButtonSurface = cairo_dock_load_image_for_icon (pCairoContext,
			cIconPath,
			g_iDialogButtonWidth,
			g_iDialogButtonHeight);
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
	if (pDialog != NULL)
	{
		if (g_atomic_int_exchange_and_add (&pDialog->iRefCount, 1) > 0)  // il etait > 0 avant l'incrementation.  // && pDialog->pIcon != NULL
			return TRUE;  // on peut l'utiliser.
		else
		{
			//g_atomic_int_add (&pDialog->iRefCount, -1);
			cairo_dock_dialog_unreference (pDialog);
			return FALSE;  // il etait deja en sursis, on en profite pour lui balancer quelques coups de pieds, puis on s'en va :-)
		}
	}
	return FALSE;
}

gboolean cairo_dock_dialog_unreference (CairoDialog *pDialog)
{
	//g_print ("%s (%d)\n", __func__, pDialog->iRefCount);
	if (pDialog != NULL && pDialog->iRefCount > 0)
	{
		if (g_atomic_int_dec_and_test (&pDialog->iRefCount))  // devient nul.
		{
			cairo_dock_isolate_dialog (pDialog);
			if (g_static_rw_lock_writer_trylock (&s_mDialogsMutex))
			{
				cairo_dock_free_dialog (pDialog);
				g_static_rw_lock_writer_unlock (&s_mDialogsMutex);
			}
			return TRUE;
		}
		else
			return FALSE;
	}
	return TRUE;
}

void cairo_dock_isolate_dialog (CairoDialog *pDialog)
{
	cd_debug ("");
	if (pDialog == NULL)
		return ;

	if (pDialog->iSidTimer > 0)
	{
		g_source_remove (pDialog->iSidTimer);
		pDialog->iSidTimer = 0;
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

void cairo_dock_free_dialog (CairoDialog *pDialog)
{
	if (pDialog == NULL)
		return ;

	cd_debug ("");
	s_pDialogList = g_slist_remove (s_pDialogList, pDialog);

	cairo_surface_destroy (pDialog->pTextBuffer);
	pDialog->pTextBuffer = NULL;

	gtk_widget_destroy (pDialog->pWidget);  // detruit aussi le widget interactif.
	pDialog->pWidget = NULL;

	if (pDialog->pUserData != NULL && pDialog->pFreeUserDataFunc != NULL)
		pDialog->pFreeUserDataFunc (pDialog->pUserData);
	
	cairo_dock_set_dialog_renderer (pDialog, NULL, NULL, NULL);
	
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
	g_static_rw_lock_writer_lock (&s_mDialogsMutex);

	ic = s_pDialogList;
	do
	{
		if (ic->next == NULL)
			break;

		pDialog = ic->next->data;  // on ne peut pas enlever l'element courant, sinon on perd 'ic'.
		if (pDialog->pIcon == icon)
		{
			if (cairo_dock_dialog_unreference (pDialog))
				cairo_dock_free_dialog (pDialog);  // la liberation n'a pas pu se faire a cause du lock.
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
		if (cairo_dock_dialog_unreference (pDialog))
			cairo_dock_free_dialog (pDialog);  // la liberation n'a pas pu se faire a cause du lock.
		bDialogRemoved = TRUE;
	}

	g_static_rw_lock_writer_unlock (&s_mDialogsMutex);
	return bDialogRemoved;
}

void cairo_dock_remove_orphelans (void)
{
	CairoDialog *pDialog;
	GSList *ic;

	if (s_pDialogList == NULL)
		return ;
	g_static_rw_lock_writer_lock (&s_mDialogsMutex);

	ic = s_pDialogList;
	do
	{
		if (ic->next == NULL)
			break;

		pDialog = ic->next->data;  // on ne peut pas enlever l'element courant, sinon on perd 'ic'.
		if (pDialog->iRefCount == 0 && pDialog->pIcon == NULL)  // plus utilise, meme pas en cours d'isolation.
		{
			cairo_dock_free_dialog (pDialog);
		}
		else
		{
			ic = ic->next;
		}
	} while (TRUE);

	pDialog = s_pDialogList->data;
	if (pDialog != NULL && pDialog->iRefCount == 0 && pDialog->pIcon == NULL)
	{
		cairo_dock_free_dialog (pDialog);  // la liberation n'a pas pu se faire a cause du lock.
	}

	g_static_rw_lock_writer_unlock (&s_mDialogsMutex);
}


GtkWidget *cairo_dock_build_common_interactive_widget_for_dialog (const gchar *cInitialAnswer, double fValueForHScale, double fMaxValueForHScale)
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
}


CairoDialog *cairo_dock_build_dialog (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cImageFilePath, GtkWidget *pInteractiveWidget, GtkButtonsType iButtonsType, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	cd_message ("%s (%s)", __func__, cText);

	//\________________ On cree un dialogue qu'on insere immediatement dans la liste.
	CairoDialog *pDialog = g_new0 (CairoDialog, 1);
	pDialog->iType = CAIRO_DOCK_TYPE_DIALOG;
	pDialog->iRefCount = 2;  // on l'enleve a la fin.
	pDialog->pIcon = pIcon;
	g_static_rw_lock_writer_lock (&s_mDialogsMutex);
	s_pDialogList = g_slist_prepend (s_pDialogList, pDialog);
	g_static_rw_lock_writer_unlock (&s_mDialogsMutex);

	//\________________ On construit la fenetre du dialogue.
	if (pActionFunc == NULL)  // pas d'action, pas de bouton.
		iButtonsType = GTK_BUTTONS_NONE;
	gboolean bInteractiveWindow = (iButtonsType != GTK_BUTTONS_NONE || pInteractiveWidget != NULL);  // il y'aura des boutons ou un widget interactif, donc la fenetre doit pouvoir recevoir les evenements utilisateur.
	GtkWidget* pWindow = gtk_window_new (bInteractiveWindow ? GTK_WINDOW_TOPLEVEL : GTK_WINDOW_POPUP);  // les popus ne prennent pas le focus. En fait, ils ne sont meme pas controles par le WM.
	pDialog->pWidget = pWindow;

	if (g_bSticky)
		gtk_window_stick (GTK_WINDOW (pWindow));
	gtk_window_set_keep_above (GTK_WINDOW (pWindow), g_bKeepAbove || g_bPopUp);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_gravity (GTK_WINDOW (pWindow), GDK_GRAVITY_STATIC);

	if (bInteractiveWindow)
		GTK_WIDGET_SET_FLAGS (pWindow, GTK_CAN_FOCUS);  // a priori inutile mais bon.
	else
		GTK_WIDGET_UNSET_FLAGS (pWindow, GTK_CAN_FOCUS);  // pareil, mais bon on ne sait jamais avec ces WM.

	cairo_dock_set_colormap_for_window (pWindow);

	gtk_widget_set_app_paintable (pWindow, TRUE);
	gtk_window_set_decorated (GTK_WINDOW (pWindow), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (pWindow), TRUE);
	gtk_window_set_title (GTK_WINDOW (pWindow), "cairo-dock-dialog");

	gtk_widget_add_events (pWindow, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_window_resize(GTK_WINDOW (pWindow), CAIRO_DIALOG_DEFAULT_GAP, CAIRO_DIALOG_DEFAULT_GAP);
	gtk_widget_show_all (pWindow);

	GtkWidget *pWidgetLayout = gtk_vbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (pWindow), pWidgetLayout);

	//\________________ On dessine le texte dans une surface tampon.
	cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDialog));
	cairo_set_source_rgba (pSourceContext, 0., 0., 0., 0.);
	cairo_set_operator (pSourceContext, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pSourceContext);

	if (cText != NULL)
	{
		PangoRectangle ink, log;
		PangoLayout *pLayout = pango_cairo_create_layout (pSourceContext);

		PangoFontDescription *pDesc = pango_font_description_new ();
		pango_font_description_set_absolute_size (pDesc, g_dialogTextDescription.iSize * PANGO_SCALE);
		pango_font_description_set_family_static (pDesc, g_dialogTextDescription.cFont);
		pango_font_description_set_weight (pDesc, g_dialogTextDescription.iWeight);
		pango_font_description_set_style (pDesc, g_dialogTextDescription.iStyle);
		pango_layout_set_font_description (pLayout, pDesc);
		pango_font_description_free (pDesc);

		pango_layout_set_text (pLayout, cText, -1);

		pango_layout_get_pixel_extents (pLayout, &ink, &log);

		pDialog->iMessageWidth = MAX (ink.width, CAIRO_DIALOG_TIP_BASE + CAIRO_DIALOG_TIP_MARGIN) + 2 * CAIRO_DIALOG_TEXT_MARGIN;
		pDialog->iMessageHeight = ink.height + 2 * CAIRO_DIALOG_TEXT_MARGIN;
		pDialog->iTextHeight = ink.height;
		pDialog->iBubbleWidth = pDialog->iMessageWidth;
		pDialog->iBubbleHeight = pDialog->iMessageHeight;
		cd_debug ("  1) iBubbleWidth: %d , iBubbleHeight : %d", pDialog->iBubbleWidth, pDialog->iBubbleHeight);
		
		pDialog->pTextBuffer = cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			ink.width,
			ink.height);
		cairo_t* pSurfaceContext = cairo_create (pDialog->pTextBuffer);
		
		cairo_translate (pSurfaceContext, -ink.x, -ink.y);
		cairo_set_source_rgba (pSurfaceContext, g_dialogTextDescription.fColorStart[0], g_dialogTextDescription.fColorStart[1], g_dialogTextDescription.fColorStart[2], 1.);
		pango_cairo_show_layout (pSurfaceContext, pLayout);
		
		cairo_destroy (pSurfaceContext);
	}

	//\________________ On recupere l'icone a afficher sur le cote.
	if (cImageFilePath != NULL)
	{
		if (g_iDialogIconSize == 0 && cText != NULL)
			pDialog->iIconSize = pDialog->iTextHeight;
		else
			pDialog->iIconSize = MAX (g_iDialogIconSize, 16);
		if (strcmp (cImageFilePath, "same icon") == 0)
		{
			double fMaxScale = cairo_dock_get_max_scale (pContainer);
			pDialog->pIconBuffer = cairo_dock_duplicate_surface (pIcon->pIconBuffer,
				pSourceContext,
				pIcon->fWidth * fMaxScale, pIcon->fHeight * fMaxScale,
				pDialog->iIconSize, pDialog->iIconSize);
		}
		else
		{
			pDialog->pIconBuffer = cairo_dock_load_image_for_square_icon (pSourceContext, cImageFilePath, pDialog->iIconSize);
		}
		pDialog->iMessageWidth += pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN;
		pDialog->iMessageHeight = MAX (pDialog->iMessageHeight, pDialog->iIconSize + 2 * CAIRO_DIALOG_TEXT_MARGIN);
		pDialog->iBubbleWidth = pDialog->iMessageWidth;
		pDialog->iBubbleHeight = pDialog->iMessageHeight;
		cd_debug ("  2) iBubbleWidth: %d , iBubbleHeight : %d", pDialog->iBubbleWidth, pDialog->iBubbleHeight);
	}
	
	//\________________ On cree la surface tampon avec ces 2 elements.
	if (pDialog->pTextBuffer != NULL || pDialog->pIconBuffer != NULL)
	{
		pDialog->pMessageWidget = gtk_hbox_new (0, FALSE);
		gtk_widget_set (pDialog->pMessageWidget, "height-request", pDialog->iMessageHeight, "width-request", pDialog->iMessageWidth, NULL);
		gtk_box_pack_start (GTK_BOX (pWidgetLayout),
			pDialog->pMessageWidget,
			FALSE,
			FALSE,
			0);
	}

	//\________________ On ajoute le widget interactif.
	pDialog->pInteractiveWidget = pInteractiveWidget;
	if (pInteractiveWidget != NULL)
	{
		if (pDialog->pMessageWidget != NULL)
		{
			pDialog->iMessageHeight += CAIRO_DIALOG_VGAP;
			gtk_widget_set (pDialog->pMessageWidget, "height-request", pDialog->iMessageHeight, NULL);
		}

		GtkRequisition requisition;
		gtk_widget_size_request (pInteractiveWidget, &requisition);
		pDialog->iInteractiveWidth = requisition.width;
		pDialog->iInteractiveHeight = requisition.height;
		//cd_debug (" pInteractiveWidget : %dx%d", pDialog->iInteractiveWidth, pDialog->iInteractiveHeight);

		pDialog->iBubbleWidth = MAX (pDialog->iBubbleWidth, pDialog->iInteractiveWidth);
		pDialog->iBubbleHeight += CAIRO_DIALOG_VGAP + pDialog->iInteractiveHeight;
		cd_debug ("  3) iBubbleWidth: %d , iBubbleHeight : %d", pDialog->iBubbleWidth, pDialog->iBubbleHeight);

		cd_debug (" ref = %d", pInteractiveWidget->object.parent_instance.ref_count);
		gtk_box_pack_start (GTK_BOX (pWidgetLayout),
			pInteractiveWidget,
			TRUE,
			TRUE,
			0);
		cd_debug (" pack -> ref = %d", pInteractiveWidget->object.parent_instance.ref_count);
	}

	//\________________ On ajoute les boutons.
	pDialog->action_on_answer = pActionFunc;
	pDialog->pUserData = data;
	pDialog->pFreeUserDataFunc = pFreeDataFunc;
	pDialog->iButtonsType = iButtonsType;
	if (pDialog->iButtonsType != GTK_BUTTONS_NONE)
	{
		if (s_pButtonOkSurface == NULL || s_pButtonCancelSurface == NULL)
			cairo_dock_load_dialog_buttons (CAIRO_CONTAINER (pDialog), NULL, NULL);
		
		pDialog->iButtonsWidth = 2 * g_iDialogButtonWidth + CAIRO_DIALOG_BUTTON_GAP + 2 * CAIRO_DIALOG_TEXT_MARGIN;
		pDialog->iButtonsHeight = CAIRO_DIALOG_VGAP + g_iDialogButtonHeight;

		pDialog->iBubbleWidth = MAX (pDialog->iBubbleWidth, pDialog->iButtonsWidth);
		pDialog->iBubbleHeight += pDialog->iButtonsHeight;
		cd_debug ("  4) iBubbleWidth: %d , iBubbleHeight : %d", pDialog->iBubbleWidth, pDialog->iBubbleHeight);

		pDialog->pButtonsWidget = gtk_hbox_new (0, FALSE);
		gtk_widget_set (pDialog->pButtonsWidget, "height-request", pDialog->iButtonsHeight, "width-request", pDialog->iButtonsWidth, NULL);
		gtk_box_pack_start (GTK_BOX (pWidgetLayout),
			pDialog->pButtonsWidget,
			FALSE,
			FALSE,
			0);
	}
	if (pDialog->iBubbleWidth == 0)
		pDialog->iBubbleWidth = 2*g_iDockRadius+g_iDockLineWidth;
	if (pDialog->iBubbleHeight == 0)
		pDialog->iBubbleHeight = 2*g_iDockRadius+g_iDockLineWidth;
	//pDialog->iBubbleWidth = MAX (pDialog->iBubbleWidth, 2*g_iDockRadius+g_iDockLineWidth);
	//pDialog->iBubbleHeight = MAX (pDialog->iBubbleHeight, 2*g_iDockRadius+g_iDockLineWidth);
	cd_debug ("=> iBubbleWidth: %d , iBubbleHeight : %d", pDialog->iBubbleWidth, pDialog->iBubbleHeight);

	//\________________ On definit la geometrie et la position de la fenetre globale.
	double fLineWidth = g_iDockLineWidth;
	pDialog->iMargin = dx(g_iDockRadius, fLineWidth);
	pDialog->fRadius = (pDialog->iBubbleHeight + 2*pDialog->iMargin > 2 * g_iDockRadius ? g_iDockRadius : (pDialog->iBubbleHeight + 2*pDialog->iMargin) / 2 - 1);  // on diminue le rayon si ca passera pas.

	gtk_container_set_border_width (GTK_CONTAINER (pWindow), pDialog->iMargin);

	pDialog->iWidth = pDialog->iBubbleWidth + 2 * pDialog->iMargin;
	pDialog->iHeight = pDialog->iBubbleHeight + 2 * pDialog->iMargin;  // resultat temporaire.

	pDialog->pTipWidget = gtk_hbox_new (0, FALSE);
	gtk_widget_set (pDialog->pTipWidget, "height-request", CAIRO_DIALOG_DEFAULT_GAP, NULL);
	gtk_box_pack_start (GTK_BOX (pWidgetLayout),
		pDialog->pTipWidget,
		TRUE,
		TRUE,
		0);
	
	gtk_widget_show_all (pWidgetLayout);
	
	//\________________ On connecte les signaux utiles.
	g_signal_connect (G_OBJECT (pWindow),
		"expose-event",
		G_CALLBACK (on_expose_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pWindow),
		"configure-event",
		G_CALLBACK (on_configure_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pWindow),
		"button-press-event",
		G_CALLBACK (on_button_press_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pWindow),
		"button-release-event",
		G_CALLBACK (on_button_press_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pWindow),
		"key-press-event",
		G_CALLBACK (on_key_press_dialog),
		pDialog);
	if (pIcon != NULL)  // on inhibe le deplacement du dialogue lorsque l'utilisateur est dedans.
	{
		g_signal_connect (G_OBJECT (pWindow),
			"enter-notify-event",
			G_CALLBACK (on_enter_dialog),
			pDialog);
		g_signal_connect (G_OBJECT (pWindow),
			"leave-notify-event",
			G_CALLBACK (on_leave_dialog),
			pDialog);
	}

	cairo_dock_place_dialog (pDialog, pContainer);  // renseigne aussi bDirectionUp, ! bIsHorizontal, et iHeight.
	cairo_dock_remove_orphelans ();  // la liste a ete verouillee par la fonction precedente pendant longtemps, empechant les dialogues d'etre detruits.

	cairo_dock_dialog_unreference (pDialog);
	return pDialog;
}

void cairo_dock_set_dialog_message (CairoDialog *pDialog, const gchar *cMessage)
{
	cairo_surface_destroy (pDialog->pTextBuffer);
	int iPrevTextHeight = pDialog->iTextHeight;
	int iPrevTextWidth = pDialog->iMessageWidth - (pDialog->iIconSize + 2 * CAIRO_DIALOG_TEXT_MARGIN);
	
	if (cMessage != NULL)
	{
		PangoRectangle ink, log;
		cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDialog));
		PangoLayout *pLayout = pango_cairo_create_layout (pSourceContext);

		PangoFontDescription *pDesc = pango_font_description_new ();
		pango_font_description_set_absolute_size (pDesc, g_dialogTextDescription.iSize * PANGO_SCALE);
		pango_font_description_set_family_static (pDesc, g_dialogTextDescription.cFont);
		pango_font_description_set_weight (pDesc, g_dialogTextDescription.iWeight);
		pango_font_description_set_style (pDesc, g_dialogTextDescription.iStyle);
		pango_layout_set_font_description (pLayout, pDesc);
		pango_font_description_free (pDesc);

		pango_layout_set_text (pLayout, cMessage, -1);

		pango_layout_get_pixel_extents (pLayout, &ink, &log);

		pDialog->iMessageWidth = MAX (ink.width, CAIRO_DIALOG_TIP_BASE + CAIRO_DIALOG_TIP_MARGIN) + pDialog->iIconSize + 2 * CAIRO_DIALOG_TEXT_MARGIN;
		pDialog->iMessageHeight = MAX (pDialog->iIconSize, ink.height) + 2 * CAIRO_DIALOG_TEXT_MARGIN;
		pDialog->iTextHeight = ink.height;
		
		pDialog->pTextBuffer = cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			ink.width,
			ink.height);
		cairo_t* pSurfaceContext = cairo_create (pDialog->pTextBuffer);
		
		cairo_translate (pSurfaceContext, -ink.x, -ink.y);
		cairo_set_source_rgba (pSurfaceContext, g_dialogTextDescription.fColorStart[0], g_dialogTextDescription.fColorStart[1], g_dialogTextDescription.fColorStart[2], 1.);
		pango_cairo_show_layout (pSurfaceContext, pLayout);
		
		cairo_destroy (pSurfaceContext);
		cairo_destroy (pSourceContext);
	}
	else
	{
		pDialog->pTextBuffer = NULL;
		pDialog->iTextHeight = 0;
		pDialog->iMessageWidth = pDialog->iIconSize + 2 * CAIRO_DIALOG_TEXT_MARGIN;
		pDialog->iMessageHeight = pDialog->iIconSize + 2 * CAIRO_DIALOG_TEXT_MARGIN;
	}
	
	pDialog->iBubbleWidth = MAX (pDialog->iMessageWidth, MAX (pDialog->iInteractiveWidth, pDialog->iBubbleWidth));
	pDialog->iBubbleHeight = pDialog->iMessageHeight + pDialog->iInteractiveHeight + pDialog->iButtonsHeight;
	
	gtk_widget_set (pDialog->pMessageWidget, "height-request", pDialog->iMessageHeight, "width-request", pDialog->iMessageWidth, NULL);
	gtk_widget_queue_draw_area (pDialog->pWidget,
		pDialog->iMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN,
		pDialog->iMargin + CAIRO_DIALOG_TEXT_MARGIN,
		MAX (iPrevTextWidth, pDialog->iMessageWidth - (pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN)),
		MAX (iPrevTextHeight, pDialog->iTextHeight));
}

void cairo_dock_set_dialog_icon (CairoDialog *pDialog, const gchar *cImageFilePath)
{
	if (cImageFilePath != NULL)
	{
		if (pDialog->pIconBuffer != NULL)
			cairo_surface_destroy (pDialog->pIconBuffer);
		cairo_t *pSourceContext = cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDialog));
		pDialog->pIconBuffer = cairo_dock_load_image_for_square_icon (pSourceContext, cImageFilePath, pDialog->iIconSize);
		cairo_destroy (pSourceContext);
		
		gtk_widget_queue_draw_area (pDialog->pWidget,
			pDialog->iMargin + CAIRO_DIALOG_TEXT_MARGIN,
			pDialog->iMargin + CAIRO_DIALOG_TEXT_MARGIN,
			pDialog->iIconSize,
			pDialog->iIconSize);
	}
	else if (pDialog->pIconBuffer != NULL)
	{
		cairo_surface_destroy (pDialog->pIconBuffer);
		pDialog->pIconBuffer = NULL;
		pDialog->iMessageWidth -= pDialog->iIconSize;
		pDialog->iMessageHeight = pDialog->iTextHeight + 2 * CAIRO_DIALOG_TEXT_MARGIN;
		pDialog->iIconSize = 0;
		
		gtk_widget_set (pDialog->pMessageWidget, "height-request", pDialog->iMessageHeight, "width-request", pDialog->iMessageWidth, NULL);
		
		gtk_widget_queue_draw (pDialog->pMessageWidget);
	}
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
				*bDirectionUp = (pDock->iWindowPositionY > g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
				*iY = (*bDirectionUp ? pDock->iWindowPositionY : pDock->iWindowPositionY + pDock->iCurrentHeight);
			}
			else
			{
				*bRight = (pDock->iWindowPositionY < g_iScreenWidth[CAIRO_DOCK_HORIZONTAL] / 2);
				*bDirectionUp = (pIcon->fXAtRest > pDock->fFlatDockWidth / 2);
				*iY = (! (*bRight) ? pDock->iWindowPositionY : pDock->iWindowPositionY + pDock->iCurrentHeight);
			}
	
			if (pDock->bAutoHide)
			{
				*iX = pDock->iWindowPositionX + (pIcon->fXAtRest + pIcon->fWidth * (*bRight ? .7 : .3)) / pDock->fFlatDockWidth * g_iVisibleZoneWidth;
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
				*bDirectionUp = (pDock->iWindowPositionY > g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
				*iY = (*bDirectionUp ? pDock->iWindowPositionY : pDock->iWindowPositionY + pDock->iCurrentHeight);
			}
			else
			{
				*bRight = (pDock->iWindowPositionY < g_iScreenWidth[CAIRO_DOCK_HORIZONTAL] / 2);
				*bDirectionUp = (pIcon->fXAtRest > pDock->fFlatDockWidth / 2);
				*iY = (! (*bRight) ? pDock->iWindowPositionY : pDock->iWindowPositionY + pDock->iCurrentHeight);
			}
			*iX = pDock->iWindowPositionX + pIcon->fDrawX + pIcon->fWidth * pIcon->fScale * pIcon->fWidthFactor / 2 + pIcon->fWidth * (*bRight ? .2 : - .2);
		}
	}
	else if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
		*bDirectionUp = (pDesklet->iWindowPositionY > g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
		*bIsHorizontal = (pDesklet->iWindowPositionX > 50 && pDesklet->iWindowPositionX + pDesklet->iHeight < g_iScreenWidth[CAIRO_DOCK_HORIZONTAL] - 50);
		
		if (*bIsHorizontal)
		{
			*bRight = (pDesklet->iWindowPositionX > g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
			*iX = pDesklet->iWindowPositionX + pDesklet->iWidth * (*bRight ? .7 : .3);
			*iY = (*bDirectionUp ? pDesklet->iWindowPositionY : pDesklet->iWindowPositionY + pDesklet->iHeight);
		}
		else
		{
			*bRight = (pDesklet->iWindowPositionX < g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
			*iY = pDesklet->iWindowPositionX + pDesklet->iWidth * (*bRight ? 1 : 0);
			*iX =pDesklet->iWindowPositionY + pDesklet->iHeight / 2;
		}
	}
}


void cairo_dock_dialog_find_optimal_placement (CairoDialog *pDialog)
{
	//g_print ("%s (Ybulle:%d; width:%d)\n", __func__, pDialog->iPositionY, pDialog->iWidth);
	g_return_if_fail (pDialog->iPositionY > 0);

	double fRadius = pDialog->fRadius;
	Icon *icon;
	CairoDialog *pDialogOnOurWay;

	double fXLeft = 0, fXRight = g_iScreenWidth[pDialog->bIsHorizontal];
	if (pDialog->bRight)
	{
		fXLeft = -1e4;
		fXRight = MAX (g_iScreenWidth[pDialog->bIsHorizontal], pDialog->iAimedX + 2*CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE + fRadius + .5*g_iDockLineWidth + 1);
	}
	else
	{
		fXLeft = MIN (0, pDialog->iAimedX - (2*CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE + fRadius + .5*g_iDockLineWidth + 1));
		fXRight = 1e4;
	}
	gboolean bCollision = FALSE;
	double fNextYStep = (pDialog->bDirectionUp ? -1e4 : 1e4);
	int iYInf, iYSup;
	GSList *ic;
	g_static_rw_lock_reader_lock (&s_mDialogsMutex);
	for (ic = s_pDialogList; ic != NULL; ic = ic->next)
	{
		pDialogOnOurWay = ic->data;
		if (pDialogOnOurWay != pDialog)
		{
			if (cairo_dock_dialog_reference (pDialogOnOurWay))
			{
				if (GTK_WIDGET_VISIBLE (pDialogOnOurWay->pWidget) && pDialogOnOurWay->pIcon != NULL)
				{
					iYInf = (pDialog->bDirectionUp ? pDialogOnOurWay->iPositionY : pDialogOnOurWay->iPositionY + pDialogOnOurWay->iHeight - (pDialogOnOurWay->iBubbleHeight + 2 * g_iDockLineWidth));
					iYSup = (pDialog->bDirectionUp ? pDialogOnOurWay->iPositionY + pDialogOnOurWay->iBubbleHeight + 2 * g_iDockLineWidth : pDialogOnOurWay->iPositionY + pDialogOnOurWay->iHeight);
					if (iYInf < pDialog->iPositionY + pDialog->iBubbleHeight + 2 * g_iDockLineWidth && iYSup > pDialog->iPositionY)
					{
						//g_print ("pDialogOnOurWay : %d - %d ; pDialog : %d - %d\n", iYInf, iYSup, pDialog->iPositionY, pDialog->iPositionY + (pDialog->iBubbleHeight + 2 * g_iDockLineWidth));
						if (pDialogOnOurWay->iAimedX < pDialog->iAimedX)
							fXLeft = MAX (fXLeft, pDialogOnOurWay->iPositionX + pDialogOnOurWay->iWidth);
						else
							fXRight = MIN (fXRight, pDialogOnOurWay->iPositionX);
						bCollision = TRUE;
						fNextYStep = (pDialog->bDirectionUp ? MAX (fNextYStep, iYInf) : MIN (fNextYStep, iYSup));
					}
				}
				cairo_dock_dialog_unreference (pDialogOnOurWay);
			}
		}
	}
	g_static_rw_lock_reader_unlock (&s_mDialogsMutex);

	//g_print (" -> [%.2f ; %.2f]\n", fXLeft, fXRight);

	if ((fXRight - fXLeft > pDialog->iWidth) && (
		(pDialog->bRight && fXLeft + fRadius + .5*g_iDockLineWidth < pDialog->iAimedX && fXRight > pDialog->iAimedX + 2*CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE + fRadius + .5*g_iDockLineWidth)
		||
		(! pDialog->bRight && fXLeft < pDialog->iAimedX - 2*CAIRO_DIALOG_TIP_MARGIN - CAIRO_DIALOG_TIP_BASE - fRadius - .5*g_iDockLineWidth && fXRight > pDialog->iAimedX + fRadius + .5*g_iDockLineWidth)
		))
	{
		if (pDialog->bRight)
			pDialog->iPositionX = MIN (pDialog->iAimedX - fRadius - .5*g_iDockLineWidth, fXRight - pDialog->iWidth);
		else
			pDialog->iPositionX = MAX (pDialog->iAimedX - fRadius - .5*g_iDockLineWidth - pDialog->iWidth, fXLeft);  /// pDialog->iBubbleWidth (?)
	}
	else
	{
		//g_print (" * Aim : (%d ; %d) ; Width : %d\n", pDialog->iAimedX, pDialog->iAimedY, pDialog->iWidth);
		pDialog->iPositionY = fNextYStep - (pDialog->bDirectionUp ? pDialog->iBubbleHeight + 2*dy(fRadius, g_iDockLineWidth) : 0);
		cairo_dock_dialog_find_optimal_placement (pDialog);
	}
}

void cairo_dock_place_dialog (CairoDialog *pDialog, CairoContainer *pContainer)
{
	//g_print ("%s ()\n", __func__);
	//g_return_if_fail (pDock != NULL && pDialog->pIcon != NULL);
	double fLineWidth = g_iDockLineWidth;
	int iPrevPositionX = pDialog->iPositionX, iPrevPositionY = pDialog->iPositionY;
	if (pContainer != NULL && pDialog->pIcon != NULL)
	{
		cairo_dock_dialog_calculate_aimed_point (pDialog->pIcon, pContainer, &pDialog->iAimedX, &pDialog->iAimedY, &pDialog->bRight, &pDialog->bIsHorizontal, &pDialog->bDirectionUp);
		//cd_debug (" Aim (%d;%d) / %d,%d,%d", pDialog->iAimedX, pDialog->iAimedY, pDialog->bIsHorizontal, pDialog->bDirectionUp, pDialog->bInside);
		
		if (pDialog->bIsHorizontal)
		{
			if (! pDialog->bInside)
			{
				pDialog->iPositionY = (pDialog->bDirectionUp ? pDialog->iAimedY - (pDialog->iBubbleHeight + 2 * pDialog->iMargin + CAIRO_DIALOG_DEFAULT_GAP) : pDialog->iAimedY + CAIRO_DIALOG_DEFAULT_GAP);  // on place la bulle d'abord sans prendre en compte la pointe.
				cairo_dock_dialog_find_optimal_placement (pDialog);
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
				pDialog->iPositionX = (pDialog->bRight ? pDialog->iAimedX : pDialog->iAimedX - pDialog->iWidth);
				pDialog->iPositionY = (pDialog->bDirectionUp ? pDialog->iAimedY - (pDialog->iBubbleHeight + 2 * pDialog->iMargin + CAIRO_DIALOG_DEFAULT_GAP) : pDialog->iAimedY + CAIRO_DIALOG_DEFAULT_GAP);  // on place la bulle (et non pas la fenetre) sans faire d'optimisation.
			}
		}
		//cd_debug (" => position : (%d;%d)", pDialog->iPositionX, pDialog->iPositionY);
		int iOldDistance = pDialog->iDistanceToDock;
		pDialog->iDistanceToDock =  (pDialog->bDirectionUp ? pDialog->iAimedY - pDialog->iPositionY - pDialog->iBubbleHeight - 2*pDialog->iMargin : pDialog->iPositionY - pDialog->iAimedY);
		///pDialog->iHeight = (pDialog->bDirectionUp ? pDialog->iAimedY - pDialog->iPositionY : pDialog->iPositionY + pDialog->iBubbleHeight + 2 * dy(pDialog->fRadius, fLineWidth) - pDialog->iAimedY);
		if (! pDialog->bDirectionUp)  // iPositionY est encore la position du coin haut gauche de la bulle et non de la fenetre.
			pDialog->iPositionY = pDialog->iAimedY;

		///double fGapFromDock = pDialog->iHeight - (pDialog->iBubbleHeight + 2 * pDialog->iMargin - .5 * fLineWidth);
		double fGapFromDock = pDialog->iDistanceToDock + .5 * fLineWidth;
		double cos_gamma = 1 / sqrt (1. + 1. * (CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE) / fGapFromDock * (CAIRO_DIALOG_TIP_MARGIN + CAIRO_DIALOG_TIP_BASE) / fGapFromDock);
		double cos_theta = 1 / sqrt (1. + 1. * CAIRO_DIALOG_TIP_MARGIN / fGapFromDock * CAIRO_DIALOG_TIP_MARGIN / fGapFromDock);
		pDialog->fTipHeight = fGapFromDock / (1. + fLineWidth / 2. / CAIRO_DIALOG_TIP_BASE * (1./cos_gamma + 1./cos_theta));
		//g_print ("pDialog->fTipHeight <- %.2f\n", pDialog->fTipHeight);
		
		//if ((int) fGapFromDock != pDialog->iDistanceToDock)
		if (pDialog->iDistanceToDock != iOldDistance)
		{
			//g_print ("  On change la taille de la pointe a : %d pixels ( -> %d)\n", pDialog->iDistanceToDock, pDialog->iMessageHeight + pDialog->iInteractiveHeight +pDialog->iButtonsHeight + pDialog->iDistanceToDock);
			gtk_widget_set (pDialog->pTipWidget, "height-request", MAX (0, pDialog->iDistanceToDock), NULL);

			/*if (pDialog->bIsHorizontal && ! pDialog->bDirectionUp)
			{
				pDialog->iPositionY -= (iOldDistance - pDialog->iDistanceToDock);
			}*/
			
			if ((iOldDistance == 0 || pDialog->iDistanceToDock < iOldDistance))
			{
				//g_print ("    cela reduit la fenetre a %dx%d\n", pDialog->iBubbleWidth + 2 * pDialog->iMargin, pDialog->iMessageHeight + pDialog->iInteractiveHeight +pDialog->iButtonsHeight +  pDialog->iDistanceToDock);
				//if (pDialog->bDirectionUp || ! pDialog->bInside)
					gtk_window_resize (GTK_WINDOW (pDialog->pWidget),
						pDialog->iBubbleWidth + 2 * pDialog->iMargin,
						pDialog->iMessageHeight + pDialog->iInteractiveHeight +pDialog->iButtonsHeight + pDialog->iDistanceToDock);
				/*else
				{
					pDialog->iPositionY -= (iOldDistance - pDialog->iDistanceToDock);
					gdk_window_move_resize (pDialog->pWidget->window,
						pDialog->iPositionX,
						pDialog->iPositionY,
						pDialog->iBubbleWidth + 2 * pDialog->iMargin,
						pDialog->iMessageHeight + pDialog->iInteractiveHeight +pDialog->iButtonsHeight + pDialog->iDistanceToDock);
				}*/
			}
			/*else if (pDialog->iDistanceToDock != iOldDistance && pDialog->bIsHorizontal && ! pDialog->bDirectionUp)
			{
				//pDialog->iPositionY -= iOldDistance - pDialog->iDistanceToDock;
				iPrevPositionY = pDialog->iPositionY;
				gdk_window_move_resize (pDialog->pWidget->window,
					pDialog->iPositionX,
					pDialog->iPositionY,
					pDialog->iBubbleWidth + 2 * pDialog->iMargin,
					pDialog->iMessageHeight + pDialog->iInteractiveHeight +pDialog->iButtonsHeight + pDialog->iDistanceToDock);
			}*/
		}
	}
	else
	{
		pDialog->bDirectionUp = TRUE;
		pDialog->iPositionX = (g_iScreenWidth [CAIRO_DOCK_HORIZONTAL] - pDialog->iWidth) / 2;
		pDialog->iPositionY = (g_iScreenHeight[CAIRO_DOCK_HORIZONTAL] - pDialog->iHeight) / 2;
		pDialog->iHeight = pDialog->iBubbleHeight + 2 * pDialog->iMargin;
	}

	if (iPrevPositionX != pDialog->iPositionX || iPrevPositionY != pDialog->iPositionY)
	{
		//g_print (" => (%d;%d) %dx%d\n", pDialog->iPositionX, pDialog->iPositionY, pDialog->iWidth, pDialog->iHeight);
		gtk_window_move (GTK_WINDOW (pDialog->pWidget),
			pDialog->iPositionX,
			pDialog->iPositionY);
	}
	/*gdk_window_move_resize (pDialog->pWidget->window,
		pDialog->iPositionX,
		pDialog->iPositionY,
		pDialog->iWidth,
		pDialog->iHeight);*/

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
	g_static_rw_lock_reader_lock (&s_mDialogsMutex);

	for (ic = s_pDialogList; ic != NULL; ic = ic->next)
	{
		pDialog = ic->data;

		if (cairo_dock_dialog_reference (pDialog))
		{
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
			cairo_dock_dialog_unreference (pDialog);
		}
	}
	g_static_rw_lock_reader_unlock (&s_mDialogsMutex);

	cairo_dock_remove_orphelans ();  // la liste a ete verouillee pendant longtemps, empechant les dialogues d'etre detruits.
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

CairoDialog *cairo_dock_show_dialog_full (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, gchar *cIconPath, GtkButtonsType iButtonsType, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	if (pIcon != NULL && pIcon->fPersonnalScale > 0)  // icone en cours de suppression.
		return NULL;

	CairoDialog *pDialog = cairo_dock_build_dialog (cText, pIcon, pContainer, cIconPath, pInteractiveWidget, iButtonsType, pActionFunc, data, pFreeDataFunc);

	if (pDialog != NULL && fTimeLength > 0)
		pDialog->iSidTimer = g_timeout_add (fTimeLength, (GSourceFunc) _cairo_dock_dialog_auto_delete, (gpointer) pDialog);

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
	CairoDialog *pDialog = cairo_dock_show_dialog_full (cFullText, pIcon, pContainer, fTimeLength, cIconPath, GTK_BUTTONS_NONE, NULL, NULL, NULL, NULL);
	g_free (cIconPath);
	g_free (cFullText);
	va_end (args);
	return pDialog;
}



CairoDialog *cairo_dock_show_dialog_with_question (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, 0, cIconPath, GTK_BUTTONS_YES_NO, NULL, pActionFunc, data, pFreeDataFunc);
}

CairoDialog *cairo_dock_show_dialog_with_entry (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, const gchar  *cTextForEntry, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (cTextForEntry, -1, -1);

	return cairo_dock_show_dialog_full (cText, pIcon, pContainer, 0, cIconPath, GTK_BUTTONS_OK_CANCEL, pWidget, pActionFunc, data, pFreeDataFunc);
}

CairoDialog *cairo_dock_show_dialog_with_value (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, double fValue, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	fValue = MAX (0, fValue);
	fValue = MIN (1, fValue);
	GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (NULL, fValue, 1.);

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
	GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog ((cInitialAnswer != NULL ? cInitialAnswer : ""), -1, -1);
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
	GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (NULL, fInitialValue, fMaxValue);
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
	gchar *cIconPath = g_strdup_printf ("%s/%s", CAIRO_DOCK_SHARE_DATA_DIR, CAIRO_DOCK_ICON);  // trouver une icone de question.
	int iAnswer = cairo_dock_show_dialog_and_wait (cQuestion, pIcon, pContainer, 0, cIconPath, GTK_BUTTONS_YES_NO, NULL);
	g_free (cIconPath);

	return (iAnswer == GTK_RESPONSE_OK ? GTK_RESPONSE_YES : GTK_RESPONSE_NO);
}



gboolean cairo_dock_icon_has_dialog (Icon *pIcon)
{
	g_static_rw_lock_reader_lock (&s_mDialogsMutex);

	GSList *ic;
	CairoDialog *pDialog;
	for (ic = s_pDialogList; ic != NULL; ic = ic->next)
	{
		pDialog = ic->data;
		if (pDialog->pIcon == pIcon)
			break ;
	}

	g_static_rw_lock_reader_unlock (&s_mDialogsMutex);
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
	if (! cairo_dock_dialog_reference (pDialog))
		return ;

	gtk_widget_hide (pDialog->pWidget);
	pDialog->bInside = FALSE;

	cairo_dock_dialog_unreference (pDialog);
}

void cairo_dock_unhide_dialog (CairoDialog *pDialog)
{
	if (! cairo_dock_dialog_reference (pDialog))
		return ;
	
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
	
	cairo_dock_dialog_unreference (pDialog);
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
