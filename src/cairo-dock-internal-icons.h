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


#ifndef __CAIRO_DOCK_INTERNAL_ICONS__
#define  __CAIRO_DOCK_INTERNAL_ICONS__

#include <glib.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-config.h"
#include "cairo-dock-icons.h"


typedef struct _CairoConfigIcons CairoConfigIcons;
#ifndef _INTERNAL_MODULE_
extern CairoConfigIcons myIcons;
#endif
G_BEGIN_DECLS

struct _CairoConfigIcons {
	gdouble fFieldDepth;
	gdouble fAlbedo;
	gdouble fAmplitude;
	gint iSinusoidWidth;
	gint iIconGap;
	gint iStringLineWidth;
	gdouble fStringColor[4];
	gdouble fAlphaAtRest;
	gdouble fReflectSize;
	gchar *cIconTheme;
	gpointer *pDefaultIconDirectory;
	gint iNbIconPlaces;
	gint tIconAuthorizedWidth[CAIRO_DOCK_NB_TYPES];
	gint tIconAuthorizedHeight[CAIRO_DOCK_NB_TYPES];
	gchar *cBackgroundImagePath;
	gchar *cSeparatorImage;
	gboolean bRevolveSeparator;
	gboolean bConstantSeparatorSize;
	gboolean iSeparateIcons;
	/**gboolean bUseSeparator;
	gboolean bMixAppletsAndLaunchers;
	gboolean bMixApplisAndLaunchers;*/
	CairoDockIconType tIconTypeOrder[CAIRO_DOCK_NB_TYPES];
	gint iIconsTypesList[3];  // pour la conf simple.
	};

#define g_fAmplitude myIcons.fAmplitude
#define g_fReflectSize myIcons.fReflectSize

DEFINE_PRE_INIT (Icons);

G_END_DECLS
#endif
