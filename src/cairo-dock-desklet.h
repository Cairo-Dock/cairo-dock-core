/*
** Login : <ctaf42@gmail.com>
** Started on  Sun Jan 27 18:35:38 2008 Cedric GESTES
** $Id$
**
** Author(s)
**  - Cedric GESTES <ctaf42@gmail.com>
**  - Fabrice REY
**
** Copyright (C) 2008 Cedric GESTES
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __CAIRO_DESKLET_H__
#define  __CAIRO_DESKLET_H__

#include "cairo-dock-struct.h"
#include "cairo-dock-container.h"
//#include "cairo-dock-icons.h"
G_BEGIN_DECLS


/**
*@file cairo-dock-desklet.h This class defines the Desklets, that are Widgets placed directly on your desktop.
* A Desklet is a container that holds 1 applet's icon plus an optionnal list of other icons and an optionnal GTK widget, has a decoration, suports several accessibility types (like Compiz Widget Layer), and has a renderer.
* Desklets can be resized or moved directly with the mouse, and can be rotated in the 3 directions of space.
*/

typedef enum {
	CAIRO_DESKLET_NORMAL = 0,
	CAIRO_DESKLET_KEEP_ABOVE,
	CAIRO_DESKLET_KEEP_BELOW,
	CAIRO_DESKLET_ON_WIDGET_LAYER,
	CAIRO_DESKLET_RESERVE_SPACE
	} CairoDeskletAccessibility;

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
	gint iDecorationPlanesRotation;
	};

struct _CairoDeskletAttribute {
	gboolean bDeskletUseSize;
	gint iDeskletWidth;
	gint iDeskletHeight;
	gint iDeskletPositionX;
	gint iDeskletPositionY;
	gboolean bPositionLocked;
	gint iRotation;
	gint iDepthRotationY;
	gint iDepthRotationX;
	gchar *cDecorationTheme;
	CairoDeskletDecoration *pUserDecoration;
	CairoDeskletAccessibility iAccessibility;
	gboolean bOnAllDesktops;
} ;

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
	/// intervalle de temps entre 2 etapes de l'animation.
	gint iAnimationDeltaT;
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
	/// le facteur de zoom lors du detachage d'une applet.
	gdouble fZoom;
	gboolean bUseReflect_unused;
	/// contexte OpenGL associe a la fenetre.
	GLXContext glContext;
	/// TRUE <=> une animation lente est en cours.
	gboolean bKeepSlowAnimation;
	/// compteur pour l'animation.
	gint iAnimationStep;
	/// Liste eventuelle d'icones placees sur le desklet, et susceptibles de recevoir des clics.
	GList *icons;
	/// liste des notifications disponibles.
	GPtrArray *pNotificationsTab;
	/// le moteur de rendu utilise pour dessiner le desklet.
	CairoDeskletRenderer *pRenderer;
	/// donnees pouvant etre utilisees par le moteur de rendu.
	gpointer pRendererData;
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
	gdouble fDepthRotationY;
	gdouble fDepthRotationX;
	gboolean rotatingY;
	gboolean rotatingX;
	gboolean rotating;
	/// rattachement au dock.
	gboolean retaching;
	/// date du clic.
	guint time;
	GLuint iBackGroundTexture;
	GLuint iForeGroundTexture;
	gboolean bFixedAttitude;
	GtkWidget *pInteractiveWidget;
	gboolean bSpaceReserved;
};

#define CD_NB_ITER_FOR_GRADUATION 10


/**
* Teste si le container est un desklet.
*@param pContainer le container.
*@return TRUE ssi le container a ete declare comme un desklet.
*/
#define CAIRO_DOCK_IS_DESKLET(pContainer) (pContainer != NULL && (pContainer)->iType == CAIRO_DOCK_TYPE_DESKLET)
/**
* Caste un container en desklet.
*@param pContainer le container.
*@return le desklet.
*/
#define CAIRO_DESKLET(pContainer) ((CairoDesklet *)pContainer)

void cairo_dock_load_desklet_buttons (cairo_t *pSourceContext);
void cairo_dock_load_desklet_buttons_texture (void);
void cairo_dock_unload_desklet_buttons_texture (void);

gboolean cairo_dock_render_desklet_notification (gpointer pUserData, CairoDesklet *pDesklet);

/**
* Créer un desklet tout simple, sans le placer ni lui définir un moteur de rendu.
*@param pIcon l'icône principale du desklet (jamais testé avec une icône nulle).
*@param pInteractiveWidget le widget d'interaction du desklet, ou NULL si aucun.
*@param iAccessibility pour savoir s'il faut placer dés maintenant le desklet sur la couche des widgets ou reserver son espace.
*@return le desklet nouvellement crée.
*/
CairoDesklet *cairo_dock_create_desklet (Icon *pIcon, GtkWidget *pInteractiveWidget, CairoDeskletAccessibility iAccessibility);

/**
* Configure entièrement un desklet en fonction de ses paramètres de sauvegarde extraits d'un fichier de conf. Le place, le dimensionne, le garde devant/derrière, vérouille sa position, et le place sur la couche des widgets, et pour finir definit ses decorations.
*@param pDesklet le desklet à placer.
*@param pAttribute la config du desklet, contenant tous les paramètres necessaires.
*/
void cairo_dock_configure_desklet (CairoDesklet *pDesklet, CairoDeskletAttribute *pAttribute);

/**
* Détruit un desklet, et libère toutes les ressources allouées. Son widget interactif est épargné et peut être re-inséré autre part après l'opération.
*@param pDesklet le desklet à détruire.
*/
void cairo_dock_free_desklet (CairoDesklet *pDesklet);


/**
* Ajoute un GtkWidget à la fenêtre du desklet. Ce widget ne doit pas appartenir à un autre container. 1 seul widget par desklet, mais si ce widget est un GtkContainer, on peut en mettre plusieurs autres dedans.
*@param pInteractiveWidget le widget à ajouter.
*@param pDesklet le desklet dans lequel le rajouter.
*@param iRightMargin marge a droite en pixels, 0 si on ne veut pas de marge.
*/
void cairo_dock_add_interactive_widget_to_desklet_full (GtkWidget *pInteractiveWidget, CairoDesklet *pDesklet, int iRightMargin);

#define cairo_dock_add_interactive_widget_to_desklet(pInteractiveWidget, pDesklet) cairo_dock_add_interactive_widget_to_desklet_full (pInteractiveWidget, pDesklet, 0)

void cairo_dock_set_desklet_margin (CairoDesklet *pDesklet, int iRightMargin);

/**
* Détache un GtkWidget d'un desklet, le rendant libre d'être inséré autre part.
*@param pDesklet le desklet contenant un widge tinteractif.
*/
void cairo_dock_steal_interactive_widget_from_desklet (CairoDesklet *pDesklet);


void cairo_dock_project_coords_on_3D_desklet (CairoDesklet *pDesklet, int iMouseX, int iMouseY, int *iX, int *iY);
#define cairo_dock_get_coords_on_3D_desklet(pDesklet, xptr, yptr) cairo_dock_project_coords_on_3D_desklet (pDesklet, pDesklet->iMouseX, pDesklet->iMouseY, xptr, yptr)
/**
* Trouve l'icône cliquée dans un desklet, en cherchant parmi l'icône principale et éventuellement la liste des icônes associées.
*@param pDesklet le desklet cliqué.
*@return l'icône cliquée ou NULL si on a cliqué à côté.
*/
Icon *cairo_dock_find_clicked_icon_in_desklet (CairoDesklet *pDesklet);

/**
* Cache un desklet.
*@param pDesklet le desklet à cacher.
*/
void cairo_dock_hide_desklet (CairoDesklet *pDesklet);
/**
* Montre un desklet, lui donnant le focus.
*@param pDesklet le desklet à montrer.
*/
void cairo_dock_show_desklet (CairoDesklet *pDesklet);

/**
* Rend tous les desklets visibles si ils étaient cachés, et au premier plan s'ils étaient derrière ou sur la widget layer.
*@param bOnWidgetLayerToo TRUE ssi on veut faire passer tous les desklets de la couche widgets à la couche normale.
*/
void cairo_dock_set_all_desklets_visible (gboolean bOnWidgetLayerToo);
/**
* Remet les desklets dans la position occupée avant l'appel à la fonction précédente.
*/
void cairo_dock_set_desklets_visibility_to_default (void);

/**
* Renvoie le desklet dont la fenetre correspond à la ressource X donnée.
*@param Xid ID du point de vue de X.
*@return le desklet ou NULL si aucun ne correspond.
*/
CairoDesklet *cairo_dock_get_desklet_by_Xid (Window Xid);

/**
* Lance une animation de zoom vers l'avant sur un desklet.
*@param pDesklet le desklet.
*/
void cairo_dock_zoom_out_desklet (CairoDesklet *pDesklet);


void cairo_dock_load_desklet_decorations (CairoDesklet *pDesklet, cairo_t *pSourceContext);

void cairo_dock_reload_desklets_decorations (gboolean bDefaultThemeOnly, cairo_t *pSourceContext);

void cairo_dock_free_desklet_decoration (CairoDeskletDecoration *pDecoration);


#define cairo_dock_set_static_desklet(pDesklet) (pDesklet)->bFixedAttitude = TRUE


void cairo_dock_reserve_space_for_desklet (CairoDesklet *pDesklet, gboolean bReserve);


G_END_DECLS

#endif
