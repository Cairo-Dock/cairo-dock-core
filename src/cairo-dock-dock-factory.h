
#ifndef __CAIRO_DOCK_FACTORY__
#define  __CAIRO_DOCK_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-dock-facility.h"
G_BEGIN_DECLS

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


G_END_DECLS
#endif
