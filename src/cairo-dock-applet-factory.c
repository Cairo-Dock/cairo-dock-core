/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <cairo.h>

#include "cairo-dock-load.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-launcher-factory.h"
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-internal-icons.h"
#include "cairo-dock-applet-factory.h"




cairo_surface_t *cairo_dock_create_applet_surface (gchar *cIconFileName, cairo_t *pSourceContext, double fMaxScale, double *fWidth, double *fHeight, gboolean bApplySizeRestriction)
{
	cd_message ("%s (%.2fx%.2f x %.2f / %d)", __func__, *fWidth, *fHeight,fMaxScale, bApplySizeRestriction);
	g_return_val_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, NULL);
	double fIconWidthSaturationFactor, fIconHeightSaturationFactor;
	cairo_dock_calculate_size_fill (fWidth,
		fHeight,
		(bApplySizeRestriction ? myIcons.tIconAuthorizedWidth[CAIRO_DOCK_APPLET] : 0),
		(bApplySizeRestriction ? myIcons.tIconAuthorizedHeight[CAIRO_DOCK_APPLET] : 0),
		FALSE,
		&fIconWidthSaturationFactor, &fIconHeightSaturationFactor);
	cd_message (" -> %.2fx%.2f x %.2f", *fWidth, *fHeight,fMaxScale);
	cairo_surface_t *pNewSurface;
	if (cIconFileName == NULL)
		/*pNewSurface = cairo_surface_create_similar (cairo_get_target (pSourceContext),
			CAIRO_CONTENT_COLOR_ALPHA,
			ceil (*fWidth * fMaxScale),
			ceil (*fHeight * fMaxScale));*/
		pNewSurface = _cairo_dock_create_blank_surface (pSourceContext,
			ceil (*fWidth * fMaxScale),
			ceil (*fHeight * fMaxScale));
	else
	{
		gchar *cIconPath = cairo_dock_search_icon_s_path (cIconFileName);
		pNewSurface = cairo_dock_create_surface_from_image (cIconPath,
			pSourceContext,
			fMaxScale,
			*fWidth,
			*fHeight,
			CAIRO_DOCK_FILL_SPACE,
			fWidth,
			fHeight,
			NULL, NULL);
		g_free (cIconPath);
	}
	return pNewSurface;
}


Icon *cairo_dock_create_icon_for_applet (CairoContainer *pContainer, int iWidth, int iHeight, gchar *cName, gchar *cIconFileName, CairoDockModuleInstance *pModuleInstance)
{
	Icon *icon = g_new0 (Icon, 1);
	icon->iType = CAIRO_DOCK_APPLET;
	icon->pModuleInstance = pModuleInstance;

	icon->acName = g_strdup (cName);
	icon->acFileName = g_strdup (cIconFileName);  // NULL si cIconFileName = NULL.

	icon->fScale = 1;
	icon->fWidth =iWidth;
	icon->fHeight =iHeight;
	icon->fWidthFactor = 1.;
	icon->fHeightFactor = 1.;
	cairo_t *pSourceContext = cairo_dock_create_context_from_window (pContainer);
	g_return_val_if_fail (cairo_status (pSourceContext) == CAIRO_STATUS_SUCCESS, icon);

	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		cairo_dock_fill_icon_buffers_for_dock (icon, pSourceContext, pDock);
	}
	else
	{
		if (iWidth >= 0 && iHeight >= 0)
			cairo_dock_fill_one_icon_buffer (icon, pSourceContext, 1., CAIRO_DOCK_HORIZONTAL, FALSE, pContainer->bDirectionUp);
	}

	cairo_destroy (pSourceContext);
	return icon;
}
