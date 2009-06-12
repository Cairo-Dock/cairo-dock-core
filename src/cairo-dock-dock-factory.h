
#ifndef __CAIRO_DOCK_FACTORY__
#define  __CAIRO_DOCK_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-dock-factory.h This class defines the Docks.
* A dock is a container that holds a set of icons and a renderer (also known as view).
* It has the ability to be placed anywhere on the screen edges and to resize itself automatically to fit the screen's size.
* It supports internal dragging of its icons with the mouse, and dragging of itself with alt+mouse.
* A dock can be either a main-dock (not linked to any icon) or a sub-dock (linked to an icon of another dock), and there can be as many dock of each sort as you want.
*/

typedef void (*CairoDockCalculateMaxDockSizeFunc) (CairoDock *pDock);
typedef Icon * (*CairoDockCalculateIconsFunc) (CairoDock *pDock);
typedef void (*CairoDockRenderFunc) (cairo_t *pCairoContext, CairoDock *pDock);
typedef void (*CairoDockRenderOptimizedFunc) (cairo_t *pCairoContext, CairoDock *pDock, GdkRectangle *pArea);
typedef void (*CairoDockSetSubDockPositionFunc) (Icon *pPointedIcon, CairoDock *pParentDock);
typedef void (*CairoDockGLRenderFunc) (CairoDock *pDock);

struct _CairoDockRenderer {
	/// chemin d'un fichier readme destine a presenter de maniere succinte la vue.
	gchar *cReadmeFilePath;
	/// fonction calculant la taille max d'un dock.
	CairoDockCalculateMaxDockSizeFunc calculate_max_dock_size;
	/// fonction calculant l'ensemble des parametres des icones.
	CairoDockCalculateIconsFunc calculate_icons;
	/// fonction de rendu.
	CairoDockRenderFunc render;
	/// fonction de rendu optimise, ne dessinant qu'une seule icone.
	CairoDockRenderOptimizedFunc render_optimized;
	/// fonction de rendu OpenGL (optionnelle).
	CairoDockGLRenderFunc render_opengl;
	/// fonction calculant la position d'un sous-dock.
	CairoDockSetSubDockPositionFunc set_subdock_position;
	/// TRUE ssi cette vue utilise les reflets.
	gboolean bUseReflect;
	/// chemin d'une image de previsualisation.
	gchar *cPreviewFilePath;
	/// TRUE ssi cette vue utilise le stencil buffer d'OpenGL.
	gboolean bUseStencil;
	/// nom affiche dans la liste (traduit suivant la langue).
	const gchar *cDisplayedName;
};

typedef enum {
	CAIRO_DOCK_MOUSE_INSIDE,
	CAIRO_DOCK_MOUSE_ON_THE_EDGE,
	CAIRO_DOCK_MOUSE_OUTSIDE
	} CairoDockMousePositionType;

struct _CairoDock {
	/// type "dock".
	CairoDockTypeContainer iType;
	/// sa fenetre de dessin.
	GtkWidget *pWidget;
	/// largeur de la fenetre, _apres_ le redimensionnement par GTK.
	gint iCurrentWidth;
	/// hauteur de la fenetre, _apres_ le redimensionnement par GTK.
	gint iCurrentHeight;
	/// position courante en X du coin haut gauche de la fenetre sur l'ecran.
	gint iWindowPositionX;
	/// position courante en Y du coin haut gauche de la fenetre sur l'ecran.
	gint iWindowPositionY;
	/// lorsque la souris est dans le dock.
	gboolean bInside;
	/// dit si le dock est horizontal ou vertical.
	CairoDockTypeHorizontality bHorizontalDock;
	/// donne l'orientation du dock.
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
	/// pour l'animation des docks.
	gint iSidGLAnimation;
	/// intervalle entre 2 etapes de l'animation.
	gint iAnimationDeltaT;
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
	/// ratio applique aux icones.
	gdouble fRatio;
	/// dit si la vue courante utilise les reflets ou pas (utile pour les plug-ins).
	gboolean bUseReflect;
	/// contexte OpenGL associe a la fenetre.
	GLXContext glContext;
	/// TRUE <=> une animation lente est en cours.
	gboolean bKeepSlowAnimation;
	/// compteur pour l'animation.
	gint iAnimationStep;
	/// la liste de ses icones.
	GList* icons;
	/// liste des notifications disponibles.
	GPtrArray *pNotificationsTab;
	/// si le dock est le dock racine.
	gboolean bIsMainDock;
	/// le nombre d'icones pointant sur lui.
	gint iRefCount;

	//\_______________ Les parametres de position et de geometrie de la fenetre du dock.
	/// ecart de la fenetre par rapport au bord de l'ecran.
	gint iGapX;
	/// decalage de la fenetre par rapport au point d'alignement sur le bord de l'ecran.
	gint iGapY;
	/// alignement, entre 0 et 1, du dock sur le bord de l'ecran.
	gdouble fAlign;
	/// TRUE ssi le dock doit se cacher automatiquement.
	gboolean bAutoHide;
	
	/// magnitude maximale de la vague.
	gdouble fMagnitudeMax;
	/// max des hauteurs des icones.
	gdouble iMaxIconHeight;
	/// largeur du dock a plat, avec juste les icones.
	gdouble fFlatDockWidth;
	/// largeur du dock au repos.
	gint iMinDockWidth;
	/// hauteur du dock au repos.
	gint iMinDockHeight;
	/// largeur du dock actif.
	gint iMaxDockWidth;
	/// hauteur du dock actif.
	gint iMaxDockHeight;
	/// largeur des decorations.
	gint iDecorationsWidth;
	/// hauteur des decorations.
	gint iDecorationsHeight;

	gint iMaxLabelWidth;
	gint iMinLeftMargin;
	gint iMinRightMargin;
	gint iMaxLeftMargin;
	gint iMaxRightMargin;
	gint iLeftMargin;
	gint iRightMargin;

	//\_______________ Les parametres lies a une animation du dock.
	/// pour faire defiler les icones avec la molette.
	gint iScrollOffset;
	/// indice de calcul du coef multiplicateur de l'amplitude de la sinusoide (entre 0 et CAIRO_DOCK_NB_MAX_ITERATIONS).
	gint iMagnitudeIndex;
	/// un facteur d'acceleration lateral des icones lors du grossissement initial.
	gdouble fFoldingFactor;
	/// type d'icone devant eviter la souris, -1 si aucun.
	gint iAvoidingMouseIconType;
	/// marge d'evitement de la souris, en fraction de la largeur d'une icone (entre 0 et 0.5)
	gdouble fAvoidingMouseMargin;

	/// pointeur sur le 1er element de la liste des icones a etre dessine, en partant de la gauche.
	GList *pFirstDrawnElement;
	/// decalage des decorations pour les faire suivre la souris.
	gdouble fDecorationsOffsetX;

	/// le dock est en bas au repos.
	gboolean bAtBottom;
	/// le dock est en haut pret a etre utilise.
	gboolean bAtTop;
	/// Whether the dock is in a popped up state or not
	gboolean bPopped;
	/// lorsque le menu du clique droit est visible.
	gboolean bMenuVisible;
	/// Est-on en train de survoler le dock avec quelque chose dans la souris ?
	gboolean bIsDragging;
	/// Valeur de l'auto-hide avant le cachage-rapide.
	gboolean bAutoHideInitialValue;
	/// TRUE ssi on ne peut plus entrer dans un dock.
	gboolean bEntranceDisabled;
	
	//\_______________ Les ID des threads en cours sur le dock.
	/// serial ID du thread de descente de la fenetre.
	gint iSidMoveDown;
	/// serial ID du thread de montee de la fenetre.
	gint iSidMoveUp;
	/// serial ID for window popping up to the top layer event.
	gint iSidPopUp;
	/// serial ID for window popping down to the bottom layer.
	gint iSidPopDown;
	/// serial ID du thread qui enverra le signal de sortie retarde.
	gint iSidLeaveDemand;
	/// serial ID du thread qui deplace les icones lateralement lors d'un glisse d'icone interne
	gint iSidIconGlide;
	
	//\_______________ Les fonctions de dessin du dock.
	/// nom de la vue, utile pour charger les fonctions de rendu posterieurement a la creation du dock.
	gchar *cRendererName;
	/// recalculer la taille maximale du dock.
	CairoDockCalculateMaxDockSizeFunc calculate_max_dock_size;
	/// calculer tous les parametres des icones.
	CairoDockCalculateIconsFunc calculate_icons;
	/// dessiner le tout.
	CairoDockRenderFunc render;
	/// dessiner une portion du dock de maniere optimisee.
	CairoDockRenderOptimizedFunc render_optimized;
	/// fonction de rendu OpenGL (optionnelle).
	CairoDockGLRenderFunc render_opengl;
	/// calculer la position d'un sous-dock.
	CairoDockSetSubDockPositionFunc set_subdock_position;
	/// TRUE ssi on peut dropper entre 2 icones.
	gboolean bCanDrop;
	gboolean bIsShrinkingDown;
	gboolean bIsGrowingUp;
	GdkRectangle inputArea;
	GdkBitmap* pShapeBitmap;
	CairoDockMousePositionType iMousePositionType;
	/// TRUE ssi cette vue utilise le stencil buffer d'OpenGL.
	gboolean bUseStencil;
};


/** Teste si le container est un dock.
* @param pContainer le container.
* @return TRUE ssi le container a ete declare comme un dock.
*/
#define CAIRO_DOCK_IS_DOCK(pContainer) (pContainer != NULL && (pContainer)->iType == CAIRO_DOCK_TYPE_DOCK)
/** Caste un container en dock.
* @param pContainer le container.
* @return le dock.
*/
#define CAIRO_DOCK(pContainer) ((CairoDock *)pContainer)


/**
* Cree un nouveau dock principal.
* @param iWmHint indicateur du type de fenetre pour le WM.
* @param cDockName nom du dock, qui pourra etre utilise pour retrouver celui-ci rapidement.
* @param cRendererName nom de la fonction de rendu a appliquer au dock. si NULL, le rendu par defaut sera applique.
* @return le dock nouvellement alloué, a detruire avec #cairo_dock_destroy_dock
*/
CairoDock *cairo_dock_create_new_dock (GdkWindowTypeHint iWmHint, gchar *cDockName, gchar *cRendererName);
/**
* Desactive un dock : le rend inoperant, en detruisant tout ce qu'il contient, sauf sa liste d'icones.
* @param pDock le dock.
*/
void cairo_dock_deactivate_one_dock (CairoDock *pDock);
/**
* Diminue le nombre d'icones pointant sur un dock de 1. Si aucune icone ne pointe plus sur lui apres ca, le detruit ainsi que tous ses sous-docks, et libere la memoire qui lui etait allouee. Ne fais rien pour le dock principal, utiliser #cairo_dock_free_all_docks pour cela.
* @param pDock le dock a detruire.
* @param cDockName son nom.
* @param ReceivingDock un dock qui recuperera les icones, ou NULL pour detruire toutes les icones contenues dans le dock.
* @param cReceivingDockName le nom du dock qui recuperera les icones, ou NULL si aucun n'est fourni.
*/
void cairo_dock_destroy_dock (CairoDock *pDock, const gchar *cDockName, CairoDock *ReceivingDock, gchar *cReceivingDockName);

/**
* Incremente de 1 la reference d'un dock, c'est-a-dire le nombre d'icones pointant sur ce dock. Si le dock etait auparavant un dock principal, il devient un sous-dock, prenant du meme coup les parametres propres aux sous-docks.
* @param pDock un dock.
* @param pParentDock son dock parent, si sa reference passse a 1, sinon peu etre NULL.
*/
void cairo_dock_reference_dock (CairoDock *pDock, CairoDock *pParentDock);

/**
* Cree un nouveau dock de type "sous-dock", et y insere la liste des icones fournie. La liste est appropriee par le dock, et ne doit donc _pas_ etre liberee apres cela. Chaque icone est chargee, et a donc juste besoin d'avoir un nom et un fichier d'image.
* @param pIconList une liste d'icones qui seront entierement chargees et inserees dans le dock.
* @param cDockName le nom desire pour le dock.
* @param iWindowTypeHint indicateur du type de fenetre pour le WM.
* @param pParentDock le dock parent du sous-dock cree.
* @return le dock nouvellement alloue.
*/
CairoDock *cairo_dock_create_subdock_from_scratch_with_type (GList *pIconList, gchar *cDockName, GdkWindowTypeHint iWindowTypeHint, CairoDock *pParentDock);
#define cairo_dock_create_subdock_from_scratch(pIconList, cDockName, pParentDock) cairo_dock_create_subdock_from_scratch_with_type (pIconList, cDockName, GDK_WINDOW_TYPE_HINT_DOCK, pParentDock)
#define cairo_dock_create_subdock_for_class_appli(cClassName, pParentDock) cairo_dock_create_subdock_from_scratch_with_type (NULL, cClassName, GDK_WINDOW_TYPE_HINT_DOCK, pParentDock)

/**
* Charge un ensemble de fichiers .desktop definissant des icones, et construit l'arborescence des docks.
* Toutes les icones sont creees et placees dans leur conteneur repectif, qui est cree si necessaire. Cette fonction peut tres bien s'utiliser pour 
* A la fin du processus, chaque dock est calcule, et place a la position qui lui est assignee.
* Il faut fournir un dock pour avoir ujn contexte de dessin, car les icones sont crees avant leur conteneur.
* @param pMainDock un dock quelconque.
* @param cDirectory le repertoire contenant les fichiers .desktop.
*/
void cairo_dock_build_docks_tree_with_desktop_files (CairoDock *pMainDock, gchar *cDirectory);

/**
* Detruit tous les docks et toutes les icones contenues dedans, et libere la memoire qui leur etait allouee. Les applets sont stoppees au prealable, ainsi que la barre des taches.
*/
void cairo_dock_free_all_docks (void);


/**
* Insere une icone dans le dock, a la position indiquee par son champ fOrder.
* Ne fais rien si l'icone existe deja dans le dock.
* @param icon l'icone a inserer.
* @param pDock le dock dans lequel l'inserer.
* @param bUpdateSize TRUE pour mettre a jour la taille du dock apres insertion.
* @param bAnimated TRUE pour regler la taille de l'icone au minimum de facon a la faire grossir apres.
* @param bInsertSeparator TRUE pour inserer un separateur si necessaire.
* @param pCompareFunc la fonction de comparaison
*/
void cairo_dock_insert_icon_in_dock_full (Icon *icon, CairoDock *pDock, gboolean bUpdateSize, gboolean bAnimated, gboolean bInsertSeparator, GCompareFunc pCompareFunc);
/**
* Insere une icone dans le dock, a la position indiquee par son champ fOrder.
* Insere un separateur si necessaire. Ne fais rien si l'icone existe deja dans le dock.
* @param icon l'icone a inserer.
* @param pDock le dock dans lequel l'inserer.
* @param bUpdateSize TRUE pour recalculer la taille du dock apres insertion.
* @param bAnimated TRUE pour regler la taille de l'icone au minimum de facon a la faire grossir apres.
*/
#define cairo_dock_insert_icon_in_dock(icon, pDock, bUpdateSize, bAnimated) cairo_dock_insert_icon_in_dock_full (icon, pDock, bUpdateSize, bAnimated, myIcons.bUseSeparator, NULL)

/**
*Detache une icone de son dock, en enlevant les separateurs superflus si necessaires. L'icone n'est pas detruite, et peut etre re-inseree autre part telle qu'elle; elle garde son sous-dock, mais perd ses dialogues. Ne fais rien si l'icone n'existe pas dans le dock.
*@param icon l'icone a detacher.
*@param pDock le dock contenant l'icone.
*@param bCheckUnusedSeparator si TRUE, alors teste si des separateurs sont devenus superflus, et les enleve le cas echeant.
*@return TRUE ssi l'icone a ete detachee.
*/
gboolean cairo_dock_detach_icon_from_dock (Icon *icon, CairoDock *pDock, gboolean bCheckUnusedSeparator);
/**
*Enleve une icone du dock : l'icone n'est pas detruite, et garde son sous-dock, mais est n'existe plus nulle part (son .desktop est detruit, son module est desactive, et son Xid est effacee du registre (la classe est geree aussi)). Les separateurs superflus ne sont pas testes.
*@param pDock le dock contenant l'icone.
*@param icon l'icone a detacher.
*/
void cairo_dock_remove_one_icon_from_dock (CairoDock *pDock, Icon *icon);
/**
*Idem que precedemment, mais enleve aussi les separateurs superflus;
*@param pDock le dock contenant l'icone.
*@param icon l'icone a detacher.
*/
void cairo_dock_remove_icon_from_dock (CairoDock *pDock, Icon *icon);

/**
*Enleve et detruit toutes les icones de separateurs automatiques.
*@param pDock le dock duquel supprimer les icones.
*/
void cairo_dock_remove_all_separators (CairoDock *pDock);

/**
*Ajoute des separateurs automatiques entre les differents types d'icones.
*@param pDock le dock auquel rajouter les separateurs.
*/
void cairo_dock_insert_separators_in_dock (CairoDock *pDock);


void cairo_dock_add_new_launcher_by_uri (const gchar *cDesktopFileURI, CairoDock *pReceivingDock, double fOrder);

G_END_DECLS
#endif
