
#ifndef __CAIRO_DOCK_PARTICLE_SYSTEM__
#define  __CAIRO_DOCK_PARTICLE_SYSTEM__

#include <glib.h>

/**
*@file cairo-dock-particle-system.h A particle system is a set of particles that evolve according to a given model. Each particle will see its parameters change with time : direction, speed, oscillation, color, size, etc.
* Particle Systems fully take advantage of OpenGL and are able to render many thousand of particles at a high frequency refresh.
* 
*/

/// A particle of a particle system.
typedef struct _CairoParticle {
	/// position in space
	GLfloat x, y, z;
	/// speed
	GLfloat vx, vy;
	/// size
	GLfloat fWidth, fHeight;
	/// color r,g,b,a
	GLfloat color[4];
	/// phase of the oscillations
	GLfloat fOscillation;
	/// oscillation variation speed.
	GLfloat fOmega;
	/// current size factor
	GLfloat fSizeFactor;
	/// size variation speed.
	GLfloat fResizeSpeed;
	/// current life time.
	gint iLife;
	/// initial life time.
	gint iInitialLife;
	} CairoParticle;

/// A particle system.
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

/// Function that re-initializes a particle when its life is over.
typedef void (CairoDockRewindParticleFunc) (CairoParticle *pParticle, double dt);

/** Render all the particles of a particle system with a given depth.
*@param pParticleSystem the particle system.
*@param iDepth depth of the particles that will be rendered. If set to -1, only particles with a negative z will be rendered, if set to 1, only particles with a positive z will be rendered, if set to 0, all the particles will be rendered.
*/
void cairo_dock_render_particles_full (CairoParticleSystem *pParticleSystem, int iDepth);
/** Render all the particles of a particle system.
*@param pParticleSystem the particle system.
*/
#define cairo_dock_render_particles(pParticleSystem) cairo_dock_render_particles_full (pParticleSystem, 0)

/**  Create a particle system.
*@param iNbParticles number of particles of the system.
*@param iTexture texture to map on each particle.
*@param fWidth width of the system.
*@param fHeight height of the system.
*@return a newly allocated particle system.
*/
CairoParticleSystem *cairo_dock_create_particle_system (int iNbParticles, GLuint iTexture, double fWidth, double fHeight);

/** Destroy a particle system, freeing all the ressources it was using.
*@param pParticleSystem the particle system.
*/
void cairo_dock_free_particle_system (CairoParticleSystem *pParticleSystem);

/** Update a particle system to the next step with a generic particle behavior model.
*@param pParticleSystem the particle system.
*@param pRewindParticle function called on a particle when its life is over.
*@return TRUE if some particles are still alive.
*/
gboolean cairo_dock_update_default_particle_system (CairoParticleSystem *pParticleSystem, CairoDockRewindParticleFunc pRewindParticle);


#endif
