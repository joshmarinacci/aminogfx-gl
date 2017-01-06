#ifndef _MATHUTILS_H
#define _MATHUTILS_H

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "gfx.h"

//these should probably move into the NodeStage class or a GraphicsUtils class
#define ASSERT_EQ(A, B) {if ((A) != (B)) {printf ("ERROR: %d\n", __LINE__); exit(9); }}
#define ASSERT_NE(A, B) {if ((A) == (B)) {printf ("ERROR: %d\n", __LINE__); exit(9); }}
#define EXPECT_TRUE(A) {if ((A) == 0) {printf ("ERROR: %d\n", __LINE__); exit(9); }}

void clear_matrix(GLfloat *m);

void make_y_rot_matrix(GLfloat angle, GLfloat *m);
void make_z_rot_matrix(GLfloat angle, GLfloat *m);
void make_x_rot_matrix(GLfloat angle, GLfloat *m);

void print_matrix(GLfloat *m);

void make_identity_matrix(GLfloat *m);

void make_scale_matrix(GLfloat xs, GLfloat ys, GLfloat zs, GLfloat *m);
void make_trans_matrix(GLfloat x, GLfloat y, GLfloat z, GLfloat *m);
void make_trans_z_matrix(GLfloat z, GLfloat *m);
void make_shear_x_matrix(GLfloat sx, GLfloat *m);
void make_shear_y_matrix(GLfloat sy, GLfloat *m);

bool make_quad_to_quad_matrix(GLfloat dx0, GLfloat dy0, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2, GLfloat dx3, GLfloat dy3, GLfloat sx0, GLfloat sy0, GLfloat sx1, GLfloat sy1, GLfloat sx2, GLfloat sy2, GLfloat sx3, GLfloat sy3, GLfloat *matrix);
bool make_quad_to_square_matrix(GLfloat sx0, GLfloat sy0, GLfloat sx1, GLfloat sy1, GLfloat sx2, GLfloat sy2, GLfloat sx3, GLfloat sy3, GLfloat *matrix);
bool make_square_to_quad_matrix(GLfloat dx0, GLfloat dy0, GLfloat dx1, GLfloat dy1, GLfloat dx2, GLfloat dy2, GLfloat dx3, GLfloat dy3, GLfloat *matrix);

void mul_matrix(GLfloat *prod, const GLfloat *a, const GLfloat *b);

void loadOrthoMatrix(GLfloat *modelView, GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near, GLfloat far);

void copy_matrix(GLfloat *dst, const GLfloat *src);

void loadPerspectiveMatrix(GLfloat *m, GLfloat fov, GLfloat aspect, GLfloat znear, GLfloat zfar);

void loadPixelPerfectMatrix(GLfloat *m, float width, float height, float z_eye, float z_near, float z_far);
void loadPixelPerfectOrthographicMatrix(GLfloat *m, float width, float height, float z_eye, float z_near, float z_far);

void transpose_matrix(GLfloat *b, const GLfloat *a);

bool invert_matrix(const GLfloat m[16], GLfloat invOut[16]);

#endif