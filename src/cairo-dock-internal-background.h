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


#ifndef __CAIRO_DOCK_INTERNAL_BACKGROUND__
#define  __CAIRO_DOCK_INTERNAL_BACKGROUND__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"


typedef struct _CairoConfigBackground CairoConfigBackground;
#ifndef _INTERNAL_MODULE_
extern CairoConfigBackground myBackground;
#endif
G_BEGIN_DECLS

struct _CairoConfigBackground {
	gint iDockRadius;
	gint iDockLineWidth;
	gint iFrameMargin;
	gdouble fLineColor[4];
	gboolean bRoundedBottomCorner;
	gchar *cBackgroundImageFile;
	gdouble fBackgroundImageAlpha;
	gboolean bBackgroundImageRepeat;
	gint iNbStripes;
	gdouble fStripesWidth;
	gdouble fStripesColorBright[4];
	gdouble fStripesColorDark[4];
	gdouble fStripesAngle;
	gchar *cVisibleZoneImageFile;
	gdouble fVisibleZoneAlpha;
	gboolean bReverseVisibleImage;
	gdouble fDecorationSpeed;
	gboolean bDecorationsFollowMouse;
	};

#define g_iDockLineWidth myBackground.iDockLineWidth
#define g_iDockRadius myBackground.iDockRadius

DEFINE_PRE_INIT (Background);

G_END_DECLS
#endif
