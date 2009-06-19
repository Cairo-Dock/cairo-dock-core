/*********************************************************************************

This file is a part of the cairo-dock program,
released under the terms of the GNU General Public License.

Written by Fabrice Rey (for any bug report, please mail me to fabounet@users.berlios.de)

*********************************************************************************/
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <cairo.h>

#include "cairo-dock-log.h"
#include "cairo-dock-draw-opengl.h"
#include "cairo-dock-particle-system.h"

static GLfloat s_pCornerCoords[8] = {0.0, 0.0,
	0.0, 1.0,
	1.0, 1.0,
	1.0, 0.0};

void cairo_dock_render_particles_full (CairoParticleSystem *pParticleSystem, int iDepth)
{
	_cairo_dock_enable_texture ();
	
	if (pParticleSystem->bAddLuminance)
		_cairo_dock_set_blend_over ();
		//glBlendFunc (GL_SRC_ALPHA, GL_ONE);
	else
		_cairo_dock_set_blend_alpha ();
	
	glBindTexture(GL_TEXTURE_2D, pParticleSystem->iTexture);
	
	GLfloat *vertices = pParticleSystem->pVertices;
	GLfloat *coords = pParticleSystem->pCoords;
	GLfloat *colors = pParticleSystem->pColors;
	
	GLfloat x,y,z;
	GLfloat w, h;
	GLfloat fHeight = pParticleSystem->fHeight;
	
	int numActive = 0;
	CairoParticle *p;
	int i;
	for (i = 0; i < pParticleSystem->iNbParticles; i ++)
	{
		p = &pParticleSystem->pParticles[i];
		if (p->iLife == 0 || iDepth * p->z < 0)
			continue;
		
		numActive += 4;
		w = p->fWidth * p->fSizeFactor;
		h = p->fHeight * p->fSizeFactor;
		x = p->x * pParticleSystem->fWidth / 2;
		y = p->y * pParticleSystem->fHeight;
		z = p->z;
		
		vertices[0] = x - w;
		vertices[2] = z;
		vertices[3] = x - w;
		vertices[5] = z;
		vertices[6] = x + w;
		vertices[8] = z;
		vertices[9] = x + w;
		vertices[11] = z;
		if (pParticleSystem->bDirectionUp)
		{
			vertices[1] = y + h;
			vertices[4] = y - h;
			vertices[7] = y - h;
			vertices[10] = y + h;
		}
		else
		{
			vertices[1] = fHeight - y + h;
			vertices[4] = fHeight - y - h;
			vertices[7] = fHeight - y - h;
			vertices[10] = fHeight - y + h;
		}
		vertices += 12;
		
		memcpy (coords, s_pCornerCoords, sizeof (s_pCornerCoords));
		coords += 8;

		colors[0] = p->color[0];
		colors[1] = p->color[1];
		colors[2] = p->color[2];
		colors[3] = p->color[3];
		memcpy (colors + 4, colors, 4*sizeof (GLfloat));
		memcpy (colors + 8, colors, 8*sizeof (GLfloat));
		colors += 16;
	}
	
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	glEnableClientState (GL_VERTEX_ARRAY);
	
	glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), pParticleSystem->pCoords);
	glVertexPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), pParticleSystem->pVertices);
	glColorPointer(4, GL_FLOAT, 4 * sizeof(GLfloat), pParticleSystem->pColors);

	glDrawArrays(GL_QUADS, 0, numActive);

	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	glDisableClientState (GL_VERTEX_ARRAY);
	
	_cairo_dock_disable_texture ();
}

CairoParticleSystem *cairo_dock_create_particle_system (int iNbParticles, GLuint iTexture, double fWidth, double fHeight)
{
	g_return_val_if_fail (iNbParticles > 0, NULL);
	CairoParticleSystem *pParticleSystem = g_new0 (CairoParticleSystem, 1);
	pParticleSystem->iNbParticles = iNbParticles;
	pParticleSystem->pParticles = g_new0 (CairoParticle, iNbParticles);
	
	pParticleSystem->iTexture = iTexture;
	
	pParticleSystem->fWidth = fWidth;
	pParticleSystem->fHeight = fHeight;
	pParticleSystem->bDirectionUp = TRUE;
	
	pParticleSystem->pVertices = malloc(iNbParticles * 4 * 3 * sizeof(GLfloat));
	pParticleSystem->pCoords = malloc(iNbParticles * 4 * 2 * sizeof(GLfloat));
	pParticleSystem->pColors = malloc(iNbParticles * 4 * 4 * sizeof(GLfloat));
	return pParticleSystem;
}


void cairo_dock_free_particle_system (CairoParticleSystem *pParticleSystem)
{
	if (pParticleSystem == NULL)
		return ;
	
	g_free (pParticleSystem->pParticles);
	
	free (pParticleSystem->pVertices);
	free (pParticleSystem->pCoords);
	free (pParticleSystem->pColors);
	
	g_free (pParticleSystem);
}


gboolean cairo_dock_update_default_particle_system (CairoParticleSystem *pParticleSystem, CairoDockRewindParticleFunc pRewindParticle)
{
	gboolean bAllParticlesEnded = TRUE;
	CairoParticle *p;
	int i;
	for (i = 0; i < pParticleSystem->iNbParticles; i ++)
	{
		p = &(pParticleSystem->pParticles[i]);
		
		p->fOscillation += p->fOmega;
		p->x += p->vx + (p->z + 2)/3. * .02 * sin (p->fOscillation);  // 3%
		p->y += p->vy;
		p->color[3] = 1.*p->iLife / p->iInitialLife;
		p->fSizeFactor += p->fResizeSpeed;
		if (p->iLife > 0)
		{
			p->iLife --;
			if (pRewindParticle && p->iLife == 0)
			{
				pRewindParticle (p, pParticleSystem->dt);
			}
			if (bAllParticlesEnded && p->iLife != 0)
				bAllParticlesEnded = FALSE;
		}
		else if (pRewindParticle)
			pRewindParticle (p, pParticleSystem->dt);
	}
	return ! bAllParticlesEnded;
}
