
#ifndef __CAIRO_DIALOGS__
#define  __CAIRO_DIALOGS__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

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


void cairo_dock_dialog_calculate_aimed_point (Icon *pIcon, CairoContainer *pContainer, int *iX, int *iY, gboolean *bRight, CairoDockTypeHorizontality *bIsHorizontal, gboolean *bDirectionUp);

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
*@param iButtonsType type des boutons (GTK_BUTTONS_OK_CANCEL, GTK_BUTTONS_YES_NO, ou GTK_BUTTONS_NONE).
*@param pInteractiveWidget un widget d'interaction avec l'utilisateur; il est rattache au dialogue, donc sera detruit avec lui. Faire un 'gtk_widget_reparent()' avant de detruire le dialogue pour eviter cela, ou bien utilisez #cairo_dock_show_dialog_and_wait.
*@param pActionFunc la fonction d'action appelee lorsque l'utilisateur valide son choix. NULL revient a avoir des boutons du type GTK_BUTTONS_NONE.
*@param data donnees transmises a la fonction ci-dessus.
*@param pFreeDataFunc fonction de liberation des donnees ci-dessus, appelee lors de la destruction du dialogue, ou NULL si non necessaire.
*@return Le dialogue nouvellement cree et visible, avec une reference a 1.
*/
CairoDialog *cairo_dock_show_dialog_full (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, gchar *cIconPath, GtkButtonsType iButtonsType, GtkWidget *pInteractiveWidget, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);

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
CairoDialog *cairo_dock_show_dialog_with_entry (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, gchar *cIconPath, const gchar  *cTextForEntry, CairoDockActionOnAnswerFunc pActionFunc, gpointer data, GFreeFunc pFreeDataFunc);
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
*@param iButtonsType type des boutons (GTK_BUTTONS_OK_CANCEL ou GTK_BUTTONS_YES_NO).
*@param pInteractiveWidget un widget d'interaction avec l'utilisateur.
*@return GTK_RESPONSE_OK si l'utilisateur a valide, GTK_RESPONSE_CANCEL s'il a annule, GTK_RESPONSE_NONE si le dialogue s'est fait detruire avant que l'utilisateur ait pu repondre. Le widget interactif n'est pas detruit avec le dialogue, ce qui permet de recuperer les modifications effectuees par l'utilisateur. Il vous appartient de le detruire avec gtk_widget_destroy() quand vous en avez fini avec lui.
*/
int cairo_dock_show_dialog_and_wait (const gchar *cText, Icon *pIcon, CairoContainer *pContainer, double fTimeLength, gchar *cIconPath, GtkButtonsType iButtonsType, GtkWidget *pInteractiveWidget);
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
*Retire un widget de son container GTK sans le detruire Le widget peut alors etre replace ailleurs ulterieurement.
*@param pWidget le widget.
*/
GtkWidget *cairo_dock_steal_widget_from_its_container (GtkWidget *pWidget);


void cairo_dock_set_new_dialog_text_surface (CairoDialog *pDialog, cairo_surface_t *pNewTextSurface, int iNewTextWidth, int iNewTextHeight);
void cairo_dock_set_new_dialog_icon_surface (CairoDialog *pDialog, cairo_surface_t *pNewIconSurface, int iNewIconSize);

void cairo_dock_set_dialog_message (CairoDialog *pDialog, const gchar *cMessage);
void cairo_dock_set_dialog_icon (CairoDialog *pDialog, const gchar *cImageFilePath);

void cairo_dock_damage_icon_dialog (CairoDialog *pDialog);
void cairo_dock_damage_text_dialog (CairoDialog *pDialog);
void cairo_dock_damage_interactive_widget_dialog (CairoDialog *pDialog);


// Futur applet dialog-rendering
void cairo_dock_set_frame_size_comics (CairoDialog *pDialog);
void cairo_dock_draw_decorations_comics (cairo_t *pCairoContext, CairoDialog *pDialog);

void cairo_dock_set_frame_size_modern (CairoDialog *pDialog);
void cairo_dock_draw_decorations_modern (cairo_t *pCairoContext, CairoDialog *pDialog);

void cairo_dock_set_frame_size_3Dplane (CairoDialog *pDialog);
void cairo_dock_draw_decorations_3Dplane (cairo_t *pCairoContext, CairoDialog *pDialog);

void cairo_dock_set_frame_size_tooltip (CairoDialog *pDialog);
void cairo_dock_draw_decorations_tooltip (cairo_t *pCairoContext, CairoDialog *pDialog);


G_END_DECLS
#endif
