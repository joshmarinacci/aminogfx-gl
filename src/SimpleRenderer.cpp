#include "SimpleRenderer.h"

#define DEBUG_RENDERER false

SimpleRenderer::SimpleRenderer(ColorShader *colorShader, TextureShader *textureShader, GLfloat *modelView): colorShader(colorShader), textureShader(textureShader), modelView(modelView) {
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
}

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
    c->translate(root->propX->value, root->propY->value);
    c->scale(root->propScaleX->value, root->propScaleY->value);
    c->rotate(root->propRotateX->value, root->propRotateY->value, root->propRotateZ->value);

    //draw
    switch (root->type) {
        case GROUP:
            this->drawGroup(c, (Group *)root);
            break;

        case RECT:
            this->drawRect(c, (Rect *)root);
            break;

        case POLY:
            this->drawPoly(c, (Polygon *)root);
            break;

        case TEXT:
            this->drawText(c, (TextNode *)root);
            break;

        default:
            printf("invalid node type: %i\n", root->type);
            break;
    }

    //done
    c->restore();
}

void colorShaderApply(GLContext *ctx, ColorShader* shader, GLfloat modelView[16], GLfloat verts[][2], GLfloat colors[][3], GLfloat opacity) {
    ctx->useProgram(shader->prog);
    glUniformMatrix4fv(shader->u_matrix, 1, GL_FALSE, modelView);
    glUniformMatrix4fv(shader->u_trans,  1, GL_FALSE, ctx->globaltx);
    glUniform1f(shader->u_opacity, opacity);

    if (opacity != 1.0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glVertexAttribPointer(shader->attr_pos,   2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(shader->attr_color, 3, GL_FLOAT, GL_FALSE, 0, colors);
    glEnableVertexAttribArray(shader->attr_pos);
    glEnableVertexAttribArray(shader->attr_color);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(shader->attr_pos);
    glDisableVertexAttribArray(shader->attr_color);
}

void textureShaderApply(GLContext *ctx, TextureShader *shader, GLfloat modelView[16], GLfloat verts[][2], GLfloat texcoords[][2], int texid, GLfloat opacity) {
    //printf("doing texture shader apply %d opacity = %f\n",texid, opacity);

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
    ctx->bindTexture(texid);
    glDrawArrays(GL_TRIANGLES, 0, 6); //contains colors

    glDisableVertexAttribArray(shader->attr_pos);
    glDisableVertexAttribArray(shader->attr_texcoords);
    glDisable(GL_BLEND);
}

void SimpleRenderer::drawGroup(GLContext *c, Group *group) {
    if (DEBUG_RENDERER) {
        printf("-> drawGroup()\n");
    }

    if (group->propCliprect->value) {
        //turn on stenciling
        glDepthMask(GL_FALSE);
        glEnable(GL_STENCIL_TEST);
        //clear the buffers

        //setup the stencil
        glStencilFunc(GL_ALWAYS, 0x1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);
        glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

        GLfloat colors[6][3];

        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 3; j++) {
                colors[i][j] = 1.0;
            }
        }

        colorShaderApply(c, colorShader, modelView, verts, colors, 1.0);

        //set function to draw pixels where the buffer is equal to 1
        glStencilFunc(GL_EQUAL, 0x1, 0xFF);
        glStencilMask(0x00);

        //turn color buffer drawing back on
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
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
}

void SimpleRenderer::drawPoly(GLContext *ctx, Polygon *poly) {
    if (DEBUG_RENDERER) {
        printf("-> drawPoly()\n");
    }

    std::vector<float> *geometry = &poly->propGeometry->value;
    int len = geometry->size();
    int dim = poly->propDimension->value;
    GLfloat verts[len][dim];// = malloc(sizeof(GLfloat[2])*len);

    for (int i = 0; i < len / dim; i++) {
        verts[i][0] = geometry->at(i * dim);

        if (dim >=2) {
            verts[i][1] = geometry->at(i * dim + 1);
        }
        if (dim >=3) {
            verts[i][2] = geometry->at(i * dim + 2);
        }
    }

    GLfloat colors[len][3];

    for (int i = 0; i < len / dim; i++) {
        colors[i][0] = poly->propFillR->value;
        colors[i][1] = poly->propFillG->value;
        colors[i][2] = poly->propFillB->value;
    }

    ctx->useProgram(colorShader->prog);
    glUniformMatrix4fv(colorShader->u_matrix, 1, GL_FALSE, modelView);
    glUniformMatrix4fv(colorShader->u_trans,  1, GL_FALSE, ctx->globaltx);

    //opacity
    GLfloat opacity = poly->propOpacity->value * ctx->opacity;

    glUniform1f(colorShader->u_opacity, opacity);

    if (opacity != 1.0) {
        //blend mode needed
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    if (dim == 2) {
        glVertexAttribPointer(colorShader->attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
    } else if (dim == 3) {
        glVertexAttribPointer(colorShader->attr_pos, 3, GL_FLOAT, GL_FALSE, 0, verts);
    }

    glVertexAttribPointer(colorShader->attr_color, 3, GL_FLOAT, GL_FALSE, 0, colors);
    glEnableVertexAttribArray(colorShader->attr_pos);
    glEnableVertexAttribArray(colorShader->attr_color);

    if (poly->propFilled->value) {
        glDrawArrays(GL_TRIANGLE_FAN, 0, len / dim);
    } else {
        glDrawArrays(GL_LINE_LOOP, 0, len / dim);
    }

    glDisableVertexAttribArray(colorShader->attr_pos);
    glDisableVertexAttribArray(colorShader->attr_color);

    if (opacity != 1.0) {
        glDisable(GL_BLEND);
    }
}

void SimpleRenderer::drawRect(GLContext *c, Rect *rect) {
    if (DEBUG_RENDERER) {
        printf("-> drawRect()\n");
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

    if (rect->texid != INVALID) {
        //texture

        //image coordinates (fractional world coordinates)
        GLfloat texcoords[6][2];
        float tx  = rect->propLeft->value;   //0
        float ty2 = rect->propBottom->value; //1;
        float tx2 = rect->propRight->value;  //1;
        float ty  = rect->propTop->value;    //0;

        texcoords[0][0] = tx;    texcoords[0][1] = ty;
        texcoords[1][0] = tx2;   texcoords[1][1] = ty;
        texcoords[2][0] = tx2;   texcoords[2][1] = ty2;

        texcoords[3][0] = tx2;   texcoords[3][1] = ty2;
        texcoords[4][0] = tx;    texcoords[4][1] = ty2;
        texcoords[5][0] = tx;    texcoords[5][1] = ty;

        textureShaderApply(c, textureShader, modelView, verts, texcoords, rect->texid, opacity);
    } else if (!rect->hasImage) {
        //color
        GLfloat colors[6][3];
        float r = rect->propR->value;
        float g = rect->propG->value;
        float b = rect->propB->value;

        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 3; j++) {
                colors[i][j] = 0.5;

                if (j==0) {
                    colors[i][j] = r;
                } else if (j==1) {
                    colors[i][j] = g;
                } else if (j==2) {
                    colors[i][j] = b;
                }
            }
        }

        colorShaderApply(c, colorShader, modelView, verts, colors, opacity);
    }

    c->restore();
}

/**
 * Render text.
 */
void SimpleRenderer::drawText(GLContext *c, TextNode *text) {
    if (DEBUG_RENDERER) {
        printf("-> drawText()\n");
    }

    if (fontmap.empty()) {
        return;
    }

    if (text->fontid == INVALID) {
        return;
    }

    AminoFont *font = fontmap[text->fontid];

    c->save();

    //flip the y axis
    c->scale(1, -1);

    //baseline at top/left
    texture_font_t *tf = font->fonts[text->fontsize];

    //debug
    //sprintf("font: size=%f height=%f ascender=%f descender=%f\n", tf->size, tf->height, tf->ascender, tf->descender);

    switch (text->vAlign) {
        case VALIGN_TOP:
            c->translate(0, -tf->ascender);
            break;

        case VALIGN_BOTTOM:
            c->translate(0,  - text->h - tf->descender + (text->lineNr - 1) * tf->height);
            break;

        case VALIGN_MIDDLE:
            c->translate(0, - tf->ascender - (text->h - text->lineNr * tf->height) / 2);
            break;

        case VALIGN_BASELINE:
        default:
            break;
    }

    //use texture
    glActiveTexture(GL_TEXTURE0);
    c->bindTexture(font->atlas->id);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    c->useProgram(font->shader);

    //by only doing this init work once we save almost 80% of the time for drawing text
    if (font->texuni == -1) {
        font->texuni   = glGetUniformLocation(font->shader, "texture");
        font->mvpuni   = glGetUniformLocation(font->shader, "mvp");
        font->transuni = glGetUniformLocation(font->shader, "trans");
        font->coloruni = glGetUniformLocation(font->shader, "color");
    }

    //color & opacity
    GLfloat color[4] = {text->r, text->g, text->b, c->opacity * text->propOpacity->value};

    glUniform4fv(font->coloruni, 1, color);

    glUniform1i(font->texuni, 0); //GL_TEXTURE0

    glUniformMatrix4fv(font->mvpuni, 1, 0, modelView);

    //only the global transform will change each time
    glUniformMatrix4fv(font->transuni, 1, 0, c->globaltx);

    //render
    vertex_buffer_render(text->buffer, GL_TRIANGLES);

    c->restore();
}
