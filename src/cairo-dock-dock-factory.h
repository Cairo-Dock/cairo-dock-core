
#ifndef __CAIRO_DOCK_FACTORY__
#define  __CAIRO_DOCK_FACTORY__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-dock-factory.h This class defines the Docks, and gives the way to create, destroy, and fill them.
*
* A dock is a container that holds a set of icons and a renderer (also known as view).
*
* It has the ability to be placed anywhere on the screen edges and to resize itself automatically to fit the screen's size.
*
* It supports internal dragging of its icons with the mouse, and dragging of itself with alt+mouse.
*
* A dock can be either a main-dock (not linked to any icon) or a sub-dock (linked to an icon of another dock), and there can be as many docks of each sort as you want.
*/

typedef void (*CairoDockCalculateMaxDockSizeFunc) (CairoDock *pDock);
typedef Icon * (*CairoDockCalculateIconsFunc) (CairoDock *pDock);
typedef void (*CairoDockRenderFunc) (cairo_t *pCairoContext, CairoDock *pDock);
typedef void (*CairoDockRenderOptimizedFunc) (cairo_t *pCairoContext, CairoDock *pDock, GdkRectangle *pArea);
typedef void (*CairoDockSetSubDockPositionFunc) (Icon *pPointedIcon, CairoDock *pParentDock);
typedef void (*CairoDockGLRenderFunc) (CairoDock *pDock);

/// Dock's renderer, also known as 'view'.
struct _CairoDockRenderer {
	/// chemin d'un fichier readme destine a presenter de maniere succinte la vue.
	gchar *cReadmeFilePath;
	/// fonction calculant la taille max d'a dock.
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

/// Definition of a Dock, which derives from a Container.
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
	/// lorsque la souris est dans the dock.
	gboolean bInside;
	/// dit si the dock est horizontal ou vertical.
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
	/// si the dock est the dock racine.
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
	/// TRUE ssi the dock doit se cacher automatiquement.
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
	/// marge d'evitement de la souris, en fraction de la largeur d'an icon (entre 0 et 0.5)
	gdouble fAvoidingMouseMargin;

	/// pointeur sur le 1er element de la liste des icones a etre dessine, en partant de la gauche.
	GList *pFirstDrawnElement;
	/// decalage des decorations pour les faire suivre la souris.
	gdouble fDecorationsOffsetX;

	/// the dock est en bas au repos.
	gboolean bAtBottom;
	/// the dock est en haut pret a etre utilise.
	gboolean bAtTop;
	/// Whether the dock is in a popped up state or not
	gboolean bPopped;
	/// lorsque le menu du clique droit est visible.
	gboolean bMenuVisible;
	/// Est-on en train de survoler the dock avec quelque chose dans la souris ?
	gboolean bIsDragging;
	/// Valeur de l'auto-hide avant le cachage-rapide.
	gboolean bAutoHideInitialValue;
	/// TRUE ssi on ne peut plus entrer dans a dock.
	gboolean bEntranceDisabled;
	
	//\_______________ Les ID des threads en cours sur the dock.
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


/** Say if a Container is a Dock.
* @param pContainer the container.
* @return TRUE if the container is a Dock.
*/
#define CAIRO_DOCK_IS_DOCK(pContainer) (pContainer != NULL && (pContainer)->iType == CAIRO_DOCK_TYPE_DOCK)

/** Cast a Container into a Dock.
* @param pContainer the container.
* @return the dock.
*/
#define CAIRO_DOCK(pContainer) ((CairoDock *)pContainer)


/** Create a new root dock.
* @param cDockName name of the dock, used to identify it quickly. If the name is already used, the corresponding dock is returned.
* @param cRendererName name of a renderer. If NULL, the default renderer will be applied.
* @return the newly allocated dock, to destroy with #cairo_dock_destroy_dock
*/
CairoDock *cairo_dock_create_new_dock (const gchar *cDockName, const gchar *cRendererName);

/*
* Desactive a dock : le rend inoperant, en detruisant tout ce qu'il contient, sauf sa liste d'icones.
* @param pDock the dock.
*/
void cairo_dock_deactivate_one_dock (CairoDock *pDock);

/** Decrease the number of pointing icons, and destroy the dock if no more icons point on it. Also destroy all of its sub-docks, and free any allocated ressources. Do nothing for the main dock, use #cairo_dock_free_all_docks for it.
* @param pDock the dock to be destroyed.
* @param cDockName its name.
* @param ReceivingDock a dock that will get its icons, or NULL to also destroy its icons.
* @param cReceivingDockName the name of the dock that will get its icons, or NULL if none is provided.
*/
void cairo_dock_destroy_dock (CairoDock *pDock, const gchar *cDockName, CairoDock *ReceivingDock, const gchar *cReceivingDockName);

/** Increase by 1 the number of pointing icons. If the dock was a root dock, it becomes a sub-dock.
* @param pDock a dock.
* @param pParentDock its parent dock, if it becomes a sub-dock, otherwise it can be NULL.
*/
void cairo_dock_reference_dock (CairoDock *pDock, CairoDock *pParentDock);

/** Create a new dock of type "sub-dock", and load a given list of icons inside. The list then belongs to the dock, so it must not be freeed after that. The buffers of each icon are loaded, so they just need to have an image filename and a name.
* @param pIconList a list of icons that will be loaded and inserted into the new dock.
* @param cDockName desired name for the new dock.
* @param pParentDock the parent dock.
* @return the newly allocated dock.
*/
CairoDock *cairo_dock_create_subdock_from_scratch (GList *pIconList, gchar *cDockName, CairoDock *pParentDock);

/** Load a set of .desktop files that define icons, and build the corresponding tree of docks.
* All the icons are created and placed inside their dock, which is created if necessary.
* In the end, each dock is computed and placed on the screen.
* @param pMainDock a dock, needed to have an initial cairo context (because icons are created before their container).
* @param cDirectory a folder containing some .desktop files.
*/
void cairo_dock_build_docks_tree_with_desktop_files (CairoDock *pMainDock, gchar *cDirectory);

/** Destroy all docks and all icons contained inside, and free all allocated ressources. Applets and Taskbar are stopped beforehand.
*/
void cairo_dock_free_all_docks (void);


/** Insert an icon into a dock.
* Do nothing if the icon already exists inside the dock.
* @param icon the icon to be inserted.
* @param pDock the dock to insert inside.
* @param bUpdateSize TRUE to update the size of the dock after the insertion.
* @param bAnimated TRUE to arm the icon's animation for insertion.
* @param bInsertSeparator TRUE to insert an automatic separator if needed.
* @param pCompareFunc a sorting function to place the new icon amongst the others, or NULL to sort by group/order.
*/
void cairo_dock_insert_icon_in_dock_full (Icon *icon, CairoDock *pDock, gboolean bUpdateSize, gboolean bAnimated, gboolean bInsertSeparator, GCompareFunc pCompareFunc);

/** Insert an icon into a dock, at the position given by its 'fOrder' field.
* Insert an automatic separator if needed. Do nothing if the icon already exists inside the dock.
* @param icon the icon to be inserted.
* @param pDock the dock to insert inside.
* @param bUpdateSize TRUE to update the size of the dock after the insertion.
* @param bAnimated TRUE to arm the icon's animation for insertion.
*/
#define cairo_dock_insert_icon_in_dock(icon, pDock, bUpdateSize, bAnimated) cairo_dock_insert_icon_in_dock_full (icon, pDock, bUpdateSize, bAnimated, myIcons.bUseSeparator, NULL)

/** Detach an icon from its dock, removing the unnecessary separators. The icon is not destroyed, and can be directly re-inserted in another container; it keeps its sub-dock, but loose its dialogs. Do nothing if the icon doesn't exist inside the dock.
*@param icon the icon to detach.
*@param pDock the dock containing the icon.
*@param bCheckUnusedSeparator TRUE to check and remove unnecessary separators.
*@return TRUE if the icon has been detached.
*/
gboolean cairo_dock_detach_icon_from_dock (Icon *icon, CairoDock *pDock, gboolean bCheckUnusedSeparator);

void cairo_dock_remove_icon_from_dock_full (CairoDock *pDock, Icon *icon, gboolean bCheckUnusedSeparator);

/** Remove an icon from its dock, that is to say detach the icon, and  remove all links with Cairo-Dock : its .desktop is deleted, its module is deactivated, and its Xid is removed from the Taskbar (its class is handled too).
* Unnecessary separators are not tested.
* The icon is not yet destroyed, and keeps its sub-dock.
*@param pDock the dock contenant the icon.
*@param icon the icon to be removed.
*/
#define cairo_dock_remove_one_icon_from_dock(pDock, icon) cairo_dock_remove_icon_from_dock_full (pDock, icon, FALSE)

/** Remove an icon from its dock, that is to say detach the icon, and  remove all links with Cairo-Dock : its .desktop is deleted, its module is deactivated, and its Xid is removed from the Taskbar (its class is handled too).
* Unnecessary separators are removed as well.
* The icon is not yet destroyed, and keeps its sub-dock.
*@param pDock the dock contenant the icon.
*@param icon the icon to be removed.
*/
#define cairo_dock_remove_icon_from_dock(pDock, icon) cairo_dock_remove_icon_from_dock_full (pDock, icon, TRUE)

/** Remove and destroy all automatic separators inside a dock.
*@param pDock the dock.
*/
void cairo_dock_remove_automatic_separators (CairoDock *pDock);

/** Add automatic separators between the different types of icons inside a dock.
*@param pDock the dock.
*/
void cairo_dock_insert_separators_in_dock (CairoDock *pDock);

/** Add a launcher from a common desktop file : create and add the corresponding .desktop file with the others, load the corresponding icon, and insert it inside a dock with an animtion.
*@param cExternDesktopFileURI path to a desktop file.
*@param pReceivingDock the dock that will hold the new launcher.
*@param fOrder the order of the icon inside the dock.
*/
void cairo_dock_add_new_launcher_by_uri (const gchar *cExternDesktopFileURI, CairoDock *pReceivingDock, double fOrder);


G_END_DECLS
#endif
