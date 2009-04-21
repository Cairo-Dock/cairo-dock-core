/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 2 -*- */

#ifndef __CAIRO_DOCK_APPLET_FACILITY__
#define  __CAIRO_DOCK_APPLET_FACILITY__

#include "cairo-dock-struct.h"
#include "cairo-dock-modules.h"
#include "cairo-dock-menu.h"
#include "cairo-dock-applet-factory.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-applet-facility.h A collection of useful macros for applets.
* Macros provides a normalized API that will :
*   let you perform complex operations with a minimum amount of code
*   ensure a bug-free functioning
*   mask the internal complexity
*   allow a normalized and easy-to-maintain code amongst all the applets.
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
*Renvoie le chemin correspondant au theme figurant dans la config.
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
*@param pUserData structure passee en entree des fonctions read et update.
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
*Dis si une mesure est en cours, c'est a dire si elle est soit dans le thread, soit en attente d'update.
*@param pMeasureTimer la mesure periodique.
*@return TRUE ssi la mesure est en cours.
*/
gboolean cairo_dock_measure_is_running (CairoDockMeasure *pMeasureTimer);
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


/**Renvoie la version de gnome.
*@param iMajor adresse de la majeure.
*@param iMinor adresse de la mineure.
*@param iMicro adresse de la micro.
*/
void cairo_dock_get_gnome_version (int *iMajor, int *iMinor, int *iMicro);


void cairo_dock_pop_up_about_applet (GtkMenuItem *menu_item, CairoDockModuleInstance *pModuleInstance);


  //////////////
 /// CONFIG ///
//////////////

/** Recupere la valeur d'un parametre 'booleen' du fichier de conf.
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
*Recupere la valeur d'un parametre 'couleur' au format RVBA. du fichier de conf.
*@param cGroupName nom du groupe dans le fichier de conf.
*@param cKeyName nom de la cle dans le fichier de conf.
*@param pColorBuffer tableau de 4 double deja alloue, et qui sera rempli avec les 4 composantes de la couleur.
*@param pDefaultColor valeur par defaut si la cle et/ou le groupe n'est pas trouve (typiquement si cette cle est nouvelle). C'est un tableau de 4 double, ou NULL.
*/
#define CD_CONFIG_GET_COLOR_WITH_DEFAULT(cGroupName, cKeyName, pColorBuffer, pDefaultColor) cairo_dock_get_double_list_key_value (pKeyFile, cGroupName, cKeyName, &bFlushConfFileNeeded, (double*)pColorBuffer, 4, pDefaultColor, NULL, NULL)
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
*Trouve le chemin du theme specifie en conf.
*@param cGroupName nom du groupe (dans le fichier de conf) du parametre correspondant au theme.
*@param cKeyName nom de la cle (dans le fichier de conf) du parametre correspondant au theme.
*@param cThemeDirName nom commun aux repertoires contenant les themes locaux, utilisateurs, et distants.
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


  ////////////
 /// MENU ///
////////////
/** Cree et ajoute un sous-menu a un menu.
*@param cLabel nom du sous-menu, tel qu'il apparaitra dans le menu.
*@param pMenu GtkWidget du menu auquel on rajoutera le sous-menu.
*@return le sous-menu nouvellement cree et attache au menu.
*/
#define CD_APPLET_ADD_SUB_MENU(cLabel, pMenu) \
	__extension__ ({\
	GtkWidget *_pSubMenu = gtk_menu_new (); \
	pMenuItem = gtk_menu_item_new_with_label (cLabel); \
	gtk_menu_shell_append  (GTK_MENU_SHELL (pMenu), pMenuItem); \
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), _pSubMenu);\
	_pSubMenu; })

/** Cree et ajoute un sous-menu par defaut au menu principal. Ce sous-menu est nomme suivant le nom de l'applet, et est represente par l'icone de l'applet.
*@return le sous-menu nouvellement cree et attache au menu.
*/
#define CD_APPLET_CREATE_MY_SUB_MENU(...) \
	__extension__ ({\
	pMenuItem = gtk_image_menu_item_new_with_label (myApplet->pModule->pVisitCard->cModuleName);\
	GdkPixbuf *_pixbuf = gdk_pixbuf_new_from_file_at_size (MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE, 32, 32, NULL);\
	image = gtk_image_new_from_pixbuf (_pixbuf);\
	g_object_unref (_pixbuf);\
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (pMenuItem), image);\
	gtk_menu_shell_append  (GTK_MENU_SHELL (CD_APPLET_MY_MENU), pMenuItem);\
	GtkWidget *_pSubMenu = gtk_menu_new ();\
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (pMenuItem), _pSubMenu);\
	_pSubMenu; })

/** Cree et ajoute un sous-menu a un menu deja existant.
*@param cLabel nom du sous-menu, tel qu'il apparaitra dans le menu.
*@param pMenu GtkWidget du menu auquel on rajoutera le sous-menu.
*@return le GtkWidget du sous-menu.
*/
#define CD_APPLET_CREATE_AND_ADD_SUB_MENU(cLabel, pMenu) cairo_dock_create_sub_menu (cLabel, pMenu)

/** Cree et ajoute un sous-menu a un menu.
*@param cLabel nom du sous-menu, tel qu'il apparaitra dans le menu.
*@param pFunction fonction appelee lors de la selection de cette entree.
*@param pMenu GtkWidget du menu auquel on rajoutera le sous-menu.
*@param pData donnees passees en parametre de la fonction (doit contenir myApplet).
*/
#define CD_APPLET_ADD_IN_MENU_WITH_DATA(cLabel, pFunction, pMenu, pData) do { \
	pMenuItem = gtk_menu_item_new_with_label (cLabel); \
	gtk_menu_shell_append  (GTK_MENU_SHELL (pMenu), pMenuItem); \
	g_signal_connect (G_OBJECT (pMenuItem), "activate", G_CALLBACK (pFunction), pData); } while (0)

/** Ajoute une entree a un menu deja existant.
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
#define CD_APPLET_ADD_IN_MENU_WITH_STOCK_AND_DATA CAIRO_DOCK_ADD_IN_MENU_WITH_STOCK_AND_DATA

/**
*Ajoute une entree avec une icone GTK a un menu deja existant.
*@param cLabel nom de l'entree, tel qu'il apparaitra dans le menu.
*@param gtkStock icone GTK
*@param pFunction fonction appelee lors de la selection de cette entree.
*@param pMenu GtkWidget du menu auquel on rajoutera l'entree.
*/
#define CD_APPLET_ADD_IN_MENU_WITH_STOCK(cLabel, gtkStock, pFunction, pMenu) CD_APPLET_ADD_IN_MENU_WITH_STOCK_AND_DATA(cLabel, gtkStock, pFunction, pMenu, myApplet)

/**
 * Ajoute un separateur dans un menu deja existant
 */
#define CD_APPLET_ADD_SEPARATOR_IN_MENU(pMenu) do { \
	pMenuItem = gtk_separator_menu_item_new (); \
	gtk_menu_shell_append(GTK_MENU_SHELL (pMenu), pMenuItem); } while (0)
#define CD_APPLET_ADD_SEPARATOR CD_APPLET_ADD_SEPARATOR_IN_MENU

/**
*Ajoute une entree pour la fonction 'A propos'.
*@param pMenu GtkWidget du menu auquel sera ajoutee l'entree.
*/
#define CD_APPLET_ADD_ABOUT_IN_MENU(pMenu) CD_APPLET_ADD_IN_MENU_WITH_STOCK (_("About this applet"), GTK_STOCK_ABOUT, cairo_dock_pop_up_about_applet, pMenu)


  /////////////////////////////
 /// VARIABLES DISPONIBLES ///
/////////////////////////////

//\______________________ click droit, click milieu, click gauche.
/** Icone cliquee.
*/
#define CD_APPLET_CLICKED_ICON pClickedIcon
/** Container clique.
*/
#define CD_APPLET_CLICKED_CONTAINER pClickedContainer

//\______________________ click droit
/**  La touche 'SHIFT' est-elle enfoncee au moment du clic ?
*/
#define CD_APPLET_SHIFT_CLICK (iButtonState & GDK_SHIFT_MASK)
/**  La touche 'CTRL' est-elle enfoncee au moment du clic ?
*/
#define CD_APPLET_CTRL_CLICK (iButtonState & GDK_CONTROL_MASK)
/**  La touche 'ALT' est-elle enfoncee au moment du clic ?
*/
#define CD_APPLET_ALT_CLICK (iButtonState & GDK_MOD1_MASK)

//\______________________ construction du menu.
/** Menu principal de l'applet.
*/
#define CD_APPLET_MY_MENU pAppletMenu
/** Icone cliquee.
*/
/** Donne la derniere entree ajoutee au menu par vous.
*@return le GtkWidget de la derniere entree.
*/
#define CD_APPLET_LAST_ITEM_IN_MENU pMenuItem

//\______________________ drop.
/** Donnees recues (chaine de caracteres).
*/
#define CD_APPLET_RECEIVED_DATA cReceivedData

//\______________________ scroll
/** Direction du scroll.
*/
#define CD_APPLET_SCROLL_DIRECTION iDirection
/** Scroll vers le haut.
*/
#define CD_APPLET_SCROLL_UP (CD_APPLET_SCROLL_DIRECTION == GDK_SCROLL_UP)
/** Scroll vers le bas.
*/
#define CD_APPLET_SCROLL_DOWN (CD_APPLET_SCROLL_DIRECTION == GDK_SCROLL_DOWN)


  //////////////////////
 /// DESSIN SURFACE ///
//////////////////////

/**
*Redessine l'icone de l'applet a l'ecran (des que la main loop est disponible).
*/
#define CD_APPLET_REDRAW_MY_ICON \
	cairo_dock_redraw_icon (myIcon, myContainer)
#define CAIRO_DOCK_REDRAW_MY_CONTAINER \
	cairo_dock_redraw_container (myContainer)
/**
*Recalcule le reflet de l'icone pour son dessin cairo (inutile en OpenGL).
*/
#define CD_APPLET_UPDATE_REFLECT_ON_MY_ICON \
	if (myContainer->bUseReflect) cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer)

/**
*Charge une image dans une surface, aux dimensions de l'icone de l'applet.
*@param cImagePath chemin du fichier de l'image.
*@return la surface nouvellement creee.
*/
#define CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET(cImagePath) \
	cairo_dock_create_surface_for_icon (cImagePath, myDrawContext, myIcon->fWidth * (myDock ? (1 + g_fAmplitude) / myDock->fRatio : 1), myIcon->fHeight* (myDock ? (1 + g_fAmplitude) / myDock->fRatio : 1))
/**
*Charge une image utilisateur dans une surface, aux dimensions de l'icone de l'applet, ou une image par defaut si la premiere est NULL.
*@param cUserImageName nom de l'image utilisateur.
*@param cDefaultLocalImageName icone par defaut
*@return la surface nouvellement creee.
*/
#define CD_APPLET_LOAD_USER_SURFACE_FOR_MY_APPLET(cUserImageName, cDefaultLocalImageName) \
	__extension__ ({\
	gchar *_cImagePath; \
	if (cUserImageName != NULL) \
		_cImagePath = cairo_dock_generate_file_path (cUserImageName); \
	else if (cDefaultLocalImageName != NULL)\
		_cImagePath = g_strdup_printf ("%s/%s", MY_APPLET_SHARE_DATA_DIR, cDefaultLocalImageName); \
	else\
		_cImagePath = NULL;\
	cairo_surface_t *pSurface = CD_APPLET_LOAD_SURFACE_FOR_MY_APPLET (_cImagePath); \
	g_free (_cImagePath);\
	pSurface; })


/**
*Applique une surface existante sur le contexte de dessin de l'applet, et la rafraichit.
*@param pSurface la surface cairo a dessiner.
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON(pSurface) do { \
	cairo_dock_set_icon_surface_with_reflect (myDrawContext, pSurface, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)
/**
*Applique une surface existante sur le contexte de dessin de l'applet en la zoomant, et la redessine.
*@param pSurface la surface cairo a dessiner.
*@param fScale le facteur de zoom (a 1 la surface remplit toute l'icone).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_ZOOM(pSurface, fScale) do { \
	cairo_dock_set_icon_surface_full (myDrawContext, pSurface, fScale, 1., myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)
/**
*Applique une surface existante sur le contexte de dessin de l'applet avec un facteur de transparence, et la rafraichit.
*@param pSurface la surface cairo a dessiner.
*@param fAlpha la transparence (dans [0 , 1]).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_ALPHA(pSurface, fAlpha) do { \
	cairo_dock_set_icon_surface_full (myDrawContext, pSurface, 1., fAlpha, myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)
/**
*Applique une surface existante sur le contexte de dessin de l'applet et ajoute une barre a sa base, et la rafraichit.
*@param pSurface la surface cairo a dessiner.
*@param fValue la valeur en fraction de la valeur max (donc dans [0 , 1]).
*/
#define CD_APPLET_SET_SURFACE_ON_MY_ICON_WITH_BAR(pSurface, fValue) do { \
	cairo_dock_set_icon_surface_with_bar (myDrawContext, pSurface, fValue, myIcon, myContainer); \
	cairo_dock_add_reflection_to_icon (myDrawContext, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)

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
*Idem que precedemment mais l'image est definie soit relativement au repertoire utilisateur, soit relativement au repertoire d'installation de l'applet si la 1ere est NULL.
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

#define CD_APPLET_SET_DEFAULT_IMAGE_ON_MY_ICON_IF_NONE do { \
	if (myIcon->acFileName == NULL) { \
		CD_APPLET_SET_LOCAL_IMAGE_ON_MY_ICON (MY_APPLET_ICON_FILE); } } while (0)

/**
*Applique une surface existante sur le contexte de dessin de l'applet, et la redessine. La surface est redimensionnee aux dimensions de l'icone.
*@param pSurface
*@param fScale
*/
#define CD_APPLET_SET_ZOOMED_SURFACE_ON_MY_ICON(pSurface, fScale) do { \
	cairo_dock_set_icon_surface_with_zoom (myDrawContext, pSurface, fScale, myIcon, myContainer); \
	cairo_dock_redraw_icon (myIcon, myContainer); } while (0)


  /////////////
 /// LABEL ///
/////////////
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


  /////////////////
 /// QUICK-INFO///
/////////////////
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


  /////////////////
 /// ANIMATION ///
/////////////////
/** Empeche l'icone d'etre animee au passage de la souris (a faire 1 fois au debut).
*/
#define CD_APPLET_SET_STATIC_ICON cairo_dock_set_icon_static (myIcon)

/** Lance l'animation de l'icone de l'applet.
*@param cAnimationName nom de l'animation.
*@param iAnimationLength duree de l'animation, en nombre de tours.
*/
#define CD_APPLET_ANIMATE_MY_ICON(cAnimationName, iAnimationLength) \
	cairo_dock_request_icon_animation (myIcon, myDock, cAnimationName, iAnimationLength)


#define CD_APPLET_START_DRAWING_MY_ICON cairo_dock_begin_draw_icon (myIcon, myContainer)

#define CD_APPLET_START_DRAWING_MY_ICON_OR_RETURN(...) \
	if (! cairo_dock_begin_draw_icon (myIcon, myContainer)) \
		return __VA_ARGS__

#define CD_APPLET_FINISH_DRAWING_MY_ICON cairo_dock_end_draw_icon (myIcon, myContainer)


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

#define CD_APPLET_SET_TRANSITION_ON_MY_ICON(render_step_cairo, render_step_opengl, bFastPace, iDuration, bRemoveWhenFinished) \
	cairo_dock_set_transition_on_icon (myIcon, myContainer, myDrawContext,\
		(CairoDockTransitionRenderFunc) render_step_cairo,\
		(CairoDockTransitionGLRenderFunc) render_step_opengl,\
		bFastPace,\
		iDuration,\
		bRemoveWhenFinished,\
		myApplet)
#define CD_APPLET_GET_TRANSITION_FRACTION(...) \
	(cairo_dock_has_transition (myIcon) ? cairo_dock_get_transition_fraction (myIcon) : 1.)
#define CD_APPLET_REMOVE_TRANSITION_ON_MY_ICON \
	cairo_dock_remove_transition_on_icon (myIcon)


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

/** Empeche le desklet d'etre tourner dans l'espace ou le plan.
*/
#define CD_APPLET_SET_STATIC_DESKLET cairo_dock_set_static_desklet (myDesklet)

/**
*Cree et charge entierement un sous-dock pour notre icone.
*@param pIconsList la liste (eventuellement NULL) des icones du sous-dock; celles-ci seront chargees en dans la foulee.
*@param cRenderer nom du rendu (optionnel).
*/
#define CD_APPLET_CREATE_MY_SUBDOCK(pIconsList, cRenderer) do { \
	if (myIcon->acName == NULL) { \
		CD_APPLET_SET_NAME_FOR_MY_ICON (myApplet->pModule->pVisitCard->cModuleName); } \
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
		CD_APPLET_SET_NAME_FOR_MY_ICON (myIcon->pModuleInstance->pModule->pVisitCard->cModuleName); }\
	else { \
		Icon *icon;\
		GList *ic;\
		for (ic = pIconsList; ic != NULL; ic = ic->next) {\
			icon = ic->data;\
			if (icon->cParentDockName == NULL)\
				icon->cParentDockName = g_strdup (myIcon->acName); } } \
	myIcon->pSubDock->icons = pIconsList; \
	myIcon->pSubDock->pFirstDrawnElement = pIconsList; \
	cairo_dock_load_buffers_in_one_dock (myIcon->pSubDock); \
	cairo_dock_update_dock_size (myIcon->pSubDock); } while (0)

#define CD_APPLET_DELETE_MY_ICONS_LIST do {\
	if (myDesklet && myDesklet->icons != NULL) {\
		g_list_foreach (myDesklet->icons, (GFunc) cairo_dock_free_icon, NULL);\
		g_list_free (myDesklet->icons);\
		myDesklet->icons = NULL; }\
	if (myIcon->pSubDock != NULL) {\
		if (myDesklet) {\
			CD_APPLET_DESTROY_MY_SUBDOCK; }\
		else {\
			g_list_foreach (myIcon->pSubDock->icons, (GFunc) cairo_dock_free_icon, NULL);\
			g_list_free (myIcon->pSubDock->icons);\
			myIcon->pSubDock->icons = NULL;\
			myIcon->pSubDock->pFirstDrawnElement = NULL; } } } while (0)

#define CD_APPLET_LOAD_MY_ICONS_LIST(pIconList, cDockRendererName, cDeskletRendererName, pDeskletRendererConfig) do {\
	if (myDock) {\
		if (myIcon->pSubDock == NULL) {\
			if (pIconList != NULL) {\
				CD_APPLET_CREATE_MY_SUBDOCK (pIconList, cDockRendererName); } }\
		else {\
			if (pIconList == NULL) {\
				CD_APPLET_DESTROY_MY_SUBDOCK; }\
			else {\
				cairo_dock_set_renderer (myIcon->pSubDock, cDockRendererName); \
				CD_APPLET_LOAD_ICONS_IN_MY_SUBDOCK (pIconList); } } }\
	else {\
		if (myIcon->pSubDock != NULL) {\
			CD_APPLET_DESTROY_MY_SUBDOCK; }\
		myDesklet->icons = pIconList;\
		CD_APPLET_SET_DESKLET_RENDERER_WITH_DATA (cDeskletRendererName, pDeskletRendererConfig);\
		CAIRO_DOCK_REDRAW_MY_CONTAINER;\
	} } while (0)

#define CD_APPLET_MY_ICONS_LIST (myDock ? (myIcon->pSubDock ? myIcon->pSubDock->icons : NULL) : myDesklet->icons)
#define CD_APPLET_MY_ICONS_LIST_CONTAINER (myDock ? CAIRO_CONTAINER (myIcon->pSubDock) : CAIRO_CONTAINER (myDesklet))


//\_________________________________ INTERNATIONNALISATION
/**
*Macro pour gettext, similaire a _() et N_(), mais avec le nom de domaine en parametre. Encadrez toutes vos chaines de caracteres statiques avec ca, de maniere a ce que l'utilitaire 'xgettext' puisse les trouver et les inclure automatiquement dans les fichiers de traduction.
*/
#define D_(message) dgettext (MY_APPLET_GETTEXT_DOMAIN, message)
#define _D D_

G_END_DECLS
#endif
