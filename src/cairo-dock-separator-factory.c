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
#include <cairo.h>
#include <stdlib.h>

#ifdef HAVE_GLITZ
#include <gdk/gdkx.h>
#include <glitz-glx.h>
#include <cairo-glitz.h>
#endif

#include "cairo-dock-icons.h"
#include "cairo-dock-load.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-separator-factory.h"


cairo_surface_t *cairo_dock_create_separator_surface (cairo_t *pSourceContext, double fMaxScale, gboolean bIsHorizontal, gboolean bDirectionUp, double *fWidth, double *fHeight)
{
	g_return_val_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	
	cairo_surface_t *pNewSurface = NULL;
	if (myIcons.cSeparatorImage == NULL)
	{
		*fWidth = myIcons.tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12];
		*fHeight = myIcons.tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12];
		pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
			ceil (*fWidth * fMaxScale),
			ceil (*fHeight * fMaxScale));
	}
	else
	{
		gchar *cImagePath = cairo_dock_generate_file_path (myIcons.cSeparatorImage);
		double fRotationAngle;  // on est obligé de faire la rotation maintenant plutot que pendant le dessin, a cause du reflet a charger en cairo.
		if (! myIcons.bRevolveSeparator)
			fRotationAngle = 0;
		else if (bIsHorizontal)
			if (bDirectionUp)
				fRotationAngle = 0;
			else
				fRotationAngle = G_PI;
		else
			if (bDirectionUp)
				fRotationAngle = -G_PI/2;
			else
				fRotationAngle = G_PI/2;
		
		pNewSurface = cairo_dock_create_surface_from_image (cImagePath,
			pSourceContext,
			fMaxScale,
			myIcons.tIconAuthorizedWidth[CAIRO_DOCK_SEPARATOR12],
			myIcons.tIconAuthorizedHeight[CAIRO_DOCK_SEPARATOR12],
			CAIRO_DOCK_FILL_SPACE,
			fWidth,
			fHeight,
			NULL, NULL);
		if (fRotationAngle != 0)  /// le faire pendant le dessin ...
		{
			cairo_surface_t *pNewSurfaceRotated = cairo_dock_rotate_surface (pNewSurface, pSourceContext, *fWidth * fMaxScale, *fHeight * fMaxScale, fRotationAngle);
			cairo_surface_destroy (pNewSurface);
			pNewSurface = pNewSurfaceRotated;
		}
		g_free (cImagePath);
	}
	
	return pNewSurface;
}



Icon *cairo_dock_create_separator_icon (cairo_t *pSourceContext, int iSeparatorType, CairoDock *pDock)
{
	//g_print ("%s ()\n", __func__);
	if ((iSeparatorType & 1) && ! myIcons.iSeparateIcons)
		return NULL;
	
	//\____________ On cree l'icone.
	Icon *icon = g_new0 (Icon, 1);
	icon->iType = iSeparatorType;
	
	//\____________ On remplit ses buffers.
	cairo_dock_fill_icon_buffers_for_dock (icon, pSourceContext, pDock);
	///cairo_dock_fill_one_icon_buffer (icon, pSourceContext, 1 + myIcons.fAmplitude, pDock->container.bIsHorizontal, pDock->container.bDirectionUp);

	return icon;
}
