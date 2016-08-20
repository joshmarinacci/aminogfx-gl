#ifndef _SIMPLE_RENDERER_H
#define _SIMPLE_RENDERER_H

#include "base.h"

class GLContext {
public:
    std::stack<void *> matrixStack;
    GLfloat *globaltx = new GLfloat[16];
    GLfloat opacity = 1;

    int depth = 0;

    AnyAminoShader *prevShader = NULL;
    GLuint prevTex = INVALID_TEXTURE;

    GLContext() {
        //matrix
        make_identity_matrix(globaltx);
    }

    virtual ~GLContext() {
        assert(matrixStack.size() == 0);
        delete[] globaltx;
    }

    void dumpGlobalTransform() {
        print_matrix(globaltx);
    }

    void translate(GLfloat x, GLfloat y) {
        translate(x, y, 0);
    }

    void translate(GLfloat x, GLfloat y, GLfloat z) {
        GLfloat tr[16];
        GLfloat trans2[16];

        make_trans_matrix(x, y, z, tr);
        mul_matrix(trans2, globaltx, tr);
        copy_matrix(globaltx, trans2);
    }

    void rotate(GLfloat x, GLfloat y, GLfloat z) {
        GLfloat rot[16];
        GLfloat temp[16];

        make_x_rot_matrix(x, rot);
        mul_matrix(temp, globaltx, rot);
        copy_matrix(globaltx,temp);

        make_y_rot_matrix(y, rot);
        mul_matrix(temp, globaltx, rot);
        copy_matrix(globaltx,temp);

        make_z_rot_matrix(z, rot);
        mul_matrix(temp, globaltx, rot);
        copy_matrix(globaltx,temp);
    }

    void scale(GLfloat x, GLfloat y) {
        GLfloat scale[16];
        GLfloat temp[16];

        make_scale_matrix(x, y, 1.0, scale);
        mul_matrix(temp, globaltx, scale);
        copy_matrix(globaltx, temp);
    }

    GLfloat applyOpacity(GLfloat opacity) {
        this->opacity *= opacity;

        return this->opacity;
    }

    void saveOpacity() {
        GLfloat *temp = new GLfloat[1];

        temp[0] = opacity;
        matrixStack.push(temp);
    }

    void restoreOpacity() {
        GLfloat *temp = (GLfloat *)matrixStack.top();

        matrixStack.pop();

        opacity = temp[0];
        delete[] temp;
    }

    void save() {
        //matrix
        GLfloat *temp = new GLfloat[16];

        copy_matrix(temp, globaltx);
        matrixStack.push(globaltx);
        globaltx = temp;
    }

    void restore() {
        //matrix
        delete[] globaltx;
        globaltx = (GLfloat *)matrixStack.top();
        matrixStack.pop();

        assert(globaltx);
    }

    void useShader(AnyAminoShader *shader) {
        if (shader != prevShader) {
            assert(shader);

            shader->useShader();

            prevShader = shader;
        }
    }

    void bindTexture(GLuint tex) {
        if (prevTex != tex) {
            glBindTexture(GL_TEXTURE_2D, tex);

            prevTex = tex;
        }
    }

    void enableDepth() {
        depth++;

        if (depth == 1) {
            glDepthMask(GL_TRUE);

            //debug
            //printf("using depth mask\n");
        }
    }

    void disableDepth() {
        depth--;

        if (depth == 0) {
            glDepthMask(GL_FALSE);
        }
    }
};

class SimpleRenderer {
public:
    SimpleRenderer(AminoFontShader *fontShader, ColorShader *colorShader, TextureShader *textureShader, GLfloat *modelView);
    virtual ~SimpleRenderer() { }

    virtual void startRender(AminoNode *node);
    virtual void render(GLContext *ctx, AminoNode *node);

    virtual void drawGroup(GLContext *ctx, AminoGroup *group);
    virtual void drawRect(GLContext *ctx, AminoRect *rect);
    virtual void drawPoly(GLContext *ctx, AminoPolygon *poly);
    virtual void drawText(GLContext *ctx, AminoText *text);

    static int showGLErrors();
    static int showGLErrors(std::string msg);

private:
    AminoFontShader *fontShader;
    ColorShader *colorShader;
    TextureShader *textureShader;
    GLfloat *modelView;

    void applyColorShader(GLContext *ctx, GLfloat *verts, GLsizei dim, GLsizei count, GLfloat color[4], GLenum mode = GL_TRIANGLES);
    void applyTextureShader(GLContext *ctx, GLfloat *verts, GLsizei dim, GLsizei count, GLfloat texcoords[][2], GLuint texId, GLfloat opacity);
};

#endif
