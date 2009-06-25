
#ifndef __CAIRO_DOCK_ICONS__
#define  __CAIRO_DOCK_ICONS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-modules.h"
G_BEGIN_DECLS


/**
*@file cairo-dock-icons.h This class defines the elements contained in containers : Icons.
* An icon is an union of 3 parts : a launcher, an appli, and an applet. It can be all of them at the same time.
* - a launcher is any icon having a command or pointing to a sub-dock.
* - an appli is an icon pointing to an X ID, that is to say a window.
* - an applet is an icon holding a module instance.
* - an icon being none of them is a separator.
* 
*/

typedef enum {
	CAIRO_DOCK_LAUNCHER = 0,
	CAIRO_DOCK_SEPARATOR12,
	CAIRO_DOCK_APPLI,
	CAIRO_DOCK_SEPARATOR23,
	CAIRO_DOCK_APPLET,
	CAIRO_DOCK_NB_TYPES
	} CairoDockIconType;

typedef enum {
	CAIRO_DOCK_STATE_REST = 0,
	CAIRO_DOCK_STATE_MOUSE_HOVERED,
	CAIRO_DOCK_STATE_CLICKED,
	CAIRO_DOCK_STATE_AVOID_MOUSE,
	CAIRO_DOCK_STATE_FOLLOW_MOUSE,
	CAIRO_DOCK_STATE_REMOVE_INSERT,
	CAIRO_DOCK_NB_STATES
	} CairoDockAnimationState;

struct _Icon {
	//\____________ renseignes lors de la creation de l'icone.
	/// Nom (et non pas chemin) du fichier .desktop definissant l'icone, ou NULL si l'icone n'est pas definie pas un fichier.
	gchar *acDesktopFileName;
	/// URI.
	gchar *cBaseURI;
	/// ID d'un volume.
	gint iVolumeID;
	/// Nom (et non pas chemin) du fichier de l'image, ou NULL si son image n'est pas definie pas un fichier.
	gchar *acFileName;
	/// Nom de l'icone tel qu'il apparaitra dans son etiquette. Donne le nom au sous-dock.
	gchar *acName;
	/// Commande a executer lors d'un clique gauche clique, ou NULL si aucune.
	gchar *acCommand;
	/// Repertoire ou s'executera la commande.
	gchar *cWorkingDirectory;
	/// Type de l'icone.
	CairoDockIconType iType;
	/// Ordre de l'icone dans son dock, parmi les icones de meme type.
	gdouble fOrder;
	/// Sous-dock sur lequel pointe l'icone, ou NULL si aucun.
	CairoDock *pSubDock;
	/// Nom du dock contenant l'icone (y compris lorsque l'icone est dans un desklet).
	gchar *cParentDockName;
	//\____________ calcules lors du chargement de l'icone.
	/// Dimensions de la surface de l'icone.
	gdouble fWidth, fHeight;
	/// Surface cairo de l'image.
	cairo_surface_t* pIconBuffer;
	/// Surface cairo de l'etiquette.
	cairo_surface_t* pTextBuffer;
	/// Surface cairo du reflet.
	cairo_surface_t* pReflectionBuffer;
	/// dimensions de l'etiquette.
	gint iTextWidth, iTextHeight;
	/// Decalage en X et en Y de l'etiquette.
	gdouble fTextXOffset, fTextYOffset;
	/// Abscisse maximale (droite) que l'icone atteindra (variable avec la vague).
	gdouble fXMax;
	/// Abscisse minimale (gauche) que l'icone atteindra (variable avec la vague).
	gdouble fXMin;
	//\____________ calcules a chaque scroll et insertion/suppression d'une icone.
	/// Abscisse de l'icone au repos.
	gdouble fXAtRest;
	//\____________ calcules a chaque fois.
	/// Phase de l'icone (entre -pi et piconi).
	gdouble fPhase;
	/// Abscisse temporaire du bord gauche de l'image de l'icone.
	gdouble fX;
	/// Ordonnee temporaire du bord haut de l'image de l'icone.
	gdouble fY;
	/// Echelle courante de l'icone (facteur de zoom, >= 1).
	gdouble fScale;
	/// Abscisse du bord gauche de l'image de l'icone.
	gdouble fDrawX;
	/// Ordonnee du bord haut de l'image de l'icone.
	gdouble fDrawY;
	/// Facteur de zoom sur la largeur de l'icone.
	gdouble fWidthFactor;
	/// Facteur de zoom sur la hauteur de l'icone.
	gdouble fHeightFactor;
	/// Transparence (<= 1).
	gdouble fAlpha;
	/// TRUE ssi l'icone est couramment pointee.
	gboolean bPointed;
	/// Facteur de zoom personnel, utilise pour l'apparition et la suppression des icones.
	gdouble fPersonnalScale;
	/// Decalage en ordonnees de reflet (pour le rebond, >= 0).
	gdouble fDeltaYReflection;
	/// Orientation de l'icone (angle par rapport a la verticale Oz).
	gdouble fOrientation;
	/// Rotation autour de l'axe Ox (animation upside-down).
	gint iRotationX;
	/// Rotation autour de l'axe Oy (animation rotate).
	gint iRotationY;
	//\____________ Pour les applis.
	/// PID de l'application correspondante.
	gint iPid;
	/// ID de la fenetre X de l'application correspondante.
	Window Xid;
	/// Classe de l'application correspondante (ou NULL si aucune).
	gchar *cClass;
	/// Etiquette du lanceur a sa creation, ecrasee par le nom courant de l'appli.
	gchar *cInitialName;
	/// Heure de derniere verification de la presence de l'application dans la barre des taches.
	gint iLastCheckTime;
	/// TRUE ssi la fenetre de l'application correspondante est minimisee.
	gboolean bIsHidden;
	/// Position et taille de la fenetre.
	GtkAllocation windowGeometry;
	/// TRUE ssi la fenetre est en mode plein ecran.
	gboolean bIsFullScreen;
	/// TRUE ssi la fenetre est en mode maximisee.
	gboolean bIsMaximized;
	/// TRUE ssi la fenetre demande l'attention ou est en mode urgente.
	gboolean bIsDemandingAttention;
	/// TRUE ssi l'icone a un indicateur (elle controle une appli).
	gboolean bHasIndicator;
	/// ID du pixmap de sauvegarde de la fenetre pour quand elle est cachee.
	Pixmap iBackingPixmap;
	//Damage iDamageHandle;
	//\____________ Pour les modules.
	/// Instance de module que represente l'icone.
	CairoDockModuleInstance *pModuleInstance;
	/// Texte de l'info rapide.
	gchar *cQuickInfo;
	/// Surface cairo de l'info rapide.
	cairo_surface_t* pQuickInfoBuffer;
	/// Largeur de l'info rapide.
	gint iQuickInfoWidth;
	/// Heuteur de l'info rapide.
	gint iQuickInfoHeight;
	/// Decalage en X de la surface de l'info rapide.
	gdouble fQuickInfoXOffset;
	/// Decalage en Y de la surface de l'info rapide.
	gdouble fQuickInfoYOffset;
	/// decalage pour le glissement des icones.
	gdouble fGlideOffset;
	/// direction dans laquelle glisse l'icone.
	gint iGlideDirection;
	/// echelle d'adaptation au glissement.
	gdouble fGlideScale;
	
	GLuint iIconTexture;
	GLuint iLabelTexture;
	GLuint iQuickInfoTexture;
	gpointer pDataSlot[CAIRO_DOCK_NB_DATA_SLOT];
	gboolean bStatic;
	CairoDockAnimationState iAnimationState;
	gboolean bBeingRemovedByCairo;
	gpointer pbuffer;
	/// liste des notifications disponibles.
	GPtrArray *pNotificationsTab;
	CairoDataRenderer *pDataRenderer;
	gint iNumDesktop;
	gint iViewPortX, iViewPortY;
	gint iStackOrder;
	CairoDockTransition *pTransition;
	gdouble fReflectShading;
};


/**
*TRUE ssi l'icone est une icone de lanceur.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_LAUNCHER(icon) (icon != NULL && ((icon)->acCommand != NULL || ((icon)->pSubDock != NULL && (icon)->pModuleInstance == NULL && (icon)->Xid == 0)))
/**
*TRUE ssi l'icone est une icone d'appli.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_APPLI(icon) (icon != NULL && (icon)->Xid != 0)
/**
*TRUE ssi l'icone est une icone d'applet.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_APPLET(icon) (icon != NULL && (icon)->pModuleInstance != NULL)
/**
*TRUE ssi l'icone est une icone de separateur.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_SEPARATOR(icon) (icon != NULL && ((icon)->acName == NULL && (icon)->pModuleInstance == NULL && (icon)->Xid == 0))  /* ((icon)->iType & 1) || */

/**
*TRUE ssi l'icone est une icone de lanceur defini par un fichier .desktop.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_NORMAL_LAUNCHER(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && (icon)->acDesktopFileName != NULL)
/**
*TRUE ssi l'icone est une icone de lanceur representant une URI.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_URI_LAUNCHER(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && (icon)->cBaseURI != NULL)
/**
*TRUE ssi l'icone est une icone de paille.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_FAKE_LAUNCHER(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && (icon)->acCommand == NULL && (icon)->cClass != NULL)
/**
*TRUE ssi l'icone est une icone pointant sur le sous-dock d'une classe d'appli.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_MULTI_APPLI(icon) (CAIRO_DOCK_IS_LAUNCHER (icon) && icon->pSubDock != NULL && icon->pSubDock->icons != NULL && icon->cClass != NULL)
/**
*TRUE ssi l'icone est une icone de separateur ajoutee automatiquement.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_AUTOMATIC_SEPARATOR(icon) (CAIRO_DOCK_IS_SEPARATOR (icon) && (icon)->acDesktopFileName == NULL)
/**
*TRUE ssi l'icone est une icone de separateur ajoutee par l'utilisateur.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_USER_SEPARATOR(icon) (CAIRO_DOCK_IS_SEPARATOR (icon) && (icon)->acDesktopFileName != NULL)
/**
*TRUE ssi l'icone est une icone d'appli seulement.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_NORMAL_APPLI(icon) (CAIRO_DOCK_IS_APPLI (icon) && (icon)->acDesktopFileName == NULL && (icon)->pModuleInstance == NULL)
/**
*TRUE ssi l'icone est une icone d'applet detachable en desklet.
*@param icon une icone.
*/
#define CAIRO_DOCK_IS_DETACHABLE_APPLET(icon) (CAIRO_DOCK_IS_APPLET (icon) && (icon)->pModuleInstance->bCanDetach)


/**
* Desactive une icone, et libere tous ses buffers, ainsi qu'elle-meme. Le sous-dock pointee par elle n'est pas dereferencee, cela doit etre fait au prealable. Gere egalement la classe.
*@param icon l'icone a liberer.
*/
void cairo_dock_free_icon (Icon *icon);
/**
* Libere tous les buffers d'une incone.
*@param icon l'icone.
*/
void cairo_dock_free_icon_buffers (Icon *icon);


#define cairo_dock_get_group_order(iType) (iType < CAIRO_DOCK_NB_TYPES ? myIcons.tIconTypeOrder[iType] : iType)
#define cairo_dock_get_icon_order(icon) cairo_dock_get_group_order ((icon)->iType)

/**
*Donne le type d'une icone relativement a son contenu.
*@param icon l'icone.
*/
CairoDockIconType cairo_dock_get_icon_type (Icon *icon);
/**
*Compare 2 icones grace a la relation d'ordre sur le couple (position du type , ordre).
*@param icon1 une icone.
*@param icon2 une autre icone.
*@return -1 si icone1 < icone2, 1 si icone1 > icone2, 0 si icone1 = icone2 (au sens de la relation d'ordre).
*/
int cairo_dock_compare_icons_order (Icon *icon1, Icon *icon2);
/**
*Compare 2 icones grace a la relation d'ordre sur le nom (ordre alphabetique sur les noms passes en minuscules).
*@param icon1 une icone.
*@param icon2 une autre icone.
*@return -1 si icone1 < icone2, 1 si icone1 > icone2, 0 si icone1 = icone2 (au sens de la relation d'ordre).
*/
int cairo_dock_compare_icons_name (Icon *icon1, Icon *icon2);
/**
*Compare 2 icones grace a la relation d'ordre sur l'extension des URIs (ordre alphabetique compte en passant en minuscules).
*@param icon1 une icone.
*@param icon2 une autre icone.
*@return -1 si icone1 < icone2, 1 si icone1 > icone2, 0 si icone1 = icone2 (au sens de la relation d'ordre).
*/
int cairo_dock_compare_icons_extension (Icon *icon1, Icon *icon2);

/**
*Trie une liste en se basant sur la relation d'ordre sur le couple (position du type , ordre).
*@param pIconList la liste d'icones.
*@return la liste triee. Les elements sont les memes que ceux de la liste initiale, seul leur ordre a change.
*/
GList *cairo_dock_sort_icons_by_order (GList *pIconList);
/**
*Trie une liste en se basant sur la relation d'ordre alphanumerique sur le nom des icones.
*@param pIconList la liste d'icones.
*@return la liste triee. Les elements sont les memes que ceux de la liste initiale, seul leur ordre a change. Les ordres des icones sont mis a jour pour refleter le nouvel ordre global.
*/
GList *cairo_dock_sort_icons_by_name (GList *pIconList);

/**
*Renvoie la 1ere icone d'une liste d'icones.
*@param pIconList la liste d'icones.
*@return la 1ere icone, ou NULL si la liste est vide.
*/

Icon *cairo_dock_get_first_icon (GList *pIconList);
/**
*Renvoie la derniere icone d'une liste d'icones.
*@param pIconList la liste d'icones.
*@return la derniere icone, ou NULL si la liste est vide.
*/
Icon *cairo_dock_get_last_icon (GList *pIconList);
/**
*Renvoie la 1ere icone a etre dessinee dans un dock (qui n'est pas forcement la 1ere icone de la liste, si l'utilisateur a scrolle).
*@param pDock le dock.
*@return la 1ere icone a etre dessinee, ou NULL si la liste est vide.
*/
Icon *cairo_dock_get_first_drawn_icon (CairoDock *pDock);
/**
*Renvoie la derniere icone a etre dessinee dans un dock (qui n'est pas forcement la derniere icone de la liste, si l'utilisateur a scrolle).
*@param pDock le dock.
*@return la derniere icone a etre dessinee, ou NULL si la liste est vide.
*/
Icon *cairo_dock_get_last_drawn_icon (CairoDock *pDock);
/**
*Renvoie la 1ere icone du type donne.
*@param pIconList la liste d'icones.
*@param iType le type d'icone recherche.
*@return la 1ere icone trouvee ayant ce type, ou NULL si aucune icone n'est trouvee.
*/
Icon *cairo_dock_get_first_icon_of_type (GList *pIconList, CairoDockIconType iType);
/**
*Renvoie la derniere icone du type donne.
*@param pIconList la liste d'icones.
*@param iType le type d'icone recherche.
*@return la derniere icone trouvee ayant ce type, ou NULL si aucune icone n'est trouvee.
*/
Icon *cairo_dock_get_last_icon_of_type (GList *pIconList, CairoDockIconType iType);
/**
*Renvoie la 1ere icone dont le groupe correspond.
*@param pIconList la liste d'icones.
*@param iType le type/groupe dont on recherche l'ordre.
*@return la 1ere icone trouvee ayant cet ordre, ou NULL si aucune icone n'est trouvee.
*/
Icon* cairo_dock_get_first_icon_of_order (GList *pIconList, CairoDockIconType iType);
/**
*Renvoie la derniere icone dont le groupe correspond.
*@param pIconList la liste d'icones.
*@param iType le type/groupe d'icone recherche.
*@return la derniere icone trouvee ayant cet ordre, ou NULL si aucune icone n'est trouvee.
*/
Icon* cairo_dock_get_last_icon_of_order (GList *pIconList, CairoDockIconType iType);
/**
*Renvoie l'icone actuellement pointee parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@return l'icone dont le champ bPointed a TRUE, ou NULL si aucune icone n'est pointee.
*/
Icon *cairo_dock_get_pointed_icon (GList *pIconList);
/**
*Renvoie l'icone actuellement en cours d'insertion ou de suppression parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@return la 1ere icone dont le champ fPersonnalScale est non nul ou NULL si aucune icone n'est en cours d'insertion / suppression.
*/
Icon *cairo_dock_get_removing_or_inserting_icon (GList *pIconList);
/**
*Renvoie l'icone suivante dans la liste d'icones. Cout en O(n).
*@param pIconList la liste d'icones.
*@param pIcon l'icone dont on veut le voisin.
*@return l'icone dont le voisin de gauche est pIcon, ou NULL si pIcon est la derniere icone de la liste.
*/
Icon *cairo_dock_get_next_icon (GList *pIconList, Icon *pIcon);
/**
*Renvoie l'icone precedente dans la liste d'icones. Cout en O(n).
*@param pIconList la liste d'icones.
*@param pIcon l'icone dont on veut le voisin.
*@return l'icone dont le voisin de droite est pIcon, ou NULL si pIcon est la 1ere icone de la liste.
*/
Icon *cairo_dock_get_previous_icon (GList *pIconList, Icon *pIcon);
/**
*Renvoie le prochain element dans la liste, en bouclant si necessaire.
*@param ic l'element courant.
*@param list la liste d'icones.
*@return l'element suivant de la liste bouclee.
*/
#define cairo_dock_get_next_element(ic, list) (ic == NULL || ic->next == NULL ? list : ic->next)
/**
*Renvoie l'element precedent dans la liste, en bouclant si necessaire.
*@param ic l'element courant.
*@param list la liste d'icones.
*@return l'element precedent de la liste bouclee.
*/
#define cairo_dock_get_previous_element(ic, list) (ic == NULL || ic->prev == NULL ? g_list_last (list) : ic->prev)
/**
*Cherche l'icone ayant une commande donnee parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param cCommand la chaine de commande.
*@return la 1ere icone ayant le champ 'acExec' identique a la commande fournie, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_command (GList *pIconList, gchar *cCommand);
/**
*Cherche l'icone ayant une URI de base donnee parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param cBaseURI l'URI.
*@return la 1ere icone ayant le champ 'cBaseURI' identique a l'URI fournie, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_base_uri (GList *pIconList, const gchar *cBaseURI);

Icon *cairo_dock_get_icon_with_name (GList *pIconList, const gchar *cName);

/**
*Cherche l'icone pointant sur un sous-dock donne parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param pSubDock le sous-dock.
*@return la 1ere icone ayant le champ 'pSubDock' identique au sous-dock fourni, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_subdock (GList *pIconList, CairoDock *pSubDock);
/**
*Cherche l'icone correspondante a un module donne parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param pModule le module.
*@return la 1ere icone ayant le champ 'pModule' identique au module fourni, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_module (GList *pIconList, CairoDockModule *pModule);
/**
*Cherche l'icone d'une application de classe donnee parmi une liste d'icones.
*@param pIconList la liste d'icones.
*@param cClass la classe d'application.
*@return la 1ere icone ayant le champ 'cClass' identique a la classe fournie, ou NULL si aucune icone ne correspond.
*/
Icon *cairo_dock_get_icon_with_class (GList *pIconList, gchar *cClass);

#define cairo_dock_get_first_launcher(pIconList) cairo_dock_get_first_icon_of_type (pIconList, CAIRO_DOCK_LAUNCHER)
#define cairo_dock_get_last_launcher(pIconList) cairo_dock_get_last_icon_of_type (pIconList, CAIRO_DOCK_LAUNCHER)
#define cairo_dock_get_first_appli(pIconList) cairo_dock_get_first_icon_of_type (pIconList, CAIRO_DOCK_APPLI)
#define cairo_dock_get_last_appli(pIconList) cairo_dock_get_last_icon_of_type (pIconList, CAIRO_DOCK_APPLI)

/** Renvoie les dimensions allouee a la surface/texture d'une icone.
@param pIcon l'icone
@param pContainer son container
@param iWidth pointeur sur la largeur
@param iHeight pointeur sur la hauteur
*/
void cairo_dock_get_icon_extent (Icon *pIcon, CairoContainer *pContainer, int *iWidth, int *iHeight);
#define CD_APPLET_GET_MY_ICON_EXTENT(iWidthPtr, iHeightPtr) cairo_dock_get_icon_extent (myIcon, myContainer, iWidthPtr, iHeightPtr)
/** Renvoie la taille d'une icone a laquelle elle est couramment dessinee sur l'ecran (en tenant compte de son zoom et de celui du container).
@param pIcon l'icone
@param pContainer son container
@param fSizeX pointeur sur la taille en X (horizontal)
@param fSizeY pointeur sur la taille en Y (vertical)
*/
void cairo_dock_get_current_icon_size (Icon *pIcon, CairoContainer *pContainer, double *fSizeX, double *fSizeY);
/** Renvoie la zone totale occupee par une icone sur son container (reflet, zoom et etirement compris).
@param icon l'icone
@param pContainer son container
@param pArea zone occupee par l'icone sur son container.
*/
void cairo_dock_compute_icon_area (Icon *icon, CairoContainer *pContainer, GdkRectangle *pArea);



void cairo_dock_normalize_icons_order (GList *pIconList, CairoDockIconType iType);
void cairo_dock_swap_icons (CairoDock *pDock, Icon *icon1, Icon *icon2);
void cairo_dock_move_icon_after_icon (CairoDock *pDock, Icon *icon1, Icon *icon2);

/**
*Effectue une action sur toutes les icones d'un type donne. L'action peut meme detruire et enlever de la liste l'icone courante.
*@param pIconList la liste d'icones a parcourir.
*@param iType le type d'icone.
*@param pFuntion l'action a effectuer sur chaque icone.
*@param data un pointeur qui sera passe en entree de l'action.
*@return le separateur avec le type a gauche si il y'en a, NULL sinon.
*/
Icon *cairo_dock_foreach_icons_of_type (GList *pIconList, CairoDockIconType iType, CairoDockForeachIconFunc pFuntion, gpointer data);

/**
*Met a jour une icone de lanceur ou d'applet avec le nom de son nouveau dock. Met a jour son fichier de conf aussi.
*@param icon l'icone du lanceur ou de l'applet.
*@param cNewParentDockName le nom de son nouveau dock.
*/
void cairo_dock_update_icon_s_container_name (Icon *icon, const gchar *cNewParentDockName);

#define cairo_dock_set_icon_static(icon) ((icon)->bStatic = TRUE)

/**
*Modifie l'etiquette d'une icone.
*@param pSourceContext un contexte de dessin; n'est pas altere par la fonction.
*@param cIconName la nouvelle etiquette de l'icone.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*/
void cairo_dock_set_icon_name (cairo_t *pSourceContext, const gchar *cIconName, Icon *pIcon, CairoContainer *pContainer);
/**
*Modifie l'etiquette d'une icone, en prenant une chaine au format 'printf'.
*@param pSourceContext un contexte de dessin; n'est pas altere par la fonction.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*@param cIconNameFormat la nouvelle etiquette de l'icone.
*/
void cairo_dock_set_icon_name_full (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconNameFormat, ...);

/**
*Ecris une info-rapide sur l'icone. C'est un petit texte (quelques caracteres) qui vient se superposer sur l'icone, avec un fond fonce.
*@param pSourceContext un contexte de dessin; n'est pas altere par la fonction.
*@param cQuickInfo le texte de l'info-rapide.
*@param pIcon l'icone.
*@param fMaxScale le facteur de zoom max.
*/
void cairo_dock_set_quick_info (cairo_t *pSourceContext, const gchar *cQuickInfo, Icon *pIcon, double fMaxScale);
/**
*Ecris une info-rapide sur l'icone, en prenant une chaine au format 'printf'.
*@param pSourceContext un contexte de dessin; n'est pas altere par la fonction.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*@param cQuickInfoFormat le texte de l'info-rapide, au format 'printf' (%s, %d, etc)
*@param ... les donnees a inserer dans la chaine de caracteres.
*/
void cairo_dock_set_quick_info_full (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, const gchar *cQuickInfoFormat, ...);
/**
*Efface l'info-rapide d'une icone.
*@param pIcon l'icone.
*/
#define cairo_dock_remove_quick_info(pIcon) cairo_dock_set_quick_info (NULL, NULL, pIcon, 1)


G_END_DECLS
#endif

