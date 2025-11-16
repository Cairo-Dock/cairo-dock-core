/*
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

#ifndef __CAIRO_DOCK_OPENGL_PATH__
#define  __CAIRO_DOCK_OPENGL_PATH__

G_BEGIN_DECLS

/**
*@file cairo-dock-opengl-path.h This class define OpenGL path, with similar functions as cairo.
* You create a path with \ref cairo_dock_new_gl_path, then you add lines, curves or arcs to it.
* Once the path is defined, you can eigher stroke it with \ref cairo_dock_stroke_gl_path or fill it with \ref cairo_dock_fill_gl_path. You can fill a path with the current color or with a texture, in this case you must provide the dimension of the husk.
* To destroy the path, use \ref cairo_dock_free_gl_path.
*/

/// Definition of a CairoDockGLPath.
struct _CairoDockGLPath {
	int iNbPoints;
	GLfloat *pVertices;
	int iCurrentPt;
	int iWidth, iHeight;
	};

/** Create a new path. It will start at the point (x0, y0). If you want to be abe to fill it with a texture, you can specify here the dimension of the path's husk.
 * @param iNbVertices maximum number of vertices the path will have
 * @param x0 x coordinate of the origin point
 * @param y0 y coordinate of the origin point
 * @param iWidth width of the husk of the path.
 * @param iHeight height of the husk of the path
 * @return a newly allocated path, with 1 point.
 */
CairoDockGLPath *cairo_dock_new_gl_path (int iNbVertices, double x0, double y0, int iWidth, int iHeight);

/** Destroy a path and free its allocated resources.
 * @param pPath the path.
 */
void cairo_dock_free_gl_path (CairoDockGLPath *pPath);

/** Rewind the path, defining its origin point. The path has only 1 point after a call to this function.
 * @param pPath the path.
 * @param x0 x coordinate of the origin point
 * @param y0 y coordinate of the origin point
 */
void cairo_dock_gl_path_move_to (CairoDockGLPath *pPath, double x0, double y0);

/** Define the dimension of the hulk. This is needed if you intend to fill the path with a texture.
 * @param pPath the path.
 * @param iWidth width of the hulk
 * @param iHeight height of the hulk
 */
void cairo_dock_gl_path_set_extent (CairoDockGLPath *pPath, int iWidth, int iHeight);

/** Add a line between the current point and a given point.
 * @param pPath the path.
 * @param x x coordinate of the point
 * @param y y coordinate of the point
 */
void cairo_dock_gl_path_line_to (CairoDockGLPath *pPath, GLfloat x, GLfloat y);

/** Add a line defined relatively to the current point.
 * @param pPath the path.
 * @param dx horizontal offset
 * @param dy vertical offset
 */
void cairo_dock_gl_path_rel_line_to (CairoDockGLPath *pPath, GLfloat dx, GLfloat dy);

/** Add a Bezier cubic curve starting from the current point.
 * @param pPath the path.
 * @param iNbPoints number of points used to discretize the curve
 * @param x1 first control point x
 * @param y1 first control point y
 * @param x2 second control point x
 * @param y2 second control point y
 * @param x3 terminal point of the curve x
 * @param y3 terminal point of the curve y
 */
void cairo_dock_gl_path_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2, GLfloat x3, GLfloat y3);

/** Add a Bezier cubic curve starting from the current point. The control and terminal points are defined relatively to the current point.
 * @param pPath the path.
 * @param iNbPoints number of points used to discretize the curve
 * @param dx1 first control point offset x
 * @param dy1 first control point offset y
 * @param dx2 second control point offset x
 * @param dy2 second control point offset y
 * @param dx3 terminal point of the curve offset x
 * @param dy3 terminal point of the curve offset y
 */
void cairo_dock_gl_path_rel_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2, GLfloat dx3, GLfloat dy3);

/** Add a Bezier bilinear curve starting from the current point
 * @param pPath the path.
 * @param iNbPoints number of points used to discretize the curve
 * @param x1 control point x
 * @param y1 control point y
 * @param x2 terminal point of the curve x
 * @param y2 terminal point of the curve y
 */
void cairo_dock_gl_path_simple_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);

/** Add a Bezier bilinear curve starting from the current point. The control and terminal points are defined relatively to the current point.
 * @param pPath the path.
 * @param iNbPoints number of points used to discretize the curve
 * @param dx1 control point offset x
 * @param dy1 control point offset y
 * @param dx2 terminal point of the curve offset x
 * @param dy2 terminal point of the curve offset y
 */
void cairo_dock_gl_path_rel_simple_curve_to (CairoDockGLPath *pPath, int iNbPoints, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2);

/** Add an arc to the path, joining the current point to the beginning of the arc with a line.
 * @param pPath the path.
 * @param iNbPoints number of points used to discretize the arc
 * @param xc x coordinate of the center
 * @param yc y coordinate of the center
 * @param r radius
 * @param teta0 initial angle
 * @param cone cone of the arc (a negative value means clockwise).
 */
void cairo_dock_gl_path_arc (CairoDockGLPath *pPath, int iNbPoints, GLfloat xc, GLfloat yc, double r, double teta0, double cone);

/** Stroke a path with the current color and with the current line width.
 * @param pPath the path.
 * @param bClosePath whether to close the path (that is to say, join the last point with the first one) or not.
 */
void cairo_dock_stroke_gl_path (const CairoDockGLPath *pPath, gboolean bClosePath);

/** Fill a path with a texture, or with the current color if the texture is 0.
 * @param pPath the path.
 * @param iTexture the texture, or 0 to fill the path with the current color. To fill the path with a gradation, use GL_COLOR_ARRAY and feed it with a table of colors that matches the vertices.
 */
void cairo_dock_fill_gl_path (const CairoDockGLPath *pPath, GLuint iTexture);


const CairoDockGLPath *cairo_dock_generate_rectangle_path (double fFrameWidth, double fTotalHeight, double fRadius, gboolean bRoundedBottomCorner);

const CairoDockGLPath *cairo_dock_generate_trapeze_path (double fUpperFrameWidth, double fTotalHeight, double fRadius, gboolean bRoundedBottomCorner, double fInclination, double *fExtraWidth);

const CairoDockGLPath *cairo_dock_generate_string_path_opengl (CairoDock *pDock, gboolean bIsLoop, gboolean bForceConstantSeparator);

void cairo_dock_draw_current_path_opengl (double fLineWidth, double *fLineColor, int iNbVertex);

/** Draw a rectangle with rounded corners. The rectangle will be centered at the current point. The current matrix is not altered.
*@param fFrameWidth width of the rectangle, without the corners.
*@param fFrameHeight height of the rectangle, including the corners.
*@param fRadius radius of the corners (can be 0).
*@param fLineWidth width of the line. If set to 0, the background will be filled with the provided color, otherwise the path will be stroke with this color.
*@param fLineColor color of the line if fLineWidth is non nul, or color of the background otherwise.
*/
void cairo_dock_draw_rounded_rectangle_opengl (double fFrameWidth, double fFrameHeight, double fRadius, double fLineWidth, double *fLineColor);

void cairo_dock_draw_string_opengl (CairoDock *pDock, double fStringLineWidth, gboolean bIsLoop, gboolean bForceConstantSeparator);


G_END_DECLS
#endif
