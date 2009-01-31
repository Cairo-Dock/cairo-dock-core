/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */

#ifndef __CAIRO_DOCK_APPLET_FACILITY__
#define  __CAIRO_DOCK_APPLET_FACILITY__

#include "cairo-dock-struct.h"
#include "cairo-dock-modules.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-applet-facility.h Les macros forment un canevas dedie aux applets. Elles permettent un developpement rapide et normalise d'une applet pour Cairo-Dock.
*
* Pour un exemple tres simple, consultez les sources de l'applet 'logout'.
*/

/**
*Applique une surface sur un contexte, en effacant tout au prealable, et en appliquant un facteur de zoom et de transparence.
*@param pIconContext le contexte du dessin; n'est pas altere par la fonction.
*@param pSurface la surface a appliquer.
*@param fScale le zoom.
*@param fAlpha la transparence.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*/
void cairo_dock_set_icon_surface_full (cairo_t *pIconContext, cairo_surface_t *pSurface, double fScale, double fAlpha, Icon *pIcon, CairoContainer *pContainer);
/**
*Applique une surface sur un contexte, en effacant tout au prealable.
*@param pIconContext le contexte du dessin; est modifie par la fonction.
*@param pSurface la surface a appliquer.
*/
#define cairo_dock_set_icon_surface(pIconContext, pSurface) cairo_dock_set_icon_surface_full (pIconContext, pSurface, 1, 1, NULL, NULL)
/**
*Dessine une barre degradee rouge->vert representant une valeur donnee a la base de l'icone.
*@param pIconContext le contexte du dessin; n'est pas altere par la fonction.
*@param fValue la valeur representant un pourcentage, <=1.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*/
void cairo_dock_draw_bar_on_icon (cairo_t *pIconContext, double fValue, Icon *pIcon, CairoContainer *pContainer);

void cairo_dock_set_icon_surface_with_bar (cairo_t *pIconContext, cairo_surface_t *pSurface, double fValue, Icon *pIcon, CairoContainer *pContainer);


/**
*Cree les surfaces de reflection d'une icone.
*@param pIconContext le contexte de dessin lie a la surface de l'icone; n'est pas altere par la fonction.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*/
void cairo_dock_add_reflection_to_icon (cairo_t *pIconContext, Icon *pIcon, CairoContainer *pContainer);
/**
*Applique une surface sur le contexte d'une icone, en effacant tout au prealable et en creant les reflets correspondant.
*@param pIconContext le contexte de dessin lie a la surface de l'icone; est modifie par la fonction.
*@param pSurface la surface a appliquer a l'icone.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*/
void cairo_dock_set_icon_surface_with_reflect (cairo_t *pIconContext, cairo_surface_t *pSurface, Icon *pIcon, CairoContainer *pContainer);
/**
*Applique une image sur le contexte d'une icone, en effacant tout au prealable et en creant les reflets correspondant.
*@param pIconContext le contexte de dessin lie a la surface de l'icone; est modifie par la fonction.
*@param cImagePath chemin de l'image a appliquer a l'icone.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*/
void cairo_dock_set_image_on_icon (cairo_t *pIconContext, gchar *cImagePath, Icon *pIcon, CairoContainer *pContainer);

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

void cairo_dock_set_hours_minutes_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds);
void cairo_dock_set_minutes_secondes_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, int iTimeInSeconds);
void cairo_dock_set_size_as_quick_info (cairo_t *pSourceContext, Icon *pIcon, CairoContainer *pContainer, long long int iSizeInBytes);

typedef enum {
	CAIRO_DOCK_INFO_NONE = 0,
	CAIRO_DOCK_INFO_ON_ICON,
	CAIRO_DOCK_INFO_ON_LABEL,
	CAIRO_DOCK_NB_INFO_DISPLAY
} CairoDockInfoDisplay;


/**
*Liste les themes contenu dans un repertoire, met a jour le fichier de conf avec, et renvoie le chemin correspondant au theme choisi.
*@param pKeyFile le fichier de conf ouvert.
*@param cGroupName nom du groupe (dans le fichier de conf) du parametre correspondant au theme.
*@param cKeyName nom de la cle (dans le fichier de conf) du parametre correspondant au theme.
*@param bFlushConfFileNeeded pointeur sur un booleen mis a TRUE si la cle n'existe pas.
*@param cDefaultThemeName nom du theme par defaut au cas ou le precedent n'existerait pas.
*@param cShareThemesDir chemin vers les themes pre-installes.
*@param cExtraDirName nom du repertoire des themes extras et distants.
*@return Le chemin du repertoire du theme choisi, dans une chaine nouvellement allouee.
*/
gchar *cairo_dock_get_theme_path_for_module (GKeyFile *pKeyFile, gchar *cGroupName, gchar *cKeyName, gboolean *bFlushConfFileNeeded, gchar *cDefaultThemeName, const gchar *cShareThemesDir, const gchar *cExtraDirName);

/**
*Cree un sous-menu, et l'ajoute a un menu deja existant.
*@param cLabel nom du sous-menu, tel qu'il apparaitra dans le menu.
*@param pMenu menu auquel on rajoutera le sous-menu.
*@return le sous-menu cree et ajoute au menu.
*/
GtkWidget *cairo_dock_create_sub_menu (gchar *cLabel, GtkWidget *pMenu);


typedef enum {
	CAIRO_DOCK_FREQUENCY_NORMAL = 0,
	CAIRO_DOCK_FREQUENCY_LOW,
	CAIRO_DOCK_FREQUENCY_VERY_LOW,
	CAIRO_DOCK_FREQUENCY_SLEEP,
	CAIRO_DOCK_NB_FREQUENCIES
} CairoDockFrequencyState;

typedef void (* CairoDockAquisitionTimerFunc ) (gpointer data);
typedef void (* CairoDockReadTimerFunc ) (gpointer data);
typedef gboolean (* CairoDockUpdateTimerFunc ) (gpointer data);
typedef struct {
	/// Sid du timer des mesures.
	gint iSidTimer;
	/// Sid du timer de fin de mesure.
	gint iSidTimerRedraw;
	/// Valeur atomique a 1 ssi le thread de mesure est en cours.
	gint iThreadIsRunning;
	/// mutex d'accessibilite a la structure des resultats.
	GMutex *pMutexData;
	/// fonction realisant l'acquisition des donnees. N'accede jamais a la structure des resultats.
	CairoDockAquisitionTimerFunc acquisition;
	/// fonction realisant la lecture des donnees precedemment acquises; stocke les resultats dans la structures des resultats.
	CairoDockReadTimerFunc read;
	/// fonction realisant la mise a jour de l'IHM en fonction des nouveaux resultats. Renvoie TRUE pour continuer, FALSE pour arreter.
	CairoDockUpdateTimerFunc update;
	/// intervalle de temps en secondes, eventuellement nul pour une mesure unitaire.
	gint iCheckInterval;
	/// etat de la frequence des mesures.
	CairoDockFrequencyState iFrequencyState;
	/// donnees passees en entree de chaque fonction.
	gpointer pUserData;
} CairoDockMeasure;

/**
*Lance les mesures periodiques, prealablement preparee avec #cairo_dock_new_measure_timer. La 1ere iteration est executee immediatement. L'acquisition et la lecture des donnees est faite de maniere asynchrone (dans un thread secondaire), alors que le chargement des mesures se fait dans la boucle principale. La frequence est remise a son etat normal.
*@param pMeasureTimer la mesure periodique.
*/
void cairo_dock_launch_measure (CairoDockMeasure *pMeasureTimer);
/**
*Idem que ci-dessus mais après un délai.
*@param pMeasureTimer la mesure periodique.
*@param fDelay délai en ms.
*/
void cairo_dock_launch_measure_delayed (CairoDockMeasure *pMeasureTimer, double fDelay);
/**
*Cree une mesure periodique.
*@param iCheckInterval l'intervalle en s entre 2 mesures, eventuellement nul pour une mesure unitaire.
*@param acquisition fonction realisant l'acquisition des donnees. N'accede jamais a la structure des resultats.
*@param read fonction realisant la lecture des donnees precedemment acquises; stocke les resultats dans la structures des resultats.
*@param update fonction realisant la mise a jour de l'interface en fonction des nouveaux resultats, lus dans la structures des resultats.
*@return la mesure nouvellement allouee. A liberer avec #cairo_dock_free_measure_timer.
*/
CairoDockMeasure *cairo_dock_new_measure_timer (int iCheckInterval, CairoDockAquisitionTimerFunc acquisition, CairoDockReadTimerFunc read, CairoDockUpdateTimerFunc update, gpointer pUserData);
/**
*Stoppe les mesures. Si une mesure est en cours, le thread d'acquisition/lecture se terminera tout seul plus tard, et la mesure sera ignoree. On peut reprendre les mesures par un simple #cairo_dock_launch_measure. Ne doit _pas_ etre appelée durant la fonction 'read' ou 'update'; utiliser la sortie de 'update' pour cela.
*@param pMeasureTimer la mesure periodique.
*/
void cairo_dock_stop_measure_timer (CairoDockMeasure *pMeasureTimer);
/**
*Stoppe et detruit une mesure periodique, liberant toutes ses ressources allouees.
*@param pMeasureTimer la mesure periodique.
*/
void cairo_dock_free_measure_timer (CairoDockMeasure *pMeasureTimer);
/**
*Dis si une mesure est active, c'est a dire si elle est appelee periodiquement.
*@param pMeasureTimer la mesure periodique.
*@return TRUE ssi la mesure est active.
*/
gboolean cairo_dock_measure_is_active (CairoDockMeasure *pMeasureTimer);
/**
*Change la frequence des mesures. La prochaine mesure aura lien dans 1 iteration si elle etait deja active.
*@param pMeasureTimer la mesure periodique.
*@param iNewCheckInterval le nouvel intervalle entre 2 mesures, en s.
*/
void cairo_dock_change_measure_frequency (CairoDockMeasure *pMeasureTimer, int iNewCheckInterval);
/**
*Change la frequence des mesures et les relance immediatement. La prochaine mesure est donc tout de suite.
*@param pMeasureTimer la mesure periodique.
*@param iNewCheckInterval le nouvel intervalle entre 2 mesures, en s.
*/
void cairo_dock_relaunch_measure_immediately (CairoDockMeasure *pMeasureTimer, int iNewCheckInterval);

/**
*Degrade la frequence des mesures. La mesure passe dans un etat moins actif (typiquement utile si la mesure a echouee).
*@param pMeasureTimer la mesure periodique.
*/
void cairo_dock_downgrade_frequency_state (CairoDockMeasure *pMeasureTimer);
/**
*Remet la frequence des mesures a un etat normal. Notez que cela est fait automatiquement au 1er lancement de la mesure.
*@param pMeasureTimer la mesure periodique.
*/
void cairo_dock_set_normal_frequency_state (CairoDockMeasure *pMeasureTimer);


/**
*Joue un son par l'intermédiaire de pulseaudio ou alsa (en priorité).
*@param cSoundPath le chemin vers le fichier audio a jouer.
*/
void cairo_dock_play_sound (const gchar *cSoundPath);


/**
*Renvoie la version de gnome.
*@param iMajor adresse de la majeure.
*@param iMinor adresse de la mineure.
*@param iMicro adresse de la micro.
*/
void cairo_dock_get_gnome_version (int *iMajor, int *iMinor, int *iMicro);


typedef struct _AppletConfig AppletConfig;
typedef struct _AppletData AppletData;

//\_________________________________ INIT
/**
*Definition des fonctions d'initialisation de l'applet; a inclure dans le .h du fichier d'init de l'applet.
*/
#define CD_APPLET_H \
gboolean pre_init (CairoDockVisitCard *pVisitCard, CairoDockModuleInterface *pInterface); \
void init (CairoDockModuleInstance *myApplet, GKeyFile *pKeyFile); \
void stop (CairoDockModuleInstance *myApplet); \
gboolean reload (CairoDockModuleInstance *myApplet, CairoContainer *pOldContainer, GKeyFile *pKeyFile);

//\______________________ pre_init.
/**
*Debut de la fonction de pre-initialisation de l'applet (celle qui est appele a l'enregistrement de tous les plug-ins).
*Definit egalement les variables globales suivantes : myIcon, myDock, myDesklet, myContainer, et myDrawContext.
*@param cName nom de sous lequel l'applet sera enregistree par Cairo-Dock.
*@param iMajorVersion version majeure du dock necessaire au bon fonctionnement de l'applet.
*@param iMinorVersion version mineure du dock necessaire au bon fonctionnement de l'applet.
*@param iMicroVersion version micro du dock necessaire au bon fonctionnement de l'applet.
*@param iAppletCategory Catégorie de l'applet (CAIRO_DOCK_CATEGORY_ACCESSORY, CAIRO_DOCK_CATEGORY_DESKTOP, CAIRO_DOCK_CATEGORY_CONTROLER)
*/
#define CD_APPLET_PRE_INIT_ALL_BEGIN(cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory) \
gboolean pre_init (CairoDockVisitCard *pVisitCard, CairoDockModuleInterface *pInterface) \
{ \
	pVisitCard->cModuleName = g_strdup (cName); \
	pVisitCard->cReadmeFilePath = g_strdup_printf ("%s/%s", MY_APPLET_SHARE_DATA_DIR, MY_APPLET_README_FILE); \
	pVisitCard->iMajorVersionNeeded = iMajorVersion; \
	pVisitCard->iMinorVersionNeeded = iMinorVersion; \
	pVisitCard->iMicroVersionNeeded = iMicroVersion; \
	pVisitCard->cPreviewFilePath = g_strdup_printf ("%s/%s", MY_APPLET_SHARE_DATA_DIR, MY_APPLET_PREVIEW_FILE); \
	pVisitCard->cGettextDomain = g_strdup (MY_APPLET_GETTEXT_DOMAIN); \
	pVisitCard->cDockVersionOnCompilation = g_strdup (MY_APPLET_DOCK_VERSION); \
	pVisitCard->cUserDataDir = g_strdup (MY_APPLET_USER_DATA_DIR); \
	pVisitCard->cShareDataDir = g_strdup (MY_APPLET_SHARE_DATA_DIR); \
	pVisitCard->cConfFileName = (MY_APPLET_CONF_FILE != NULL && strcmp (MY_APPLET_CONF_FILE, "none") != 0 ? g_strdup (MY_APPLET_CONF_FILE) : NULL); \
	pVisitCard->cModuleVersion = g_strdup (MY_APPLET_VERSION);\
	pVisitCard->iCategory = iAppletCategory ;\
	pVisitCard->cIconFilePath = g_strdup_printf ("%s/%s", MY_APPLET_SHARE_DATA_DIR, MY_APPLET_ICON_FILE); \
	pVisitCard->iSizeOfConfig = sizeof (AppletConfig);\
	pVisitCard->iSizeOfData = sizeof (AppletData);

#define CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE \
	pInterface->initModule = init;\
	pInterface->stopModule = stop;\
	pInterface->reloadModule = reload;\
	pInterface->reset_config = reset_config;\
	pInterface->reset_data = reset_data;\
	pInterface->read_conf_file = read_conf_file;

/**
*Fin de la fonction de pre-initialisation de l'applet.
*/
#define CD_APPLET_PRE_INIT_END \
	return TRUE ;\
}
/**
*Fonction de pre-initialisation generique. Ne fais que definir l'applet (en appelant les 2 macros precedentes), la plupart du temps cela est suffisant.
*/
#define CD_APPLET_DEFINITION(cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory) \
CD_APPLET_PRE_INIT_BEGIN (cName, iMajorVersion, iMinorVersion, iMicroVersion, iAppletCategory) \
CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE \
CD_APPLET_PRE_INIT_END

#define CD_APPLET_CAN_DETACH TRUE

//\______________________ init.
/**
*Debut de la fonction d'initialisation de l'applet (celle qui est appelee a chaque chargement de l'applet).
*Lis le fichier de conf de l'applet, et cree son icone ainsi que son contexte de dessin.
*@param erreur une GError, utilisable pour reporter une erreur ayant lieu durant l'initialisation.
*/
#define CD_APPLET_INIT_ALL_BEGIN(pApplet) \
void init (CairoDockModuleInstance *pApplet, GKeyFile *pKeyFile) \
{ \
	cd_message ("%s (%s)", __func__, pApplet->cConfFilePath);

/**
*Fin de la fonction d'initialisation de l'applet.
*/
#define CD_APPLET_INIT_END \
}

//\______________________ stop.
/**
*Debut de la fonction d'arret de l'applet.
*/
#define CD_APPLET_STOP_BEGIN \
void stop (CairoDockModuleInstance *myApplet) \
{

/**
*Fin de la fonction d'arret de l'applet.
*/
#define CD_APPLET_STOP_END \
	cairo_dock_release_data_slot (myApplet); \
}

//\______________________ reload.
/**
*Debut de la fonction de rechargement de l'applet.
*/
#define CD_APPLET_RELOAD_ALL_BEGIN \
gboolean reload (CairoDockModuleInstance *myApplet, CairoContainer *pOldContainer, GKeyFile *pKeyFile) \
{ \
	cd_message ("%s (%s)\n", __func__, myApplet->cConfFilePath);

/**
*Fin de la fonction de rechargement de l'applet.
*/
#define CD_APPLET_RELOAD_END \
	return TRUE; \
}

/**
*TRUE ssi le fichier de conf de l'applet a change juste avant le reload.
*/
#define CD_APPLET_MY_CONFIG_CHANGED (pKeyFile != NULL)

/**
*TRUE ssi le type de container a change.
*/
#define CD_APPLET_MY_CONTAINER_TYPE_CHANGED (myApplet->pContainer == NULL || myApplet->pContainer->iType != pOldContainer->iType)

/**
*Le conteneur precedent le reload.
*/
#define CD_APPLET_MY_OLD_CONTAINER pOldContainer;


/**
*Chemin du fichier de conf de l'applet, appelable durant les fonctions d'init, de config, et de reload.
*/
#define CD_APPLET_MY_CONF_FILE myApplet->cConfFilePath
/**
*Fichier de cles de l'applet, appelable durant les fonctions d'init, de config, et de reload.
*/
#define CD_APPLET_MY_KEY_FILE pKeyFile


//\_________________________________ CONFIG
//\______________________ read_conf_file.
/**
*Debut de la fonction de configuration de l'applet (celle qui est appelee au debt de l'init).
*/
#define CD_APPLET_GET_CONFIG_ALL_BEGIN \
gboolean read_conf_file (CairoDockModuleInstance *myApplet, GKeyFile *pKeyFile) \
{ \
	gboolean bFlushConfFileNeeded = FALSE;

/**
*Fin de la fonction de configuration de l'applet.
*/
#define CD_APPLET_GET_CONFIG_END \
	return bFlushConfFileNeeded; \
}


/**
*Definition de la fonction de configuration, a inclure dans le .h correspondant.
*/
#define CD_APPLET_CONFIG_H \
gboolean read_conf_file (CairoDockModuleInstance *myApplet, GKeyFile *pKeyFile); \
void reset_config (CairoDockModuleInstance *myApplet); \
void reset_data (CairoDockModuleInstance *myApplet);


/**
*Recupere la valeur d'un parametre 'booleen' du fichier de conf.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param bDefaultValue valeur par defaut si la cle et/ou le groupe n'est pas trouve (typiquement si cette cle est nouvelle).
*@return un gboolean.
*/
#define CD_CONFIG_GET_BOOLEAN_WITH_DEFAULT(cGroupName, cKeyName, bDefaultValue) cairo_dock_get_boolean_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, bDefaultValue, NULL, NULL)

/**
*Recupere la valeur d'un parametre 'booleen' du fichier de conf, avec TRUE comme valeur par defaut.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@return un gboolean.
*/
#define CD_CONFIG_GET_BOOLEAN(cGroupName, cKeyName) CD_CONFIG_GET_BOOLEAN_WITH_DEFAULT (cGroupName, cKeyName, TRUE)

/**
*Recupere la valeur d'un parametre 'entier' du fichier de conf.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param iDefaultValue valeur par defaut si la cle et/ou le groupe n'est pas trouve (typiquement si cette cle est nouvelle).
*@return un entier.
*/
#define CD_CONFIG_GET_INTEGER_WITH_DEFAULT(cGroupName, cKeyName, iDefaultValue) cairo_dock_get_integer_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, iDefaultValue, NULL, NULL)
/**
*Recupere la valeur d'un parametre 'entier' du fichier de conf, avec 0 comme valeur par defaut.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@return un entier.
*/
#define CD_CONFIG_GET_INTEGER(cGroupName, cKeyName) CD_CONFIG_GET_INTEGER_WITH_DEFAULT (cGroupName, cKeyName, 0)

/**
*Recupere la valeur d'un parametre 'double' du fichier de conf.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param fDefaultValue valeur par defaut si la cle et/ou le groupe n'est pas trouve (typiquement si cette cle est nouvelle).
*@return un double.
*/
#define CD_CONFIG_GET_DOUBLE_WITH_DEFAULT(cGroupName, cKeyName, fDefaultValue) cairo_dock_get_double_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, 0., NULL, NULL)
/**
*Recupere la valeur d'un parametre 'double' du fichier de conf, avec 0. comme valeur par defaut.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@return un double.
*/
#define CD_CONFIG_GET_DOUBLE(cGroupName, cKeyName) CD_CONFIG_GET_DOUBLE_WITH_DEFAULT (cGroupName, cKeyName, 0.)

#define CD_CONFIG_GET_INTEGER_LIST(cGroupName, cKeyName, iNbElements, iValueBuffer) \
cairo_dock_get_integer_list_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, iValueBuffer, iNbElements, NULL, NULL, NULL)

/**
*Recupere la valeur d'un parametre 'chaine de caracteres' du fichier de conf.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param cDefaultValue valeur par defaut si la cle et/ou le groupe n'est pas trouve (typiquement si cette cle est nouvelle). NULL accepte.
*@return une chaine de caracteres nouvellement allouee.
*/
#define CD_CONFIG_GET_STRING_WITH_DEFAULT(cGroupName, cKeyName, cDefaultValue) cairo_dock_get_string_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, cDefaultValue, NULL, NULL)
/**
*Recupere la valeur d'un parametre 'chaine de caracteres' du fichier de conf, avec NULL comme valeur par defaut.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@return une chaine de caracteres nouvellement allouee.
*/
#define CD_CONFIG_GET_STRING(cGroupName, cKeyName) CD_CONFIG_GET_STRING_WITH_DEFAULT (cGroupName, cKeyName, NULL)

/**
*Recupere la valeur d'un parametre 'fichier' du fichier de conf, avec NULL comme valeur par defaut. Si le parametre est NULL, un fichier local a l'applet est utilise, mais le fichier de conf n'est pas renseigné avec.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param cDefaultFileName fichier par defaut si aucun n'est specifie dans la conf.
*@return une chaine de caracteres nouvellement allouee donnant le chemin complet du fichier.
*/
#define CD_CONFIG_GET_FILE_PATH(cGroupName, cKeyName, cDefaultFileName) cairo_dock_get_file_path_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, NULL, NULL, MY_APPLET_SHARE_DATA_DIR, cDefaultFileName)

/**
*Recupere la valeur d'un parametre 'liste de chaines de caracteres' du fichier de conf.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param length pointeur sur un entier, rempli avec le nombre de chaines recuperees.
*@param cDefaultValues valeur par defaut si la cle et/ou le groupe n'est pas trouve (typiquement si cette cle est nouvelle). C'est une chaine de caractere contenant les mots separes par des ';', ou NULL.
*@return un tableau de chaine de caracteres, a liberer avec 'g_strfreev'.
*/
#define CD_CONFIG_GET_STRING_LIST_WITH_DEFAULT(cGroupName, cKeyName, length, cDefaultValues) cairo_dock_get_string_list_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, length, cDefaultValues, NULL, NULL)
/**
*Recupere la valeur d'un parametre 'liste de chaines de caracteres' du fichier de conf, avec NULL comme valeur par defaut.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param length pointeur sur un entier, rempli avec le nombre de chaines recuperees.
*@return un tableau de chaine de caracteres, a liberer avec 'g_strfreev'.
*/
#define CD_CONFIG_GET_STRING_LIST(cGroupName, cKeyName, length) CD_CONFIG_GET_STRING_LIST_WITH_DEFAULT(cGroupName, cKeyName, length, NULL)

/**
*Recupere la valeur d'un parametre 'type d'animation' du fichier de conf.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param iDefaultAnimation valeur par defaut si la cle et/ou le groupe n'est pas trouve (typiquement si cette cle est nouvelle).
*@return le type de l'animation, un #CairoDockAnimationType.
*/
#define CD_CONFIG_GET_ANIMATION_WITH_DEFAULT(cGroupName, cKeyName, iDefaultAnimation) cairo_dock_get_animation_type_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, iDefaultAnimation, NULL, NULL)
/**
*Recupere la valeur d'un parametre 'type d'animation' du fichier de conf, avec #CAIRO_DOCK_BOUNCE comme valeur par defaut.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@return le type de l'animation, un #CairoDockAnimationType.
*/
#define CD_CONFIG_GET_ANIMATION(cGroupName, cKeyName) CD_CONFIG_GET_ANIMATION_WITH_DEFAULT(cGroupName, cKeyName, CAIRO_DOCK_BOUNCE)

/**
*Recupere la valeur d'un parametre 'couleur' au format RVBA. du fichier de conf.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param pColorBuffer tableau de 4 double deja alloue, et qui sera rempli avec les 4 composantes de la couleur.
*@param pDefaultColor valeur par defaut si la cle et/ou le groupe n'est pas trouve (typiquement si cette cle est nouvelle). C'est un tableau de 4 double, ou NULL.
*/
#define CD_CONFIG_GET_COLOR_WITH_DEFAULT(cGroupName, cKeyName, pColorBuffer, pDefaultColor) cairo_dock_get_double_list_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, pColorBuffer, 4, pDefaultColor, NULL, NULL)
/**
*Recupere la valeur d'un parametre 'couleur' au format RVBA. du fichier de conf, avec NULL comme valeur par defaut.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param pColorBuffer tableau de 4 double deja alloue, et qui sera rempli avec les 4 composantes de la couleur.
*/
#define CD_CONFIG_GET_COLOR(cGroupName, cKeyName, pColorBuffer) CD_CONFIG_GET_COLOR_WITH_DEFAULT(cGroupName, cKeyName, pColorBuffer, NULL)
/**
*Recupere la valeur d'un parametre 'couleur' au format RVB. du fichier de conf.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param pColorBuffer tableau de 3 double deja alloue, et qui sera rempli avec les 3 composantes de la couleur.
*@param pDefaultColor valeur par defaut si la cle et/ou le groupe n'est pas trouve (typiquement si cette cle est nouvelle). C'est un tableau de 3 double, ou NULL.
*/
#define CD_CONFIG_GET_COLOR_RVB_WITH_DEFAULT(cGroupName, cKeyName, pColorBuffer, pDefaultColor) cairo_dock_get_double_list_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, pColorBuffer, 3, pDefaultColor, NULL, NULL)
/**
*Recupere la valeur d'un parametre 'couleur' au format RVB. du fichier de conf, avec NULL comme valeur par defaut.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param pColorBuffer tableau de 3 double deja alloue, et qui sera rempli avec les 3 composantes de la couleur.
*/
#define CD_CONFIG_GET_COLOR_RVB(cGroupName, cKeyName, pColorBuffer) CD_CONFIG_GET_COLOR_RVB_WITH_DEFAULT(cGroupName, cKeyName, pColorBuffer, NULL)

/**
*Liste les themes contenu dans un repertoire, met a jour le fichier de conf avec, et renvoie le chemin correspondant au theme choisi.
*@param cGroupName nom du groupe (dans le fichier de conf) du parametre correspondant au theme.
*@param cKeyName nom de la cle (dans le fichier de conf) du parametre correspondant au theme.
*@param cThemesDirName nom du sous-repertoire regroupant tous les themes.
*@param cDefaultThemeName valeur par defaut si la cle et/ou le groupe et/ou le theme n'existe(nt) pas.
*@return Le chemin vers le repertoire du theme, dans une chaine nouvellement allouee.
*/
#define CD_CONFIG_GET_THEME_PATH(cGroupName, cKeyName, cThemeDirName, cDefaultThemeName) \
cairo_dock_get_theme_path_for_module (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, cDefaultThemeName, MY_APPLET_SHARE_DATA_DIR"/"cThemeDirName, MY_APPLET_USER_DATA_DIR)

/**
*Recupere la valeur d'un theme de gauge, en mettant a jour la liste des jauges disponibles dans le fichier de conf.
*@param cGroupName nom du groupe (dans le fichier de conf) du parametre correspondant au theme.
*@param cKeyName nom de la cle (dans le fichier de conf) du parametre correspondant au theme.
*/
#define CD_CONFIG_GET_GAUGE_THEME(cGroupName, cKeyName) \
cairo_dock_get_gauge_key_value(CD_APPLET_MY_CONF_FILE, pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, "turbo-night-fuel")

/**
*Debut de la fonction de liberation des donnees de la config.
*/
#define CD_APPLET_RESET_CONFIG_ALL_BEGIN \
void reset_config (CairoDockModuleInstance *myApplet) \
{
/**
*Fin de la fonction de liberation des donnees de la config.
*/
#define CD_APPLET_RESET_CONFIG_END \
}

/**
*Debut de la fonction de liberation des donnees internes.
*/
#define CD_APPLET_RESET_DATA_BEGIN \
void reset_data (CairoDockModuleInstance *myApplet) \
{
/**
*Fin de la fonction de liberation des donnees internes.
*/
#define CD_APPLET_RESET_DATA_ALL_END \
}

//\_________________________________ NOTIFICATIONS
//\______________________ fonction about.
/**
*Fonction 'A propos' toute faite, qui affiche un message dans une info-bulle. A inclure dans le .c.
*@param cMessage message a afficher dans l'info-bulle.
*/
#define CD_APPLET_ABOUT(cMessage) \
void about (GtkMenuItem *menu_item, CairoDockModuleInstance *myApplet) \
{ \
	cairo_dock_show_temporary_dialog (cMessage, myIcon, myContainer, 0); \
}
/**
*Definition de la fonction precedente; a inclure dans le .h correspondant.
*/
#define CD_APPLET_ABOUT_H \
void about (GtkMenuItem *menu_item, CairoDockModuleInstance *myApplet);

//\______________________ notification clique gauche.
#define CD_APPLET_ON_CLICK action_on_click
/**
*Abonne l'applet aux notifications du clic gauche. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_CLICK_EVENT cairo_dock_register_notification (CAIRO_DOCK_CLICK_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_CLICK, CAIRO_DOCK_RUN_AFTER, myApplet);
/**
*Desabonne l'applet aux notifications du clic gauche. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_CLICK_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_CLICK_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_CLICK, myApplet);

/**
*Debut de la fonction de notification au clic gauche.
*/
#define CD_APPLET_ON_CLICK_BEGIN \
gboolean CD_APPLET_ON_CLICK (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer, guint iButtonState) \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{

/**
*Fin de la fonction de notification au clic gauche. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_CLICK_END \
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION; \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}
/**
*Definition de la fonction precedente; a inclure dans le .h correspondant.
*/
#define CD_APPLET_ON_CLICK_H \
gboolean CD_APPLET_ON_CLICK (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer, guint iButtonState);

//\______________________ notification construction menu.
#define CD_APPLET_ON_BUILD_MENU applet_on_build_menu
/**
*Abonne l'applet aux notifications de construction du menu. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_BUILD_MENU_EVENT cairo_dock_register_notification (CAIRO_DOCK_BUILD_MENU, (CairoDockNotificationFunc) CD_APPLET_ON_BUILD_MENU, CAIRO_DOCK_RUN_FIRST, myApplet);
/**
*Desabonne l'applet aux notifications de construction du menu. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_BUILD_MENU_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_BUILD_MENU, (CairoDockNotificationFunc) CD_APPLET_ON_BUILD_MENU, myApplet);

/**
*Debut de la fonction de notification de construction du menu.
*/
#define CD_APPLET_ON_BUILD_MENU_BEGIN \
gboolean CD_APPLET_ON_BUILD_MENU (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer, GtkWidget *pAppletMenu) \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{ \
		GtkWidget *pMenuItem, *image; \
		pMenuItem = gtk_separator_menu_item_new (); \
		gtk_menu_shell_append(GTK_MENU_SHELL (pAppletMenu), pMenuItem);

/**
*Fin de la fonction de notification de construction du menu. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_BUILD_MENU_END \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}
/**
*Definition de la fonction precedente; a inclure dans le .h correspondant.
*/
#define CD_APPLET_ON_BUILD_MENU_H \
gboolean CD_APPLET_ON_BUILD_MENU (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer, GtkWidget *pAppletMenu);

/**
*Menu principal de l'applet.
*/
#define CD_APPLET_MY_MENU pAppletMenu

/**
*Icone cliquee.
*/
#define CD_APPLET_CLICKED_ICON pClickedIcon
/**
*Container clique.
*/
#define CD_APPLET_CLICKED_CONTAINER pClickedContainer

/**
* La touche 'SHIFT' est-elle enfoncee au moment du clic ?
*/
#define CD_APPLET_SHIFT_CLICK (iButtonState & GDK_SHIFT_MASK)
/**
* La touche 'CTRL' est-elle enfoncee au moment du clic ?
*/
#define CD_APPLET_CTRL_CLICK (iButtonState & GDK_CONTROL_MASK)
/**
* La touche 'ALT' est-elle enfoncee au moment du clic ?
*/
#define CD_APPLET_ALT_CLICK (iButtonState & GDK_MOD1_MASK)


/**
*Cree et ajoute un sous-menu a un menu.
*@param cLabel nom du sous-menu, tel qu'il apparaitra dans le menu.
*@param pSubMenu GtkWidget du sous-menu; il doit juste avoir ete declare, il sera cree par la macro.
*@param pMenu GtkWidget du menu auquel on rajoutera le sous-menu.
*/
#define CD_APPLET_ADD_SUB_MENU(cLabel, pSubMenu, pMenu) \
	GtkWidget *pSubMenu = gtk_menu_new (); \
	pMenuItem = gtk_menu_item_new_with_label (cLabel); \
	gtk_menu_shell_append  (GTK_MENU_SHELL (pMenu), pMenuItem); \
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), pSubMenu)

/**
*Cree et ajoute un sous-menu a un menu deja existant.
*@param cLabel nom du sous-menu, tel qu'il apparaitra dans le menu.
*@param pMenu GtkWidget du menu auquel on rajoutera le sous-menu.
*@return le GtkWidget du sous-menu.
*/
#define CD_APPLET_CREATE_AND_ADD_SUB_MENU(cLabel, pMenu) cairo_dock_create_sub_menu (cLabel, pMenu)

/**
*Cree et ajoute un sous-menu a un menu.
*@param cLabel nom du sous-menu, tel qu'il apparaitra dans le menu.
*@param pFunction fonction appelee lors de la selection de cette entree.
*@param pMenu GtkWidget du menu auquel on rajoutera le sous-menu.
*@param pData donnees passees en parametre de la fonction (doit contenir myApplet).
*/
#define CD_APPLET_ADD_IN_MENU_WITH_DATA(cLabel, pFunction, pMenu, pData) do { \
	pMenuItem = gtk_menu_item_new_with_label (cLabel); \
	gtk_menu_shell_append  (GTK_MENU_SHELL (pMenu), pMenuItem); \
	g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK (pFunction), pData); } while (0)

/**
*Ajoute une entree a un menu deja existant.
*@param cLabel nom de l'entree, tel qu'il apparaitra dans le menu.
*@param pFunction fonction appelee lors de la selection de cette entree.
*@param pMenu GtkWidget du menu auquel on rajoutera l'entree.
*/
#define CD_APPLET_ADD_IN_MENU(cLabel, pFunction, pMenu) CD_APPLET_ADD_IN_MENU_WITH_DATA(cLabel, pFunction, pMenu, myApplet)

/**
*Ajoute une entree avec une icone GTK a un menu deja existant.
*@param cLabel nom de l'entree, tel qu'il apparaitra dans le menu.
*@param gtkStock nom d'une icone de GTK.
*@param pFunction fonction appelee lors de la selection de cette entree.
*@param pMenu GtkWidget du menu auquel on rajoutera l'entree.
*@param pData donnees passees en parametre de la fonction (doit contenir myApplet).
*/
#define CD_APPLET_ADD_IN_MENU_WITH_STOCK_AND_DATA(cLabel, gtkStock, pFunction, pMenu, pData) do { \
	pMenuItem = gtk_image_menu_item_new_with_label (cLabel); \
	image = gtk_image_new_from_stock (gtkStock, GTK_ICON_SIZE_MENU); \
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image); \
	gtk_menu_shell_append  (GTK_MENU_SHELL (pMenu), pMenuItem); \
	g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK(pFunction), pData); } while (0)

/**
*Ajoute une entree avec une icone GTK a un menu deja existant.
*@param cLabel nom de l'entree, tel qu'il apparaitra dans le menu.
*@param pFunction fonction appelee lors de la selection de cette entree.
*@param pMenu GtkWidget du menu auquel on rajoutera l'entree.
*/
#define CD_APPLET_ADD_IN_MENU_WITH_STOCK(cLabel, gtkStock, pFunction, pMenu) CD_APPLET_ADD_IN_MENU_WITH_STOCK_AND_DATA(cLabel, gtkStock, pFunction, pMenu, myApplet)

/**
 * Ajoute un separateur dans un menu deja existant
 */
#define CD_APPLET_ADD_SEPARATOR(pMenu) do { \
	pMenuItem = gtk_separator_menu_item_new (); \
	gtk_menu_shell_append(GTK_MENU_SHELL (pMenu), pMenuItem); } while (0)

/**
*Ajoute une entree pour la fonction 'A propos'.
*@param pMenu GtkWidget du menu auquel sera ajoutee l'entree.
*/
#define CD_APPLET_ADD_ABOUT_IN_MENU(pMenu) CD_APPLET_ADD_IN_MENU_WITH_STOCK (_("About"), GTK_STOCK_ABOUT, about, pMenu)

/**
*Recupere la derniere entree ajoutee dans la fonction.
*@return le GtkWidget de la derniere entree.
*/
#define CD_APPLET_LAST_ITEM_IN_MENU pMenuItem

//\______________________ notification clique milieu.
#define CD_APPLET_ON_MIDDLE_CLICK action_on_middle_click
/**
*Abonne l'applet aux notifications du clic du milieu. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_MIDDLE_CLICK_EVENT cairo_dock_register_notification (CAIRO_DOCK_MIDDLE_CLICK_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_MIDDLE_CLICK, CAIRO_DOCK_RUN_AFTER, myApplet)
/**
*Desabonne l'applet aux notifications du clic du milieu. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_MIDDLE_CLICK_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_MIDDLE_CLICK_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_MIDDLE_CLICK, myApplet)

/**
*Debut de la fonction de notification du clic du milieu.
*/
#define CD_APPLET_ON_MIDDLE_CLICK_BEGIN \
gboolean CD_APPLET_ON_MIDDLE_CLICK (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer) \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{
/**
*Fin de la fonction de notification du clic du milieu. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_MIDDLE_CLICK_END \
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION; \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}
/**
*Definition de la fonction precedente; a inclure dans le .h correspondant.
*/
#define CD_APPLET_ON_MIDDLE_CLICK_H \
gboolean CD_APPLET_ON_MIDDLE_CLICK (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer);

//\______________________ notification drag'n'drop.
#define CD_APPLET_ON_DROP_DATA action_on_drop_data
/**
*Abonne l'applet aux notifications du glisse-depose. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_DROP_DATA_EVENT cairo_dock_register_notification (CAIRO_DOCK_DROP_DATA, (CairoDockNotificationFunc) CD_APPLET_ON_DROP_DATA, CAIRO_DOCK_RUN_FIRST, myApplet);
/**
*Desabonne l'applet aux notifications du glisse-depose. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_DROP_DATA_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_DROP_DATA, (CairoDockNotificationFunc) CD_APPLET_ON_DROP_DATA, myApplet);

/**
*Debut de la fonction de notification du glisse-depose.
*/
#define CD_APPLET_ON_DROP_DATA_BEGIN \
gboolean CD_APPLET_ON_DROP_DATA (CairoDockModuleInstance *myApplet, const gchar *cReceivedData, Icon *pClickedIcon, double fPosition, CairoContainer *pClickedContainer) \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{ \
		g_return_val_if_fail (cReceivedData != NULL, CAIRO_DOCK_LET_PASS_NOTIFICATION);

/**
*Donnees recues (chaine de caracteres).
*/
#define CD_APPLET_RECEIVED_DATA cReceivedData

/**
*Fin de la fonction de notification du glisse-depose. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_DROP_DATA_END \
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION; \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}
/**
*Definition de la fonction precedente; a inclure dans le .h correspondant.
*/
#define CD_APPLET_ON_DROP_DATA_H \
gboolean CD_APPLET_ON_DROP_DATA (CairoDockModuleInstance *myApplet, const gchar *cReceivedData, Icon *pClickedIcon, double fPosition, CairoContainer *pClickedContainer);

//\______________________ notification de scroll molette.
#define CD_APPLET_ON_SCROLL action_on_scroll
/**
*Abonne l'applet aux notifications du clic gauche. A effectuer lors de l'init de l'applet.
*/
#define CD_APPLET_REGISTER_FOR_SCROLL_EVENT cairo_dock_register_notification (CAIRO_DOCK_SCROLL_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_SCROLL, CAIRO_DOCK_RUN_FIRST, myApplet)
/**
*Desabonne l'applet aux notifications du clic gauche. A effectuer lors de l'arret de l'applet.
*/
#define CD_APPLET_UNREGISTER_FOR_SCROLL_EVENT cairo_dock_remove_notification_func (CAIRO_DOCK_SCROLL_ICON, (CairoDockNotificationFunc) CD_APPLET_ON_SCROLL, myApplet)

/**
*Debut de la fonction de notification au clic gauche.
*/
#define CD_APPLET_ON_SCROLL_BEGIN \
gboolean CD_APPLET_ON_SCROLL (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer, int iDirection) \
{ \
	if (pClickedIcon == myIcon || (myIcon != NULL && pClickedContainer == CAIRO_CONTAINER (myIcon->pSubDock)) || pClickedContainer == CAIRO_CONTAINER (myDesklet)) \
	{

/**
*Fin de la fonction de notification au clic gauche. Par defaut elle intercepte la notification si elle l'a recue.
*/
#define CD_APPLET_ON_SCROLL_END \
		return CAIRO_DOCK_INTERCEPT_NOTIFICATION; \
	} \
	return CAIRO_DOCK_LET_PASS_NOTIFICATION; \
}
/**
*Definition de la fonction precedente; a inclure dans le .h correspondant.
*/
#define CD_APPLET_ON_SCROLL_H \
gboolean CD_APPLET_ON_SCROLL (CairoDockModuleInstance *myApplet, Icon *pClickedIcon, CairoContainer *pClickedContainer, int iDirection);

/**
*Direction du scroll.
*/
#define CD_APPLET_SCROLL_DIRECTION iDirection
/**
*Scroll vers le haut.
*/
#define CD_APPLET_SCROLL_UP (CD_APPLET_SCROLL_DIRECTION == GDK_SCROLL_UP)
/**
*Scroll vers le bas.
*/
#define CD_APPLET_SCROLL_DOWN (CD_APPLET_SCROLL_DIRECTION == GDK_SCROLL_DOWN)

//\_________________________________ DESSIN

/**
*Redessine immediatement l'icone de l'applet.
*/
#define CD_APPLET_REDRAW_MY_ICON \
	cairo_dock_redraw_my_icon (myIcon, myContainer)

/**
*Applique une surface existante sur le contexte de dessin de l'applet, et la redessine. La surface est redimensionnee aux dimensions de l'icone.
*@param pSurface la surface cairo a dessiner.
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON(pSurface) do { \
	cairo_dock_set_icon_surface_with_reflect (myDrawContext, pSurface, myIcon, myContainer); \
	cairo_dock_redraw_my_icon (myIcon, myContainer); } while (0)
/**
*Applique une surface existante sur le contexte de dessin de l'applet, et la redessine. La surface est redimensionnee aux dimensions de l'icone, et zoomee.
*@param pSurface la surface cairo a dessiner.
*@param fScale le facteur de zoom (>= 0)
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_ZOOM(pSurface, fScale) do { \
	cairo_dock_set_icon_surface_full (myDrawContext, pSurface, fScale, 1., myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer); \
	cairo_dock_redraw_my_icon (myIcon, myContainer); } while (0)
/**
*Applique une surface existante sur le contexte de dessin de l'applet, et la redessine. La surface est redimensionnee aux dimensions de l'icone, et avec un facteur de transparence.
*@param pSurface la surface cairo a dessiner.
*@param fAlpha la transparence (dans [0 , 1]).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_ALPHA(pSurface, fAlpha) do { \
	cairo_dock_set_icon_surface_full (myDrawContext, pSurface, 1., fAlpha, myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer); \
	cairo_dock_redraw_my_icon (myIcon, myContainer); } while (0)
/**
*Applique une surface existante sur le contexte de dessin de l'applet, et la redessine. La surface est redimensionnee aux dimensions de l'icone, et une barre est dessinee a sa base.
*@param pSurface la surface cairo a dessiner.
*@param fValue la valeur en fraction de la valeur max (donc dans [0 , 1]).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_BAR(pSurface, fValue) do { \
	cairo_dock_set_icon_surface_with_bar (myDrawContext, pSurface, fValue, myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer); \
	cairo_dock_redraw_my_icon (myIcon, myContainer); } while (0)

/**
*Applique une image definie par son chemin sur le contexte de dessin de l'applet, mais ne la rafraichit pas. L'image est redimensionnee aux dimensions de l'icone.
*@param cImagePath chemin du fichier de l'image.
*/
#define CD_APPLET_SET_IMAGE_ON_MY_ICON(cImagePath) do { \
	if (cImagePath != myIcon->acFileName) \
	{ \
		g_free (myIcon->acFileName); \
		myIcon->acFileName = g_strdup (cImagePath); \
	} \
	cairo_dock_set_image_on_icon (myDrawContext, cImagePath, myIcon, myContainer); } while (0)

/**
*Idem que precedemment mais l'image est definie par son nom localement au repertoire d'installation de l'applet
*@param cImageName nom du fichier de l'image 
*/
#define CD_APPLET_SET_LOCAL_IMAGE_ON_MY_ICON(cImageName) do { \
		gchar *_cImageFilePath = g_strconcat (MY_APPLET_SHARE_DATA_DIR, "/", cImageName, NULL); \
		CD_APPLET_SET_IMAGE_ON_MY_ICON (_cImageFilePath); \
		g_free (_cImageFilePath); } while (0)

/**
*Idem que precedemment mais l'image est definie relativement au repertoire utilisateur, ou relativement au repertoire d'installation de l'applet si la 1ere est NULL.
*@param cUserImageName nom du fichier de l'image cote utilisateur.
*@param cDefaultLocalImageName image locale par defaut cote installation.
*/
#define CD_APPLET_SET_USER_IMAGE_ON_MY_ICON(cUserImageName, cDefaultLocalImageName) do { \
		gchar *cImagePath; \
		if (cUserImageName != NULL) \
			cImagePath = cairo_dock_generate_file_path (cUserImageName); \
		else \
			cImagePath = g_strdup_printf ("%s/%s", MY_APPLET_SHARE_DATA_DIR, cDefaultLocalImageName); \
		CD_APPLET_SET_IMAGE_ON_MY_ICON (cImagePath); \
		g_free (cImagePath); } while (0)

/**
*Applique une surface existante sur le contexte de dessin de l'applet, et la redessine. La surface est redimensionnee aux dimensions de l'icone.
*@param pSurface
*@param fScale
*/
#define CD_APPLET_SET_ZOOMED_SURFACE_ON_MY_ICON(pSurface, fScale) do { \
	cairo_dock_set_icon_surface_with_zoom (myDrawContext, pSurface, fScale, myIcon, myContainer); \
	cairo_dock_redraw_my_icon (myIcon, myContainer); } while (0)


/**
*Remplace l'etiquette de l'icone de l'applet par une nouvelle.
*@param cIconName la nouvelle etiquette.
*/
#define CD_APPLET_SET_NAME_FOR_MY_ICON(cIconName) \
	cairo_dock_set_icon_name (myDrawContext, cIconName, myIcon, myContainer)
/**
*Remplace l'etiquette de l'icone de l'applet par une nouvelle.
*@param cIconNameFormat la nouvelle etiquette au format 'printf'.
*/
#define CD_APPLET_SET_NAME_FOR_MY_ICON_PRINTF(cIconNameFormat, ...) \
	cairo_dock_set_icon_name_full (myDrawContext, myIcon, myContainer, cIconNameFormat, ##__VA_ARGS__)

/**
*Ecris une info-rapide sur l'icone de l'applet.
*@param cQuickInfo l'info-rapide. Ce doit etre une chaine de caracteres particulièrement petite, representant une info concise, puisque ecrite directement sur l'icone.
*/
#define CD_APPLET_SET_QUICK_INFO_ON_MY_ICON(cQuickInfo) \
	cairo_dock_set_quick_info (myDrawContext, cQuickInfo, myIcon, myDock ? (1 + myIcons.fAmplitude) / 1 : 1)
/**
*Ecris une info-rapide sur l'icone de l'applet.
*@param cQuickInfoFormat l'info-rapide, au format 'printf'. Ce doit etre une chaine de caracteres particulièrement petite, representant une info concise, puisque ecrite directement sur l'icone.
*/
#define CD_APPLET_SET_QUICK_INFO_ON_MY_ICON_PRINTF(cQuickInfoFormat, ...) \
	cairo_dock_set_quick_info_full (myDrawContext, myIcon, myContainer, cQuickInfoFormat, ##__VA_ARGS__)

/**
*Ecris le temps en heures-minutes en info-rapide sur l'icone de l'applet.
*@param iTimeInSeconds le temps en secondes.
*/
#define CD_APPLET_SET_HOURS_MINUTES_AS_QUICK_INFO(iTimeInSeconds) \
	cairo_dock_set_hours_minutes_as_quick_info (myDrawContext, myIcon, myContainer, iTimeInSeconds)
/**
*Ecris le temps en minutes-secondes en info-rapide sur l'icone de l'applet.
*@param iTimeInSeconds le temps en secondes.
*/
#define CD_APPLET_SET_MINUTES_SECONDES_AS_QUICK_INFO(iTimeInSeconds) \
	cairo_dock_set_minutes_secondes_as_quick_info (myDrawContext, myIcon, myContainer, iTimeInSeconds)
/**
*Ecris une taille en octets en info-rapide sur l'icone de l'applet.
*@param iSizeInBytes la taille en octets.
*/
#define CD_APPLET_SET_SIZE_AS_QUICK_INFO(iSizeInBytes) \
	cairo_dock_set_size_as_quick_info (myDrawContext, myIcon, myContainer, iSizeInBytes)

/**
*Empeche l'icone d'etre animee au passage de la souris.
*/
#define CD_APPLET_SET_STATIC_ICON cairo_dock_set_icon_static (myIcon)

/**
*Lance l'animation de l'icone de l'applet.
*@param iAnimationType type de l'animation (un #CairoDockAnimationType).
*@param iAnimationLength duree de l'animation, en nombre de tours.
*/
#define CD_APPLET_ANIMATE_MY_ICON(cAnimationName, iAnimationLength) \
	cairo_dock_request_icon_animation (myIcon, myDock, cAnimationName, iAnimationLength)

/**
*Charge une image dans une surface, aux dimensions de l'icone de l'applet.
*@param cImagePath chemin du fichier de l'image.
*@return la surface nouvellement creee.
*/
#define CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET(cImagePath) \
	cairo_dock_create_surface_for_icon (cImagePath, myDrawContext, myIcon->fWidth * (myDock ? (1 + g_fAmplitude) / myDock->fRatio : 1), myIcon->fHeight* (myDock ? (1 + g_fAmplitude) / myDock->fRatio : 1))


#define CD_APPLET_RENDER_GAUGE(pGauge, fValue) cairo_dock_render_gauge (myDrawContext, myContainer, myIcon, pGauge, fValue)
#define CD_APPLET_RENDER_GAUGE_MULTI_VALUE(pGauge, pValueList) cairo_dock_render_gauge_multi_value (myDrawContext, myContainer, myIcon, pGauge, pValueList)

#define CD_APPLET_RENDER_GRAPH(pGraph) cairo_dock_render_graph (myDrawContext, myContainer, myIcon, pGraph);
#define CD_APPLET_RENDER_GRAPH_NEW_VALUE(pGraph, fValue) do { \
	cairo_dock_update_graph (pGraph, fValue); \
	CD_APPLET_RENDER_GRAPH (pGraph); } while (0)
#define CD_APPLET_RENDER_GRAPH_NEW_VALUES(pGraph, fValue, fValue2) do { \
	cairo_dock_update_double_graph (pGraph, fValue, fValue2); \
	CD_APPLET_RENDER_GRAPH (pGraph); } while (0)


#define CD_APPLET_GET_MY_ICON_DATA(pIcon) cairo_dock_get_icon_data (pIcon, myApplet)
#define CD_APPLET_GET_MY_CONTAINER_DATA(pContainer) cairo_dock_get_container_data (pContainer, myApplet)
#define CD_APPLET_GET_MY_DOCK_DATA(pDock) CD_APPLET_GET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDock))
#define CD_APPLET_GET_MY_DESKLET_DATA(pDesklet) CD_APPLET_GET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDesklet))

#define CD_APPLET_SET_MY_ICON_DATA(pIcon, pData) cairo_dock_set_icon_data (pIcon, myApplet, pData)
#define CD_APPLET_SET_MY_CONTAINER_DATA(pContainer, pData) cairo_dock_set_container_data (pContainer, myApplet, pData)
#define CD_APPLET_SET_MY_DOCK_DATA(pDock, pData) CD_APPLET_SET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDock), pData)
#define CD_APPLET_SET_MY_DESKLET_DATA(pDesklet, pData) CD_APPLET_SET_MY_CONTAINER_DATA (CAIRO_CONTAINER (pDesklet), pData)

#define CD_APPLET_LOAD_LOCAL_TEXTURE(cImageName) cairo_dock_load_local_texture (cImageName, MY_APPLET_SHARE_DATA_DIR)

#define CD_APPLET_MY_CONTAINER_IS_OPENGL (g_bUseOpenGL && ((myDock && myDock->render_opengl) || (myDesklet && myDesklet->pRenderer && myDesklet->pRenderer->render_opengl)))

//\_________________________________ DESKLETS et SOUS-DOCKS

/**
*Definit le moteur de rendu de l'applet en mode desklet et le contexte de dessin associe a l'icone. A appeler a l'init mais ausi au reload pour prendre en compte les redimensionnements.
*@param cRendererName nom du rendu.
*@param pConfig donnees de configuration du rendu.
*/
#define CD_APPLET_SET_DESKLET_RENDERER_WITH_DATA(cRendererName, pConfig) do { \
	cairo_dock_set_desklet_renderer_by_name (myDesklet, cRendererName, NULL, CAIRO_DOCK_LOAD_ICONS_FOR_DESKLET, (CairoDeskletRendererConfigPtr) pConfig); \
	myDrawContext = cairo_create (myIcon->pIconBuffer); } while (0)
/**
*Definit le moteur de rendu de l'applet en mode desklet et le contexte de dessin associe a l'icone. A appeler a l'init mais ausi au reload pour prendre en compte les redimensionnements.
*@param cRendererName nom du rendu.
*/
#define CD_APPLET_SET_DESKLET_RENDERER(cRendererName) CD_APPLET_SET_DESKLET_RENDERER_WITH_DATA (cRendererName, NULL)

/**
*Cree et charge entierement un sous-dock pour notre icone.
*@param pIconsList la liste (eventuellement NULL) des icones du sous-dock; celles-ci seront chargees en dans la foulee.
*@param cRenderer nom du rendu.
*/
#define CD_APPLET_CREATE_MY_SUBDOCK(pIconsList, cRenderer) do { \
	if (myIcon->acName == NULL) { \
		CD_APPLET_SET_NAME_FOR_MY_ICON (myIcon->pModuleInstance->pModule->pVisitCard->cModuleName); } \
	if (cairo_dock_check_unique_subdock_name (myIcon)) { \
		CD_APPLET_SET_NAME_FOR_MY_ICON (myIcon->acName); } \
	myIcon->pSubDock = cairo_dock_create_subdock_from_scratch (pIconsList, myIcon->acName, myDock); \
	cairo_dock_set_renderer (myIcon->pSubDock, cRenderer); \
	cairo_dock_update_dock_size (myIcon->pSubDock); } while (0)
/**
*Detruit notre sous-dock et les icones contenues dedans s'il y'en a.
*/
#define CD_APPLET_DESTROY_MY_SUBDOCK do { \
	cairo_dock_destroy_dock (myIcon->pSubDock, myIcon->acName, NULL, NULL); \
	myIcon->pSubDock = NULL; } while (0)
/**
*Charge entierement une liste d'icones dans le sous-dock de notre icone.
*@param pIconsList la liste (eventuellement NULL) des icones du sous-dock; celles-ci seront chargees en dans la foulee.
*/
#define CD_APPLET_LOAD_ICONS_IN_MY_SUBDOCK(pIconsList) do { \
	if (myIcon->acName == NULL) { \
		CD_APPLET_SET_NAME_FOR_MY_ICON (myIcon->pModuleInstance->pModule->pVisitCard->cModuleName); } \
	myIcon->pSubDock->icons = pIconsList; \
	cairo_dock_load_buffers_in_one_dock (myIcon->pSubDock); \
	cairo_dock_update_dock_size (myIcon->pSubDock); } while (0)

//\_________________________________ INCLUDE
/**
*Exportation des variables globales de l'applet. A inclure dans chaque .c ou elles sont utilisees (directement ou via les macros).
*/
#define CD_APPLET_INCLUDE_MY_VARS 

#ifdef CD_APPLET_MULTI_INSTANCE
#include <cairo-dock-applet-multi-instance.h>
#else
#include <cairo-dock-applet-single-instance.h>
#endif


//\_________________________________ INTERNATIONNALISATION
/**
*Macro pour gettext, similaire a _() et N_(), mais avec le nom de domaine en parametre. Encadrez toutes vos chaines de caracteres statiques avec ca, de maniere a ce que l'utilitaire 'xgettext' puisse les trouver et les inclure automatiquement dans les fichiers de traduction.
*/
#define D_(message) dgettext (MY_APPLET_GETTEXT_DOMAIN, message)
#define _D D_

G_END_DECLS
#endif
