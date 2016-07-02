#include "SimpleRenderer.h"
//#include <node_buffer.h>

SimpleRenderer::SimpleRenderer() {
    modelViewChanged = false;
}

void SimpleRenderer::startRender(AminoNode *root) {
    GLContext *c = new GLContext();

    this->render(c, root);
    delete c;

//    printf("shader count = %d\n",c->shadercount);
//    printf("shader dupe count = %d\n",c->shaderDupCount);
//    printf("texture dupe count = %d\n",c->texDupCount);

}
void SimpleRenderer::render(GLContext *c, AminoNode *root) {
    if (root == NULL) {
        printf("WARNING. NULL NODE!\n");
        return;
    }
    //skip non-visible nodes
    if (root->visible != 1) {
        return;
    }

    c->save();

    //transform
    c->translate(root->x, root->y);
    c->scale(root->scalex, root->scaley);
    c->rotate(root->rotatex, root->rotatey, root->rotatez);

    //draw
    switch (root->type) {
        case GROUP:
            this->drawGroup(c, (Group *)root);
            break;

        case RECT:
            this->drawRect(c, (Rect *)root);
            break;

        case POLY:
            this->drawPoly(c, (PolyNode *)root);
            break;

        case TEXT:
            this->drawText(c, (TextNode *)root);
            break;

        case GLNODE:
            this->drawGLNode(c, (GLNode *)root);
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
    if (group->cliprect == 1) {
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
        float x2 = group->w;
        float y2 = group->h;
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
    c->applyOpacity(group->opacity);

    //render items
    for (std::size_t i = 0; i < group->children.size(); i++) {
        this->render(c, group->children[i]);
    }

    //restore opacity
    c->restoreOpacity();

    if (group->cliprect == 1) {
        glDisable(GL_STENCIL_TEST);
    }
}

void SimpleRenderer::drawPoly(GLContext *ctx, PolyNode *poly) {
    std::vector<float> *geometry = poly->getGeometry();
    int len = geometry->size();
    int dim = poly->dimension;
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
        colors[i][0] = poly->r;
        colors[i][1] = poly->g;
        colors[i][2] = poly->b;
    }

    ctx->useProgram(colorShader->prog);
    glUniformMatrix4fv(colorShader->u_matrix, 1, GL_FALSE, modelView);
    glUniformMatrix4fv(colorShader->u_trans,  1, GL_FALSE, ctx->globaltx);
    glUniform1f(colorShader->u_opacity, poly->opacity);

    GLfloat opacity = poly->opacity * ctx->opacity;

    if (opacity != 1.0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    if (dim == 2) {
        glVertexAttribPointer(colorShader->attr_pos,   2, GL_FLOAT, GL_FALSE, 0, verts);
    } else if (dim == 3) {
        glVertexAttribPointer(colorShader->attr_pos,   3, GL_FLOAT, GL_FALSE, 0, verts);
    }

    glVertexAttribPointer(colorShader->attr_color, 3, GL_FLOAT, GL_FALSE, 0, colors);
    glEnableVertexAttribArray(colorShader->attr_pos);
    glEnableVertexAttribArray(colorShader->attr_color);

    if (poly->filled == 1) {
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
    c->save();

    //two triangles
    float x =  0;
    float y =  0;
    float x2 = rect->w;
    float y2 = rect->h;

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

    GLfloat opacity = rect->opacity * c->opacity;

    if (rect->texid != INVALID) {
        //texture

        //image coordinates (fractional world coordinates)
        GLfloat texcoords[6][2];
        float tx  = rect->left;   //0
        float ty2 = rect->bottom; //1;
        float tx2 = rect->right;  //1;
        float ty  = rect->top;    //0;

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

        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 3; j++) {
                colors[i][j] = 0.5;

                if (j==0) {
                    colors[i][j] = rect->r;
                } else if (j==1) {
                    colors[i][j] = rect->g;
                } else if (j==2) {
                    colors[i][j] = rect->b;
                }
            }
        }

        colorShaderApply(c, colorShader, modelView, verts, colors, opacity);
    }

    c->restore();
}

int te = 0;

/**
 * Render text.
 */
void SimpleRenderer::drawText(GLContext *c, TextNode *text) {
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
    GLfloat color[4] = {text->r, text->g, text->b, c->opacity * text->opacity};

    glUniform4fv(font->coloruni, 1, color);

    glUniform1i(font->texuni, 0); //GL_TEXTURE0

    /*
    if (modelViewChanged) {
        glUniformMatrix4fv(font->mvpuni,         1, 0,  modelView  );
    }
    */

    glUniformMatrix4fv(font->mvpuni, 1, 0, modelView);

    //only the global transform will change each time
    glUniformMatrix4fv(font->transuni, 1, 0, c->globaltx);

    //render
    vertex_buffer_render(text->buffer, GL_TRIANGLES);

    c->restore();
}

NAN_METHOD(node_glGetString) {
  int val = info[0]->Uint32Value();
  const char *version = (const char*)glGetString(val);

  info.GetReturnValue().Set(Nan::New(version).ToLocalChecked());
}

NAN_METHOD(node_glGetError) {
  int val = glGetError();;

  info.GetReturnValue().Set(val);
}

/*
NAN_METHOD(node_glGenVertexArrays) {
  HandleScope scope;
  int val   = info[0]->Uint32Value();
  GLuint vao;
  glGenVertexArrays(val, &vao);
  Local<Number> str = Number::New(vao);
  return scope.Close(str);
}


NAN_METHOD(node_glBindVertexArray) {
  HandleScope scope;
  int val   = info[0]->Uint32Value();
  glBindVertexArray(val);
  return scope.Close(Undefined());
}
*/

NAN_METHOD(node_glGenBuffers) {
  int val   = info[0]->Uint32Value();
  GLuint vbo;

  glGenBuffers(val, &vbo);

  info.GetReturnValue().Set(vbo);
}

NAN_METHOD(node_glBindBuffer) {
  int type   = info[0]->Uint32Value();
  int vbo    = info[1]->Uint32Value();

  glBindBuffer(type,vbo);
}


NAN_METHOD(node_glBufferData) {
  int type   = info[0]->Uint32Value();
  Handle<Array> array = Handle<Array>::Cast(info[1]);
  float* verts = new float[array->Length()];

  for (std::size_t i = 0; i < array->Length(); i++) {
      verts[i] = array->Get(i)->ToNumber()->NumberValue();
  }

  int kind = info[2]->Uint32Value();

  glBufferData(type, sizeof(float) * array->Length(), verts, kind);
}

NAN_METHOD(node_glGenFramebuffers) {
//  int val   = info[0]->Uint32Value();
//  GLuint buf;
//  glGenFramebuffers(val, &buf);
//  info.GetReturnValue().Set(buf);
}

NAN_METHOD(node_glBindFramebuffer) {
//  int type   = info[0]->Uint32Value();
//  int buf    = info[1]->Uint32Value();
//  glBindFramebuffer(type,buf);
}

NAN_METHOD(node_glCheckFramebufferStatus) {
//  int buf    = info[0]->Uint32Value();
//  GLuint val = glCheckFramebufferStatus(buf);
//  info.GetReturnValue().Set(val);
}

NAN_METHOD(node_glGenTextures) {
  int val   = info[0]->Uint32Value();
  GLuint tex;

  glGenTextures(val, &tex);

  info.GetReturnValue().Set(tex);
}

NAN_METHOD(node_glBindTexture) {
  int type   = info[0]->Uint32Value();
  int tex    = info[1]->Uint32Value();

  glBindTexture(type,tex);
}

NAN_METHOD(node_glActiveTexture) {
  int type   = info[0]->Uint32Value();

  glActiveTexture(type);
}

NAN_METHOD(node_glTexImage2D) {
  int type     = info[0]->Uint32Value();
  int v1       = info[1]->Uint32Value();
  int format1  = info[2]->Uint32Value();
  int width    = info[3]->Uint32Value();
  int height   = info[4]->Uint32Value();
  int depth    = info[5]->Uint32Value();
  int format2  = info[6]->Uint32Value();
  int type2    = info[7]->Uint32Value();
//  int data     = info[8]->Uint32Value();

  glTexImage2D(type, v1, format1, width, height, depth, format2, type2, NULL);
}

NAN_METHOD(node_glTexParameteri) {
    int type     = info[0]->Uint32Value();
    int param    = info[1]->Uint32Value();
    int value    = info[2]->Uint32Value();

    glTexParameteri(type, param, value);
}

//NAN_METHOD(node_glFramebufferTexture2D) {
//    int type     = info[0]->Uint32Value();
//    int attach   = info[1]->Uint32Value();
//    int type2    = info[2]->Uint32Value();
//    int value    = info[3]->Uint32Value();
//    int other    = info[4]->Uint32Value();
//    glFramebufferTexture2D(type, attach, type2, value, other);
//}

//using node::Buffer;
NAN_METHOD(node_glReadPixels) {
/*
    int x     = info[0]->Uint32Value();
    int y     = info[1]->Uint32Value();
    int w     = info[2]->Uint32Value();
    int h     = info[3]->Uint32Value();
    int format= info[4]->Uint32Value();
    int type  = info[5]->Uint32Value();

    int length = w*h*3;
    char* data = (char*)malloc(length);
    glReadPixels(x,y,w,h, format, type, data);
    Buffer *slowBuffer = Buffer::New(length);
    memcpy(Buffer::Data(slowBuffer), data, length);
    Local<Object> globalObj = Context::GetCurrent()->Global();
    Local<Function> bufferConstructor = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
    Handle<Value> constructorArgs[3] = { slowBuffer->handle_, v8::Integer::New(length), v8::Integer::New(0) };
    Local<Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);
    return scope.Close(actualBuffer);
    */
}

NAN_METHOD(node_glEnableVertexAttribArray) {
  int loc                 = info[0]->Uint32Value();

  glEnableVertexAttribArray(loc);

  info.GetReturnValue().Set(loc);
}

NAN_METHOD(node_glVertexAttribPointer) {
  int loc                        = info[0]->Uint32Value();
  int count                      = info[1]->Uint32Value();
  int size                       = info[2]->Uint32Value();
  int other                      = info[3]->Uint32Value();
  int size2  = sizeof(float)*(int)(info[4]->Uint32Value());
  int offset = sizeof(float)*(int)(info[5]->Uint32Value());

  glVertexAttribPointer(loc, count, size, other, size2, (void *)offset);
}

NAN_METHOD(node_glUniform1f) {
  //int loc                 = info[0]->Uint32Value();
  //float value             = info[1]->Uint32Value();
  //glUniform1f(loc,value);
}

NAN_METHOD(node_glUniform2f) {
//  int loc                 = info[0]->Uint32Value();
//  float value             = info[1]->Uint32Value();
//  float value2            = info[2]->Uint32Value();
//  glUniform2f(loc,value,value2);
}

NAN_METHOD(node_glPointSize) {
//  float size                 = args[0]->ToDoubleValue();
//  glPointSize(size);
}

NAN_METHOD(node_glEnable) {
  int var                 = info[0]->Uint32Value();

  glEnable(var);
}

NAN_METHOD(node_glBlendFunc) {
  int src                 = info[0]->Uint32Value();
  int dst                 = info[1]->Uint32Value();

  glBlendFunc(src, dst);
}

NAN_METHOD(node_glBlendFuncSeparate) {
  int a                 = info[0]->Uint32Value();
  int b                 = info[1]->Uint32Value();
  int c                 = info[2]->Uint32Value();
  int d                 = info[3]->Uint32Value();

  glBlendFuncSeparate(a, b, c, d);
}

NAN_METHOD(node_glBlendEquation) {
  int eq                 = info[0]->Uint32Value();

  glBlendEquation(eq);
}


NAN_METHOD(node_glDrawArrays) {
  int type               = info[0]->Uint32Value();
  int c1                 = info[1]->Uint32Value();
  int count              = info[2]->Uint32Value();

  glDrawArrays(type, c1, count);
}

NAN_METHOD(node_setModelView) {
  int id               = info[0]->Uint32Value();

  //printf("setting to %d\n",id);
  glUniformMatrix4fv(id, 1, GL_FALSE, modelView);
//    glUniformMatrix4fv(u_trans,  1, GL_FALSE, trans);
}

static GLContext* sctx;

NAN_METHOD(node_setGlobalTransform) {
  int id               = info[0]->Uint32Value();

//  sctx->dumpGlobalTransform();
  glUniformMatrix4fv(id, 1, GL_FALSE, sctx->globaltx);
}

void SimpleRenderer::drawGLNode(GLContext* ctx, GLNode* glnode) {
    sctx = ctx;

    //FIXME not used
    v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();
    Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("glnode").ToLocalChecked());
    Nan::Set(event_obj, Nan::New("GL_SHADING_LANGUAGE_VERSION").ToLocalChecked(), Nan::New(GL_SHADING_LANGUAGE_VERSION));
    Nan::Set(event_obj, Nan::New("GL_ARRAY_BUFFER").ToLocalChecked(), Nan::New(GL_ARRAY_BUFFER));
    Nan::Set(event_obj, Nan::New("GL_STATIC_DRAW").ToLocalChecked(), Nan::New(GL_STATIC_DRAW));
    Nan::Set(event_obj, Nan::New("GL_VERTEX_SHADER").ToLocalChecked(), Nan::New(GL_VERTEX_SHADER));
    Nan::Set(event_obj, Nan::New("GL_FRAGMENT_SHADER").ToLocalChecked(), Nan::New(GL_FRAGMENT_SHADER));
    Nan::Set(event_obj, Nan::New("GL_COMPILE_STATUS").ToLocalChecked(), Nan::New(GL_COMPILE_STATUS));
    Nan::Set(event_obj, Nan::New("GL_LINK_STATUS").ToLocalChecked(), Nan::New(GL_LINK_STATUS));
    Nan::Set(event_obj, Nan::New("GL_TRUE").ToLocalChecked(), Nan::New(GL_TRUE));
    Nan::Set(event_obj, Nan::New("GL_FALSE").ToLocalChecked(), Nan::New(GL_FALSE));
    Nan::Set(event_obj, Nan::New("GL_FLOAT").ToLocalChecked(), Nan::New(GL_FLOAT));
    Nan::Set(event_obj, Nan::New("GL_BLEND").ToLocalChecked(), Nan::New(GL_BLEND));
    Nan::Set(event_obj, Nan::New("GL_SRC_ALPHA").ToLocalChecked(), Nan::New(GL_SRC_ALPHA));
    Nan::Set(event_obj, Nan::New("GL_ONE_MINUS_SRC_ALPHA").ToLocalChecked(), Nan::New(GL_ONE_MINUS_SRC_ALPHA));

//    Nan::Set(event_obj, Nan::New("GL_MAX"), Nan::New(GL_MAX_EXT));
    //Nan::Set(event_obj, Nan::New("GL_ADD"), Nan::New(GL_ADD));
    Nan::Set(event_obj, Nan::New("GL_POINTS").ToLocalChecked(), Nan::New(GL_POINTS));
    Nan::Set(event_obj, Nan::New("GL_LINES").ToLocalChecked(), Nan::New(GL_LINES));
    Nan::Set(event_obj, Nan::New("GL_LINE_STRIP").ToLocalChecked(), Nan::New(GL_LINE_STRIP));
    Nan::Set(event_obj, Nan::New("GL_LINE_LOOP").ToLocalChecked(), Nan::New(GL_LINE_LOOP));
    Nan::Set(event_obj, Nan::New("GL_TRIANGLE_FAN").ToLocalChecked(), Nan::New(GL_TRIANGLE_FAN));
    Nan::Set(event_obj, Nan::New("GL_TRIANGLE_STRIP").ToLocalChecked(), Nan::New(GL_TRIANGLE_STRIP));
    Nan::Set(event_obj, Nan::New("GL_TRIANGLES").ToLocalChecked(), Nan::New(GL_TRIANGLES));

    Nan::Set(event_obj, Nan::New("GL_NO_ERROR").ToLocalChecked(), Nan::New(GL_NO_ERROR));
    Nan::Set(event_obj, Nan::New("GL_INVALID_ENUM").ToLocalChecked(), Nan::New(GL_INVALID_ENUM));
    Nan::Set(event_obj, Nan::New("GL_INVALID_VALUE").ToLocalChecked(), Nan::New(GL_INVALID_VALUE));
    Nan::Set(event_obj, Nan::New("GL_INVALID_OPERATION").ToLocalChecked(), Nan::New(GL_INVALID_OPERATION));
    Nan::Set(event_obj, Nan::New("GL_OUT_OF_MEMORY").ToLocalChecked(), Nan::New(GL_OUT_OF_MEMORY));
    Nan::Set(event_obj, Nan::New("GL_TEXTURE_2D").ToLocalChecked(), Nan::New(GL_TEXTURE_2D));
    Nan::Set(event_obj, Nan::New("GL_TEXTURE0").ToLocalChecked(), Nan::New(GL_TEXTURE0));
    Nan::Set(event_obj, Nan::New("GL_RGB").ToLocalChecked(), Nan::New(GL_RGB));
    Nan::Set(event_obj, Nan::New("GL_TEXTURE_MIN_FILTER").ToLocalChecked(), Nan::New(GL_TEXTURE_MIN_FILTER));
    Nan::Set(event_obj, Nan::New("GL_TEXTURE_MAG_FILTER").ToLocalChecked(), Nan::New(GL_TEXTURE_MAG_FILTER));
    Nan::Set(event_obj, Nan::New("GL_LINEAR").ToLocalChecked(), Nan::New(GL_LINEAR));
    Nan::Set(event_obj, Nan::New("GL_UNSIGNED_BYTE").ToLocalChecked(), Nan::New(GL_UNSIGNED_BYTE));
//    Nan::Set(event_obj, Nan::New("NULL").ToLocalChecked(), Nan::New((int)NULL));
//    Nan::Set(event_obj, Nan::New("GL_FRAMEBUFFER").ToLocalChecked(), Nan::New(GL_FRAMEBUFFER));
//    Nan::Set(event_obj, Nan::New("GL_FRAMEBUFFER_COMPLETE").ToLocalChecked(), Nan::New(GL_FRAMEBUFFER_COMPLETE));
//    Nan::Set(event_obj, Nan::New("GL_COLOR_ATTACHMENT0").ToLocalChecked(), Nan::New(GL_COLOR_ATTACHMENT0));

    Nan::Set(event_obj, Nan::New("glGetString").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetString)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glGetError").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetError)).ToLocalChecked());
//    Nan::Set(event_obj, Nan::New("glGenVertexArrays").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGenVertexArrays)).ToLocalChecked());
//    Nan::Set(event_obj, Nan::New("glBindVertexArray").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glBindVertexArray)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glGenBuffers").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGenBuffers)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glBindBuffer").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glBindBuffer)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glBufferData").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glBufferData)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glCreateShader").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glCreateShader)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glShaderSource").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glShaderSource)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glCompileShader").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glCompileShader)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glGetShaderiv").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetShaderiv)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glGetProgramiv").ToLocalChecked(),  Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetProgramiv)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glGetShaderInfoLog").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetShaderInfoLog)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glGetProgramInfoLog").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetProgramInfoLog)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glCreateProgram").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glCreateProgram)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glAttachShader").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glAttachShader)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glLinkProgram").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glLinkProgram)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glUseProgram").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glUseProgram)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glGetAttribLocation").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetAttribLocation)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glGetUniformLocation").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetUniformLocation)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glEnableVertexAttribArray").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glEnableVertexAttribArray)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glVertexAttribPointer").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glVertexAttribPointer)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glUniform1f").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glUniform1f)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glUniform2f").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glUniform2f)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glPointSize").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glPointSize)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glEnable").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glEnable)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glBlendFunc").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glBlendFunc)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glBlendFuncSeparate").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glBlendFuncSeparate)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glBlendEquation").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glBlendEquation)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("GL_FUNC_ADD").ToLocalChecked(), Nan::New(GL_FUNC_ADD));
    Nan::Set(event_obj, Nan::New("GL_ONE").ToLocalChecked(), Nan::New(GL_ONE));
    Nan::Set(event_obj, Nan::New("GL_ZERO").ToLocalChecked(), Nan::New(GL_ZERO));
    Nan::Set(event_obj, Nan::New("glDrawArrays").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glDrawArrays)).ToLocalChecked());

    Nan::Set(event_obj, Nan::New("glGenFramebuffers").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGenFramebuffers)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glBindFramebuffer").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glBindFramebuffer)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glCheckFramebufferStatus").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glCheckFramebufferStatus)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glGenTextures").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGenTextures)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glBindTexture").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glBindTexture)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glActiveTexture").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glActiveTexture)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glTexImage2D").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glTexImage2D)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glTexParameteri").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glTexParameteri)).ToLocalChecked());
//    Nan::Set(event_obj, Nan::New("glFramebufferTexture2D").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glFramebufferTexture2D)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("glReadPixels").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glReadPixels)).ToLocalChecked());


    Nan::Set(event_obj, Nan::New("setModelView").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_setModelView)).ToLocalChecked());
    Nan::Set(event_obj, Nan::New("setGlobalTransform").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_setGlobalTransform)).ToLocalChecked());

//    Handle<Value> event_argv[] = {event_obj};
//    glnode->callback->Call(Context::GetCurrent()->Global(),1,event_argv);


}
