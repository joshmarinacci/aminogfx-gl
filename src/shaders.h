#ifndef _SHADERS_H
#define _SHADERS_H

#include "gfx.h"

/**
 * Color Shader properties.
 */
class ColorShader {
public:
    int prog;
    GLint u_matrix, u_trans, u_opacity;
    GLint attr_pos;
    GLint attr_color;

    ColorShader();
};

/**
 * Texture Shader properties.
 */
class TextureShader {
public:
    int prog;
    GLint u_matrix, u_trans, u_opacity;
    GLint attr_pos;
    GLint attr_texcoords, texID;

    TextureShader();
};

#endif