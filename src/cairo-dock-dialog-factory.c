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

#include "cairo-dock-icons.h"
#include "cairo-dock-container.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-log.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-renderer-manager.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-dialogs.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-notifications.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-gui-manager.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-X-manager.h"
#include "cairo-dock-dialog-factory.h"

extern CairoDock *g_pMainDock;
extern CairoDockDesktopGeometry g_desktopGeometry;
extern gboolean g_bSticky;
extern gboolean g_bKeepAbove;
extern gboolean g_bUseOpenGL;

#define _drawn_text_width(pDialog) (pDialog->iMaxTextWidth != 0 && pDialog->iTextWidth > pDialog->iMaxTextWidth ? pDialog->iMaxTextWidth : pDialog->iTextWidth)

static void _cairo_dock_compute_dialog_sizes (CairoDialog *pDialog)
{
	pDialog->iMessageWidth = pDialog->iIconSize + _drawn_text_width (pDialog) + (pDialog->iTextWidth != 0 ? 2 : 0) * CAIRO_DIALOG_TEXT_MARGIN;  // icone + marge + texte + marge.
	pDialog->iMessageHeight = MAX (pDialog->iIconSize, pDialog->iTextHeight) + (pDialog->pInteractiveWidget != NULL ? CAIRO_DIALOG_VGAP : 0);  // (icone/texte + marge) + widget + (marge + boutons) + pointe.
	
	if (pDialog->pButtons != NULL)
	{
		pDialog->iButtonsWidth = pDialog->iNbButtons * myDialogs.iDialogButtonWidth + (pDialog->iNbButtons - 1) * CAIRO_DIALOG_BUTTON_GAP + 2 * CAIRO_DIALOG_TEXT_MARGIN;  // marge + bouton1 + ecart + bouton2 + marge.
		pDialog->iButtonsHeight = CAIRO_DIALOG_VGAP + myDialogs.iDialogButtonHeight;  // il y'a toujours quelque chose au-dessus (texte et/out widget)
	}
	
	pDialog->iBubbleWidth = MAX (pDialog->iInteractiveWidth, MAX (pDialog->iButtonsWidth, MAX (pDialog->iMessageWidth, pDialog->iMinFrameWidth)));
	pDialog->iBubbleHeight = pDialog->iMessageHeight + pDialog->iInteractiveHeight + pDialog->iButtonsHeight;
	if (pDialog->iBubbleWidth == 0)  // precaution.
		pDialog->iBubbleWidth = 20;
	if (pDialog->iBubbleHeight == 0)
		pDialog->iBubbleHeight = 20;
	
	pDialog->container.iWidth = pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin;
	pDialog->container.iHeight = pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iMinBottomGap;  // resultat temporaire, sans la pointe.
}

static gboolean on_expose_dialog (GtkWidget *pWidget,
	GdkEventExpose *pExpose,
	CairoDialog *pDialog)
{
	//g_print ("%s (%dx%d)\n", __func__, pDialog->container.iWidth, pDialog->container.iHeight);
	int x, y;
	if (0 && g_bUseOpenGL && (pDialog->pDecorator == NULL || pDialog->pDecorator->render_opengl != NULL) && (pDialog->pRenderer == NULL || pDialog->pRenderer->render_opengl != NULL))
	{
		GdkGLContext *pGlContext = gtk_widget_get_gl_context (pDialog->container.pWidget);
		GdkGLDrawable *pGlDrawable = gtk_widget_get_gl_drawable (pDialog->container.pWidget);
		if (!gdk_gl_drawable_gl_begin (pGlDrawable, pGlContext))
			return FALSE;
		
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity ();
		
		cairo_dock_apply_desktop_background_opengl (CAIRO_CONTAINER (pDialog));
		
		if (pDialog->pDecorator != NULL && pDialog->pDecorator->render_opengl != NULL)
		{
			glPushMatrix ();
			pDialog->pDecorator->render_opengl (pDialog);
			glPopMatrix ();
		}
		
		cairo_dock_notify (CAIRO_DOCK_RENDER_DIALOG, pDialog, NULL);
		
		if (gdk_gl_drawable_is_double_buffered (pGlDrawable))
			gdk_gl_drawable_swap_buffers (pGlDrawable);
		else
			glFlush ();
		gdk_gl_drawable_gl_end (pGlDrawable);
	}
	else
	{
		cairo_t *pCairoContext;
		
		if ((pExpose->area.x != 0 || pExpose->area.y != 0))
		{
			pCairoContext = cairo_dock_create_drawing_context_on_area (CAIRO_CONTAINER (pDialog), &pExpose->area, myDialogs.fDialogColor);
		}
		else
		{
			pCairoContext = cairo_dock_create_drawing_context_on_container (CAIRO_CONTAINER (pDialog));
		}
		
		if (pDialog->pDecorator != NULL)
		{
			cairo_save (pCairoContext);
			pDialog->pDecorator->render (pCairoContext, pDialog);
			cairo_restore (pCairoContext);
		}
		
		cairo_dock_notify (CAIRO_DOCK_RENDER_DIALOG, pDialog, pCairoContext);
		
		cairo_destroy (pCairoContext);
	}
	return FALSE;
}

static gboolean on_configure_dialog (GtkWidget* pWidget,
	GdkEventConfigure* pEvent,
	CairoDialog *pDialog)
{
	//g_print ("%s (%dx%d, %d;%d) [%d]\n", __func__, pEvent->width, pEvent->height, pEvent->x, pEvent->y, pDialog->bPositionForced);
	if (pEvent->width == CAIRO_DIALOG_MIN_SIZE && pEvent->height == CAIRO_DIALOG_MIN_SIZE && ! pDialog->bNoInput)
		return FALSE;
	
	int iWidth = pDialog->container.iWidth, iHeight = pDialog->container.iHeight;
	//\____________ On recupere la taille du widget interactif qui a pu avoir change.
	if (pDialog->pInteractiveWidget != NULL)
	{
		GtkRequisition requisition;
		gtk_widget_size_request (pDialog->pInteractiveWidget, &requisition);
		pDialog->iInteractiveWidth = requisition.width;
		pDialog->iInteractiveHeight = requisition.height;
		//g_print ("  pInteractiveWidget : %dx%d\n", pDialog->iInteractiveWidth, pDialog->iInteractiveHeight);

		pDialog->iBubbleWidth = MAX (pDialog->iMessageWidth, MAX (pDialog->iInteractiveWidth, pDialog->iButtonsWidth));
		pDialog->iBubbleHeight = pDialog->iMessageHeight + pDialog->iInteractiveHeight + pDialog->iButtonsHeight;
		//g_print (" -> iBubbleWidth: %d , iBubbleHeight : %d\n", pDialog->iBubbleWidth, pDialog->iBubbleHeight);
		_cairo_dock_compute_dialog_sizes (pDialog);
	}

	if (iWidth != pEvent->width || iHeight != pEvent->height)
	{
		if ((pEvent->width != CAIRO_DIALOG_MIN_SIZE || pEvent->height != CAIRO_DIALOG_MIN_SIZE) && (pEvent->width < iWidth || pEvent->height < iHeight))
		{
			//g_print ("non, on a dit %dx%d !\n", pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin,
			//	pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iDistanceToDock);
			gtk_window_resize (GTK_WINDOW (pDialog->container.pWidget),
				pDialog->iBubbleWidth + pDialog->iLeftMargin + pDialog->iRightMargin,
				pDialog->iBubbleHeight + pDialog->iTopMargin + pDialog->iBottomMargin + pDialog->iDistanceToDock);
		}
		pDialog->container.iWidth = pEvent->width;
		pDialog->container.iHeight = pEvent->height;
		
		if (pDialog->bNoInput)
		{
			if (pDialog->pShapeBitmap != NULL)
				g_object_unref ((gpointer) pDialog->pShapeBitmap);
			pDialog->pShapeBitmap = (GdkBitmap*) gdk_pixmap_new (NULL,
				pEvent->width,
				pEvent->height,
				1);
			cairo_t *pCairoContext = gdk_cairo_create (pDialog->pShapeBitmap);
			cairo_set_source_rgba (pCairoContext, 0.0f, 0.0f, 0.0f, 0.0f);
			cairo_set_operator (pCairoContext, CAIRO_OPERATOR_SOURCE);
			cairo_paint (pCairoContext);
			cairo_destroy (pCairoContext);
			
			gtk_widget_input_shape_combine_mask (pDialog->container.pWidget,
				pDialog->pShapeBitmap,
				0,
				0);
		}
	}
	else if (pEvent->y != pDialog->container.iWindowPositionY && !pDialog->bPositionForced)
	{
		gtk_window_move (GTK_WINDOW (pDialog->container.pWidget), pDialog->container.iWindowPositionX, pDialog->container.iWindowPositionY);
		pDialog->bPositionForced = TRUE;
	}
	gtk_widget_queue_draw (pDialog->container.pWidget);  // les widgets internes peuvent avoir changer de taille sans que le dialogue n'en ait change, il faut donc redessiner tout le temps.

	return FALSE;
}

static gboolean on_unmap_dialog (GtkWidget* pWidget,
	GdkEvent *pEvent,
	CairoDialog *pDialog)
{
	//g_print ("unmap dialog (bAllowMinimize:%d, visible:%d)\n", pDialog->bAllowMinimize, GTK_WIDGET_VISIBLE (pWidget));
	if (! pDialog->bAllowMinimize)
	{
		if (pDialog->pUnmapTimer)
		{
			double fElapsedTime = g_timer_elapsed (pDialog->pUnmapTimer, NULL);
			//g_print ("fElapsedTime : %fms\n", fElapsedTime);
			g_timer_destroy (pDialog->pUnmapTimer);
			pDialog->pUnmapTimer = NULL;
			if (fElapsedTime < .2)
				return TRUE;
		}
		gtk_window_present (GTK_WINDOW (pWidget));
	}
	else
	{
		pDialog->bAllowMinimize = FALSE;
		if (pDialog->pUnmapTimer == NULL)
			pDialog->pUnmapTimer = g_timer_new ();  // starts the timer.
	}
	return TRUE;  // stops other handlers from being invoked for the event.
}

static GtkWidget *_cairo_dock_add_dialog_internal_box (CairoDialog *pDialog, int iWidth, int iHeight, gboolean bCanResize)
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

static CairoDialog *_cairo_dock_create_empty_dialog (gboolean bInteractive)
{
	//\________________ On cree un dialogue qu'on insere immediatement dans la liste.
	CairoDialog *pDialog = g_new0 (CairoDialog, 1);
	pDialog->container.iType = CAIRO_DOCK_TYPE_DIALOG;
	pDialog->iRefCount = 1;
	pDialog->container.fRatio = 1.;
	pDialog->container.iAnimationDeltaT = mySystem.iCairoAnimationDeltaT;

	//\________________ On construit la fenetre du dialogue.
	//GtkWidget* pWindow = gtk_window_new (bInteractiveWindow ? GTK_WINDOW_TOPLEVEL : GTK_WINDOW_POPUP);  // les popups ne prennent pas le focus. En fait, ils ne sont meme pas controles par le WM.
	GtkWidget* pWindow = cairo_dock_create_container_window_no_opengl ();
	
	pDialog->container.pWidget = pWindow;

	gtk_window_set_title (GTK_WINDOW (pWindow), "cairo-dock-dialog");
	if (! bInteractive)
		gtk_window_set_type_hint (GTK_WINDOW (pDialog->container.pWidget), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);  // pour ne pas prendre le focus.
	
	gtk_widget_add_events (pWindow, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_window_resize (GTK_WINDOW (pWindow), CAIRO_DIALOG_MIN_SIZE, CAIRO_DIALOG_MIN_SIZE);
	gtk_window_set_keep_above (GTK_WINDOW (pWindow), TRUE);
	gtk_widget_show_all (pWindow);
	
	return pDialog;
}

static cairo_surface_t *_cairo_dock_create_dialog_text_surface (const gchar *cText, CairoDockLabelDescription *pTextDescription, cairo_t *pSourceContext, int *iTextWidth, int *iTextHeight)
{
	if (cText == NULL)
		return NULL;
	//g_print ("%x;%x\n", pTextDescription, pSourceContext);
	cairo_surface_t *pTextBuffer = cairo_dock_create_surface_from_text_full (cText,
		pSourceContext,
		(pTextDescription ? pTextDescription : &myDialogs.dialogTextDescription),
		1.,
		0,
		iTextWidth,
		iTextHeight,
		NULL,
		NULL);
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
		int iWidth, iHeight;
		cairo_dock_get_icon_extent (pIcon, pContainer, &iWidth, &iHeight);
		pIconBuffer = cairo_dock_duplicate_surface (pIcon->pIconBuffer,
			pSourceContext,
			iWidth, iHeight,
			iDesiredSize, iDesiredSize);
	}
	else
	{
		double fImageWidth = iNbFrames * iDesiredSize, fImageHeight = iDesiredSize;
		pIconBuffer = cairo_dock_create_surface_from_image_simple (cImageFilePath,
			pSourceContext,
			fImageWidth,
			fImageHeight);
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
CairoDialog *cairo_dock_new_dialog (CairoDialogAttribute *pAttribute, Icon *pIcon, CairoContainer *pContainer)
{
	g_return_val_if_fail (pAttribute != NULL, NULL);
	if (cairo_dock_search_window_on_our_way (g_pMainDock, FALSE, TRUE) != NULL)
	{
		cd_debug ("skip dialog since a fullscreen window would mask it");
		return NULL;
	}
	
	//\________________ On cree un nouveau dialogue.
	CairoDialog *pDialog = _cairo_dock_create_empty_dialog (pAttribute->pInteractiveWidget || pAttribute->pActionFunc);
	pDialog->pIcon = pIcon;
	cairo_t *pSourceContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDialog));
	//cairo_t *pSourceContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (g_pMainDock?g_pMainDock:pDialog));
	
	//\________________ On cree la surface du message.
	if (pAttribute->cText != NULL)
	{
		pDialog->iMaxTextWidth = pAttribute->iMaxTextWidth;
		pDialog->pTextBuffer = _cairo_dock_create_dialog_text_surface (pAttribute->cText,
			pAttribute->pTextDescription, pSourceContext,
			&pDialog->iTextWidth, &pDialog->iTextHeight);
		///pDialog->iTextTexture = cairo_dock_create_texture_from_surface (pDialog->pTextBuffer);
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
		///pDialog->iIconTexture = cairo_dock_create_texture_from_surface (pDialog->pIconBuffer);
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
	pDialog->action_on_answer = pAttribute->pActionFunc;
	pDialog->pUserData = pAttribute->pUserData;
	pDialog->pFreeUserDataFunc = pAttribute->pFreeDataFunc;
	if (pAttribute->cButtonsImage != NULL && pAttribute->pActionFunc != NULL)
	{
		int i;
		for (i = 0; pAttribute->cButtonsImage[i] != NULL; i++);
		pDialog->iNbButtons = i;
		
		pDialog->pButtons = g_new0 (CairoDialogButton, pDialog->iNbButtons);
		gchar *cButtonImage;
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
					cButtonPath = cairo_dock_search_icon_s_path (cButtonImage);
				else
					cButtonPath = cButtonImage;
				pDialog->pButtons[i].pSurface = cairo_dock_create_surface_from_image_simple (cButtonPath,
					pSourceContext,
					myDialogs.iDialogButtonWidth,
					myDialogs.iDialogButtonHeight);
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

	//\________________ On lui affecte un decorateur.
	cairo_dock_set_dialog_decorator_by_name (pDialog, (pAttribute->cDecoratorName ? pAttribute->cDecoratorName : myDialogs.cDecoratorName));
	if (pDialog->pDecorator != NULL)
		pDialog->pDecorator->set_size (pDialog);
	
	//\________________ Maintenant qu'on connait tout, on calcule les tailles des divers elements.
	_cairo_dock_compute_dialog_sizes (pDialog);
	
	//\________________ On definit son orientation.
	cairo_dock_set_dialog_orientation (pDialog, pContainer);  // renseigne aussi bDirectionUp, bIsHorizontal, et iHeight.
	
	//\________________ On reserve l'espace pour les decorations.
	GtkWidget *pMainHBox = gtk_hbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (pDialog->container.pWidget), pMainHBox);
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
		cd_debug (" ref = %d", pAttribute->pInteractiveWidget->object.parent_instance.ref_count);
		gtk_box_pack_start (GTK_BOX (pDialog->pWidgetLayout),
			pDialog->pInteractiveWidget,
			FALSE,
			FALSE,
			0);
		cd_debug (" pack -> ref = %d", pAttribute->pInteractiveWidget->object.parent_instance.ref_count);
		g_print ("grab focus\n");
		gtk_window_present (GTK_WINDOW (pDialog->container.pWidget));
		gtk_widget_grab_focus (pDialog->pInteractiveWidget);
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
	
	//\________________ On connecte les signaux utiles.
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"expose-event",
		G_CALLBACK (on_expose_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"configure-event",
		G_CALLBACK (on_configure_dialog),
		pDialog);
	g_signal_connect (G_OBJECT (pDialog->container.pWidget),
		"unmap-event",
		G_CALLBACK (on_unmap_dialog),
		pDialog);
	
	cairo_destroy (pSourceContext);
	
	return pDialog;
}

void cairo_dock_free_dialog (CairoDialog *pDialog)
{
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
	
	cd_debug ("");

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
	
	gtk_widget_destroy (pDialog->container.pWidget);  // detruit aussi le widget interactif.
	
	if (pDialog->pUnmapTimer != NULL)
		g_timer_destroy (pDialog->pUnmapTimer);
	
	if (pDialog->pShapeBitmap != NULL)
		g_object_unref ((gpointer) pDialog->pShapeBitmap);
	
	if (pDialog->pUserData != NULL && pDialog->pFreeUserDataFunc != NULL)
		pDialog->pFreeUserDataFunc (pDialog->pUserData);
	
	if (pDialog->pIcon && pDialog->pIcon->cParentDockName != NULL)
		cairo_dock_dialog_window_destroyed ();
	g_free (pDialog);
}


static void _cairo_dock_dialog_calculate_aimed_point (Icon *pIcon, CairoContainer *pContainer, int *iX, int *iY, gboolean *bRight, CairoDockTypeHorizontality *bIsHorizontal, gboolean *bDirectionUp, double fAlign)
{
	g_return_if_fail (pIcon != NULL && pContainer != NULL);
	//g_print ("%s (%.2f, %.2f)\n", __func__, pIcon->fXAtRest, pIcon->fDrawX);
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		if (pDock->iRefCount > 0 && ! GTK_WIDGET_VISIBLE (pDock->container.pWidget))  // sous-dock invisible.  // pDock->bAtBottom
		{
			//g_print ("sous-dock invisible\n");
			CairoDock *pParentDock = NULL;
			Icon *pPointingIcon = cairo_dock_search_icon_pointing_on_dock (pDock, &pParentDock);
			_cairo_dock_dialog_calculate_aimed_point (pPointingIcon, CAIRO_CONTAINER (pParentDock), iX, iY, bRight, bIsHorizontal, bDirectionUp, fAlign);
		}
		else/* if (pDock->iRefCount == 0)*/  // un dock principal au repos.  // && pDock->bAtBottom
		{
			//g_print ("dock %d\n", pDock->iRefCount);
			*bIsHorizontal = (pDock->container.bIsHorizontal == CAIRO_DOCK_HORIZONTAL);
			int dy;
			if (pDock->iInputState == CAIRO_DOCK_INPUT_ACTIVE)
				dy = 0;
			else if (cairo_dock_is_hidden (pDock))
					dy = pDock->container.iHeight - MIN (myAccessibility.iVisibleZoneHeight, pDock->iMaxDockHeight);
			else
				dy = pDock->container.iHeight - pDock->iMinDockHeight;
			if (pDock->container.bIsHorizontal)
			{
				*bRight = (pIcon->fXAtRest > pDock->fFlatDockWidth / 2);
				*bDirectionUp = pDock->container.bDirectionUp;
				
				//g_print ("y = %d + %d\n", pDock->container.iWindowPositionY, dy);
				
				if (*bDirectionUp)
					*iY = pDock->container.iWindowPositionY + dy;
				else
					*iY = pDock->container.iWindowPositionY + pDock->container.iHeight - dy;
			}
			else
			{
				*bRight = (pDock->container.iWindowPositionY < g_desktopGeometry.iScreenWidth[CAIRO_DOCK_HORIZONTAL] / 2);
				*bDirectionUp = (pIcon->fXAtRest > pDock->fFlatDockWidth / 2);
				*iY = (! (*bRight) ? pDock->container.iWindowPositionY : pDock->container.iWindowPositionY + pDock->container.iHeight) + (pDock->container.bDirectionUp ? dy : -dy);
			}
			
			if (cairo_dock_is_hidden (pDock))
			{
				*iX = pDock->container.iWindowPositionX +
					pDock->iMaxDockWidth/2 -
					MIN (myAccessibility.iVisibleZoneWidth, pDock->iMaxDockWidth)/2 + 
					(pIcon->fXAtRest + pIcon->fWidth * (.5 + (*bRight ? .2 : -.2) * 2*(.5-fAlign))) / pDock->fFlatDockWidth * MIN (myAccessibility.iVisibleZoneWidth, pDock->iMaxDockWidth);
				//cd_debug ("placement sur un dock cache -> %d", *iX);
			}
			else
			{
				*iX = pDock->container.iWindowPositionX +
					pIcon->fDrawX + pIcon->fWidth * pIcon->fScale * (.5 + (*bRight ? .2 : -.2) * 2*(.5-fAlign));
			}
		}
	}
	else if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
		*bDirectionUp = (pDesklet->container.iWindowPositionY > g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
		*bIsHorizontal = (pDesklet->container.iWindowPositionX > 50 && pDesklet->container.iWindowPositionX + pDesklet->container.iHeight < g_desktopGeometry.iXScreenWidth[CAIRO_DOCK_HORIZONTAL] - 50);
		
		if (*bIsHorizontal)
		{
			*bRight = (pDesklet->container.iWindowPositionX > g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
			*iX = pDesklet->container.iWindowPositionX + pDesklet->container.iWidth * (*bRight ? .7 : .3);
			*iY = (*bDirectionUp ? pDesklet->container.iWindowPositionY : pDesklet->container.iWindowPositionY + pDesklet->container.iHeight);
		}
		else
		{
			*bRight = (pDesklet->container.iWindowPositionX < g_desktopGeometry.iXScreenHeight[CAIRO_DOCK_HORIZONTAL] / 2);
			*iY = pDesklet->container.iWindowPositionX + pDesklet->container.iWidth * (*bRight ? 1 : 0);
			*iX =pDesklet->container.iWindowPositionY + pDesklet->container.iHeight / 2;
		}
	}
}


void cairo_dock_set_dialog_orientation (CairoDialog *pDialog, CairoContainer *pContainer)
{
	if (pContainer != NULL && pDialog->pIcon != NULL)
	{
		_cairo_dock_dialog_calculate_aimed_point (pDialog->pIcon, pContainer, &pDialog->iAimedX, &pDialog->iAimedY, &pDialog->bRight, &pDialog->container.bIsHorizontal, &pDialog->container.bDirectionUp, pDialog->fAlign);
		
	}
	else
	{
		pDialog->container.bDirectionUp = TRUE;
	}
}


GtkWidget *cairo_dock_steal_widget_from_its_container (GtkWidget *pWidget)
{
	g_return_val_if_fail (pWidget != NULL, NULL);
	GtkWidget *pContainer = gtk_widget_get_parent (pWidget);
	if (pContainer != NULL)
	{
		cd_debug (" ref : %d", pWidget->object.parent_instance.ref_count);
		g_object_ref (G_OBJECT (pWidget));
		gtk_container_remove (GTK_CONTAINER (pContainer), pWidget);
		cd_debug (" -> %d", pWidget->object.parent_instance.ref_count);
	}
	return pWidget;
}

GtkWidget *cairo_dock_steal_interactive_widget_from_dialog (CairoDialog *pDialog)
{
	if (pDialog == NULL)
		return NULL;
	
	GtkWidget *pInteractiveWidget = pDialog->pInteractiveWidget;
	if (pInteractiveWidget != NULL)
	{
		pInteractiveWidget = cairo_dock_steal_widget_from_its_container (pInteractiveWidget);
		pDialog->pInteractiveWidget = NULL;
	}
	return pInteractiveWidget;
}

void cairo_dock_set_new_dialog_text_surface (CairoDialog *pDialog, cairo_surface_t *pNewTextSurface, int iNewTextWidth, int iNewTextHeight)
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
	_cairo_dock_compute_dialog_sizes (pDialog);

	if (pDialog->iMessageWidth != iPrevMessageWidth || pDialog->iMessageHeight != iPrevMessageHeight)
	{
		gtk_widget_set (pDialog->pMessageWidget, "width-request", pDialog->iMessageWidth, "height-request", pDialog->iMessageHeight, NULL);  // inutile de replacer le dialogue puisque sa gravite fera le boulot.
		
		gtk_widget_queue_draw (pDialog->container.pWidget);
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
	
	cairo_t *pSourceContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDialog));
	pDialog->pIconBuffer = cairo_dock_duplicate_surface (pNewIconSurface, pSourceContext, iNewIconSize, iNewIconSize, iNewIconSize, iNewIconSize);
	if (pDialog->iIconTexture != 0)
		_cairo_dock_delete_texture (pDialog->iIconTexture);
	///	pDialog->iIconTexture = cairo_dock_create_texture_from_surface (pDialog->pIconBuffer);
	
	pDialog->iIconSize = iNewIconSize;
	_cairo_dock_compute_dialog_sizes (pDialog);

	if (pDialog->iMessageWidth != iPrevMessageWidth || pDialog->iMessageHeight != iPrevMessageHeight)
	{
		gtk_widget_set (pDialog->pMessageWidget, "width-request", pDialog->iMessageWidth, "height-request", pDialog->iMessageHeight, NULL);  // inutile de replacer le dialogue puisque sa gravite fera le boulot.
		
		gtk_widget_queue_draw (pDialog->container.pWidget);
	}
	else
	{
		cairo_dock_damage_icon_dialog (pDialog);
	}
}


void cairo_dock_set_dialog_message (CairoDialog *pDialog, const gchar *cMessage)
{
	cairo_t *pSourceContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDialog));
	
	int iNewTextWidth=0, iNewTextHeight=0;
	cairo_surface_t *pNewTextSurface = _cairo_dock_create_dialog_text_surface (cMessage, NULL, pSourceContext, &iNewTextWidth, &iNewTextHeight);
	
	cairo_dock_set_new_dialog_text_surface (pDialog, pNewTextSurface, iNewTextWidth, iNewTextHeight);

	cairo_destroy (pSourceContext);
}
void cairo_dock_set_dialog_message_printf (CairoDialog *pDialog, const gchar *cMessageFormat, ...)
{
	g_return_if_fail (cMessageFormat != NULL);
	va_list args;
	va_start (args, cMessageFormat);
	gchar *cMessage = g_strdup_vprintf (cMessageFormat, args);
	cairo_dock_set_dialog_message (pDialog, cMessage);
	g_free (cMessage);
	va_end (args);
}

void cairo_dock_set_dialog_icon (CairoDialog *pDialog, const gchar *cImageFilePath)
{
	cairo_t *pSourceContext = cairo_dock_create_drawing_context_generic (CAIRO_CONTAINER (pDialog));

	cairo_surface_t *pNewIconSurface = cairo_dock_create_surface_for_square_icon (cImageFilePath, pSourceContext, myDialogs.iDialogIconSize);
	int iNewIconSize = (pNewIconSurface != NULL ? myDialogs.iDialogIconSize : 0);
			
	cairo_dock_set_new_dialog_icon_surface (pDialog, pNewIconSurface, iNewIconSize);

	cairo_destroy (pSourceContext);
}


void cairo_dock_damage_icon_dialog (CairoDialog *pDialog)
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

void cairo_dock_damage_text_dialog (CairoDialog *pDialog)
{
	if (!pDialog->container.bUseReflect)
		gtk_widget_queue_draw_area (pDialog->container.pWidget,
			pDialog->iLeftMargin + pDialog->iIconSize + CAIRO_DIALOG_TEXT_MARGIN,
			(pDialog->container.bDirectionUp ? 
				pDialog->iTopMargin :
				pDialog->container.iHeight - (pDialog->iTopMargin + pDialog->iBubbleHeight)),
			_drawn_text_width (pDialog),
			pDialog->iTextHeight);
	else
		gtk_widget_queue_draw (pDialog->container.pWidget);
}

void cairo_dock_damage_interactive_widget_dialog (CairoDialog *pDialog)
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
