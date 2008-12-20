#ifndef __CAIRO_DOCK_GLOBAL_VARIABLES_H__
#define __CAIRO_DOCK_GLOBAL_VARIABLES_H__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/// Pointeur sur le dock principal.
extern CairoDock *g_pMainDock;
/// Largeur de l'ecran en mode horizontal/vertical.
extern gint g_iScreenWidth[2];
/// Hauteur de l'ecran en mode horizontal/vertical.
extern gint g_iScreenHeight[2];
/// Nombre de bureaux virtuels.
extern int g_iNbDesktops;
extern int g_iNbViewportX;
extern int g_iNbViewportY;
/// Nombre de "faces" du cube en largeur et en hauteur.
//extern int g_iNbViewportX, g_iNbViewportY ;
/// Chemin du fichier de conf de l'appli.
extern gchar *g_cConfFile;

/// Largeur de la zone de rappel.
extern int g_iVisibleZoneWidth;
/// hauteur de la zone de rappel.
extern int g_iVisibleZoneHeight;

/// Le chemin vers le repertoire racine.
extern gchar *g_cCairoDockDataDir;
/// Le chemin vers le repertoire du theme courant.
extern gchar *g_cCurrentThemePath;
/// Le chemin vers le repertoire des lanceurs/icones du theme courant.
extern gchar *g_cCurrentLaunchersPath;

/// Surface de la zone de rappel.
extern cairo_surface_t *g_pVisibleZoneSurface;
extern double g_fVisibleZoneImageWidth, g_fVisibleZoneImageHeight;

extern GLuint g_pGradationTexture[2];

/// Environnement de bureau detecte.
extern CairoDockDesktopEnv g_iDesktopEnv;

extern cairo_surface_t *g_pBackgroundSurfaceFull;
extern cairo_surface_t *g_pBackgroundSurface;

extern gboolean g_bEasterEggs;

extern int g_iBackgroundTexture;
extern gboolean g_bUseOpenGL;
extern gdouble g_iGLAnimationDeltaT, g_iCairoAnimationDeltaT;

G_END_DECLS
#endif
