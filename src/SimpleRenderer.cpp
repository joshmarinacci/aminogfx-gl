#include "SimpleRenderer.h"

#define DEBUG_RENDERER false
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
void SimpleRenderer::render(GLContext *c, AminoNode *root) {
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

    c->save();

    //transform
    if (root->propW) {
        //apply origin
        c->translate(root->propW->value* root->propOriginX->value, root->propH->value * root->propOriginY->value);
    }

    c->translate(root->propX->value, root->propY->value, root->propZ->value);
    c->scale(root->propScaleX->value, root->propScaleY->value);
    c->rotate(root->propRotateX->value, root->propRotateY->value, root->propRotateZ->value);

    if (root->propW) {
        //apply origin
        c->translate(- (root->propW->value* root->propOriginX->value), - (root->propH->value * root->propOriginY->value));
    }

    //draw
    switch (root->type) {
        case GROUP:
            this->drawGroup(c, (AminoGroup *)root);
            break;

        case RECT:
            this->drawRect(c, (AminoRect *)root);
            break;

        case POLY:
            this->drawPoly(c, (AminoPolygon *)root);
            break;

        case TEXT:
            this->drawText(c, (AminoText *)root);
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
    c->restore();
}

/**
 * Use solid color shader.
 */
void applyColorShader(GLContext *ctx, ColorShader* shader, GLfloat modelView[16], GLfloat verts[][2], GLfloat color[4]) {
    ctx->useProgram(shader->prog);

    //set uniforms
    glUniformMatrix4fv(shader->u_matrix, 1, GL_FALSE, modelView);
    glUniformMatrix4fv(shader->u_trans,  1, GL_FALSE, ctx->globaltx);
    glUniform4f(shader->u_color, color[0], color[1], color[2], color[3]);

    if (color[3] != 1.0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //set attributes (vertex pos & color)
    glVertexAttribPointer(shader->attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(shader->attr_pos);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(shader->attr_pos);
    glDisable(GL_BLEND);
}

/**
 * Draw texture.
 */
void applyTextureShader(GLContext *ctx, TextureShader *shader, GLfloat modelView[16], GLfloat verts[][2], GLfloat texcoords[][2], GLuint texId, GLfloat opacity) {
    //printf("doing texture shader apply %d opacity = %f\n", texId, opacity);

    ctx->useProgram(shader->prog);

    //blend
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //shader values
    glUniformMatrix4fv(shader->u_matrix, 1, GL_FALSE, modelView);
    glUniformMatrix4fv(shader->u_trans,  1, GL_FALSE, ctx->globaltx);
    glUniform1f(shader->u_opacity, opacity);

    glVertexAttribPointer(shader->attr_texcoords, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
    glEnableVertexAttribArray(shader->attr_texcoords);

    glVertexAttribPointer(shader->attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(shader->attr_pos);
    glActiveTexture(GL_TEXTURE0);

    //render
    ctx->bindTexture(texId);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(shader->attr_pos);
    glDisableVertexAttribArray(shader->attr_texcoords);
    glDisable(GL_BLEND);
}

void SimpleRenderer::drawGroup(GLContext *c, AminoGroup *group) {
    if (DEBUG_RENDERER) {
        printf("-> drawGroup()\n");
    }

    if (group->propDepth->value) {
        //enable depth mask
        c->enableDepth();
    }

    if (group->propCliprect->value) {
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

        applyColorShader(c, colorShader, modelView, verts, color);

        //set function to draw pixels where the buffer is equal to 1
        glStencilFunc(GL_EQUAL, 0x1, 0xFF);
        glStencilMask(0x00);

        //turn color buffer drawing back on
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    //group opacity
    c->saveOpacity();
    c->applyOpacity(group->propOpacity->value);

    //render items
    std::size_t count = group->children.size();

    for (std::size_t i = 0; i < count; i++) {
        this->render(c, group->children[i]);
    }

    //restore opacity
    c->restoreOpacity();

    if (group->propCliprect->value) {
        glDisable(GL_STENCIL_TEST);
    }

    if (group->propDepth->value) {
        //disable depth mask again
        c->disableDepth();
    }
}

/**
 * Draw a polygon.
 */
void SimpleRenderer::drawPoly(GLContext *ctx, AminoPolygon *poly) {
    if (DEBUG_RENDERER) {
        printf("-> drawPoly()\n");
    }

    //setup shader
    ctx->useProgram(colorShader->prog);

    glUniformMatrix4fv(colorShader->u_matrix, 1, GL_FALSE, modelView);
    glUniformMatrix4fv(colorShader->u_trans,  1, GL_FALSE, ctx->globaltx);

    //color
    GLfloat opacity = poly->propOpacity->value * ctx->opacity;

    glUniform4f(colorShader->u_color, poly->propFillR->value, poly->propFillG->value, poly->propFillB->value, opacity);

    if (opacity != 1.0) {
        //blend mode needed
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //vertices
    std::vector<float> *geometry = &poly->propGeometry->value;
    int len = geometry->size();
    int dim = poly->propDimension->value;
    GLfloat verts[len][dim];

    for (int i = 0; i < len / dim; i++) {
        verts[i][0] = geometry->at(i * dim);

        if (dim >=2) {
            verts[i][1] = geometry->at(i * dim + 1);
        }
        if (dim >=3) {
            verts[i][2] = geometry->at(i * dim + 2);
        }
    }

    assert(dim == 2 || dim == 3);

    glVertexAttribPointer(colorShader->attr_pos, dim, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(colorShader->attr_pos);

    if (poly->propFilled->value) {
        //filled polygon
        glDrawArrays(GL_TRIANGLE_FAN, 0, len / dim);
    } else {
        //draw outline (glLineWidth() not used yet)
        glDrawArrays(GL_LINE_LOOP, 0, len / dim);
    }

    glDisableVertexAttribArray(colorShader->attr_pos);

    if (opacity != 1.0) {
        glDisable(GL_BLEND);
    }
}

void SimpleRenderer::drawRect(GLContext *c, AminoRect *rect) {
    if (DEBUG_RENDERER) {
        printf("-> drawRect() hasImage=%s\n", rect->hasImage ? "true":"false");
    }

    c->save();

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

    GLfloat opacity = rect->propOpacity->value * c->opacity;

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

            applyTextureShader(c, textureShader, modelView, verts, texCoords, texture->textureId, opacity);
        }
    } else {
        //color only
        GLfloat color[4] = { rect->propR->value, rect->propG->value, rect->propB->value, opacity };

        applyColorShader(c, colorShader, modelView, verts, color);
    }

    c->restore();
}

/**
 * Render text.
 */
void SimpleRenderer::drawText(GLContext *c, AminoText *text) {
    if (DEBUG_RENDERER) {
        printf("-> drawText()\n");
    }

    if (!text->layoutText()) {
        return;
    }

    c->save();

    //flip the y axis
    c->scale(1, -1);

    //baseline at top/left
    texture_font_t *tf = text->fontSize->fontTexture;

    //debug
    //sprintf("font: size=%f height=%f ascender=%f descender=%f\n", tf->size, tf->height, tf->ascender, tf->descender);

    switch (text->vAlign) {
        case AminoText::VALIGN_TOP:
            c->translate(0, -tf->ascender);
            break;

        case AminoText::VALIGN_BOTTOM:
            c->translate(0,  - text->propH->value - tf->descender + (text->lineNr - 1) * tf->height);
            break;

        case AminoText::VALIGN_MIDDLE:
            c->translate(0, - tf->ascender - (text->propH->value - text->lineNr * tf->height) / 2);
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
    c->bindTexture(texture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    c->useProgram(fontShader->shader);

    //color & opacity
    GLfloat color[4] = { text->propR->value, text->propG->value, text->propB->value, c->opacity * text->propOpacity->value };

    glUniform4fv(fontShader->colorUni, 1, color);
    glUniform1i(fontShader->texUni, 0); //GL_TEXTURE0
    glUniformMatrix4fv(fontShader->mvpUni, 1, 0, modelView);

    //only the global transform will change each time
    glUniformMatrix4fv(fontShader->transUni, 1, 0, c->globaltx);

    if (DEBUG_RENDERER_ERRORS) {
        showGLErrors("before text rendering");
    }

    //render
    vertex_buffer_render(text->buffer, GL_TRIANGLES);

    if (DEBUG_RENDERER_ERRORS) {
        showGLErrors("after text rendering");
    }

    c->restore();
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
