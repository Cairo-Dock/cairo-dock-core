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


#ifndef __CAIRO_DOCK_BACKENDS_MANAGER__
#define  __CAIRO_DOCK_BACKENDS_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dialog-factory.h"
#include "cairo-dock-data-renderer.h"
G_BEGIN_DECLS

struct _CairoDockAnimationRecord {
	gint id;
	const gchar *cDisplayedName;
	gboolean bIsEffect;
	};

struct _CairoDockDataRendererRecord {
	CairoDataRendererInterface interface;
	gulong iStructSize;
	const gchar *cThemeDirName;
	const gchar *cDefaultTheme;
	};

CairoDockRenderer *cairo_dock_get_renderer (const gchar *cRendererName, gboolean bForMainDock);
void cairo_dock_register_renderer (const gchar *cRendererName, CairoDockRenderer *pRenderer);
void cairo_dock_remove_renderer (const gchar *cRendererName);

CairoDeskletRenderer *cairo_dock_get_desklet_renderer (const gchar *cRendererName);
void cairo_dock_register_desklet_renderer (const gchar *cRendererName, CairoDeskletRenderer *pRenderer);
void cairo_dock_remove_desklet_renderer (const gchar *cRendererName);
void cairo_dock_predefine_desklet_renderer_config (CairoDeskletRenderer *pRenderer, const gchar *cConfigName, CairoDeskletRendererConfigPtr pConfig);
CairoDeskletRendererConfigPtr cairo_dock_get_desklet_renderer_predefined_config (const gchar *cRendererName, const gchar *cConfigName);

CairoDialogRenderer *cairo_dock_get_dialog_renderer (const gchar *cRendererName);
void cairo_dock_register_dialog_renderer (const gchar *cRendererName, CairoDialogRenderer *pRenderer);
void cairo_dock_remove_dialog_renderer (const gchar *cRendererName);

CairoDeskletDecoration *cairo_dock_get_desklet_decoration (const gchar *cDecorationName);
void cairo_dock_register_desklet_decoration (const gchar *cDecorationName, CairoDeskletDecoration *pDecoration);
void cairo_dock_remove_desklet_decoration (const gchar *cDecorationName);

CairoDockDataRendererRecord *cairo_dock_get_data_renderer_record (const gchar *cRendererName);
void cairo_dock_register_data_renderer (const gchar *cRendererName, CairoDockDataRendererRecord *pRecord);
void cairo_dock_remove_data_renderer_entry_point (const gchar *cRendererName);

CairoDockHidingEffect *cairo_dock_get_hiding_effect (const gchar *cHidingEffect);
void cairo_dock_register_hiding_effect (const gchar *cHidingEffect, CairoDockHidingEffect *pEffect);
void cairo_dock_remove_hiding_effect (const gchar *cHidingEffect);

CairoIconContainerRenderer *cairo_dock_get_icon_container_renderer (const gchar *cRendererName);
void cairo_dock_register_icon_container_renderer (const gchar *cRendererName, CairoIconContainerRenderer *pRenderer);
void cairo_dock_remove_icon_container_renderer (const gchar *cRendererName);
void cairo_dock_foreach_icon_container_renderer (GHFunc pCallback, gpointer data);

void cairo_dock_init_backends_manager (void);

void cairo_dock_set_renderer (CairoDock *pDock, const gchar *cRendererName);
void cairo_dock_set_default_renderer (CairoDock *pDock);

void cairo_dock_set_desklet_renderer (CairoDesklet *pDesklet, CairoDeskletRenderer *pRenderer, CairoDeskletRendererConfigPtr pConfig);
void cairo_dock_set_desklet_renderer_by_name (CairoDesklet *pDesklet, const gchar *cRendererName, CairoDeskletRendererConfigPtr pConfig);

void cairo_dock_set_dialog_renderer (CairoDialog *pDialog, CairoDialogRenderer *pRenderer, CairoDialogRendererConfigPtr pConfig);
void cairo_dock_set_dialog_renderer_by_name (CairoDialog *pDialog, const gchar *cRendererName, CairoDialogRendererConfigPtr pConfig);

CairoDialogDecorator *cairo_dock_get_dialog_decorator (const gchar *cDecoratorName);
void cairo_dock_register_dialog_decorator (const gchar *cDecoratorName, CairoDialogDecorator *pDecorator);
void cairo_dock_remove_dialog_decorator (const gchar *cDecoratorName);
void cairo_dock_set_dialog_decorator (CairoDialog *pDialog, CairoDialogDecorator *pDecorator);
void cairo_dock_set_dialog_decorator_by_name (CairoDialog *pDialog, const gchar *cDecoratorName);


void cairo_dock_render_desklet_with_new_data (CairoDesklet *pDesklet, CairoDeskletRendererDataPtr pNewData);
void cairo_dock_render_dialog_with_new_data (CairoDialog *pDialog, CairoDialogRendererDataPtr pNewData);


void cairo_dock_update_renderer_list_for_gui (void);
void cairo_dock_update_desklet_decorations_list_for_gui (void);
void cairo_dock_update_desklet_decorations_list_for_applet_gui (void);
void cairo_dock_update_animations_list_for_gui (void);
void cairo_dock_update_dialog_decorator_list_for_gui (void);


int cairo_dock_register_animation (const gchar *cAnimation, const gchar *cDisplayedName, gboolean bIsEffect);
void cairo_dock_free_animation_record (CairoDockAnimationRecord *pRecord);
int cairo_dock_get_animation_id (const gchar *cAnimation);
const gchar *cairo_dock_get_animation_displayed_name (const gchar *cAnimation);
void cairo_dock_unregister_animation (const gchar *cAnimation);
void cairo_dock_foreach_animation (GHFunc pHFunction, gpointer data);


#define CAIRO_CONTAINER_IS_OPENGL(pContainer) (g_bUseOpenGL && ((CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->pRenderer->render_opengl) || (CAIRO_DOCK_IS_DESKLET (pContainer) && CAIRO_DESKLET (pContainer)->pRenderer && CAIRO_DESKLET (pContainer)->pRenderer->render_opengl)))
#define CAIRO_DOCK_CONTAINER_IS_OPENGL CAIRO_CONTAINER_IS_OPENGL

G_END_DECLS
#endif
