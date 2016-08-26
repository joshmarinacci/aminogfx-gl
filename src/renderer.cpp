#include "renderer.h"

#define DEBUG_RENDERER false
#define DEBUG_RENDERER_ERRORS false

/**
 * OpenGL ES 2.0 renderer.
 */
AminoRenderer::AminoRenderer() {
    if (DEBUG_RENDERER) {
        printf("created AminoRenderer\n");
    }
}

AminoRenderer::~AminoRenderer () {
    //renderer (shader programs)

    //color shader
    if (colorShader) {
        colorShader->destroy();
        delete colorShader;
        colorShader = NULL;
    }

    //texture shader
    if (textureShader) {
        textureShader->destroy();
        delete textureShader;
        textureShader = NULL;
    }

    //texture clamp to border shader
    if (textureClampToBorderShader) {
        textureClampToBorderShader->destroy();
        delete textureClampToBorderShader;
        textureClampToBorderShader = NULL;
    }

    //font shader
    if (fontShader) {
        fontShader->destroy();
        delete fontShader;
        fontShader = NULL;
    }

    //context
    if (ctx) {
        delete ctx;
        ctx = NULL;
    }
 }

/**
 * Setup renderer.
 */
void AminoRenderer::setup() {
    if (DEBUG_RENDERER) {
        printf("-> setup()\n");
    }

    //set hints
    glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

    //color shader
	colorShader = new ColorShader();

    bool res = colorShader->create();

    assert(res);

    //texture shader
	textureShader = new TextureShader();
    res = textureShader->create();

    assert(res);

    //font shader
    fontShader = new AminoFontShader();
    res = fontShader->create();

    assert(res);

    //context
    ctx = new GLContext();
}

/**
 * Update the model view projection matrix.
 */
void AminoRenderer::updateViewport(GLfloat width, GLfloat height, GLfloat viewportW, GLfloat viewportH) {
    //set up the viewport (y-inversion, top-left origin)

    //scale
    GLfloat *scaleM = new GLfloat[16];

    make_scale_matrix(1, -1, 1, scaleM);

    //translate
    GLfloat *transM = new GLfloat[16];

    make_trans_matrix(- width / 2, height / 2, 0, transM);

    //combine
    GLfloat *m4 = new GLfloat[16];

    mul_matrix(m4, transM, scaleM);

    //3D perspective
    GLfloat *pixelM = new GLfloat[16];
    const float near = 150;
    const float far = -300;
    const float eye = 600;

    loadPixelPerfectMatrix(pixelM, width, height, eye, near, far);
    mul_matrix(modelView, pixelM, m4);

    delete[] m4;
    delete[] pixelM;
    delete[] scaleM;
    delete[] transM;

    glViewport(0, 0, viewportW, viewportH);
}

/**
 * Init the scene.
 */
void AminoRenderer::initScene(GLfloat r, GLfloat g, GLfloat b, GLfloat opacity) {
    //enable depth mask
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    //prepare
    glClearColor(r, g, b, opacity);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //disable depth mask (use painter's algorithm by default)
    glDepthMask(GL_FALSE);
}

/**
 * Render a complete scene.
 */
void AminoRenderer::renderScene(AminoNode *node) {
    if (DEBUG_RENDERER) {
        printf("-> renderScene()\n");
    }

    render(node);

    ctx->reset();
}

/**
 * Render a node.
 */
void AminoRenderer::render(AminoNode *root) {
    if (DEBUG_RENDERER) {
        printf("-> render()\n");
    }

    if (root == NULL) {
        printf("WARNING. NULL NODE!\n");
        return;
    }

    //skip non-visible nodes
    if (!root->propVisible->value) {
        return;
    }

    ctx->save();

    //transform
    if (root->propW) {
        //apply origin
        ctx->translate(root->propW->value* root->propOriginX->value, root->propH->value * root->propOriginY->value);
    }

    ctx->translate(root->propX->value, root->propY->value, root->propZ->value);
    ctx->scale(root->propScaleX->value, root->propScaleY->value);
    ctx->rotate(root->propRotateX->value, root->propRotateY->value, root->propRotateZ->value);

    if (root->propW) {
        //apply origin
        ctx->translate(- (root->propW->value* root->propOriginX->value), - (root->propH->value * root->propOriginY->value));
    }

    //draw
    switch (root->type) {
        case GROUP:
            this->drawGroup((AminoGroup *)root);
            break;

        case RECT:
            this->drawRect((AminoRect *)root);
            break;

        case POLY:
            this->drawPoly((AminoPolygon *)root);
            break;

        case TEXT:
            this->drawText((AminoText *)root);
            break;

        default:
            printf("invalid node type: %i\n", root->type);
            break;
    }

    //debug
    if (DEBUG_RENDERER_ERRORS) {
        showGLErrors();
    }

    //done
    ctx->restore();
}

/**
 * Use solid color shader.
 */
void AminoRenderer::applyColorShader(GLfloat *verts, GLsizei dim, GLsizei count, GLfloat color[4], GLenum mode) {
    //use shader
    ctx->useShader(colorShader);

    colorShader->setTransformation(modelView, ctx->globaltx);
    colorShader->setColor(color);

    if (color[3] != 1.0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //draw
    colorShader->drawTriangles(verts, dim, count, mode);

    //cleanup
    glDisable(GL_BLEND);
}

/**
 * Draw texture.
 */
void AminoRenderer::applyTextureShader(GLfloat *verts, GLsizei dim, GLsizei count, GLfloat texcoords[][2], GLuint texId, GLfloat opacity, bool needsClampToBorder) {
    //printf("doing texture shader apply %d opacity = %f\n", texId, opacity);

    //use shader
    TextureShader *shader;

    if (needsClampToBorder) {
        if (!textureClampToBorderShader) {
            textureClampToBorderShader = new TextureClampToBorderShader();

            bool res = textureClampToBorderShader->create();

            assert(res);
        }

        shader = textureClampToBorderShader;

        //debug
        //printf("using clamp to border shader\n");
    } else {
        shader = textureShader;
    }

    ctx->useShader(shader);

    //blend
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //shader values
    shader->setTransformation(modelView, ctx->globaltx);
    shader->setOpacity(opacity);

    //draw
    ctx->bindTexture(texId);
    shader->drawTexture(verts, dim, texcoords, count);

    //cleanup
    glDisable(GL_BLEND);
}

/**
 * Draw group.
 */
void AminoRenderer::drawGroup(AminoGroup *group) {
    if (DEBUG_RENDERER) {
        printf("-> drawGroup()\n");
    }

    bool useDepth = group->propDepth->value;

    if (useDepth) {
        //enable depth mask
        ctx->enableDepth();
    }

    bool useClipping = group->propCliprect->value;

    if (useClipping) {
        //turn on stenciling
        glEnable(GL_STENCIL_TEST);

        //setup the stencil
        glStencilFunc(GL_ALWAYS, 0x1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        //clear the buffers
        glClear(GL_STENCIL_BUFFER_BIT);

        //draw the stencil
        float x = 0;
        float y = 0;
        float x2 = group->propW->value;
        float y2 = group->propH->value;
        GLfloat verts[6][2];

        verts[0][0] = x;
        verts[0][1] = y;
        verts[1][0] = x2;
        verts[1][1] = y;
        verts[2][0] = x2;
        verts[2][1] = y2;

        verts[3][0] = x2;
        verts[3][1] = y2;
        verts[4][0] = x;
        verts[4][1] = y2;
        verts[5][0] = x;
        verts[5][1] = y;

        GLfloat color[4] = { 1.0, 1.0, 1.0, 1.0 };

        applyColorShader((float *)verts, 2, 6, color);

        //set function to draw pixels where the buffer is equal to 1
        glStencilFunc(GL_EQUAL, 0x1, 0xFF);
        glStencilMask(0x00);

        //turn color buffer drawing back on
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    //group opacity
    ctx->saveOpacity();
    ctx->applyOpacity(group->propOpacity->value);

    //render items
    std::size_t count = group->children.size();

    for (std::size_t i = 0; i < count; i++) {
        this->render(group->children[i]);
    }

    //restore opacity
    ctx->restoreOpacity();

    if (useClipping) {
        glDisable(GL_STENCIL_TEST);
    }

    if (useDepth) {
        //disable depth mask again
        ctx->disableDepth();
    }
}

/**
 * Draw a polygon.
 */
void AminoRenderer::drawPoly(AminoPolygon *poly) {
    if (DEBUG_RENDERER) {
        printf("-> drawPoly()\n");
    }

    //vertices
    std::vector<float> *geometry = &poly->propGeometry->value;
    int len = geometry->size();
    int dim = poly->propDimension->value;
    GLfloat *verts = geometry->data();

    assert(dim == 2 || dim == 3);

    //draw
    GLenum mode;

    if (poly->propFilled->value) {
        //filled polygon
        mode = GL_TRIANGLE_FAN;
    } else {
        //draw outline (glLineWidth() not used yet)
        mode = GL_LINE_LOOP;
    }

    GLfloat opacity = poly->propOpacity->value * ctx->opacity;
    GLfloat color[4] = { poly->propFillR->value, poly->propFillG->value, poly->propFillB->value, opacity };

    applyColorShader(verts, dim, len / dim, color, mode);
}

/**
 * Draw rect.
 */
void AminoRenderer::drawRect(AminoRect *rect) {
    if (DEBUG_RENDERER) {
        printf("-> drawRect() hasImage=%s\n", rect->hasImage ? "true":"false");
    }

    ctx->save();

    //two triangles
    float x =  0;
    float y =  0;
    float x2 = rect->propW->value;
    float y2 = rect->propH->value;

    GLfloat verts[6][2];

    verts[0][0] = x;
    verts[0][1] = y;
    verts[1][0] = x2;
    verts[1][1] = y;
    verts[2][0] = x2;
    verts[2][1] = y2;

    verts[3][0] = x2;
    verts[3][1] = y2;
    verts[4][0] = x;
    verts[4][1] = y2;
    verts[5][0] = x;
    verts[5][1] = y;

    GLfloat opacity = rect->propOpacity->value * ctx->opacity;

    if (rect->hasImage) {
        //has optional texture
        AminoTexture *texture = (AminoTexture *)rect->propTexture->value;

        if (texture && texture->textureId != INVALID_TEXTURE) {
            //texture

            //debug
            //printf("texture: %i\n", texture->textureId);

            //image coordinates (fractional world coordinates)
            GLfloat texCoords[6][2];
            float tx  = rect->propLeft->value;   //0
            float ty2 = rect->propBottom->value; //1
            float tx2 = rect->propRight->value;  //1
            float ty  = rect->propTop->value;    //0

            texCoords[0][0] = tx;    texCoords[0][1] = ty;
            texCoords[1][0] = tx2;   texCoords[1][1] = ty;
            texCoords[2][0] = tx2;   texCoords[2][1] = ty2;

            texCoords[3][0] = tx2;   texCoords[3][1] = ty2;
            texCoords[4][0] = tx;    texCoords[4][1] = ty2;
            texCoords[5][0] = tx;    texCoords[5][1] = ty;

            //check clamp to border
            bool needsClampToBorder = (tx < 0 || tx > 1) || (tx2 < 0 || tx2 > 1) || (ty < 0 || ty > 1) || (ty2 < 0 || ty2 > 1);

            applyTextureShader((float *)verts, 2, 6, texCoords, texture->textureId, opacity, needsClampToBorder);
        }
    } else {
        //color only
        GLfloat color[4] = { rect->propR->value, rect->propG->value, rect->propB->value, opacity };

        applyColorShader((float *)verts, 2, 6, color);
    }

    ctx->restore();
}

/**
 * Render text.
 */
void AminoRenderer::drawText(AminoText *text) {
    if (DEBUG_RENDERER) {
        printf("-> drawText()\n");
    }

    if (!text->layoutText()) {
        return;
    }

    ctx->save();

    //flip the y axis
    ctx->scale(1, -1);

    //baseline at top/left
    texture_font_t *tf = text->fontSize->fontTexture;

    //debug
    //sprintf("font: size=%f height=%f ascender=%f descender=%f\n", tf->size, tf->height, tf->ascender, tf->descender);

    switch (text->vAlign) {
        case AminoText::VALIGN_TOP:
            ctx->translate(0, -tf->ascender);
            break;

        case AminoText::VALIGN_BOTTOM:
            ctx->translate(0,  - text->propH->value - tf->descender + (text->lineNr - 1) * tf->height);
            break;

        case AminoText::VALIGN_MIDDLE:
            ctx->translate(0, - tf->ascender - (text->propH->value - text->lineNr * tf->height) / 2);
            break;

        case AminoText::VALIGN_BASELINE:
        default:
            break;
    }

    //use texture
    GLuint texture = text->updateTexture();

    if (DEBUG_RENDERER_ERRORS) {
        showGLErrors("updateTexture()");
    }

    glActiveTexture(GL_TEXTURE0);
    ctx->bindTexture(texture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //font shader
    ctx->useShader(fontShader);

    //color & opacity
    fontShader->setTransformation(modelView, ctx->globaltx);
    fontShader->setOpacity(ctx->opacity * text->propOpacity->value);

    GLfloat color[3] = { text->propR->value, text->propG->value, text->propB->value };

    fontShader->setColor(color);

    if (DEBUG_RENDERER_ERRORS) {
        showGLErrors("before text rendering");
    }

    //render
    vertex_buffer_render(text->buffer, GL_TRIANGLES);

    if (DEBUG_RENDERER_ERRORS) {
        showGLErrors("after text rendering");
    }

    //cleanup
    glDisable(GL_BLEND);

    ctx->restore();
}

/**
 * Get texture for atlas.
 *
 * Note: has to be called on OpenGL thread.
 */
amino_atlas_t AminoRenderer::getAtlasTexture(texture_atlas_t *atlas) {
    assert(fontShader);

    return fontShader->getAtlasTexture(atlas);
}

/**
 * Output all occured OpenGL errors.
 */
int AminoRenderer::showGLErrors() {
    GLenum err = GL_NO_ERROR;
    int count = 0;

    while ((err = glGetError()) != GL_NO_ERROR) {
        count++;
        printf("OpenGL error: %08x\n", err);
    }

    return count;
}

/**
 * Output all occured OpenGL errors.
 */
int AminoRenderer::showGLErrors(std::string msg) {
    int res = showGLErrors();

    if (res) {
        printf("%i OpenGL errors at '%s'\n'", res, msg.c_str());
    }

    return res;
}
