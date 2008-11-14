static int numPs = 2
static ParticleSystem *ps;

static void fxBurnInit(CompScreen * s, CompWindow * w)
{
	if (!numPs) {
		ps = calloc(1, 2 * sizeof(ParticleSystem));
		numPs = 2;
	}
	initParticles(as->opt[ANIM_SCREEN_OPTION_FIRE_PARTICLES].value.i /
		      10, &ps[0]);
	initParticles(as->opt[ANIM_SCREEN_OPTION_FIRE_PARTICLES].value.i,
		      &ps[1]);
	ps[1].slowdown =
	    as->opt[ANIM_SCREEN_OPTION_FIRE_SLOWDOWN].value.f;
	ps[1].darken = 0.5;
	ps[1].blendMode = GL_ONE;

	ps[0].slowdown =
	    as->opt[ANIM_SCREEN_OPTION_FIRE_SLOWDOWN].value.f / 2.0;
	ps[0].darken = 0.0;
	ps[0].blendMode = GL_ONE_MINUS_SRC_ALPHA;

	if (!ps[0].tex)
		glGenTextures(1, &ps[0].tex);
	glBindTexture(GL_TEXTURE_2D, ps[0].tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, fireTex);
	glBindTexture(GL_TEXTURE_2D, 0);

	if (!ps[1].tex)
		glGenTextures(1, &ps[1].tex);
	glBindTexture(GL_TEXTURE_2D, ps[1].tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, fireTex);
	glBindTexture(GL_TEXTURE_2D, 0);

	int i;

	for (i = 0; i < LIST_SIZE(animDirectionName); i++) {
		if (strcmp
		    (as->opt[ANIM_SCREEN_OPTION_FIRE_DIRECTION].value.s,
		     animDirectionName[i]) == 0) {
			aw->animDirection = i;
		}
	}

	if (aw->animDirection == AnimDirectionRandom) {
		aw->animDirection = rand() % 4;
	}
	if (as->opt[ANIM_SCREEN_OPTION_FIRE_CONSTANT_SPEED].value.b) {
		aw->animTotalTime *= WIN_H(w) / 500.0;
		aw->animRemainingTime *= WIN_H(w) / 500.0;
	}
}

static void
fxBurnGenNewFire(CompScreen * s, ParticleSystem * ps, int x, int y,
		 int width, int height, float size, float time)
{
	ANIM_SCREEN(s);

	float max_new =
	    ps->numParticles * (time / 50) * (1.05 -
					      as->
					      opt
					      [ANIM_SCREEN_OPTION_FIRE_LIFE].
					      value.f);
	int i;
	Particle *part;
	float rVal;

	for (i = 0; i < ps->numParticles && max_new > 0; i++) {
		part = &ps->particles[i];
		if (part->life <= 0.0f) {
			// give gt new life
			rVal = (float) (random() & 0xff) / 255.0;
			part->life = 1.0f;
			part->fade = (rVal * (1 - as->opt[ANIM_SCREEN_OPTION_FIRE_LIFE].value.f)) + (0.2f * (1.01 - as->opt[ANIM_SCREEN_OPTION_FIRE_LIFE].value.f));	// Random Fade Value

			// set size
			part->width =
			    as->opt[ANIM_SCREEN_OPTION_FIRE_SIZE].value.f;
			part->height =
			    as->opt[ANIM_SCREEN_OPTION_FIRE_SIZE].value.f *
			    1.5;
			rVal = (float) (random() & 0xff) / 255.0;
			part->w_mod = size * rVal;
			part->h_mod = size * rVal;

			// choose random position
			rVal = (float) (random() & 0xff) / 255.0;
			part->x = x + ((width > 1) ? (rVal * width) : 0);
			rVal = (float) (random() & 0xff) / 255.0;
			part->y = y + ((height > 1) ? (rVal * height) : 0);
			part->z = 0.0;
			part->xo = part->x;
			part->yo = part->y;
			part->zo = part->z;

			// set speed and direction
			rVal = (float) (random() & 0xff) / 255.0;
			part->xi = ((rVal * 20.0) - 10.0f);
			rVal = (float) (random() & 0xff) / 255.0;
			part->yi = ((rVal * 20.0) - 15.0f);
			part->zi = 0.0f;
			rVal = (float) (random() & 0xff) / 255.0;

			if (as->opt[ANIM_SCREEN_OPTION_FIRE_MYSTICAL].
			    value.b) {
				// Random colors! (aka Mystical Fire)
				rVal = (float) (random() & 0xff) / 255.0;
				part->r = rVal;
				rVal = (float) (random() & 0xff) / 255.0;
				part->g = rVal;
				rVal = (float) (random() & 0xff) / 255.0;
				part->b = rVal;
			} else {
				// set color ABAB as->opt[ANIM_SCREEN_OPTION_FIRE_COLOR].value.f
				part->r =
				    (float) as->
				    opt[ANIM_SCREEN_OPTION_FIRE_COLOR].
				    value.c[0] / 0xffff -
				    (rVal / 1.7 *
				     (float) as->
				     opt[ANIM_SCREEN_OPTION_FIRE_COLOR].
				     value.c[0] / 0xffff);
				part->g =
				    (float) as->
				    opt[ANIM_SCREEN_OPTION_FIRE_COLOR].
				    value.c[1] / 0xffff -
				    (rVal / 1.7 *
				     (float) as->
				     opt[ANIM_SCREEN_OPTION_FIRE_COLOR].
				     value.c[1] / 0xffff);
				part->b =
				    (float) as->
				    opt[ANIM_SCREEN_OPTION_FIRE_COLOR].
				    value.c[2] / 0xffff -
				    (rVal / 1.7 *
				     (float) as->
				     opt[ANIM_SCREEN_OPTION_FIRE_COLOR].
				     value.c[2] / 0xffff);
			}
			// set transparancy
			part->a =
			    (float) as->opt[ANIM_SCREEN_OPTION_FIRE_COLOR].
			    value.c[3] / 0xffff;

			// set gravity
			part->xg = (part->x < part->xo) ? 1.0 : -1.0;
			part->yg = -3.0f;
			part->zg = 0.0f;

			ps->active = TRUE;
			max_new -= 1;
		} else {
			part->xg = (part->x < part->xo) ? 1.0 : -1.0;
		}
	}

}

static void
fxBurnGenNewSmoke(CompScreen * s, ParticleSystem * ps, int x, int y,
		  int width, int height, float size, float time)
{
	ANIM_SCREEN(s);

	float max_new =
	    ps->numParticles * (time / 50) * (1.05 -
					      as->
					      opt
					      [ANIM_SCREEN_OPTION_FIRE_LIFE].
					      value.f);
	int i;
	Particle *part;
	float rVal;

	for (i = 0; i < ps->numParticles && max_new > 0; i++) {
		part = &ps->particles[i];
		if (part->life <= 0.0f) {
			// give gt new life
			rVal = (float) (random() & 0xff) / 255.0;
			part->life = 1.0f;
			part->fade = (rVal * (1 - as->opt[ANIM_SCREEN_OPTION_FIRE_LIFE].value.f)) + (0.2f * (1.01 - as->opt[ANIM_SCREEN_OPTION_FIRE_LIFE].value.f));	// Random Fade Value

			// set size
			part->width =
			    as->opt[ANIM_SCREEN_OPTION_FIRE_SIZE].value.f *
			    size * 5;
			part->height =
			    as->opt[ANIM_SCREEN_OPTION_FIRE_SIZE].value.f *
			    size * 5;
			rVal = (float) (random() & 0xff) / 255.0;
			part->w_mod = -0.8;
			part->h_mod = -0.8;

			// choose random position
			rVal = (float) (random() & 0xff) / 255.0;
			part->x = x + ((width > 1) ? (rVal * width) : 0);
			rVal = (float) (random() & 0xff) / 255.0;
			part->y = y + ((height > 1) ? (rVal * height) : 0);
			part->z = 0.0;
			part->xo = part->x;
			part->yo = part->y;
			part->zo = part->z;

			// set speed and direction
			rVal = (float) (random() & 0xff) / 255.0;
			part->xi = ((rVal * 20.0) - 10.0f);
			rVal = (float) (random() & 0xff) / 255.0;
			part->yi = (rVal + 0.2) * -size;
			part->zi = 0.0f;

			// set color
			rVal = (float) (random() & 0xff) / 255.0;
			part->r = rVal / 4.0;
			part->g = rVal / 4.0;
			part->b = rVal / 4.0;
			rVal = (float) (random() & 0xff) / 255.0;
			part->a = 0.5 + (rVal / 2.0);

			// set gravity
			part->xg = (part->x < part->xo) ? size : -size;
			part->yg = -size;
			part->zg = 0.0f;

			ps->active = TRUE;
			max_new -= 1;
		} else {
			part->xg = (part->x < part->xo) ? size : -size;
		}
	}

}

static void fxBurnModelStep(CompScreen * s, CompWindow * w, float time)
{
	int steps;

	ANIM_SCREEN(s);
	ANIM_WINDOW(w);

	Model *model = aw->model;

	Bool smoke = as->opt[ANIM_SCREEN_OPTION_FIRE_SMOKE].value.b;

	float timestep = (s->slowAnimations ? 2 :	// For smooth slow-mo (refer to display.c)
			  as->opt[ANIM_SCREEN_OPTION_TIME_STEP_INTENSE].
			  value.i);
	float old = 1 - (aw->animRemainingTime) / (aw->animTotalTime);
	float stepSize;
	model->remainderSteps += time / timestep;
	steps = floor(model->remainderSteps);
	model->remainderSteps -= steps;
	if (!steps && aw->animRemainingTime < aw->animTotalTime)
		return;
	steps = MAX(1, steps);

	aw->animRemainingTime -= timestep;
	if (aw->animRemainingTime <= 0)
		aw->animRemainingTime = 0;	// avoid sub-zero values
	float new = 1 - (aw->animRemainingTime) / (aw->animTotalTime);

	stepSize = new - old;

	if (aw->curWindowEvent == WindowEventCreate ||
	    aw->curWindowEvent == WindowEventUnminimize ||
	    aw->curWindowEvent == WindowEventUnshade) {
		old = 1 - old;
		new = 1 - new;
	}

	if (!aw->drawRegion)
		aw->drawRegion = XCreateRegion();
	if (aw->animRemainingTime > 0) {
		XRectangle rect;
		switch (aw->animDirection) {
		case AnimDirectionUp:
			rect.x = 0;
			rect.y = 0;
			rect.width = WIN_W(w);
			rect.height = WIN_H(w) - (old * WIN_H(w));
			break;
		case AnimDirectionRight:
			rect.x = (old * WIN_W(w));
			rect.y = 0;
			rect.width = WIN_W(w) - (old * WIN_W(w));
			rect.height = WIN_H(w);
			break;
		case AnimDirectionLeft:
			rect.x = 0;
			rect.y = 0;
			rect.width = WIN_W(w) - (old * WIN_W(w));
			rect.height = WIN_H(w);
			break;
		case AnimDirectionDown:
		default:
			rect.x = 0;
			rect.y = (old * WIN_H(w));
			rect.width = WIN_W(w);
			rect.height = WIN_H(w) - (old * WIN_H(w));
			break;
		}
		XUnionRectWithRegion(&rect, getEmptyRegion(),
				     aw->drawRegion);
	} else {
		XUnionRegion(getEmptyRegion(), getEmptyRegion(),
			     aw->drawRegion);
	}
	if (new != 0)
		aw->useDrawRegion = TRUE;
	else
		aw->useDrawRegion = FALSE;

	if (aw->animRemainingTime > 0) {
		switch (aw->animDirection) {
		case AnimDirectionUp:
			if (smoke)
				fxBurnGenNewSmoke(s, &ps[0], WIN_X(w),
						  WIN_Y(w) +
						  ((1 - old) * WIN_H(w)),
						  WIN_W(w), 1,
						  WIN_W(w) / 40.0, time);
			fxBurnGenNewFire(s, &ps[1], WIN_X(w),
					 WIN_Y(w) + ((1 - old) * WIN_H(w)),
					 WIN_W(w), (stepSize) * WIN_H(w),
					 WIN_W(w) / 40.0, time);
			break;
		case AnimDirectionLeft:
			if (smoke)
				fxBurnGenNewSmoke(s, &ps[0],
						  WIN_X(w) +
						  ((1 - old) * WIN_W(w)),
						  WIN_Y(w),
						  (stepSize) * WIN_W(w),
						  WIN_H(w),
						  WIN_H(w) / 40.0, time);
			fxBurnGenNewFire(s, &ps[1],
					 WIN_X(w) + ((1 - old) * WIN_W(w)),
					 WIN_Y(w), (stepSize) * WIN_W(w),
					 WIN_H(w), WIN_H(w) / 40.0, time);
			break;
		case AnimDirectionRight:
			if (smoke)
				fxBurnGenNewSmoke(s, &ps[0],
						  WIN_X(w) +
						  (old * WIN_W(w)),
						  WIN_Y(w),
						  (stepSize) * WIN_W(w),
						  WIN_H(w),
						  WIN_H(w) / 40.0, time);
			fxBurnGenNewFire(s, &ps[1],
					 WIN_X(w) + (old * WIN_W(w)),
					 WIN_Y(w), (stepSize) * WIN_W(w),
					 WIN_H(w), WIN_H(w) / 40.0, time);
			break;
		case AnimDirectionDown:
		default:
			if (smoke)
				fxBurnGenNewSmoke(s, &ps[0], WIN_X(w),
						  WIN_Y(w) +
						  (old * WIN_H(w)),
						  WIN_W(w), 1,
						  WIN_W(w) / 40.0, time);
			fxBurnGenNewFire(s, &ps[1], WIN_X(w),
					 WIN_Y(w) + (old * WIN_H(w)),
					 WIN_W(w), (stepSize) * WIN_H(w),
					 WIN_W(w) / 40.0, time);
			break;
		}

	}
	if (aw->animRemainingTime <= 0 && aw->numPs
	    && (ps[0].active || ps[1].active))
		aw->animRemainingTime = timestep;

	if (!aw->numPs) {
		if (ps) {
			finiParticles(ps);
			free(ps);
			ps = NULL;
		}
		return;		// FIXME - is this correct behaviour?
	}

	int i;
	Particle *part;
	for (i = 0;
	     i < ps[0].numParticles && aw->animRemainingTime > 0
	     && smoke; i++) {
		part = &ps[0].particles[i];
		part->xg =
		    (part->x <
		     part->xo) ? WIN_W(w) / 40.0 : -WIN_W(w) / 40.0;
	}
	ps[0].x = WIN_X(w);
	ps[0].y = WIN_Y(w);

	for (i = 0;
	     i < ps[1].numParticles && aw->animRemainingTime > 0;
	     i++) {
		part = &ps[1].particles[i];
		part->xg = (part->x < part->xo) ? 1.0 : -1.0;
	}
	ps[1].x = WIN_X(w);
	ps[1].y = WIN_Y(w);

	modelCalcBounds(model);
}








// =====================  Particle engine  =========================

typedef struct _Particle {
	float life;		// particle life
	float fade;		// fade speed
	float width;		// particle width
	float height;		// particle height
	float w_mod;		// particle size modification during life
	float h_mod;		// particle size modification during life
	float r;		// red value
	float g;		// green value
	float b;		// blue value
	float a;		// alpha value
	float x;		// X position
	float y;		// Y position
	float z;		// Z position
	float xi;		// X direction
	float yi;		// Y direction
	float zi;		// Z direction
	float xg;		// X gravity
	float yg;		// Y gravity
	float zg;		// Z gravity
	float xo;		// orginal X position
	float yo;		// orginal Y position
	float zo;		// orginal Z position
} Particle;

typedef struct _ParticleSystem {
	int numParticles;
	Particle *particles;
	float slowdown;
	GLuint tex;
	Bool active;
	int x, y;
	float darken;
	GLuint blendMode;
} ParticleSystem;


static void initParticles(int numParticles, ParticleSystem * ps)
{
	if (ps->particles)
		free(ps->particles);
	ps->particles = calloc(1, sizeof(Particle) * numParticles);
	ps->tex = 0;
	ps->numParticles = numParticles;
	ps->slowdown = 1;
	ps->active = FALSE;

	int i;
	for (i = 0; i < numParticles; i++) {
		ps->particles[i].life = 0.0f;
	}
}

static void
drawParticles(CompScreen * s, CompWindow * w, ParticleSystem * ps)
{
	glPushMatrix();
	if (w)
		glTranslated(WIN_X(w) - ps->x, WIN_Y(w) - ps->y, 0);
	glEnable(GL_BLEND);
	if (ps->tex) {
		glBindTexture(GL_TEXTURE_2D, ps->tex);
		glEnable(GL_TEXTURE_2D);
	}
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	int i;
	Particle *part;

	GLfloat *vertices =
	    malloc(ps->numParticles * 4 * 3 * sizeof(GLfloat));
	GLfloat *coords =
	    malloc(ps->numParticles * 4 * 2 * sizeof(GLfloat));
	GLfloat *colors =
	    malloc(ps->numParticles * 4 * 4 * sizeof(GLfloat));
	GLfloat *dcolors = 0;

	GLfloat *vert = vertices;
	GLfloat *coord = coords;
	GLfloat *col = colors;

	if (ps->darken > 0) {
		dcolors =
		    malloc(ps->numParticles * 4 * 4 * sizeof(GLfloat));
	}

	GLfloat *dcol = dcolors;

	int numActive = 0;

	for (i = 0; i < ps->numParticles; i++) {
		part = &ps->particles[i];
		if (part->life > 0.0f) {
			numActive += 4;

			float w = part->width / 2;
			float h = part->height / 2;
			w += (w * part->w_mod) * part->life;
			h += (h * part->h_mod) * part->life;

			vert[0] = part->x - w;
			vert[1] = part->y - h;
			vert[2] = part->z;

			vert[3] = part->x - w;
			vert[4] = part->y + h;
			vert[5] = part->z;

			vert[6] = part->x + w;
			vert[7] = part->y + h;
			vert[8] = part->z;

			vert[9] = part->x + w;
			vert[10] = part->y - h;
			vert[11] = part->z;

			vert += 12;

			coord[0] = 0.0;
			coord[1] = 0.0;

			coord[2] = 0.0;
			coord[3] = 1.0;

			coord[4] = 1.0;
			coord[5] = 1.0;

			coord[6] = 1.0;
			coord[7] = 0.0;

			coord += 8;

			col[0] = part->r;
			col[1] = part->g;
			col[2] = part->b;
			col[3] = part->life * part->a;
			col[4] = part->r;
			col[5] = part->g;
			col[6] = part->b;
			col[7] = part->life * part->a;
			col[8] = part->r;
			col[9] = part->g;
			col[10] = part->b;
			col[11] = part->life * part->a;
			col[12] = part->r;
			col[13] = part->g;
			col[14] = part->b;
			col[15] = part->life * part->a;

			col += 16;

			if (ps->darken > 0) {

				dcol[0] = part->r;
				dcol[1] = part->g;
				dcol[2] = part->b;
				dcol[3] =
				    part->life * part->a * ps->darken;
				dcol[4] = part->r;
				dcol[5] = part->g;
				dcol[6] = part->b;
				dcol[7] =
				    part->life * part->a * ps->darken;
				dcol[8] = part->r;
				dcol[9] = part->g;
				dcol[10] = part->b;
				dcol[11] =
				    part->life * part->a * ps->darken;
				dcol[12] = part->r;
				dcol[13] = part->g;
				dcol[14] = part->b;
				dcol[15] =
				    part->life * part->a * ps->darken;

				dcol += 16;
			}
		}
	}

	glEnableClientState(GL_COLOR_ARRAY);

	glTexCoordPointer(2, GL_FLOAT, 2 * sizeof(GLfloat), coords);
	glVertexPointer(3, GL_FLOAT, 3 * sizeof(GLfloat), vertices);

	// darken of the background
	if (ps->darken > 0) {
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
		glColorPointer(4, GL_FLOAT, 4 * sizeof(GLfloat), dcolors);
		glDrawArrays(GL_QUADS, 0, numActive);
	}
	// draw particles
	glBlendFunc(GL_SRC_ALPHA, ps->blendMode);

	glColorPointer(4, GL_FLOAT, 4 * sizeof(GLfloat), colors);

	glDrawArrays(GL_QUADS, 0, numActive);

	glDisableClientState(GL_COLOR_ARRAY);

	free(vertices);
	free(coords);
	free(colors);

	if (ps->darken > 0) {
		free(dcolors);
	}

	glPopMatrix();
	glColor4usv(defaultColor);
	screenTexEnvMode(s, GL_REPLACE);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

static void updateParticles(ParticleSystem * ps, float time)
{
	int i;
	Particle *part;
	float speed = (time / 50.0);
	float slowdown =
	    ps->slowdown * (1 - MAX(0.99, time / 1000.0)) * 1000;
	ps->active = FALSE;

	for (i = 0; i < ps->numParticles; i++) {
		part = &ps->particles[i];
		if (part->life > 0.0f) {
			// move particle
			part->x += part->xi / slowdown;
			part->y += part->yi / slowdown;
			part->z += part->zi / slowdown;

			// modify speed
			part->xi += part->xg * speed;
			part->yi += part->yg * speed;
			part->zi += part->zg * speed;

			// modify life
			part->life -= part->fade * speed;
			ps->active = TRUE;
		}
	}
}

static void finiParticles(ParticleSystem * ps)
{
	free(ps->particles);
	if (ps->tex)
		glDeleteTextures(1, &ps->tex);
}

// =====================  END: Particle engine  =========================
