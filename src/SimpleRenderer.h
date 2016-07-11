#ifndef _SIMPLE_RENDERER_H
#define _SIMPLE_RENDERER_H

#include "base.h"

class GLContext {
public:
    std::stack<void *> matrixStack;
    GLfloat *globaltx;
    GLfloat opacity;
    int shadercount;
    int shaderDupCount;
    int texDupCount;
    int prevProg;
    int prevtex;

    GLContext() {
        shadercount = 0;
        shaderDupCount = 0;
        texDupCount = 0;
        prevProg = -1;
        prevtex = -1;

        opacity = 1;

        //matrix
        this->globaltx = new GLfloat[16];
        make_identity_matrix(this->globaltx);
    }

    virtual ~GLContext() {
        delete this->globaltx;
    }

    void dumpGlobalTransform() {
        print_matrix(this->globaltx);
    }

    void translate(double x, double y) {
        GLfloat tr[16];
        GLfloat trans2[16];

        make_trans_matrix((float)x, (float)y, 0, tr);
        mul_matrix(trans2, this->globaltx, tr);
        copy_matrix(this->globaltx, trans2);
    }

    void rotate(double x, double y, double z) {
        GLfloat rot[16];
        GLfloat temp[16];

        make_x_rot_matrix(x, rot);
        mul_matrix(temp, this->globaltx, rot);
        copy_matrix(this->globaltx,temp);

        make_y_rot_matrix(y, rot);
        mul_matrix(temp, this->globaltx, rot);
        copy_matrix(this->globaltx,temp);

        make_z_rot_matrix(z, rot);
        mul_matrix(temp, this->globaltx, rot);
        copy_matrix(this->globaltx,temp);
    }

    void scale(double x, double y) {
        GLfloat scale[16];
        GLfloat temp[16];

        make_scale_matrix((float)x, (float)y, 1.0, scale);
        mul_matrix(temp, this->globaltx, scale);
        copy_matrix(this->globaltx, temp);
    }

    GLfloat applyOpacity(GLfloat opacity) {
        this->opacity *= opacity;

        return this->opacity;
    }

    void saveOpacity() {
        GLfloat *temp = new GLfloat[1];

        temp[0] = opacity;
        this->matrixStack.push(temp);
    }

    void restoreOpacity() {
        GLfloat *temp = (GLfloat *)this->matrixStack.top();

        this->matrixStack.pop();

        opacity = temp[0];
        delete temp;
    }

    void save() {
        //matrix
        GLfloat *temp = new GLfloat[16];

        copy_matrix(temp, this->globaltx);
        this->matrixStack.push(this->globaltx);
        this->globaltx = temp;
    }

    void restore() {
        //matrix
        delete this->globaltx;
        this->globaltx = (GLfloat *)this->matrixStack.top();
        this->matrixStack.pop();
    }

    void useProgram(int prog) {
//        if(prog == prevProg) {
//            shaderDupCount++;
//        } else {
            glUseProgram(prog);
//        }
        prevProg = prog;
        shadercount++;
    }

    void bindTexture(int tex) {
//        if(prevtex == tex) {
//            texDupCount++;
//        } else {
            glBindTexture( GL_TEXTURE_2D, tex);
//        }
        prevtex = tex;
    }

};

class SimpleRenderer {
public:
    SimpleRenderer();
    virtual ~SimpleRenderer() { }

    virtual void startRender(AminoNode *node);
    virtual void render(GLContext *c, AminoNode *node);
    virtual void drawGroup(GLContext *c, Group *group);
    virtual void drawRect(GLContext *c, Rect *rect);
    virtual void drawPoly(GLContext *c, PolyNode *poly);
    virtual void drawText(GLContext *c, TextNode *text);
    virtual void drawGLNode(GLContext *c, GLNode *glnode);
};


#endif
