#ifndef __CAIRO_DOCK_EMBLEM__
#define  __CAIRO_DOCK_EMBLEM__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

typedef struct _CairoDockFullEmblem CairoDockFullEmblem;
typedef struct _CairoDockTempEmblem CairoDockTempEmblem;

struct _CairoDockFullEmblem {
	cairo_surface_t *pSurface;
	double fEmblemW;
	double fEmblemH;
	gchar *cImagePath;
};

struct _CairoDockTempEmblem {
	int iSidTimer;
	Icon *pIcon;
	cairo_t *pIconContext;
	CairoContainer *pContainer;
};

//Needed for emblem type
typedef enum {
	CAIRO_DOCK_EMBLEM_UPPER_RIGHT = 0,
	CAIRO_DOCK_EMBLEM_LOWER_RIGHT,
	CAIRO_DOCK_EMBLEM_UPPER_LEFT,
	CAIRO_DOCK_EMBLEM_LOWER_LEFT,
	CAIRO_DOCK_EMBLEM_MIDDLE,
	CAIRO_DOCK_EMBLEM_MIDDLE_BOTTOM,
	CAIRO_DOCK_EMBLEM_BACKGROUND,
	CAIRO_DOCK_EMBLEM_TOTAL_NB,
} CairoDockEmblem;

#define cairo_dock_make_emblem_from_file(cIconFile, iEmblemType, bPersistent) cairo_dock_draw_emblem_on_my_icon(myDrawContext, cIconFile, myIcon, myContainer, iEmblemType, bPersistent);
#define CD_APPLET_DRAW_EMBLEM_FROM_FILE(cIconFile, iEmblemType) cairo_dock_draw_emblem_on_my_icon(myDrawContext, cIconFile, myIcon, myContainer, iEmblemType, TRUE);

/**
*Dessine un embleme sur une icone.
*@param pIconContext le contexte du dessin; n'est pas altere par la fonction.
*@param cIconFile le fichier image a afficher comme embleme.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*@param iEmblemType énumération du type d'embleme.
*@param bPersistent a TRUE l'emblème est imprimé directement sur l'icône, FALSE l'emblème est sur une couche supérieur et s'éffacera si on touche au CairoContexte de l'icône.
*/
void cairo_dock_draw_emblem_on_my_icon(cairo_t *pIconContext, const gchar *cIconFile, Icon *pIcon, CairoContainer *pContainer, CairoDockEmblem iEmblemType, gboolean bPersistent);

#define cairo_dock_make_emblem_from_surface(pSurface, iEmblemType, bPersistent) cairo_dock_draw_emblem_on_my_icon(myDrawContext, pSurface, myIcon, myContainer, iEmblemType, bPersistent);
#define CD_APPLET_DRAW_EMBLEM_FROM_SURFACE(pSurface, iEmblemType) cairo_dock_draw_emblem_on_my_icon(myDrawContext, pSurface, myIcon, myContainer, iEmblemType, TRUE);

/**
*Dessine un embleme sur une icone a partir d'une surface.
*@param pIconContext le contexte du dessin; n'est pas altere par la fonction.
*@param pSurface la surface a utiliser comme embleme.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*@param iEmblemType énumération du type d'embleme.
*@param bPersistent a TRUE l'emblème est imprimé directement sur l'icône, FALSE l'emblème est sur une couche supérieure et s'éffacera si on touche au CairoContexte de l'icône.
*/
void cairo_dock_draw_emblem_from_surface (cairo_t *pIconContext, cairo_surface_t *pSurface, Icon *pIcon, CairoContainer *pContainer, CairoDockEmblem iEmblemType, gboolean bPersistent);

//Needed for emblem classic type
typedef enum {
	CAIRO_DOCK_EMBLEM_BLANK = 0,
	CAIRO_DOCK_EMBLEM_CHARGE,
	CAIRO_DOCK_EMBLEM_DROP_INDICATOR,
	CAIRO_DOCK_EMBLEM_PLAY,
	CAIRO_DOCK_EMBLEM_PAUSE,
	CAIRO_DOCK_EMBLEM_STOP,
	CAIRO_DOCK_EMBLEM_BROKEN,
	CAIRO_DOCK_EMBLEM_ERROR,
	CAIRO_DOCK_EMBLEM_WARNING,
	CAIRO_DOCK_EMBLEM_LOCKED,
	CAIRO_DOCK_EMBLEM_CLASSIC_NB,
} CairoDockClassicEmblem;

#define cairo_dock_make_emblem(iEmblemClassic, iEmblemType, bPersistent) cairo_dock_draw_emblem_classic(myDrawContext, myIcon, myContainer, iEmblemClassic, iEmblemType, bPersistent);
#define CD_APPLET_DRAW_EMBLEM(iEmblemClassic, iEmblemType) cairo_dock_draw_emblem_classic(myDrawContext, myIcon, myContainer, iEmblemClassic, iEmblemType, TRUE);

/**
*Dessine un embleme sur une icone avec une banque local.
*@param pIconContext le contexte du dessin; n'est pas altere par la fonction.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*@param iEmblemClassic énumération du type d'embleme classique.
*@param iEmblemType énumération du type d'embleme.
*@param bPersistent a TRUE l'emblème est imprimé directement sur l'icône, FALSE l'emblème est sur une couche supérieur et s'éffacera si on touche au CairoContexte de l'icône.
*/
void cairo_dock_draw_emblem_classic (cairo_t *pIconContext, Icon *pIcon, CairoContainer *pContainer, CairoDockClassicEmblem iEmblemClassic, CairoDockEmblem iEmblemType, gboolean bPersistent);

#define cairo_dock_make_temporary_emblem(cIconFile, iEmblemClassic, iEmblemType, fTimeLength) cairo_dock_draw_temporary_emblem_on_my_icon(myDrawContext, myIcon, myContainer, cIconFile, iEmblemClassic, iEmblemType, fTimeLength);
#define CD_APPLET_MAKE_TEMPORARY_EMBLEM_WITH_FILE(cIconFile, iEmblemType, fTimeLength) cairo_dock_draw_temporary_emblem_on_my_icon(myDrawContext, myIcon, myContainer, cIconFile, NULL, iEmblemType, fTimeLength);
#define CD_APPLET_MAKE_TEMPORARY_EMBLEM_CLASSIC(iEmblemClassic, iEmblemType, fTimeLength) cairo_dock_draw_temporary_emblem_on_my_icon(myDrawContext, myIcon, myContainer, NULL, iEmblemClassic, iEmblemType, fTimeLength);

/**
*Dessine un embleme sur une icone avec une banque local.
*@param pIconContext le contexte du dessin; n'est pas altere par la fonction.
*@param pIcon l'icone.
*@param pContainer le container de l'icone.
*@param cIconFile le fichier image a afficher comme embleme.
*@param iEmblemClassic énumération du type d'embleme classique.
*@param iEmblemType énumération du type d'embleme.
*@param bPersistent a TRUE l'emblème est imprimé directement sur l'icône, FALSE l'emblème est sur une couche supérieur et s'éffacera si on touche au CairoContexte de l'icône.
*@param fTimeLength temps en millisecondes.
*/
void cairo_dock_draw_temporary_emblem_on_my_icon (cairo_t *pIconContext, Icon *pIcon, CairoContainer *pContainer, const gchar *cIconFile, CairoDockClassicEmblem iEmblemClassic, CairoDockEmblem iEmblemType, double fTimeLength);

/**
*Récupère l'emplacement des emblèmes utilisateur dans le fichier de configuration.
*@param pKeyFile le fichier de configuration.
*@param bFlushConfFileNeeded mis a TRUE si une clé est manquante.
*/
void cairo_dock_get_emblem_path (GKeyFile *pKeyFile, gboolean *bFlushConfFileNeeded);

/**
*Libère les surfaces et les emplacements dans la mémoire.
*/
void cairo_dock_free_emblem (void);

/**
*Met a jour l'emplacement des emblèmes utilisateurs.
*@param pKeyFile le fichier de configuration.
*@param bFlushConfFileNeeded mis a TRUE si une clé est manquante.
*/
void cairo_dock_updated_emblem_conf_file (GKeyFile *pKeyFile, gboolean *bFlushConfFileNeeded);

G_END_DECLS
#endif
