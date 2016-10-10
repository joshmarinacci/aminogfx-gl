#include "renderer.h"

#define DEBUG_RENDERER false
#define DEBUG_RENDERER_ERRORS false
#define DEBUG_FONT_PERFORMANCE 0

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

    //color lighting shader
    if (colorLightingShader) {
        colorLightingShader->destroy();
        delete colorLightingShader;
        colorLightingShader = NULL;
    }

    //texture lighting shader
    if (textureLightingShader) {
        textureLightingShader->destroy();
        delete textureLightingShader;
        textureLightingShader = NULL;
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
 * Setup perspective default values.
 */
void AminoRenderer::setupPerspective(v8::Local<v8::Object> &perspective) {
    //orthographic
    Nan::MaybeLocal<v8::Value> orthographicMaybe = Nan::Get(perspective, Nan::New<v8::String>("orthographic").ToLocalChecked());

    if (!orthographicMaybe.IsEmpty()) {
        v8::Local<v8::Value> orthographicValue = orthographicMaybe.ToLocalChecked();

        if (orthographicValue->IsBoolean()) {
            orthographic = orthographicValue->BooleanValue();
        }
    }

    //near
    Nan::MaybeLocal<v8::Value> nearMaybe = Nan::Get(perspective, Nan::New<v8::String>("near").ToLocalChecked());

    if (!nearMaybe.IsEmpty()) {
        v8::Local<v8::Value> nearValue = nearMaybe.ToLocalChecked();

        if (nearValue->IsNumber()) {
            near = nearValue->NumberValue();
        }
    }

    //far
    Nan::MaybeLocal<v8::Value> farMaybe = Nan::Get(perspective, Nan::New<v8::String>("far").ToLocalChecked());

    if (!farMaybe.IsEmpty()) {
        v8::Local<v8::Value> farValue = farMaybe.ToLocalChecked();

        if (farValue->IsNumber()) {
            far = farValue->NumberValue();
        } else {
            //set default value
            if (orthographic) {
                //do not limit depth
                far = -2048;
            } else {
                far = -300;
            }
        }
    }

    //eye
    Nan::MaybeLocal<v8::Value> eyeMaybe = Nan::Get(perspective, Nan::New<v8::String>("eye").ToLocalChecked());

    if (!eyeMaybe.IsEmpty()) {
        v8::Local<v8::Value> eyeValue = eyeMaybe.ToLocalChecked();

        if (eyeValue->IsNumber()) {
            eye = eyeValue->NumberValue();
        }
    }
}

/**
 * Update the model view projection matrix.
 */
void AminoRenderer::updateViewport(GLfloat width, GLfloat height, GLfloat viewportW, GLfloat viewportH) {
    //set up the viewport (y-inversion, top-left origin)

    //scale
    GLfloat scaleM[16];

    make_scale_matrix(1, -1, 1, scaleM);

    //translate
    GLfloat transM[16];

    make_trans_matrix(- width / 2, height / 2, 0, transM);

    //combine
    GLfloat m4[16];

    mul_matrix(m4, transM, scaleM);

    //3D perspective
    GLfloat pixelM[16];

    if (orthographic) {
        loadPixelPerfectOrthographicMatrix(pixelM, width, height, eye, near, far);
    } else {
        loadPixelPerfectMatrix(pixelM, width, height, eye, near, far);
    }

    mul_matrix(modelView, pixelM, m4);

    //set viewport
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

        case MODEL:
            this->drawModel((AminoModel *)root);
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

    bool hasAlpha = color[3] != 1.0;

    if (hasAlpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //vertex data
    colorShader->setVertexData(dim, verts);

    //draw
    if (dim == 0) {
        //special case: VBO elements
        colorShader->drawElements(NULL, count, mode);
    } else {
        //render vertices (array or VBO)
        colorShader->drawTriangles(count, mode);
    }

    //cleanup
    if (hasAlpha) {
        glDisable(GL_BLEND);
    }
}

/**
 * Draw texture.
 */
void AminoRenderer::applyTextureShader(GLfloat *verts, GLsizei dim, GLsizei count, GLfloat uv[][2], GLuint texId, GLfloat opacity, bool needsClampToBorder, bool repeatX, bool repeatY) {
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

    if (needsClampToBorder) {
        ((TextureClampToBorderShader *)shader)->setRepeat(repeatX, repeatY);
    }

    //draw
    ctx->bindTexture(texId);
    shader->setVertexData(dim, verts);
    shader->setTextureCoordinates(uv);
    shader->drawTriangles(count, GL_TRIANGLES);

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

    /*
     * Clipping:
     *
     *  - quite slow on Raspberry Pi!
     */
    bool useClipping = group->propClipRect->value;

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
 * Draw 3D model.
 */
void AminoRenderer::drawModel(AminoModel *model) {
    //check rendering mode

    // 1) vertices
    std::vector<float> *vecVertices = &model->propVertices->value;

    if (vecVertices->empty()) {
        return;
    }

    // 2) indices (optional)
    std::vector<ushort> *vecIndices = &model->propIndices->value;
    bool useElements = !vecIndices->empty();

    if (useElements) {
        if (model->vboIndex == INVALID_BUFFER) {
            glGenBuffers(1, &model->vboIndex);
            model->vboIndexModified = true;
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->vboIndex);

        if (model->vboIndexModified) {
            model->vboIndexModified = false;
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ushort) * vecIndices->size(), vecIndices->data(), GL_STATIC_DRAW);
        }
    }

    // 3) normals (optional)
    std::vector<float> *vecNormals = &model->propNormals->value;
    bool useNormals = !vecNormals->empty();

    // 4) texture coordinates (optional)
    std::vector<float> *vecUVs = &model->propUVs->value;
    bool useUVs = !vecUVs->empty();

    if (useUVs && !model->propTexture->value) {
        //teture not yet loaded
        return;
    }

    //shader
    AnyAminoShader *shader = NULL;
    ColorShader *colorShader = NULL;
    TextureShader *textureShader = NULL;
    bool hasAlpha = false;

    if (useNormals) {
        //use lighting shader

        if (!useElements) {
            assert(vecNormals->size() == vecVertices->size());
        }

        //get normals
        if (model->vboNormal == INVALID_BUFFER) {
            glGenBuffers(1, &model->vboNormal);
            model->vboNormalModified = true;
        }

        glBindBuffer(GL_ARRAY_BUFFER, model->vboNormal);

        if (model->vboNormalModified) {
            model->vboNormalModified = false;
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vecNormals->size(), vecNormals->data(), GL_STATIC_DRAW);
        }

        //get shader
        if (useUVs) {
            //texture lighting shader
            if (!textureLightingShader) {
                textureLightingShader = new TextureLightingShader();

                bool res = textureLightingShader->create();

                assert(res);
            }

            textureShader = textureLightingShader;
            shader = textureShader;

            //set normals
            ctx->useShader(shader);

            textureLightingShader->setNormalVectors(NULL);
        } else {
            //color lighting shader
            if (!colorLightingShader) {
                colorLightingShader = new ColorLightingShader();

                bool res = colorLightingShader->create();

                assert(res);
            }

            colorShader = colorLightingShader;
            shader = colorShader;

            //set normals
            ctx->useShader(shader);

            colorLightingShader->setNormalVectors(NULL);
        }
    } else {
        //without lighting

        if (useUVs) {
            //texture shader
            textureShader = this->textureShader;
            shader = textureShader;
        } else {
            //color shader
            colorShader = this->colorShader;
            shader = colorShader;
        }

        ctx->useShader(shader);
    }

    //color shader
    if (colorShader) {
        GLfloat opacity = model->propOpacity->value * ctx->opacity;
        GLfloat color[4] = { model->propFillR->value, model->propFillG->value, model->propFillB->value, opacity };

        colorShader->setColor(color);
        hasAlpha = opacity != 1.0;
    }

    //texture shader
    if (textureShader) {
        //set texture coordinates
        if (model->vboUV == INVALID_BUFFER) {
            glGenBuffers(1, &model->vboUV);
            model->vboUVModified = true;
        }

        glBindBuffer(GL_ARRAY_BUFFER, model->vboUV);

        if (model->vboUVModified) {
            model->vboUVModified = false;
            glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vecUVs->size(), vecUVs->data(), GL_STATIC_DRAW);
        }

        textureShader->setTextureCoordinates(NULL);

        //opacity
        GLfloat opacity = model->propOpacity->value * ctx->opacity;

        textureShader->setOpacity(opacity);
        hasAlpha = opacity != 1.0;

        //texture
        AminoTexture *texture = (AminoTexture *)model->propTexture->value;

        ctx->bindTexture(texture->textureId);
    }

    //alpha
    if (hasAlpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    //vertices
    if (model->vboVertex == INVALID_BUFFER) {
        glGenBuffers(1, &model->vboVertex);
        model->vboVertexModified = true;
    }

    glBindBuffer(GL_ARRAY_BUFFER, model->vboVertex);

    if (model->vboVertexModified) {
        model->vboVertexModified = false;
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vecVertices->size(), vecVertices->data(), GL_STATIC_DRAW);
    }

    shader->setVertexData(3, NULL);

    //enable depth mask
    if (!hasAlpha) {
        //use depth mask (if not transparent)
        ctx->enableDepth();
    }

    //draw
    shader->setTransformation(modelView, ctx->globaltx);

    if (useElements) {
        //special case: VBO elements
        shader->drawElements(NULL, vecIndices->size(), GL_TRIANGLES);
    } else {
        //render vertices (array or VBO)
        shader->drawTriangles(vecVertices->size() / 3, GL_TRIANGLES);
    }

    //cleanup
    if (!hasAlpha) {
        ctx->disableDepth();
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (useElements) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    if (hasAlpha) {
        glDisable(GL_BLEND);
    }
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
            bool needsClampToBorder = (tx < 0 || tx > 1) || (tx2 < 0 || tx2 > 1) || (ty < 0 || ty > 1) || (ty2 < 0 || ty2 > 1) || rect->repeatX || rect->repeatY;

            applyTextureShader((float *)verts, 2, 6, texCoords, texture->textureId, opacity, needsClampToBorder, rect->repeatX, rect->repeatY);
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

    //get texture
    GLuint texture = text->getTextureId();

    if (texture == INVALID_TEXTURE) {
        return;
    }

    ctx->save();

    //flip the y axis
    ctx->scale(1, -1);

    //baseline at top/left
    texture_font_t *tf = text->fontSize->fontTexture;

    //debug
    //sprintf("font: size=%f height=%f ascender=%f descender=%f\n", tf->size, tf->height, tf->ascender, tf->descender);

    //horizontal alignment
    switch (text->align) {
        case AminoText::ALIGN_CENTER:
            ctx->translate((text->propW->value - text->lineW) / 2, 0);
            break;

        case AminoText::ALIGN_RIGHT:
            ctx->translate(text->propW->value - text->lineW, 0);
            break;

        case AminoText::ALIGN_LEFT:
        default:
            break;
    }

    //vertical alignment
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
amino_atlas_t AminoRenderer::getAtlasTexture(texture_atlas_t *atlas, bool createIfMissing) {
    assert(fontShader);

    return fontShader->getAtlasTexture(atlas, createIfMissing);
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

/**
 * Check texture CPU to GPU performance.
 */
void AminoRenderer::checkTexturePerformance() {
    GLint textureW = 512;
    GLint textureH = 512;
    char *data = new char[textureW * textureH * 4]; //RGBA

    //create texture
    GLuint textureId = INVALID_TEXTURE;

    glGenTextures(1, &textureId);

    assert(textureId != INVALID_TEXTURE);

    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    //intro
    double startTime, diff;

    printf("Texture performance: %ix%i\n", textureW, textureH);

    //depth 1, alpha
    startTime = getTime();

    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, textureW, textureH, 0, GL_ALPHA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    diff = getTime() - startTime;
    printf("-> GL_ALPHA: %i ms\n", (int)diff);

    //depth 1, grayscale
    startTime = getTime();

    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, textureW, textureH, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    diff = getTime() - startTime;
    printf("-> GL_LUMINANCE: %i ms\n", (int)diff);

    //depth 3, RGB
    startTime = getTime();

    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureW, textureH, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    diff = getTime() - startTime;
    printf("-> GL_RGB: %i ms\n", (int)diff);

    //depth 4, RGBA
    startTime = getTime();

    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureW, textureH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    diff = getTime() - startTime;
    printf("-> GL_RGBA: %i ms\n", (int)diff);

    //cleanup
    delete[] data;

    glDeleteTextures(1, &textureId);
}