
#ifndef __CAIRO_DOCK_CONFIG__
#define  __CAIRO_DOCK_CONFIG__

#include <glib.h>

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-config.h This file, together with cairo-dock-keyfile-utilities, handles the configuration system of Cairo-Dock.
*/

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
gboolean cairo_dock_get_boolean_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, gboolean bDefaultValue, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName);
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
int cairo_dock_get_integer_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, int iDefaultValue, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName);
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
double cairo_dock_get_double_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, double fDefaultValue, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName);
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
gchar *cairo_dock_get_string_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultValue, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName);
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
void cairo_dock_get_integer_list_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, int *iValueBuffer, guint iNbElements, int *iDefaultValues, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName);
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
void cairo_dock_get_double_list_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, double *fValueBuffer, guint iNbElements, double *fDefaultValues, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName);
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
gchar **cairo_dock_get_string_list_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, gsize *length, const gchar *cDefaultValues, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName);

void cairo_dock_get_size_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, gint iDefaultSize, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName, int *iWidth, int *iHeight);
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
gchar *cairo_dock_get_file_path_key_value (GKeyFile *pKeyFile, const gchar *cGroupName, const gchar *cKeyName, gboolean *bFlushConfFileNeeded, const gchar *cDefaultGroupName, const gchar *cDefaultKeyName, const gchar *cDefaultDir, const gchar *cDefaultFileName);

#define cairo_dock_get_size_key_value_helper(pKeyFile, cGroupName, cKeyPrefix, bFlushConfFileNeeded, iWidth, iHeight) \
	cairo_dock_get_size_key_value (pKeyFile, cGroupName, cKeyPrefix"size", &bFlushConfFileNeeded, 0, NULL, NULL, &iWidth, &iHeight);\
	if (iWidth == 0) {\
		iWidth = g_key_file_get_integer (pKeyFile, cGroupName, cKeyPrefix"width", NULL);\
		if (iWidth != 0) {\
			iHeight = g_key_file_get_integer (pKeyFile, cGroupName, cKeyPrefix"height", NULL);\
			int iSize[2] = {iWidth, iHeight};\
			g_key_file_set_integer_list (pKeyFile, cGroupName, cKeyPrefix"size", iSize, 2); } }

#define cairo_dock_get_pango_weight_from_1_9(iWeight) (((PANGO_WEIGHT_HEAVY - PANGO_WEIGHT_ULTRALIGHT) * iWeight + 9 * PANGO_WEIGHT_ULTRALIGHT - PANGO_WEIGHT_HEAVY) / 8)
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
*Recupere les 3 numeros de version d'une chaine.
*@param cVersionString la version representee par une chaine.
*@param iMajorVersion numero de version majeure renvoyee.
*@param iMinorVersion numero de version mineure renvoyee.
*@param iMicroVersion numero de version micro renvoyee.
*/
void cairo_dock_get_version_from_string (const gchar *cVersionString, int *iMajorVersion, int *iMinorVersion, int *iMicroVersion);


/**
* Decrypte une chaine de caracteres (DES-encryption de libcrypt).
*@param cEncryptedString la version encryptee de la chaine.
*@param cDecryptedString la version non encryptee de la chaine.
*/
void cairo_dock_decrypt_string( const guchar *cEncryptedString,  guchar **cDecryptedString );

/**
* Crypte une chaine de caracteres (DES-encryption de libcrypt).
*@param cDecryptedString la version non encryptee de la chaine.
*@param cEncryptedString la version encryptee de la chaine.
*/
void cairo_dock_encrypt_string( const guchar *cDecryptedString,  guchar **cEncryptedString );

#define DEFINE_PRE_INIT(cGroupName) \
void cairo_dock_pre_init_##cGroupName (CairoDockInternalModule *pModule)


G_END_DECLS
#endif
