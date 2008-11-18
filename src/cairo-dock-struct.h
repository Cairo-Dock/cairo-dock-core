/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */
/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/

#ifndef __CAIRO_DOCK_STRUCT__
#define  __CAIRO_DOCK_STRUCT__

#include <X11/Xlib.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>
#include <glib/gi18n.h>
//#include <X11/extensions/Xdamage.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif
#include <GL/gl.h>


typedef struct _CairoDockRenderer CairoDockRenderer;
typedef struct _CairoDeskletRenderer CairoDeskletRenderer;
typedef struct _CairoDeskletDecoration CairoDeskletDecoration;
typedef struct _CairoDialogRenderer CairoDialogRenderer;

typedef struct _Icon Icon;
typedef struct _CairoContainer CairoContainer;
typedef struct _CairoDock CairoDock;
typedef struct _CairoDesklet CairoDesklet;
typedef struct _CairoDialog CairoDialog;
typedef struct _CairoFlyingContainer CairoFlyingContainer;

typedef struct _CairoDockModule CairoDockModule;
typedef struct _CairoDockModuleInterface CairoDockModuleInterface;
typedef struct _CairoDockModuleInstance CairoDockModuleInstance;
typedef struct _CairoDockVisitCard CairoDockVisitCard;
typedef struct _CairoDockMinimalAppletConfig CairoDockMinimalAppletConfig;
typedef struct _CairoDockVFSBackend CairoDockVFSBackend;
typedef struct _CairoDockClassAppli CairoDockClassAppli;
typedef struct _CairoDockLabelDescription CairoDockLabelDescription;
typedef struct _CairoDockInternalModule CairoDockInternalModule;


typedef enum {
	CAIRO_DOCK_VERTICAL = 0,
	CAIRO_DOCK_HORIZONTAL
	} CairoDockTypeHorizontality;

typedef enum {
	CAIRO_DOCK_TYPE_DOCK = 0,
	CAIRO_DOCK_TYPE_DESKLET,
	CAIRO_DOCK_TYPE_DIALOG,
	CAIRO_DOCK_TYPE_FLYING_CONTAINER
	} CairoDockTypeContainer;

#define CAIRO_DOCK_NB_DATA_SLOT 10

struct _CairoContainer {
	/// type de container.
	CairoDockTypeContainer iType;
	/// La fenetre du widget.
	GtkWidget *pWidget;
	/// Taille de la fenetre. La surface allouee a l'applet s'en deduit.
	gint iWidth, iHeight;
	/// Position de la fenetre.
	gint iWindowPositionX, iWindowPositionY;
	/// Vrai ssi le pointeur est dans le desklet (widgets fils inclus).
	gboolean bInside;
	/// TRUE ssi le container est horizontal.
	CairoDockTypeHorizontality bIsHorizontal;
	/// TRUE ssi le container est oriente vers le haut.
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
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
};

#define CAIRO_CONTAINER(p) ((CairoContainer *) (p))


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
};

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
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
	
	/// la liste de ses icones.
	GList* icons;
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
	/// ratio applique aux icones.
	gdouble fRatio;

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
	/// serial ID du thread de grossisement des icones.
	gint iSidGrowUp;
	/// serial ID du thread de diminution des icones.
	gint iSidShrinkDown;
	/// serial ID du thread qui enverra le signal de sortie retarde.
	gint iSidLeaveDemand;
	/// serial ID du thread qui signale qu'on peut inserer un element dans le dock a cet endroit.
	gint iSidDropIndicator;
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
	/// dit si la vue courante utilise les reflets ou pas (utile pour les plug-ins).
	gboolean bUseReflect;
	/// decalage du signal d'insertion pour son animation.
	gint iDropIndicatorOffset;
	gint iDropIndicatorRotation;
	/// TRUE ssi on peut dropper entre 2 icones.
	gboolean bCanDrop;
};


typedef enum {
	CAIRO_DOCK_CATEGORY_SYSTEM,
	CAIRO_DOCK_CATEGORY_THEME,
	CAIRO_DOCK_CATEGORY_APPLET_ACCESSORY,
	CAIRO_DOCK_CATEGORY_APPLET_DESKTOP,
	CAIRO_DOCK_CATEGORY_APPLET_CONTROLER,
	CAIRO_DOCK_CATEGORY_PLUG_IN,
	CAIRO_DOCK_NB_CATEGORY
	} CairoDockPluginCategory;
#define CAIRO_DOCK_CATEGORY_DESKTOP CAIRO_DOCK_CATEGORY_APPLET_DESKTOP
#define CAIRO_DOCK_CATEGORY_ACCESSORY CAIRO_DOCK_CATEGORY_APPLET_ACCESSORY
#define CAIRO_DOCK_CATEGORY_CONTROLER CAIRO_DOCK_CATEGORY_APPLET_CONTROLER

struct _CairoDockVisitCard {
	/// nom du module qui servira a l'identifier.
	gchar *cModuleName;
	/// chemin d'un fichier readme destine a presenter de maniere succinte le module.
	gchar *cReadmeFilePath;
	/// numero de version majeure de cairo-dock necessaire au bon fonctionnement du module.
	short iMajorVersionNeeded;
	/// numero de version mineure de cairo-dock necessaire au bon fonctionnement du module.
	short iMinorVersionNeeded;
	/// numero de version micro de cairo-dock necessaire au bon fonctionnement du module.
	short iMicroVersionNeeded;
	/// chemin d'une image de previsualisation.
	gchar *cPreviewFilePath;
	/// Nom du domaine pour la traduction du module par 'gettext'.
	gchar *cGettextDomain;
	/// Version du dock pour laquelle a ete compilee le module.
	gchar *cDockVersionOnCompilation;
	/// version courante du module.
	gchar *cModuleVersion;
	/// repertoire du plug-in cote utilisateur.
	gchar *cUserDataDir;
	/// repertoire d'installation du plug-in.
	gchar *cShareDataDir;
	/// nom de son fichier de conf.
	gchar *cConfFileName;
	/// categorie de l'applet.
	CairoDockPluginCategory iCategory;
	/// chemin d'une image pour l'icone du module dans le panneau de conf du dock.
	gchar *cIconFilePath;
	/// taille de la structure contenant la config du module.
	gint iSizeOfConfig;
	/// taille de la structure contenant les donnees du module.
	gint iSizeOfData;
	/// octets reserves pour preserver la compatibilite binaire lors de futurs ajouts sur l'interface entre plug-ins et dock.
	/// VRAI ssi le plug-in peut etre instancie plusiers fois.
	gboolean bMultiInstance;
	char reserved[28];
};

struct _CairoDockModuleInterface {
	void (* initModule) (CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
	void (* stopModule) (CairoDockModuleInstance *pInstance);
	gboolean (* reloadModule) (CairoDockModuleInstance *pInstance, CairoContainer *pOldContainer, GKeyFile *pKeyFile);
	gboolean (* read_conf_file) (CairoDockModuleInstance *pInstance, GKeyFile *pKeyFile);
	void (* reset_config) (CairoDockModuleInstance *pInstance);
	void (* reset_data) (CairoDockModuleInstance *pInstance);
};

struct _CairoDockModuleInstance {
	CairoDockModule *pModule;
	gchar *cConfFilePath;
	gboolean bCanDetach;
	Icon *pIcon;
	CairoContainer *pContainer;
	CairoDock *pDock;
	CairoDesklet *pDesklet;
	cairo_t *pDrawContext;
	gint iSlotID;
	/**gpointer *myConfig;
	gpointer *myData;*/
};

/// Construit et renvoie la carte de visite du module.
typedef gboolean (* CairoDockModulePreInit) (CairoDockVisitCard *pVisitCard, CairoDockModuleInterface *pInterface);

/// Initialise le module, et renvoie son icone si il en a.
/*typedef void (*CairoDockModuleInit) (GKeyFile *pKeyFile, CairoDockModuleInstance *pInstance);

/// Stoppe le module et libere toutes les ressources allouees par lui.
typedef void (*CairoDockModuleStop) (CairoDockModuleInstance *pInstance);

/// Recharge le module (optionnel).
typedef gboolean (*CairoDockModuleReload) (GKeyFile *pKeyFile, CairoDockModuleInstance *pInstance, CairoContainer *pOldContainer);*/

struct _CairoDockModule {
	/// chemin du .so
	gchar *cSoFilePath;
	/// structure du module, contenant les pointeurs vers les fonctions du module.
	GModule *pModule;
	/// fonctions d'interface du module.
	CairoDockModuleInterface *pInterface;
	/// carte de visite du module.
	CairoDockVisitCard *pVisitCard;
	/// chemin du fichier de conf du module.
	gchar *cConfFilePath;
	/// TRUE si le module est actif (c'est-a-dire utilise).
	///gboolean bActive;
	/// VRAI ssi l'appet est prevue pour pouvoir se detacher.
	gboolean bCanDetach;
	/// le container dans lequel va se charger le module, ou NULL.
	///CairoContainer *pContainer;
	/// Heure de derniere (re)activation du module.
	gdouble fLastLoadingTime;
	/// Liste d'instance du plug-in.
	GList *pInstancesList;
};

struct _CairoDockMinimalAppletConfig {
	gint iDesiredIconWidth;
	gint iDesiredIconHeight;
	gchar *cLabel;
	gchar *cIconFileName;
	gdouble fOrder;
	gchar *cDockName;
	gboolean bDeskletUseSize;
	gint iDeskletWidth;
	gint iDeskletHeight;
	gint iDeskletPositionX;
	gint iDeskletPositionY;
	gboolean bIsDetached;
	gboolean bKeepBelow;
	gboolean bKeepAbove;
	gboolean bOnWidgetLayer;
	gboolean bPositionLocked;
	gint iRotation;
	gchar *cDecorationTheme;
	CairoDeskletDecoration *pUserDecoration;
};

typedef void (* CairoDockApplyConfigFunc) (gpointer data);

typedef void (* CairoDockActionOnAnswerFunc) (int iAnswer, GtkWidget *pWidget, gpointer data);

typedef gpointer CairoDialogRendererDataParameter;
typedef CairoDialogRendererDataParameter* CairoDialogRendererDataPtr;
typedef gpointer CairoDialogRendererConfigParameter;
typedef CairoDialogRendererConfigParameter* CairoDialogRendererConfigPtr;

typedef void (* CairoDialogRenderFunc) (cairo_t *pCairoContext, CairoDialog *pDialog);
typedef gpointer (* CairoDialogConfigureRendererFunc) (CairoDialog *pDialog, cairo_t *pSourceContext, CairoDialogRendererConfigPtr pConfig);
typedef void (* CairoDialogUpdateRendererDataFunc) (CairoDialog *pDialog, CairoDialogRendererDataPtr pNewData);
typedef void (* CairoDialogFreeRendererDataFunc) (CairoDialog *pDialog);
struct _CairoDialogRenderer {
	CairoDialogRenderFunc 				render;
	CairoDialogConfigureRendererFunc 	configure;
	CairoDialogFreeRendererDataFunc 	free_data;
	CairoDialogUpdateRendererDataFunc 	update;
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
	/// pour l'animation des desklets.
	gint iSidGLAnimation;
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
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
	/// rayon des coins.
	gdouble fRadius;
	/// hauteur de la pointe, sans la partie "aiguisee".
	gdouble fTipHeight;
	/// surface representant le message du dialogue.
	cairo_surface_t* pTextBuffer;
	/// surface representant l'icone dans la marge a gauche du texte.
	cairo_surface_t* pIconBuffer;
	/// dimension de l'icone, sans les marges (0 si aucune icone).
	gint iIconSize;
	/// hauteur du texte, sans les marges.
	gint iTextHeight;
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
	/// Marge due au rayon.
	gint iMargin;
	/// decalage pour l'effet de clique sur le bouton Ok.
	gint iButtonOkOffset;
	/// decalage pour l'effet de clique sur le bouton Annuler.
	gint iButtonCancelOffset;
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
	/// le type des boutons (GTK_BUTTONS_NONE, GTK_BUTTONS_OK_CANCEL ou GTK_BUTTONS_YES_NO).
	gint iButtonsType;
	/// fonction appelee au clique sur l'un des 2 boutons.
	CairoDockActionOnAnswerFunc action_on_answer;
	/// donnees transmises a la fonction.
	gpointer pUserData;
	/// fonction appelee pour liberer les donnees.
	GFreeFunc pFreeUserDataFunc;
	/// icone sur laquelle pointe le dialogue.
	Icon *pIcon;
};


typedef enum {
	CAIRO_DOCK_LAUNCHER = 0,
	CAIRO_DOCK_SEPARATOR12,
	CAIRO_DOCK_APPLI,
	CAIRO_DOCK_SEPARATOR23,
	CAIRO_DOCK_APPLET,
	CAIRO_DOCK_NB_TYPES
	} CairoDockIconType;

typedef enum {
	CAIRO_DOCK_BOUNCE = 0,
	CAIRO_DOCK_ROTATE,
	CAIRO_DOCK_BLINK,
	CAIRO_DOCK_PULSE,
	CAIRO_DOCK_UPSIDE_DOWN,
	CAIRO_DOCK_WOBBLY,
	CAIRO_DOCK_RANDOM,
	CAIRO_DOCK_NB_ANIMATIONS,
	CAIRO_DOCK_FOLLOW_MOUSE,
	CAIRO_DOCK_AVOID_MOUSE
	} CairoDockAnimationType;


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
	/// Largeur de l'image de l'icone.
	gdouble fWidth;
	/// Hauteur de l'image de l'icone.
	gdouble fHeight;
	/// Surface cairo de l'image.
	cairo_surface_t* pIconBuffer;
	/// Surface cairo de l'etiquette.
	cairo_surface_t* pTextBuffer;
	/// Surface cairo du reflet.
	cairo_surface_t* pReflectionBuffer;
	/// Surface cairo de l'image et de son reflet.
	cairo_surface_t* pFullIconBuffer;
	/// Largeur de l'etiquette.
	gint iTextWidth;
	/// Hauteur de l'etiquette.
	gint iTextHeight;
	/// Decalage en X de l'etiquette.
	gdouble fTextXOffset;
	/// Decalage en Y de l'etiquette.
	gdouble fTextYOffset;
	/// Abscisse maximale (droite) que l'icone atteindra (variable avec la vague).
	gdouble fXMax;
	/// Abscisse minimale (gauche) que l'icone atteindra (variable avec la vague).
	gdouble fXMin;
	//\____________ calcules a chaque scroll et insertion/suppression d'une icone.
	/// Abscisse de l'icone au repos.
	gdouble fXAtRest;
	//\____________ calcules a chaque fois.
	/// Phase de l'icone (entre -pi et pi).
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
	/// Position de l'icone hors animation.
	gdouble fDrawXAtRest, fDrawYAtRest;
	/// Facteur de zoom sur la largeur de l'icone.
	gdouble fWidthFactor;
	/// Facteur de zoom sur la hauteur de l'icone.
	gdouble fHeightFactor;
	/// Transparence (<= 1).
	gdouble fAlpha;
	/// TRUE ssi l'icone est couramment pointee.
	gboolean bPointed;
	/// Compteur de l'animation de l'icone (> 0 si une animation est en cours, 0 sinon).
	gint iCount;
	/// Type de l'animation.
	CairoDockAnimationType iAnimationType;
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
	/// Heure de derniere verification de la presence de l'application dans la barre des taches.
	gdouble fLastCheckTime;
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
	/// TRUE ssi l'icone a un indicateur (elle controle une appli).
	gboolean bHasIndicator;
	/// ID du pixmap de sauvegarde de la fenetre pour quand elle est cachee.
	Pixmap iBackingPixmap;
	//Damage iDamageHandle;
	/// decalage pour le glissement des icones.
	gdouble fGlideOffset;
	/// direction dans laquelle glisse l'icone.
	gint iGlideDirection;
	/// echelle d'adaptation au glissement.
	double fGlideScale;
	
	GLuint iIconTexture;
	GLuint iReflectionTexture;
	GLuint iFullIconTexture;
	GLuint iLabelTexture;
	GLuint iQuickInfoTexture;
	gboolean bOnMouseOverAnimating;
	gpointer pDataSlot[CAIRO_DOCK_NB_DATA_SLOT];
};


typedef enum {
	CAIRO_DOCK_FILE_MODIFIED=0,
	CAIRO_DOCK_FILE_DELETED,
	CAIRO_DOCK_FILE_CREATED,
	CAIRO_DOCK_NB_EVENT_ON_FILES
	} CairoDockFMEventType;

typedef enum {
	CAIRO_DOCK_FM_SORT_BY_NAME=0,
	CAIRO_DOCK_FM_SORT_BY_DATE,
	CAIRO_DOCK_FM_SORT_BY_SIZE,
	CAIRO_DOCK_FM_SORT_BY_TYPE,
	CAIRO_DOCK_NB_SORT_ON_FILE
	} CairoDockFMSortType;

typedef void (*CairoDockFMGetFileInfoFunc) (const gchar *cBaseURI, gchar **cName, gchar **cURI, gchar **cIconName, gboolean *bIsDirectory, int *iVolumeID, double *fOrder, CairoDockFMSortType iSortType);
typedef void (*CairoDockFMFilePropertiesFunc) (const gchar *cURI, guint64 *iSize, time_t *iLastModificationTime, gchar **cMimeType, int *iUID, int *iGID, int *iPermissionsMask);
typedef GList * (*CairoDockFMListDirectoryFunc) (const gchar *cURI, CairoDockFMSortType g_fm_iSortType, int iNewIconsType, gboolean bListHiddenFiles, gchar **cFullURI);
typedef void (*CairoDockFMLaunchUriFunc) (const gchar *cURI);

typedef gchar * (*CairoDockFMIsMountedFunc) (const gchar *cURI, gboolean *bIsMounted);
typedef gboolean (*CairoDockFMCanEjectFunc) (const gchar *cURI);
typedef gboolean (*CairoDockFMEjectDriveFunc) (const gchar *cURI);

typedef void (*CairoDockFMMountCallback) (gboolean bMounting, gboolean bSuccess, const gchar *cName, Icon *icon, CairoContainer *pContainer);
typedef void (*CairoDockFMMountFunc) (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer);
typedef void (*CairoDockFMUnmountFunc) (const gchar *cURI, int iVolumeID, CairoDockFMMountCallback pCallback, Icon *icon, CairoContainer *pContainer);

typedef void (*CairoDockFMMonitorCallback) (CairoDockFMEventType iEventType, const gchar *cURI, gpointer data);
typedef void (*CairoDockFMAddMonitorFunc) (const gchar *cURI, gboolean bDirectory, CairoDockFMMonitorCallback pCallback, gpointer data);
typedef void (*CairoDockFMRemoveMonitorFunc) (const gchar *cURI);

typedef gboolean (*CairoDockFMDeleteFileFunc) (const gchar *cURI);
typedef gboolean (*CairoDockFMRenameFileFunc) (const gchar *cOldURI, const gchar *cNewName);
typedef gboolean (*CairoDockFMMoveFileFunc) (const gchar *cURI, const gchar *cDirectoryURI);

typedef gchar * (*CairoDockFMGetTrashFunc) (const gchar *cNearURI, gchar **cFileInfoPath);
typedef gchar * (*CairoDockFMGetDesktopFunc) (void);
typedef void (*CairoDockFMUserActionFunc) (void);

struct _CairoDockVFSBackend {
	CairoDockFMGetFileInfoFunc 		get_file_info;
	CairoDockFMFilePropertiesFunc 	get_file_properties;
	CairoDockFMListDirectoryFunc 	list_directory;
	CairoDockFMLaunchUriFunc 		launch_uri;
	CairoDockFMIsMountedFunc 		is_mounted;
	CairoDockFMCanEjectFunc 		can_eject;
	CairoDockFMEjectDriveFunc 		eject;
	CairoDockFMMountFunc 			mount;
	CairoDockFMUnmountFunc 		unmount;
	CairoDockFMAddMonitorFunc 	add_monitor;
	CairoDockFMRemoveMonitorFunc 	remove_monitor;
	CairoDockFMDeleteFileFunc 		delete_file;
	CairoDockFMRenameFileFunc 	rename;
	CairoDockFMMoveFileFunc 		move;
	CairoDockFMGetTrashFunc 		get_trash_path;
	CairoDockFMGetDesktopFunc 	get_desktop_path;
	CairoDockFMUserActionFunc		logout;
	CairoDockFMUserActionFunc		shutdown;
	CairoDockFMUserActionFunc		setup_time;
};


typedef gpointer CairoDeskletRendererDataParameter;
typedef CairoDeskletRendererDataParameter* CairoDeskletRendererDataPtr;
typedef gpointer CairoDeskletRendererConfigParameter;
typedef CairoDeskletRendererConfigParameter* CairoDeskletRendererConfigPtr;
typedef struct {
	gchar *cName;
	CairoDeskletRendererConfigPtr pConfig;
} CairoDeskletRendererPreDefinedConfig;
typedef void (* CairoDeskletRenderFunc) (cairo_t *pCairoContext, CairoDesklet *pDesklet, gboolean bRenderOptimized);
typedef void (*CairoDeskletGLRenderFunc) (CairoDesklet *pDesklet);
typedef gpointer (* CairoDeskletConfigureRendererFunc) (CairoDesklet *pDesklet, cairo_t *pSourceContext, CairoDeskletRendererConfigPtr pConfig);
typedef void (* CairoDeskletLoadRendererDataFunc) (CairoDesklet *pDesklet, cairo_t *pSourceContext);
typedef void (* CairoDeskletUpdateRendererDataFunc) (CairoDesklet *pDesklet, CairoDeskletRendererDataPtr pNewData);
typedef void (* CairoDeskletFreeRendererDataFunc) (CairoDesklet *pDesklet);
typedef void (* CairoDeskletLoadIconsFunc) (CairoDesklet *pDesklet, cairo_t *pSourceContext);
struct _CairoDeskletRenderer {
	CairoDeskletRenderFunc 			render;
	CairoDeskletGLRenderFunc 		render_opengl;
	CairoDeskletConfigureRendererFunc 	configure;
	CairoDeskletLoadRendererDataFunc 	load_data;
	CairoDeskletFreeRendererDataFunc 	free_data;
	CairoDeskletLoadIconsFunc 		load_icons;
	CairoDeskletUpdateRendererDataFunc 	update;
	GList *pPreDefinedConfigList;
};

struct _CairoDesklet {
	/// type "desklet".
	CairoDockTypeContainer iType;
	/// La fenetre du widget.
	GtkWidget *pWidget;
	/// Taille de la fenetre. La surface allouee a l'applet s'en deduit.
	gint iWidth, iHeight;
	/// Position de la fenetre.
	gint iWindowPositionX, iWindowPositionY;
	/// Vrai ssi le pointeur est dans le desklet (widgets fils inclus).
	gboolean bInside;
	/// Toujours vrai pour un desklet.
	CairoDockTypeHorizontality bIsHorizontal;
	/// donne l'orientation du desket (toujours TRUE).
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
	/// pour l'animation des desklets.
	gint iSidGLAnimation;
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
	/// le moteur de rendu utilise pour dessiner le desklet.
	CairoDeskletRenderer *pRenderer;
	/// donnees pouvant etre utilisees par le moteur de rendu.
	gpointer pRendererData;
	/// Liste eventuelle d'icones placees sur le desklet, et susceptibles de recevoir des clics.
	GList *icons;
	/// pour le deplacement manuel de la fenetre.
	gboolean moving;
	/// L'icone de l'applet.
	Icon *pIcon;
	/// un timer pour retarder l'ecriture dans le fichier lors des deplacements.
	gint iSidWritePosition;
	/// un timer pour retarder l'ecriture dans le fichier lors des redimensionnements.
	gint iSidWriteSize;
	/// compteur pour le fondu lors de l'entree dans le desklet.
	gint iGradationCount;
	/// timer associe.
	gint iSidGradationOnEnter;
	/// TRUE ssi on ne peut pas deplacer le widget a l'aide du simple clic gauche.
	gboolean bPositionLocked;
	/// un timer pour faire apparaitre le desklet avec un effet de zoom lors du detachage d'une applet.
	gint iSidGrowUp;
	/// le facteur de zoom correspondant.
	gdouble fZoom;
	/// taille a atteindre (fixee par l'utilisateur dans le.conf)
	gint iDesiredWidth, iDesiredHeight;
	/// taille connue par l'applet associee.
	gint iKnownWidth, iKnownHeight;
	/// les decorations.
	gchar *cDecorationTheme;
	CairoDeskletDecoration *pUserDecoration;
	gint iLeftSurfaceOffset;
        gint iTopSurfaceOffset;
        gint iRightSurfaceOffset;
        gint iBottomSurfaceOffset;
        cairo_surface_t *pBackGroundSurface;
        cairo_surface_t *pForeGroundSurface;
        gdouble fImageWidth;
        gdouble fImageHeight;
	gdouble fBackGroundAlpha;
        gdouble fForeGroundAlpha;
	/// rotation.
	gdouble fRotation;
	gboolean rotating;
	/// rattachement au dock.
	gboolean retaching;
	/// date du clic.
	guint time;
	GLuint iBackGroundTexture;
	GLuint iForeGroundTexture;
};

typedef enum {
	CAIRO_DOCK_FILL_SPACE 		= 0,
	CAIRO_DOCK_KEEP_RATIO 		= 1<<0,
	CAIRO_DOCK_DONT_ZOOM_IN 	= 1<<1
	} CairoDockLoadImageModifier;

struct _CairoDeskletDecoration {
	gchar *cBackGroundImagePath;
	gchar *cForeGroundImagePath;
	CairoDockLoadImageModifier iLoadingModifier;
	gdouble fBackGroundAlpha;
        gdouble fForeGroundAlpha;
	gint iLeftMargin;
	gint iTopMargin;
	gint iRightMargin;
	gint iBottomMargin;
	};

#define CAIRO_DOCK_FM_VFS_ROOT "_vfsroot_"
#define CAIRO_DOCK_FM_NETWORK "_network_"
#define CAIRO_DOCK_FM_TRASH "_trash_"
#define CAIRO_DOCK_FM_DESKTOP "_desktop_"


typedef gboolean (* CairoDockForeachDeskletFunc) (CairoDesklet *pDesklet, CairoDockModuleInstance *pInstance, gpointer data);

typedef void (* CairoDockForeachIconFunc) (Icon *icon, gpointer data);

typedef void (* CairoDockConfigFunc) (gchar *cConfFile, gpointer data);


struct _CairoDockClassAppli {
	/// TRUE ssi l'appli doit utiliser l'icone fournie par X au lieu de celle du theme.
	gboolean bUseXIcon;
	/// Liste des inhibiteurs de la classe.
	GList *pIconsOfClass;
	/// Liste des icones d'appli de cette classe.
	GList *pAppliOfClass;
};

struct _CairoDockLabelDescription {
	/// Taille de la police (et hauteur du texte en pixels).
	gint iSize;
	/// Police de caracteres.
	gchar *cFont;
	/// Epaisseur des traits.
	PangoWeight iWeight;
	/// Style du trace (italique ou droit).
	PangoStyle iStyle;
	/// Couleur de debut du dégradé.
	gdouble fColorStart[3];
	/// Couleur de fin du dégradé.
	gdouble fColorStop[3];
	/// TRUE ssi le dégradé est du haut vers le bas.
	gboolean bVerticalPattern;
	/// Couleur du fond.
	gdouble fBackgroundColor[4];
};


struct _CairoFlyingContainer {
        /// type de container.
        CairoDockTypeContainer iType;
        /// La fenetre du widget.
        GtkWidget *pWidget;
        /// Taille de la fenetre. La surface allouee a l'applet s'en deduit.
        gint iWidth, iHeight;
        /// Position de la fenetre.
        gint iPositionX, iPositionY;
        /// TRUE ssi le pointeur est dedans.
        gboolean bInside;
        /// TRUE ssi le container est horizontal.
        CairoDockTypeHorizontality bIsHorizontal;
        /// TRUE ssi le container est oriente vers le haut.
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
	/// pour l'animation des desklets.
	gint iSidGLAnimation;
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
	/// L'icone volante.
	Icon *pIcon;
	/// le timer de l'animation.
	gint iSidAnimationTimer;
	/// compteur pour l'animation.
	gint iAnimationCount;
};

typedef gpointer CairoInternalModuleConfigPtr;
typedef gpointer CairoInternalModuleDataPtr;
typedef void (* CairoDockInternalModuleReloadFunc) (CairoInternalModuleConfigPtr *pPrevConfig, CairoInternalModuleConfigPtr *pNewConfig);
typedef gboolean (* CairoDockInternalModuleGetConfigFunc) (GKeyFile *pKeyFile, CairoInternalModuleConfigPtr *pConfig);
typedef void (* CairoDockInternalModuleResetConfigFunc) (CairoInternalModuleConfigPtr *pConfig);
typedef void (* CairoDockInternalModuleResetDataFunc) (CairoInternalModuleDataPtr *pData);
struct _CairoDockInternalModule {
	//\_____________ Carte de visite.
	gchar *cModuleName;
	gchar *cDescription;
	gchar *cIcon;
	gchar *cTitle;
	CairoDockPluginCategory iCategory;
	gint iSizeOfConfig;
	gint iSizeOfData;
	//\_____________ Interface.
	CairoDockInternalModuleReloadFunc reload;
	CairoDockInternalModuleGetConfigFunc get_config;
	CairoDockInternalModuleResetConfigFunc reset_config;
	CairoDockInternalModuleResetDataFunc reset_data;
	//\_____________ Instance.
	CairoInternalModuleConfigPtr pConfig;
	CairoInternalModuleDataPtr pData;
};


/// Nom du repertoire de travail de cairo-dock.
#define CAIRO_DOCK_DATA_DIR "cairo-dock"
/// Nom du repertoire des themes utilisateur.
#define CAIRO_DOCK_THEMES_DIR "themes"
/// Nom du repertoire des extras utilisateur/themes (jauges, clock, etc).
#define CAIRO_DOCK_EXTRAS_DIR "extras"
/// Nom du repertoire des jauges utilisateur/themes.
#define CAIRO_DOCK_GAUGES_DIR "gauges"
/// Nom du repertoire du theme courant.
#define CAIRO_DOCK_CURRENT_THEME_NAME "current_theme"
/// Nom du repertoire des lanceurs.
#define CAIRO_DOCK_LAUNCHERS_DIR "launchers"
/// Nom du repertoire des icones locales.
#define CAIRO_DOCK_LOCAL_ICONS_DIR "icons"
/// Mot cle representant le repertoire local des icones.
#define CAIRO_DOCK_LOCAL_THEME_KEYWORD "_LocalTheme_"
/// Nom du dock principal (le 1er cree).
#define CAIRO_DOCK_MAIN_DOCK_NAME "_MainDock_"
/// Nom de la vue par defaut.
#define CAIRO_DOCK_DEFAULT_RENDERER_NAME "default"


#define CAIRO_DOCK_LAST_ORDER -1e9
#define CAIRO_DOCK_NB_MAX_ITERATIONS 1000

#define CAIRO_DOCK_LOAD_ICONS_FOR_DESKLET TRUE

#define CAIRO_DOCK_UPDATE_DOCK_SIZE TRUE
#define CAIRO_DOCK_ANIMATE_ICON TRUE
#define CAIRO_DOCK_APPLY_RATIO TRUE
#define CAIRO_DOCK_INSERT_SEPARATOR TRUE

#ifdef CAIRO_DOCK_VERBOSE
#define CAIRO_DOCK_MESSAGE if (g_bVerbose) g_message
#else
#define CAIRO_DOCK_MESSAGE(s, ...)
#endif

#define CAIRO_DOCK_GL_ANIMATION_DT 30.

typedef enum {
	CAIRO_DOCK_MAX_SIZE,
	CAIRO_DOCK_NORMAL_SIZE,
	CAIRO_DOCK_MIN_SIZE
	} CairoDockSizeType;

typedef enum {
	CAIRO_DOCK_MOUSE_INSIDE,
	CAIRO_DOCK_MOUSE_ON_THE_EDGE,
	CAIRO_DOCK_MOUSE_OUTSIDE
	} CairoDockMousePositionType;

typedef enum {
	CAIRO_DOCK_UNKNOWN_ENV=0,
	CAIRO_DOCK_GNOME,
	CAIRO_DOCK_KDE,
	CAIRO_DOCK_XFCE,
	CAIRO_DOCK_NB_DESKTOPS
	} CairoDockDesktopEnv;

typedef enum {
	CAIRO_DOCK_BOTTOM = 0,
	CAIRO_DOCK_TOP,
	CAIRO_DOCK_RIGHT,
	CAIRO_DOCK_LEFT,
	CAIRO_DOCK_NB_POSITIONS
	} CairoDockPositionType;

typedef enum {
	CAIRO_DOCK_LAUNCHER_FROM_DESKTOP_FILE = 0,
	CAIRO_DOCK_LAUNCHER_FOR_CONTAINER,
	CAIRO_DOCK_LAUNCHER_FOR_SEPARATOR,
	CAIRO_DOCK_NB_NEW_LAUNCHER_TYPE
	} CairoDockNewLauncherType;

#endif
