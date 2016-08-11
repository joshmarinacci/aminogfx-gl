#ifndef _SHADERS_H
#define _SHADERS_H

#include "gfx.h"

/**
 * Color Shader properties.
 */
class ColorShader {
public:
    GLuint prog = INVALID_PROGRAM;
    GLint u_matrix, u_trans, u_opacity;
    GLint attr_pos;
    GLint attr_color;

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
    GLint attr_texcoords, texID;

    TextureShader();

    void destroy();
};

#endif