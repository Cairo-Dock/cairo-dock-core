
#ifndef __CAIRO_DIALOGS__
#define  __CAIRO_DIALOGS__

#include "cairo-dock-container.h"
G_BEGIN_DECLS

/** @file cairo-dock-dialogs.h This class defines the dialog container, that are useful to bring interaction with the user.
* 
* With dialogs, you can pop-up messages, ask for question, etc. Any GTK widget can be embedded inside a dialog, giving you any possible interaction with the user.
* 
* Dialogs are constructed with a set of attributes grouped inside a _CairoDialogAttribute. A dialog contains the following optionnal components :
* - a message
* - an image on its left
* - a interaction widget below it
* - some buttons at the bottom.
* 
* To add buttons, you specify a list of images in the attributes. "ok" and "cancel" are key words for the default ok/cancel buttons. You also has to provide a callback function that will be called on click. When the user clicks on a button, the function is called with the number of the clicked button, counted from 0. -1 and -2 are set if the user pushed the Return or Escape keys. The dialog is unreferenced after the user's answer, so <i>you have to reference the dialog in the callback if you want to keep the dialog alive</i>.
* 
* The most generic way to build a Dialog is to fill a _CairoDialogAttribute and pass it to \ref cairo_dock_build_dialog.
* 
* But in most of case, you can just use one of the following convenient functions, that will do the job for you.
* - to show a message, you can use \ref cairo_dock_show_temporary_dialog_with_icon
* - to ask the user a choice, a value or a text, you can use \ref cairo_dock_show_dialog_with_question, \ref cairo_dock_show_dialog_with_value or \ref cairo_dock_show_dialog_with_entry.
* - if you need to block while waiting for the user, use the xxx_and_wait version of these functions.
* - if you want to pop up only 1 dialog at once on a given icon, use \ref cairo_dock_remove_dialog_if_any before you pop up your dialog.
*/

typedef gpointer CairoDialogRendererDataParameter;
typedef CairoDialogRendererDataParameter* CairoDialogRendererDataPtr;
typedef gpointer CairoDialogRendererConfigParameter;
typedef CairoDialogRendererConfigParameter* CairoDialogRendererConfigPtr;

typedef void (* CairoDialogRenderFunc) (cairo_t *pCairoContext, CairoDialog *pDialog, double fAlpha);
typedef void (* CairoDialogGLRenderFunc) (CairoDialog *pDialog, double fAlpha);
typedef gpointer (* CairoDialogConfigureRendererFunc) (CairoDialog *pDialog, cairo_t *pSourceContext, CairoDialogRendererConfigPtr pConfig);
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

/// Definition of a generic callback of a dialog, called when the user clicks on a button. Buttons unmber starts wito 0, -1 means 'Return' and -2 means 'Escape'.
typedef void (* CairoDockActionOnAnswerFunc) (int iClickedButton, GtkWidget *pInteractiveWidget, gpointer data, CairoDialog *pDialog);

/// Configuration attributes of a Dialog, which derives from a Container.
struct _CairoDialogAttribute {
	/// path to an image to display in the left margin, or NULL.
	gchar *cImageFilePath;
	/// size of the icon in the left margin, or 0 to use the default one.
	gint iIconSize;
	
	/// number of frames of the image, if it's an animated image, otherwise 0.
	gint iNbFrames;  // 0 <=> 1.
	/// text of the message, or NULL.
	gchar *cText;
	/// maximum authorized width of the message; it will scroll if it is too large. Set 0 to not limit it.
	gint iMaxTextWidth;  // 0 => pas de limite.
	/// A text rendering description of the message, or NULL to use the default one.
	CairoDockLabelDescription *pTextDescription;  // NULL => &myDialogs.dialogTextDescription
	
	/// a widget to interact with the user, or NULL.
	GtkWidget *pInteractiveWidget;
	/// a NULL-terminated list of images for buttons, or NULL. "ok" and "cancel" are key word to load the default "ok" and "cancel" buttons.
	gchar **cButtonsImage;
	/// function that will be called when the user click on a button, or NULL.
	CairoDockActionOnAnswerFunc pActionFunc;
	/// data passed as a parameter of the callback, or NULL.
	gpointer pUserData;
	/// a function to free the data when the dialog is destroyed, or NULL.
	GFreeFunc pFreeDataFunc;
	
	/// life time of the dialog, or 0 for an unlimited dialog.
	gint iTimeLength;
	/// name of a decorator, or NULL to use the default one.
	gchar *cDecoratorName;
};

struct _CairoDialogButton {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	int iOffset;  // offset courant du au clic.
};

/// Definition of a Dialog.
struct _CairoDialog {
	/// type of container.
	CairoDockTypeContainer iType;
	/// GTK window of the dialog.
	GtkWidget *pWidget;
	/// largeur de la fenetre GTK du dialog (pointe comprise).
	gint iWidth;
	/// hauteur de la fenetre GTK du dialog (pointe comprise).
	gint iHeight;
	/// position en X du coin haut gauche de la fenetre GTK du dialog.
	gint iPositionX;
	/// position en Y du coin haut gauche de la fenetre GTK du dialog.
	gint iPositionY;
	/// vrai ssi la souris est dans the dialog, auquel cas on le garde immobile.
	gboolean bInside;
	/// FALSE ssi the dialog est perpendiculaire au dock.
	CairoDockTypeHorizontality bIsHorizontal;
	/// TRUE ssi la pointe est orientée vers le bas.
	gboolean bDirectionUp;
#ifdef HAVE_GLITZ
	glitz_drawable_format_t *pDrawFormat;
	glitz_drawable_t* pGlitzDrawable;
	glitz_format_t* pGlitzFormat;
#else
	gpointer padding[3];
#endif // HAVE_GLITZ
	/// Donnees exterieures.
	gpointer pDataSlot[CAIRO_DOCK_NB_DATA_SLOT];
	/// pour l'animation des dialogs.
	gint iSidGLAnimation;
	/// intervalle de temps entre 2 etapes de l'animation.
	gint iAnimationDeltaT;
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
	/// ratio des icons (non utilise).
	gdouble fRatio;
	/// transparence du reflet du dialog, 0 si le decorateur ne gere pas le reflet.
	gdouble fReflectAlpha;
	/// contexte OpenGL associe a la fenetre.
	GLXContext glContext;
	/// TRUE <=> une animation lente est en cours.
	gboolean bKeepSlowAnimation;
	/// compteur pour l'animation (inutilise pour les dialogs).
	gint iAnimationStep;
	/// icon sur laquelle pointe the dialog.
	Icon *pIcon;
	/// le moteur de rendu utilise pour dessiner the dialog.
	CairoDialogRenderer *pRenderer;
	/// donnees pouvant etre utilisees par le moteur de rendu.
	gpointer pRendererData;
	/// position en X visee par la pointe dans le référentiel de l'écran.
	gint iAimedX;
	/// position en Y visee par la pointe dans le référentiel de l'écran.
	gint iAimedY;
	/// TRUE ssi the dialog est a droite de l'écran; dialog a droite <=> pointe a gauche.
	gboolean bRight;
	/// dimension de la surface du texte.
	gint iTextWidth, iTextHeight;
	/// surface representant the message to display.
	cairo_surface_t* pTextBuffer;
	/// surface representant the icon dans la marge a gauche du texte.
	cairo_surface_t* pIconBuffer;
	/// dimension de the icon, sans les marges (0 si aucune icon).
	gint iIconSize;
	/// dimensions de la bulle (message + widget utilisateur + boutons).
	gint iBubbleWidth, iBubbleHeight;
	/// dimensions du message en comptant la marge du texte + vgap en bas si necessaire.
	gint iMessageWidth, iMessageHeight;
	/// dimensions des boutons + vgap en haut.
	gint iButtonsWidth, iButtonsHeight;
	/// dimensions du widget interactif.
	gint iInteractiveWidth, iInteractiveHeight;
	/// distance de la bulle au dock, donc hauteur totale de la pointe.
	gint iDistanceToDock;
	/// le widget de remplissage ou l'on dessine le message.
	GtkWidget *pMessageWidget;
	/// le widget de remplissage ou l'on dessine les boutons.
	GtkWidget *pButtonsWidget;
	/// le widget de remplissage ou l'on dessine la pointe.
	GtkWidget *pTipWidget;
	/// le timer pour la destruction automatique du dialog.
	gint iSidTimer;
	/// conmpteur de reference.
	gint iRefCount;
	/// le widget d'interaction utilisateur (GtkEntry, GtkHScale, zone de dessin, etc).
	GtkWidget *pInteractiveWidget;
	/// fonction appelee au clique sur l'un des boutons.
	CairoDockActionOnAnswerFunc action_on_answer;
	/// donnees transmises a la fonction.
	gpointer pUserData;
	/// fonction appelee pour liberer les donnees.
	GFreeFunc pFreeUserDataFunc;
	/// la structure interne du widget.
	GtkWidget *pLeftPaddingBox, *pRightPaddingBox, *pWidgetLayout;
	/// le decorateur de fenetre.
	CairoDialogDecorator *pDecorator;
	/// taille que s'est reserve le decorateur.
	gint iLeftMargin, iRightMargin, iTopMargin, iBottomMargin, 	iMinFrameWidth, iMinBottomGap;
	/// position relative du dialog par rapport a the icon, ou ce qui revient au meme, de sa pointe par rapport au bord d'alignement.
	/// alignement de la pointe.
	gdouble fAlign;
	/// pour l'animation de the icon.
	gint iNbFrames, iCurrentFrame;
	/// pour le defilement du texte.
	gint iMaxTextWidth;
	/// offset for text scrolling.
	gint iCurrentTextOffset;
	/// timers these 2 animations.
	gint iSidAnimateIcon, iSidAnimateText;
	/// number of buttons.
	int iNbButtons;
	/// List of buttons.
	CairoDialogButton *pButtons;
	/// textures.
	GLuint iIconTexture, iTextTexture;
	gboolean bFinalized;
};


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

void cairo_dock_load_dialog_buttons (CairoContainer *pContainer, gchar *cButtonOkImage, gchar *cButtonCancelImage);
void cairo_dock_unload_dialog_buttons (void);

/** Increase by 1 the reference of a dialog. Use #cairo_dock_dialog_unreference when you're done, so that the dialog can be destroyed.
*@param pDialog the dialog.
*@return TRUE if the reference was not nul, otherwise you must not use it.
*/
gboolean cairo_dock_dialog_reference (CairoDialog *pDialog);

/** Decrease by 1 the reference of a dialog. If the reference becomes nul, the disalog is destroyed.
*@param pDialog the dialog.
*@return TRUE if the reference became nul, in which case the dialog must not be used anymore.
*/
gboolean cairo_dock_dialog_unreference (CairoDialog *pDialog);

/** Free a dialog and all its allocated ressources, and remove it from the list of dialogs. Should never be used, use #cairo_dock_dialog_unreference instead.
*@param pDialog the dialog.
*/
void cairo_dock_free_dialog (CairoDialog *pDialog);

/** Unreference all the dialogs pointed by an icon.
*@param icon the icon you want to delete all dialogs from.
*@returns TRUE if at least one dialog has been unreferenced.
*/
gboolean cairo_dock_remove_dialog_if_any (Icon *icon);

GtkWidget *cairo_dock_add_dialog_internal_box (CairoDialog *pDialog, int iWidth, int iHeight, gboolean bCanResize);


/** Generic function to pop up a dialog.
*@param pAttribute attributes of the dialog.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@return a newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_build_dialog (CairoDialogAttribute *pAttribute, Icon *pIcon, CairoContainer *pContainer);


void cairo_dock_dialog_calculate_aimed_point (Icon *pIcon, CairoContainer *pContainer, int *iX, int *iY, gboolean *bRight, CairoDockTypeHorizontality *bIsHorizontal, gboolean *bDirectionUp, double fAlign);

void cairo_dock_place_dialog (CairoDialog *pDialog, CairoContainer *pContainer);

void cairo_dock_replace_all_dialogs (void);

void cairo_dock_compute_dialog_sizes (CairoDialog *pDialog);

/** Pop up a dialog with a message, a widget, 2 buttons ok/cancel and an icon, all optionnal.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param cIconPath path to an icon to display in the margin.
*@param pInteractiveWidget a GTK widget; It is destroyed with the dialog. Use 'gtk_widget_reparent()' before if you want to keep it alive, or use #cairo_dock_show_dialog_and_wait.
*@param pActionFunc the callback called when the user makes its choice. NULL means there will be no buttons.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data when the dialog is destroyed, or NULL if unnecessary.
*@return the newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_show_dialog_full (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog with a message, and a limited duration, and an icon in the margin.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param cIconPath path to an icon.
*@param ... arguments to insert in the message, in a printf way.
*@return the newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog_with_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, ...);

/** Pop up a dialog with a message, and a limited duration, with no icon.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param ... arguments to insert in the message, in a printf way.
*@return the newly created dialog, visible, with a reference of 1 et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, ...);

/** Pop up a dialog with a message, and a limited duration, and a default icon.
*@param cText the format of the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength the duration of the dialog (in ms), or 0 for an unlimited dialog.
*@param ... arguments to insert in the message, in a printf way.
*@return the newly created dialog, visible, with a reference of 1 et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog_with_default_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, ...);

/** Pop up a dialog with a question and 2 buttons ok/cancel.
* When the user make its choice, the callback is called with the value "yes" if he clicked on "ok", and with "no" if he clicked on "cancel".
* The dialog is unreferenced after that, so if you want to keep it alive, you have to reference it in the callback.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param pActionFunc the callback.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data.
*@return the newly created dialog, visible, with a reference of 1 et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_dialog_with_question (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconPath, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog with a text entry and 2 buttons ok/cancel.
* When the user make its choice, the callback is called with the value of the entry if he clicked on "ok", and with "" if he clicked on "cancel".
* The dialog is unreferenced after that, so if you want to keep it alive, you have to reference it in the callback.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param cTextForEntry text to display initially in the entry.
*@param pActionFunc the callback.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data.
*@return the newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_show_dialog_with_entry (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconPath, const gchar *cTextForEntry, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/** Pop up a dialog with an horizontal scale between 0 and fMaxValue and 2 buttons ok/cancel.
* When the user make its choice, the callback is called with the value of the scale in a text form if he clicked on "ok", and with "-1" if he clicked on "cancel".
* The dialog is unreferenced after that, so if you want to keep it alive, you have to reference it in the callback.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cIconPath path to an icon to display in the margin.
*@param fValue initial value of the scale.
*@param pActionFunc the callback.
*@param data data passed as a parameter of the callback.
*@param pFreeDataFunc function used to free the data.
*@return the newly created dialog, visible, with a reference of 1.
*/
CairoDialog *cairo_dock_show_dialog_with_value (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconPath, double fValue, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);



/** Pop up a dialog with GTK widget and 2 buttons ok/cancel, and block until the user makes its choice.
*@param cText the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fTimeLength time length of the dialog, or 0 for an unlimited dialog.
*@param cIconPath path to an icon to display in the margin.
*@param pInteractiveWidget an interactive widget.
*@return GTK_RESPONSE_OK if the user has valideated, GTK_RESPONSE_CANCEL if he cancelled, GTK_RESPONSE_NONE if the dialog has been destroyed before. The dialog is destroyed after the user choosed, but the interactive widget is not destroyed, which allows to retrieve the changes made by the user. Destroy it with 'gtk_widget_destroy' when you're done with it.
*/
int cairo_dock_show_dialog_and_wait (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, const gchar *cIconPath, GtkWidget *pInteractiveWidget);

/** Pop up a dialog with a text entry, and 2 buttons ok/cancel, and block until the user makes its choice.
*@param cMessage the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param cInitialAnswer the initial value of the entry (can be NULL).
*@return the text entered by the user, or NULL if he cancelled or if the dialog has been destroyed before.
*/
gchar *cairo_dock_show_demand_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, const gchar *cInitialAnswer);

/** Pop up a dialog with an horizontal scale between 0 and fMaxValue, and 2 buttons ok/cancel, and block until the user makes its choice.
*@param cMessage the message to display.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@param fInitialValue the initial value of the scale.
*@param fMaxValue the maximum value of the scale.
*@return the value choosed by the user, or -1 if he cancelled or if the dialog has been destroyed before.
*/
double cairo_dock_show_value_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, double fInitialValue, double fMaxValue);

/** Pop up a dialog with a question and 2 buttons yes/no, and block until the user makes its choice.
*@param cQuestion the question to ask.
*@param pIcon the icon that will hold the dialog.
*@param pContainer the container of the icon.
*@return GTK_RESPONSE_YES ou GTK_RESPONSE_NO according to the user's choice, or GTK_RESPONSE_NONE if the dialog has been destroyed before.
*/
int cairo_dock_ask_question_and_wait (const gchar *cQuestion, Icon *pIcon, CairoContainer *pContainer);


/** Test if an icon has at least one dialog.
*@param pIcon the icon.
*@return TRUE if the icon has one or more dialog(s).
*/
gboolean cairo_dock_icon_has_dialog (Icon *pIcon);

/** Search the "the best icon possible" to hold a general dialog.
*@return an icon, never NULL except if the dock is.
*/
Icon *cairo_dock_get_dialogless_icon (void);


/** Pop up a dialog, pointing on "the best icon possible". This allows to display a general message.
*@param cMessage le message.
*@param fTimeLength la duree de vie du dialog.
*@return the newly created dialog, visible and with a reference of 1.
*/
CairoDialog * cairo_dock_show_general_message (const gchar *cMessage, double fTimeLength);

/* Pop up a blocking dialog with a question, pointing on "the best icon possible". This allows to ask a general question.
*@param cQuestion the question to ask.
*@return same #cairo_dock_ask_question_and_wait.
*/
int cairo_dock_ask_general_question_and_wait (const gchar *cQuestion);


/** Hide a dialog.
*@param pDialog the dialog.
*/
void cairo_dock_hide_dialog (CairoDialog *pDialog);
/** Show a dialog and give it focus.
*@param pDialog the dialog.
*/
void cairo_dock_unhide_dialog (CairoDialog *pDialog);
/** Toggle the visibility of a dialog.
*@param pDialog the dialog.
*/
void cairo_dock_toggle_dialog_visibility (CairoDialog *pDialog);

GtkWidget *cairo_dock_steal_widget_from_its_container (GtkWidget *pWidget);


void cairo_dock_set_new_dialog_text_surface (CairoDialog *pDialog, cairo_surface_t *pNewTextSurface, int iNewTextWidth, int iNewTextHeight);
void cairo_dock_set_new_dialog_icon_surface (CairoDialog *pDialog, cairo_surface_t *pNewIconSurface, int iNewIconSize);

void cairo_dock_set_dialog_message (CairoDialog *pDialog, const gchar *cMessage);
void cairo_dock_set_dialog_message_printf (CairoDialog *pDialog, const gchar *cMessageFormat, ...);
void cairo_dock_set_dialog_icon (CairoDialog *pDialog, const gchar *cImageFilePath);

void cairo_dock_damage_icon_dialog (CairoDialog *pDialog);
void cairo_dock_damage_text_dialog (CairoDialog *pDialog);
void cairo_dock_damage_interactive_widget_dialog (CairoDialog *pDialog);


G_END_DECLS
#endif
