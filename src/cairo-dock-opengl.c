/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <math.h>
#include <gtk/gtk.h>

#include <gtk/gtkgl.h>
#include <GL/glu.h>

#include "cairo-dock-opengl.h"


void cairo_dock_initialize_gl_context (CairoContainer *pContainer)
{
	cairo_dock_begin_opengl_draw (pContainer);
	
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth (1.0f);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel (GL_FLAT);
	glEnable (GL_TEXTURE_2D);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glDisable (GL_CULL_FACE);
	/*GLfloat afLightDiffuse[] = {1.0f, 1.0f, 1.0f, 0.0f};
	glLightModelf (GL_LIGHT_MODEL_TWO_SIDE, 0.0f);
	glEnable (GL_LIGHTING);
	glEnable (GL_LIGHT0);
	glLightfv (GL_LIGHT0, GL_DIFFUSE, afLightDiffuse);*/

	g_print ("OpenGL version: %s\n", glGetString (GL_VERSION));
	g_print ("OpenGL vendor: %s\n", glGetString (GL_VENDOR));
	g_print ("OpenGL renderer: %s\n", glGetString (GL_RENDERER));

	//glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);  // GL_MODULATE / GL_DECAL /  GL_BLEND
	glTexParameteri (GL_TEXTURE_2D,
			 GL_TEXTURE_MIN_FILTER,
			 GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri (GL_TEXTURE_2D,
			 GL_TEXTURE_MAG_FILTER,
			 GL_LINEAR);
	
	cairo_dock_end_opengl_draw ();
}



void cairo-dock_draw_texture (GLuint iTexture, double fAlpha)
{
	glBindTexture (GL_TEXTURE_2D, iTexture);
	glColor4f(1.0f, 1.0f, 1.0f, fAlpha);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, 1,  0.0f);  // Bottom Left Of The Texture and Quad
	glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, 1,  0.0f);  // Bottom Right Of The Texture and Quad
	glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, -1,  0.0f);  // Top Right Of The Texture and Quad
	glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  -1,  0.0f);  // Top Left Of The Texture and Quad
	glEnd();
}

void cairo-dock_draw_texture_with_size (GLuint iTexture, double fAlpha, double fWidth, double fHeight)
{
	glScalef (fWidth/2, fHeight/2, 1.);
	dock_draw_texture (iTexture, fAlpha);
}



