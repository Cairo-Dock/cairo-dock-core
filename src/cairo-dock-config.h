
#ifndef __CAIRO_DOCK_CONFIG__
#define  __CAIRO_DOCK_CONFIG__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*Recupere une cle booleene d'un fichier de cles.
*@param pKeyFile le fichier de cles.
*@param cGroupName le com du groupe.
*@param cKeyName le nom de la cle.
*@param bFlushConfFileNeeded est mis a TRUE si la cle est manquante.
*@param bDefaultValue valeur par defaut a utiliser et a inserer dans le fichier de cles au cas ou la cle est manquante.
*@param cDefaultGroupName nom de groupe alternatif, ou NULL si aucun autre.
*@param cDefaultKeyName nom de cle alternative, ou NULL si aucune autre.
*@return la valeur booleene de la cle.
*/
gboolean cairo_dock_get_boolean_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gboolean bDefaultValue, gchar *cDefaultGroupName, gchar *cDefaultKeyName);
/**
*Recupere une cle entiere d'un fichier de cles.
*@param pKeyFile le fichier de cles.
*@param cGroupName le com du groupe.
*@param cKeyName le nom de la cle.
*@param bFlushConfFileNeeded est mis a TRUE si la cle est manquante.
*@param iDefaultValue valeur par defaut a utiliser et a inserer dans le fichier de cles au cas ou la cle est manquante.
*@param cDefaultGroupName nom de groupe alternatif, ou NULL si aucun autre.
*@param cDefaultKeyName nom de cle alternative, ou NULL si aucune autre.
@return la valeur entiere de la cle.
*/
int cairo_dock_get_integer_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, int iDefaultValue, gchar *cDefaultGroupName, gchar *cDefaultKeyName);
/**
*Recupere une cle flottante d'un fichier de cles.
*@param pKeyFile le fichier de cles.
*@param cGroupName le com du groupe.
*@param cKeyName le nom de la cle.
*@param bFlushConfFileNeeded est mis a TRUE si la cle est manquante.
*@param fDefaultValue valeur par defaut a utiliser et a inserer dans le fichier de cles au cas ou la cle est manquante.
*@param cDefaultGroupName nom de groupe alternatif, ou NULL si aucun autre.
*@param cDefaultKeyName nom de cle alternative, ou NULL si aucune autre.
*@return la valeur flottante de la cle.
*/
double cairo_dock_get_double_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, double fDefaultValue, gchar *cDefaultGroupName, gchar *cDefaultKeyName);
/**
*Recupere une cle d'un fichier de cles sous la forme d'une chaine de caractere.
*@param pKeyFile le fichier de cles.
*@param cGroupName le com du groupe.
*@param cKeyName le nom de la cle.
*@param bFlushConfFileNeeded est mis a TRUE si la cle est manquante.
*@param cDefaultValue valeur par defaut a utiliser et a inserer dans le fichier de cles au cas ou la cle est manquante.
*@param cDefaultGroupName nom de groupe alternatif, ou NULL si aucun autre.
*@param cDefaultKeyName nom de cle alternative, ou NULL si aucune autre.
*@return la chaine de caractere nouvellement allouee correspondante a la cle.
*/
gchar *cairo_dock_get_string_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultValue, gchar *cDefaultGroupName, gchar *cDefaultKeyName);
/**
*Recupere une cle d'un fichier de cles sous la forme d'un tableau d'entiers.
*@param pKeyFile le fichier de cles.
*@param cGroupName le com du groupe.
*@param cKeyName le nom de la cle.
*@param bFlushConfFileNeeded est mis a TRUE si la cle est manquante.
*@param iValueBuffer tableau qui sera rempli.
*@param iNbElements nombre d'elements a recuperer; c'est le nombre d'elements du tableau passe en entree.
*@param iDefaultValues valeur par defaut a utiliser et a inserer dans le fichier de cles au cas ou la cle est manquante.
*@param cDefaultGroupName nom de groupe alternatif, ou NULL si aucun autre.
*@param cDefaultKeyName nom de cle alternative, ou NULL si aucune autre.
*/
void cairo_dock_get_integer_list_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, int *iValueBuffer, int iNbElements, int *iDefaultValues, gchar *cDefaultGroupName, gchar *cDefaultKeyName);
/**
*Recupere une cle d'un fichier de cles sous la forme d'un tableau de doubles.
*@param pKeyFile le fichier de cles.
*@param cGroupName le com du groupe.
*@param cKeyName le nom de la cle.
*@param bFlushConfFileNeeded est mis a TRUE si la cle est manquante.
*@param fValueBuffer tableau qui sera rempli.
*@param iNbElements nombre d'elements a recuperer; c'est le nombre d'elements du tableau passe en entree.
*@param fDefaultValues valeur par defaut a utiliser et a inserer dans le fichier de cles au cas ou la cle est manquante.
*@param cDefaultGroupName nom de groupe alternatif, ou NULL si aucun autre.
*@param cDefaultKeyName nom de cle alternative, ou NULL si aucune autre.
*/
void cairo_dock_get_double_list_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, double *fValueBuffer, int iNbElements, double *fDefaultValues, gchar *cDefaultGroupName, gchar *cDefaultKeyName);
/**
*Recupere une cle d'un fichier de cles sous la forme d'un tableau de chaines de caracteres.
*@param pKeyFile le fichier de cles.
*@param cGroupName le com du groupe.
*@param cKeyName le nom de la cle.
*@param bFlushConfFileNeeded est mis a TRUE si la cle est manquante.
*@param length nombre de chaines de caracteres recuperees.
*@param cDefaultValues valeur par defaut a utiliser et a inserer dans le fichier de cles au cas ou la cle est manquante.
*@param cDefaultGroupName nom de groupe alternatif, ou NULL si aucun autre.
*@param cDefaultKeyName nom de cle alternative, ou NULL si aucune autre.
*@return un tableau de chaines de caracteres; a liberer avec g_strfreev().
*/
gchar **cairo_dock_get_string_list_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gsize *length, gchar *cDefaultValues, gchar *cDefaultGroupName, gchar *cDefaultKeyName);
/**
*Recupere une cle d'un fichier de cles sous la forme d'un type d'animation.
*@param pKeyFile le fichier de cles.
*@param cGroupName le com du groupe.
*@param cKeyName le nom de la cle.
*@param bFlushConfFileNeeded est mis a TRUE si la cle est manquante.
*@param iDefaultAnimation valeur par defaut a utiliser et a inserer dans le fichier de cles au cas ou la cle est manquante.
*@param cDefaultGroupName nom de groupe alternatif, ou NULL si aucun autre.
*@param cDefaultKeyName nom de cle alternative, ou NULL si aucune autre.
*@return le type de l'animation correspondante a la cle.
*/
CairoDockAnimationType cairo_dock_get_animation_type_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, CairoDockAnimationType iDefaultAnimation, gchar *cDefaultGroupName, gchar *cDefaultKeyName);
/**
*Recupere une cle d'un fichier de cles sous la forme d'un chemin de fichier complet. La clé peut soit être un fichier relatif au thème courant, soit un chemin començant par '~', soit un chemin complet, soit vide auquel cas le chemin d'un fichier par defaut est renvoye s'il est specifie.
*@param pKeyFile le fichier de cles.
*@param cGroupName le com du groupe.
*@param cKeyName le nom de la cle.
*@param bFlushConfFileNeeded est mis a TRUE si la cle est manquante.
*@param cDefaultGroupName nom de groupe alternatif, ou NULL si aucun autre.
*@param cDefaultKeyName nom de cle alternative, ou NULL si aucune autre.
*@param cDefaultDir si la cle est vide, on prendra un fichier par defaut situe dans ce repertoire. (optionnel)
*@param cDefaultFileName si la cle est vide, on prendra ce fichier par defaut dans le repertoire defini ci-dessus. (optionnel)
*@return le chemin complet du fichier, a liberer avec g_free().
*/
gchar *cairo_dock_get_file_path_key_value (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultGroupName, gchar *cDefaultKeyName, gchar *cDefaultDir, gchar *cDefaultFileName);


/**
*Lis le fichier de conf et recharge l'appli en consequence.
*@param cConfFilePath chemin du fichier de conf.
*@param pDock le dock principal, cree prealablement si necessaire.
*/
void cairo_dock_read_conf_file (gchar *cConfFilePath, CairoDock *pDock);

/**
*Dis si l'appli est en cours de chargement.
*@return TRUE ssi le dock est en cours de rechargement.
*/
gboolean cairo_dock_is_loading (void);

/**
*Met a jour un fichier de conf avec une liste de valeurs de la forme : type, nom du groupe, nom de la cle, valeur. Finir par G_TYPE_INVALID.
*@param cConfFilePath chemin du fichier de conf.
*@param iFirstDataType type de la 1ere donnee.
*/
void cairo_dock_update_conf_file (const gchar *cConfFilePath, GType iFirstDataType, ...);
/**
*Met a jour un fichier de conf de dock racine avec sa position définie par les écarts en x et en y.
*@param cConfFilePath chemin du fichier de conf.
*@param x écart latéral.
*@param y écart vertical.
*/
void cairo_dock_update_conf_file_with_position (const gchar *cConfFilePath, int x, int y);

/**
*Essaye de determiner l'environnement de bureau dela session courante.
*@return l'environnement de bureau (couramment Gnome, KDE et XFCE sont detectés).
*/
CairoDockDesktopEnv cairo_dock_guess_environment (void);


/**
*Recupere les 3 numeros de version d'une chaine.
*@param cVersionString la version representee par une chaine.
*@param iMajorVersion numero de version majeure renvoyee.
*@param iMinorVersion numero de version mineure renvoyee.
*@param iMicroVersion numero de version micro renvoyee.
*/
void cairo_dock_get_version_from_string (gchar *cVersionString, int *iMajorVersion, int *iMinorVersion, int *iMicroVersion);



#define GET_GROUP_CONFIG_BEGIN(cGroupName) \
gboolean cairo_dock_read_conf_file_##cGroupName (GKeyFile *pKeyFile, CairoDock *pDock) \
{ \
	gboolean bFlushConfFileNeeded = FALSE;
	
#define GET_GROUP_CONFIG_END \
	return bFlushConfFileNeeded; \
}

#define cairo_dock_read_group_conf_file(cGroupName) \
cairo_dock_read_conf_file_##cGroupName (pKeyFile, pDock)

#define DEFINE_PRE_INIT(cGroupName) \
void cairo_dock_pre_init_##cGroupName (CairoDockInternalModule *pModule)


typedef struct _CairoConfigSystem {
	gboolean bUseFakeTransparency;
	gboolean bLabelForPointedIconOnly;
	gdouble fLabelAlphaThreshold;
	gboolean bTextAlwaysHorizontal;
	gdouble fUnfoldAcceleration;
	gboolean bAnimateOnAutoHide;
	gint iGrowUpInterval, iShrinkDownInterval;
	gdouble fMoveUpSpeed, fMoveDownSpeed;
	gdouble fRefreshInterval;
	gboolean bDynamicReflection;
	gboolean bAnimateSubDock;
	gdouble fStripesSpeedFactor;
	gboolean bDecorationsFollowMouse;
	gint iScrollAmount;
	gboolean bResetScrollOnLeave;
	gdouble fScrollAcceleration;
	gchar **cActiveModuleList;
	gint iFileSortType;
	gboolean bShowHiddenFiles;
	} CairoConfigSystem;

typedef struct _CairoConfigTaskBar {
	gboolean bShowAppli;
	gboolean bUniquePid;
	gboolean bGroupAppliByClass;
	gint iAppliMaxNameLength;
	gboolean bMinimizeOnClick;
	gboolean bCloseAppliOnMiddleClick;
	gboolean bAutoHideOnFullScreen;
	gboolean bAutoHideOnMaximized;
	gboolean bHideVisibleApplis;
	gdouble fVisibleAppliAlpha;
	gboolean bAppliOnCurrentDesktopOnly;
	gboolean bDemandsAttentionWithDialog;
	gboolean bDemandsAttentionWithAnimation;
	gboolean bAnimateOnActiveWindow;
	gboolean bOverWriteXIcons;
	gboolean bShowThumbnail;
	gboolean bMixLauncherAppli;
	} CairoConfigTaskBar;

typedef struct _CairoConfigHiddenDock {
	gchar *cVisibleZoneImageFile;
	gint iVisibleZoneWidth, iVisibleZoneHeight;
	gdouble fVisibleZoneAlpha;
	gboolean bReverseVisibleImage;
	} CairoConfigHiddenDock;

typedef struct _CairoConfigBackGround {
	gint iDockRadius;
	gint iDockLineWidth;
	gint iFrameMargin;
	gdouble fLineColor[4];
	gboolean bRoundedBottomCorner;
	gdouble fStripesColorBright[4];
	gchar *cBackgroundImageFile;
	gdouble fBackgroundImageAlpha;
	gboolean bBackgroundImageRepeat;
	gint iNbStripes;
	gdouble fStripesWidth;
	gdouble fStripesColorDark;
	gdouble fStripesAngle;
	} CairoConfigBackGround;

typedef struct _CairoConfigLabels {
	CairoDockLabelDescription iconTextDescription;
	CairoDockLabelDescription quickInfoTextDescription;
	} CairoConfigLabels;

typedef struct _CairoConfigIcons {
	gdouble fFieldDepth;
	gdouble fAlbedo;
	gdouble fAmplitude;
	gint iSinusoidWidth;
	gint iIconGap;
	gint iStringLineWidth;
	gdouble fStringColor;
	gdouble fAlphaAtRest;
	gpointer *pDefaultIconDirectory;
	gint tIconAuthorizedWidth[CAIRO_DOCK_NB_TYPES];
	gint tIconAuthorizedHeight[CAIRO_DOCK_NB_TYPES];
	gint tAnimationType[CAIRO_DOCK_NB_TYPES];
	gint tNbAnimationRounds[CAIRO_DOCK_NB_TYPES];
	gint tIconTypeOrder[CAIRO_DOCK_NB_TYPES];
	gboolean bUseSeparator;
	gchar *cSeparatorImage;
	gboolean bRevolveSeparator;
	gboolean bConstantSeparatorSize;
	} CairoConfigIcons;

typedef struct _CairoConfigIndicators {
	gdouble fActiveColor;
	gint iActiveLineWidth;
	gint iActiveCornerRadius;
	gboolean bActiveIndicatorAbove;
	gchar *cActiveIndicatorImagePath;
	gchar *cIndicatorImagePath;
	gboolean bIndicatorAbove;
	gdouble fIndicatorRatio;
	gboolean bLinkIndicatorWithIcon;
	gint iIndicatorDeltaY;
	gchar *cDropIndicatorImagePath;
	} CairoConfigIndicators;

typedef struct _CairoConfigDialogs {
	gchar *cButtonOkImage;
	gchar *cButtonCancelImage;
	gint iDialogButtonWidth;
	gint iDialogButtonHeight;
	gdouble fDialogColor;
	gint iDialogIconSize;
	CairoDockLabelDescription dialogTextDescription;
	
	} CairoConfigDialogs;

typedef struct _CairoConfigViews {
	gchar *cMainDockDefaultRendererName;
	gchar *cSubDockDefaultRendererName;
	gboolean bSameHorizontality;
	gdouble fSubDockSizeRatio;
	} CairoConfigViews;

typedef struct _CairoConfigDesklets {
	gchar *cDeskletDecorationsName;
	gint iDeskletButtonSize;
	gchar *cRotateButtonImage;
	gchar *cRetachButtonImage;
	} CairoConfigDesklets;

G_END_DECLS
#endif
