#ifndef _SIMPLE_RENDERER_H
#define _SIMPLE_RENDERER_H

#include "base.h"

#include "mathutils.h"

#include <stack>

/**
 * Rendering context.
 */
class GLContext {
public:
    std::stack<void *> matrixStack;
    GLfloat *globaltx = new GLfloat[16];
    GLfloat opacity = 1;

    int depth = 0;

    AnyAminoShader *prevShader = NULL;
    GLuint prevTex = INVALID_TEXTURE;

    /**
     * Constructor.
     */
    GLContext() {
        //matrix
        make_identity_matrix(globaltx);
    }

    /**
     * Destructor.
     */
    virtual ~GLContext() {
        assert(matrixStack.empty());
        delete[] globaltx;
    }

    /**
     * Reset context (prepare for next cycle).
     */
    void reset() {
        assert(matrixStack.empty());
        assert(depth == 0);

        //reset
        if (prevShader) {
            prevShader = NULL;

            //Note: not needed -> reset on unbind
            //glUseProgram(0);
        }

        if (prevTex != INVALID_TEXTURE) {
            prevTex = INVALID_TEXTURE;

            //Note: not needed -> reset on unbind
            //glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    /**
     * Display current matrix.
     */
    void dumpGlobalTransform() {
        print_matrix(globaltx);
    }

    /**
     * Translate x/y.
     */
    void translate(GLfloat x, GLfloat y) {
        translate(x, y, 0);
    }

    /**
     * Translate x/y/z.
     */
    void translate(GLfloat x, GLfloat y, GLfloat z) {
        GLfloat tr[16];
        GLfloat trans2[16];

        make_trans_matrix(x, y, z, tr);
        mul_matrix(trans2, globaltx, tr);
        copy_matrix(globaltx, trans2);
    }

    /**
     * Rotate x/y/z. Angles in degrees.
     *
     * Order: 1) x, 2) y, 3) z
     */
    void rotate(GLfloat x, GLfloat y, GLfloat z) {
        GLfloat rot[16];
        GLfloat temp[16];

        //x-rotation
        make_x_rot_matrix(x, rot);
        mul_matrix(temp, globaltx, rot);
        copy_matrix(globaltx, temp);

        //y-rotation
        make_y_rot_matrix(y, rot);
        mul_matrix(temp, globaltx, rot);
        copy_matrix(globaltx, temp);

        //z-rotation
        make_z_rot_matrix(z, rot);
        mul_matrix(temp, globaltx, rot);
        copy_matrix(globaltx, temp);
    }

    /**
     * Scale in x und y directions.
     */
    void scale(GLfloat x, GLfloat y) {
        GLfloat scale[16];
        GLfloat temp[16];

        make_scale_matrix(x, y, 1.0, scale);
        mul_matrix(temp, globaltx, scale);
        copy_matrix(globaltx, temp);
    }

    /**
     * Set opacity value.
     */
    GLfloat applyOpacity(GLfloat opacity) {
        this->opacity *= opacity;

        return this->opacity;
    }

    /**
     * Save opacity.
     */
    void saveOpacity() {
        GLfloat *temp = new GLfloat[1];

        temp[0] = opacity;
        matrixStack.push(temp);
    }

    /**
     * Restore the opacity.
     */
    void restoreOpacity() {
        GLfloat *temp = (GLfloat *)matrixStack.top();

        matrixStack.pop();

        opacity = temp[0];
        delete[] temp;
    }

    /**
     * Save matrix.
     */
    void save() {
        //matrix
        GLfloat *temp = new GLfloat[16];

        copy_matrix(temp, globaltx);
        matrixStack.push(globaltx);
        globaltx = temp;
    }

    /**
     * Restore matrix.
     */
    void restore() {
        //matrix
        delete[] globaltx;
        globaltx = (GLfloat *)matrixStack.top();
        matrixStack.pop();

        assert(globaltx);
    }

    /**
     * Use a shader.
     */
    void useShader(AnyAminoShader *shader) {
        if (shader != prevShader) {
            //shader changed
            assert(shader);

            shader->useShader(false);

            prevShader = shader;
        } else {
            //same shader
            assert(shader);

            shader->useShader(true);
        }
    }

    /**
     * Use texture.
     */
    void bindTexture(GLuint tex) {
        if (prevTex != tex) {
            glBindTexture(GL_TEXTURE_2D, tex);

            prevTex = tex;
        }
    }

    /**
     * Reset texture (use with care).
     */
    void unbindTexture() {
        bindTexture(0);
    }

    /**
     * Check if depth buffer is active.
     */
    bool hasDepth() {
        return depth > 0;
    }

    /**
     * Enabled 3D rendering using depth buffer.
     */
    void enableDepth() {
        depth++;

        if (depth == 1) {
            glDepthMask(GL_TRUE);

            /*
             * Transparent texture support:
             *
             *   - using GLSL discard, code below not available on OpenGL ES 2.0!
             */
            /*
            glAlphaFunc(GL_GREATER, 0.);
            glEnable(GL_ALPHA_TEST);
            */

            //debug
            //printf("using depth mask\n");
        }
    }

    /**
     * Disable 3D rendering.
     */
    void disableDepth() {
        depth--;

        if (depth == 0) {
            //clear depth buffer
            glClear(GL_DEPTH_BUFFER_BIT);

            //disable mask usage
            glDepthMask(GL_FALSE);
        }
    }
};

/**
 * OpenGL ES 2.0 renderer.
 */
class AminoRenderer {
public:
    AminoRenderer(AminoGfx *gfx);
    virtual ~AminoRenderer();

    virtual void setup();
    virtual void setupPerspective(v8::Local<v8::Object> &perspective);

    virtual void updateViewport(GLfloat width, GLfloat height, GLfloat viewportW, GLfloat viewportH);
    virtual void initScene(GLfloat r, GLfloat g, GLfloat b, GLfloat opacity);
    virtual void renderScene(AminoNode *node);

    amino_atlas_t getAtlasTexture(texture_atlas_t *atlas, bool createIfMissing, bool &newTexture);

    static int showGLErrors();
    static int showGLErrors(std::string msg);

    static void checkTexturePerformance();

protected:
    virtual void render(AminoNode *node);

    virtual void drawGroup(AminoGroup *group);
    virtual void drawRect(AminoRect *rect);
    virtual void drawPoly(AminoPolygon *poly);
    virtual void drawModel(AminoModel *model);
    virtual void drawText(AminoText *text);

private:
    AminoGfx *gfx;

    //basic shaders
    AminoFontShader *fontShader = NULL;
    ColorShader *colorShader = NULL;
    TextureShader *textureShader = NULL;
    TextureClampToBorderShader *textureClampToBorderShader = NULL;

    //model shaders
    ColorLightingShader *colorLightingShader = NULL;
    TextureLightingShader *textureLightingShader = NULL;

    //perspective
    bool orthographic = true;
    float near = 150;
    float far = -2048; //dynamic: -2048/-300
    float eye = 600;
    GLfloat vanishingPoint[2] = { .5f, .5f };

    GLfloat corrSrc[8] = { 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f };
    GLfloat corrDst[8] = { 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f };
    bool corrUsed = false;

    //matrix
    GLfloat modelView[16];
    GLContext *ctx = NULL;

    void applyColorShader(GLfloat *verts, GLsizei dim, GLsizei count, GLfloat color[4], GLenum mode = GL_TRIANGLES);
    void applyTextureShader(GLfloat *verts, GLsizei dim, GLsizei count, GLfloat uv[][2], GLuint texId, GLfloat opacity, bool needsClampToBorder, bool repeatX, bool repeatY);
};

#endif
