
#ifndef __CAIRO_DOCK_DBUS__
#define  __CAIRO_DOCK_DBUS__

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include "cairo-dock-struct.h"
G_BEGIN_DECLS


/*
* Initialise le gestionnaire de bus dans le dock. Il recupere la connexion 'session' de DBus, y etablit un proxy, et les met a disposition de tout le monde.
*/
//void cairo_dock_initialize_dbus_manager(void);

/**
* Retourne la connexion 'session' de DBus.
*@return la connexion au bus.
*/
DBusGConnection *cairo_dock_get_session_connection (void);
#define cairo_dock_get_dbus_connection cairo_dock_get_session_connection
/**
* Enregistre un nouveau service sur le bus.
*@param cServiceName le nom du service.
*/
void cairo_dock_register_service_name (const gchar *cServiceName);
/**
* Dis si le bus est disponible dans le dock.
*@return TRUE ssi le bus a pu etre recupere precedemment.
*/
gboolean cairo_dock_bdus_is_enabled (void);

/**
* Cree un nouveau proxy pour la connexion 'session'.
*@param name un nom sur le bus.
*@param path le chemin associe.
*@param interface nom de l'interface associee.
*@return le proxy nouvellement cree.
*/
DBusGProxy *cairo_dock_create_new_session_proxy (const char *name, const char *path, const char *interface);
#define cairo_dock_create_new_dbus_proxy cairo_dock_create_new_session_proxy
/**
* Cree un nouveau proxy pour la connexion 'system'.
*@param name un nom sur le bus.
*@param path le chemin associe.
*@param interface nom de l'interface associee.
*@return le proxy nouvellement cree.
*/
DBusGProxy *cairo_dock_create_new_system_proxy (const char *name, const char *path, const char *interface);

/**
* Detecte si une application est couramment lancee.
*@param cName nom de l'application.
*@return TRUE ssi l'application est lancee et possede un service sur le bus.
*/
gboolean cairo_dock_dbus_detect_application (const gchar *cName);

/**
* Recupere la valeur d'un parametre booleen sur le bus.
*@param pDbusProxy associe a la connexion.
*@param cParameter nom du parametre.
*@return la valeur du parametre.
*/
gboolean cairo_dock_dbus_get_boolean (DBusGProxy *pDbusProxy, const gchar *cParameter);

/**
* Recupere la valeur d'un parametre entier non signe sur le bus.
*@param pDbusProxy associe a la connexion.
*@param cParameter nom du parametre.
*@return la valeur du parametre.
*/
guint cairo_dock_dbus_get_uinteger (DBusGProxy *pDbusProxy, const gchar *cParameter);

/**
* Recupere la valeur d'un parametre entier sur le bus.
*@param pDbusProxy associe a la connexion.
*@param cParameter nom du parametre.
*@return la valeur du parametre.
*/
int cairo_dock_dbus_get_integer (DBusGProxy *pDbusProxy, const gchar *cParameter);

/**
* Recupere la valeur d'un parametre 'chaine de caracteres' sur le bus.
*@param pDbusProxy associe a la connexion.
*@param cParameter nom du parametre.
*@return la valeur du parametre.
*/
gchar *cairo_dock_dbus_get_string (DBusGProxy *pDbusProxy, const gchar *cParameter);

gchar **cairo_dock_dbus_get_string_list (DBusGProxy *pDbusProxy, const gchar *cParameter);

/**
* Recupere la valeur d'un parametre 'caracteres non signe' sur le bus.
*@param pDbusProxy associe a la connexion.
*@param cParameter nom du parametre.
*@return la valeur du parametre.
*/
guchar *cairo_dock_dbus_get_uchar (DBusGProxy *pDbusProxy, const gchar *cParameter);

gdouble cairo_dock_dbus_get_double (DBusGProxy *pDbusProxy, const gchar *cParameter);

/**
* Appelle une commande sur le bus.
*@param pDbusProxy associe a la connexion.
*@param cCommand nom de la commande.
*/
void cairo_dock_dbus_call (DBusGProxy *pDbusProxy, const gchar *cCommand);

G_END_DECLS
#endif
