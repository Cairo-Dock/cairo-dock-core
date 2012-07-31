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


#ifndef __CAIRO_DOCK_PROGRESSBAR__
#define  __CAIRO_DOCK_PROGRESSBAR__

#include "cairo-dock-struct.h"
#include "cairo-dock-data-renderer-manager.h"
G_BEGIN_DECLS

/**
*@file cairo-dock-progressbar.h This class defines the ProgressBar, which derives from the DataRenderer.
* All you need to know is the attributes that define a ProgressBar, the API to use is the common API for DataRenderer, defined in cairo-dock-data-renderer.h.
*/


typedef struct _CairoProgressBarAttribute CairoProgressBarAttribute;

/// Attributes of a PgrogressBar.
struct _CairoProgressBarAttribute {
	/// General attributes of any DataRenderer.
	CairoDataRendererAttribute rendererAttribute;
	/// image or NULL
	gchar *cImageGradation;
	/// color gradation of the bar (an array of 8 doubles, representing 2 RGBA values) or NULL
	gdouble *fColorGradation;  // 2*4
	/// TRUE to define a custom position (by default it is placed at the middle bottom)
	gboolean bUseCustomPosition;
	/// custom position
	CairoOverlayPosition iCustomPosition;
	/// invert colors
	gboolean bInverted;
};


void cairo_dock_register_data_renderer_progressbar (void);


G_END_DECLS
#endif
