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

#include <stdlib.h>
#include <string.h>

#include "cairo-dock-draw.h"
#include "cairo-dock-icons.h"
#include "cairo-dock-callbacks.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-gui-factory.h"
#include "cairo-dock-internal-views.h"
#include "cairo-dock-internal-desklets.h"
#include "cairo-dock-internal-system.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dialog-manager.h"
#include "cairo-dock-container.h"
#include "cairo-dock-backends-manager.h"

extern gboolean g_bUseOpenGL;

static GHashTable *s_hRendererTable = NULL;  // table des rendus de dock.
static GHashTable *s_hDeskletRendererTable = NULL;  // table des rendus des desklets.
static GHashTable *s_hDialogRendererTable = NULL;  // table des rendus des dialogues.
static GHashTable *s_hDeskletDecorationsTable = NULL;  // table des decorations des desklets.
static GHashTable *s_hAnimationsTable = NULL;  // table des animations disponibles.
static GHashTable *s_hDialogDecoratorTable = NULL;  // table des decorateurs de dialogues disponibles.
static GHashTable *s_hDataRendererTable = NULL;  // table des rendus de donnees disponibles.
static GHashTable *s_hHidingEffectTable = NULL;  // table des effets de cachage des docks.
static GHashTable *s_hIconContainerTable = NULL;  // table des rendus d'icones de container.

typedef struct _CairoBackendMgr CairoBackendMgr;
struct _CairoBackendMgr {
	GHashTable *pTable;
	gpointer (*get_backend) (CairoBackendMgr *pBackendMgr, const gchar *cName);
	gpointer (*select_backend) (CairoBackendMgr *pBackendMgr, const gchar *cName, gpointer data);
	void (*register_backend) (CairoBackendMgr *pBackendMgr, const gchar *cName, gpointer pBackend);
	void (*remove_backend) (const gchar *cName);
	};

gpointer _get_backend (CairoBackendMgr *pBackendMgr, const gchar *cName)
{
	gpointer pBackend = NULL;
	if (cName != NULL)
		pBackend = g_hash_table_lookup (pBackendMgr->pTable, cName);
	return pBackend;
}

void _register_backend (CairoBackendMgr *pBackendMgr, const gchar *cName, gpointer pBackend)
{
	g_hash_table_insert (pBackendMgr->pTable, g_strdup (cName), pBackend);
}

void _remove_backend (CairoBackendMgr *pBackendMgr, const gchar *cName)
{
	g_hash_table_remove (pBackendMgr->pTable, cName);
}


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


CairoDataRendererNewFunc cairo_dock_get_data_renderer_entry_point (const gchar *cRendererName)
{
	if (cRendererName != NULL)
		return g_hash_table_lookup (s_hDataRendererTable, cRendererName);
	else
		return NULL;
}

void cairo_dock_register_data_renderer_entry_point (const gchar *cRendererName, CairoDataRendererNewFunc pFunc)
{
	cd_message ("%s (%s)", __func__, cRendererName);
	g_hash_table_insert (s_hDataRendererTable, g_strdup (cRendererName), pFunc);
}

void cairo_dock_remove_data_renderer_entry_point (const gchar *cRendererName)
{
	g_hash_table_remove (s_hDataRendererTable, cRendererName);
}


CairoDockHidingEffect *cairo_dock_get_hiding_effect (const gchar *cHidingEffect)
{
	if (cHidingEffect != NULL)
		return g_hash_table_lookup (s_hHidingEffectTable, cHidingEffect);
	else
		return NULL;
}

void cairo_dock_register_hiding_effect (const gchar *cHidingEffect, CairoDockHidingEffect *pEffect)
{
	cd_message ("%s (%s)", __func__, cHidingEffect);
	g_hash_table_insert (s_hHidingEffectTable, g_strdup (cHidingEffect), pEffect);
}

void cairo_dock_remove_hiding_effect (const gchar *cHidingEffect)
{
	g_hash_table_remove (s_hHidingEffectTable, cHidingEffect);
}


CairoIconContainerRenderer *cairo_dock_get_icon_container_renderer (const gchar *cRendererName)
{
	if (cRendererName != NULL)
		return g_hash_table_lookup (s_hIconContainerTable, cRendererName);
	else
		return NULL;
}

void cairo_dock_register_icon_container_renderer (const gchar *cRendererName, CairoIconContainerRenderer *pRenderer)
{
	cd_message ("%s (%s)", __func__, cRendererName);
	g_hash_table_insert (s_hIconContainerTable, g_strdup (cRendererName), pRenderer);
}

void cairo_dock_remove_icon_container_renderer (const gchar *cRendererName)
{
	g_hash_table_remove (s_hIconContainerTable, cRendererName);
}

void cairo_dock_foreach_icon_container_renderer (GHFunc pCallback, gpointer data)
{
	g_hash_table_foreach (s_hIconContainerTable, pCallback, data);
}


void cairo_dock_init_backends_manager (void)
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
		(GFreeFunc) cairo_dock_free_animation_record);
	
	s_hDialogDecoratorTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		g_free);
	
	s_hDataRendererTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		NULL);
	
	s_hHidingEffectTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		g_free);
	
	s_hIconContainerTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,
		g_free);
}


void cairo_dock_set_renderer (CairoDock *pDock, const gchar *cRendererName)
{
	g_return_if_fail (pDock != NULL);
	cd_message ("%s (%x:%s)", __func__, pDock, cRendererName);
	
	if (pDock->pRenderer && pDock->pRenderer->free_data)
	{
		pDock->pRenderer->free_data (pDock);
		pDock->pRendererData = NULL;
	}
	pDock->pRenderer = cairo_dock_get_renderer (cRendererName, (pDock->iRefCount == 0));
	
	pDock->fMagnitudeMax = 1.;
	pDock->container.bUseReflect = pDock->pRenderer->bUseReflect;
	gtk_widget_set_double_buffered (pDock->container.pWidget, ! (g_bUseOpenGL && pDock->pRenderer->render_opengl != NULL));
	
	int iAnimationDeltaT = pDock->container.iAnimationDeltaT;
	pDock->container.iAnimationDeltaT = (g_bUseOpenGL && pDock->pRenderer->render_opengl != NULL ? mySystem.iGLAnimationDeltaT : mySystem.iCairoAnimationDeltaT);
	if (pDock->container.iAnimationDeltaT == 0)
		pDock->container.iAnimationDeltaT = 30;  // le main dock est cree avant meme qu'on ait recuperer la valeur en conf. Lorsqu'une vue lui sera attribuee, la bonne valeur sera renseignee, en attendant on met un truc non nul.
	if (iAnimationDeltaT != pDock->container.iAnimationDeltaT && pDock->container.iSidGLAnimation != 0)
	{
		g_source_remove (pDock->container.iSidGLAnimation);
		pDock->container.iSidGLAnimation = 0;
		cairo_dock_launch_animation (CAIRO_CONTAINER (pDock));
	}
	if (/**cRendererName != NULL && */pDock->cRendererName != cRendererName)  // NULL ecrase le nom de l'ancienne vue depuis le 12/11/2009
	{
		g_free (pDock->cRendererName);
		pDock->cRendererName = g_strdup (cRendererName);
	}
}

void cairo_dock_set_default_renderer (CairoDock *pDock)
{
	g_return_if_fail (pDock != NULL);
	cairo_dock_set_renderer (pDock, (pDock->cRendererName != NULL ? pDock->cRendererName : NULL));  // NULL => laissera le champ cRendererName nul plutot que de mettre le nom de la vue par defaut.
}


void cairo_dock_set_desklet_renderer (CairoDesklet *pDesklet, CairoDeskletRenderer *pRenderer, gboolean bLoadIcons, CairoDeskletRendererConfigPtr pConfig)
{
	g_return_if_fail (pDesklet != NULL);
	
	if (pDesklet->pRenderer != NULL && pDesklet->pRenderer->free_data != NULL)  // meme si pDesklet->pRendererData == NULL.
	{
		pDesklet->pRenderer->free_data (pDesklet);
		pDesklet->pRendererData = NULL;
	}
	
	pDesklet->pRenderer = pRenderer;
	gtk_widget_set_double_buffered (pDesklet->container.pWidget, ! (g_bUseOpenGL && pRenderer != NULL && pRenderer->render_opengl != NULL));
	pDesklet->container.iAnimationDeltaT = (g_bUseOpenGL && pRenderer != NULL && pRenderer->render_opengl != NULL ? mySystem.iGLAnimationDeltaT : mySystem.iCairoAnimationDeltaT);
	
	if (pRenderer != NULL)
	{
		if (pRenderer->configure != NULL)
			pDesklet->pRendererData = pRenderer->configure (pDesklet, pConfig);
		
		if (bLoadIcons && pRenderer->load_icons != NULL)
			pRenderer->load_icons (pDesklet);
		
		if (pRenderer->load_data != NULL)
			pRenderer->load_data (pDesklet);
	}
}

void cairo_dock_set_desklet_renderer_by_name (CairoDesklet *pDesklet, const gchar *cRendererName, gboolean bLoadIcons, CairoDeskletRendererConfigPtr pConfig)
{
	cd_message ("%s (%s, %d)", __func__, cRendererName, bLoadIcons);
	CairoDeskletRenderer *pRenderer = (cRendererName != NULL ? cairo_dock_get_desklet_renderer (cRendererName) : NULL);
	
	cairo_dock_set_desklet_renderer (pDesklet, pRenderer, bLoadIcons, pConfig);
}


void cairo_dock_set_dialog_renderer (CairoDialog *pDialog, CairoDialogRenderer *pRenderer, CairoDialogRendererConfigPtr pConfig)
{
	g_return_if_fail (pDialog != NULL);
	
	if (pDialog->pRenderer != NULL && pDialog->pRenderer->free_data != NULL)
	{
		pDialog->pRenderer->free_data (pDialog);
		pDialog->pRendererData = NULL;
	}
	
	pDialog->pRenderer = pRenderer;
	
	if (pRenderer != NULL)
	{
		if (pRenderer->configure != NULL)
			pDialog->pRendererData = pRenderer->configure (pDialog, pConfig);
	}
}

void cairo_dock_set_dialog_renderer_by_name (CairoDialog *pDialog, const gchar *cRendererName, CairoDialogRendererConfigPtr pConfig)
{
	cd_message ("%s (%s)", __func__, cRendererName);
	CairoDialogRenderer *pRenderer = (cRendererName != NULL ? cairo_dock_get_dialog_renderer (cRendererName) : NULL);
	
	cairo_dock_set_dialog_renderer (pDialog, pRenderer, pConfig);
}


void cairo_dock_render_desklet_with_new_data (CairoDesklet *pDesklet, CairoDeskletRendererDataPtr pNewData)
{
	if (pDesklet->pRenderer != NULL && pDesklet->pRenderer->update != NULL)
		pDesklet->pRenderer->update (pDesklet, pNewData);
	
	gtk_widget_queue_draw (pDesklet->container.pWidget);
}

void cairo_dock_render_dialog_with_new_data (CairoDialog *pDialog, CairoDialogRendererDataPtr pNewData)
{
	if (pDialog->pRenderer != NULL && pDialog->pRenderer->update != NULL)
		pDialog->pRenderer->update (pDialog, pNewData);
	
	if (pDialog->pInteractiveWidget != NULL)
		cairo_dock_damage_interactive_widget_dialog (pDialog);
	else
		gtk_widget_queue_draw (pDialog->container.pWidget);
}


CairoDialogDecorator *cairo_dock_get_dialog_decorator (const gchar *cDecoratorName)
{
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
int cairo_dock_register_animation (const gchar *cAnimation, const gchar *cDisplayedName)
{
	cd_message ("%s (%s)", __func__, cAnimation);
	iNbAnimation ++;
	CairoDockAnimationRecord *pRecord = g_new (CairoDockAnimationRecord, 1);
	pRecord->id = iNbAnimation;
	pRecord->cDisplayedName = cDisplayedName;
	g_hash_table_insert (s_hAnimationsTable, g_strdup (cAnimation), pRecord);
	return iNbAnimation;
}

void cairo_dock_free_animation_record (CairoDockAnimationRecord *pRecord)
{
	g_free (pRecord);
}

int cairo_dock_get_animation_id (const gchar *cAnimation)
{
	g_return_val_if_fail (cAnimation != NULL, 0);
	CairoDockAnimationRecord *pRecord = g_hash_table_lookup (s_hAnimationsTable, cAnimation);
	return (pRecord ? pRecord->id : 0);
}

const gchar *cairo_dock_get_animation_displayed_name (const gchar *cAnimation)
{
	g_return_val_if_fail (cAnimation != NULL, NULL);
	CairoDockAnimationRecord *pRecord = g_hash_table_lookup (s_hAnimationsTable, cAnimation);
	return (pRecord ? pRecord->cDisplayedName : NULL);
}

void cairo_dock_unregister_animation (const gchar *cAnimation)
{
	g_hash_table_remove (s_hAnimationsTable, cAnimation);
}
