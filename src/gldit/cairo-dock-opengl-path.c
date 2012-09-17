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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <glib/gstdio.h>

#include <pango/pango.h>


#include <X11/extensions/Xrender.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#include "cairo-dock-struct.h"
#include "cairo-dock-icon-facility.h"  // cairo_dock_generate_string_path_opengl
#include "cairo-dock-dock-factory.h"
#include "cairo-dock-opengl-path.h"

#define _CD_PATH_DIM 2
#define _cd_gl_path_set_nth_vertex_x(pPath, _x, i) pPath->pVertices[_CD_PATH_DIM*(i)] = _x
#define _cd_gl_path_set_nth_vertex_y(pPath, _y, i) pPath->pVertices[_CD_PATH_DIM*(i)+1] = _y
#define _cd_gl_path_set_vertex_x(pPath, _x) _cd_gl_path_set_nth_vertex_x (pPath, _x, pPath->iCurrentPt)
#define _cd_gl_path_set_vertex_y(pPath, _y) _cd_gl_path_set_nth_vertex_y (pPath, _y, pPath->iCurrentPt)
#define _cd_gl_path_set_current_vertex(pPath, _x, _y) do {\
	_cd_gl_path_set_vertex_x(pPath, _x);\
	_cd_gl_path_set_vertex_y(pPath, _y); } while (0)
#define _cd_gl_path_get_nth_vertex_x(pPath, i) pPath->pVertices[_CD_PATH_DIM*(i)]
#define _cd_gl_path_get_nth_vertex_y(pPath, i) pPath->pVertices[_CD_PATH_DIM*(i)+1]
#define _cd_gl_path_get_current_vertex_x(pPath) _cd_gl_path_get_nth_vertex_x (pPath, pPath->iCurrentPt-1)
#define _cd_gl_path_get_current_vertex_y(pPath) _cd_gl_path_get_nth_vertex_y (pPath, pPath->iCurrentPt-1)

CairoDockGLPath *cairo_dock_new_gl_path (int iNbVertices, double x0, double y0, int iWidth, int iHeight)
{
	CairoDockGLPath *pPath = g_new0 (CairoDockGLPath, 1);
	pPath->pVertices = g_new0 (GLfloat, (iNbVertices+1) * _CD_PATH_DIM);  // +1 = securite
	pPath->iNbPoints = iNbVertices;
	_cd_gl_path_set_current_vertex (pPath, x0, y0);
	pPath->iCurrentPt ++;
	pPath->iWidth = iWidth;
	pPath->iHeight = iHeight;
	return pPath;
}

void cairo_dock_free_gl_path (CairoDockGLPath *pPath)
{
	if (!pPath)
		return;
	g_free (pPath->pVertices);
	g_free (pPath);
}

void cairo_dock_gl_path_move_to (CairoDockGLPath *pPath, double x0, double y0)
{
	pPath->iCurrentPt = 0;
	_cd_gl_path_set_current_vertex (pPath, x0, y0);
	pPath->iCurrentPt ++;
}

void cairo_dock_gl_path_set_extent (CairoDockGLPath *pPath, int iWidth, int iHeight)
{
	pPath->iWidth = iWidth;
	pPath->iHeight = iHeight;
}

void cairo_dock_gl_path_line_to (CairoDockGLPath *pPath, GLfloat x, GLfloat y)
{
	g_return_if_fail (pPath->iCurrentPt < pPath->iNbPoints);
	_cd_gl_path_set_current_vertex (pPath, x, y);
	pPath->iCurrentPt ++;
}

void cairo_dock_gl_path_rel_line_to (CairoDockGLPath *pPath, GLfloat dx, GLfloat dy)
{
	cairo_dock_gl_path_line_to (pPath,
		_cd_gl_path_get_current_vertex_x (pPath) + dx,
		_cd_gl_path_get_current_vertex_y (pPath) + dy);
}

// OM(t) = sum ([k=0..n] Bn,k(t)*OAk)
// Bn,k(x) = Cn,k*x^k*(1-x)^(n-k)
#define B0(t) (1-t)*(1-t)*(1-t)
#define B1(t) 3*t*(1-t)*(1-t)
#define B2(t) 3*t*t*(1-t)
#define B3(t) t*t*t
#define Bezier(x0,x1,x2,x3,t) (B0(t)*x0 + B1(t)*x1 + B2(t)*x2 + B3(t)*x3)
void cairo_dock_gl_path_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2, GLfloat x3, GLfloat y3)
{
	g_return_if_fail (pPath->iCurrentPt + iNbPoints <= pPath->iNbPoints);
	GLfloat x0, y0;
	x0 = _cd_gl_path_get_current_vertex_x (pPath);
	y0 = _cd_gl_path_get_current_vertex_y (pPath);
	double t;
	int i;
	for (i = 0; i < iNbPoints; i ++)
	{
		t = (double)(i+1)/iNbPoints;  // [0;1]
		_cd_gl_path_set_nth_vertex_x (pPath, Bezier (x0, x1, x2, x3, t), pPath->iCurrentPt + i);
		_cd_gl_path_set_nth_vertex_y (pPath, Bezier (y0, y1, y2, y3, t), pPath->iCurrentPt + i);
	}
	pPath->iCurrentPt += iNbPoints;
}

void cairo_dock_gl_path_rel_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2, GLfloat dx3, GLfloat dy3)
{
	GLfloat x0, y0;
	x0 = _cd_gl_path_get_current_vertex_x (pPath);
	y0 = _cd_gl_path_get_current_vertex_y (pPath);
	cairo_dock_gl_path_curve_to (pPath, iNbPoints, x0 + dx1, y0 + dy1, x0 + dx2, y0 + dy2, x0 + dx3, y0 + dy3);
}

#define Bezier2(p,q,s,t) ((1-t) * (1-t) * p + 2 * t * (1-t) * q + t * t * s)
void cairo_dock_gl_path_simple_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	g_return_if_fail (pPath->iCurrentPt + iNbPoints <= pPath->iNbPoints);
	GLfloat x0, y0;
	x0 = _cd_gl_path_get_current_vertex_x (pPath);
	y0 = _cd_gl_path_get_current_vertex_y (pPath);
	double t;
	int i;
	for (i = 0; i < iNbPoints; i ++)
	{
		t = (double)(i+1)/iNbPoints;  // [0;1]
		_cd_gl_path_set_nth_vertex_x (pPath, Bezier2 (x0, x1, x2, t), pPath->iCurrentPt + i);
		_cd_gl_path_set_nth_vertex_y (pPath, Bezier2 (y0, y1, y2, t), pPath->iCurrentPt + i);
	}
	pPath->iCurrentPt += iNbPoints;
}

void cairo_dock_gl_path_rel_simple_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2)
{
	GLfloat x0, y0;
	x0 = _cd_gl_path_get_current_vertex_x (pPath);
	y0 = _cd_gl_path_get_current_vertex_y (pPath);
	cairo_dock_gl_path_simple_curve_to (pPath, iNbPoints, x0 + dx1, y0 + dy1, x0 + dx2, y0 + dy2);
}

void cairo_dock_gl_path_arc (CairoDockGLPath *pPath, int iNbPoints, GLfloat xc, GLfloat yc, double r, double teta0, double cone)
{
	g_return_if_fail (pPath->iCurrentPt + iNbPoints <= pPath->iNbPoints);
	double t;
	int i;
	for (i = 0; i < iNbPoints; i ++)
	{
		t = teta0 + (double)i/(iNbPoints-1) * cone;  // [teta0, teta0+cone]
		_cd_gl_path_set_nth_vertex_x (pPath, xc + r * cos (t), pPath->iCurrentPt + i);
		_cd_gl_path_set_nth_vertex_y (pPath, yc + r * sin (t), pPath->iCurrentPt + i);
	}
	pPath->iCurrentPt += iNbPoints;
}

static inline void _draw_current_path (int iNbPoints, gboolean bClosePath)
{
	//\__________________ On active l'antialiasing.
	glPolygonMode (GL_FRONT, GL_LINE);
	glEnable (GL_LINE_SMOOTH);
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable (GL_BLEND);  // On active le blend pour l'antialiasing.
	//glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glLineWidth(fLineWidth);
	//glColor4f (fLineColor[0], fLineColor[1], fLineColor[2], fLineColor[3]); // Et sa couleur.
	
	//\__________________ On dessine le cadre.
	glEnableClientState (GL_VERTEX_ARRAY);
	glDrawArrays (bClosePath ? GL_LINE_LOOP : GL_LINE_STRIP, 0, iNbPoints);
	glDisableClientState (GL_VERTEX_ARRAY);
	
	//\__________________ On desactive l'antialiasing.
	glDisable (GL_LINE_SMOOTH);
	glDisable (GL_BLEND);
}
void cairo_dock_stroke_gl_path (const CairoDockGLPath *pPath, gboolean bClosePath)
{
	glVertexPointer (_CD_PATH_DIM, GL_FLOAT, 0, pPath->pVertices);
	_draw_current_path (pPath->iCurrentPt, bClosePath);
}

void cairo_dock_fill_gl_path (const CairoDockGLPath *pPath, GLuint iTexture)
{
	//\__________________ On active l'antialiasing.
	glPolygonMode (GL_FRONT, GL_FILL);
	//glEnable (GL_POLYGON_SMOOTH);  // makes horrible white lines where the triangles overlaps :-/
	//glHint (GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glEnable (GL_BLEND);  // On active le blend pour l'antialiasing.
	
	//\__________________ On mappe la texture dans le cadre.
	if (iTexture != 0)
	{
		// on active le texturing.
		glColor4f(1., 1., 1., 1.); // Couleur a fond
		glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D); // Je veux de la texture
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBindTexture(GL_TEXTURE_2D, iTexture); // allez on bind la texture
		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR ); // ok la on selectionne le type de generation des coordonnees de la texture
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		glEnable(GL_TEXTURE_GEN_S); // oui je veux une generation en S
		glEnable(GL_TEXTURE_GEN_T); // Et en T aussi
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		// on met la texture a sa place.
		glMatrixMode(GL_TEXTURE);
		glPushMatrix ();
		glTranslatef (.5, .5, 0.);
		if (pPath->iWidth != 0 && pPath->iHeight != 0)
			glScalef (1./pPath->iWidth, -1./pPath->iHeight, 1.);
		glMatrixMode (GL_MODELVIEW);
		
	}
	
	//\__________________ On dessine le cadre.
	glEnableClientState (GL_VERTEX_ARRAY);
	glVertexPointer (_CD_PATH_DIM, GL_FLOAT, 0, pPath->pVertices);
	glDrawArrays (GL_TRIANGLE_FAN, 0, pPath->iCurrentPt);  // GL_POLYGON / GL_TRIANGLE_FAN
	glDisableClientState (GL_VERTEX_ARRAY);
	
	//\__________________ On desactive l'antialiasing et la texture.
	if (iTexture != 0)
	{
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_2D); // Plus de texture merci
		
		glMatrixMode(GL_TEXTURE);
		glPopMatrix ();
		glMatrixMode (GL_MODELVIEW);
	}
	//glDisable (GL_POLYGON_SMOOTH);
	glDisable (GL_BLEND);
}


// HELPER FUNCTIONS //

#define DELTA_ROUND_DEGREE 3
const CairoDockGLPath *cairo_dock_generate_rectangle_path (double fFrameWidth, double fTotalHeight, double fRadius, gboolean bRoundedBottomCorner)
{
	static CairoDockGLPath *pPath = NULL;
	double fTotalWidth = fFrameWidth + 2 * fRadius;
	double fFrameHeight = MAX (0, fTotalHeight - 2 * fRadius);
	double w = fFrameWidth / 2;
	double h = fFrameHeight / 2;
	double r = fRadius;
	
	int iNbPoins1Round = 90/10;
	if (pPath == NULL)
	{
		pPath = cairo_dock_new_gl_path ((iNbPoins1Round+1)*4+1, w+r, h, fTotalWidth, fTotalHeight);  // on commence au centre droit pour avoir une bonne triangulation du polygone, et en raisonnant par rapport au centre du rectangle.
		///pPath = cairo_dock_new_gl_path ((iNbPoins1Round+1)*4+1, 0, 0, fTotalWidth, fTotalHeight);  // on commence au centre pour avoir une bonne triangulation
	}
	else
	{
		cairo_dock_gl_path_move_to (pPath, w+r, h);
		///cairo_dock_gl_path_move_to (pPath, 0, 0);
		cairo_dock_gl_path_set_extent (pPath, fTotalWidth, fTotalHeight);
	}
	//cairo_dock_gl_path_move_to (pPath, 0., h+r);
	//cairo_dock_gl_path_rel_line_to (pPath, -w, 0.);
	
	cairo_dock_gl_path_arc (pPath, iNbPoins1Round, w, h, r, 0.,     +G_PI/2);  // coin haut droit.
	
	cairo_dock_gl_path_arc (pPath, iNbPoins1Round, -w,  h, r, G_PI/2,  +G_PI/2);  // coin haut gauche.
	
	if (bRoundedBottomCorner)
	{
		cairo_dock_gl_path_arc (pPath, iNbPoins1Round, -w, -h, r, G_PI,    +G_PI/2);  // coin bas gauche.
		
		cairo_dock_gl_path_arc (pPath, iNbPoins1Round,  w, -h, r, -G_PI/2, +G_PI/2);  // coin bas droit.
	}
	else
	{
		cairo_dock_gl_path_rel_line_to (pPath, 0., - (fFrameHeight + r));
		cairo_dock_gl_path_rel_line_to (pPath, fTotalWidth, 0.);
	}
	//cairo_dock_gl_path_arc (pPath, iNbPoins1Round, w, h, r, 0.,     +G_PI/2);  // coin haut droit.
	
	return pPath;
}


const CairoDockGLPath *cairo_dock_generate_trapeze_path (double fUpperFrameWidth, double fTotalHeight, double fRadius, gboolean bRoundedBottomCorner, double fInclination, double *fExtraWidth)
{
	static CairoDockGLPath *pPath = NULL;
	
	double a = atan (fInclination);
	double cosa = 1. / sqrt (1 + fInclination * fInclination);
	double sina = cosa * fInclination;
	
	double fFrameHeight = MAX (0, fTotalHeight - 2 * fRadius);
	*fExtraWidth = fInclination * (fFrameHeight - (bRoundedBottomCorner ? 2 : 1-cosa) * fRadius) + fRadius * (bRoundedBottomCorner ? 1 : sina);
	double fTotalWidth = fUpperFrameWidth + 2*(*fExtraWidth);
	double dw = (bRoundedBottomCorner ? fInclination * (fFrameHeight) : *fExtraWidth);
	double r = fRadius;
	double w = fUpperFrameWidth / 2;
	double h = fFrameHeight / 2;
	double w_ = w + dw + (bRoundedBottomCorner ? 0 : 0*r * cosa);
	
	int iNbPoins1Round = 70/DELTA_ROUND_DEGREE;  // pour une inclinaison classique (~30deg), les coins du haut feront moins d'1/4 de tour.
	int iNbPoins1Curve = 10;
	if (pPath == NULL)
		pPath = cairo_dock_new_gl_path ((iNbPoins1Round+1)*2 + (iNbPoins1Curve+1)*2 + 1, 0., fTotalHeight/2, fTotalWidth, fTotalHeight);
	else
	{
		cairo_dock_gl_path_move_to (pPath, 0., fTotalHeight/2);
		cairo_dock_gl_path_set_extent (pPath, fTotalWidth, fTotalHeight);
	}
	cairo_dock_gl_path_arc (pPath, iNbPoins1Round, -w, h, r, G_PI/2, G_PI/2 - a);  // coin haut gauche. 90 -> 180-a
	
	if (bRoundedBottomCorner)
	{
		double t = G_PI-a;
		double x0 = -w_ + r * cos (t);
		double y0 = -h + r * sin (t);
		double x1 = x0 - fInclination * r * (1+sina);
		double y1 = -h - r;
		double x2 = -w_;
		double y2 = y1;
		cairo_dock_gl_path_line_to (pPath, x0, y0);
		cairo_dock_gl_path_simple_curve_to (pPath, iNbPoins1Curve, x1, y1, x2, y2);  // coin bas gauche.
		
		double x3 = x0, y3 = y0;  // temp.
		x0 = - x2;
		y0 = y2;
		x1 = - x1;
		x2 = - x3;
		y2 = y3;
		cairo_dock_gl_path_line_to (pPath, x0, y0);
		cairo_dock_gl_path_simple_curve_to (pPath, iNbPoins1Curve, x1, y1, x2, y2);  // coin bas droit.
	}
	else
	{
		cairo_dock_gl_path_line_to (pPath,
			-w_,
			-h - r);  // bas gauche.
		cairo_dock_gl_path_line_to (pPath,
			w_,
			-h - r);  // bas droit.
	}
	
	cairo_dock_gl_path_arc (pPath, iNbPoins1Round, w, h, r, a, G_PI/2 - a);  // coin haut droit. a -> 90
	
	return pPath;
}


#define _get_icon_center_x(icon) (icon->fDrawX + icon->fWidth * icon->fScale/2)
#define _get_icon_center_y(icon) (icon->fDrawY + (bForceConstantSeparator && CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (icon) ? icon->fHeight * (icon->fScale - .5) : icon->fHeight * icon->fScale/2))
#define _get_icon_center(icon,x,y) do {\
	if (pDock->container.bIsHorizontal) {\
		x = _get_icon_center_x (icon);\
		y = pDock->container.iHeight - _get_icon_center_y (icon); }\
	else {\
		 y = _get_icon_center_x (icon);\
		 x = pDock->container.iWidth - _get_icon_center_y (icon); } } while (0)
#define _calculate_slope(x0,y0,x1,y1,dx,dy) do {\
	dx = x1 - x0;\
	dy = y1 - y0;\
	norme = sqrt (dx*dx + dy*dy);\
	dx /= norme;\
	dy /= norme; } while (0)
#define NB_VERTEX_PER_ICON_PAIR 10
const CairoDockGLPath *cairo_dock_generate_string_path_opengl (CairoDock *pDock, gboolean bIsLoop, gboolean bForceConstantSeparator)
{
	static CairoDockGLPath *pPath = NULL;
	if (pPath == NULL)
		pPath = cairo_dock_new_gl_path (100*NB_VERTEX_PER_ICON_PAIR + 1, 0., 0., 0., 0.);
	
	GList *ic, *next_ic, *next2_ic, *pFirstDrawnElement = pDock->icons;
	Icon *pIcon, *pNextIcon, *pNext2Icon;
	double x0,y0, x1,y1, x2,y2;  // centres des icones P0, P1, P2, en coordonnees opengl.
	double norme;  // pour normaliser les pentes.
	double dx, dy;  // direction au niveau de l'icone courante P0.
	double dx_, dy_;  // direction au niveau de l'icone suivante P1.
	double x0_,y0_, x1_,y1_;  // points de controle entre P0 et P1.
	if (pFirstDrawnElement == NULL)
	{
		return pPath;
	}
	
	// direction initiale.
	ic = pFirstDrawnElement;
	pIcon = ic->data;
	_get_icon_center (pIcon,x0,y0);
	next_ic = cairo_dock_get_next_element (ic, pDock->icons);
	pNextIcon = next_ic->data;
	_get_icon_center (pNextIcon,x1,y1);
	if (! bIsLoop)
	{
		_calculate_slope (x0,y0, x1,y1, dx,dy);
	}
	else
	{
		next2_ic = cairo_dock_get_previous_element (ic, pDock->icons);  // icone precedente dans la boucle.
		pNext2Icon = next2_ic->data;
		_get_icon_center (pNext2Icon,x2,y2);
		_calculate_slope (x2,y2, x0,y0, dx,dy);
	}
	// point suivant.
	next2_ic = cairo_dock_get_next_element (next_ic, pDock->icons);
	pNext2Icon = next2_ic->data;
	_get_icon_center (pNext2Icon,x2,y2);
	
	cairo_dock_gl_path_move_to (pPath, x0, y0);
	if (pDock->container.bIsHorizontal)
		cairo_dock_gl_path_set_extent (pPath, pDock->container.iWidth, pDock->container.iHeight);
	else
		cairo_dock_gl_path_set_extent (pPath, pDock->container.iHeight, pDock->container.iWidth);
	
	// on parcourt les icones.
	do
	{
		// l'icone courante, la suivante, et celle d'apres.
		pIcon = ic->data;
		pNextIcon = next_ic->data;
		pNext2Icon = next2_ic->data;
		
		// on va tracer de (x0,y0) a (x1,y1)
		_get_icon_center (pIcon,x0,y0);
		_get_icon_center (pNextIcon,x1,y1);
		_get_icon_center (pNext2Icon,x2,y2);
		
		// la pente au point (x1,y1)
		_calculate_slope (x0,y0, x2,y2, dx_,dy_);
		
		// points de controle.
		norme = sqrt ((x1-x0) * (x1-x0) + (y1-y0) * (y1-y0))/2;  // distance de prolongation suivant la pente.
		x0_ = x0 + dx * norme;
		y0_ = y0 + dy * norme;
		x1_ = x1 - dx_ * norme;
		y1_ = y1 - dy_ * norme;
		
		cairo_dock_gl_path_curve_to (pPath, NB_VERTEX_PER_ICON_PAIR,
			x0_, y0_,
			x1_, y1_,
			x1,  y1);
		
		// on decale tout d'un cran.
		ic = next_ic;
		next_ic = next2_ic;
		next2_ic = cairo_dock_get_next_element (next_ic, pDock->icons);
		dx = dx_;
		dy = dy_;
		if (next_ic == pFirstDrawnElement && ! bIsLoop)
			break ;
	}
	while (ic != pFirstDrawnElement);
	
	return pPath;
}


void cairo_dock_draw_current_path_opengl (double fLineWidth, double *fLineColor, int iNbVertex)
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(fLineWidth); // Ici on choisit l'epaisseur du contour du polygone.
	if (fLineColor != NULL)
		glColor4f (fLineColor[0], fLineColor[1], fLineColor[2], fLineColor[3]); // Et sa couleur.
	
	_draw_current_path (iNbVertex, FALSE);
}


void cairo_dock_draw_rounded_rectangle_opengl (double fFrameWidth, double fFrameHeight, double fRadius, double fLineWidth, double *fLineColor)
{
	const CairoDockGLPath *pPath = cairo_dock_generate_rectangle_path (fFrameWidth, fFrameHeight, fRadius, TRUE);
	
	if (fLineColor != NULL)
		glColor4f (fLineColor[0], fLineColor[1], fLineColor[2], fLineColor[3]);
	if (fLineWidth == 0)
	{
		cairo_dock_fill_gl_path (pPath, 0);
	}
	else
	{
		glLineWidth (fLineWidth);
		cairo_dock_stroke_gl_path (pPath, TRUE);
	}
}

void cairo_dock_draw_string_opengl (CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator)
{
	const CairoDockGLPath *pPath = cairo_dock_generate_string_path_opengl (pDock, bIsLoop, bForceConstantSeparator);
	if (pPath == NULL || pPath->iCurrentPt < 2)
		return;
	
	glLineWidth (fStringLineWidth);
	///glColor4f (myIconsParam.fStringColor[0], myIconsParam.fStringColor[1], myIconsParam.fStringColor[2], myIconsParam.fStringColor[3]);
	cairo_dock_stroke_gl_path (pPath, FALSE);
}
