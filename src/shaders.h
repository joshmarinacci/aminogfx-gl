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

    //params
    virtual void setTransformation(GLfloat modelView[16], GLfloat transition[16]);

    //per vertex data
    void setVertexData(GLsizei dim, GLfloat *vertices);

    //draw
    virtual void drawTriangles(GLsizei vertices, GLenum mode);
    virtual void drawElements(GLushort *indices, GLsizei elements, GLenum mode);

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
 * Color Lighting Shader.
 */
class ColorLightingShader : public ColorShader {
public:
    ColorLightingShader();

    //params
    void setTransformation(GLfloat modelView[16], GLfloat transition[16]) override;
    void setLightDirection(GLfloat color[3]);

    //per vertex values
    void setNormalVectors(GLfloat *normals);

    //draw
    void drawTriangles(GLsizei vertices, GLenum mode) override;
    void drawElements(GLushort *indices, GLsizei elements, GLenum mode) override;

protected:
    GLint aNormal;
    //GLint uNormalMatrix;
    GLint uLightDir;

    void initShader() override;
};

/**
 * Texture Shader.
 */
class TextureShader : public AnyAminoShader {
public:
    TextureShader();

    //params
    void setOpacity(GLfloat opacity);

    //per vertex data
    void setTextureCoordinates(GLfloat uv[][2]);

    //draw
    void drawTriangles(GLsizei vertices, GLenum mode) override;

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