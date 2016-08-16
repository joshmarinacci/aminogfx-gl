#ifndef _SHADERS_H
#define _SHADERS_H

#include "gfx.h"

/**
 * Color Shader properties.
 */
class ColorShader {
public:
    GLuint prog = INVALID_PROGRAM;
    GLint u_matrix, u_trans, u_color;
    GLint attr_pos;

    ColorShader();

    void destroy();
};

/**
 * Texture Shader properties.
 */
class TextureShader {
public:
    GLuint prog = INVALID_PROGRAM;
    GLint u_matrix, u_trans, u_opacity;
    GLint attr_pos;
    GLint attr_texcoords;

    TextureShader();

    void destroy();
};

#endif