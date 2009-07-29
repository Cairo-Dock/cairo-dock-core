#ifndef __CAIRO_DOCK_GLOBAL_VARIABLES_H__
#define __CAIRO_DOCK_GLOBAL_VARIABLES_H__

#include "cairo-dock-struct.h"
G_BEGIN_DECLS

/// Pointeur sur le dock principal.
extern CairoDock *g_pMainDock;
/// Chemin du fichier de conf de l'appli.
extern gchar *g_cConfFile;
/// Le chemin vers le repertoire racine.
extern gchar *g_cCairoDockDataDir;
/// Le chemin vers le repertoire du theme courant.
extern gchar *g_cCurrentThemePath;
/// Le chemin vers le repertoire des lanceurs/icones du theme courant.
extern gchar *g_cCurrentLaunchersPath;

/// Dimensions de l'ecran en mode horizontal/vertical.
extern int g_iScreenWidth[2], g_iScreenHeight[2];
/// Dimensions de l'ecran logique en mode horizontal/vertical.
extern int g_iXScreenWidth[2], g_iXScreenHeight[2];
/// Nombre de bureaux virtuels.
extern int g_iNbDesktops;
/// Nombre de "faces" du cube en largeur et en hauteur.
extern int g_iNbViewportX, g_iNbViewportY ;

/// Largeur de la zone de rappel.
extern int g_iVisibleZoneWidth;
/// hauteur de la zone de rappel.
extern int g_iVisibleZoneHeight;
/// Surface de la zone de rappel.
extern cairo_surface_t *g_pVisibleZoneSurface;
extern double g_fVisibleZoneImageWidth, g_fVisibleZoneImageHeight;

/// Environnement de bureau detecte.
extern CairoDockDesktopEnv g_iDesktopEnv;

extern cairo_surface_t *g_pBackgroundSurfaceFull;
extern cairo_surface_t *g_pBackgroundSurface;

extern gboolean g_bEasterEggs;

extern GLuint g_iBackgroundTexture;
extern gboolean g_bUseOpenGL;

extern GLuint g_pGradationTexture[2];
extern GLuint g_iIndicatorTexture;
extern double g_fIndicatorWidth, g_fIndicatorHeight;

G_END_DECLS
#endif
