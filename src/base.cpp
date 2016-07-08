#include "base.h"

using namespace v8;

ColorShader *colorShader;
TextureShader *textureShader;
GLfloat *modelView;
GLfloat *globaltx;
float window_fill_red;
float window_fill_green;
float window_fill_blue;
float window_opacity;
int width = 640;
int height = 480;

std::stack<void *> matrixStack;
int rootHandle;
std::map<int, AminoFont *> fontmap;
Nan::Callback *NODE_EVENT_CALLBACK;
std::vector<AminoNode *> rects;
std::vector<Anim *> anims;
std::vector<Update *> updates;

//
//  AminoGfx
//

AminoGfx::AminoGfx(std::string name): AminoJSObject(name) {
    //empty
}

AminoGfx::~AminoGfx() {
    if (startCallback) {
        delete startCallback;
    }

    if (!destroyed) {
        destroy();
    }
}

/**
 * Initialize AminoGfx template.
 */
void AminoGfx::Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target, AminoJSObjectFactory* factory) {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(factory);

    //prototype methods
    Nan::SetPrototypeMethod(tpl, "start", Start);
    Nan::SetPrototypeMethod(tpl, "destroy", Destroy);

    //global template instance
    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

/**
 * JS object was initalized.
 */
void AminoGfx::setup() {
    //register native properties
    addPropertyWatcher("w", SetW);
    addPropertyWatcher("h", SetH);

    //screen size
    int w, h, refreshRate;

    if (getScreenInfo(w, h, refreshRate)) {
        v8::Local<v8::Object> obj = Nan::New<v8::Object>();

        //add screen property
        Nan::Set(handle(), Nan::New("screen").ToLocalChecked(), obj);

        //properties
        Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

        if (refreshRate > 0) {
            Nan::Set(obj, Nan::New("refreshRate").ToLocalChecked(), Nan::New(refreshRate));
        }
    } else {
        //QHD
        w = 640;
        h = 360;
    }

    //default size
    updateSize(w, h);

    //OpenGL information
    /* FIXME move to OpenGL init code
    v8::Local<v8::Object> obj = Nan::New<v8::Object>();

    Nan::Set(handle(), Nan::New("runtime").ToLocalChecked(), obj);

    // 1) OpenGL version
    Nan::Set(obj, Nan::New("renderer").ToLocalChecked(), Nan::New(std::string((char *)glGetString(GL_RENDERER))).ToLocalChecked());
    Nan::Set(obj, Nan::New("version").ToLocalChecked(), Nan::New(std::string((char *)glGetString(GL_VERSION))).ToLocalChecked());
    Nan::Set(obj, Nan::New("vendor").ToLocalChecked(), Nan::New(std::string((char *)glGetString(GL_VENDOR))).ToLocalChecked());

    // 2) texture size
    GLint maxTextureSize;

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    Nan::Set(obj, Nan::New("maxTextureSize").ToLocalChecked(), Nan::New(maxTextureSize));

    // 3) platform specific
    populateRuntimeProperties(obj);
    */
}

/**
 * Initialize the renderer.
 */
NAN_METHOD(AminoGfx::Start) {
    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    //validate state
    if (obj->startCallback || obj->started) {
        Nan::ThrowTypeError("already started");
        return;
    }

    //get callback
    Nan::Callback *callback = new Nan::Callback(info[0].As<v8::Function>());

    obj->startCallback = callback;

    //start (must call ready())
    obj->start();
}

void AminoGfx::start() {
    //overwrite
}

void AminoGfx::ready() {
    //create scope
    Nan::HandleScope scope;

    started = true;

    //call callback
    if (startCallback && !startCallback->IsEmpty()) {
        v8::Local<v8::Value> argv[] = { Nan::Null(), handle() };

        startCallback->Call(2, argv);
        delete startCallback;
        startCallback = NULL;
    }
}

/**
 * Stop rendering and free resources.
 */
NAN_METHOD(AminoGfx::Destroy) {
    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    if (obj->started) {
        obj->destroy();
    }
}

/**
 * Stop rendering and free resources.
 */
void AminoGfx::destroy() {
    //to be overwritten
    destroyed = true;
}

/**
 * JS wants to change width.
 */
NAN_METHOD(AminoGfx::SetW) {
    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    obj->requestW(info[0]->IntegerValue());
}

/**
 * JS wants to change height.
 */
NAN_METHOD(AminoGfx::SetH) {
    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    obj->requestH(info[0]->IntegerValue());
}

/**
 * Size has changed, update internal and JS properties.
 */
void AminoGfx::updateSize(int w, int h) {
    if (this->w != w) {
        this->w = w;

        updateProperty("w", w);
    }

    if (this->h != h) {
        this->h = h;

        updateProperty("h", h);
    }
}




void scale(double x, double y) {
    GLfloat scale[16];
    GLfloat temp[16];

    make_scale_matrix((float)x, (float)y, 1.0, scale);
    mul_matrix(temp, globaltx, scale);
    copy_matrix(globaltx, temp);
}

void translate(double x, double y) {
    GLfloat tr[16];
    GLfloat trans2[16];

    make_trans_matrix((float)x, (float)y, 0, tr);
    mul_matrix(trans2, globaltx, tr);
    copy_matrix(globaltx, trans2);
}

void rotate(double x, double y, double z) {
    GLfloat rot[16];
    GLfloat temp[16];

    make_x_rot_matrix(x, rot);
    mul_matrix(temp, globaltx, rot);
    copy_matrix(globaltx, temp);

    make_y_rot_matrix(y, rot);
    mul_matrix(temp, globaltx, rot);
    copy_matrix(globaltx, temp);

    make_z_rot_matrix(z, rot);
    mul_matrix(temp, globaltx, rot);
    copy_matrix(globaltx, temp);
}

void save() {
    GLfloat *temp = new GLfloat[16];

    copy_matrix(temp, globaltx);
    matrixStack.push(globaltx);
    globaltx = temp;
}

void restore() {
    delete globaltx;
    globaltx = (GLfloat*)matrixStack.top();
    matrixStack.pop();
}

// --------------------------------------------------------------- add_text ---
static void add_text( vertex_buffer_t *buffer, texture_font_t *font,
               wchar_t *text, vec2 *pen, int wrap, int width, int *lineNr )
{
    size_t len = wcslen(text);

    *lineNr = 1;

    size_t lineStart = 0; //start of current line
    size_t linePos = 0; //character pos current line
    float penXStart = pen->x;

    //debug
    //printf("add_text: wrap=%i width=%i\n", wrap, width);

    //add glyphs
    for (size_t i = 0; i < len; ++i) {
        wchar_t ch = text[i];
        texture_glyph_t *glyph = texture_font_get_glyph(font, ch);

        if (glyph) {
            //kerning
            int kerning = 0;

            if (linePos > 0) {
                kerning = texture_glyph_get_kerning(glyph, text[i - 1]);
            }

            //wrap
            if (wrap != WRAP_NONE) {
                bool newLine = false;
                bool skip = false;
                bool wordWrap = false;

                //check new line
                if (ch == '\n') {
                    //next line
                    newLine = true;
                    skip = true;
                } else if (pen->x + kerning + glyph->offset_x + glyph->width > width) {
                    //have to wrap

                    //next line
                    newLine = true;
                    wordWrap = wrap == WRAP_WORD;

                    //check space
                    if (iswspace(ch)) {
                        skip = true;
                    }
                }

                //check white space
                if (!skip && (newLine || lineStart == i) && isspace(ch)) {
                    skip = true;
                }

                //process
                if (newLine) {
                    linePos = 0;

                    //check word wrapping
                    bool wrapped = false;

                    if (wordWrap && !skip) {
                        //find white space
                        int wrapPos = -1;

                        for (size_t j = i - 1; j > lineStart; j--) {
                            if (iswspace(text[j])) {
                                wrapPos = j;
                                break;
                            }
                        }

                        if (wrapPos != -1) {
                            //calc vertex buffer offset
                            size_t count = vertex_buffer_size(buffer);
                            size_t start = count - (i - wrapPos);

                            //printf("remove %i of %i\n", (int)start, (int)count);
                            //printf("wrapping pos=%d char=%lc start=%d count=%d\n", i, ch, start, count);

                            //remove white space
                            vertex_buffer_erase(buffer, start);
                            count--;

                            //update existing glyphs
                            float xOffset = pen->x; //case: space before
                            float xLast = 0;

                            for (size_t j = start; j < count; j++) {
                                //glyph info
                                ivec4 *item = (ivec4 *)vector_get(buffer->items, j);
                                size_t vstart = item->x;
                                size_t vcount = item->y;

                                assert(vcount == 4);

                                //values
                                vertex_t *vertices = (vertex_t *)vector_get(buffer->vertices, vstart);

                                if (j == start) {
                                    xOffset = vertices->x - penXStart;

                                    //Note: kerning ignored (after space)
                                }

                                for (size_t k = 0; k < vcount; k++) {
                                    vertices->x -= xOffset;
                                    vertices->y -= font->height; //inverse coordinates

                                    vertices++;
                                }

                                xLast = vertices->x;
                                linePos++;
                            }

                            pen->x -= xOffset;
                            lineStart = i - linePos;
                            skip = false;

                            wrapped = true;
                        }
                    }

                    //wrap new character
                    if (!wrapped) {
                        pen->x = penXStart;
                        kerning = 0;
                        lineStart = i;
                    }

                    //debug
                    //printf("pen.x=%f lineStart=%lc\n", pen->x, text[lineStart]);

                    (*lineNr)++;
                    pen->y -= font->height; //inverse coordinates
                }

                if (skip) {
                    lineStart++;
                    continue;
                }
            }

            pen->x += kerning;

            //glyph position
            float x0  = ( pen->x + glyph->offset_x );
            float y0  = ( pen->y + glyph->offset_y );
            float x1  = ( x0 + glyph->width );
            float y1  = ( y0 - glyph->height );
            float s0 = glyph->s0;
            float t0 = glyph->t0;
            float s1 = glyph->s1;
            float t1 = glyph->t1;
            float advance = glyph->advance_x;

            //skip special characters
            if (ch == 0x9d) {
                //hide
                x1 = x0;
                y1 = y0;
                advance = 0;
            }

            GLushort indices[6] = {0,1,2, 0,2,3};
            vertex_t vertices[4] = { { x0,y0,0,  s0,t0 },
                                     { x0,y1,0,  s0,t1 },
                                     { x1,y1,0,  s1,t1 },
                                     { x1,y0,0,  s1,t0 } };

            //append
            vertex_buffer_push_back(buffer, vertices, 4, indices, 6);
            linePos++;

            //next
            pen->x += advance;
        } else {
            //debug
            //printf("not a glyph: %lc\n", ch);
        }
    }
}

/**
 * Update the rendered text.
 */
void TextNode::refreshText() {
    if (fontid == INVALID) {
        return;
    }

    //get font with size
    AminoFont *font = fontmap[fontid];
    std::map<int, texture_font_t *>::iterator it = font->fonts.find(fontsize);

    if (it == font->fonts.end()) {
        if (DEBUG_BASE) {
            printf("loading size %d for font %s\n", fontsize, font->filename);
        }

        //add new size
        font->fonts[fontsize] = texture_font_new(font->atlas, font->filename, fontsize);

        if (DEBUG_RESOURCES) {
            printf("created font texture (name=%s, size=%i)\n", font->filename, fontsize);
        }
    }

    //render text
    vec2 pen;

    pen.x = 0;
    pen.y = 0;

    wchar_t *t2 = const_cast<wchar_t *>(text.c_str());

    if (buffer) {
        vertex_buffer_clear(buffer);
    } else {
        buffer = vertex_buffer_new("vertex:3f,tex_coord:2f");
    }

    texture_font_t *f = font->fonts[fontsize];

    assert(f);
    add_text(buffer, f, t2, &pen, wrap, w, &lineNr);
}

NAN_METHOD(getTextLineCount) {
    int textHandle = info[0]->Uint32Value();
    TextNode *node = (TextNode *)rects[textHandle];

    info.GetReturnValue().Set(node->lineNr);
}

NAN_METHOD(getTextHeight) {
    int textHandle = info[0]->Uint32Value();
    TextNode *node = (TextNode *)rects[textHandle];
    AminoFont *font = fontmap[node->fontid];
    texture_font_t *texture = font->fonts[node->fontsize];

    info.GetReturnValue().Set(node->lineNr * texture->height);
}

NAN_METHOD(node_glCreateShader) {
  int type   = info[0]->Uint32Value();
  int shader = glCreateShader(type);

  info.GetReturnValue().Set(shader);
}

NAN_METHOD(node_glShaderSource) {
  int shader   = info[0]->Uint32Value();
  int count    = info[1]->Uint32Value();
  v8::String::Utf8Value jsource(info[2]->ToString());
  const char *source = *jsource;

  glShaderSource(shader, count, &source, NULL);
}

NAN_METHOD(node_glCompileShader) {
  int shader   = info[0]->Uint32Value();

  glCompileShader(shader);
}

NAN_METHOD(node_glGetShaderiv) {
  int shader   = info[0]->Uint32Value();
  int flag   = info[1]->Uint32Value();
  GLint status;

  glGetShaderiv(shader, flag, &status);

  info.GetReturnValue().Set(status);
}

NAN_METHOD(node_glGetProgramiv) {
  int prog   = info[0]->Uint32Value();
  int flag   = info[1]->Uint32Value();
  GLint status;

  glGetProgramiv(prog, flag, &status);

  info.GetReturnValue().Set(status);
}

NAN_METHOD(node_glGetShaderInfoLog) {
  int shader   = info[0]->Uint32Value();
  char buffer[513];

  glGetShaderInfoLog(shader, 512, NULL, buffer);

  info.GetReturnValue().Set(Nan::New(buffer, strlen(buffer)).ToLocalChecked());
}

NAN_METHOD(node_glGetProgramInfoLog) {
  int shader   = info[0]->Uint32Value();
  char buffer[513];

  glGetProgramInfoLog(shader, 512, NULL, buffer);

  info.GetReturnValue().Set(Nan::New(buffer, strlen(buffer)).ToLocalChecked());
}

NAN_METHOD(node_glCreateProgram) {
  int prog = glCreateProgram();

  info.GetReturnValue().Set(prog);
}

NAN_METHOD(node_glAttachShader) {
  int prog     = info[0]->Uint32Value();
  int shader   = info[1]->Uint32Value();

  glAttachShader(prog,shader);
}

NAN_METHOD(node_glLinkProgram) {
    int prog     = info[0]->Uint32Value();

    glLinkProgram(prog);
}

NAN_METHOD(node_glUseProgram) {
    int prog     = info[0]->Uint32Value();

    glUseProgram(prog);
}

NAN_METHOD(node_glGetAttribLocation) {
  int prog                 = info[0]->Uint32Value();
  v8::String::Utf8Value name(info[1]->ToString());
  int loc = glGetAttribLocation(prog, *name);

  info.GetReturnValue().Set(loc);
}

NAN_METHOD(node_glGetUniformLocation) {
    int prog                 = info[0]->Uint32Value();
    v8::String::Utf8Value name(info[1]->ToString());
    int loc = glGetUniformLocation(prog, *name);

    info.GetReturnValue().Set(loc);
}

NAN_METHOD(updateProperty) {
    int rectHandle   = info[0]->Uint32Value();
    int property     = info[1]->Uint32Value();

    if (DEBUG_BASE) {
        printf("updateProperty()\n");
    }

    float value = 0;
    std::wstring wstr = L"";
    std::vector<float> *arr = NULL;

    if (info[2]->IsNumber()) {
        value = info[2]->NumberValue();

        if (DEBUG_BASE) {
            printf("-> setting number %f on prop %d \n", value, property);
        }
    } else if (info[2]->IsBoolean()) {
        //convert boolean to 1 or 0
        value = info[2]->BooleanValue() ? 1:0;

        if (DEBUG_BASE) {
            printf("-> setting boolean %f on prop %d \n", value, property);
        }
    } else if (info[2]->IsString()) {
        wstr = GetWString(info[2]->ToString());

        if (DEBUG_BASE) {
            printf("-> string '%S' on prop %d\n", wstr.c_str(), property);
        }
    } else if (info[2]->IsArray()) {
        if (DEBUG_BASE) {
            printf("-> float array on prop %d\n", property);
        }
        //Note: has to be free'd
        arr = GetFloatArray(v8::Handle<v8::Array>::Cast(info[2]));
    } else {
        printf("unsupported property format: %i\n", property);
    }

    updates.push_back(new Update(RECT, rectHandle, property, value, wstr, arr, NULL));
}

NAN_METHOD(updateAnimProperty) {
    int rectHandle   = info[0]->Uint32Value();
    int property     = info[1]->Uint32Value();

    //value
    float value = 0;
    std::wstring wstr = L"";
    Nan::Callback *callback = NULL;

    if (DEBUG_BASE) {
        printf("updateAnimProperty()\n");
    }

    if (info[2]->IsNumber()) {
        value = info[2]->NumberValue();

        if (DEBUG_BASE) {
            printf("-> setting number %f on prop %d \n", value, property);
        }
    } else if (info[2]->IsBoolean()) {
        //convert boolean to 1 or 0
        value = info[2]->BooleanValue() ? 1:0;

        if (DEBUG_BASE) {
            printf("-> setting boolean %f on prop %d \n", value, property);
        }
    } else if (info[2]->IsString()) {
        if (DEBUG_BASE) {
            printf("-> trying to do a string\n");
        }

       char *cstr = TO_CHAR(info[2]);

       wstr = GetWC(cstr);
       free(cstr);
    } else if (info[2]->IsFunction()) {
        if (DEBUG_BASE) {
            printf("-> callback\n");
        }

        callback = new Nan::Callback(info[2].As<v8::Function>());
    } else if (info[2]->IsNull() || info[2]->IsUndefined()) {
        if (DEBUG_BASE) {
            printf("-> callback (null)\n");
        }
    } else {
        printf("unsupported anim property format: %i\n", property);
    }

    updates.push_back(new Update(ANIM, rectHandle, property, value, wstr, NULL, callback));
}

NAN_METHOD(updateWindowProperty) {
    if (DEBUG_BASE) {
        printf("updateWindowProperty()\n");
    }

    int windowHandle   = info[0]->Uint32Value();
    int property       = info[1]->Uint32Value();
    float value = 0;
    std::wstring wstr = L"";

    if (info[2]->IsNumber()) {
        if (DEBUG_BASE) {
            printf("-> updating window property %d \n", property);
        }

        value = info[2]->Uint32Value();
    } else if (info[2]->IsString()) {
        char *cstr = TO_CHAR(info[2]);

        wstr = GetWC(cstr);
        free(cstr);
    } else {
        printf("unsupported window property format: %i\n", property);
    }

    updates.push_back(new Update(WINDOW, windowHandle, property, value, wstr, NULL, NULL));
}

NAN_METHOD(addNodeToGroup) {
    int rectHandle   = info[0]->Uint32Value();
    int groupHandle  = info[1]->Uint32Value();

    //update group
    Group *group = (Group *)rects[groupHandle];
    AminoNode *node = rects[rectHandle];

    group->children.push_back(node);
}

NAN_METHOD(removeNodeFromGroup) {
    int rectHandle   = info[0]->Uint32Value();
    int groupHandle  = info[1]->Uint32Value();
    Group *group = (Group*)rects[groupHandle];
    AminoNode *node = rects[rectHandle];
    int n = -1;

    //TODO simplify
    for (std::size_t i = 0; i < group->children.size(); i++) {
        if (group->children[i] == node) {
            n = i;
        }
    }

    group->children.erase(group->children.begin() + n);
}

NAN_METHOD(setRoot) {
    rootHandle = info[0]->Uint32Value();
}

NAN_METHOD(loadBufferToTexture) {
    int texid = info[0]->Uint32Value();
    int w     = info[1]->Uint32Value();
    int h     = info[2]->Uint32Value();
    int bpp   = info[3]->Uint32Value(); // this is *bytes* per pixel. usually 3 or 4

    //printf("got w=%d h=%d bpp=%d\n", w, h, bpp);

    //buffer
    Local<Object> bufferObj = info[4]->ToObject();
    char *bufferData = Buffer::Data(bufferObj);
    size_t bufferLength = Buffer::Length(bufferObj);

    //printf("buffer length = %d\n", bufferLength);

    assert(w * h * bpp == (int)bufferLength);

    GLuint texture;

    if (texid >= 0) {
        //use existing texture
	    texture = texid;
    } else {
        //create new texture
	    glGenTextures(1, &texture);
    }

    //FIXME glDeleteTextures(1, &texture) never called
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (bpp == 3) {
        //RGB (24-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, bufferData);
    } else if (bpp == 4) {
        //RGBA (32-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferData);
    } else if (bpp == 1) {
        //grayscale (8-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bufferData);
    } else {
        //unsupported
        printf("unsupported texture format: bpp=%d\n", bpp);
    }

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    //create object
    v8::Local<v8::Object> obj = Nan::New<v8::Object>();

    Nan::Set(obj, Nan::New("texid").ToLocalChecked(), Nan::New(texture));
    Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
    Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

    info.GetReturnValue().Set(obj);
}

/**
 * Get font texture.
 *
 * Note: creates font size if it did not exist before.
 */
texture_font_t* getFontTexture(int index, int size) {
    AminoFont *font = fontmap[index];

    assert(font);

    std::map<int, texture_font_t *>::iterator it = font->fonts.find(size);

    if (it == font->fonts.end()) {
        //create font size
        if (DEBUG_RESOURCES) {
            printf("Font is missing glyphs for size %d\n", size);
            printf("loading size %d for font %s\n", size, font->filename);
        }

        font->fonts[size] = texture_font_new(font->atlas, font->filename, size);
    }

    return font->fonts[size];
}

/**
 * Get the font height.
 */
NAN_METHOD(getFontHeight) {
    int fontsize   = info[0]->Uint32Value();
    int fontindex  = info[1]->Uint32Value();

    //get font
    texture_font_t *tf = getFontTexture(fontindex, fontsize);

    info.GetReturnValue().Set(tf->ascender - tf->descender);
}

NAN_METHOD(getFontAscender) {
    int fontsize   = info[0]->Uint32Value();
    int fontindex  = info[1]->Uint32Value();

    //get font
    texture_font_t *tf = getFontTexture(fontindex, fontsize);

    info.GetReturnValue().Set(tf->ascender);
}

NAN_METHOD(getFontDescender) {
    int fontsize   = info[0]->Uint32Value();
    int fontindex  = info[1]->Uint32Value();

    //get font
    texture_font_t *tf = getFontTexture(fontindex, fontsize);

    info.GetReturnValue().Set(tf->descender);
}

NAN_METHOD(getCharWidth) {
    std::wstring wstr = GetWString(info[0]->ToString());
    int fontsize  = info[1]->Uint32Value();
    int fontindex = info[2]->Uint32Value();

    //get font
    texture_font_t *tf = getFontTexture(fontindex, fontsize);
    float w = 0;

    //length seems to include the null string
    std::size_t len = wstr.length();

    for (std::size_t i = 0; i < len; i++) {
        wchar_t ch  = wstr.c_str()[i]; //TODO cache

        //skip null terminators
        if (ch == '\0') {
            continue;
        }

        //FIXME kerning
        texture_glyph_t *glyph = texture_font_get_glyph(tf, wstr.c_str()[i]);

        if (glyph == 0) {
            printf("WARNING. Got empty glyph from texture_font_get_glyph");
        }

        w += glyph->advance_x;
    }

    info.GetReturnValue().Set(w);
}

/**
 * Create native font and shader.
 */
NAN_METHOD(createNativeFont) {
    if (DEBUG_BASE) {
        printf("createNativeFont()\n");
    }

    //create font object
    AminoFont *afont = new AminoFont();

    //store
    int id = fontmap.size();

    fontmap[id] = afont;

    //load font & shader
    afont->filename = TO_CHAR(info[0]);

    char *shader_base = TO_CHAR(info[1]);

    if (DEBUG_BASE) {
        printf("loading font file %s\n", afont->filename);
        printf("shader base = %s\n", shader_base);
    }

    std::string str ("");
    std::string str2 = str + shader_base;
    std::string vert = str2 + "/v3f-t2f.vert";
    std::string frag = str2 + "/v3f-t2f.frag";

    //printf("shader: vertex=%s fragment=%s\n", vert.c_str(), frag.c_str());

    free(shader_base);

    afont->atlas = texture_atlas_new(512, 512, 1);
    afont->shader = shader_load(vert.c_str(), frag.c_str());

    if (DEBUG_RESOURCES) {
        printf("created font shader\n");
    }

    //TODO reuse shader (create only once)
    //TODO glDeleteProgram (on destroy)

    info.GetReturnValue().Set(id);
}

/**
 * Set color shader bindings.
 */
NAN_METHOD(initColorShader) {
    if (info.Length() < 5) {
        printf("initColorShader: not enough args\n");
        exit(1);
    };

    colorShader->prog        = info[0]->Uint32Value();
    colorShader->u_matrix    = info[1]->Uint32Value();
    colorShader->u_trans     = info[2]->Uint32Value();
    colorShader->u_opacity   = info[3]->Uint32Value();

    colorShader->attr_pos    = info[4]->Uint32Value();
    colorShader->attr_color  = info[5]->Uint32Value();
}

/**
 * Set texture shader bindings.
 */
NAN_METHOD(initTextureShader) {
    if (info.Length() < 6) {
        printf("initTextureShader: not enough args\n");
        exit(1);
    };

    textureShader->prog        = info[0]->Uint32Value();
    textureShader->u_matrix    = info[1]->Uint32Value();
    textureShader->u_trans     = info[2]->Uint32Value();
    textureShader->u_opacity   = info[3]->Uint32Value();

    textureShader->attr_pos       = info[4]->Uint32Value();
    textureShader->attr_texcoords = info[5]->Uint32Value();
}

NAN_METHOD(createRect) {
    Rect *rect = new Rect(info[0]->BooleanValue());

    rects.push_back(rect);

    //return id
    info.GetReturnValue().Set((int)rects.size() - 1);
}

NAN_METHOD(createPoly) {
    PolyNode *node = new PolyNode();

    rects.push_back(node);

    //return id
    info.GetReturnValue().Set((int)rects.size() - 1);
}

NAN_METHOD(createText) {
    TextNode *node = new TextNode();

    rects.push_back(node);

    //return id
    info.GetReturnValue().Set((int)rects.size() - 1);
}

NAN_METHOD(createGroup) {
    Group *node = new Group();

    rects.push_back(node);

    //return id
    info.GetReturnValue().Set((int)rects.size() - 1);
}

NAN_METHOD(createGLNode) {
    GLNode *node = new GLNode();

    //node->callback = Persistent<Function>::New(Handle<Function>::Cast(args[0]));
    rects.push_back(node);

    //return id
    info.GetReturnValue().Set((int)rects.size() - 1);
}

/**
 * Create animation handler.
 *
 * createAnim(rect, property, startm end, duration)
 */
NAN_METHOD(createAnim) {
    int rectHandle   = info[0]->Uint32Value();
    int property     = info[1]->Uint32Value();
    float start      = info[2]->NumberValue();
    float end        = info[3]->NumberValue();
    float duration   = info[4]->Uint32Value();

    Anim *anim = new Anim(rects[rectHandle], property, start, end, duration);

    //add to collection
    anims.push_back(anim);

    anim->id = anims.size() - 1;
    anim->active = true;

    //return id
    info.GetReturnValue().Set(anim->id);
}

/**
 * Stop animation.
 */
NAN_METHOD(stopAnim) {
	int id = info[0]->Uint32Value();

    //deactivate the animation
	Anim *anim = anims[id];

	anim->active = false;
}

//FIXME animations are never removed
//FIXME animation id handling is not flexible enough