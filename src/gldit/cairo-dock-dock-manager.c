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

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <cairo.h>

#include "gldi-config.h"
#include "cairo-dock-image-buffer.h"
#include "cairo-dock-config.h"
#include "cairo-dock-icon-factory.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-separator-manager.h"  // gldi_automatic_separators_add_in_list
#include "cairo-dock-launcher-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-class-manager.h"  // cairo_dock_update_class_subdock_name
#include "cairo-dock-backends-manager.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_add_conf_file
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-container.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-indicator-manager.h"  // myIndicatorsParam.bUseClassIndic
#include "cairo-dock-style-manager.h"
#include "cairo-dock-opengl.h"
#include "cairo-dock-dock-visibility.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-windows-manager.h"



// public (manager, config, data)
CairoDocksParam myDocksParam;
GldiManager myDocksMgr;
GldiObjectManager myDockObjectMgr;
CairoDockImageBuffer g_pVisibleZoneBuffer;
CairoDock *g_pMainDock = NULL;  // pointeur sur le dock principal.
CairoDockHidingEffect *g_pHidingBackend = NULL;
CairoDockHidingEffect *g_pKeepingBelowBackend = NULL;

// dependancies
extern gchar *g_cConfFile;
extern gchar *g_cCurrentThemePath;
extern gboolean g_bUseOpenGL;
extern CairoDockGLConfig g_openglConfig;

// private
static GHashTable *s_hDocksTable = NULL;  // table des docks existant.
static GList *s_pRootDockList = NULL;
static guint s_iSidPollScreenEdge = 0;
static int s_iNbPolls = 0;
static gboolean s_bQuickHide = FALSE;
static gboolean s_bKeepAbove = FALSE;
static GldiShortkey *s_pPopupBinding = NULL;  // option 'pop up on shortkey'
static gboolean s_bResetAll = FALSE;

#define MOUSE_POLLING_DT 150  // mouse polling delay in ms

static gboolean _get_root_dock_config (CairoDock *pDock);
static void _start_polling_screen_edge (void);
static void _stop_polling_screen_edge (void);
static void _synchronize_sub_docks_orientation (CairoDock *pDock, gboolean bUpdateDockSize);
static void _set_dock_orientation (CairoDock *pDock, CairoDockPositionType iScreenBorder);
static void _remove_root_dock_config (const gchar *cDockName);

typedef struct {
	gboolean bUpToDate;
	gint x;
	gint y;
	gboolean bNoMove;
	gdouble dx;
	gdouble dy;
} CDMousePolling;

  /////////////
 // MANAGER //
/////////////


void cairo_dock_force_docks_above (void)
{
	if (g_pMainDock != NULL)
	{
		cd_warning ("this function must be called before any dock is created.");
		return;
	}
	s_bKeepAbove = TRUE;
}


// UNLOAD //
static gboolean _free_one_dock (G_GNUC_UNUSED const gchar *cDockName, CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	g_free (pDock->cDockName);
	pDock->cDockName = NULL;  // to not remove it from the table/list
	gldi_object_unref (GLDI_OBJECT(pDock));
	return TRUE;
}
void cairo_dock_reset_docks_table (void)
{
	s_bResetAll = TRUE;
	g_hash_table_foreach_remove (s_hDocksTable, (GHRFunc) _free_one_dock, NULL);
	g_pMainDock = NULL;
	
	g_list_free (s_pRootDockList);
	s_pRootDockList = NULL;
	s_bResetAll = FALSE;
}


static void _make_sub_dock (CairoDock *pDock, CairoDock *pParentDock, const gchar *cRendererName)
{
	//\__________________ set sub-dock flag
	pDock->iRefCount = 1;
	gtk_window_set_title (GTK_WINDOW (pDock->container.pWidget), "cairo-dock-sub");
	
	//\__________________ set the orientation relatively to the parent dock
	pDock->container.bIsHorizontal = pParentDock->container.bIsHorizontal;
	pDock->container.bDirectionUp = pParentDock->container.bDirectionUp;
	pDock->iNumScreen = pParentDock->iNumScreen;
	
	//\__________________ set a renderer
	cairo_dock_set_renderer (pDock, cRendererName);
	
	//\__________________ update the icons size and the ratio.
	double fPrevRatio = pDock->container.fRatio;
	pDock->container.fRatio = MIN (pDock->container.fRatio, myBackendsParam.fSubDockSizeRatio);
	pDock->iIconSize = pParentDock->iIconSize;
	
	Icon *icon;
	GList *ic;
	pDock->fFlatDockWidth = - myIconsParam.iIconGap;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		cairo_dock_icon_set_requested_size (icon, 0, 0);  // no request
		icon->fWidth = icon->fHeight = 0;  /// useful ?...
		cairo_dock_set_icon_size_in_dock (pDock, icon);
		pDock->fFlatDockWidth += icon->fWidth + myIconsParam.iIconGap;
	}
	pDock->iMaxIconHeight *= pDock->container.fRatio / fPrevRatio;
	
	//\__________________ remove any input shape
	if (pDock->pShapeBitmap != NULL)
	{
		cairo_region_destroy (pDock->pShapeBitmap);
		pDock->pShapeBitmap = NULL;
		if (pDock->iInputState != CAIRO_DOCK_INPUT_ACTIVE)
		{
			cairo_dock_set_input_shape_active (pDock);
			pDock->iInputState = CAIRO_DOCK_INPUT_ACTIVE;
		}
	}
	
	//\__________________ hide the dock
	pDock->bAutoHide = FALSE;
	gtk_widget_hide (pDock->container.pWidget);
}
void gldi_dock_make_subdock (CairoDock *pDock, CairoDock *pParentDock, const gchar *cRendererName)
{
	//g_print ("%s (%s)\n", __func__, cRendererName);
	if (pDock->iRefCount == 0)  // il devient un sous-dock.
	{
		//\__________________ make it a sub-dock.
		if (pParentDock == NULL)
			pParentDock = g_pMainDock;
		_make_sub_dock (pDock, pParentDock, cRendererName);
		cairo_dock_update_dock_size (pDock);
		
		//\__________________ remove from main-dock land
		_remove_root_dock_config (pDock->cDockName);  // just in case.
		
		s_pRootDockList = g_list_remove (s_pRootDockList, pDock);
		
		gldi_dock_set_visibility (pDock, CAIRO_DOCK_VISI_KEEP_ABOVE);  // si la visibilite n'avait pas ete mise (sub-dock), ne fera rien (vu que la visibilite par defaut est KEEP_ABOVE).
	}
}



static void _find_similar_root_dock (CairoDock *pDock, gpointer *data)
{
	CairoDock *pDock0 = data[0];
	if (pDock == pDock0)
		data[2] = GINT_TO_POINTER (TRUE);
	if (data[2])
		return;
	if (pDock->container.bIsHorizontal == pDock0->container.bIsHorizontal
		&& pDock->container.bDirectionUp == pDock0->container.bDirectionUp)
	{
		int *i = data[1];
		*i = *i + 1;
	}
}
gchar *gldi_dock_get_readable_name (CairoDock *pDock)
{
	g_return_val_if_fail (pDock != NULL, NULL);
	gchar *cUserName = NULL;
	if (pDock->iRefCount == 0)
	{
		int i = 0;
		gpointer data[3] = {pDock, &i, NULL};
		GList *d = g_list_last (s_pRootDockList);
		for (;d != NULL; d = d->prev)  // parse the list from the end to get the main dock first (docks are prepended).
			_find_similar_root_dock (d->data, data);
		const gchar *cPosition;
		if (pDock->container.bIsHorizontal)
		{
			if (pDock->container.bDirectionUp)
				cPosition = _("Bottom dock");
			else
				cPosition = _("Top dock");
		}
		else
		{
			if (pDock->container.bDirectionUp)
				cPosition = _("Right dock");
			else
				cPosition = _("Left dock");
		}
		if (i > 0)
			cUserName = g_strdup_printf ("%s (%d)", cPosition, i+1);
		else
			cUserName = g_strdup (cPosition);
	}
	
	return cUserName;
}

CairoDock *gldi_dock_get (const gchar *cDockName)
{
	if (cDockName == NULL)
		return NULL;
	return g_hash_table_lookup (s_hDocksTable, cDockName);
}

static gboolean _cairo_dock_search_icon_from_subdock (G_GNUC_UNUSED gchar *cDockName, CairoDock *pDock, gpointer *data)
{
	if (pDock == data[0])
		return FALSE;
	Icon **pIconFound = data[1];
	CairoDock **pDockFound = data[2];
	Icon *icon = cairo_dock_get_icon_with_subdock (pDock->icons, data[0]);
	if (icon != NULL)
	{
		*pIconFound = icon;
		if (pDockFound != NULL)
			*pDockFound = pDock;
		return TRUE;
	}
	else
		return FALSE;
}
Icon *cairo_dock_search_icon_pointing_on_dock (CairoDock *pDock, CairoDock **pParentDock)  // pParentDock peut etre NULL.
{
	if (pDock == NULL || pDock->bIsMainDock)  // par definition. On n'utilise pas iRefCount, car si on est en train de detruire un dock, sa reference est deja decrementee. C'est dommage mais c'est comme ca.
		return NULL;
	Icon *pPointingIcon = NULL;
	gpointer data[3] = {pDock, &pPointingIcon, pParentDock};
	g_hash_table_find (s_hDocksTable, (GHRFunc)_cairo_dock_search_icon_from_subdock, data);
	return pPointingIcon;
}

gchar *cairo_dock_get_unique_dock_name (const gchar *cPrefix)
{
	const gchar *cNamepattern = (cPrefix != NULL && *cPrefix != '\0' && strcmp (cPrefix, "cairo-dock") != 0 ? cPrefix : "dock");
	GString *sNameString = g_string_new (cNamepattern);
	
	int i = 2;
	while (g_hash_table_lookup (s_hDocksTable, sNameString->str) != NULL)
	{
		g_string_printf (sNameString, "%s-%d", cNamepattern, i);
		i ++;
	}
	
	gchar *cUniqueName = sNameString->str;
	g_string_free (sNameString, FALSE);
	return cUniqueName;
}

gboolean cairo_dock_check_unique_subdock_name (Icon *pIcon)
{
	cd_debug ("%s (%s)", __func__, pIcon->cName);
	gchar *cUniqueName = cairo_dock_get_unique_dock_name (pIcon->cName);
	if (pIcon->cName == NULL || strcmp (pIcon->cName, cUniqueName) != 0)
	{
		g_free (pIcon->cName);
		pIcon->cName = cUniqueName;
		cd_debug (" cName <- %s", cUniqueName);
		return TRUE;
	}
	return FALSE;
}


void gldi_dock_rename (CairoDock *pDock, const gchar *cNewName)
{
	// ensure everything is ok
	g_return_if_fail (pDock != NULL && cNewName != NULL);
	g_return_if_fail (g_hash_table_lookup (s_hDocksTable, cNewName) == NULL);
	cd_debug("%s (%s -> %s)", __func__, pDock->cDockName, cNewName);
	
	// if it's a class subdock, first update the dock name in the class
	cairo_dock_update_class_subdock_name (pDock, cNewName);
	
	// rename in the hash table
	g_hash_table_remove (s_hDocksTable, pDock->cDockName);
	g_free (pDock->cDockName);
	pDock->cDockName = g_strdup (cNewName);
	g_hash_table_insert (s_hDocksTable, pDock->cDockName, pDock);
	
	// rename in each icons
	GList* ic;
	Icon *icon;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		gldi_theme_icon_write_container_name_in_conf_file (icon, cNewName);
		g_free (icon->cParentDockName);
		icon->cParentDockName = g_strdup (cNewName);
	}
}


void gldi_docks_foreach (GHFunc pFunction, gpointer data)
{
	g_hash_table_foreach (s_hDocksTable, pFunction, data);
}

void gldi_docks_foreach_root (GFunc pFunction, gpointer data)
{
	g_list_foreach (s_pRootDockList, pFunction, data);
}

static void _gldi_icons_foreach_in_dock (G_GNUC_UNUSED gchar *cDockName, CairoDock *pDock, gpointer *data)
{
	GldiIconFunc pFunction = data[0];
	gpointer pUserData = data[1];
	GList *ic = pDock->icons, *next_ic;
	while (ic != NULL)
	{
		next_ic = ic->next;  // the function below may remove the current icon
		pFunction ((Icon*)ic->data, pUserData);
		ic = next_ic;
	}
}
void gldi_icons_foreach_in_docks (GldiIconFunc pFunction, gpointer pUserData)
{
	gpointer data[2] = {pFunction, pUserData};
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _gldi_icons_foreach_in_dock, data);
}



static void _reload_buffer_in_one_dock (/**const gchar *cDockName, */CairoDock *pDock, gpointer data)
{
	cairo_dock_reload_buffers_in_dock (pDock, TRUE, GPOINTER_TO_INT (data));
}
static void _cairo_dock_draw_one_subdock_icon (G_GNUC_UNUSED const gchar *cDockName, CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	Icon *icon;
	GList *ic;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pSubDock != NULL
		&& (GLDI_OBJECT_IS_STACK_ICON (icon) || CAIRO_DOCK_IS_MULTI_APPLI (icon) || GLDI_OBJECT_IS_APPLET_ICON (icon))
		&& (icon->iSubdockViewType != 0
			|| (CAIRO_DOCK_IS_MULTI_APPLI (icon) && !myIndicatorsParam.bUseClassIndic))
		/**&& icon->iSidRedrawSubdockContent == 0*/)  // icone de sous-dock ou de classe ou d'applets.
		{
			cairo_dock_trigger_redraw_subdock_content_on_icon (icon);
		}
	}
}
void cairo_dock_reload_buffers_in_all_docks (gboolean bUpdateIconSize)
{
	g_list_foreach (s_pRootDockList, (GFunc)_reload_buffer_in_one_dock, GINT_TO_POINTER (bUpdateIconSize));  // we load the root docks first, so that sub-docks can have the correct icon size.
	
	// now that all icons in sub-docks are drawn, redraw icons pointing to a sub-dock
	g_hash_table_foreach (s_hDocksTable, (GHFunc)_cairo_dock_draw_one_subdock_icon, NULL);
}



static void _cairo_dock_set_one_dock_view_to_default (G_GNUC_UNUSED gchar *cDockName, CairoDock *pDock, gpointer data)
{
	//g_print ("%s (%s)\n", __func__, cDockName);
	int iDockType = GPOINTER_TO_INT (data);
	if (iDockType == 0 || (iDockType == 1 && pDock->iRefCount == 0) || (iDockType == 2 && pDock->iRefCount != 0))
	{
		cairo_dock_set_default_renderer (pDock);
		cairo_dock_update_dock_size (pDock);
	}
}
void cairo_dock_set_all_views_to_default (int iDockType)
{
	//g_print ("%s ()\n", __func__);
	g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_set_one_dock_view_to_default, GINT_TO_POINTER (iDockType));
}


  //////////////////////
 // ROOT DOCK CONFIG //
//////////////////////

void gldi_rootdock_write_gaps (CairoDock *pDock)
{
	if (pDock->iRefCount > 0)
		return;
	
	cairo_dock_prevent_dock_from_out_of_screen (pDock);
	if (pDock->bIsMainDock)
	{
		cairo_dock_update_conf_file (g_cConfFile,
			G_TYPE_INT, "Position", "x gap", pDock->iGapX,
			G_TYPE_INT, "Position", "y gap", pDock->iGapY,
			G_TYPE_INVALID);
	}
	else
	{
		const gchar *cDockName = gldi_dock_get_name (pDock);
		gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
		if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))  // shouldn't happen
		{
			cairo_dock_add_conf_file (GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_MAIN_DOCK_CONF_FILE, cConfFilePath);
		}
		
		cairo_dock_update_conf_file (cConfFilePath,
			G_TYPE_INT, "Behavior", "x gap", pDock->iGapX,
			G_TYPE_INT, "Behavior", "y gap", pDock->iGapY,
			G_TYPE_INVALID);
		g_free (cConfFilePath);
	}
}

int cairo_dock_convert_icon_size_to_pixels (GldiIconSizeEnum s, double *fMaxScale, double *fReflectSize, int *iIconGap)
{
	int iIconSize;
	switch (s)
	{
		case ICON_DEFAULT:
		default:
			iIconSize = myIconsParam.iIconWidth;
			*fMaxScale = 1 + myIconsParam.fAmplitude;
			*iIconGap = myIconsParam.iIconGap;
			*fReflectSize = myIconsParam.fReflectHeightRatio;
		break;
		case ICON_TINY:
			iIconSize = ICON_SIZE_TINY;
			*fMaxScale = 2;
			*iIconGap = 4;
			*fReflectSize = .4;
		break;
		case ICON_VERY_SMALL:
			iIconSize = ICON_SIZE_VERY_SMALL;
			*fMaxScale = 1.8;
			*iIconGap = 4;
			*fReflectSize = .4;
		break;
		case ICON_SMALL:
			iIconSize = ICON_SIZE_SMALL;
			*fMaxScale = 1.8;
			*iIconGap = 4;
			*fReflectSize = .4;
		break;
		case ICON_MEDIUM:
			iIconSize = ICON_SIZE_MEDIUM;
			*fMaxScale = 1.6;
			*iIconGap = 3;
			*fReflectSize = .5;
		break;
		case ICON_BIG:
			iIconSize = ICON_SIZE_BIG;
			*fMaxScale = 1.5;
			*iIconGap = 2;
			*fReflectSize = .6;
		break;
		case ICON_HUGE:
			iIconSize = ICON_SIZE_HUGE;
			*fMaxScale = 1.3;
			*iIconGap = 2;
			*fReflectSize = .6;
		break;
	}
	return iIconSize;
}

GldiIconSizeEnum cairo_dock_convert_icon_size_to_enum (int iIconSize)
{
	GldiIconSizeEnum s = ICON_DEFAULT;
	if (iIconSize <= ICON_SIZE_TINY+2)
		s = ICON_TINY;
	else if (iIconSize <= ICON_SIZE_VERY_SMALL+2)
		s = ICON_VERY_SMALL;
	else if (iIconSize >= ICON_SIZE_HUGE-2)
		s = ICON_HUGE;
	else if (iIconSize > ICON_SIZE_MEDIUM)
		s = ICON_BIG;
	else
	{
		if (myIconsParam.fAmplitude >= 2 || iIconSize <= ICON_SIZE_SMALL)
			s = ICON_SMALL;
		else
			s = ICON_MEDIUM;  // moyennes.
	}
	return s;
}

static gboolean _get_root_dock_config (CairoDock *pDock)
{
	g_return_val_if_fail (pDock != NULL, FALSE);
	if (pDock->iRefCount > 0)
		return FALSE;
	
	if (pDock->bIsMainDock)  // the main dock doesn't have a config file, it uses the global params (that we already got from the Dock-manager)
	{
		pDock->iGapX = myDocksParam.iGapX;
		pDock->iGapY = myDocksParam.iGapY;
		
		pDock->fAlign = myDocksParam.fAlign;
		
		pDock->iNumScreen = myDocksParam.iNumScreen;
		
		_set_dock_orientation (pDock, myDocksParam.iScreenBorder);  // do it after all position parameters have been set; it sets the sub-docks orientation too.
		
		gldi_dock_set_visibility (pDock, myDocksParam.iVisibility);
		
		pDock->bGlobalIconSize = TRUE;  // default icon size
		
		pDock->bGlobalBg = TRUE;  // default background
		// pDock->cRendererName and pDock->iIconSize stay at 0
		
		pDock->bExtendedMode = myDocksParam.bExtendedMode;
		
		return TRUE;
	}
	
	//\______________ On verifie la presence du fichier de conf associe.
	//g_print ("%s (%s)\n", __func__, pDock->cDockName);
	gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, pDock->cDockName);
	if (! g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))  // pas encore de fichier de conf pour ce dock.
	{
		pDock->container.bIsHorizontal = g_pMainDock->container.bIsHorizontal;
		pDock->container.bDirectionUp = g_pMainDock->container.bDirectionUp;
		pDock->fAlign = g_pMainDock->fAlign;
		
		g_free (cConfFilePath);
		return FALSE;
	}
	
	//\______________ On ouvre le fichier de conf.
	GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
	if (pKeyFile == NULL)
	{
		cd_warning ("wrong conf file (%s) !", cConfFilePath);
		g_free (cConfFilePath);
		return FALSE;
	}
	
	//\______________ Position.
	gboolean bFlushConfFileNeeded = FALSE;
	pDock->iGapX = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "x gap", &bFlushConfFileNeeded, 0, "Position", NULL);
	pDock->iGapY = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "y gap", &bFlushConfFileNeeded, 0, "Position", NULL);
	
	pDock->fAlign = cairo_dock_get_double_key_value (pKeyFile, "Behavior", "alignment", &bFlushConfFileNeeded, 0.5, "Position", NULL);
	
	pDock->iNumScreen = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "num_screen", &bFlushConfFileNeeded, GLDI_DEFAULT_SCREEN, "Position", NULL);
	
	CairoDockPositionType iScreenBorder = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "screen border", &bFlushConfFileNeeded, 0, "Position", NULL);
	_set_dock_orientation (pDock, iScreenBorder);  // do it after all position parameters have been set; it sets the sub-docks orientation too.
	
	//\______________ Visibility.
	CairoDockVisibility iVisibility = cairo_dock_get_integer_key_value (pKeyFile, "Behavior", "visibility", &bFlushConfFileNeeded, FALSE, "Position", NULL);
	gldi_dock_set_visibility (pDock, iVisibility);
	
	//\______________ Icons size.
	int s = cairo_dock_get_integer_key_value (pKeyFile, "Appearance", "icon size", &bFlushConfFileNeeded, ICON_DEFAULT, NULL, NULL);  // ICON_DEFAULT <=> same as main dock
	double fMaxScale, fReflectSize;
	int iIconGap;
	pDock->iIconSize = cairo_dock_convert_icon_size_to_pixels (s, &fMaxScale, &fReflectSize, &iIconGap);
	pDock->bGlobalIconSize = (s == ICON_DEFAULT);
	
	//\______________ View.
	g_free (pDock->cRendererName);
	pDock->cRendererName = cairo_dock_get_string_key_value (pKeyFile, "Appearance", "main dock view", &bFlushConfFileNeeded, NULL, "Views", NULL);
	
	//\______________ Background.
	int iBgType = cairo_dock_get_integer_key_value (pKeyFile, "Appearance", "fill bg", &bFlushConfFileNeeded, 0, NULL, NULL);
	
	pDock->bGlobalBg = (iBgType == 0);
	
	if (!pDock->bGlobalBg)
	{
		if (iBgType == 1)
		{
			gchar *cBgImage = cairo_dock_get_string_key_value (pKeyFile, "Appearance", "background image", &bFlushConfFileNeeded, NULL, NULL, NULL);
			g_free (pDock->cBgImagePath);
			if (cBgImage != NULL)
			{
				pDock->cBgImagePath = cairo_dock_search_image_s_path (cBgImage);
				g_free (cBgImage);
			}
			else
				pDock->cBgImagePath = NULL;
			
			pDock->bBgImageRepeat = cairo_dock_get_boolean_key_value (pKeyFile, "Appearance", "repeat image", &bFlushConfFileNeeded, FALSE, NULL, NULL);
		}
		// on recupere la couleur tout le temps pour avoir un plan B.
		GldiColor couleur = {{.7, .7, 1., .7}};
		cairo_dock_get_color_key_value (pKeyFile, "Appearance", "stripes color dark", &bFlushConfFileNeeded, &pDock->fBgColorDark, &couleur, NULL, NULL);
		
		GldiColor couleur2 = {{.7, .9, .7, .4}};
		cairo_dock_get_color_key_value (pKeyFile, "Appearance", "stripes color bright", &bFlushConfFileNeeded, &pDock->fBgColorBright, &couleur2, NULL, NULL);
	}
	
	pDock->bExtendedMode = cairo_dock_get_boolean_key_value (pKeyFile, "Appearance", "extended", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	//\______________ On met a jour le fichier de conf.
	if (! bFlushConfFileNeeded)
		bFlushConfFileNeeded = cairo_dock_conf_file_needs_update (pKeyFile, GLDI_VERSION);
	if (bFlushConfFileNeeded)
	{
		//g_print ("update %s conf file\n", pDock->cDockName);
		cairo_dock_upgrade_conf_file (cConfFilePath, pKeyFile, GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_MAIN_DOCK_CONF_FILE);
	}
	
	g_key_file_free (pKeyFile);
	g_free (cConfFilePath);
	return TRUE;
}

static void _remove_root_dock_config (const gchar *cDockName)
{
	if (! cDockName || strcmp (cDockName, "cairo-dock") == 0)
		return ;
	gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
	if (g_file_test (cConfFilePath, G_FILE_TEST_EXISTS))
	{
		cairo_dock_delete_conf_file (cConfFilePath);
	}
	g_free (cConfFilePath);
}

void gldi_dock_add_conf_file_for_name (const gchar *cDockName)
{
	// on cree le fichier de conf a partir du template.
	cd_debug ("%s (%s)", __func__, cDockName);
	gchar *cConfFilePath = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
	cairo_dock_add_conf_file (GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_MAIN_DOCK_CONF_FILE, cConfFilePath);
	
	// on placera le nouveau dock a l'oppose du main dock, meme ecran et meme visibilite.
	cairo_dock_update_conf_file (cConfFilePath,
		G_TYPE_INT, "Behavior", "screen border",
		(g_pMainDock->container.bIsHorizontal ?
			(g_pMainDock->container.bDirectionUp ? 1 : 0) :
			(g_pMainDock->container.bDirectionUp ? 3 : 2)),
		G_TYPE_INT, "Behavior", "visibility",
		g_pMainDock->iVisibility,
		G_TYPE_INT, "Behavior", "num_screen",
		g_pMainDock->iNumScreen,
		G_TYPE_INVALID);
	g_free (cConfFilePath);
}

gchar *gldi_dock_add_conf_file (void)
{
	// on genere un nom unique.
	gchar *cValidDockName = cairo_dock_get_unique_dock_name (CAIRO_DOCK_MAIN_DOCK_NAME);  // meme nom que le main dock avec un numero en plus, plus facile pour les reperer.
	
	// on genere un fichier de conf pour ce nom.
	gldi_dock_add_conf_file_for_name (cValidDockName);
	
	return cValidDockName;
}


void gldi_docks_redraw_all_root (void)
{
	gldi_docks_foreach_root ((GFunc)cairo_dock_redraw_container, NULL);
}


static void _reposition_one_root_dock (G_GNUC_UNUSED const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount == 0 && ! (data && pDock->bIsMainDock))
	{
		if (!pDock->bIsMainDock)
			_get_root_dock_config (pDock);  // relit toute la conf.
		
		cairo_dock_update_dock_size (pDock);  // la taille max du dock depend de la taille de l'ecran, donc on recalcule son ratio.
		cairo_dock_move_resize_dock (pDock);
		gtk_widget_show (pDock->container.pWidget);
		gtk_widget_queue_draw (pDock->container.pWidget);
		_synchronize_sub_docks_orientation (pDock, TRUE);
	}
}
static void _reposition_root_docks (gboolean bExceptMainDock)
{
	g_hash_table_foreach (s_hDocksTable, (GHFunc)_reposition_one_root_dock, GINT_TO_POINTER (bExceptMainDock));
}

void gldi_subdock_synchronize_orientation (CairoDock *pSubDock, CairoDock *pDock, gboolean bUpdateDockSize)
{
	if (pSubDock->container.bDirectionUp != pDock->container.bDirectionUp)
	{
		pSubDock->container.bDirectionUp = pDock->container.bDirectionUp;
		bUpdateDockSize = TRUE;
	}
	if (pSubDock->container.bIsHorizontal != pDock->container.bIsHorizontal)
	{
		pSubDock->container.bIsHorizontal = pDock->container.bIsHorizontal;
		bUpdateDockSize = TRUE;
	}
	if (pSubDock->iNumScreen != pDock->iNumScreen)
	{
		pSubDock->iNumScreen = pDock->iNumScreen;
		bUpdateDockSize = TRUE;
	}
	
	if (bUpdateDockSize)
	{
		cairo_dock_update_dock_size (pSubDock);
	}
	
	_synchronize_sub_docks_orientation (pSubDock, bUpdateDockSize);
}

static void _synchronize_sub_docks_orientation (CairoDock *pDock, gboolean bUpdateDockSize)
{
	GList* ic;
	Icon *icon;
	for (ic = pDock->icons; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->pSubDock != NULL)
		{
			gldi_subdock_synchronize_orientation (icon->pSubDock, pDock, bUpdateDockSize);  // recursively synchronize all children (no need to check for loops, as it shouldn't occur... if it does, then the problem is to fix upstream).
		}
	}
}

static void _set_dock_orientation (CairoDock *pDock, CairoDockPositionType iScreenBorder)
{
	switch (iScreenBorder)
	{
		case CAIRO_DOCK_BOTTOM :
			pDock->container.bIsHorizontal = CAIRO_DOCK_HORIZONTAL;
			pDock->container.bDirectionUp = TRUE;
		break;
		case CAIRO_DOCK_TOP :
			pDock->container.bIsHorizontal = CAIRO_DOCK_HORIZONTAL;
			pDock->container.bDirectionUp = FALSE;
		break;
		case CAIRO_DOCK_RIGHT :
			pDock->container.bIsHorizontal = CAIRO_DOCK_VERTICAL;
			pDock->container.bDirectionUp = TRUE;
		break;
		case CAIRO_DOCK_LEFT :
			pDock->container.bIsHorizontal = CAIRO_DOCK_VERTICAL;
			pDock->container.bDirectionUp = FALSE;
		break;
		case CAIRO_DOCK_INSIDE_SCREEN :
		case CAIRO_DOCK_NB_POSITIONS :
		break;
	}
	
	gldi_container_set_anchor ( &(pDock->container), iScreenBorder);
	_synchronize_sub_docks_orientation (pDock, FALSE);
}


  ////////////////
 // VISIBILITY //
////////////////

static void _cairo_dock_quick_hide_one_root_dock (G_GNUC_UNUSED const gchar *cDockName, CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->iRefCount == 0)
	{
		pDock->bAutoHide = TRUE;
		cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
	}
}
void cairo_dock_quick_hide_all_docks (void)
{
	if (! s_bQuickHide)
	{
		s_bQuickHide = TRUE;
		g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_quick_hide_one_root_dock, NULL);
		_start_polling_screen_edge ();
	}
}

static void _cairo_dock_stop_quick_hide_one_root_dock (G_GNUC_UNUSED const gchar *cDockName, CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->iRefCount == 0 && ! pDock->bTemporaryHidden && pDock->bAutoHide && pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE)
	{
		pDock->bAutoHide = FALSE;
		
		if (! pDock->container.bInside)  // on le fait re-apparaitre.
		{
			cairo_dock_start_showing (pDock);  // l'input shape sera mise lors du configure.
		}
	}
}
void cairo_dock_stop_quick_hide (void)
{
	if (s_bQuickHide)
	{
		s_bQuickHide = FALSE;
		_stop_polling_screen_edge ();
		
		g_hash_table_foreach (s_hDocksTable, (GHFunc) _cairo_dock_stop_quick_hide_one_root_dock, NULL);
	}
}

void cairo_dock_allow_entrance (CairoDock *pDock)
{
	pDock->bEntranceDisabled = FALSE;
}

void cairo_dock_disable_entrance (CairoDock *pDock)
{
	pDock->bEntranceDisabled = TRUE;
}

gboolean cairo_dock_entrance_is_allowed (CairoDock *pDock)
{
	return (! pDock->bEntranceDisabled);
}

void cairo_dock_activate_temporary_auto_hide (CairoDock *pDock)
{
	if (pDock->iRefCount == 0 && ! pDock->bTemporaryHidden && pDock->iVisibility != CAIRO_DOCK_VISI_AUTO_HIDE)
	{
		pDock->bAutoHide = TRUE;
		pDock->bTemporaryHidden = TRUE;
		if (!pDock->container.bInside)  // on ne declenche pas le cachage lorsque l'on change par exemple de bureau via le switcher ou un clic sur une appli.
		{
			cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));  // un cairo_dock_start_hiding ne cacherait pas les sous-docks.
		}
	}
}

void cairo_dock_deactivate_temporary_auto_hide (CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	if (pDock->iRefCount == 0 && pDock->bTemporaryHidden && ! s_bQuickHide)
	{
		pDock->bTemporaryHidden = FALSE;
		pDock->bAutoHide = FALSE;
		
		if (! pDock->container.bInside)  // on le fait re-apparaitre.
		{
			cairo_dock_start_showing (pDock);
		}
	}
}


static gboolean _cairo_dock_hide_back_dock (CairoDock *pDock)
{
	//g_print ("hide back\n");
	if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW && ! pDock->container.bInside)
		cairo_dock_pop_down (pDock);
	else if (pDock->bAutoHide)
		cairo_dock_start_hiding (pDock);
	pDock->iSidHideBack = 0;
	return FALSE;
}
static gboolean _cairo_dock_unhide_dock_delayed (CairoDock *pDock)
{
	//g_print ("%s (%d, %d)\n", __func__, pDock->container.bInside, pDock->iInputState);
	if (pDock->container.bInside && pDock->iInputState != CAIRO_DOCK_INPUT_HIDDEN && !pDock->bIsBelow)  // already inside and reachable (caution) => no need to show it again.
	{
		pDock->iSidUnhideDelayed = 0;
		return FALSE;
	}
	
	//g_print ("let's show this dock (%d)\n", pDock->bIsMainDock);
	if (pDock->bAutoHide)
		cairo_dock_start_showing (pDock);
	if (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW)
		cairo_dock_pop_up (pDock);
	
	if (pDock->iSidHideBack == 0)  // on se recachera dans 2s si on n'est pas entre dans le dock entre-temps.
		pDock->iSidHideBack = g_timeout_add (2000, (GSourceFunc) _cairo_dock_hide_back_dock, (gpointer) pDock);
	pDock->iSidUnhideDelayed = 0;
	return FALSE;
}
static void _cairo_dock_unhide_root_dock_on_mouse_hit (CairoDock *pDock, CDMousePolling *pMouse)
{
	if (! pDock->bAutoHide && pDock->iVisibility != CAIRO_DOCK_VISI_KEEP_BELOW)
		return;
	
	int iScreenWidth = gldi_dock_get_screen_width (pDock);
	int iScreenHeight = gldi_dock_get_screen_height (pDock);
	int iScreenX = gldi_dock_get_screen_offset_x (pDock);
	int iScreenY = gldi_dock_get_screen_offset_y (pDock);
	
	//\________________ On recupere la position du pointeur.
	gint x, y;
	if (! pMouse->bUpToDate)  // pas encore recupere le pointeur.
	{
		pMouse->bUpToDate = TRUE;
		gldi_display_get_pointer (&x, &y);
		if (x == pMouse->x && y == pMouse->y)  // le pointeur n'a pas bouge, on quitte.
		{
			pMouse->bNoMove = TRUE;
			return ;
		}
		pMouse->bNoMove = FALSE;
		pMouse->dx = (x - pMouse->x);
		pMouse->dy = (y - pMouse->y);
		double d = sqrt (pMouse->dx * pMouse->dx + pMouse->dy * pMouse->dy);
		pMouse->dx /= d;
		pMouse->dy /= d;
		pMouse->x = x;
		pMouse->y = y;
	}
	else  // le pointeur a ete recupere auparavant.
	{
		if (pMouse->bNoMove)  // position inchangee.
			return;
		x = pMouse->x;
		y = pMouse->y;
	}
	
	if (!pDock->container.bIsHorizontal)
	{
		x = pMouse->y;
		y = pMouse->x;
	}
	y -= iScreenY;  // relative to the border of the dock's screen.
	if (pDock->container.bDirectionUp)
	{
		y = iScreenHeight - 1 - y;
		
	}
	
	//\________________ On verifie les conditions.
	int x1, x2;  // coordinates range on the X screen edge.
	gboolean bShow = FALSE;
	int Ws = (pDock->container.bIsHorizontal ? gldi_desktop_get_width() : gldi_desktop_get_height());
	switch (myDocksParam.iCallbackMethod)
	{
		case CAIRO_HIT_SCREEN_BORDER:
		default:
			if (y != 0)
				break;
			if (x < iScreenX || x > iScreenX + iScreenWidth - 1)  // only check the border of the dock's screen.
				break ;
			bShow = TRUE;
		break;
		case CAIRO_HIT_DOCK_PLACE:
			
			if (y != 0)
				break;
			x1 = pDock->container.iWindowPositionX + (pDock->container.iWidth - pDock->iActiveWidth) * pDock->fAlign;
			x2 = x1 + pDock->iActiveWidth;
			if (x1 < 8)  // avoid corners, since this is actually the purpose of this option (corners can be used by the WM to trigger actions).
				x1 = 8;
			if (x2 > Ws - 8)
				x2 = Ws - 8;
			if (x < x1 || x > x2)
				break;
			bShow = TRUE;
		break;
		case CAIRO_HIT_SCREEN_CORNER:
			if (y != 0)
				break;
			if (x > 0 && x < Ws - 1)  // avoid the corners of the X screen (since we can't actually hit the corner of a screen that would be inside the X screen).
				break ;
			bShow = TRUE;
		break;
		case CAIRO_HIT_ZONE:
			if (y > myDocksParam.iZoneHeight)
				break;
			x1 = pDock->container.iWindowPositionX + (pDock->container.iWidth - myDocksParam.iZoneWidth) * pDock->fAlign;
			x2 = x1 + myDocksParam.iZoneWidth;
			if (x < x1 || x > x2)
				break;
			bShow = TRUE;
		break;
	}
	if (! bShow)
	{
		if (pDock->iSidUnhideDelayed != 0)
		{
			g_source_remove (pDock->iSidUnhideDelayed);
			pDock->iSidUnhideDelayed = 0;
		}
		return;
	}
	
	//\________________ On montre ou on programme le montrage du dock.
	int nx, ny;  // normal vector to the screen edge.
	double cost;  // cos (teta), where teta = angle between mouse vector and dock's normal
	double f = 1.;  // delay factor
	if (pDock->container.bIsHorizontal)
	{
		nx = 0;
		ny = (pDock->container.bDirectionUp ? -1 : 1);
	}
	else
	{
		ny = 0;
		nx = (pDock->container.bDirectionUp ? -1 : 1);
	}
	cost = nx * pMouse->dx + ny * pMouse->dy;
	f = 2 + cost;  // so if cost = -1, we arrive straight onto the screen edge, and f = 1, => normal delay. if cost = 0, f = 2 and we have a bigger delay.
	
	int iDelay = f * myDocksParam.iUnhideDockDelay;
	//g_print (" dock will be shown in %dms (%.2f, %d)\n", iDelay, f, pDock->bIsMainDock);
	if (iDelay != 0)  // on programme une apparition.
	{
		if (pDock->iSidUnhideDelayed == 0)
			pDock->iSidUnhideDelayed = g_timeout_add (iDelay, (GSourceFunc) _cairo_dock_unhide_dock_delayed, (gpointer) pDock);
	}
	else  // on montre le dock tout de suite.
	{
		_cairo_dock_unhide_dock_delayed (pDock);
	}
}

static gboolean _cairo_dock_poll_screen_edge (G_GNUC_UNUSED gpointer data)  // thanks to Smidgey for the pop-up patch !
{
	static CDMousePolling mouse;
	
	// if the active window is full screen, avoid showing the docks on edge hit
	// some WM will show the dock on top of fullscreen windows, and it's a problem in case of games, for instance
	GldiWindowActor *actor = gldi_windows_get_active();
	if (actor && actor->bIsFullScreen)
		return TRUE;

	mouse.bUpToDate = FALSE;  // mouse position will be updated by the first hidden dock.
	g_list_foreach (s_pRootDockList, (GFunc) _cairo_dock_unhide_root_dock_on_mouse_hit, &mouse);
	
	return TRUE;
}
static void _start_polling_screen_edge (void)
{
	s_iNbPolls ++;
	cd_debug ("%s (%d)", __func__, s_iNbPolls);
	if (s_iSidPollScreenEdge == 0)
		s_iSidPollScreenEdge = g_timeout_add (MOUSE_POLLING_DT, (GSourceFunc) _cairo_dock_poll_screen_edge, NULL);
}

static void _stop_polling_screen_edge_now (void)
{
	if (s_iSidPollScreenEdge != 0)
	{
		g_source_remove (s_iSidPollScreenEdge);
		s_iSidPollScreenEdge = 0;
	}
	s_iNbPolls = 0;
}
static void _stop_polling_screen_edge (void)
{
	cd_debug ("%s (%d)", __func__, s_iNbPolls);
	s_iNbPolls --;
	if (s_iNbPolls <= 0)
	{
		_stop_polling_screen_edge_now ();  // remet tout a 0.
	}
}

void gldi_dock_set_visibility (CairoDock *pDock, CairoDockVisibility iVisibility)
{
	//\_______________ jeu de parametres.
	gboolean bReserveSpace = (iVisibility == CAIRO_DOCK_VISI_RESERVE);
	gboolean bKeepBelow = (iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW);
	gboolean bAutoHideOnOverlap = (iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP);
	gboolean bAutoHideOnAnyOverlap = (iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY);
	gboolean bAutoHide = (iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE);
	gboolean bShortKey = (iVisibility == CAIRO_DOCK_VISI_SHORTKEY);
	
	gboolean bReserveSpace0 = (pDock->iVisibility == CAIRO_DOCK_VISI_RESERVE);
	gboolean bKeepBelow0 = (pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW);
	gboolean bAutoHideOnOverlap0 = (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP);
	gboolean bAutoHideOnAnyOverlap0 = (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY);
	gboolean bAutoHide0 = (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE);
	gboolean bShortKey0 = (pDock->iVisibility == CAIRO_DOCK_VISI_SHORTKEY);
	
	pDock->iVisibility = iVisibility;
	
	//\_______________ changement dans le Reserve Space.
	if (bReserveSpace != bReserveSpace0)
		cairo_dock_reserve_space_for_dock (pDock, bReserveSpace);
	
	//\_______________ changement dans le Keep below.
	if (bKeepBelow != bKeepBelow0)
	{
		if (bKeepBelow)
			cairo_dock_pop_down (pDock);
		else
			cairo_dock_pop_up (pDock);
	}
	
	//\_______________ changement dans l'Auto-Hide
	if (bAutoHideOnOverlap != bAutoHideOnOverlap0 ||
		bAutoHideOnAnyOverlap != bAutoHideOnAnyOverlap0 ||
		bAutoHide != bAutoHide0)
	{
		if (bAutoHide)
		{
			pDock->bTemporaryHidden = FALSE;
			pDock->bAutoHide = TRUE;
			cairo_dock_start_hiding (pDock);
		}
		else if (bAutoHideOnAnyOverlap)
		{
			pDock->bTemporaryHidden = pDock->bAutoHide;  // needed to use the following function
			gldi_dock_hide_if_any_window_overlap_or_show (pDock);
		}
		else
		{
			if (! bAutoHideOnOverlap)
			{
				pDock->bTemporaryHidden = FALSE;
				pDock->bAutoHide = FALSE;
				cairo_dock_start_showing (pDock);
			}
			if (bAutoHideOnOverlap)
			{
				pDock->bTemporaryHidden = pDock->bAutoHide;  // needed to use the following function
				gldi_dock_hide_show_if_current_window_is_on_our_way (pDock);
			}
		}
	}
	
	//\_______________ shortkey
	if (pDock->bIsMainDock)
	{
		if (bShortKey)  // option is enabled.
		{
			if (s_pPopupBinding && gldi_shortkey_could_grab (s_pPopupBinding))  // a shortkey has been registered and grabbed to show/hide the dock => hide the dock.
			{
				gtk_widget_hide (pDock->container.pWidget);
			}
			else  // bind couldn't be done (no shortkey or couldn't grab it).
			{
				// g_print ("bind couldn't be done (no shortkey or couldn't grab it).\n");
				pDock->iVisibility = CAIRO_DOCK_VISI_KEEP_ABOVE;
			}
		}
		else if (bShortKey0)  // option is now disabled => show the dock.
		{
			_reposition_root_docks (FALSE);  // FALSE => tous.
		}
	}
	
	//\_______________ on arrete/demarre la scrutation des bords.
	gboolean bIsPolling = (bAutoHide0 || bAutoHideOnOverlap0 || bAutoHideOnAnyOverlap0 || bKeepBelow0);
	gboolean bShouldPoll = (bAutoHide || bAutoHideOnOverlap || bAutoHideOnAnyOverlap || bKeepBelow);
	if (bIsPolling && ! bShouldPoll)
		_stop_polling_screen_edge ();
	else if (!bIsPolling && bShouldPoll)
		_start_polling_screen_edge ();
		
	//\_______________ layer shell additions
	GldiContainerLayer layer = CAIRO_DOCK_LAYER_BOTTOM;
	if (iVisibility == CAIRO_DOCK_VISI_KEEP_ABOVE || iVisibility == CAIRO_DOCK_VISI_RESERVE)
	{
		layer = CAIRO_DOCK_LAYER_TOP;
	}
	gldi_container_set_layer ( &(pDock->container), layer);
}


  /////////////////
 /// CALLBACKS ///
/////////////////

static gboolean _autohide_after_shortkey (CairoDock *pDock)
{
	if (pDock->iVisibility == CAIRO_DOCK_VISI_SHORTKEY && gldi_container_is_visible (CAIRO_CONTAINER (pDock)) && ! pDock->container.bInside)
		gtk_widget_hide (pDock->container.pWidget);
	pDock->iSidHideBack = 0;
	return FALSE;
}
static void _show_dock_at_mouse (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->iVisibility != CAIRO_DOCK_VISI_SHORTKEY)
		return;
	
	if (gldi_container_is_visible (CAIRO_CONTAINER (pDock)))  // already visible -> hide (toggle)
	{
		gtk_widget_hide (pDock->container.pWidget);
	}
	else  // invisible -> show
	{
		// calculate the position where the dock will be shown (under the mouse)
		gldi_container_update_mouse_position (CAIRO_CONTAINER (pDock));
	
		int W = gldi_dock_get_screen_width (pDock), H = gldi_dock_get_screen_height (pDock);
		int iScreenOffsetX = gldi_dock_get_screen_offset_x (pDock), iScreenOffsetY = gldi_dock_get_screen_offset_y (pDock);
		///pDock->iGapX = pDock->container.iWindowPositionX + iMouseX - g_desktopGeometry.iScreenWidth[pDock->container.bIsHorizontal] * pDock->fAlign;
		///pDock->iGapY = (pDock->container.bDirectionUp ? g_desktopGeometry.iScreenHeight[pDock->container.bIsHorizontal] - (pDock->container.iWindowPositionY + iMouseY) : pDock->container.iWindowPositionY + iMouseY);
		pDock->iGapX = pDock->container.iWindowPositionX + pDock->container.iMouseX - (W - pDock->container.iWidth) * pDock->fAlign - pDock->container.iWidth/2 - iScreenOffsetX;
		pDock->iGapY = (pDock->container.bDirectionUp ? H - (pDock->container.iWindowPositionY + pDock->container.iMouseY) : pDock->container.iWindowPositionY + pDock->container.iMouseY) - iScreenOffsetY;
		cd_debug (" => %d;%d", g_pMainDock->iGapX, g_pMainDock->iGapY);
	
		int iNewPositionX, iNewPositionY;
		cairo_dock_get_window_position_at_balance (pDock,
			pDock->container.iWidth, pDock->container.iHeight,
			&iNewPositionX, &iNewPositionY);
		cd_debug (" ==> %d;%d", iNewPositionX, iNewPositionY);
		if (iNewPositionX < 0)
			iNewPositionX = 0;
		else if (iNewPositionX + pDock->container.iWidth > W)
			iNewPositionX = W - pDock->container.iWidth;
	
		if (iNewPositionY < 0)
			iNewPositionY = 0;
		else if (iNewPositionY + pDock->container.iHeight > H)
			iNewPositionY = H - pDock->container.iHeight;
		
		// show the dock
		gtk_window_move (GTK_WINDOW (pDock->container.pWidget),
			(pDock->container.bIsHorizontal ? iNewPositionX : iNewPositionY),
			(pDock->container.bIsHorizontal ? iNewPositionY : iNewPositionX));
		gtk_widget_show (pDock->container.pWidget);
		
		// schedule a hiding in a few seconds
		if (pDock->iSidHideBack != 0)
			g_source_remove (pDock->iSidHideBack);
		pDock->iSidHideBack = g_timeout_add (3000, (GSourceFunc) _autohide_after_shortkey, (gpointer) pDock);
	}
}
static void _raise_from_shortcut (G_GNUC_UNUSED const char *cKeyShortcut, G_GNUC_UNUSED gpointer data)
{
	// g_print ("shortkey\n");
	gldi_docks_foreach_root ((GFunc)_show_dock_at_mouse, NULL);
}

static gboolean _on_screen_geometry_changed (G_GNUC_UNUSED gpointer data, gboolean bSizeHasChanged)
{
	if (bSizeHasChanged)
		_reposition_root_docks (FALSE);  // FALSE <=> main dock included
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_new_dialog (G_GNUC_UNUSED gpointer data, CairoDialog *pDialog)
{
	//\________________ hide sub-dock or label that would overlap it
	Icon *pIcon = pDialog->pIcon;
	if (! pIcon)
		return GLDI_NOTIFICATION_LET_PASS;
	
	if (pIcon->pSubDock)  // un sous-dock par-dessus le dialogue est tres genant.
	{
		cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pIcon->pSubDock));
	}
	
	GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
	if (CAIRO_DOCK_IS_DOCK (pContainer) && cairo_dock_get_icon_max_scale (pIcon) < 1.01)  // view without zoom, the dialog is stuck to the icon, and therefore is under the label, so hide this one.
	{
		if (pIcon->iHideLabel == 0)
			gtk_widget_queue_draw (pContainer->pWidget);
		pIcon->iHideLabel ++;
	}
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _render_dock_notification (G_GNUC_UNUSED gpointer pUserData, CairoDock *pDock, cairo_t *pCairoContext)
{
	if (pCairoContext)  // cairo
	{
		if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->pre_render)
			g_pHidingBackend->pre_render (pDock, pDock->fHideOffset, pCairoContext);
	
		if (pDock->iFadeCounter != 0 && g_pKeepingBelowBackend != NULL && g_pKeepingBelowBackend->pre_render)
			g_pKeepingBelowBackend->pre_render (pDock, (double) pDock->iFadeCounter / myBackendsParam.iHideNbSteps, pCairoContext);
		
		/// TODO: see if it's ok to not use the optimized rendering any more...
		/// if not, we can probably get the clip on the cairo context
		pDock->pRenderer->render (pCairoContext, pDock);
		
		if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->post_render)
			g_pHidingBackend->post_render (pDock, pDock->fHideOffset, pCairoContext);
	
		if (pDock->iFadeCounter != 0 && g_pKeepingBelowBackend != NULL && g_pKeepingBelowBackend->post_render)
			g_pKeepingBelowBackend->post_render (pDock, (double) pDock->iFadeCounter / myBackendsParam.iHideNbSteps, pCairoContext);
	}
	else  // opengl
	{
		if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->pre_render_opengl)
			g_pHidingBackend->pre_render_opengl (pDock, pDock->fHideOffset);
		
		if (pDock->iFadeCounter != 0 && g_pKeepingBelowBackend != NULL && g_pKeepingBelowBackend->pre_render_opengl)
			g_pKeepingBelowBackend->pre_render_opengl (pDock, (double) pDock->iFadeCounter / myBackendsParam.iHideNbSteps);
		
		pDock->pRenderer->render_opengl (pDock);
		
		if (pDock->fHideOffset != 0 && g_pHidingBackend != NULL && g_pHidingBackend->post_render_opengl)
			g_pHidingBackend->post_render_opengl (pDock, pDock->fHideOffset);
		
		if (pDock->iFadeCounter != 0 && g_pKeepingBelowBackend != NULL && g_pKeepingBelowBackend->post_render_opengl)
			g_pKeepingBelowBackend->post_render_opengl (pDock, (double) pDock->iFadeCounter / myBackendsParam.iHideNbSteps);
	}
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean _on_leave_dock (G_GNUC_UNUSED gpointer data, CairoDock *pDock, G_GNUC_UNUSED gboolean *bStartAnimation)
{
	//g_print ("%s (%d, %d)\n", __func__, pDock->iRefCount, pDock->bHasModalWindow);
	
	//\_______________ On lance l'animation du dock.
	if (pDock->iRefCount == 0)
	{
		//g_print ("%s (auto-hide:%d)\n", __func__, pDock->bAutoHide);
		if (pDock->bAutoHide)
		{
			///pDock->fFoldingFactor = (myBackendsParam.bAnimateOnAutoHide ? 0.001 : 0.);
			cairo_dock_start_hiding (pDock);
		}
	}
	else if (pDock->icons != NULL)
	{
		pDock->fFoldingFactor = (myDocksParam.bAnimateSubDock ? 0.001 : 0.);
		Icon *pIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
		//g_print ("'%s' se replie\n", pIcon?pIcon->cName:"none");
		gldi_object_notify (pIcon, NOTIFICATION_UNFOLD_SUBDOCK, pIcon);
	}
	//g_print ("start shrinking\n");
	cairo_dock_start_shrinking (pDock);  // on commence a faire diminuer la taille des icones.
	return GLDI_NOTIFICATION_LET_PASS;
}

static void _update_removing_inserting_icon_size (Icon *icon)
{
	icon->fInsertRemoveFactor *= .85;
	if (icon->fInsertRemoveFactor > 0)
	{
		if (icon->fInsertRemoveFactor < 0.05)
			icon->fInsertRemoveFactor = 0.05;
	}
	else if (icon->fInsertRemoveFactor < 0)
	{
		if (icon->fInsertRemoveFactor > -0.05)
			icon->fInsertRemoveFactor = -0.05;
	}
}

static gboolean on_update_inserting_removing_icon (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon, CairoDock *pDock, gboolean *bContinueAnimation)
{
	if (pIcon->iGlideDirection != 0)
	{
		pIcon->fGlideOffset += pIcon->iGlideDirection * .1;
		if (fabs (pIcon->fGlideOffset) > .99)
		{
			pIcon->fGlideOffset = pIcon->iGlideDirection;
			pIcon->iGlideDirection = 0;
		}
		else if (fabs (pIcon->fGlideOffset) < .01)
		{
			pIcon->fGlideOffset = 0;
			pIcon->iGlideDirection = 0;
		}
		*bContinueAnimation = TRUE;
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	}
	
	if (pIcon->fInsertRemoveFactor != 0) // the icon is being inserted/removed
	{
		_update_removing_inserting_icon_size (pIcon);
		if (fabs (pIcon->fInsertRemoveFactor) > 0.05)  // the animation is not yet finished
		{
			cairo_dock_mark_icon_as_inserting_removing (pIcon);
			*bContinueAnimation = TRUE;
		}
		cairo_dock_redraw_container (CAIRO_CONTAINER (pDock));
	}
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean on_insert_remove_icon (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon, G_GNUC_UNUSED CairoDock *pDock)
{
	if (pIcon->fInsertRemoveFactor == 0)  // animation not needed.
		return GLDI_NOTIFICATION_LET_PASS;
	
	cairo_dock_mark_icon_as_inserting_removing (pIcon);  // On prend en charge le dessin de l'icone pendant sa phase d'insertion/suppression.
	
	return GLDI_NOTIFICATION_LET_PASS;
}

static gboolean on_stop_inserting_removing_icon (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon)
{
	pIcon->fGlideOffset = 0;
	pIcon->iGlideDirection = 0;
	return GLDI_NOTIFICATION_LET_PASS;
}


  //////////////////
 /// GET CONFIG ///
//////////////////

static gboolean get_config (GKeyFile *pKeyFile, CairoDocksParam *pDocksParam)
{
	gboolean bFlushConfFileNeeded = FALSE;
	
	CairoDocksParam *pBackground = pDocksParam;
	CairoDocksParam *pPosition = pDocksParam;
	CairoDocksParam *pAccessibility = pDocksParam;
	CairoDocksParam *pSystem = pDocksParam;
	
	// frame
	pBackground->iDockRadius = cairo_dock_get_integer_key_value (pKeyFile, "Background", "corner radius", &bFlushConfFileNeeded, 12, NULL, NULL);

	pBackground->iDockLineWidth = cairo_dock_get_integer_key_value (pKeyFile, "Background", "line width", &bFlushConfFileNeeded, 2, NULL, NULL);

	pBackground->iFrameMargin = cairo_dock_get_integer_key_value (pKeyFile, "Background", "frame margin", &bFlushConfFileNeeded, 2, NULL, NULL);

	GldiColor couleur = {{0., 0., 0.6, 0.4}};
	cairo_dock_get_color_key_value (pKeyFile, "Background", "line color", &bFlushConfFileNeeded, &pBackground->fLineColor, &couleur, NULL, NULL);

	pBackground->bRoundedBottomCorner = cairo_dock_get_boolean_key_value (pKeyFile, "Background", "rounded bottom corner", &bFlushConfFileNeeded, TRUE, NULL, NULL);
	
	// background image
	int iStyle = cairo_dock_get_integer_key_value (pKeyFile, "Background", "style", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 pour intercepter le cas ou la cle n'existe pas.
	if (iStyle == -1)  // old params < 3.4
	{
		iStyle = g_key_file_get_integer (pKeyFile, "Background", "fill bg", NULL);
		iStyle ++;
		g_key_file_set_integer (pKeyFile, "Background", "style", iStyle);
	}
	
	if (iStyle == 0)
	{
		pBackground->bUseDefaultColors = TRUE;
		pBackground->iDockRadius = myStyleParam.iCornerRadius;
		pBackground->iDockLineWidth = myStyleParam.iLineWidth;
	}
	else if (iStyle == 1)
	{
		gchar *cBgImage = (iStyle == 1 ? cairo_dock_get_string_key_value (pKeyFile, "Background", "background image", &bFlushConfFileNeeded, NULL, NULL, NULL) : NULL);
		if (cBgImage != NULL)
		{
			pBackground->cBackgroundImageFile = cairo_dock_search_image_s_path (cBgImage);
			g_free (cBgImage);
			pBackground->fBackgroundImageAlpha = cairo_dock_get_double_key_value (pKeyFile, "Background", "image alpha", &bFlushConfFileNeeded, 0.5, NULL, NULL);
			pBackground->bBackgroundImageRepeat = cairo_dock_get_boolean_key_value (pKeyFile, "Background", "repeat image", &bFlushConfFileNeeded, FALSE, NULL, NULL);
		}
	}
	
	// background gradation
	if (iStyle != 0 && pBackground->cBackgroundImageFile == NULL)
	{
		pBackground->iNbStripes = cairo_dock_get_integer_key_value (pKeyFile, "Background", "number of stripes", &bFlushConfFileNeeded, 10, NULL, NULL);
		
		if (pBackground->iNbStripes != 0)
		{
			pBackground->fStripesWidth = MAX (.01, MIN (.99, cairo_dock_get_double_key_value (pKeyFile, "Background", "stripes width", &bFlushConfFileNeeded, 0.2, NULL, NULL))) / pBackground->iNbStripes;
		}
		GldiColor couleur3 = {{.7, .7, 1., .7}};
		cairo_dock_get_color_key_value (pKeyFile, "Background", "stripes color dark", &bFlushConfFileNeeded, &pBackground->fStripesColorDark, &couleur3, NULL, NULL);
		
		GldiColor couleur2 = {{.7, .9, .7, .4}};
		cairo_dock_get_color_key_value (pKeyFile, "Background", "stripes color bright", &bFlushConfFileNeeded, &pBackground->fStripesColorBright, &couleur2, NULL, NULL);
		
		pBackground->fStripesAngle = cairo_dock_get_double_key_value (pKeyFile, "Background", "stripes angle", &bFlushConfFileNeeded, 90., NULL, NULL);
	}
	
	pAccessibility->bExtendedMode = cairo_dock_get_boolean_key_value (pKeyFile, "Background", "extended", &bFlushConfFileNeeded, FALSE, "Accessibility", NULL);
	
	// hidden bg
	GldiColor hcolor = {{.8, .8, .8, .5}};
	cairo_dock_get_color_key_value (pKeyFile, "Background", "hidden bg color", &bFlushConfFileNeeded, &pBackground->fHiddenBg, &hcolor, NULL, NULL);
	
	// position
	pPosition->iGapX = cairo_dock_get_integer_key_value (pKeyFile, "Position", "x gap", &bFlushConfFileNeeded, 0, NULL, NULL);
	pPosition->iGapY = cairo_dock_get_integer_key_value (pKeyFile, "Position", "y gap", &bFlushConfFileNeeded, 0, NULL, NULL);

	pPosition->iScreenBorder = cairo_dock_get_integer_key_value (pKeyFile, "Position", "screen border", &bFlushConfFileNeeded, 0, NULL, NULL);
	if (pPosition->iScreenBorder >= CAIRO_DOCK_NB_POSITIONS)
		pPosition->iScreenBorder = 0;

	pPosition->fAlign = cairo_dock_get_double_key_value (pKeyFile, "Position", "alignment", &bFlushConfFileNeeded, 0.5, NULL, NULL);
	
	pPosition->iNumScreen = cairo_dock_get_integer_key_value (pKeyFile, "Position", "num_screen", &bFlushConfFileNeeded, GLDI_DEFAULT_SCREEN, NULL, NULL);  // Note: if this screen doesn't exist at this time, we keep this number anyway, in case it is plugged later. Until then, it will point on the X screen.
	if (g_key_file_has_key (pKeyFile, "Position", "xinerama", NULL))  // "xinerama" and "num screen" old keys
	{
		if (g_key_file_get_boolean (pKeyFile," Position", "xinerama", NULL))  // xinerama was used -> set num-screen back
		{
			pPosition->iNumScreen = g_key_file_get_integer (pKeyFile, "Position", "num screen", NULL); // "num screen" was the old key
			g_key_file_set_integer (pKeyFile, "Position", "num_screen", pPosition->iNumScreen);
		}
	}
	
	//\____________________ Visibilite
	int iVisibility = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "visibility", &bFlushConfFileNeeded, -1, NULL, NULL);  // -1 pour pouvoir intercepter le cas ou la cle n'existe pas.
	
	gchar *cShortkey = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "raise shortcut", &bFlushConfFileNeeded, NULL, "Position", NULL);
	
	pAccessibility->cHideEffect = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "hide effect", &bFlushConfFileNeeded, NULL, NULL, NULL);
	
	if (iVisibility == -1)  // option nouvelle 2.1.3
	{
		if (g_key_file_get_boolean (pKeyFile, "Accessibility", "reserve space", NULL))
			iVisibility = CAIRO_DOCK_VISI_KEEP_ABOVE;
		else if (g_key_file_get_boolean (pKeyFile, "Accessibility", "pop-up", NULL))  // on force au nouveau mode.
		{
			iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY;
			pAccessibility->cHideEffect = g_strdup_printf ("Fade out");  // on force a "fade out" pour garder le meme effet.
			g_key_file_set_string (pKeyFile, "Accessibility", "hide effect", pAccessibility->cHideEffect);
		}
		else if (g_key_file_get_boolean (pKeyFile, "Accessibility", "auto-hide", NULL))
			iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE;
		else if (g_key_file_get_boolean (pKeyFile, "Accessibility", "auto quick hide on max", NULL))
			iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY;
		else if (cShortkey)
		{
			iVisibility = CAIRO_DOCK_VISI_SHORTKEY;
			pAccessibility->cRaiseDockShortcut = cShortkey;
			cShortkey = NULL;
		}
		else
			iVisibility = CAIRO_DOCK_VISI_KEEP_ABOVE;
		
		g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
	}
	else
	{
		if (pAccessibility->cHideEffect == NULL)  // nouvelle option 2.2.0, cela a change l'ordre du menu.
		{
			// avant c'etait : KEEP_ABOVE, RESERVE, KEEP_BELOW, AUTO_HIDE,       HIDE_ON_MAXIMIZED,   SHORTKEY
			// mtn c'est     : KEEP_ABOVE, RESERVE, KEEP_BELOW, HIDE_ON_OVERLAP, HIDE_ON_OVERLAP_ANY, AUTO_HIDE, VISI_SHORTKEY,
			if (iVisibility == 2)  // on force au nouveau mode.
			{
				iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY;
				pAccessibility->cHideEffect = g_strdup_printf ("Fade out");  // on force a "fade out" pour garder le meme effet.
				g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
				g_key_file_set_string (pKeyFile, "Accessibility", "hide effect", pAccessibility->cHideEffect);
			}
			else if (iVisibility == 3)
			{
				iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE;
				g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
			}
			else if (iVisibility == 4)
			{
				iVisibility = CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY;
				g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
			}
			else if (iVisibility == 5)
			{
				iVisibility = CAIRO_DOCK_VISI_SHORTKEY;
				g_key_file_set_integer (pKeyFile, "Accessibility", "visibility", iVisibility);
			}
		}
		if (iVisibility == CAIRO_DOCK_VISI_SHORTKEY)
		{
			pAccessibility->cRaiseDockShortcut = cShortkey;
			cShortkey = NULL;
		}
	}
	pAccessibility->iVisibility = iVisibility;
	g_free (cShortkey);
	if (pAccessibility->cHideEffect == NULL) 
	{
		pAccessibility->cHideEffect = g_strdup_printf ("Move down");
		g_key_file_set_string (pKeyFile, "Accessibility", "hide effect", pAccessibility->cHideEffect);
	}
	
	pAccessibility->iCallbackMethod = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "callback", &bFlushConfFileNeeded, CAIRO_HIT_DOCK_PLACE, NULL, NULL);
	
	if (pAccessibility->iCallbackMethod == CAIRO_HIT_ZONE)
	{
		if (! g_key_file_has_key (pKeyFile, "Accessibility", "zone size", NULL))
		{
			pAccessibility->iZoneWidth = 100;
			pAccessibility->iZoneHeight = 10;
			int list[2] = {pAccessibility->iZoneWidth, pAccessibility->iZoneHeight};
			g_key_file_set_integer_list (pKeyFile, "Accessibility", "zone size", list, 2);
		}
		cairo_dock_get_size_key_value_helper (pKeyFile, "Accessibility", "zone ", bFlushConfFileNeeded, pAccessibility->iZoneWidth, pAccessibility->iZoneHeight);
		if (pAccessibility->iZoneWidth < 20)
			pAccessibility->iZoneWidth = 20;
		if (pAccessibility->iZoneHeight < 2)
			pAccessibility->iZoneHeight = 2;
		pAccessibility->cZoneImage = cairo_dock_get_string_key_value (pKeyFile, "Accessibility", "callback image", &bFlushConfFileNeeded, 0, "Background", NULL);
		pAccessibility->fZoneAlpha = 1.;  // on laisse l'utilisateur definir la transparence qu'il souhaite directement dans l'image.
	}
	
	//\____________________ Autres parametres.
	double fSensitivity = cairo_dock_get_double_key_value (pKeyFile, "Accessibility", "edge sensitivity", &bFlushConfFileNeeded, -1, NULL, NULL);  // replace "unhide delay"
	if (fSensitivity < 0)  // old param
	{
		int iUnhideDockDelay = g_key_file_get_integer (pKeyFile, "Accessibility", "unhide delay", NULL);
		fSensitivity = iUnhideDockDelay / 1500.;  // 0 -> 0 = sensitive, 1500 -> 1 = not sensitive
		g_key_file_set_double (pKeyFile, "Accessibility", "edge sensitivity", fSensitivity);
	}
	pAccessibility->iUnhideDockDelay = fSensitivity * 1000;  // so we decreased the old delay by 1.5, since we handle mouse movements better.
	
	//\____________________ sous-docks.
	pAccessibility->iLeaveSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "leaving delay", &bFlushConfFileNeeded, 330, "System", NULL);
	pAccessibility->iShowSubDockDelay = cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "show delay", &bFlushConfFileNeeded, 300, "System", NULL);
	if (!g_key_file_has_key (pKeyFile, "Accessibility", "show_on_click", NULL))
	{
		pAccessibility->bShowSubDockOnClick = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "show on click", &bFlushConfFileNeeded, FALSE, "System", NULL);
		g_key_file_set_integer (pKeyFile, "Accessibility", "show_on_click", pAccessibility->bShowSubDockOnClick ? 1 : 0);
		bFlushConfFileNeeded = TRUE;
	}
	else
		pAccessibility->bShowSubDockOnClick = (cairo_dock_get_integer_key_value (pKeyFile, "Accessibility", "show_on_click", &bFlushConfFileNeeded, 0, NULL, NULL) == 1);
	
	//\____________________ lock
	///pAccessibility->bLockAll = cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "lock all", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	pAccessibility->bLockIcons = pAccessibility->bLockAll || cairo_dock_get_boolean_key_value (pKeyFile, "Accessibility", "lock icons", &bFlushConfFileNeeded, FALSE, NULL, NULL);
	
	// system
	pSystem->bAnimateSubDock = cairo_dock_get_boolean_key_value (pKeyFile, "System", "animate subdocks", &bFlushConfFileNeeded, TRUE, "Sub-Docks", NULL);
	
	return bFlushConfFileNeeded;
}


  ////////////////////
 /// RESET CONFIG ///
////////////////////

static void reset_config (CairoDocksParam *pDocksParam)
{
	CairoDocksParam *pBackground = pDocksParam;
	CairoDocksParam *pAccessibility = pDocksParam;
	
	// background
	g_free (pBackground->cBackgroundImageFile);
	
	// accessibility
	g_free (pAccessibility->cRaiseDockShortcut);
	g_free (pAccessibility->cHideEffect);
	g_free (pAccessibility->cZoneImage);
}


  ////////////
 /// LOAD ///
////////////

static void _load_visible_zone (const gchar *cVisibleZoneImageFile, int iVisibleZoneWidth, int iVisibleZoneHeight, double fVisibleZoneAlpha)
{
	cairo_dock_unload_image_buffer (&g_pVisibleZoneBuffer);
	
	cairo_dock_load_image_buffer_full (&g_pVisibleZoneBuffer,
		cVisibleZoneImageFile,
		iVisibleZoneWidth,
		iVisibleZoneHeight,
		CAIRO_DOCK_FILL_SPACE,
		fVisibleZoneAlpha);
}
static void load (void)
{
	_load_visible_zone (myDocksParam.cZoneImage, myDocksParam.iZoneWidth, myDocksParam.iZoneHeight, myDocksParam.fZoneAlpha);
	
	g_pHidingBackend = cairo_dock_get_hiding_effect (myDocksParam.cHideEffect);
	
	if (g_pKeepingBelowBackend == NULL)  // pas d'option en config pour ca.
		g_pKeepingBelowBackend = cairo_dock_get_hiding_effect ("Fade out");
	
	// the first main dock doesn't have a config file, its parameters are the global ones.
	if (g_pMainDock)
	{
		g_pMainDock->iGapX = myDocksParam.iGapX;
		g_pMainDock->iGapY = myDocksParam.iGapY;
		g_pMainDock->fAlign = myDocksParam.fAlign;
		g_pMainDock->iNumScreen = myDocksParam.iNumScreen;
		g_pMainDock->bExtendedMode = myDocksParam.bExtendedMode;
		
		_set_dock_orientation (g_pMainDock, myDocksParam.iScreenBorder);
		cairo_dock_move_resize_dock (g_pMainDock);
		
		g_pMainDock->fFlatDockWidth = - myIconsParam.iIconGap;  // car on ne le connaissait pas encore au moment de sa creation.
		
		// register a key binding
		if (myDocksParam.iVisibility == CAIRO_DOCK_VISI_SHORTKEY)  // register a key binding
		{
			if (s_pPopupBinding == NULL)
			{
				s_pPopupBinding = gldi_shortkey_new (myDocksParam.cRaiseDockShortcut,
					"Cairo-Dock",
					_("Pop up the main dock"),
					GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON,
					g_cConfFile,
					"Accessibility",
					"raise shortcut",
					(CDBindkeyHandler) _raise_from_shortcut,
					NULL);
			}
			else
			{
				gldi_shortkey_rebind (s_pPopupBinding, myDocksParam.cRaiseDockShortcut, NULL);
			}
		}
		
		gldi_dock_set_visibility (g_pMainDock, myDocksParam.iVisibility);
	}
}


  //////////////
 /// RELOAD ///
//////////////

static void _reload_bg (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	pDock->backgroundBuffer.iWidth ++;  // force the reload
	cairo_dock_trigger_load_dock_background (pDock);
}
static void _init_hiding (CairoDock *pDock, G_GNUC_UNUSED gpointer data)
{
	if (pDock->bIsShowing || pDock->bIsHiding)
	{
		g_pHidingBackend->init (pDock);
	}
}
static void reload (CairoDocksParam *pPrevDocksParam, CairoDocksParam *pDocksParam)
{
	CairoDocksParam *pBackground = pDocksParam;
	CairoDocksParam *pPosition = pDocksParam;
	CairoDocksParam *pAccessibility = pDocksParam;
	// CairoDocksParam *pViews = pDocksParam;
	// CairoDocksParam *pSystem = pDocksParam;
	CairoDocksParam *pPrevBackground = pPrevDocksParam;
	CairoDocksParam *pPrevPosition = pPrevDocksParam;
	CairoDocksParam *pPrevAccessibility = pPrevDocksParam;
	// CairoDocksParam *pPrevViews = pPrevDocksParam;
	// CairoDocksParam *pPrevSystem = pPrevDocksParam;
	CairoDock *pDock = g_pMainDock;
	
	// background
	gldi_docks_foreach_root ((GFunc)_reload_bg, NULL);
	
	// position
	pDock->iNumScreen = pPosition->iNumScreen;
	
	if (pPosition->iNumScreen != pPrevPosition->iNumScreen)
	{
		_reposition_root_docks (TRUE);  // on replace tous les docks racines sauf le main dock, puisque c'est fait apres.
	}
	
	CairoDockTypeHorizontality bWasHorizontal = pDock->container.bIsHorizontal;
	if (pPosition->iScreenBorder != pPrevPosition->iScreenBorder)
	{
		_set_dock_orientation (pDock, pPosition->iScreenBorder);
		cairo_dock_reload_buffers_in_dock (pDock, TRUE, FALSE);  // icons may have a different width and height, so changing the orientation will affect them.  also, stack-icons may be drawn differently according to the orientation (ex.: box).
	}
	pDock->bExtendedMode = pBackground->bExtendedMode;
	pDock->iGapX = pPosition->iGapX;
	pDock->iGapY = pPosition->iGapY;
	pDock->fAlign = pPosition->fAlign;
	
	if (pPosition->iNumScreen != pPrevPosition->iNumScreen
	|| pPosition->iScreenBorder != pPrevPosition->iScreenBorder  // if the orientation or the screen has changed, the available size may have changed too
	|| pPosition->iGapX != pPrevPosition->iGapX
	|| pPosition->iGapY != pPrevPosition->iGapY)
	{
		cairo_dock_update_dock_size (pDock);
		cairo_dock_move_resize_dock (pDock);
		if (bWasHorizontal != pDock->container.bIsHorizontal)
			pDock->container.iWidth --;  // la taille dans le referentiel du dock ne change pas meme si on change d'horizontalite, par contre la taille de la fenetre change. On introduit donc un biais ici pour forcer le configure-event a faire son travail, sinon ca fausse le redraw.
	}
	else if (pPosition->fAlign != pPrevPosition->fAlign  // need to update the input zone
	|| pPrevBackground->iDockLineWidth != pBackground->iDockLineWidth  // frame size has changed
	|| pPrevBackground->iFrameMargin != pBackground->iFrameMargin)  // idem
	{
		cairo_dock_update_dock_size (pDock);
	}
	
	///cairo_dock_calculate_dock_icons (pDock);
	
	gldi_docks_redraw_all_root ();  // the background is a global parameter
	
	// accessibility
	//\_______________ Shortkey.
	if (pAccessibility->iVisibility == CAIRO_DOCK_VISI_SHORTKEY)
	{
		if (s_pPopupBinding == NULL)
		{
			s_pPopupBinding = gldi_shortkey_new (myDocksParam.cRaiseDockShortcut,
				"Cairo-Dock",
				_("Pop up the main dock"),
				GLDI_SHARE_DATA_DIR"/"CAIRO_DOCK_ICON,
				g_cCurrentThemePath,
				"Accessibility",
				"raise shortcut",
				(CDBindkeyHandler) _raise_from_shortcut,
				NULL);
		}
		else
		{
			gldi_shortkey_rebind (s_pPopupBinding, myDocksParam.cRaiseDockShortcut, NULL);
		}
	}
	else
	{
		gldi_object_unref (GLDI_OBJECT(s_pPopupBinding));
		s_pPopupBinding = NULL;
	}
	
	//\_______________ Hiding effect.
	if (g_strcmp0 (pAccessibility->cHideEffect, pPrevAccessibility->cHideEffect) != 0)
	{
		g_pHidingBackend = cairo_dock_get_hiding_effect (pAccessibility->cHideEffect);
		if (g_pHidingBackend && g_pHidingBackend->init)
		{
			gldi_docks_foreach_root ((GFunc)_init_hiding, NULL);  // si le dock est en cours d'animation, comme le backend est nouveau, il n'a donc pas ete initialise au debut de l'animation => on le fait ici.
		}
	}
	
	//\_______________ Callback zone.
	if (g_strcmp0 (pAccessibility->cZoneImage, pPrevAccessibility->cZoneImage) != 0
	|| pAccessibility->iZoneWidth != pPrevAccessibility->iZoneWidth
	|| pAccessibility->iZoneHeight != pPrevAccessibility->iZoneHeight
	|| pAccessibility->fZoneAlpha != pPrevAccessibility->fZoneAlpha)
	{
		_load_visible_zone (pAccessibility->cZoneImage, pAccessibility->iZoneWidth, pAccessibility->iZoneHeight, pAccessibility->fZoneAlpha);
		
		gldi_docks_redraw_all_root ();
	}
	
	gldi_dock_set_visibility (pDock, pAccessibility->iVisibility);
}


  //////////////
 /// UNLOAD ///
//////////////

static void unload (void)
{
	cairo_dock_unload_image_buffer (&g_pVisibleZoneBuffer);
	
	_stop_polling_screen_edge_now ();
	s_bQuickHide = FALSE;
	
	gldi_object_unref (GLDI_OBJECT(s_pPopupBinding));
	s_pPopupBinding = NULL;
}


  ////////////
 /// INIT ///
////////////

static gboolean on_style_changed (G_GNUC_UNUSED gpointer data)
{
	cd_debug ("Docks: style change to %d", myDocksParam.bUseDefaultColors);
	if (myDocksParam.bUseDefaultColors)  // reload bg
	{
		cd_debug (" reload dock's bg...");
		
		gboolean bNeedUpdateSize = (myDocksParam.iDockLineWidth != myStyleParam.iLineWidth);  // frame size changed
		myDocksParam.iDockRadius = myStyleParam.iCornerRadius;
		myDocksParam.iDockLineWidth = myStyleParam.iLineWidth;
		
		if (bNeedUpdateSize)  // update docks size and background
			gldi_docks_foreach_root ((GFunc)cairo_dock_update_dock_size, NULL);
		else  // update docks background for color change
			gldi_docks_foreach_root ((GFunc)_reload_bg, NULL);
	}
	return GLDI_NOTIFICATION_LET_PASS;
}

static void init (void)
{
	s_hDocksTable = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		NULL,  // name of the dock (points directly to the dock)
		NULL);  // dock
	
	/**gldi_object_register_notification (&myDockObjectMgr,
		NOTIFICATION_RENDER,
		(GldiNotificationFunc) _render_dock_notification,
		GLDI_RUN_FIRST, NULL);*/
	gldi_object_register_notification (&myDockObjectMgr,
		NOTIFICATION_LEAVE_DOCK,
		(GldiNotificationFunc) _on_leave_dock,  // is a notification so that others can prevent a dock from hiding (ex Slide view)
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myDockObjectMgr,
		NOTIFICATION_INSERT_ICON,
		(GldiNotificationFunc) on_insert_remove_icon,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDockObjectMgr,
		NOTIFICATION_REMOVE_ICON,
		(GldiNotificationFunc) on_insert_remove_icon,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myIconObjectMgr,
		NOTIFICATION_UPDATE_ICON,
		(GldiNotificationFunc) on_update_inserting_removing_icon,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myIconObjectMgr,
		NOTIFICATION_STOP_ICON,
		(GldiNotificationFunc) on_stop_inserting_removing_icon,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myDesktopMgr,
		NOTIFICATION_DESKTOP_GEOMETRY_CHANGED,
		(GldiNotificationFunc) _on_screen_geometry_changed,
		GLDI_RUN_FIRST, NULL);
	gldi_object_register_notification (&myDialogObjectMgr,
		NOTIFICATION_NEW,
		(GldiNotificationFunc) _on_new_dialog,
		GLDI_RUN_AFTER, NULL);
	gldi_object_register_notification (&myStyleMgr,
		NOTIFICATION_STYLE_CHANGED,
		(GldiNotificationFunc) on_style_changed,
		GLDI_RUN_AFTER, NULL);
	
	// Avoid checking visibility on Wayland, since we do not have window
	// positions. This will avoid a dock being hidden by accident.
	if (!gldi_container_is_wayland_backend ())
		gldi_docks_visibility_start ();
}


  ///////////////
 /// MANAGER ///
///////////////

static void init_object (GldiObject *obj, gpointer attr)
{
	CairoDock *pDock = (CairoDock*)obj;
	CairoDockAttr *dattr = (CairoDockAttr*)attr;
	
	// check everything is ok
	g_return_if_fail (dattr != NULL && dattr->cDockName != NULL);
	
	if (g_hash_table_lookup (s_hDocksTable, dattr->cDockName) != NULL)
	{
		cd_warning ("a dock with the name '%s' is already registered", dattr->cDockName);
		return;
	}

	//\__________________ init layer-shell (if enabled) and also set parent; needs to happen before window is mapped
	if (dattr->bSubDock)
	{
		CairoDock *pParentDock = dattr->pParentDock;
		if (pParentDock == NULL)
			pParentDock = g_pMainDock;
		gtk_window_set_transient_for (GTK_WINDOW (pDock->container.pWidget), GTK_WINDOW (pParentDock->container.pWidget));
	}
	gldi_container_init_layer (&(pDock->container));
	
	//\__________________ init internals
	gldi_dock_init_internals (pDock);
	if (s_bKeepAbove)
		gtk_window_set_keep_above (GTK_WINDOW (pDock->container.pWidget), s_bKeepAbove);
	
	//\__________________ initialize its parameters (it's a root dock by default)
	pDock->cDockName = g_strdup (dattr->cDockName);
	pDock->iAvoidingMouseIconType = -1;
	pDock->fFlatDockWidth = - myIconsParam.iIconGap;
	pDock->fMagnitudeMax = 1.;
	pDock->fPostHideOffset = 1.;
	pDock->iInputState = CAIRO_DOCK_INPUT_AT_REST;  // le dock est cree au repos. La zone d'input sera mis en place lors du configure.
	pDock->iIconSize = myIconsParam.iIconWidth;  // by default
	
	gldi_object_register_notification (pDock,
		NOTIFICATION_RENDER,
		(GldiNotificationFunc) _render_dock_notification,
		GLDI_RUN_FIRST, NULL);  /// we connect here, to pass before the manager... try to avoid this hack...
	
	//\__________________ register the dock
	if (g_hash_table_size (s_hDocksTable) == 0)  // c'est le 1er.
	{
		pDock->bIsMainDock = TRUE;
		g_pMainDock = pDock;
	}
	g_hash_table_insert (s_hDocksTable, pDock->cDockName, pDock);
	
	//\__________________ set the icons.
	GList *pIconList = dattr->pIconList;
	
	gldi_automatic_separators_add_in_list (pIconList);
	
	pDock->icons = pIconList;  // set icons now, before we set the ratio and the renderer.
	Icon *icon;
	GList *ic;
	for (ic = pIconList; ic != NULL; ic = ic->next)
	{
		icon = ic->data;
		if (icon->cParentDockName == NULL)
			icon->cParentDockName = g_strdup (pDock->cDockName);
		cairo_dock_set_icon_container (icon, pDock);
	}
	
	//\__________________ 
	if (! dattr->bSubDock)
	{
		gtk_window_set_title (GTK_WINDOW (pDock->container.pWidget), "cairo-dock");
		
		//\__________________ register it as a main dock
		s_pRootDockList = g_list_prepend (s_pRootDockList, pDock);
	
		//\__________________ set additional params from its config file
		_get_root_dock_config (pDock);
	}
	else
	{
		gtk_window_set_title (GTK_WINDOW (pDock->container.pWidget), "cairo-dock-sub");
		pDock->iRefCount = 1;
		
		//\__________________ set additional params from its parent dock.
		CairoDock *pParentDock = dattr->pParentDock;
		if (pParentDock == NULL)
			pParentDock = g_pMainDock;
		
		pDock->container.bIsHorizontal = pParentDock->container.bIsHorizontal;
		pDock->container.bDirectionUp = pParentDock->container.bDirectionUp;
		pDock->iNumScreen = pParentDock->iNumScreen;
		pDock->iIconSize = pParentDock->iIconSize;
		
		pDock->container.fRatio = myBackendsParam.fSubDockSizeRatio;
		
		//\__________________ hide the dock
		gtk_widget_hide (pDock->container.pWidget);
	}
	
	//\__________________ set a renderer (got from the conf, or the default one).
	if (dattr->cRendererName)
		cairo_dock_set_renderer (pDock, dattr->cRendererName);  /// merge both functions ?...
	else
		cairo_dock_set_default_renderer (pDock);
	
	//\__________________ load the icons.
	if (pIconList != NULL)
	{
		cairo_dock_reload_buffers_in_dock (pDock, FALSE, TRUE);  // idle reload; FALSE = not recursively, TRUE = compute icons size
	}
	
	cairo_dock_update_dock_size (pDock);
}

static void reset_object (GldiObject *obj)
{
	CairoDock *pDock = (CairoDock*)obj;
	
	// stop timers
	if (pDock->iSidUnhideDelayed != 0)
		g_source_remove (pDock->iSidUnhideDelayed);
	if (pDock->iSidHideBack != 0)
		g_source_remove (pDock->iSidHideBack);
	if (pDock->iSidMoveResize != 0)
		g_source_remove (pDock->iSidMoveResize);
	if (pDock->iSidLeaveDemand != 0)
		g_source_remove (pDock->iSidLeaveDemand);
	if (pDock->iSidUpdateWMIcons != 0)
		g_source_remove (pDock->iSidUpdateWMIcons);
	if (pDock->iSidLoadBg != 0)
		g_source_remove (pDock->iSidLoadBg);
	if (pDock->iSidDestroyEmptyDock != 0)
		g_source_remove (pDock->iSidDestroyEmptyDock);
	if (pDock->iSidTestMouseOutside != 0)
		g_source_remove (pDock->iSidTestMouseOutside);
	if (pDock->iSidUpdateDockSize != 0)
		g_source_remove (pDock->iSidUpdateDockSize);
	
	// free icons that are still present
	GList *icons = pDock->icons;
	pDock->icons = NULL;  // remove the icons first, to avoid any use of 'icons' in the 'destroy' callbacks.
	GList *ic;
	for (ic = icons; ic != NULL; ic = ic->next)
	{
		Icon *pIcon = ic->data;
		cairo_dock_set_icon_container (pIcon, NULL);  // optimisation, to avoid detaching the icon from the container (it also prevents from auto-destroying the dock when it becomes empty)
		if (pIcon->pSubDock != NULL && s_bResetAll)  // if we're deleting the whole table, we don't want the icon to destroy its sub-dock
			pIcon->pSubDock = NULL;
		gldi_object_unref (GLDI_OBJECT(pIcon));
	}
	///g_list_foreach (icons, (GFunc)gldi_object_unref, NULL);
	g_list_free (icons);
	
	// if it's a sub-dock, ensure the main icon looses its sub-dock
	if (pDock->iRefCount > 0)
	{
		Icon *pPointedIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
		if (pPointedIcon != NULL)
			pPointedIcon->pSubDock = NULL;
	}
	
	// unregister it
	if (pDock->cDockName)
	{
		g_hash_table_remove (s_hDocksTable, pDock->cDockName);
		s_pRootDockList = g_list_remove (s_pRootDockList, pDock);
	}
	
	// stop the mouse scrutation
	if (pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP
	|| pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE_ON_OVERLAP_ANY
	|| pDock->iVisibility == CAIRO_DOCK_VISI_AUTO_HIDE
	|| pDock->iVisibility == CAIRO_DOCK_VISI_KEEP_BELOW)
	{
		_stop_polling_screen_edge ();
	}
	
	// free data
	if (pDock->pShapeBitmap != NULL)
		cairo_region_destroy (pDock->pShapeBitmap);
	
	if (pDock->pHiddenShapeBitmap != NULL)
		cairo_region_destroy (pDock->pHiddenShapeBitmap);
	
	if (pDock->pActiveShapeBitmap != NULL)
		cairo_region_destroy (pDock->pActiveShapeBitmap);
	
	if (pDock->pRenderer != NULL && pDock->pRenderer->free_data != NULL)
	{
		pDock->pRenderer->free_data (pDock);
	}
	
	g_free (pDock->cRendererName);
	g_free (pDock->cBgImagePath);
	cairo_dock_unload_image_buffer (&pDock->backgroundBuffer);
	if (pDock->iFboId != 0)
		glDeleteFramebuffersEXT (1, &pDock->iFboId);
	if (pDock->iRedirectedTexture != 0)
		_cairo_dock_delete_texture (pDock->iRedirectedTexture);
	g_free (pDock->cDockName);
}

static gboolean delete_object (GldiObject *obj)
{
	CairoDock *pDock = (CairoDock*)obj;
	if (pDock->bIsMainDock)  // can't delete the main dock
	{
		return FALSE;
	}
	
	// remove the conf file
	_remove_root_dock_config (pDock->cDockName);
	
	// delete all the icons
	GList *icons = pDock->icons;
	pDock->icons = NULL;  // remove the icons first, to avoid any use of 'icons' in the 'destroy' callbacks.
	GList *ic;
	for (ic = icons; ic != NULL; ic = ic->next)
	{
		Icon *pIcon = ic->data;
		cairo_dock_set_icon_container (pIcon, NULL);  // optimisation, to avoid detaching the icon from the container.
		gldi_object_delete (GLDI_OBJECT(pIcon));
	}
	///g_list_foreach (icons, (GFunc)gldi_object_delete, NULL);
	g_list_free (icons);
	
	return TRUE;
}

static GKeyFile* reload_object (GldiObject *obj, gboolean bReloadConf, G_GNUC_UNUSED GKeyFile *pKeyFile)
{
	CairoDock *pDock = (CairoDock*)obj;
	
	if (bReloadConf)  // maybe we should update the parameters that have the global value ?...
		_get_root_dock_config (pDock);
	
	cairo_dock_set_default_renderer (pDock);
	
	pDock->backgroundBuffer.iWidth ++;  // pour forcer le chargement du fond.
	cairo_dock_reload_buffers_in_dock (pDock, TRUE, TRUE);
	
	_cairo_dock_draw_one_subdock_icon (NULL, pDock, NULL);  // container-icons may be drawn differently according to the orientation (ex.: box). must be done after sub-docks are reloaded.
	
	gtk_widget_queue_draw (pDock->container.pWidget);
	return NULL;
}

void gldi_register_docks_manager (void)
{
	// Manager
	memset (&myDocksMgr, 0, sizeof (GldiManager));
	gldi_object_init (GLDI_OBJECT(&myDocksMgr), &myManagerObjectMgr, NULL);
	myDocksMgr.cModuleName   = "Docks";
	// interface
	myDocksMgr.init          = init;
	myDocksMgr.load          = load;
	myDocksMgr.unload        = unload;
	myDocksMgr.reload        = (GldiManagerReloadFunc)reload;
	myDocksMgr.get_config    = (GldiManagerGetConfigFunc)get_config;
	myDocksMgr.reset_config  = (GldiManagerResetConfigFunc)reset_config;
	// Config
	memset (&myDocksParam, 0, sizeof (CairoDocksParam));
	myDocksMgr.pConfig = (GldiManagerConfigPtr)&myDocksParam;
	myDocksMgr.iSizeOfConfig = sizeof (CairoDocksParam);
	// data
	memset (&g_pVisibleZoneBuffer, 0, sizeof (CairoDockImageBuffer));
	myDocksMgr.pData = (GldiManagerDataPtr)NULL;
	myDocksMgr.iSizeOfData = 0;
	
	// Object Manager
	memset (&myDockObjectMgr, 0, sizeof (GldiObjectManager));
	myDockObjectMgr.cName         = "Dock";
	myDockObjectMgr.iObjectSize   = sizeof (CairoDock);
	// interface
	myDockObjectMgr.init_object   = init_object;
	myDockObjectMgr.reset_object  = reset_object;
	myDockObjectMgr.delete_object = delete_object;
	myDockObjectMgr.reload_object = reload_object;
	// signals
	gldi_object_install_notifications (&myDockObjectMgr, NB_NOTIFICATIONS_DOCKS);
	// parent object
	gldi_object_set_manager (GLDI_OBJECT (&myDockObjectMgr), &myContainerObjectMgr);
}
