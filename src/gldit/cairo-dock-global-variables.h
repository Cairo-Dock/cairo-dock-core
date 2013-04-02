/*
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
extern CairoContainer *g_pPrimaryContainer;
/// Chemin du fichier de conf de l'appli.
extern gchar *g_cConfFile;
/// Le chemin vers le repertoire racine.
extern gchar *g_cCairoDockDataDir;
extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentLaunchersPath;
/// version
extern int g_iMajorVersion, g_iMinorVersion, g_iMicroVersion;

/// boite
extern CairoDockImageBuffer g_pBoxAboveBuffer;
extern CairoDockImageBuffer g_pBoxBelowBuffer;
/// icon bg
extern CairoDockImageBuffer g_pIconBackgroundBuffer;

/// ecran
extern CairoDockDesktopGeometry g_desktopGeometry;

/// config opengl
extern CairoDockGLConfig g_openglConfig;

/// Environnement de bureau detecte.
extern CairoDockDesktopEnv g_iDesktopEnv;

extern gboolean g_bEasterEggs;

extern gboolean g_bUseOpenGL;

extern GLuint g_pGradationTexture[2];

extern CairoDockModuleInstance *g_pCurrentModule;

G_END_DECLS
#endif
