/**
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CAIRO_DOCK_GLOBAL_VARIABLES_H__
#define __CAIRO_DOCK_GLOBAL_VARIABLES_H__

#include "cairo-dock-struct.h"
#include "cairo-dock-file-manager.h"  // pour g_iDesktopEnv
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
