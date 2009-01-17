/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/

#include <gdk/gdkx.h>

#include "cairo-dock-X-utilities.h"
#include "cairo-dock-log.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-internal-accessibility.h"
#define _INTERNAL_MODULE_
#include "cairo-dock-internal-taskbar.h"

CairoConfigTaskBar myTaskBar;
extern CairoDock *g_pMainDock;

static gboolean get_config (GKeyFile *pKeyFile, CairoConfigTaskBar *pTaskBar)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	pTaskBar->bShowAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "show applications", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	
	pTaskBar->bUniquePid = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "unique PID", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	
	pTaskBar->bGroupAppliByClass = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "group by class", &bFlushConfFileNeeded, FALSE, "Applications", NULL);

	pTaskBar->iAppliMaxNameLength = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "max name length", &bFlushConfFileNeeded, 15, "Applications", NULL);

	pTaskBar->bMinimizeOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "minimize on click", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	pTaskBar->bCloseAppliOnMiddleClick = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "close on middle click", &bFlushConfFileNeeded, TRUE, "Applications", NULL);

	pTaskBar->bAutoHideOnFullScreen = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "auto quick hide", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	pTaskBar->bAutoHideOnMaximized = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "auto quick hide on max", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	pTaskBar->bHideVisibleApplis = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "hide visible", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	pTaskBar->fVisibleAppliAlpha = cairo_dock_get_double_key_value (pKeyFile, "TaskBar", "visibility alpha", &bFlushConfFileNeeded, .25, "Applications", NULL);  // >0 <=> les fenetres minimisees sont transparentes.
	if (pTaskBar->bHideVisibleApplis && pTaskBar->fVisibleAppliAlpha < 0)
		pTaskBar->fVisibleAppliAlpha = 0.;  // on inhibe ce parametre, puisqu'il ne sert alors a rien.
	else if (pTaskBar->bHideVisibleApplis > .6)
		pTaskBar->bHideVisibleApplis = .6;
	else if (pTaskBar->bHideVisibleApplis < -.6)
		pTaskBar->bHideVisibleApplis = -.6;
	pTaskBar->bAppliOnCurrentDesktopOnly = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "current desktop only", &bFlushConfFileNeeded, FALSE, "Applications", NULL);
	
	pTaskBar->bDemandsAttentionWithDialog = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "demands attention with dialog", &bFlushConfFileNeeded, TRUE, "Applications", NULL);
	pTaskBar->iDialogDuration = cairo_dock_get_integer_key_value (pKeyFile, "TaskBar", "duration", &bFlushConfFileNeeded, 2, NULL, NULL);
	pTaskBar->cAnimationOnDemandsAttention = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "animation on demands attention", &bFlushConfFileNeeded, "fire", NULL, NULL);
	pTaskBar->cAnimationOnActiveWindow = cairo_dock_get_string_key_value (pKeyFile, "TaskBar", "animation on active window", &bFlushConfFileNeeded, "wobbly", NULL, NULL);
	
	pTaskBar->bMixLauncherAppli = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "mix launcher appli", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	pTaskBar->bOverWriteXIcons = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "overwrite xicon", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	pTaskBar->bShowThumbnail = cairo_dock_get_boolean_key_value (pKeyFile, "TaskBar", "window thumbnail", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	if (pTaskBar->bShowThumbnail && ! cairo_dock_xcomposite_is_available ())
	{
		cd_warning ("Sorry but either your X server does not have the XComposite extension, or your version of Cairo-Dock was not built with the support of XComposite.\n You can't have window thumbnails in the dock");
		pTaskBar->bShowThumbnail = FALSE;
	}

	return bFlushConfFileNeeded;
}


static void reset_config (CairoConfigTaskBar *pTaskBar)
{
	g_free (pTaskBar->cAnimationOnActiveWindow);
	g_free (pTaskBar->cAnimationOnDemandsAttention);
}

static void reload (CairoConfigTaskBar *pPrevTaskBar, CairoConfigTaskBar *pTaskBar)
{
	CairoDock *pDock = g_pMainDock;
	
	if (myAccessibility.bAutoHide || myAccessibility.bPopUp)
	{
		pTaskBar->bAutoHideOnFullScreen = FALSE;
		pTaskBar->bAutoHideOnMaximized = FALSE;
	}
	
	gboolean bUpdateSize = FALSE;
	if (pPrevTaskBar->bUniquePid != pTaskBar->bUniquePid ||
		pPrevTaskBar->bGroupAppliByClass != pTaskBar->bGroupAppliByClass ||
		pPrevTaskBar->bHideVisibleApplis != pTaskBar->bHideVisibleApplis ||
		pPrevTaskBar->bAppliOnCurrentDesktopOnly != pTaskBar->bAppliOnCurrentDesktopOnly ||
		pPrevTaskBar->bMixLauncherAppli != pTaskBar->bMixLauncherAppli ||
		pPrevTaskBar->bOverWriteXIcons != pTaskBar->bOverWriteXIcons ||
		pPrevTaskBar->bShowThumbnail != pTaskBar->bShowThumbnail ||
		pPrevTaskBar->iAppliMaxNameLength != pTaskBar->iAppliMaxNameLength ||
		(cairo_dock_application_manager_is_running () && ! pTaskBar->bShowAppli))  // on ne veut plus voir les applis, il faut donc les enlever.
	{
		cairo_dock_stop_application_manager ();
		bUpdateSize = TRUE;
	}
	
	if (cairo_dock_application_manager_is_running () && pPrevTaskBar->iAppliMaxNameLength != pTaskBar->iAppliMaxNameLength)
	{
		// recharger les noms ...
	}
	
	if (! cairo_dock_application_manager_is_running () && pTaskBar->bShowAppli)  // maintenant on veut voir les applis !
	{
		cairo_dock_start_application_manager (pDock);  // va inserer le separateur si necessaire.
		bUpdateSize = TRUE;
	}
	else
		gtk_widget_queue_draw (pDock->pWidget);  // pour le fVisibleAlpha
	
	/**if (bUpdateSize)  // utile ?...
	{
		cairo_dock_calculate_dock_icons (pDock);
		gtk_widget_queue_draw (pDock->pWidget);  // le 'gdk_window_move_resize' ci-dessous ne provoquera pas le redessin si la taille n'a pas change.

		cairo_dock_place_root_dock (pDock);
	}*/
}


DEFINE_PRE_INIT (TaskBar)
{
	pModule->cModuleName = "TaskBar";
	pModule->cTitle = "TaskBar";
	pModule->cIcon = "gtk-dnd-multiple";
	pModule->cDescription = "Access your applications easily.";
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
