/******************************************************************************

This file is a part of the cairo-dock program, 
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

******************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-default-view.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-background.h"
#include "cairo-dock-internal-desklets.h"
#include "cairo-dock-dialogs.h"
#include "cairo-dock-renderer-manager.h"

extern gboolean g_bUseOpenGL;

static GHashTable *s_hRendererTable = NULL;  // table des rendus de dock.
static GHashTable *s_hDeskletRendererTable = NULL;  // table des rendus des desklets.
static GHashTable *s_hDialogRendererTable = NULL;  // table des rendus des dialogues.
static GHashTable *s_hDeskletDecorationsTable = NULL;  // table des decorations des desklets.
static GHashTable *s_hAnimationsTable = NULL;  // table des animations disponibles.
static GHashTable *s_hDialogDecoratorTable = NULL;  // table des decorateurs de dialogues disponibles.

CairoDockRenderer *cairo_dock_get_renderer (const gchar *cRendererName, gboolean bForMainDock)
{
	//g_print ("%s (%s, %d)\n", __func__, cRendererName, bForMainDock);
	CairoDockRenderer *pRenderer = NULL;
	if (cRendererName != NULL)
		pRenderer = g_hash_table_lookup (s_hRendererTable, cRendererName);
	
	if (pRenderer == NULL)
	{
		const gchar *cDefaultRendererName = (bForMainDock ? myViews.cMainDockDefaultRendererName : myViews.cSubDockDefaultRendererName);
		//g_print ("  cDefaultRendererName : %s\n", cDefaultRendererName);
		if (cDefaultRendererName != NULL)
			pRenderer = g_hash_table_lookup (s_hRendererTable, cDefaultRendererName);
	}
	
	if (pRenderer == NULL)
		pRenderer = g_hash_table_lookup (s_hRendererTable, CAIRO_DOCK_DEFAULT_RENDERER_NAME);
	
	return pRenderer;
}

void cairo_dock_register_renderer (const gchar *cRendererName, CairoDockRenderer *pRenderer)
{
	cd_message ("%s (%s)", __func__, cRendererName);
	g_hash_table_insert (s_hRendererTable, g_strdup (cRendererName), pRenderer);
}

void cairo_dock_remove_renderer (const gchar *cRendererName)
{
	g_hash_table_remove (s_hRendererTable, cRendererName);
}


CairoDeskletRenderer *cairo_dock_get_desklet_renderer (const gchar *cRendererName)
{
	if (cRendererName != NULL)
		return g_hash_table_lookup (s_hDeskletRendererTable, cRendererName);
	else
		return NULL;
}

void cairo_dock_register_desklet_renderer (const gchar *cRendererName, CairoDeskletRenderer *pRenderer)
{
	cd_message ("%s (%s)", __func__, cRendererName);
	g_hash_table_insert (s_hDeskletRendererTable, g_strdup (cRendererName), pRenderer);
}

void cairo_dock_remove_desklet_renderer (const gchar *cRendererName)
{
	g_hash_table_remove (s_hDeskletRendererTable, cRendererName);
}

void cairo_dock_predefine_desklet_renderer_config (CairoDeskletRenderer *pRenderer, const gchar *cConfigName, CairoDeskletRendererConfigPtr pConfig)
{
	g_return_if_fail (cConfigName != NULL && pConfig != NULL);
	CairoDeskletRendererPreDefinedConfig *pPreDefinedConfig = g_new (CairoDeskletRendererPreDefinedConfig, 1);
	pPreDefinedConfig->cName = g_strdup (cConfigName);
	pPreDefinedConfig->pConfig = pConfig;
	pRenderer->pPreDefinedConfigList = g_list_prepend (pRenderer->pPreDefinedConfigList, pPreDefinedConfig);
}

CairoDeskletRendererConfigPtr cairo_dock_get_desklet_renderer_predefined_config (const gchar *cRendererName, const gchar *cConfigName)
{
	CairoDeskletRenderer *pRenderer = cairo_dock_get_desklet_renderer (cRendererName);
	g_return_val_if_fail (pRenderer != NULL && cConfigName != NULL, NULL);
	
	GList *c;
	CairoDeskletRendererPreDefinedConfig *pPreDefinedConfig;
	for (c = pRenderer->pPreDefinedConfigList; c != NULL; c = c->next)
	{
		pPreDefinedConfig = c->data;
		if (strcmp (pPreDefinedConfig->cName, cConfigName) == 0)
			return pPreDefinedConfig->pConfig;
	}
	return NULL;
}


CairoDialogRenderer *cairo_dock_get_dialog_renderer (const gchar *cRendererName)
{
	if (cRendererName != NULL)
		return g_hash_table_lookup (s_hDialogRendererTable, cRendererName);
	else
		return NULL;
}

void cairo_dock_register_dialog_renderer (const gchar *cRendererName, CairoDialogRenderer *pRenderer)
{
	cd_message ("%s (%s)", __func__, cRendererName);
	g_hash_table_insert (s_hDialogRendererTable, g_strdup (cRendererName), pRenderer);
}

void cairo_dock_remove_dialog_renderer (const gchar *cRendererName)
{
	g_hash_table_remove (s_hDialogRendererTable, cRendererName);
}


CairoDeskletDecoration *cairo_dock_get_desklet_decoration (const gchar *cDecorationName)
{
	cd_debug ("%s (%s)", __func__, cDecorationName);
	if (cDecorationName != NULL)
		return g_hash_table_lookup (s_hDeskletDecorationsTable, cDecorationName);
	else if (myDesklets.cDeskletDecorationsName != NULL)
		return g_hash_table_lookup (s_hDeskletDecorationsTable, myDesklets.cDeskletDecorationsName);
	else
		return NULL;
}

void cairo_dock_register_desklet_decoration (const gchar *cDecorationName, CairoDeskletDecoration *pDecoration)
{
	cd_message ("%s (%s)", __func__, cDecorationName);
	g_hash_table_insert (s_hDeskletDecorationsTable, g_strdup (cDecorationName), pDecoration);
}

void cairo_dock_remove_desklet_decoration (const gchar *cDecorationName)
{
	g_hash_table_remove (s_hDeskletDecorationsTable, cDecorationName);
}

void cairo_dock_free_desklet_decoration (CairoDeskletDecoration *pDecoration)
{
	if (pDecoration == NULL)
		return ;
	g_free (pDecoration->cBackGroundImagePath);
	g_free (pDecoration->cForeGroundImagePath);
	g_free (pDecoration);
}


void cairo_dock_initialize_renderer_manager (void)
{
	g_return_if_fail (s_hRendererTable == NULL);
	cd_message ("");
	
	s_hRendererTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		g_free);
	
	s_hDeskletRendererTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		g_free);
	
	s_hDialogRendererTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		g_free);
	
	s_hDeskletDecorationsTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		(GFreeFunc) cairo_dock_free_desklet_decoration);
	
	s_hAnimationsTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		NULL);
	
	s_hDialogDecoratorTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		NULL);
	
	cairo_dock_register_default_renderer ();
	CairoDialogDecorator *pDecorator = g_new (CairoDialogDecorator, 1);
	pDecorator->set_size = cairo_dock_set_frame_size_comics;
	pDecorator->render = cairo_dock_draw_decorations_comics;
	cairo_dock_register_dialog_decorator ("comics", pDecorator);
	
	pDecorator = g_new (CairoDialogDecorator, 1);
	pDecorator->set_size = cairo_dock_set_frame_size_modern;
	pDecorator->render = cairo_dock_draw_decorations_modern;
	cairo_dock_register_dialog_decorator ("modern", pDecorator);
	
	pDecorator = g_new (CairoDialogDecorator, 1);
	pDecorator->set_size = cairo_dock_set_frame_size_3Dplane;
	pDecorator->render = cairo_dock_draw_decorations_3Dplane;
	cairo_dock_register_dialog_decorator ("3D", pDecorator);
	
	pDecorator = g_new (CairoDialogDecorator, 1);
	pDecorator->set_size = cairo_dock_set_frame_size_tooltip;
	pDecorator->render = cairo_dock_draw_decorations_tooltip;
	cairo_dock_register_dialog_decorator ("tooltip", pDecorator);
}


void cairo_dock_set_renderer (CairoDock *pDock, const gchar *cRendererName)
{
	g_return_if_fail (pDock != NULL);
	cd_message ("%s (%s)", __func__, cRendererName);
	CairoDockRenderer *pRenderer = cairo_dock_get_renderer (cRendererName, (pDock->iRefCount == 0));
	
	pDock->calculate_max_dock_size = pRenderer->calculate_max_dock_size;
	pDock->calculate_icons = pRenderer->calculate_icons;
	pDock->render = pRenderer->render;
	pDock->render_optimized = pRenderer->render_optimized;
	pDock->render_opengl = pRenderer->render_opengl;
	gtk_widget_set_double_buffered (pDock->pWidget, ! (g_bUseOpenGL && pDock->render_opengl != NULL));
	pDock->set_subdock_position = pRenderer->set_subdock_position;
	pDock->bUseReflect = pRenderer->bUseReflect;
	pDock->bUseStencil = pRenderer->bUseStencil;
	if (cRendererName != NULL)  // NULL n'ecrase pas le nom de l'ancienne vue.
		pDock->cRendererName = g_strdup (cRendererName);
}

void cairo_dock_set_default_renderer (CairoDock *pDock)
{
	g_return_if_fail (pDock != NULL);
	cairo_dock_set_renderer (pDock, (pDock->cRendererName != NULL ? pDock->cRendererName : NULL));  // NULL => laissera le champ cRendererName nul plutot que de mettre le nom de la vue par defaut.
}


void cairo_dock_set_desklet_renderer (CairoDesklet *pDesklet, CairoDeskletRenderer *pRenderer, cairo_t *pSourceContext, gboolean bLoadIcons, CairoDeskletRendererConfigPtr pConfig)
{
	g_return_if_fail (pDesklet != NULL);
	cd_debug ("%s (%x)", __func__, pRenderer);
	
	if (pDesklet->pRenderer != NULL && pDesklet->pRenderer->free_data != NULL)  // meme si pDesklet->pRendererData == NULL.
	{
		pDesklet->pRenderer->free_data (pDesklet);
		pDesklet->pRendererData = NULL;
	}
	
	pDesklet->pRenderer = pRenderer;
	gtk_widget_set_double_buffered (pDesklet->pWidget, ! (g_bUseOpenGL && pRenderer != NULL && pRenderer->render_opengl != NULL));
	
	if (pRenderer != NULL)
	{
		cairo_t *pCairoContext = (pSourceContext != NULL ? pSourceContext : cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDesklet)));
		
		if (pRenderer->configure != NULL)
			pDesklet->pRendererData = pRenderer->configure (pDesklet, pCairoContext, pConfig);
		
		if (bLoadIcons && pRenderer->load_icons != NULL)
			pRenderer->load_icons (pDesklet, pCairoContext);
		
		if (pRenderer->load_data != NULL)
			pRenderer->load_data (pDesklet, pCairoContext);
		
		if (pSourceContext == NULL)
			cairo_destroy (pCairoContext);
	}
}

void cairo_dock_set_desklet_renderer_by_name (CairoDesklet *pDesklet, const gchar *cRendererName, cairo_t *pSourceContext, gboolean bLoadIcons, CairoDeskletRendererConfigPtr pConfig)
{
	cd_message ("%s (%s, %d)", __func__, cRendererName, bLoadIcons);
	CairoDeskletRenderer *pRenderer = (cRendererName != NULL ? cairo_dock_get_desklet_renderer (cRendererName) : NULL);
	
	cairo_dock_set_desklet_renderer (pDesklet, pRenderer, pSourceContext, bLoadIcons, pConfig);
}


void cairo_dock_set_dialog_renderer (CairoDialog *pDialog, CairoDialogRenderer *pRenderer, cairo_t *pSourceContext, CairoDialogRendererConfigPtr pConfig)
{
	g_return_if_fail (pDialog != NULL);
	cd_debug ("%s (%x)", __func__, pRenderer);
	
	if (pDialog->pRenderer != NULL && pDialog->pRenderer->free_data != NULL)
	{
		pDialog->pRenderer->free_data (pDialog);
		pDialog->pRendererData = NULL;
	}
	
	pDialog->pRenderer = pRenderer;
	
	if (pRenderer != NULL)
	{
		cairo_t *pCairoContext = (pSourceContext != NULL ? pSourceContext : cairo_dock_create_context_from_window (CAIRO_CONTAINER (pDialog)));
		
		if (pRenderer->configure != NULL)
			pDialog->pRendererData = pRenderer->configure (pDialog, pCairoContext, pConfig);
		
		if (pSourceContext == NULL)
			cairo_destroy (pCairoContext);
	}
}

void cairo_dock_set_dialog_renderer_by_name (CairoDialog *pDialog, const gchar *cRendererName, cairo_t *pSourceContext, CairoDialogRendererConfigPtr pConfig)
{
	cd_message ("%s (%s)", __func__, cRendererName);
	CairoDialogRenderer *pRenderer = (cRendererName != NULL ? cairo_dock_get_dialog_renderer (cRendererName) : NULL);
	
	cairo_dock_set_dialog_renderer (pDialog, pRenderer, pSourceContext, pConfig);
}


void cairo_dock_render_desklet_with_new_data (CairoDesklet *pDesklet, CairoDeskletRendererDataPtr pNewData)
{
	if (pDesklet->pRenderer != NULL && pDesklet->pRenderer->update != NULL)
		pDesklet->pRenderer->update (pDesklet, pNewData);
	
	gtk_widget_queue_draw_area (pDesklet->pWidget,
		.5*myBackground.iDockRadius,
		.5*myBackground.iDockRadius,
		pDesklet->iWidth - myBackground.iDockRadius,
		pDesklet->iHeight- myBackground.iDockRadius);  // marche avec glitz ?...
}

void cairo_dock_render_dialog_with_new_data (CairoDialog *pDialog, CairoDialogRendererDataPtr pNewData)
{
	if (pDialog->pRenderer != NULL && pDialog->pRenderer->update != NULL)
		pDialog->pRenderer->update (pDialog, pNewData);
	
	if (pDialog->pInteractiveWidget != NULL)
		cairo_dock_damage_interactive_widget_dialog (pDialog);
	else
		gtk_widget_queue_draw (pDialog->pWidget);
}


CairoDialogDecorator *cairo_dock_get_dialog_decorator (const gchar *cDecoratorName)
{
	cd_debug ("%s (%s)", __func__, cDecoratorName);
	CairoDialogDecorator *pDecorator = NULL;
	if (cDecoratorName != NULL)
		pDecorator = g_hash_table_lookup (s_hDialogDecoratorTable, cDecoratorName); 
	return pDecorator;
}

void cairo_dock_register_dialog_decorator (const gchar *cDecoratorName, CairoDialogDecorator *pDecorator)
{
	cd_message ("%s (%s)", __func__, cDecoratorName);
	g_hash_table_insert (s_hDialogDecoratorTable, g_strdup (cDecoratorName), pDecorator);
}

void cairo_dock_remove_dialog_decorator (const gchar *cDecoratorName)
{
	g_hash_table_remove (s_hDialogDecoratorTable, cDecoratorName);
}

void cairo_dock_set_dialog_decorator (CairoDialog *pDialog, CairoDialogDecorator *pDecorator)
{
	pDialog->pDecorator = pDecorator;
}
void cairo_dock_set_dialog_decorator_by_name (CairoDialog *pDialog, const gchar *cDecoratorName)
{
	cd_message ("%s (%s)", __func__, cDecoratorName);
	CairoDialogDecorator *pDecorator = cairo_dock_get_dialog_decorator (cDecoratorName);
	
	cairo_dock_set_dialog_decorator (pDialog, pDecorator);
}


void cairo_dock_update_renderer_list_for_gui (void)
{
	cairo_dock_build_renderer_list_for_gui (s_hRendererTable);
}

void cairo_dock_update_desklet_decorations_list_for_gui (void)
{
	cairo_dock_build_desklet_decorations_list_for_gui (s_hDeskletDecorationsTable);
}

void cairo_dock_update_desklet_decorations_list_for_applet_gui (void)
{
	cairo_dock_build_desklet_decorations_list_for_applet_gui (s_hDeskletDecorationsTable);
}

void cairo_dock_update_animations_list_for_gui (void)
{
	cairo_dock_build_animations_list_for_gui (s_hAnimationsTable);
}

void cairo_dock_update_dialog_decorator_list_for_gui (void)
{
	cairo_dock_build_dialog_decorator_list_for_gui (s_hDialogDecoratorTable);
}


static int iNbAnimation = 0;
int cairo_dock_register_animation (const gchar *cAnimation)
{
	cd_message ("%s (%s)", __func__, cAnimation);
	iNbAnimation ++;
	g_hash_table_insert (s_hAnimationsTable, g_strdup (cAnimation), GINT_TO_POINTER (iNbAnimation));
	return iNbAnimation;
}

int cairo_dock_get_animation_id (const gchar *cAnimation)
{
	g_return_val_if_fail (cAnimation != NULL, 0);
	return GPOINTER_TO_INT (g_hash_table_lookup (s_hAnimationsTable, cAnimation));
}

void cairo_dock_unregister_animation (const gchar *cAnimation)
{
	g_hash_table_remove (s_hAnimationsTable, cAnimation);
}
