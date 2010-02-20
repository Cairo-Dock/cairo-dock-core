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

#include <gdk/gdkx.h>

#include "cairo-dock-modules.h"
#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-container.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-internal-accessibility.h"
#include "cairo-dock-internal-indicators.h"
#include "cairo-dock-internal-icons.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-taskbar.h"

CairoConfigTaskBar myTaskBar;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigTaskBar *pTaskBar)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	// comportement
	pTaskBar->bShowAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "show applications", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	
	pTaskBar->bAppliOnCurrentDesktopOnly = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "current desktop only", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	
	pTaskBar->bMixLauncherAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "mix launcher appli", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	pTaskBar->bGroupAppliByClass = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "group by class", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	pTaskBar->cGroupException = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "group exception", &bFlushConfFileNeeded, "pidgin;xchat", NULL, NULL);
	if (pTaskBar->cGroupException)
	{
		int i;
		for (i = 0; pTaskBar->cGroupException[i] != '\0'; i ++)  // on passe tout en minuscule.
			pTaskBar->cGroupException[i] = g_ascii_tolower (pTaskBar->cGroupException[i]);
	}
	
	pTaskBar->bHideVisibleApplis = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "hide visible", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	
	
	// representation
	pTaskBar->bDrawIndicatorOnAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "indic on appli", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	pTaskBar->bOverWriteXIcons = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "overwrite xicon", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	pTaskBar->cOverwriteException = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "overwrite exception", &bFlushConfFileNeeded, "pidgin;xchat", NULL, NULL);
	if (pTaskBar->cOverwriteException)
	{
		int i;
		for (i = 0; pTaskBar->cOverwriteException[i] != '\0'; i ++)
			pTaskBar->cOverwriteException[i] = g_ascii_tolower (pTaskBar->cOverwriteException[i]);
	}
	
	///pTaskBar->bShowThumbnail = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "window thumbnail", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	pTaskBar->iMinimizedWindowRenderType = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "minimized", &bFlushConfFileNeeded, -1, NULL, NULL);
	if (pTaskBar->iMinimizedWindowRenderType == -1)  // anciens parametres.
	{
		gboolean bShowThumbnail = g_key_file_get_boolean (pKeyFile, "TaskBar", "window thumbnail", NULL);
		if (bShowThumbnail)
			pTaskBar->iMinimizedWindowRenderType = 1;
		else
			pTaskBar->iMinimizedWindowRenderType = 0;
		g_key_file_set_integer (pKeyFile, "TaskBar", "minimized", pTaskBar->iMinimizedWindowRenderType);
	}
	
	if (pTaskBar->iMinimizedWindowRenderType == 1 && ! cairo_dock_xcomposite_is_available ())
	{
		cd_warning ("Sorry but either your X server does not have the XComposite extension, or your version of Cairo-Dock was not built with the support of XComposite.\n You can't have window thumbnails in the dock");
		pTaskBar->iMinimizedWindowRenderType = 0;
	}
	if (pTaskBar->iMinimizedWindowRenderType == 0)
		pTaskBar->fVisibleAppliAlpha = MIN (.6, cairo_dock_get_double_key_value (pKeyFile, "TaskBar", "visibility alpha", &bFlushConfFileNeeded, .35, "Applications", NULL));
	
	pTaskBar->iAppliMaxNameLength = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "max name length", &bFlushConfFileNeeded, 15, "Applications", NULL);
	
	
	// interaction
	pTaskBar->bMinimizeOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "minimize on click", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	pTaskBar->bCloseAppliOnMiddleClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "close on middle click", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	
	pTaskBar->bDemandsAttentionWithDialog = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "demands attention with dialog", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	pTaskBar->iDialogDuration = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "duration", &bFlushConfFileNeeded, 2, NULL, NULL);
	pTaskBar->cAnimationOnDemandsAttention = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "animation on demands attention", &bFlushConfFileNeeded, "fire", NULL, NULL);
	gchar *cForceDemandsAttention = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "force demands attention", &bFlushConfFileNeeded, "pidgin;xchat", NULL, NULL);
	if (cForceDemandsAttention != NULL)
	{
		pTaskBar->cForceDemandsAttention = g_ascii_strdown (cForceDemandsAttention, -1);
		g_free (cForceDemandsAttention);
	}
	
	pTaskBar->cAnimationOnActiveWindow = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "animation on active window", &bFlushConfFileNeeded, "wobbly", NULL, NULL);
	
	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigTaskBar *pTaskBar)
{
	g_free (pTaskBar->cAnimationOnActiveWindow);
	g_free (pTaskBar->cAnimationOnDemandsAttention);
	g_free (pTaskBar->cOverwriteException);
	g_free (pTaskBar->cGroupException);
	g_free (pTaskBar->cForceDemandsAttention);
}

static void _set_indicator (Icon *pIcon, CairoContainer *pContainer, gpointer data)
{
	pIcon->bHasIndicator = GPOINTER_TO_INT (data);
}
static void reload (CairoConfigTaskBar *pPrevTaskBar, CairoConfigTaskBar *pTaskBar)
{
	CairoDock *pDock = g_pMainDock;
	
	gboolean bUpdateSize = FALSE;
	g_print ("TASKBAR: %d;%d\n", cairo_dock_application_manager_is_running () , pTaskBar->bShowAppli);
	if (/**pPrevTaskBar->bUniquePid != pTaskBar->bUniquePid ||*/
		pPrevTaskBar->bGroupAppliByClass != pTaskBar->bGroupAppliByClass ||
		pPrevTaskBar->bHideVisibleApplis != pTaskBar->bHideVisibleApplis ||
		pPrevTaskBar->bAppliOnCurrentDesktopOnly != pTaskBar->bAppliOnCurrentDesktopOnly ||
		pPrevTaskBar->bMixLauncherAppli != pTaskBar->bMixLauncherAppli ||
		pPrevTaskBar->bOverWriteXIcons != pTaskBar->bOverWriteXIcons ||
		pPrevTaskBar->iMinimizedWindowRenderType != pTaskBar->iMinimizedWindowRenderType ||
		pPrevTaskBar->iAppliMaxNameLength != pTaskBar->iAppliMaxNameLength ||
		cairo_dock_strings_differ (pPrevTaskBar->cGroupException, pTaskBar->cGroupException) ||
		cairo_dock_strings_differ (pPrevTaskBar->cOverwriteException, pTaskBar->cOverwriteException) ||
		(cairo_dock_application_manager_is_running () && ! pTaskBar->bShowAppli))  // on ne veut plus voir les applis, il faut donc les enlever.
	{
		cairo_dock_stop_application_manager ();
		bUpdateSize = TRUE;
	}
	
	if (cairo_dock_application_manager_is_running () &&
		pPrevTaskBar->bDrawIndicatorOnAppli != pTaskBar->bDrawIndicatorOnAppli)
	{
		cairo_dock_foreach_applis ((CairoDockForeachIconFunc) _set_indicator, FALSE, GINT_TO_POINTER (pTaskBar->bDrawIndicatorOnAppli));
		gtk_widget_queue_draw (pDock->container.pWidget);
	}
	
	if (! cairo_dock_application_manager_is_running () && pTaskBar->bShowAppli)  // maintenant on veut voir les applis !
	{
		cairo_dock_start_application_manager (pDock);  // va inserer le separateur si necessaire.
		bUpdateSize = TRUE;
	}
	else
		gtk_widget_queue_draw (pDock->container.pWidget);  // pour le fVisibleAlpha
	
	if (pPrevTaskBar->bShowAppli != pTaskBar->bShowAppli ||
		pPrevTaskBar->bGroupAppliByClass != pTaskBar->bGroupAppliByClass)
	{
		double fMaxScale = cairo_dock_get_max_scale (pDock);
		cairo_dock_load_class_indicator (pTaskBar->bShowAppli && pTaskBar->bGroupAppliByClass ? myIndicators.cClassIndicatorImagePath : NULL, fMaxScale);
	}
	
	if (bUpdateSize)
	{
		cairo_dock_calculate_dock_icons (pDock);
		gtk_widget_queue_draw (pDock->container.pWidget);  // le 'gdk_window_move_resize' ci-dessous ne provoquera pas le redessin si la taille n'a pas change.
		
		cairo_dock_place_root_dock (pDock);
	}
}


DEFINE_PRE_INIT (TaskBar)
{
	pModule->cModuleName = "TaskBar";
	pModule->cTitle = N_("TaskBar");
	pModule->cIcon = "icon-taskbar.png";
	pModule->cDescription = N_("Display and interact with the currently open windows.");
	pModule->iCategory = CAIRO_DOCK_CATEGORY_SYSTEM;
	pModule->iSizeOfConfig = sizeof (CairoConfigTaskBar);
	pModule->iSizeOfData = 0;
	
	pModule->reload = (CairoDockInternalModuleReloadFunc) reload;
	pModule->get_config = (CairoDockInternalModuleGetConfigFunc) get_config;
	pModule->reset_config = (CairoDockInternalModuleResetConfigFunc) reset_config;
	pModule->reset_data = NULL;
	
	pModule->pConfig = (CairoInternalModuleConfigPtr) &myTaskBar;
	pModule->pData = NULL;
}
