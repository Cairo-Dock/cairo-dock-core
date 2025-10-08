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

#ifndef __CAIRO_DOCK_INDICATOR_MANAGER__
#define  __CAIRO_DOCK_INDICATOR_MANAGER__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-object.h"
#include "cairo-dock-style-facility.h"  // GldiColor
G_BEGIN_DECLS

/**
*@file cairo-dock-indicator-manager.h This class manages the indicators.
*/

// manager
typedef struct _CairoIndicatorsParam CairoIndicatorsParam;

#ifndef _MANAGER_DEF_
extern CairoIndicatorsParam myIndicatorsParam;
#endif

// params
struct _CairoIndicatorsParam {
	// active indicator.
	gboolean bActiveUseDefaultColors;
	gboolean bActiveFillFrame;  // outline or fill frame
	gchar *cActiveIndicatorImagePath;
	GldiColor fActiveColor;
	gint iActiveLineWidth;
	gint iActiveCornerRadius;
	gboolean bActiveIndicatorAbove;
	// launched indicator.
	gchar *cIndicatorImagePath;
	gboolean bIndicatorAbove;
	gdouble fIndicatorRatio;
	gboolean bIndicatorOnIcon;
	gdouble fIndicatorDeltaY;
	gboolean bRotateWithDock;
	gboolean bDrawIndicatorOnAppli;
	// grouped indicator.
	gchar *cClassIndicatorImagePath;
	gboolean bZoomClassIndicator;
	gboolean bUseClassIndic;
	// progress bars
	gboolean bBarUseDefaultColors;
	GldiColor fBarColorStart;
	GldiColor fBarColorStop;
	GldiColor fBarColorOutline;
	gint iBarThickness;
	};

// signals
typedef enum {
	NB_NOTIFICATIONS_INDICATORS = NB_NOTIFICATIONS_OBJECT
	} CairoIndicatorsNotifications;


void gldi_register_indicators_manager (void);

G_END_DECLS
#endif
