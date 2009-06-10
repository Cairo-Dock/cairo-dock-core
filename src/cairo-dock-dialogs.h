
#ifndef __CAIRO_DIALOGS__
#define  __CAIRO_DIALOGS__

#include "cairo-dock-container.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-dialogs.h This class defines the dialog container. Dialogs are useful to bring interaction with the user.
* You can pop-up messages, ask for question, etc. Any GTK widget can be embedded inside a dialog, giving you any possible interaction with the user.
* Dialogs are constructed with a set of attributes grouped inside a #CairoDialogAttribute. A dialog contains the followinf optionnal components :
*   an icon
*   a text next to the icon
*   a interaction widget below the text and the icon
*   any buttons.
* To add buttons, you specify a list of images. "ok" and "cancel" are key words for the default ok/cancel buttons. You also has to provide a callback function that will be called on click. When the user clicks on a button, the function is called with the number of the clicked button, counted from 0. -1 and -2 are set if the user pushed the Return or Escape keys. The dialog is de-referenced after the user's answer, so you have to reference the dialog in the callback if you want to keep the dialog alive.
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
struct _CairoDialogDecorator {
	CairoDialogSetDecorationSizeFunc 		set_size;
	CairoDialogRenderDecorationFunc 		render;
	CairoDialogGLRenderDecorationFunc 		render_opengl;
	const gchar *cDisplayedName;
};

typedef void (* CairoDockActionOnAnswerFunc) (int iClickedButton, GtkWidget *pInteractiveWidget, gpointer data, CairoDialog *pDialog);

struct _CairoDialogAttribute {
	gchar *cImageFilePath;
	gint iNbFrames;  // 0 <=> 1.
	gchar *cText;
	gint iMaxTextWidth;  // 0 => pas de limite.
	CairoDockLabelDescription *pTextDescription;  // NULL => &myDialogs.dialogTextDescription
	GtkWidget *pInteractiveWidget;
	gchar **cButtonsImage;  // NULL-terminated
	CairoDockActionOnAnswerFunc pActionFunc;
	gpointer pUserData;
	GFreeFunc pFreeDataFunc;
	gint iTimeLength;
	gchar *cDecoratorName;
	gint iIconSize;
};

struct _CairoDialogButton {
	cairo_surface_t *pSurface;
	GLuint iTexture;
	int iOffset;  // offset courant du au clic.
};

struct _CairoDialog {
	/// type de container.
	CairoDockTypeContainer iType;
	/// la fenetre GTK du dialogue.
	GtkWidget *pWidget;
	/// largeur de la fenetre GTK du dialogue (pointe comprise).
	gint iWidth;
	/// hauteur de la fenetre GTK du dialogue (pointe comprise).
	gint iHeight;
	/// position en X du coin haut gauche de la fenetre GTK du dialogue.
	gint iPositionX;
	/// position en Y du coin haut gauche de la fenetre GTK du dialogue.
	gint iPositionY;
	/// vrai ssi la souris est dans le dialogue, auquel cas on le garde immobile.
	gboolean bInside;
	/// FALSE ssi le dialogue est perpendiculaire au dock.
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
	/// pour l'animation des dialogues.
	gint iSidGLAnimation;
	/// intervalle de temps entre 2 etapes de l'animation.
	gint iAnimationDeltaT;
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
	/// ratio des icones (non utilise).
	gdouble fRatio;
	/// transparence du reflet du dialogue, 0 si le decorateur ne gere pas le reflet.
	gdouble fReflectAlpha;
	/// contexte OpenGL associe a la fenetre.
	GLXContext glContext;
	/// TRUE <=> une animation lente est en cours.
	gboolean bKeepSlowAnimation;
	/// icone sur laquelle pointe le dialogue.
	Icon *pIcon;
	/// le moteur de rendu utilise pour dessiner le dialogue.
	CairoDialogRenderer *pRenderer;
	/// donnees pouvant etre utilisees par le moteur de rendu.
	gpointer pRendererData;
	/// position en X visee par la pointe dans le référentiel de l'écran.
	gint iAimedX;
	/// position en Y visee par la pointe dans le référentiel de l'écran.
	gint iAimedY;
	/// TRUE ssi le dialogue est a droite de l'écran; dialogue a droite <=> pointe a gauche.
	gboolean bRight;
	/// dimension de la surface du texte.
	gint iTextWidth, iTextHeight;
	/// surface representant le message du dialogue.
	cairo_surface_t* pTextBuffer;
	/// surface representant l'icone dans la marge a gauche du texte.
	cairo_surface_t* pIconBuffer;
	/// dimension de l'icone, sans les marges (0 si aucune icone).
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
	/// le timer pour la destruction automatique du dialogue.
	gint iSidTimer;
	/// reference atomique.
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
	/// position relative du dialogue par rapport a l'icone, ou ce qui revient au meme, de sa pointe par rapport au bord d'alignement.
	/// alignement de la pointe.
	gdouble fAlign;
	/// pour l'animation de l'icone.
	gint iNbFrames, iCurrentFrame;
	/// pour le defilement du texte.
	gint iMaxTextWidth;
	gint iCurrentTextOffset;
	/// les timer de ces 2 animations.
	gint iSidAnimateIcon, iSidAnimateText;
	int iNbButtons;
	CairoDialogButton *pButtons;
	GLuint iIconTexture, iTextTexture;
	gboolean bFinalized;
};


/**
*Teste si le container est un dialogue.
*@param pContainer le container.
*@return TRUE ssi le container a ete declare comme un dialogue.
*/
#define CAIRO_DOCK_IS_DIALOG(pContainer) (pContainer != NULL && (pContainer)->iType == CAIRO_DOCK_TYPE_DIALOG)
/**
*Caste un container en dialogue.
*@param pContainer le container.
*@return le dialogue.
*/
#define CAIRO_DIALOG(pContainer) ((CairoDialog *)pContainer)

void cairo_dock_load_dialog_buttons (CairoContainer *pContainer, gchar *cButtonOkImage, gchar *cButtonCancelImage);
void cairo_dock_unload_dialog_buttons (void);

/**
*Incremente le compteur de reference de 1 de maniere atomique, de maniere a empecher toute destruction du dialogue pendant son utilisation. Utiliser #cairo_dock_dialog_unreference apres en avoir termine.
*@param pDialog le dialogue.
*@return TRUE ssi la reference sur le dialogue n'etait pas deja nulle et a pu etre incrementee, FALSE sinon, auquel cas il ne faut pas l'utiliser.
*/
gboolean cairo_dock_dialog_reference (CairoDialog *pDialog);
/**
*Decremente le compteur de reference de 1 de maniere atomique. Si la reference est tombee a 0, tente de detruire le dialogue si la liste des dialogues n'est pas verouillee, et sinon l'isole (il sera alors detruit plus tard).
*@param pDialog le dialogue.
*@return TRUE ssi la reference sur le dialogue est tombee a zero, auquel cas il ne faut plus l'utiliser.
*/
gboolean cairo_dock_dialog_unreference (CairoDialog *pDialog);

/**
*Detruit un dialogue, libere les ressources allouees, et l'enleve de la liste des dialogues. Ne devrait pas etre utilise tel quel, utiliser #cairo_dock_dialog_unreference.
*@param pDialog le dialogue.
*/
void cairo_dock_free_dialog (CairoDialog *pDialog);
/**
*Dereference tous les dialogues pointant sur une icone; si les dialogues sont utilises par quelqu'un d'autre, il seront detruits plus tard, mais seront au moins isoles, et ne pourront donc plus etre utilises par une nouvelle personne.
*@param icon l'icone dont on veut supprimer les dialogues.
*@returns TRUE ssi au moins un dialogue a ete detruit.
*/
gboolean cairo_dock_remove_dialog_if_any (Icon *icon);



GtkWidget *cairo_dock_add_dialog_internal_box (CairoDialog *pDialog, int iWidth, int iHeight, gboolean bCanResize);


CairoDialog *cairo_dock_build_dialog (CairoDialogAttribute *pAttribute, Icon *pIcon, CairoContainer *pContainer);


void cairo_dock_dialog_calculate_aimed_point (Icon *pIcon, CairoContainer *pContainer, int *iX, int *iY, gboolean *bRight, CairoDockTypeHorizontality *bIsHorizontal, gboolean *bDirectionUp, double fAlign);

void cairo_dock_place_dialog (CairoDialog *pDialog, CairoContainer *pContainer);
/**
*Recalcule la positions de tous les dialogues et les y deplace.
*/
void cairo_dock_replace_all_dialogs (void);

void cairo_dock_compute_dialog_sizes (CairoDialog *pDialog);

/**
*Fait apparaitre un dialogue avec un message, un widget et 2 boutons, tous optionnels.
*@param cText le message du dialogue.
*@param pIcon l'icone sur laquelle pointe le dialogue.
*@param pContainer le container contenant l'icone.
*@param fTimeLength duree de vie du dialogue, ou 0 pour une duree de vie illimitee.
*@param cIconPath le chemin vers une icone a afficher dans la marge.
*@param pInteractiveWidget un widget d'interaction avec l'utilisateur; il est rattache au dialogue, donc sera detruit avec lui. Faire un 'gtk_widget_reparent()' avant de detruire le dialogue pour eviter cela, ou bien utilisez #cairo_dock_show_dialog_and_wait.
*@param pActionFunc la fonction d'action appelee lorsque l'utilisateur valide son choix. NULL revient a avoir des boutons du type GTK_BUTTONS_NONE.
*@param data donnees transmises a la fonction ci-dessus.
*@param pFreeDataFunc fonction de liberation des donnees ci-dessus, appelee lors de la destruction du dialogue, ou NULL si non necessaire.
*@return Le dialogue nouvellement cree et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_dialog_full (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, gchar *cIconPath, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/**
*Fait apparaitre un dialogue a duree de vie limitee avec une icone dans la marge a cote du texte.
*@param cText le message du dialogue.
*@param pIcon l'icone sur laquelle pointe le dialogue.
*@param pContainer le container de l'icone.
*@param fTimeLength la duree de vie du dialogue (ou 0 pour une duree de vie illimitee).
*@param cIconPath le chemin vers une icone.
*@param ... les arguments a inserer dans la chaine de texte, facon printf.
*@return Le dialogue nouvellement cree et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog_with_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, gchar *cIconPath, ...);
/**
*Fait apparaitre un dialogue a duree de vie limitee sans icone.
*@param cText le message du dialogue.
*@param pIcon l'icone sur laquelle pointe le dialogue.
*@param pContainer le container de l'icone.
*@param fTimeLength la duree de vie du dialogue (ou 0 pour une duree de vie illimitee).
*@param ... les arguments a inserer dans la chaine de texte.
*@return Le dialogue nouvellement cree et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, ...);
/**
*Fait apparaitre un dialogue a duree de vie limitee et avec l'icone par defaut.
*@param cText le message du dialogue.
*@param pIcon l'icone sur laquelle pointe le dialogue.
*@param pContainer le container de l'icone.
*@param fTimeLength la duree de vie du dialogue (ou 0 pour une duree de vie illimitee).
*@param ... les arguments a inserer dans la chaine de texte, facon printf.
*@return Le dialogue nouvellement cree et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_temporary_dialog_with_default_icon (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, ...);

/**
*Fait apparaitre un dialogue a duree de vie illimitee avec une question et 2 boutons oui/non. Lorsque l'utilisateur clique sur "oui", la fonction d'action est appelee avec "yes", et avec "no" s'il a clique sur "non".
*@param cText le message du dialogue.
*@param pIcon l'icone sur laquelle pointe le dialogue.
*@param pContainer le container de l'icone.
*@param cIconPath le chemin vers une icone a afficher dans la marge.
*@param pActionFunc la fonction d'action, appelee lors du clique utilisateur.
*@param data pointeur qui sera passe en argument de la fonction d'action.
*@param pFreeDataFunc fonction qui liberera le pointeur.
*@return Le dialogue nouvellement cree et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_dialog_with_question (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);
/**
*Fait apparaitre un dialogue a duree de vie illimitee avec une entree texte et 2 boutons ok/annuler. Lorsque l'utilisateur clique sur "ok", la fonction d'action est appelee avec le texte de l'entree, et avec "" s'il a clique sur "annuler".
*@param cText le message du dialogue.
*@param pIcon l'icone sur laquelle pointe le dialogue.
*@param pContainer le container de l'icone.
*@param cIconPath le chemin vers une icone a afficher dans la marge.
*@param cTextForEntry le texte a afficher initialement dans l'entree.
*@param pActionFunc la fonction d'action, appelee lors du clique utilisateur.
*@param data pointeur qui sera passe en argument de la fonction d'action.
*@param pFreeDataFunc fonction qui liberera le pointeur.
*@return le dialogue nouvellement cree.
*/
CairoDialog *cairo_dock_show_dialog_with_entry (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, const gchar *cTextForEntry, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);
/**
*Fait apparaitre un dialogue a duree de vie illimitee avec une echelle horizontale et 2 boutons ok/annuler. Lorsque l'utilisateur clique sur "ok", la fonction d'action est appelee avec la valeur de l'echelle sous forme de texte, et avec "-1" s'il a clique sur "annuler".
*@param cText le message du dialogue.
*@param pIcon l'icone sur laquelle pointe le dialogue.
*@param pContainer le container de l'icone.
*@param cIconPath le chemin vers une icone a afficher dans la marge.
*@param fValue la valeur initiale de l'echelle.
*@param pActionFunc la fonction d'action, appelee lors du clique utilisateur.
*@param data pointeur qui sera passe en argument de la fonction d'action.
*@param pFreeDataFunc fonction qui liberera le pointeur.
*@return le dialogue nouvellement cree.
*/
CairoDialog *cairo_dock_show_dialog_with_value (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, double fValue, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

/**
*Fait apparaitre un dialogue avec un widget et 2 boutons, et met en pause le programme jusqu'a ce que l'utilisateur ait fait son choix.
*@param cText le message du dialogue.
*@param pIcon l'icone sur laquelle pointe le dialogue.
*@param pContainer le container de l'icone.
*@param fTimeLength duree de vie du dialogue, ou 0 pour une duree de vie illimitee.
*@param cIconPath le chemin vers une icone a afficher dans la marge.
*@param pInteractiveWidget un widget d'interaction avec l'utilisateur.
*@return GTK_RESPONSE_OK si l'utilisateur a valide, GTK_RESPONSE_CANCEL s'il a annule, GTK_RESPONSE_NONE si le dialogue s'est fait detruire avant que l'utilisateur ait pu repondre. Le widget interactif n'est pas detruit avec le dialogue, ce qui permet de recuperer les modifications effectuees par l'utilisateur. Il vous appartient de le detruire avec gtk_widget_destroy() quand vous en avez fini avec lui.
*/
int cairo_dock_show_dialog_and_wait (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, gchar *cIconPath, GtkWidget *pInteractiveWidget);
/**
*Fait apparaitre un dialogue avec une entree de texte et 2 boutons ok/annuler, et met en pause le programme jusqu'a ce que l'utilisateur ait fait son choix.
*@param cMessage le message du dialogue.
*@param pIcon l'icone qui fait la demande.
*@param pContainer le container de l'icone.
*@param cInitialAnswer la valeur initiale de l'entree de texte, ou NULL si aucune n'est fournie.
*@return le texte entre par l'utilisateur, ou NULL s'il a annule ou si le dialogue s'est fait detruire avant.
*/
gchar *cairo_dock_show_demand_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, const gchar *cInitialAnswer);
/**
*Fait apparaitre un dialogue avec une echelle horizontale entre 0 et fMaxValue, et 2 boutons ok/annuler, et met en pause le programme jusqu'a ce que l'utilisateur ait fait son choix.
*@param cMessage le message du dialogue.
*@param pIcon l'icone qui fait la demande.
*@param pContainer le container de l'icone.
*@param fInitialValue la valeur initiale de l'echelle, entre 0 et 1.
*@param fMaxValue la borne superieure de l'intervalle [0 , max].
*@return la valeur choisie par l'utilisateur, ou -1 s'il a annule ou si le dialogue s'est fait detruire avant.
*/
double cairo_dock_show_value_and_wait (const gchar *cMessage, Icon *pIcon, CairoContainer *pContainer, double fInitialValue, double fMaxValue);
/**
*Fait apparaitre un dialogue de question pointant sur l'icone pointee (ou la 1ere si aucune n'est pointee) avec une question et 2 boutons oui/non, et met en pause le programme jusqu'a ce que l'utilisateur ait fait son choix.
*@param cQuestion la question a poser.
*@param pIcon l'icone qui fait la demande.
*@param pContainer le container de l'icone.
*@return GTK_RESPONSE_YES ou GTK_RESPONSE_NO suivant le choix de l'utilisateur, ou GTK_RESPONSE_NONE si le dialogue s'est fait detruire avant.
*/
int cairo_dock_ask_question_and_wait (const gchar *cQuestion, Icon *pIcon, CairoContainer *pContainer);


/**
*Cherche si une icone donnee possede au moins un dialogue.
*@param pIcon l'icone.
*@return TRUE ssi l'icone possede au moins un dialogue.
*/
gboolean cairo_dock_icon_has_dialog (Icon *pIcon);
/**
*Cherche une icone propice pour montrer un dialogue d'ordre general, dans le dock principal.
*@return une icone, ou NULL si le dock est vide.
*/
Icon *cairo_dock_get_dialogless_icon (void);


/**
*Fait apparaitre un dialogue de message non bloquant, et pointant sur une icone de separateur si possible, ou sinon sur l'icone pointee du dock principal (ou la 1ere icone sans dialogue si aucune n'est pointee). Cela permet a cairo-dock de poser une question d'ordre general.
*@param cMessage le message.
*@param fTimeLength la duree de vie du dialogue.
*@return Le dialogue nouvellement cree et visible, avec une reference a 1.
*/
CairoDialog * cairo_dock_show_general_message (const gchar *cMessage, double fTimeLength);
/**
*Fait apparaitre un dialogue de question bloquant, et pointant sur une icone de separateur si possible, ou sinon sur l'icone pointee du dock principal (ou la 1ere icone sans dialogue si aucune n'est pointee). Cela permet a cairo-dock de poser une question d'ordre general.
*@param cQuestion la question a poser.
*@return idem que pour #cairo_dock_ask_question_and_wait.
*/
int cairo_dock_ask_general_question_and_wait (const gchar *cQuestion);


/**
*Cache un dialogue, sans le detruire.
*@param pDialog le dialogue.
*/
void cairo_dock_hide_dialog (CairoDialog *pDialog);
/**
*Montre et fait prendre le focus a un dialogue.
*@param pDialog le dialogue.
*/
void cairo_dock_unhide_dialog (CairoDialog *pDialog);
/**
*Inverse la visibilite d'un dialogue (appelle simplement les 2 fonctions ci-dessus suivant la visibilité du dialogue).
*@param pDialog le dialogue.
*/
void cairo_dock_toggle_dialog_visibility (CairoDialog *pDialog);

/**
*Retire un widget de son container GTK sans le detruire Le widget peut alors etre replace ailleurs ulterieurement.
*@param pWidget le widget.
*/
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
