#ifndef _SHADERS_H
#define _SHADERS_H

#include "gfx.h"

#include <string>

/**
 * Shader base class.
 */
class AnyShader {
public:
    AnyShader();
    virtual ~AnyShader();

    bool create();
    void destroy();

    void useShader(bool active);

protected:
    //code
    std::string vertexShader;
    std::string fragmentShader;

    //compiled
    GLuint prog = INVALID_PROGRAM;
    bool failed = false;
    std::string error;

    virtual void initShader() = 0;
    GLint getAttributeLocation(std::string name);
    GLint getUniformLocation(std::string name);

private:
    GLuint compileShader(std::string source, const GLenum type);
};

/**
 * Any AminoGfx shader.
 */
class AnyAminoShader : public AnyShader {
public:
    AnyAminoShader();

    void setTransformation(GLfloat modelView[16], GLfloat transition[16]);

    void drawTriangles(GLfloat *verts, GLsizei dim, GLsizei vertices, GLenum mode);
    void drawElements(GLushort *indices, GLsizei elements, GLenum mode);

protected:
    //position
    GLint aPos;

    //transition
    GLint uMVP, uTrans;

    void initShader() override;
};

/**
 * Color Shader.
 */
class ColorShader : public AnyAminoShader {
public:
    ColorShader();

    void setColor(GLfloat color[4]);

protected:
    GLint uColor;

    void initShader() override;
};

/**
 * Texture Shader.
 */
class TextureShader : public AnyAminoShader {
public:
    TextureShader();

    void setOpacity(GLfloat opacity);

    void drawTexture(GLfloat *verts, GLsizei dim, GLfloat texcoords[][2], GLsizei vertices, GLenum mode = GL_TRIANGLES);

protected:
    GLint aTexCoord;
    GLint uOpacity, uTex;

    void initShader() override;
};

/**
 * Texture shader supporting clamp to border.
 */
class TextureClampToBorderShader : public TextureShader {
public:
    TextureClampToBorderShader();
};

#endif