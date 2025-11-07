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
#include <math.h>

#include "cairo-dock-log.h"
#include "cairo-dock-file-manager.h"  // CairoDockDesktopEnv
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_sync
#include "cairo-dock-style-manager.h"  // GldiStyleParam
#include "cairo-dock-style-facility.h"

extern CairoDockDesktopEnv g_iDesktopEnv;
extern GldiStyleParam myStyleParam;

#define NORMALIZE_CHROMA

static double hue2rgb (double p, double q, double t)
{
	if(t < 0) t += 1;
	if(t > 1) t -= 1;
	if(t < 1./6) return p + (q - p) * 6 * t;
	if(t < 1./2) return q;
	if(t < 2./3) return p + (q - p) * (2./3 - t) * 6;
	return p;
}
static void hslToRgb (double h, double s, double l, double *r, double *g, double *b)
{
	if (s == 0)  // achromatic
	{
		*r = *g = *b = l;
	}
	else
	{
		#ifdef NORMALIZE_CHROMA
		double q = (l < 0.5 ? l * (1 + s) : l + s - l * s);
		#else
		double q = -s/2 + l + s;
		#endif
		double p = 2 * l - q;
		*r = hue2rgb(p, q, h + 1./3);
		*g = hue2rgb(p, q, h);
		*b = hue2rgb(p, q, h - 1./3);
	}
}
static void rgbToHsl (double r, double g, double b, double *h_, double *s_, double *l_, double *a_)
{
	double max = MAX (MAX (r, g), b), min = MIN (MIN (r, g), b);
	double h, s, l = (max + min) / 2, a;
	
	if(max == min)  // achromatic
	{
		h = s = 0;
		a = 1;
	}
	else
	{
		double d = max - min;  // chroma
		#ifdef NORMALIZE_CHROMA
		s = (l > 0.5 ? d / (2 - max - min) : d / (max + min));  // normalize the chroma by dividing by (1 - |2l-1|)
		#else
		s = d;
		#endif
		if (max == r)
			h = (g - b) / d + (g < b ? 6 : 0);
		else if (max == g)
			h = (b - r) / d + 2;
		else
			h = (r - g) / d + 4;
		h /= 6;
		
		// normalizing the chroma makes the function s(min, max) not continuous around (0;0) and (1;1) (it tends to 1) which reinforces a lot the dominant color, even if it's only dominant by a tiny amount; to reduce this effect, we attenuate the shade.
		a = ((1-s)*(1-s) * (s*s) * 8 + .5);  // attenuation of the shade
	}
	
	*h_ = h;
	*s_ = s;
	*l_ = l;
	*a_ = a;
}
void gldi_style_color_shade (GldiColor *icolor, double shade, GldiColor *ocolor)
{
	double h, s, l, a;
	rgbToHsl (icolor->rgba.red, icolor->rgba.green, icolor->rgba.blue, &h, &s, &l, &a);
	
	shade *= a;
	
	if (l > .5)
		l -= shade;
	else
		l += shade;
	if (l > 1.) l = 1.;
	if (l < 0.) l = 0.;
	
	hslToRgb (h, s, l, &ocolor->rgba.red, &ocolor->rgba.green, &ocolor->rgba.blue);
	ocolor->rgba.alpha = icolor->rgba.alpha;
}

gchar *_get_default_system_font (void)
{
	static gchar *s_cFontName = NULL;
	if (s_cFontName == NULL)
	{
		if (g_iDesktopEnv == CAIRO_DOCK_GNOME)
		{
			//!! TODO: use GSettings directly !!
			const char * const args[] = {"gsettings", "get", "org.gnome.desktop.interface", "font-name", NULL};
			s_cFontName = cairo_dock_launch_command_argv_sync_with_stderr (args, FALSE);  // GTK3
			cd_debug ("s_cFontName: %s", s_cFontName);
			if (s_cFontName && *s_cFontName == '\'')  // the value may be between quotes... get rid of them!
			{
				s_cFontName ++;  // s_cFontName is never freed
				s_cFontName[strlen(s_cFontName) - 1] = '\0';
			}
		}
		if (! s_cFontName)
			s_cFontName = g_strdup ("Sans 10");
	}
	return g_strdup (s_cFontName);
}

void _get_color_from_pattern (cairo_pattern_t *pPattern, GldiColor *color)
{
	switch (cairo_pattern_get_type (pPattern))
	{
		case CAIRO_PATTERN_TYPE_RADIAL:
		case CAIRO_PATTERN_TYPE_LINEAR:
		{
			memset (&color->rgba, 0, sizeof (GdkRGBA));
			double r, g, b, a;
			double offset;
			int i, n=0;
			cairo_pattern_get_color_stop_count (pPattern, &n);
			if (n == 0) break;
			for (i = 0; i < n; i ++)
			{
				cairo_pattern_get_color_stop_rgba (pPattern,
					i, &offset,
					&r, &g,
					&b, &a);
				color->rgba.red += r;  // Note: we could take into account the offset to ponderate the color, but it probably doesn't worth the pain
				color->rgba.blue += b;
				color->rgba.green += g;
				color->rgba.alpha += a;
			}
			color->rgba.red /= n;
			color->rgba.blue /= n;
			color->rgba.green /= n;
			color->rgba.alpha /= n;
		}
		break;
		case CAIRO_PATTERN_TYPE_SOLID:
			cairo_pattern_get_rgba (pPattern,
				&color->rgba.red, &color->rgba.green,
				&color->rgba.blue, &color->rgba.alpha);
		break;
		case CAIRO_PATTERN_TYPE_SURFACE:  // Note: we could cairo_pattern_get_surface() and then if it's an image surface , cairo_image_surface_get_data() and then take the mean value, but I think this case is unlikely (at least in GTK themes there seems to only be color patterns).
		default:
			cd_warning ("unsupported type of pattern (%d), please report this to the devs :-)", cairo_pattern_get_type (pPattern));
			memset (&color->rgba, 0, sizeof (GdkRGBA));
		break;
	}
}



void gldi_text_description_free (GldiTextDescription *pTextDescription)
{
	if (pTextDescription == NULL)
		return ;
	g_free (pTextDescription->cFont);
	if (pTextDescription->fd)
		pango_font_description_free (pTextDescription->fd);
	g_free (pTextDescription);
}

void gldi_text_description_copy (GldiTextDescription *pDestTextDescription, GldiTextDescription *pOrigTextDescription)
{
	g_return_if_fail (pOrigTextDescription != NULL && pDestTextDescription != NULL);
	memcpy (pDestTextDescription, pOrigTextDescription, sizeof (GldiTextDescription));
	pDestTextDescription->cFont = g_strdup (pOrigTextDescription->cFont);
	pDestTextDescription->fd = pango_font_description_copy (pOrigTextDescription->fd);
}

GldiTextDescription *gldi_text_description_duplicate (GldiTextDescription *pTextDescription)
{
	g_return_val_if_fail (pTextDescription != NULL, NULL);
	GldiTextDescription *pTextDescription2 = g_memdup2 (pTextDescription, sizeof (GldiTextDescription));
	pTextDescription2->cFont = g_strdup (pTextDescription->cFont);
	pTextDescription2->fd = pango_font_description_copy (pTextDescription->fd);
	return pTextDescription2;
}

void gldi_text_description_reset (GldiTextDescription *pTextDescription)
{
	g_free (pTextDescription->cFont);
	pTextDescription->cFont = NULL;
	if (pTextDescription->fd)
	{
		pango_font_description_free (pTextDescription->fd);
		pTextDescription->fd = NULL;
	}
	pTextDescription->iSize = 0;
}

void gldi_text_description_set_font (GldiTextDescription *pTextDescription, gchar *cFont)
{
	pTextDescription->cFont = cFont;
	
	if (cFont != NULL)
	{
		pTextDescription->fd = pango_font_description_from_string (cFont);
		
		if (pango_font_description_get_size_is_absolute (pTextDescription->fd))
		{
			pTextDescription->iSize = pango_font_description_get_size (pTextDescription->fd) / PANGO_SCALE;
		}
		else
		{
			gdouble dpi = gdk_screen_get_resolution (gdk_screen_get_default ());
			if (dpi < 0) dpi = 96.;
			pTextDescription->iSize = dpi * pango_font_description_get_size (pTextDescription->fd) / PANGO_SCALE / 72.;  // font_size in dots (pixels) = font_size in points / (72 points per inch) * (dpi dots per inch)
		}
	}
	else  // no font, take the default one
	{
		pTextDescription->fd = pango_font_description_copy (myStyleParam.textDescription.fd);
		pTextDescription->iSize = myStyleParam.textDescription.iSize;
	}
}
