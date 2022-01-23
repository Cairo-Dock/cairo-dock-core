/*
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


#ifndef __CAIRO_DIALOG_FACTORY__
#define  __CAIRO_DIALOG_FACTORY__

#include "cairo-dock-container.h"
#include "cairo-dock-dialog-manager.h"  // CairoDialogAttr
G_BEGIN_DECLS

/** @file cairo-dock-dialog-factory.h This class defines the Dialog container, useful to bring interaction with the user.
* A Dialog is a container that points to an icon. It contains the following optionnal components :
* - a message
* - an image on its left
* - a interaction widget below it
* - some buttons at the bottom.
* 
* A Dialog is constructed with a set of attributes grouped inside a _CairoDialogAttribute.
* It has a Decorator that draws its shape, and a Renderer that draws its content.
* 
* To add buttons, you specify a list of images in the attributes. "ok" and "cancel" are key words for the default ok/cancel buttons. You also has to provide a callback function that will be called on click. When the user clicks on a button, the function is called with the number of the clicked button, counted from 0. -1 and -2 are set if the user pushed the Return or Escape keys. The dialog is unreferenced after the user's answer, so <i>you have to reference the dialog in the callback if you want to keep the dialog alive</i>.
* 
* This class defines various helper functions to build a Dialog.
* 
* Note that Dialogs and Menus share the same rendering.
*/

typedef gpointer CairoDialogRendererDataParameter;
typedef CairoDialogRendererDataParameter* CairoDialogRendererDataPtr;
typedef gpointer CairoDialogRendererConfigParameter;
typedef CairoDialogRendererConfigParameter* CairoDialogRendererConfigPtr;

typedef void (* CairoDialogRenderFunc) (cairo_t *pCairoContext, CairoDialog *pDialog, double fAlpha);
typedef void (* CairoDialogGLRenderFunc) (CairoDialog *pDialog, double fAlpha);
typedef gpointer (* CairoDialogConfigureRendererFunc) (CairoDialog *pDialog, CairoDialogRendererConfigPtr pConfig);
typedef void (* CairoDialogUpdateRendererDataFunc) (CairoDialog *pDialog, CairoDialogRendererDataPtr pNewData);
typedef void (* CairoDialogFreeRendererDataFunc) (CairoDialog *pDialog);
/// Definition of a Dialog renderer. It draws the inside of the Dialog.
struct _CairoDialogRenderer {
	CairoDialogRenderFunc 				render;
	CairoDialogConfigureRendererFunc 	configure;
	CairoDialogFreeRendererDataFunc 	free_data;
	CairoDialogUpdateRendererDataFunc 	update;
	CairoDialogGLRenderFunc 			render_opengl;
};

typedef void (* CairoDialogSetDecorationSizeFunc) (CairoDialog *pDialog);
typedef void (* CairoDialogRenderDecorationFunc) (cairo_t *pCairoContext, CairoDialog *pDialog);
typedef void (* CairoDialogGLRenderDecorationFunc) (CairoDialog *pDialog);
typedef void (* CairoMenuSetupFunc) (GtkWidget *pMenu);
typedef void (* CairoMenuRenderFunc) (GtkWidget *pMenu, cairo_t *pCairoContext);
/// Definition of a Dialog/Menu decorator. It draws the frame of the Dialog/Menu.
struct _CairoDialogDecorator {
	/// defines the various margins and alignment of the dialog
	CairoDialogSetDecorationSizeFunc    set_size;
	/// draw the dialog's frame (outline and background)
	CairoDialogRenderDecorationFunc     render;
	CairoDialogGLRenderDecorationFunc   render_opengl;  // not used, all drawings are done with cairo
	/// defines the GldiMenuParams of the menu (radius, alignment, arrow height)
	CairoMenuSetupFunc                  setup_menu;
	/// draw the menu's frame (outline and background); in the end, must clip the shape of the frame on the context
	CairoMenuRenderFunc                 render_menu;
	/// readable name of the decorator
	const gchar *cDisplayedName;
};

struct _CairoDialogButton {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	gint iOffset;  // offset courant du au clic.
	gint iDefaultType;  // used if no surface, 0 = Cancel image, 1 = Ok image.
};

/// Definition of a Dialog.
struct _CairoDialog {
	/// container.
	GldiContainer container;
	//\_____________________ Position
	Icon *pIcon;// icon sur laquelle pointe the dialog.
	gint iAimedX;// position en X visee par la pointe dans le referentiel de l'ecran.
	gint iAimedY;// position en Y visee par la pointe dans le referentiel de l'ecran.
	gboolean bRight;// TRUE ssi the dialog est a droite de l'écran; dialog a droite <=> pointe a gauche.
	gint iComputedPositionX;  // position du coin du dialogue dependant de sa gravite.
	gint iComputedPositionY;
	gint iComputedWidth;
	gint iComputedHeight;
	//\_____________________ Structure interne.
	gint iBubbleWidth, iBubbleHeight;// dimensions de la bulle (message + widget utilisateur + boutons).
	gint iMessageWidth, iMessageHeight;// dimensions du message en comptant la marge du texte + vgap en bas si necessaire.
	gint iButtonsWidth, iButtonsHeight;// dimensions des boutons + vgap en haut.
	gint iInteractiveWidth, iInteractiveHeight;// dimensions du widget interactif.
	GtkWidget *pLeftPaddingBox, *pRightPaddingBox, *pWidgetLayout;// la structure interne du widget.
	GtkWidget *pMessageWidget;// le widget de remplissage ou l'on dessine le message.
	GtkWidget *pButtonsWidget;// le widget de remplissage ou l'on dessine les boutons.
	GtkWidget *pTipWidget;// le widget de remplissage ou l'on dessine la pointe.
	GtkWidget *pTopWidget;// le widget de remplissage de la marge du haut.
	
	//\_____________________ Elements visibles.
	gint iTextWidth, iTextHeight;// dimension de la surface du texte.
	cairo_surface_t* pTextBuffer;// surface representant the message to display.
	GLuint iTextTexture;
	gint iIconSize;// dimension de the icon, sans les marges (0 si aucune icon).
	cairo_surface_t* pIconBuffer;// surface representant the icon dans la marge a gauche du texte.
	GLuint iIconTexture;
	GtkWidget *pInteractiveWidget;// le widget d'interaction utilisateur (GtkEntry, GtkHScale, zone de dessin, etc).
	gint iNbButtons;// number of buttons.
	CairoDialogButton *pButtons;// List of buttons.
	//\_____________________ Renderer
	CairoDialogRenderer *pRenderer;// le moteur de rendu utilise pour dessiner the dialog.
	gpointer pRendererData;// donnees pouvant etre utilisees par le moteur de rendu.
	//\_____________________ Decorateur
	CairoDialogDecorator *pDecorator;// le decorateur de fenetre.
	gint iLeftMargin, iRightMargin, iTopMargin, iBottomMargin, iMinFrameWidth, iMinBottomGap;// taille que s'est reserve le decorateur.
	gdouble fAlign;// alignement de la pointe.
	gint iIconOffsetX, iIconOffsetY;// decalage de l'icone.
	//\_____________________ Actions
	CairoDockActionOnAnswerFunc action_on_answer;// fonction appelee au clique sur l'un des boutons.
	gpointer pUserData;// donnees transmises a la fonction.
	GFreeFunc pFreeUserDataFunc;// fonction appelee pour liberer les donnees.
	
	gint iSidTimer;// le timer pour la destruction automatique du dialog.
	gboolean bUseMarkup;// whether markup is used to draw the text (as defined in the attributes on init)
	gboolean bNoInput;// whether the dialog is transparent to mouse input.
	gboolean bAllowMinimize;  // TRUE to allow the dialog to be minimized once. The flag is reseted to FALSE after the desklet has minimized.
	GTimer *pUnmapTimer;  // timer to filter 2 consecutive unmap events
	cairo_region_t* pShapeBitmap;
	gboolean bPositionForced;
	gdouble fAppearanceCounter;
	gboolean bTopBottomDialog;
	gboolean bHideOnClick;
	guint iButtonPressTime;
	gboolean bInAnswer;
	gchar *cText;
	// note: use an union to keep ABI
	union {
		// True if new positioning mechanism should be used. This means using gldi_container_move_to_rect()
		// and setting the aimed point in the configure event handlers in a way that should work on both X
		// and Wayland.
		guint f;
		gpointer reserved[1];
	} uFlags;
};

#define CAIRO_DIALOG_FIRST_BUTTON 0
#define CAIRO_DIALOG_ENTER_KEY -1
#define CAIRO_DIALOG_ESCAPE_KEY -2

#define CAIRO_DIALOG_MIN_SIZE 20
#define CAIRO_DIALOG_TEXT_MARGIN 3
#define CAIRO_DIALOG_MIN_ENTRY_WIDTH 150
#define CAIRO_DIALOG_MIN_SCALE_WIDTH 150
#define CAIRO_DIALOG_BUTTON_OFFSET 3
#define CAIRO_DIALOG_VGAP 4
#define CAIRO_DIALOG_BUTTON_GAP 16

#define CAIRO_DIALOG_FLAGS_USE_NEW_POSITIONING 1
#define CAIRO_DIALOG_FLAGS_PENDING_CLOSE 2

static inline gboolean gldi_dialog_use_new_positioning (CairoDialog *pDialog)
{
	return pDialog->uFlags.f & CAIRO_DIALOG_FLAGS_USE_NEW_POSITIONING;
}

/** Say if an object is a Dialog.
*@param obj the object.
*@return TRUE if the object is a dialog.
*/
#define CAIRO_DOCK_IS_DIALOG(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myDialogObjectMgr)

/** Cast a Container into a Dialog.
*@param pContainer the container.
*@return the dialog.
*/
#define CAIRO_DIALOG(pContainer) ((CairoDialog *)pContainer)

void gldi_dialog_init_internals (CairoDialog *pDialog, CairoDialogAttr *pAttribute);

/** Create a new dialog.
*@param pAttribute attributes of the dialog.
*@return the dialog.
*/
CairoDialog *gldi_dialog_new (CairoDialogAttr *pAttribute);

/** Pop up a dialog with a message, a widget, 2 buttons ok/cancel and an icon, all optionnal.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param cIconPath path to an icon to display in the margin.
*@param pInteractiveWidget a GTK widget; It is destroyed with the dialog. Use 'cairo_dock_steal_interactive_widget_from_dialog()' before if you want to keep it alive.
*@param pActionFunc the callback called when the user makes its choice. NULL means there will be no buttons.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data when the dialog is destroyed, or NULL if unnecessary.
*@return the newly created dialog.
*/
CairoDialog *gldi_dialog_show (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog with a message, and a limited duration, and an icon in the margin.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param cIconPath path to an icon.
*@param ... arguments to insert in the message, in a printf way.
*@return the newly created dialog.
*/
CairoDialog *gldi_dialog_show_temporary_with_icon_printf (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength, const gchar *cIconPath, ...) G_GNUC_PRINTF (1, 6);

/** Pop up a dialog with a message, and a limited duration, and an icon in the margin.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param cIconPath path to an icon.
*@return the newly created dialog.
*/
CairoDialog *gldi_dialog_show_temporary_with_icon (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength, const gchar *cIconPath);

/** Pop up a dialog with a message, and a limited duration, with no icon.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@return the newly created dialog et visible, avec une reference a 1.
*/
CairoDialog *gldi_dialog_show_temporary (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength);

/** Pop up a dialog with a message, and a limited duration, and a default icon.
*@param cText the format of the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@return the newly created dialog et visible, avec une reference a 1.
*/
CairoDialog *gldi_dialog_show_temporary_with_default_icon (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, double fTimeLength);

/** Pop up a dialog with a question and 2 buttons ok/cancel.
* The dialog is unreferenced after the user has answered, so if you want to keep it alive, you have to reference it in the callback.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param pActionFunc the callback.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data.
*@return the newly created dialog et visible, avec une reference a 1.
*/
CairoDialog *gldi_dialog_show_with_question (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, const gchar *cIconPath, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog with a text entry and 2 buttons ok/cancel.
* The dialog is unreferenced after the user has answered, so if you want to keep it alive, you have to reference it in the callback.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param cTextForEntry text to display initially in the entry.
*@param pActionFunc the callback.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data.
*@return the newly created dialog.
*/
CairoDialog *gldi_dialog_show_with_entry (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, const gchar *cIconPath, const gchar *cTextForEntry, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog with an horizontal scale between 0 and fMaxValue and 2 buttons ok/cancel.
* The dialog is unreferenced after the user has answered, so if you want to keep it alive, you have to reference it in the callback.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param fValue initial value of the scale.
*@param fMaxValue maximum value of the scale.
*@param pActionFunc the callback.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data.
*@return the newly created dialog.
*/
CairoDialog *gldi_dialog_show_with_value (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, const gchar *cIconPath, double fValue, double fMaxValue, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog, pointing on "the best icon possible". This allows to display a general message.
*@param cMessage the message.
*@param fTimeLength life time of the dialog, in ms.
*@return the newly created dialog, visible and with a reference of 1.
*/
CairoDialog * gldi_dialog_show_general_message (const gchar *cMessage, double fTimeLength);

/** Pop up a dialog with GTK widget and 2 buttons ok/cancel, and block until the user makes its choice.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param pInteractiveWidget an interactive widget.
*@return the number of the button that was clicked : 0 or -1 for OK, 1 or -2 for CANCEL, -3 if the dialog has been destroyed before. The dialog is destroyed after the user choosed, but the interactive widget is not destroyed, which allows to retrieve the changes made by the user. Destroy it with 'gtk_widget_destroy' when you're done with it.
*/
int gldi_dialog_show_and_wait (const gchar *cText, Icon *pIcon, GldiContainer *pContainer, const gchar *cIconPath, GtkWidget *pInteractiveWidget);


GtkWidget *cairo_dock_steal_widget_from_its_container (GtkWidget *pWidget);  // should be elsewhere

/** Detach the interactive widget from a dialog. The widget can then be placed anywhere after that. You have to unref it after you placed it into a container, or to destroy it.
*@param pDialog the desklet with an interactive widget.
*@return the widget.
*/
GtkWidget *gldi_dialog_steal_interactive_widget (CairoDialog *pDialog);

void gldi_dialog_redraw_interactive_widget (CairoDialog *pDialog);

void gldi_dialog_set_icon (CairoDialog *pDialog, const gchar *cImageFilePath);
void gldi_dialog_set_icon_surface (CairoDialog *pDialog, cairo_surface_t *pNewIconSurface, int iNewIconSize);

void gldi_dialog_set_message (CairoDialog *pDialog, const gchar *cMessage);
void gldi_dialog_set_message_printf (CairoDialog *pDialog, const gchar *cMessageFormat, ...);

void gldi_dialog_reload (CairoDialog *pDialog);

#define gldi_dialog_set_widget_text_color(...)
#define gldi_dialog_set_widget_bg_color(...)

G_END_DECLS
#endif
