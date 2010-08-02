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
* This class only defines the constructor and destructor of a Dialog; to actually pop up a Dialog, use the Dialog Manager's functions in \ref cairo-dock-dialog-manager.h.
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
/// Definition of a Dialog decorator. It draws the frame of the Dialog.
struct _CairoDialogDecorator {
	CairoDialogSetDecorationSizeFunc 		set_size;
	CairoDialogRenderDecorationFunc 		render;
	CairoDialogGLRenderDecorationFunc 		render_opengl;
	const gchar *cDisplayedName;
};

/// Definition of a generic callback of a dialog, called when the user clicks on a button. Buttons are numbered from 0, -1 means 'Return' and -2 means 'Escape'.
typedef void (* CairoDockActionOnAnswerFunc) (int iClickedButton, GtkWidget *pInteractiveWidget, gpointer data, CairoDialog *pDialog);

/// Configuration attributes of a Dialog.
struct _CairoDialogAttribute {
	/// path to an image to display in the left margin, or NULL.
	const gchar *cImageFilePath;
	/// size of the icon in the left margin, or 0 to use the default one.
	gint iIconSize;
	
	/// number of frames of the image, if it's an animated image, otherwise 0.
	gint iNbFrames;  // 0 <=> 1.
	/// text of the message, or NULL.
	const gchar *cText;
	/// maximum authorized width of the message; it will scroll if it is too large. Set 0 to not limit it.
	gint iMaxTextWidth;  // 0 => pas de limite.
	/// A text rendering description of the message, or NULL to use the default one.
	CairoDockLabelDescription *pTextDescription;  // NULL => &myDialogs.dialogTextDescription
	
	/// a widget to interact with the user, or NULL.
	GtkWidget *pInteractiveWidget;
	/// a NULL-terminated list of images for buttons, or NULL. "ok" and "cancel" are key word to load the default "ok" and "cancel" buttons.
	const gchar **cButtonsImage;
	/// function that will be called when the user click on a button, or NULL.
	CairoDockActionOnAnswerFunc pActionFunc;
	/// data passed as a parameter of the callback, or NULL.
	gpointer pUserData;
	/// a function to free the data when the dialog is destroyed, or NULL.
	GFreeFunc pFreeDataFunc;
	
	/// life time of the dialog (in ms), or 0 for an unlimited dialog.
	gint iTimeLength;
	/// name of a decorator, or NULL to use the default one.
	const gchar *cDecoratorName;
	
	/// whether the dialog should be transparent to mouse input.
	gboolean bNoInput;
	/// whether to pop-up the dialog in front of al other windows, including fullscreen windows.
	gboolean bForceAbove;
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
	CairoContainer container;
	gint iRefCount;// conmpteur de reference.
	//\_____________________ Position
	Icon *pIcon;// icon sur laquelle pointe the dialog.
	gint iAimedX;// position en X visee par la pointe dans le referentiel de l'ecran.
	gint iAimedY;// position en Y visee par la pointe dans le referentiel de l'ecran.
	gboolean bRight;// TRUE ssi the dialog est a droite de l'écran; dialog a droite <=> pointe a gauche.
	//\_____________________ Structure interne.
	gint iBubbleWidth, iBubbleHeight;// dimensions de la bulle (message + widget utilisateur + boutons).
	gint iMessageWidth, iMessageHeight;// dimensions du message en comptant la marge du texte + vgap en bas si necessaire.
	gint iButtonsWidth, iButtonsHeight;// dimensions des boutons + vgap en haut.
	gint iInteractiveWidth, iInteractiveHeight;// dimensions du widget interactif.
	gint iDistanceToDock;// distance de la bulle au dock, donc hauteur totale de la pointe.
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
	gint iNbFrames, iCurrentFrame;// pour l'animation de the icon.
	gint iMaxTextWidth;// pour le defilement du texte.
	gint iCurrentTextOffset;// offset for text scrolling.
	gint iSidAnimateIcon, iSidAnimateText;// timers these 2 animations.
	gboolean bNoInput;// whether the dialog is transparent to mouse input.
	gboolean bAllowMinimize;// TRUE to allow the dialog to be minimized once. The flag is reseted to FALSE after the desklet has minimized.
	GdkBitmap* pShapeBitmap;
	GTimer *pUnmapTimer;
	gboolean bPositionForced;
	gdouble fAppearanceCounter;
	gboolean bTopBottomDialog;
};

#define CAIRO_DIALOG_MIN_SIZE 20
#define CAIRO_DIALOG_TEXT_MARGIN 3
#define CAIRO_DIALOG_MIN_ENTRY_WIDTH 150
#define CAIRO_DIALOG_MIN_SCALE_WIDTH 150
#define CAIRO_DIALOG_BUTTON_OFFSET 3
#define CAIRO_DIALOG_VGAP 4
#define CAIRO_DIALOG_BUTTON_GAP 16

/** Say if a Container is a Dialog.
*@param pContainer the container.
*@return TRUE if the container is a dialog.
*/
#define CAIRO_DOCK_IS_DIALOG(pContainer) (pContainer != NULL && (pContainer)->iType == CAIRO_DOCK_TYPE_DIALOG)

/** Cast a Container into a Dialog.
*@param pContainer the container.
*@return the dialog.
*/
#define CAIRO_DIALOG(pContainer) ((CairoDialog *)pContainer)

/** Creates a Dialog from a set of attributes. The Dialog is not placed, and has no interaction with the user.
*@param pAttribute attributes of the dialog.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@return a newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_new_dialog (CairoDialogAttribute *pAttribute, Icon *pIcon, CairoContainer *pContainer);

/** Free a Dialog and all its allocated ressources. Should never be used, use #cairo_dock_dialog_unreference instead.
*@param pDialog the dialog.
*/
void cairo_dock_free_dialog (CairoDialog *pDialog);


void cairo_dock_set_dialog_orientation (CairoDialog *pDialog, CairoContainer *pContainer);

GtkWidget *cairo_dock_steal_widget_from_its_container (GtkWidget *pWidget);
/** Detach the interactive widget from a dialog. The widget can then be placed anywhere after that. You have to unref it after you placed it into a container, or to destroy it.
*@param pDialog the desklet with an interactive widget.
*@return the widget.
*/
GtkWidget *cairo_dock_steal_interactive_widget_from_dialog (CairoDialog *pDialog);

//void cairo_dock_set_new_dialog_text_surface (CairoDialog *pDialog, cairo_surface_t *pNewTextSurface, int iNewTextWidth, int iNewTextHeight);
void cairo_dock_set_new_dialog_icon_surface (CairoDialog *pDialog, cairo_surface_t *pNewIconSurface, int iNewIconSize);

void cairo_dock_set_dialog_message (CairoDialog *pDialog, const gchar *cMessage);
void cairo_dock_set_dialog_message_printf (CairoDialog *pDialog, const gchar *cMessageFormat, ...);
void cairo_dock_set_dialog_icon (CairoDialog *pDialog, const gchar *cImageFilePath);

void cairo_dock_damage_icon_dialog (CairoDialog *pDialog);
void cairo_dock_damage_text_dialog (CairoDialog *pDialog);
void cairo_dock_damage_interactive_widget_dialog (CairoDialog *pDialog);


G_END_DECLS
#endif
