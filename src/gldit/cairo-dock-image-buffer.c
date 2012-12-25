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
#include <stdlib.h>

#include "cairo-dock-icon-manager.h"  // myIconsParam.iIconWidth
#include "cairo-dock-desklet-factory.h"  // CAIRO_DOCK_IS_DESKLET
#include "cairo-dock-surface-factory.h"
#include "cairo-dock-log.h"
#include "cairo-dock-draw.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-image-buffer.h"

extern gchar *g_cCurrentThemePath;
extern gchar *g_cCurrentIconsPath;
extern gchar *g_cCurrentImagesPath;

extern gboolean g_bUseOpenGL;
extern CairoDockGLConfig g_openglConfig;
extern CairoContainer *g_pPrimaryContainer;
extern gboolean g_bEasterEggs;


void cairo_dock_free_label_description (CairoDockLabelDescription *pTextDescription)
{
	if (pTextDescription == NULL)
		return ;
	g_free (pTextDescription->cFont);
	g_free (pTextDescription);
}

void cairo_dock_copy_label_description (CairoDockLabelDescription *pDestTextDescription, CairoDockLabelDescription *pOrigTextDescription)
{
	g_return_if_fail (pOrigTextDescription != NULL && pDestTextDescription != NULL);
	memcpy (pDestTextDescription, pOrigTextDescription, sizeof (CairoDockLabelDescription));
	pDestTextDescription->cFont = g_strdup (pOrigTextDescription->cFont);
}

CairoDockLabelDescription *cairo_dock_duplicate_label_description (CairoDockLabelDescription *pOrigTextDescription)
{
	g_return_val_if_fail (pOrigTextDescription != NULL, NULL);
	CairoDockLabelDescription *pTextDescription = g_memdup (pOrigTextDescription, sizeof (CairoDockLabelDescription));
	pTextDescription->cFont = g_strdup (pOrigTextDescription->cFont);
	return pTextDescription;
}

gchar *cairo_dock_search_image_s_path (const gchar *cImageFile)
{
	g_return_val_if_fail (cImageFile != NULL, NULL);
	gchar *cImagePath;
	if (*cImageFile == '~')
	{
		cImagePath = g_strdup_printf ("%s%s", getenv("HOME"), cImageFile + 1);
		if (!g_file_test (cImagePath, G_FILE_TEST_EXISTS))
		{
			g_free (cImagePath);
			cImagePath = NULL;
		}
	}
	else if (*cImageFile == '/')
	{
		if (!g_file_test (cImageFile, G_FILE_TEST_EXISTS))
			cImagePath = NULL;
		else
			cImagePath = g_strdup (cImageFile);
	}
	else
	{
		cImagePath = g_strdup_printf ("%s/%s", g_cCurrentImagesPath, cImageFile);
		if (!g_file_test (cImagePath, G_FILE_TEST_EXISTS))
		{
			g_free (cImagePath);
			cImagePath = g_strdup_printf ("%s/%s", g_cCurrentThemePath, cImageFile);
			if (!g_file_test (cImagePath, G_FILE_TEST_EXISTS))
			{
				g_free (cImagePath);
				cImagePath = g_strdup_printf ("%s/%s", g_cCurrentIconsPath, cImageFile);
				if (!g_file_test (cImagePath, G_FILE_TEST_EXISTS))
				{
					g_free (cImagePath);
					cImagePath = NULL;
				}
			}
		}
	}
	return cImagePath;
}


void cairo_dock_load_image_buffer_full (CairoDockImageBuffer *pImage, const gchar *cImageFile, int iWidth, int iHeight, CairoDockLoadImageModifier iLoadModifier, double fAlpha)
{
	if (cImageFile == NULL)
		return;
	gchar *cImagePath = cairo_dock_search_image_s_path (cImageFile);
	double w=0, h=0;
	pImage->pSurface = cairo_dock_create_surface_from_image (
		cImagePath,
		1.,
		iWidth,
		iHeight,
		iLoadModifier,
		&w,
		&h,
		&pImage->fZoomX,
		&pImage->fZoomY);
	pImage->iWidth = w;
	pImage->iHeight = h;
	
	if ((iLoadModifier & CAIRO_DOCK_ANIMATED_IMAGE) && h != 0)
	{
		//g_print ("%dx%d\n", (int)w, (int)h);
		if (w >= 2*h)  // we need at least 2 frames (Note: we assume that frames are wide).
		{
			if ((int)w % (int)h == 0)  // w = k*h
			{
				pImage->iNbFrames = w / h;
			}
			else if (w > 2 * h)  // if we're pretty sure this image is an animated one, try to be smart, to handle the case of non-square frames.
			{
				// assume we have wide frames => w > h
				int w_ = h+1;
				do
				{
					//g_print (" %d/%d\n", w_, (int)w);
					if ((int)w % w_ == 0)
					{
						pImage->iNbFrames = w / w_;
						break;
					}
					w_ ++;
				} while (w_ < w / 2);
			}
		}
		//g_print ("CAIRO_DOCK_ANIMATED_IMAGE -> %d frames\n", pImage->iNbFrames);
		if (pImage->iNbFrames != 0)
		{
			pImage->fDeltaFrame = 1. / pImage->iNbFrames;  // default value
			gettimeofday (&pImage->time, NULL);
		}
	}
	
	if (fAlpha < 1 && pImage->pSurface != NULL)
	{
		cairo_surface_t *pNewSurfaceAlpha = cairo_dock_create_blank_surface (
			w,
			h);
		cairo_t *pCairoContext = cairo_create (pNewSurfaceAlpha);

		cairo_set_source_surface (pCairoContext, pImage->pSurface, 0, 0);
		cairo_paint_with_alpha (pCairoContext, fAlpha);
		cairo_destroy (pCairoContext);

		cairo_surface_destroy (pImage->pSurface);
		pImage->pSurface = pNewSurfaceAlpha;
	}
	
	if (g_bUseOpenGL)
		pImage->iTexture = cairo_dock_create_texture_from_surface (pImage->pSurface);
	
	g_free (cImagePath);
}

void cairo_dock_load_image_buffer_from_surface (CairoDockImageBuffer *pImage, cairo_surface_t *pSurface, int iWidth, int iHeight)
{
	if ((iWidth == 0 || iHeight == 0) && pSurface != NULL)  // should never happen, but just in case, prevent any inconsistency.
	{
		cd_warning ("An image has an invalid size, will not be loaded.");
		pSurface = NULL;
	}
	pImage->pSurface = pSurface;
	pImage->iWidth = iWidth;
	pImage->iHeight = iHeight;
	pImage->fZoomX = 1.;
	pImage->fZoomY = 1.;
	if (g_bUseOpenGL)
		pImage->iTexture = cairo_dock_create_texture_from_surface (pImage->pSurface);
}

void cairo_dock_load_image_buffer_from_texture (CairoDockImageBuffer *pImage, GLuint iTexture, int iWidth, int iHeight)
{
	pImage->iTexture = iTexture;
	pImage->iWidth = iWidth;
	pImage->iHeight = iHeight;
	pImage->fZoomX = 1.;
	pImage->fZoomY = 1.;
}

CairoDockImageBuffer *cairo_dock_create_image_buffer (const gchar *cImageFile, int iWidth, int iHeight, CairoDockLoadImageModifier iLoadModifier)
{
	CairoDockImageBuffer *pImage = g_new0 (CairoDockImageBuffer, 1);
	
	cairo_dock_load_image_buffer (pImage, cImageFile, iWidth, iHeight, iLoadModifier);
	
	return pImage;
}

void cairo_dock_unload_image_buffer (CairoDockImageBuffer *pImage)
{
	if (pImage->pSurface != NULL)
	{
		cairo_surface_destroy (pImage->pSurface);
	}
	if (pImage->iTexture != 0)
	{
		_cairo_dock_delete_texture (pImage->iTexture);
	}
	memset (pImage, 0, sizeof (CairoDockImageBuffer));
}

void cairo_dock_free_image_buffer (CairoDockImageBuffer *pImage)
{
	if (pImage == NULL)
		return;
	cairo_dock_unload_image_buffer (pImage);
	g_free (pImage);
}

void cairo_dock_image_buffer_next_frame (CairoDockImageBuffer *pImage)
{
	if (pImage->iNbFrames == 0)
		return;
	struct timeval cur_time = pImage->time;
	gettimeofday (&pImage->time, NULL);
	double fElapsedTime = (pImage->time.tv_sec - cur_time.tv_sec) + (pImage->time.tv_usec - cur_time.tv_usec) * 1e-6;
	double fElapsedFrame = fElapsedTime / pImage->fDeltaFrame;
	pImage->iCurrentFrame += fElapsedFrame;
	
	int n = ((int)pImage->iCurrentFrame);
	double dn = pImage->iCurrentFrame - n;
	pImage->iCurrentFrame = (n % pImage->iNbFrames) + dn;
	//g_print (" + %.2f => %.2f -> %.2f\n", fElapsedTime, fElapsedFrame, pImage->iCurrentFrame);
}

void cairo_dock_apply_image_buffer_surface_with_offset (CairoDockImageBuffer *pImage, cairo_t *pCairoContext, double x, double y, double fAlpha)
{
	if (cairo_dock_image_buffer_is_animated (pImage))
	{
		int iFrameWidth = pImage->iWidth / pImage->iNbFrames;
		
		cairo_save (pCairoContext);
		cairo_translate (pCairoContext, x, y);
		cairo_rectangle (pCairoContext, 0, 0, iFrameWidth, pImage->iHeight);
		cairo_clip (pCairoContext);
		
		int n = (int) pImage->iCurrentFrame;
		double dn = pImage->iCurrentFrame - n;
		
		cairo_set_source_surface (pCairoContext, pImage->pSurface, - n * iFrameWidth, 0.);
		cairo_paint_with_alpha (pCairoContext, fAlpha * (1 - dn));
		
		int n2 = n + 1;
		if (n2 >= pImage->iNbFrames)
			n2  = 0;
		cairo_set_source_surface (pCairoContext, pImage->pSurface, - n2 * iFrameWidth, 0.);
		cairo_paint_with_alpha (pCairoContext, fAlpha * dn);
		
		cairo_restore (pCairoContext);
	}
	else
	{
		cairo_set_source_surface (pCairoContext, pImage->pSurface, x, y);
		cairo_paint_with_alpha (pCairoContext, fAlpha);
	}
}

void cairo_dock_apply_image_buffer_texture_with_offset (CairoDockImageBuffer *pImage, double x, double y)
{
	glBindTexture (GL_TEXTURE_2D, pImage->iTexture);
	if (cairo_dock_image_buffer_is_animated (pImage))
	{
		int iFrameWidth = pImage->iWidth / pImage->iNbFrames;
		
		int n = (int) pImage->iCurrentFrame;
		double dn = pImage->iCurrentFrame - n;
		
		_cairo_dock_set_blend_alpha ();
		
		_cairo_dock_set_alpha (1. - dn);
		_cairo_dock_apply_current_texture_portion_at_size_with_offset ((double)n / pImage->iNbFrames, 0,
			1. / pImage->iNbFrames, 1.,
			iFrameWidth, pImage->iHeight,
			x, y);
		
		int n2 = n + 1;
		if (n2 >= pImage->iNbFrames)
			n2  = 0;
		_cairo_dock_set_alpha (dn);
		_cairo_dock_apply_current_texture_portion_at_size_with_offset ((double)n2 / pImage->iNbFrames, 0,
			1. / pImage->iNbFrames, 1.,
			iFrameWidth, pImage->iHeight,
			x, y);
	}
	else
	{
		_cairo_dock_apply_current_texture_at_size_with_offset (pImage->iWidth, pImage->iHeight, x, y);
	}
}

void cairo_dock_apply_image_buffer_surface_at_size (CairoDockImageBuffer *pImage, cairo_t *pCairoContext, int w, int h, double x, double y, double fAlpha)
{
	if (cairo_dock_image_buffer_is_animated (pImage))
	{
		int iFrameWidth = pImage->iWidth / pImage->iNbFrames;
		
		cairo_save (pCairoContext);
		cairo_translate (pCairoContext, x, y);
		
		cairo_scale (pCairoContext, (double) w/pImage->iWidth, (double) h/pImage->iHeight);
		
		cairo_rectangle (pCairoContext, 0, 0, iFrameWidth, pImage->iHeight);
		cairo_clip (pCairoContext);
		
		int n = (int) pImage->iCurrentFrame;
		double dn = pImage->iCurrentFrame - n;
		
		cairo_set_source_surface (pCairoContext, pImage->pSurface, - n * iFrameWidth, 0.);
		cairo_paint_with_alpha (pCairoContext, fAlpha * (1 - dn));
		
		int n2 = n + 1;
		if (n2 >= pImage->iNbFrames)
			n2  = 0;
		cairo_set_source_surface (pCairoContext, pImage->pSurface, - n2 * iFrameWidth, 0.);
		cairo_paint_with_alpha (pCairoContext, fAlpha * dn);
		
		cairo_restore (pCairoContext);
	}
	else
	{
		cairo_save (pCairoContext);
		cairo_translate (pCairoContext, x, y);
		
		cairo_scale (pCairoContext, (double) w/pImage->iWidth, (double) h/pImage->iHeight);
		
		cairo_set_source_surface (pCairoContext, pImage->pSurface, 0, 0);
		cairo_paint_with_alpha (pCairoContext, fAlpha);
		
		cairo_restore (pCairoContext);
	}
}

void cairo_dock_apply_image_buffer_texture_at_size (CairoDockImageBuffer *pImage, int w, int h, double x, double y)
{
	glBindTexture (GL_TEXTURE_2D, pImage->iTexture);
	if (cairo_dock_image_buffer_is_animated (pImage))
	{
		int n = (int) pImage->iCurrentFrame;
		double dn = pImage->iCurrentFrame - n;
		
		_cairo_dock_set_blend_alpha ();
		
		_cairo_dock_set_alpha (1. - dn);
		_cairo_dock_apply_current_texture_portion_at_size_with_offset ((double)n / pImage->iNbFrames, 0,
			1. / pImage->iNbFrames, 1.,
			w, h,
			x, y);
		
		int n2 = n + 1;
		if (n2 >= pImage->iNbFrames)
			n2  = 0;
		_cairo_dock_set_alpha (dn);
		_cairo_dock_apply_current_texture_portion_at_size_with_offset ((double)n2 / pImage->iNbFrames, 0,
			1. / pImage->iNbFrames, 1.,
			w, h,
			x, y);
	}
	else
	{
		_cairo_dock_apply_current_texture_at_size_with_offset (w, h, x, y);
	}
}


void cairo_dock_apply_image_buffer_surface_with_offset_and_limit (CairoDockImageBuffer *pImage, cairo_t *pCairoContext, double x, double y, double fAlpha, int iMaxWidth)
{
	cairo_set_source_surface (pCairoContext,
		pImage->pSurface,
		x,
		y);
	
	const double a = .75;  // 3/4 plain, 1/4 gradation
	cairo_pattern_t *pGradationPattern = cairo_pattern_create_linear (x,
		0.,
		x + iMaxWidth,
		0.);
	cairo_pattern_set_extend (pGradationPattern, CAIRO_EXTEND_NONE);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		0.,
		0.,
		0.,
		0.,
		fAlpha);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		a,
		0.,
		0.,
		0.,
		fAlpha);
	cairo_pattern_add_color_stop_rgba (pGradationPattern,
		1.,
		0.,
		0.,
		0.,
		0.);
	cairo_mask (pCairoContext, pGradationPattern);
	cairo_pattern_destroy (pGradationPattern);
}

void cairo_dock_apply_image_buffer_texture_with_limit (CairoDockImageBuffer *pImage, double fAlpha, int iMaxWidth)
{
	glBindTexture (GL_TEXTURE_2D, pImage->iTexture);
	
	int w = iMaxWidth, h = pImage->iHeight;
	double u0 = 0., u1 = (double) w / pImage->iWidth;
	glBegin(GL_QUAD_STRIP);
	
	double a = .75;  // 3/4 plain, 1/4 gradation
	a = (double) (floor ((-.5+a)*w)) / w + .5;
	glColor4f (1., 1., 1., fAlpha);
	glTexCoord2f(u0, 0); glVertex3f (-.5*w,  .5*h, 0.);  // top left
	glTexCoord2f(u0, 1); glVertex3f (-.5*w, -.5*h, 0.);  // bottom left
	
	glTexCoord2f(u1*a, 0); glVertex3f ((-.5+a)*w,  .5*h, 0.);  // top middle
	glTexCoord2f(u1*a, 1); glVertex3f ((-.5+a)*w, -.5*h, 0.);  // bottom middle
	
	glColor4f (1., 1., 1., 0.);
	
	glTexCoord2f(u1, 0); glVertex3f (.5*w,  .5*h, 0.);  // top right
	glTexCoord2f(u1, 1); glVertex3f (.5*w, -.5*h, 0.);  // bottom right
	
	glEnd();
}



// to draw on image buffers
static GLuint s_iFboId = 0;
static gboolean s_bRedirected = FALSE;
static GLuint s_iRedirectedTexture = 0;
static gboolean s_bSetPerspective = FALSE;
static gint s_iRedirectWidth = 0;
static gint s_iRedirectHeight = 0;

void cairo_dock_create_icon_fbo (void)  // it has been found that you get a speed boost if your textures is the same size and you use 1 FBO for them. => c'est le cas general dans le dock. Du coup on est gagnant a ne faire qu'un seul FBO pour toutes les icones.
{
	if (! g_openglConfig.bFboAvailable)
		return ;
	g_return_if_fail (s_iFboId == 0);
	
	glGenFramebuffersEXT(1, &s_iFboId);
	
	s_iRedirectWidth = myIconsParam.iIconWidth * (1 + myIconsParam.fAmplitude);  // use a common size (it can be any size, but we'll often use it to draw on icons, so this choice will often avoid a glScale).
	s_iRedirectHeight = myIconsParam.iIconHeight * (1 + myIconsParam.fAmplitude);
	s_iRedirectedTexture = cairo_dock_create_texture_from_raw_data (NULL, s_iRedirectWidth, s_iRedirectHeight);
}

void cairo_dock_destroy_icon_fbo (void)
{
	if (s_iFboId == 0)
		return;
	glDeleteFramebuffersEXT (1, &s_iFboId);
	s_iFboId = 0;
	
	_cairo_dock_delete_texture (s_iRedirectedTexture);
	s_iRedirectedTexture = 0;
}


cairo_t *cairo_dock_begin_draw_image_buffer_cairo (CairoDockImageBuffer *pImage, gint iRenderingMode, cairo_t *pCairoContext)
{
	g_return_val_if_fail (pImage->pSurface != NULL, NULL);
	cairo_t *ctx = pCairoContext;
	if (! ctx)
	{
		ctx = cairo_create (pImage->pSurface);
	}
	else if (iRenderingMode != 1)
	{
		cairo_dock_erase_cairo_context (ctx);
	}
	return ctx;
}

void cairo_dock_end_draw_image_buffer_cairo (CairoDockImageBuffer *pImage)
{
	if (g_bUseOpenGL)
		cairo_dock_image_buffer_update_texture (pImage);
}


gboolean cairo_dock_begin_draw_image_buffer_opengl (CairoDockImageBuffer *pImage, CairoContainer *pContainer, gint iRenderingMode)
{
	int iWidth, iHeight;
	/// TODO: test without FBO and dock when iRenderingMode == 2
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		if (! gldi_glx_make_current (pContainer))
		{
			return FALSE;
		}
		iWidth = pContainer->iWidth;
		iHeight = pContainer->iHeight;
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else if (s_iFboId != 0)
	{
		// on attache la texture au FBO.
		if (pContainer->iWidth == 1 && pContainer->iHeight == 1)  // container not yet fully resized
		{
			return FALSE;
		}
		iWidth = pImage->iWidth, iHeight = pImage->iHeight;
		if (pContainer == NULL)
			pContainer = g_pPrimaryContainer;
		if (! gldi_glx_make_current (pContainer))
		{
			cd_warning ("couldn't set the opengl context");
			return FALSE;
		}
		glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, s_iFboId);  // on redirige sur notre FBO.
		s_bRedirected = (iRenderingMode == 2);
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
			GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_2D,
			s_bRedirected ? s_iRedirectedTexture : pImage->iTexture,
			0);  // attach the texture to FBO color attachment point.
		
		GLenum status = glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT);
		if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
		{
			cd_warning ("FBO not ready (tex:%d)", pImage->iTexture);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);  // switch back to window-system-provided framebuffer
			glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
				GL_COLOR_ATTACHMENT0_EXT,
				GL_TEXTURE_2D,
				0,
				0);
			return FALSE;
		}
		
		if (iRenderingMode != 1)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else
		return FALSE;
	
	if (pContainer->bPerspectiveView)
	{
		cairo_dock_set_ortho_view (pContainer);
		s_bSetPerspective = TRUE;
	}
	else
	{
		cairo_dock_set_ortho_view (pContainer);  // au demarrage, le contexte n'a pas encore de vue.
		///glLoadIdentity ();
		///glTranslatef (iWidth/2, iHeight/2, - iHeight/2);
	}
	
	glLoadIdentity ();
	
	if (s_bRedirected)  // adapt to the size of the redirected texture
	{
		glScalef ((double)s_iRedirectWidth/iWidth, (double)s_iRedirectHeight/iHeight, 1.);  // no need to revert the y-axis, since we'll apply the redirected texture on the image's texture, which will invert it.
		glTranslatef (iWidth/2, iHeight/2, - iHeight/2);  // translate to the middle of the drawing space.
	}
	else
	{
		glScalef (1., -1., 1.);  // revert y-axis since texture are drawn reversed
		glTranslatef (iWidth/2, -iHeight/2, - iHeight/2);  // translate to the middle of the drawing space.
	}
	
	glColor4f (1., 1., 1., 1.);
	
	return TRUE;
}

void cairo_dock_end_draw_image_buffer_opengl (CairoDockImageBuffer *pImage, CairoContainer *pContainer)
{
	g_return_if_fail (pContainer != NULL && pImage->iTexture != 0);
	
	if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		// copie dans notre texture
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_source ();
		_cairo_dock_set_alpha (1.);
		glBindTexture (GL_TEXTURE_2D, pImage->iTexture);
		
		int iWidth, iHeight;  // taille de la texture
		iWidth = pImage->iWidth, iHeight = pImage->iHeight;
		int x = (pContainer->iWidth - iWidth)/2;
		int y = (pContainer->iHeight - iHeight)/2;
		glCopyTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, x, y, iWidth, iHeight, 0);  // target, num mipmap, format, x,y, w,h, border.
		
		_cairo_dock_disable_texture ();
	}
	else if (s_iFboId != 0)
	{
		if (s_bRedirected)  // copie dans notre texture
		{
			glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
				GL_COLOR_ATTACHMENT0_EXT,
				GL_TEXTURE_2D,
				pImage->iTexture,
				0);  // maintenant on dessine dans la texture de l'icone.
			_cairo_dock_enable_texture ();
			_cairo_dock_set_blend_source ();
			
			int iWidth, iHeight;  // taille de la texture
			iWidth = pImage->iWidth, iHeight = pImage->iHeight;
			
			glLoadIdentity ();
			glTranslatef (iWidth/2, iHeight/2, - iHeight/2);
			_cairo_dock_apply_texture_at_size_with_alpha (s_iRedirectedTexture, iWidth, iHeight, 1.);
			
			_cairo_dock_disable_texture ();
			s_bRedirected = FALSE;
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);  // switch back to window-system-provided framebuffer
		glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
			GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_2D,
			0,
			0);  // on detache la texture (precaution).
		//glGenerateMipmapEXT(GL_TEXTURE_2D);  // si on utilise les mipmaps, il faut les generer explicitement avec les FBO.
	}
	
	if (pContainer && s_bSetPerspective)
	{
		cairo_dock_set_perspective_view (pContainer);
		s_bSetPerspective = FALSE;
	}
}

void cairo_dock_image_buffer_update_texture (CairoDockImageBuffer *pImage)
{
	if (pImage->iTexture == 0)
	{
		pImage->iTexture = cairo_dock_create_texture_from_surface (pImage->pSurface);
	}
	else
	{
		_cairo_dock_enable_texture ();
		_cairo_dock_set_blend_source ();
		_cairo_dock_set_alpha (1.);  // full white
		
		int w = cairo_image_surface_get_width (pImage->pSurface);  // extra caution
		int h = cairo_image_surface_get_height (pImage->pSurface);
		glBindTexture (GL_TEXTURE_2D, pImage->iTexture);
		
		glTexParameteri (GL_TEXTURE_2D,
			GL_TEXTURE_MIN_FILTER,
			g_bEasterEggs ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		if (g_bEasterEggs)
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		if (g_bEasterEggs)
			gluBuild2DMipmaps (GL_TEXTURE_2D,
				4,
				w,
				h,
				GL_BGRA,
				GL_UNSIGNED_BYTE,
				cairo_image_surface_get_data (pImage->pSurface));
		else
			glTexImage2D (GL_TEXTURE_2D,
				0,
				4,  // GL_ALPHA / GL_BGRA
				w,
				h,
				0,
				GL_BGRA,  // GL_ALPHA / GL_BGRA
				GL_UNSIGNED_BYTE,
				cairo_image_surface_get_data (pImage->pSurface));
		_cairo_dock_disable_texture ();
	}
}


GdkPixbuf *cairo_dock_image_buffer_to_pixbuf (CairoDockImageBuffer *pImage, int iWidth, int iHeight)
{
	GdkPixbuf *pixbuf = NULL;
	int w = iWidth, h = iHeight;
	if (pImage->iWidth > 0 && pImage->iHeight > 0 && pImage->pSurface != NULL)
	{
		cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
			w,
			h);
		cairo_t *pCairoContext = cairo_create (surface);
		cairo_scale (pCairoContext, (double)w/pImage->iWidth, (double)h/pImage->iHeight);
		cairo_set_source_surface (pCairoContext, pImage->pSurface, 0., 0.);
		cairo_paint (pCairoContext);
		cairo_destroy (pCairoContext);
		guchar *d, *data = cairo_image_surface_get_data (surface);
		int r = cairo_image_surface_get_stride (surface);
		
		// on la convertit en un pixbuf.
		pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			TRUE,
			8,
			w,
			h);
		guchar *p, *pixels = gdk_pixbuf_get_pixels (pixbuf);
		int iNbChannels = gdk_pixbuf_get_n_channels (pixbuf);
		int iRowstride = gdk_pixbuf_get_rowstride (pixbuf);
		
		int x, y;
		int red, green, blue;
		float fAlphaFactor;
		for (y = 0; y < h; y ++)
		{
			for (x = 0; x < w; x ++)
			{
				p = pixels + y * iRowstride + x * iNbChannels;
				d = data + y * r + x * 4;
				
				fAlphaFactor = (float) d[3] / 255;
				if (fAlphaFactor != 0)
				{
					red = d[0] / fAlphaFactor;
					green = d[1] / fAlphaFactor;
					blue = d[2] / fAlphaFactor;
				}
				else
				{
					red = blue = green = 0;
				}
				p[0] = blue;
				p[1] = green;
				p[2] = red;
				p[3] = d[3];
			}
		}
		
		cairo_surface_destroy (surface);
	}
	return pixbuf;
}
