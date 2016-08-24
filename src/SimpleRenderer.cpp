#include "SimpleRenderer.h"

//cbx
#define DEBUG_RENDERER true
#define DEBUG_RENDERER_ERRORS false

SimpleRenderer::SimpleRenderer(AminoFontShader *fontShader, ColorShader *colorShader, TextureShader *textureShader, GLfloat *modelView): fontShader(fontShader), colorShader(colorShader), textureShader(textureShader), modelView(modelView) {
    if (DEBUG_RENDERER) {
        printf("created SimpleRenderer\n");
    }
}

void SimpleRenderer::startRender(AminoNode *root) {
    if (DEBUG_RENDERER) {
        printf("-> startRender()\n");
    }

    GLContext *c = new GLContext();

    this->render(c, root);
    delete c;

    if (DEBUG_RENDERER) {
        printf("-> startRender() done\n");
    }
}

/**
 * Render a node.
 */
void SimpleRenderer::render(GLContext *ctx, AminoNode *root) {
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
            this->drawGroup(ctx, (AminoGroup *)root);
            break;

        case RECT:
            this->drawRect(ctx, (AminoRect *)root);
            break;

        case POLY:
            this->drawPoly(ctx, (AminoPolygon *)root);
            break;

        case TEXT:
            this->drawText(ctx, (AminoText *)root);
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
void SimpleRenderer::applyColorShader(GLContext *ctx, GLfloat *verts, GLsizei dim, GLsizei count, GLfloat color[4], GLenum mode) {
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
void SimpleRenderer::applyTextureShader(GLContext *ctx, GLfloat *verts, GLsizei dim, GLsizei count, GLfloat texcoords[][2], GLuint texId, GLfloat opacity) {
    //printf("doing texture shader apply %d opacity = %f\n", texId, opacity);

    //use shader
    ctx->useShader(textureShader);

    //blend
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //shader values
    textureShader->setTransformation(modelView, ctx->globaltx);
    textureShader->setOpacity(opacity);

    //draw
    ctx->bindTexture(texId);
    textureShader->drawTexture(verts, dim, texcoords, count);

    //cleanup
    glDisable(GL_BLEND);
}

void SimpleRenderer::drawGroup(GLContext *ctx, AminoGroup *group) {
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

        applyColorShader(ctx, (float *)verts, 2, 6, color);

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
        this->render(ctx, group->children[i]);
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
void SimpleRenderer::drawPoly(GLContext *ctx, AminoPolygon *poly) {
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
printf("applyColorShader %i %i\n", dim, len); //FIXME cbx
    applyColorShader(ctx, verts, dim, len / dim, color, mode);
}

void SimpleRenderer::drawRect(GLContext *ctx, AminoRect *rect) {
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

            applyTextureShader(ctx, (float *)verts, 2, 6, texCoords, texture->textureId, opacity);
        }
    } else {
        //color only
        GLfloat color[4] = { rect->propR->value, rect->propG->value, rect->propB->value, opacity };

        applyColorShader(ctx, (float *)verts, 2, 6, color);
    }

    ctx->restore();
}

/**
 * Render text.
 */
void SimpleRenderer::drawText(GLContext *ctx, AminoText *text) {
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
 * Output all occured OpenGL errors.
 */
int SimpleRenderer::showGLErrors() {
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
int SimpleRenderer::showGLErrors(std::string msg) {
    int res = showGLErrors();

    if (res) {
        printf("%i OpenGL errors at '%s'\n'", res, msg.c_str());
    }

    return res;
}
