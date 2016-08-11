#ifndef _SIMPLE_RENDERER_H
#define _SIMPLE_RENDERER_H

#include "base.h"

class GLContext {
public:
    std::stack<void *> matrixStack;
    GLfloat *globaltx;
    GLfloat opacity;
    GLuint prevProg;
    GLuint prevTex;

    GLContext() {
        prevProg = INVALID_PROGRAM;
        prevTex = INVALID_TEXTURE;

        opacity = 1;

        //matrix
        globaltx = new GLfloat[16];
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
        GLfloat tr[16];
        GLfloat trans2[16];

        make_trans_matrix(x, y, 0, tr);
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

    void useProgram(GLuint prog) {
        if (prog != prevProg) {
            glUseProgram(prog);

            prevProg = prog;
        }
    }

    void bindTexture(GLuint tex) {
        if (prevTex != tex) {
            glBindTexture(GL_TEXTURE_2D, tex);

            prevTex = tex;
        }
    }
};

class SimpleRenderer {
public:
    SimpleRenderer(AminoFontShader *fontShader, ColorShader *colorShader, TextureShader *textureShader, GLfloat *modelView);
    virtual ~SimpleRenderer() { }

    virtual void startRender(AminoNode *node);
    virtual void render(GLContext *c, AminoNode *node);

    virtual void drawGroup(GLContext *c, AminoGroup *group);
    virtual void drawRect(GLContext *c, AminoRect *rect);
    virtual void drawPoly(GLContext *c, AminoPolygon *poly);
    virtual void drawText(GLContext *c, AminoText *text);

    static int showGLErrors();

private:
    AminoFontShader *fontShader;
    ColorShader *colorShader;
    TextureShader *textureShader;
    GLfloat *modelView;
};


#endif
