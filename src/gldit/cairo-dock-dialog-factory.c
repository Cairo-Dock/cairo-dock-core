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

#include <GL/gl.h>
#include <GL/glu.h>

#include "gldi-config.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-container.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"  // cairo_dock_launch_animation
#include "cairo-dock-launcher-manager.h"  // cairo_dock_search_icon_s_path
#include "cairo-dock-windows-manager.h"  // gldi_windows_get_active
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-style-manager.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-dialog-factory.h"

extern gboolean g_bUseOpenGL;
extern CairoDock *g_pMainDock;


static void _compute_dialog_sizes (CairoDialog *pDialog)
{
	pDialog->iMessageWidth = pDialog->iIconSize + pDialog->iTextWidth + (pDialog->iTextWidth != 0 ? 2 : 0) * CAIRO_DIALOG_TEXT_MARGIN - pDialog->iIconOffsetX;  // icone + marge + texte + marge.
	pDialog->iMessageHeight = MAX (pDialog->iIconSize - pDialog->iIconOffsetY, pDialog->iTextHeight) + (pDialog->pInteractiveWidget != NULL ? CAIRO_DIALOG_VGAP : 0);  // (icone/texte + marge) + widget + (marge + boutons) + pointe.
	
	if (pDialog->pButtons != NULL)
	{
		pDialog->iButtonsWidth = pDialog->iNbButtons * myDialogsParam.iDialogButtonWidth + (pDialog->iNbButtons - 1) * CAIRO_DIALOG_BUTTON_GAP + 2 * CAIRO_DIALOG_TEXT_MARGIN;  // marge + bouton1 + ecart + bouton2 + marge.
		pDialog->iButtonsHeight = CAIRO_DIALOG_VGAP + myDialogsParam.iDialogButtonHeight;  // il y'a toujours quelque chose au-dessus (texte et/ou widget)
	}
	
	pDialog->iBubbleWidth = MAX (pDialog->iInteractiveWidth, MAX (pDialog->iButtonsWidth, MAX (pDialog->iMessageWidth, pDialog->iMinFrameWidth)));
	pDialog->iBubbleHeight = pDialog->iMessageHeight + pDialog->iInteractiveHeight + pDialog->iButtonsHeight;
	if (pDialog->iBubbleWidth == 0)  // precaution.
		pDialog->iBubbleWidth = 20;
	if (pDialog->iBubbleHeight == 0)
		pDialog->iBubbleHeight = 10;
	
	pDialog->iComputedWidth = pDialog->iLeftMargin + pDialog->iBubbleWidth + pDialog->iRightMargin;
	pDialog->iComputedHeight = pDialog->iTopMargin + pDialog->iBubbleHeight + pDialog->iBottomMargin + pDialog->iMinBottomGap;  // all included.
	
	pDialog->container.iWidth = pDialog->iComputedWidth;
	pDialog->container.iHeight = pDialog->iComputedHeight;
}

static gboolean on_expose_dialog (G_GNUC_UNUSED GtkWidget *pWidget, cairo_t *pCairoContext, CairoDialog *pDialog)
{
	//g_print ("%s (%dx%d ; %d;%d)\n", __func__, pDialog->container.iWidth, pDialog->container.iHeight, pExpose->area.x, pExpose->area.y);
	/* int x, y;
	// OpenGL renderers are not ready for dialogs.
	if (g_bUseOpenGL && (pDialog->pDecorator == NULL || pDialog->pDecorator->render_opengl != NULL) && (pDialog->pRenderer == NULL || pDialog->pRenderer->render_opengl != NULL))
	{
		if (! gldi_glx_begin_draw_container (CAIRO_CONTAINER (pDialog)))
			return FALSE;
		
		if (pDialog->pDecorator != NULL && pDialog->pDecorator->render_opengl != NULL)
		{
			glPushMatrix ();
			pDialog->pDecorator->render_opengl (pDialog);
			glPopMatrix ();
		}
		
		gldi_object_notify (pDialog, NOTIFICATION_RENDER, pDialog, NULL);
		
		gldi_glx_end_draw_container (CAIRO_CONTAINER (pDialog));
	}
	else
	{*/
		cairo_dock_init_drawing_context_on_container (CAIRO_CONTAINER (pDialog), pCairoContext);
		
		if (pDialog->pDecorator != NULL)
		{
			cairo_save (pCairoContext);
			pDialog->pDecorator->render (pCairoContext, pDialog);
			cairo_restore (pCairoContext);
		}
		
		gldi_object_notify (pDialog, NOTIFICATION_RENDER, pDialog, pCairoContext);
	//}
	return FALSE;
}

static gboolean on_expose_dialog_after (G_GNUC_UNUSED GtkWidget *pWidget, cairo_t *pCairoContext, CairoDialog *pDialog)
{
	if (pDialog->fAppearanceCounter < 1.)  // modify the opacity after the interaction widget has been drawn by GTK.
	{
		double fAlpha = pDialog->fAppearanceCounter * pDialog->fAppearanceCounter;
		cairo_rectangle (pCairoContext,
			0,
			0,
			pDialog->container.iWidth,
			pDialog->container.iHeight);
		cairo_set_line_width (pCairoContext, 0);
		cairo_set_operator (pCairoContext, CAIRO_OPERATOR_DEST_OUT);
		cairo_set_source_rgba (pCairoContext, 0.0, 0.0, 0.0, 1. - fAlpha);
		cairo_fill (pCairoContext);
	}
	return FALSE;
}

static void _cairo_dock_set_dialog_input_shape (CairoDialog *pDialog)
{
	if (pDialog->pShapeBitmap != NULL)
		cairo_region_destroy (pDialog->pShapeBitmap);
	
	pDialog->pShapeBitmap = gldi_container_create_input_shape (CAIRO_CONTAINER (pDialog),
		0,
		0,
		1,
		1);  // workaround a bug in X with fully transparent window => let 1 pixel ON.

	gldi_container_set_input_shape (CAIRO_CONTAINER (pDialog), pDialog->pShapeBitmap);
}

static gboolean on_configure_dialog (G_GNUC_UNUSED GtkWidget* pWidget,
	GdkEventConfigure* pEvent,
	CairoDialog *pDialog)
{
	//g_print ("%s (%dx%d, %d;%d) [%d]\n", __func__, pEvent->width, pEvent->height, pEvent->x, pEvent->y, pDialog->bPositionForced);
	if (pEvent->width <= CAIRO_DIALOG_MIN_SIZE && pEvent->height <= CAIRO_DIALOG_MIN_SIZE && ! pDialog->bNoInput)
	{
		pDialog->container.bInside = FALSE;
		return FALSE;
	}
	
	//\____________ get dialog size and position.
	int iPrevWidth = pDialog->container.iWidth, iPrevHeight = pDialog->container.iHeight;
	pDialog->container.iWidth = pEvent->width;
	pDialog->container.iHeight = pEvent->height;
	pDialog->container.iWindowPositionX = pEvent->x;
	pDialog->container.iWindowPositionY = pEvent->y;
	
	//\____________ if an interactive widget is present, internal sizes may have changed.
	if (pDialog->pInteractiveWidget != NULL)
	{
		int w = pDialog->iInteractiveWidth, h = pDialog->iInteractiveHeight;
		GtkRequisition requisition;
		gtk_widget_get_preferred_size (pDialog->pInteractiveWidget, &requisition, NULL);
		pDialog->iInteractiveWidth = requisition.width;
		pDialog->iInteractiveHeight = requisition.height;
		//g_print ("  pInteractiveWidget : %dx%d\n", pDialog->iInteractiveWidth, pDialog->iInteractiveHeight);
		_compute_dialog_sizes (pDialog);
		
		if (w != pDialog->iInteractiveWidth || h != pDialog->iInteractiveHeight)
		{
			gldi_dialogs_replace_all ();
			/*Icon *pIcon = pDialog->pIcon;
			if (pIcon != NULL)
			{
				GldiContainer *pContainer = cairo_dock_search_container_from_icon (pIcon);
				cairo_dock_place_dialog (pDialog, pContainer);
			}*/
		}
	}
	//g_print ("dialog size: %dx%d / %dx%d\n", pEvent->width, pEvent->height, pDialog->iComputedWidth, pDialog->iComputedHeight);
	
	//\____________ set input shape if size has changed or if no shape yet.
	if (pDialog->bNoInput && (iPrevWidth != pEvent->width || iPrevHeight != pEvent->height || ! pDialog->pShapeBitmap))
	{
		_cairo_dock_set_dialog_input_shape (pDialog);
		pDialog->container.bInside = FALSE;
	}
	
	//\____________ force position for buggy WM (Compiz).
	if (pDialog->iComputedWidth == pEvent->width && pDialog->iComputedHeight == pEvent->height && (pEvent->y != pDialog->iComputedPositionY || pEvent->x != pDialog->iComputedPositionX) && pDialog->bPositionForced == 3)
	{
		pDialog->container.bInside = FALSE;
		cd_debug ("force to %d;%d", pDialog->iComputedPositionX, pDialog->iComputedPositionY);
		/*gtk_window_move (GTK_WINDOW (pDialog->container.pWidget),
			pDialog->iComputedPositionX,
			pDialog->iComputedPositionY);
		*/pDialog->bPositionForced ++;
	}
	
	gtk_widget_queue_draw (pDialog->container.pWidget);  // les widgets internes peuvent avoir changer de taille sans que le dialogue n'en ait change, il faut donc redessiner tout le temps.

	return FALSE;
}

static gboolean on_unmap_dialog (GtkWidget* pWidget,
	G_GNUC_UNUSED GdkEvent *pEvent,
	CairoDialog *pDialog)
{
	//g_print ("unmap dialog (bAllowMinimize:%d, visible:%d)\n", pDialog->bAllowMinimize, GTK_WIDGET_VISIBLE (pWidget));
	if (! pDialog->bAllowMinimize)  // it's an unexpected unmap event
	{
		if (pDialog->pUnmapTimer)  // see if it happened just after an event that we expected
		{
			double fElapsedTime = g_timer_elapsed (pDialog->pUnmapTimer, NULL);
			//g_print ("fElapsedTime : %fms\n", fElapsedTime);
			if (fElapsedTime < .2)  // it's a 2nd unmap event just after the first one, ignore it, it's just some noise from the WM
				return TRUE;
		}
		gtk_window_present (GTK_WINDOW (pWidget));  // counter it, we don't want dialogs to be hidden
	}
	else  // expected event, it's an unmap that we triggered with 'gldi_dialog_hide', so let pass it
	{
		pDialog->bAllowMinimize = FALSE;
		if (pDialog->pUnmapTimer != NULL)
			g_timer_destroy (pDialog->pUnmapTimer);
		pDialog->pUnmapTimer = g_timer_new ();  // remember the time it arrived
	}
	return TRUE;  // stops other handlers from being invoked for the event.
}

static gboolean on_map_dialog (G_GNUC_UNUSED GtkWidget* pWidget,
	G_GNUC_UNUSED GdkEvent *pEvent,
	CairoDialog *pDialog)
{
	if (pDialog->pInteractiveWidget)
	{
		gldi_container_present (CAIRO_CONTAINER (pDialog));
	}
	return FALSE;
}

static gboolean on_button_press_widget (G_GNUC_UNUSED GtkWidget *widget,
	GdkEventButton *pButton,
	CairoDialog *pDialog)
{
	cd_debug ("press button on widget");
	// memorize the time when the user clicked on the widget.
	pDialog->iButtonPressTime = pButton->time;
	return FALSE;
}

static GtkWidget *_cairo_dock_add_dialog_internal_box (CairoDialog *pDialog, int iWidth, int iHeight, gboolean bCanResize)
{
	GtkWidget *pBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	if (iWidth != 0 && iHeight != 0)
		g_object_set (pBox, "height-request", iHeight, "width-request", iWidth, NULL);
	else if (iWidth != 0)
			g_object_set (pBox, "width-request", iWidth, NULL);
	else if (iHeight != 0)
			g_object_set (pBox, "height-request", iHeight, NULL);
	gtk_box_pack_start (GTK_BOX (pDialog->pWidgetLayout),
		pBox,
		bCanResize,
		bCanResize,
		0);
	return pBox;
}

static cairo_surface_t *_cairo_dock_create_dialog_text_surface (const gchar *cText, gboolean bUseMarkup, int *iTextWidth, int *iTextHeight)
{
	if (cText == NULL)
		return NULL;
	
	myDialogsParam.dialogTextDescription.bUseMarkup = bUseMarkup;  // slight optimization, rather than duplicating the TextDescription each time.
	cairo_surface_t *pSurface = cairo_dock_create_surface_from_text (cText,
		&myDialogsParam.dialogTextDescription,
		iTextWidth,
		iTextHeight);
	myDialogsParam.dialogTextDescription.bUseMarkup = FALSE;  // by default
	return pSurface;
}

static cairo_surface_t *_cairo_dock_create_dialog_icon_surface (const gchar *cImageFilePath, Icon *pIcon, int iDesiredSize, int *iIconSize)
{
	if (cImageFilePath == NULL)
		return NULL;
	if (iDesiredSize == 0)
		iDesiredSize = myDialogsParam.iDialogIconSize;
	cairo_surface_t *pIconBuffer = NULL;
	if (strcmp (cImageFilePath, "same icon") == 0)
	{
		if (pIcon && pIcon->image.pSurface)
		{
			int iWidth, iHeight;
			cairo_dock_get_icon_extent (pIcon, &iWidth, &iHeight);
			pIconBuffer = cairo_dock_duplicate_surface (pIcon->image.pSurface,
				iWidth, iHeight,
				iDesiredSize, iDesiredSize);
		}
		else if (pIcon && pIcon->cFileName)
		{
			pIconBuffer = cairo_dock_create_surface_from_image_simple (pIcon->cFileName,
				iDesiredSize,
				iDesiredSize);
		}
	}
	else
	{
		pIconBuffer = cairo_dock_create_surface_from_image_simple (cImageFilePath,
			iDesiredSize,
			iDesiredSize);
	}
	if (pIconBuffer != NULL)
		*iIconSize = iDesiredSize;
	return pIconBuffer;
}

static gboolean _animation_loop (GldiContainer *pContainer)
{
	CairoDialog *pDialog = CAIRO_DIALOG (pContainer);
	gboolean bContinue = FALSE;
	gboolean bUpdateSlowAnimation = FALSE;
	pContainer->iAnimationStep ++;
	if (pContainer->iAnimationStep * pContainer->iAnimationDeltaT >= CAIRO_DOCK_MIN_SLOW_DELTA_T)
	{
		bUpdateSlowAnimation = TRUE;
		pContainer->iAnimationStep = 0;
		pContainer->bKeepSlowAnimation = FALSE;
	}
	
	if (pDialog->fAppearanceCounter < 1)
	{
		pDialog->fAppearanceCounter += .08;
		if (pDialog->fAppearanceCounter > .99)
		{
			pDialog->fAppearanceCounter = 1.;
		}
		else
		{
			bContinue = TRUE;
		}
	}
	
	if (bUpdateSlowAnimation)
	{
		gldi_object_notify (pDialog, NOTIFICATION_UPDATE_SLOW, pDialog, &pContainer->bKeepSlowAnimation);
	}
	
	gldi_object_notify (pDialog, NOTIFICATION_UPDATE, pDialog, &bContinue);
	
	cairo_dock_redraw_container (CAIRO_CONTAINER (pDialog));
	if (! bContinue && ! pContainer->bKeepSlowAnimation)
	{
		pContainer->iSidGLAnimation = 0;
		return FALSE;
	}
	else
		return TRUE;
}

void gldi_dialog_init_internals (CairoDialog *pDialog, CairoDialogAttr *pAttribute)
{
	pDialog->container.iface.animation_loop = _animation_loop;
	
	//\________________ set up the window
	GtkWidget *pWindow = pDialog->container.pWidget;
	gtk_window_set_title (GTK_WINDOW (pWindow), "cairo-dock-dialog");
	if (! pAttribute->pInteractiveWidget && ! pAttribute->pActionFunc)  // not an interactive dialog
		gtk_window_set_type_hint (GTK_WINDOW (pDialog->container.pWidget), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);  // pour ne pas prendre le focus.
	
	gtk_widget_add_events (pWindow, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_window_resize (GTK_WINDOW (pWindow), CAIRO_DIALOG_MIN_SIZE, CAIRO_DIALOG_MIN_SIZE);
	gtk_window_set_keep_above (GTK_WINDOW (pWindow), TRUE);
	
	pDialog->pIcon = pAttribute->pIcon;
	if (pAttribute->bForceAbove)  // try to force it above other windows (with most WM, it will still stay below fullscreen windows).
	{
		gtk_window_set_keep_above (GTK_WINDOW (pDialog->container.pWidget), TRUE);
		gtk_window_set_type_hint (GTK_WINDOW (pDialog->container.pWidget), GDK_WINDOW_TYPE_HINT_DOCK);  // should be called before the window becomes visible
	}
	
	//\________________ load the message
	if (pAttribute->cText != NULL)
	{
		pDialog->cText = g_strdup (pAttribute->cText);  // it may be a const string, so duplicate it
		pDialog->pTextBuffer = _cairo_dock_create_dialog_text_surface (pAttribute->cText,
			pAttribute->bUseMarkup,
			&pDialog->iTextWidth, &pDialog->iTextHeight);
		///pDialog->iTextTexture = cairo_dock_create_texture_from_surface (pDialog->pTextBuffer);
	}
	pDialog->bUseMarkup = pAttribute->bUseMarkup;  // remember this attribute, in case another text is set (with cairo_dock_set_dialog_message).

	//\________________ load the icon
	if (pAttribute->cImageFilePath != NULL)
	{
		pDialog->pIconBuffer = _cairo_dock_create_dialog_icon_surface (pAttribute->cImageFilePath, pAttribute->pIcon, pAttribute->iIconSize, &pDialog->iIconSize);
		///pDialog->iIconTexture = cairo_dock_create_texture_from_surface (pDialog->pIconBuffer);
	}

	//\________________ load the interactive widget
	if (pAttribute->pInteractiveWidget != NULL)
	{
		pDialog->pInteractiveWidget = pAttribute->pInteractiveWidget;
		
		GtkRequisition requisition;
		gtk_widget_get_preferred_size (pAttribute->pInteractiveWidget, &requisition, NULL);
		pDialog->iInteractiveWidth = requisition.width;
		pDialog->iInteractiveHeight = requisition.height;
	}
	
	//\________________ load the buttons
	pDialog->pUserData = pAttribute->pUserData;
	pDialog->pFreeUserDataFunc = pAttribute->pFreeDataFunc;
	if (pAttribute->cButtonsImage != NULL && pAttribute->pActionFunc != NULL)
	{
		int i;
		for (i = 0; pAttribute->cButtonsImage[i] != NULL; i++);
		
		pDialog->iNbButtons = i;
		pDialog->action_on_answer = pAttribute->pActionFunc;
		pDialog->pButtons = g_new0 (CairoDialogButton, pDialog->iNbButtons);
		const gchar *cButtonImage;
		for (i = 0; i < pDialog->iNbButtons; i++)
		{
			cButtonImage = pAttribute->cButtonsImage[i];
			if (strcmp (cButtonImage, "ok") == 0)
			{
				pDialog->pButtons[i].iDefaultType = 1;
			}
			else if (strcmp (cButtonImage, "cancel") == 0)
			{
				pDialog->pButtons[i].iDefaultType = 0;
			}
			else
			{
				gchar *cButtonPath;
				if (*cButtonImage != '/')
					cButtonPath = cairo_dock_search_icon_s_path (cButtonImage,
						MAX (myDialogsParam.iDialogButtonWidth, myDialogsParam.iDialogButtonHeight));
				else
					cButtonPath = (gchar*)cButtonImage;
				pDialog->pButtons[i].pSurface = cairo_dock_create_surface_from_image_simple (cButtonPath,
					myDialogsParam.iDialogButtonWidth,
					myDialogsParam.iDialogButtonHeight);
				if (cButtonPath != cButtonImage)
					g_free (cButtonPath);
				///pDialog->pButtons[i].iTexture = cairo_dock_create_texture_from_surface (pDialog->pButtons[i].pSurface);
			}
		}
	}
	else
	{
		pDialog->bNoInput = pAttribute->bNoInput;
	}
	
	//\________________ set a decorator.
	cairo_dock_set_dialog_decorator_by_name (pDialog, (pAttribute->cDecoratorName ? pAttribute->cDecoratorName : myDialogsParam.cDecoratorName));
	if (pDialog->pDecorator != NULL)
		pDialog->pDecorator->set_size (pDialog);
	
	//\________________ Maintenant qu'on connait tout, on calcule les tailles des divers elements.
	_compute_dialog_sizes (pDialog);
	pDialog->container.iWidth = pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin;
	pDialog->container.iHeight = pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iMinBottomGap;
	
	//\________________ On reserve l'espace pour les decorations.
	GtkWidget *pMainHBox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (pDialog->container.pWidget), pMainHBox);
	pDialog->pLeftPaddingBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	g_object_set (pDialog->pLeftPaddingBox, "width-request", pDialog->iLeftMargin, NULL);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pDialog->pLeftPaddingBox,
		FALSE,
		FALSE,
		0);

	pDialog->pWidgetLayout = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pDialog->pWidgetLayout,
		FALSE,
		FALSE,
		0);

	pDialog->pRightPaddingBox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	g_object_set (pDialog->pRightPaddingBox, "width-request", pDialog->iRightMargin, NULL);
	gtk_box_pack_start (GTK_BOX (pMainHBox),
		pDialog->pRightPaddingBox,
		FALSE,
		FALSE,
		0);
	
	//\________________ On reserve l'espace pour les elements.
	if (pDialog->container.bDirectionUp)
		pDialog->pTopWidget = _cairo_dock_add_dialog_internal_box (pDialog, 0, pDialog->iTopMargin, FALSE);
	else
		pDialog->pTipWidget = _cairo_dock_add_dialog_internal_box (pDialog, 0, pDialog->iMinBottomGap + pDialog->iBottomMargin, TRUE);
	if (pDialog->iMessageWidth != 0 && pDialog->iMessageHeight != 0)
	{
		pDialog->pMessageWidget = _cairo_dock_add_dialog_internal_box (pDialog, pDialog->iMessageWidth, pDialog->iMessageHeight, FALSE);
	}
	if (pDialog->pInteractiveWidget != NULL)
	{
		gtk_box_pack_start (GTK_BOX (pDialog->pWidgetLayout),
			pDialog->pInteractiveWidget,
			FALSE,
			FALSE,
			0);
		gtk_window_present (GTK_WINDOW (pDialog->container.pWidget));
		gtk_widget_grab_focus (pDialog->pInteractiveWidget);
		
		// set a MenuItem style to the dialog, so that the interactive widget can use the style defined for menu-items (either from the GTK theme, or from our own .css), and therefore be well integrated into the dialog, as if it was inside a menu.
		GtkStyleContext *ctx = gtk_widget_get_style_context (pDialog->pWidgetLayout);
		gtk_style_context_add_class (ctx, myDialogsParam.bUseDefaultColors && myStyleParam.bUseSystemColors ? GTK_STYLE_CLASS_MENUITEM : "gldimenuitem");
	}
	if (pDialog->pButtons != NULL)
	{
		pDialog->pButtonsWidget = _cairo_dock_add_dialog_internal_box (pDialog, pDialog->iButtonsWidth, pDialog->iButtonsHeight, FALSE);
	}
	if (pDialog->container.bDirectionUp)
		pDialog->pTipWidget = _cairo_dock_add_dialog_internal_box (pDialog, 0, pDialog->iMinBottomGap + pDialog->iBottomMargin, TRUE);
	else
		pDialog->pTopWidget = _cairo_dock_add_dialog_internal_box (pDialog, 0, pDialog->iTopMargin, TRUE);
	
	gtk_widget_show_all (pDialog->container.pWidget);
	
	//\________________ load the input shape.
	if (pDialog->bNoInput)
	{
		_cairo_dock_set_dialog_input_shape (pDialog);
	}
	
	//\________________ connect the signals to the window
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"draw",
		G_CALLBACK (on_expose_dialog),
		pDialog);
	g_signal_connect_after (G_OBJECT (pDialog->container.pWidget),
		"draw",
		G_CALLBACK (on_expose_dialog_after),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"configure-event",
		G_CALLBACK (on_configure_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"unmap-event",
		G_CALLBACK (on_unmap_dialog),
		pDialog);  // prevent dialogs from being hidden (for instance by a 'show-desktop' event), because they might be modal
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"map-event",
		G_CALLBACK (on_map_dialog),
		pDialog);  // some WM (like GS) prevent the focus to be taken, so we have to force it whenever the dialog is shown (creation or unhide).
	if (pDialog->pInteractiveWidget != NULL && pDialog->pButtons == NULL)  // the dialog has no button to be closed, so it can be closed by clicking on it. But some widget (like the GTK calendar) let pass the click to their parent (= the dialog), which then close it. To prevent this, we memorize the last click on the widget.
		g_signal_connect (G_OBJECT (pDialog->pInteractiveWidget),
			"button-press-event",
			G_CALLBACK (on_button_press_widget),
			pDialog);
	
	cairo_dock_launch_animation (CAIRO_CONTAINER (pDialog));
}

CairoDialog *gldi_dialog_new (CairoDialogAttr *pAttribute)
{
	if (!pAttribute->bForceAbove)
	{
		GldiWindowActor *pActiveAppli = gldi_windows_get_active ();
		if (pActiveAppli && pActiveAppli->bIsFullScreen && gldi_window_is_on_current_desktop (pActiveAppli))
		{
			cd_debug ("skip dialog since current fullscreen window would mask it");
			return NULL;
		}
	}
	pAttribute->cattr.bNoOpengl = TRUE;
	cd_debug ("%s (%s, %s, %x, %x, (%p;%p))", __func__, pAttribute->cText, pAttribute->cImageFilePath, pAttribute->pInteractiveWidget, pAttribute->pActionFunc, pAttribute->pIcon, pAttribute->pContainer);
	return (CairoDialog*)gldi_object_new (&myDialogObjectMgr, pAttribute);
}


CairoDialog *gldi_dialog_show (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	if (pIcon != NULL && cairo_dock_icon_is_being_removed (pIcon))  // icone en cours de suppression.
	{
		cd_debug ("dialog skipped for %s (%.2f)", pIcon->cName, pIcon->fInsertRemoveFactor);
		return NULL;
	}
	
	CairoDialogAttr attr;
	memset (&attr, 0, sizeof (CairoDialogAttr));
	attr.cText = (gchar *)cText;
	attr.cImageFilePath = (gchar *)cIconPath;
	attr.pInteractiveWidget = pInteractiveWidget;
	attr.pActionFunc = pActionFunc;
	attr.pUserData = data;
	attr.pFreeDataFunc = pFreeDataFunc;
	attr.iTimeLength = (int) fTimeLength;
	const gchar *cDefaultActionButtons[3] = {"ok", "cancel", NULL};
	if (pActionFunc != NULL)
		attr.cButtonsImage = cDefaultActionButtons;
	attr.pIcon = pIcon;
	attr.pContainer = pContainer;
	
	return gldi_dialog_new (&attr);
}

CairoDialog *gldi_dialog_show_temporary_with_icon_printf (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength, const gchar *cIconPath, ...)
{
	g_return_val_if_fail (cText != NULL, NULL);
	va_list args;
	va_start (args, cIconPath);
	gchar *cFullText = g_strdup_vprintf (cText, args);
	CairoDialog *pDialog = gldi_dialog_show (cFullText, pIcon, pContainer, fTimeLength, cIconPath, NULL, NULL, NULL, NULL);
	g_free (cFullText);
	va_end (args);
	return pDialog;
}

CairoDialog *gldi_dialog_show_temporary_with_icon (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength, const gchar *cIconPath)
{
	g_return_val_if_fail (cText != NULL, NULL);
	return gldi_dialog_show (cText, pIcon, pContainer, fTimeLength, cIconPath, NULL, NULL, NULL, NULL);
}

CairoDialog *gldi_dialog_show_temporary (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength)
{
	g_return_val_if_fail (cText != NULL, NULL);
	return gldi_dialog_show (cText, pIcon, pContainer, fTimeLength, NULL, NULL, NULL, NULL, NULL);
}

CairoDialog *gldi_dialog_show_temporary_with_default_icon (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength)
{
	g_return_val_if_fail (cText != NULL, NULL);
	
	const gchar *cIconPath = GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON;
	
	CairoDialogAttr attr;
	memset (&attr, 0, sizeof (CairoDialogAttr));
	attr.cText = cText;
	attr.cImageFilePath = cIconPath;
	attr.iTimeLength = (int) fTimeLength;
	attr.pIcon = pIcon;
	attr.pContainer = pContainer;
	
	return gldi_dialog_new (&attr);
}

static inline GtkWidget *_cairo_dock_make_entry_for_dialog (const gchar *cTextForEntry)
{
	GtkWidget *pWidget = gtk_entry_new ();
	gtk_entry_set_has_frame (GTK_ENTRY (pWidget), FALSE);
	g_object_set (pWidget, "width-request", CAIRO_DIALOG_MIN_ENTRY_WIDTH, NULL);
	if (cTextForEntry != NULL)
		gtk_entry_set_text (GTK_ENTRY (pWidget), cTextForEntry);
	return pWidget;
}
static inline GtkWidget *_cairo_dock_make_hscale_for_dialog (double fValueForHScale, double fMaxValueForHScale)
{
	GtkWidget *pWidget = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, fMaxValueForHScale, fMaxValueForHScale / 100.);
	gtk_scale_set_digits (GTK_SCALE (pWidget), 2);
	gtk_range_set_value (GTK_RANGE (pWidget), fValueForHScale);

	g_object_set (pWidget, "width-request", CAIRO_DIALOG_MIN_SCALE_WIDTH, NULL);
	return pWidget;
}

CairoDialog *gldi_dialog_show_with_question (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, const gchar *cIconPath, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	return gldi_dialog_show (cText, pIcon, pContainer, 0, cIconPath, NULL, pActionFunc, data, pFreeDataFunc);
}

CairoDialog *gldi_dialog_show_with_entry (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, const gchar *cIconPath, const gchar *cTextForEntry, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (cTextForEntry, -1, -1);
	GtkWidget *pWidget = _cairo_dock_make_entry_for_dialog (cTextForEntry);

	return gldi_dialog_show (cText, pIcon, pContainer, 0, cIconPath, pWidget, pActionFunc, data, pFreeDataFunc);
}

CairoDialog *gldi_dialog_show_with_value (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, const gchar *cIconPath, double fValue, double fMaxValue, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc)
{
	fValue = MAX (0., fValue);
	fValue = MIN (fMaxValue, fValue);
	//GtkWidget *pWidget = cairo_dock_build_common_interactive_widget_for_dialog (NULL, fValue, 1.);
	GtkWidget *pWidget = _cairo_dock_make_hscale_for_dialog (fValue, fMaxValue);

	return gldi_dialog_show (cText, pIcon, pContainer, 0, cIconPath, pWidget, pActionFunc, data, pFreeDataFunc);
}

CairoDialog *gldi_dialog_show_general_message (const gchar *cMessage, double fTimeLength)
{
	Icon *pIcon = gldi_icons_get_any_without_dialog ();
	return gldi_dialog_show_temporary (cMessage, pIcon, CAIRO_CONTAINER (g_pMainDock), fTimeLength);
}


static void _cairo_dock_get_answer_from_dialog (int iClickedButton, G_GNUC_UNUSED GtkWidget *pInteractiveWidget, gpointer *data, CairoDialog *pDialog)
{
	cd_message ("%s (%d)", __func__, iClickedButton);
	int *iAnswerBuffer = data[0];
	// GMainLoop *pBlockingLoop = data[1];
	
	gldi_dialog_steal_interactive_widget (pDialog);  // le dialogue disparaitra apres cette fonction, mais le widget interactif doit rester.
	
	*iAnswerBuffer = iClickedButton;
}
static void _on_free_blocking_dialog (gpointer *data)
{
	GMainLoop *pBlockingLoop = data[1];
	if (g_main_loop_is_running (pBlockingLoop))
		g_main_loop_quit (pBlockingLoop);
}
int gldi_dialog_show_and_wait (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, const gchar *cIconPath, GtkWidget *pInteractiveWidget)
{
	int iClickedButton = -3;
	GMainLoop *pBlockingLoop = g_main_loop_new (NULL, FALSE);
	gpointer data[2] = {&iClickedButton, pBlockingLoop};  // it's useless to allocate 'data' because this function will wait for an answer

	CairoDialog *pDialog = gldi_dialog_show (cText,
		pIcon,
		pContainer,
		0.,
		cIconPath,
		pInteractiveWidget,
		(CairoDockActionOnAnswerFunc)_cairo_dock_get_answer_from_dialog,
		(gpointer) data,
		(GFreeFunc)_on_free_blocking_dialog);
	
	if (pDialog != NULL)
	{
		pDialog->fAppearanceCounter = 1.;
		cd_debug ("Start the blocking loop...");
		g_main_loop_run (pBlockingLoop);
		cd_debug ("End of the blocking loop -> %d", iClickedButton);
	}

	g_main_loop_unref (pBlockingLoop);
	return iClickedButton;
}


GtkWidget *cairo_dock_steal_widget_from_its_container (GtkWidget *pWidget)
{
	g_return_val_if_fail (pWidget != NULL, NULL);
	GtkWidget *pContainer = gtk_widget_get_parent (pWidget);
	if (pContainer != NULL)
	{
		g_object_ref (G_OBJECT (pWidget));
		gtk_container_remove (GTK_CONTAINER (pContainer), pWidget);
	}
	return pWidget;
}

GtkWidget *gldi_dialog_steal_interactive_widget (CairoDialog *pDialog)
{
	if (pDialog == NULL)
		return NULL;
	
	GtkWidget *pInteractiveWidget = pDialog->pInteractiveWidget;
	if (pInteractiveWidget != NULL)
	{
		pInteractiveWidget = cairo_dock_steal_widget_from_its_container (pInteractiveWidget);
		pDialog->pInteractiveWidget = NULL;
		
		// if we were monitoring the click events on the widget, stop it.
		g_signal_handlers_disconnect_matched (pInteractiveWidget,
			G_SIGNAL_MATCH_FUNC,
			0,
			0,
			NULL,
			on_button_press_widget,
			NULL);
	}
	return pInteractiveWidget;
}

static void _redraw_icon_surface (CairoDialog *pDialog)
{
	if (!pDialog->container.bUseReflect)
		gtk_widget_queue_draw_area (pDialog->container.pWidget,
			pDialog->iLeftMargin,
			(pDialog->container.bDirectionUp ? 
				pDialog->iTopMargin :
				pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight)),
			pDialog->iIconSize,
			pDialog->iIconSize);
	else
		gtk_widget_queue_draw (pDialog->container.pWidget);
}

static void _redraw_text_surface (CairoDialog *pDialog)
{
	if (!pDialog->container.bUseReflect)
		gtk_widget_queue_draw_area (pDialog->container.pWidget,
			pDialog->iLeftMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN,
			(pDialog->container.bDirectionUp ? 
				pDialog->iTopMargin :
				pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight)),
			pDialog->iTextWidth,
			pDialog->iTextHeight);
	else
		gtk_widget_queue_draw (pDialog->container.pWidget);
}

void gldi_dialog_redraw_interactive_widget (CairoDialog *pDialog)
{
	if (!pDialog->container.bUseReflect)
		gtk_widget_queue_draw_area (pDialog->container.pWidget,
			pDialog->iLeftMargin,
			(pDialog->container.bDirectionUp ? 
				pDialog->iTopMargin + pDialog->iMessageHeight :
				pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight) + pDialog->iMessageHeight),
			pDialog->iInteractiveWidth,
			pDialog->iInteractiveHeight);
	else
		gtk_widget_queue_draw (pDialog->container.pWidget);
}


static void _set_icon_surface (CairoDialog *pDialog, cairo_surface_t *pNewIconSurface, int iNewIconSize)
{
	int iPrevMessageWidth = pDialog->iMessageWidth;
	int iPrevMessageHeight = pDialog->iMessageHeight;
	
	cairo_surface_destroy (pDialog->pIconBuffer);
	if (pDialog->iIconTexture != 0)
		_cairo_dock_delete_texture (pDialog->iIconTexture);
	
	pDialog->pIconBuffer = pNewIconSurface;
	if (! pNewIconSurface)
		iNewIconSize = 0;
	
	if (pDialog->iIconSize != iNewIconSize)  // can happen if the dialog didn't have an icon before, or if the new one is NULL
	{
		pDialog->iIconSize = iNewIconSize;
		_compute_dialog_sizes (pDialog);
	}
	
	// redraw
	if (pDialog->iMessageWidth != iPrevMessageWidth || pDialog->iMessageHeight != iPrevMessageHeight)
	{
		g_object_set (pDialog->pMessageWidget, "width-request", pDialog->iMessageWidth, "height-request", pDialog->iMessageHeight, NULL);  // inutile de replacer le dialogue puisque sa gravite fera le boulot.
		
		gtk_widget_queue_draw (pDialog->container.pWidget);
	}
	else
	{
		_redraw_icon_surface (pDialog);
	}
}

void gldi_dialog_set_icon_surface (CairoDialog *pDialog, cairo_surface_t *pNewIconSurface, int iNewIconSize)
{
	int iIconSize = (pDialog->iIconSize != 0 ? pDialog->iIconSize : myDialogsParam.iDialogIconSize);
	cairo_surface_t *pIconBuffer = cairo_dock_duplicate_surface (pNewIconSurface, iNewIconSize, iNewIconSize, iIconSize, iIconSize);
	_set_icon_surface (pDialog, pIconBuffer, iIconSize);
}

void gldi_dialog_set_icon (CairoDialog *pDialog, const gchar *cImageFilePath)
{
	int iIconSize = (pDialog->iIconSize != 0 ? pDialog->iIconSize : myDialogsParam.iDialogIconSize);
	cairo_surface_t *pIconBuffer = cairo_dock_create_surface_for_square_icon (cImageFilePath, iIconSize);
	
	_set_icon_surface (pDialog, pIconBuffer, iIconSize);
}


static void _set_text_surface (CairoDialog *pDialog, cairo_surface_t *pNewTextSurface, int iNewTextWidth, int iNewTextHeight)
{
	int iPrevMessageWidth = pDialog->iMessageWidth;
	int iPrevMessageHeight = pDialog->iMessageHeight;

	cairo_surface_destroy (pDialog->pTextBuffer);
	pDialog->pTextBuffer = pNewTextSurface;
	if (pDialog->iTextTexture != 0)
		_cairo_dock_delete_texture (pDialog->iTextTexture);
	///pDialog->iTextTexture = cairo_dock_create_texture_from_surface (pNewTextSurface);
	
	pDialog->iTextWidth = iNewTextWidth;
	pDialog->iTextHeight = iNewTextHeight;
	_compute_dialog_sizes (pDialog);

	if (pDialog->iMessageWidth != iPrevMessageWidth || pDialog->iMessageHeight != iPrevMessageHeight)
	{
		g_object_set (pDialog->pMessageWidget, "width-request", pDialog->iMessageWidth, "height-request", pDialog->iMessageHeight, NULL);  // inutile de replacer le dialogue puisque sa gravite fera le boulot.
		
		gtk_widget_queue_draw (pDialog->container.pWidget);
		
		gboolean bInside = pDialog->container.bInside;
		pDialog->container.bInside = FALSE;  // unfortunately the gravity is really badly handled by many WMs, so we have to replace he dialog ourselves :-/
		gldi_dialogs_replace_all ();
		pDialog->container.bInside = bInside;
	}
	else
	{
		_redraw_text_surface (pDialog);
	}
}

void gldi_dialog_set_message (CairoDialog *pDialog, const gchar *cMessage)
{
	cd_debug ("%s", cMessage);
	int iNewTextWidth=0, iNewTextHeight=0;
	cairo_surface_t *pNewTextSurface = _cairo_dock_create_dialog_text_surface (cMessage, pDialog->bUseMarkup, &iNewTextWidth, &iNewTextHeight);
	
	_set_text_surface (pDialog, pNewTextSurface, iNewTextWidth, iNewTextHeight);
	
	g_free (pDialog->cText);
	pDialog->cText = g_strdup (cMessage);
}
void gldi_dialog_set_message_printf (CairoDialog *pDialog, const gchar *cMessageFormat, ...)
{
	g_return_if_fail (cMessageFormat != NULL);
	va_list args;
	va_start (args, cMessageFormat);
	gchar *cMessage = g_strdup_vprintf (cMessageFormat, args);
	gldi_dialog_set_message (pDialog, cMessage);
	g_free (cMessage);
	va_end (args);
}

void gldi_dialog_reload (CairoDialog *pDialog)
{
	// re-set the GTK style class (global style may have changed between system / custom)
	GtkStyleContext *ctx = gtk_widget_get_style_context (pDialog->pWidgetLayout);

	gtk_style_context_remove_class (ctx, GTK_STYLE_CLASS_MENUITEM);
	gtk_style_context_remove_class (ctx, "gldimenuitem");

	gtk_style_context_add_class (ctx, myDialogsParam.bUseDefaultColors && myStyleParam.bUseSystemColors ? GTK_STYLE_CLASS_MENUITEM : "gldimenuitem");

	// reload the text buffer (color or font may have changed)
	if (pDialog->cText != NULL)
	{
		gchar *cText = pDialog->cText;
		pDialog->cText = NULL;
		gldi_dialog_set_message (pDialog, cText);
		g_free (cText);
	}

	// reload sizes (radius or linewidth may have changed)
	_compute_dialog_sizes (pDialog);
}
