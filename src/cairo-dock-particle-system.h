
#ifndef __CAIRO_DOCK_PARTICLE_SYSTEM__
#define  __CAIRO_DOCK_PARTICLE_SYSTEM__

#include <glib.h>


typedef struct _CairoParticle {
	GLfloat x, y, z;
	GLfloat vx, vy;
	GLfloat fWidth, fHeight;
	GLfloat color[4];
	GLfloat fOscillation;
	GLfloat fOmega;
	GLfloat fSizeFactor;
	GLfloat fResizeSpeed;
	gint iLife;
	gint iInitialLife;
	} CairoParticle;

typedef struct _CairoParticleSystem {
	CairoParticle *pParticles;
	gint iNbParticles;
	GLuint iTexture;
	GLfloat *pVertices;
	GLfloat *pCoords;
	GLfloat *pColors;
	GLfloat fWidth, fHeight;
	double dt;
	gboolean bDirectionUp;
	gboolean bAddLuminance;
	} CairoParticleSystem;

typedef void (CairoDockRewindParticleFunc) (CairoParticle *pParticle, double dt);

void cairo_dock_render_particles_full (CairoParticleSystem *pParticleSystem, int iDepth);
#define cairo_dock_render_particles(pParticleSystem) cairo_dock_render_particles_full (pParticleSystem, 0)

CairoParticleSystem *cairo_dock_create_particle_system (int iNbParticles, GLuint iTexture, double fWidth, double fHeight);

void cairo_dock_free_particle_system (CairoParticleSystem *pParticleSystem);

gboolean cairo_dock_update_default_particle_system (CairoParticleSystem *pParticleSystem, CairoDockRewindParticleFunc pRewindParticle);


#endif
