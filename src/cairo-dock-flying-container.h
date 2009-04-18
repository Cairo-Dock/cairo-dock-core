
#ifndef __CAIRO_FLYING_CONTAINER__
#define  __CAIRO_FLYING_CONTAINER__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

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
	/// le timer de l'animation.
	gint iSidGLAnimation;
	/// intervalle de temps entre 2 etapes de l'animation.
	gint iAnimationDeltaT;
	/// derniere position en X du curseur dans le referentiel du dock.
	gint iMouseX;
	/// derniere position en Y du curseur dans le referentiel du dock.
	gint iMouseY;
	gdouble fRatio;
	gboolean bUseReflect;
	/// contexte OpenGL associe a la fenetre.
	GLXContext glContext;
	/// TRUE <=> une animation lente est en cours.
	gboolean bKeepSlowAnimation;
	/// L'icone volante.
	Icon *pIcon;
	/// compteur pour l'animation.
	gint iAnimationCount;
	gboolean bDrawHand;
};

gboolean cairo_dock_update_flying_container_notification (gpointer pUserData, CairoFlyingContainer *pFlyingContainer, gboolean *bContinueAnimation);

gboolean cairo_dock_render_flying_container_notification (gpointer pUserData, CairoFlyingContainer *pFlyingContainer, cairo_t *pCairoContext);


CairoFlyingContainer *cairo_dock_create_flying_container (Icon *pFlyingIcon, CairoDock *pOriginDock, gboolean bDrawHand);

void cairo_dock_drag_flying_container (CairoFlyingContainer *pFlyingContainer, CairoDock *pOriginDock);

void cairo_dock_free_flying_container (CairoFlyingContainer *pFlyingContainer);

void cairo_dock_terminate_flying_container (CairoFlyingContainer *pFlyingContainer);


G_END_DECLS
#endif
