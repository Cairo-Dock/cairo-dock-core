
#ifndef __CAIRO_DOCK_RENDERER_MANAGER__
#define  __CAIRO_DOCK_RENDERER_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-desklet.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-data-renderer.h"
G_BEGIN_DECLS


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

CairoDataRendererInitFunc cairo_dock_get_data_renderer_entry_point (const gchar *cRendererName);
void cairo_dock_register_data_renderer_entry_point (const gchar *cRendererName, CairoDataRendererInitFunc *pFunc);
void cairo_dock_remove_data_renderer_entry_point (const gchar *cRendererName);


void cairo_dock_initialize_renderer_manager (void);

void cairo_dock_set_renderer (CairoDock *pDock, const gchar *cRendererName);
void cairo_dock_set_default_renderer (CairoDock *pDock);

void cairo_dock_set_desklet_renderer (CairoDesklet *pDesklet, CairoDeskletRenderer *pRenderer, cairo_t *pSourceContext, gboolean bLoadIcons, CairoDeskletRendererConfigPtr pConfig);
void cairo_dock_set_desklet_renderer_by_name (CairoDesklet *pDesklet, const gchar *cRendererName, cairo_t *pSourceContext, gboolean bLoadIcons, CairoDeskletRendererConfigPtr pConfig);

void cairo_dock_set_dialog_renderer (CairoDialog *pDialog, CairoDialogRenderer *pRenderer, cairo_t *pSourceContext, CairoDialogRendererConfigPtr pConfig);
void cairo_dock_set_dialog_renderer_by_name (CairoDialog *pDialog, const gchar *cRendererName, cairo_t *pSourceContext, CairoDialogRendererConfigPtr pConfig);

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


int cairo_dock_register_animation (const gchar *cAnimation);
int cairo_dock_get_animation_id (const gchar *cAnimation);
void cairo_dock_unregister_animation (const gchar *cAnimation);


#define CAIRO_CONTAINER_IS_OPENGL(pContainer) (g_bUseOpenGL && ((CAIRO_DOCK_IS_DOCK (pContainer) && CAIRO_DOCK (pContainer)->render_opengl) || (CAIRO_DOCK_IS_DESKLET (pContainer) && CAIRO_DESKLET (pContainer)->pRenderer && CAIRO_DESKLET (pContainer)->pRenderer->render_opengl)))
#define CAIRO_DOCK_CONTAINER_IS_OPENGL CAIRO_CONTAINER_IS_OPENGL

G_END_DECLS
#endif
