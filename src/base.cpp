#include "base.h"

#include "SimpleRenderer.h"
#include "fonts/utf8-utils.h"

//
//  AminoGfx
//

AminoGfx::AminoGfx(std::string name): AminoJSEventObject(name) {
    //empty
}

AminoGfx::~AminoGfx() {
    if (startCallback) {
        delete startCallback;
    }

    //Note: properties are deleted by base class destructor
}

/**
 * Initialize AminoGfx template.
 */
void AminoGfx::Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target, AminoJSObjectFactory* factory) {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(factory);

    //prototype methods

    // basic
    Nan::SetPrototypeMethod(tpl, "_start", Start);
    Nan::SetPrototypeMethod(tpl, "_destroy", Destroy);
    Nan::SetPrototypeMethod(tpl, "tick", Tick);

    // shader
    Nan::SetPrototypeMethod(tpl, "initColorShader", InitColorShader);
    Nan::SetPrototypeMethod(tpl, "initTextureShader", InitTextureShader);
    Nan::SetPrototypeMethod(tpl, "initFontShader", InitFontShader);

    // group
    Nan::SetPrototypeMethod(tpl, "_setRoot", SetRoot);
    Nan::SetTemplate(tpl, "Group", AminoGroup::GetInitFunction());

    // other
    Nan::SetTemplate(tpl, "Rect", AminoRect::GetRectInitFunction());
    Nan::SetTemplate(tpl, "ImageView", AminoRect::GetImageViewInitFunction());
    Nan::SetTemplate(tpl, "Texture", AminoTexture::GetInitFunction());
    Nan::SetTemplate(tpl, "Polygon", AminoPolygon::GetInitFunction());
    Nan::SetTemplate(tpl, "Text", AminoText::GetInitFunction());
    Nan::SetTemplate(tpl, "Anim", AminoAnim::GetInitFunction());

    //special: GL object
    Nan::SetTemplate(tpl, "GL", createGLObject());

    //global template instance
    v8::Local<v8::Function> func = Nan::GetFunction(tpl).ToLocalChecked();

    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), func);
}

/**
 * JS object was initalized.
 */
void AminoGfx::setup() {
    //register native properties
    propX = createFloatProperty("x");
    propY = createFloatProperty("y");
    propW = createFloatProperty("w");
    propH = createFloatProperty("h");
    propR = createFloatProperty("r");
    propG = createFloatProperty("g");
    propB = createFloatProperty("b");
    propOpacity = createFloatProperty("opacity");
    propTitle = createUtf8Property("title");

    //screen size
    int w, h, refreshRate;
    bool fullscreen;

    if (getScreenInfo(w, h, refreshRate, fullscreen)) {
        v8::Local<v8::Object> obj = Nan::New<v8::Object>();

        //add screen property
        Nan::Set(handle(), Nan::New("screen").ToLocalChecked(), obj);

        //properties
        Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

        if (refreshRate > 0) {
            Nan::Set(obj, Nan::New("refreshRate").ToLocalChecked(), Nan::New(refreshRate));
        }

        Nan::Set(obj, Nan::New("fullscreen").ToLocalChecked(), Nan::New<v8::Boolean>(fullscreen));

        //screen size
        if (fullscreen) {
            //fixed size
            updateSize(w, h);
        }
    }
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

    //init renderer context
    obj->initRenderer();
    obj->setupRenderer();

    //runtime info
    obj->addRuntimeProperty();

    //start (must call ready())
    obj->start();
}

/**
 * Initialize the renderer (init OpenGL context).
 */
void AminoGfx::initRenderer() {
    if (DEBUG_BASE) {
        printf("-> initRenderer()\n");
    }

    //empty: overwrite
    viewportW = propW->value;
    viewportH = propH->value;
}

/**
 * Shader and matrix setup.
 */
void AminoGfx::setupRenderer() {
    if (DEBUG_BASE) {
        printf("-> setupRenderer()\n");
    }

    //init values
	colorShader = new ColorShader();
	textureShader = new TextureShader();

    modelView = new GLfloat[16];
}

/**
 * Set color shader bindings.
 */
NAN_METHOD(AminoGfx::InitColorShader) {
    if (DEBUG_BASE) {
        printf("-> InitColorShader()\n");
    }

    if (info.Length() < 5) {
        printf("initColorShader: not enough args\n");
        exit(1);
    };

    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());
    ColorShader *colorShader = obj->colorShader;

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
NAN_METHOD(AminoGfx::InitTextureShader) {
    if (DEBUG_BASE) {
        printf("-> InitTextureShader()\n");
    }

    if (info.Length() < 6) {
        printf("initTextureShader: not enough args\n");
        exit(1);
    };

    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());
    TextureShader *textureShader = obj->textureShader;

    textureShader->prog        = info[0]->Uint32Value();
    textureShader->u_matrix    = info[1]->Uint32Value();
    textureShader->u_trans     = info[2]->Uint32Value();
    textureShader->u_opacity   = info[3]->Uint32Value();

    textureShader->attr_pos       = info[4]->Uint32Value();
    textureShader->attr_texcoords = info[5]->Uint32Value();
}

NAN_METHOD(AminoGfx::InitFontShader) {
    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());
    v8::String::Utf8Value path(info[0]);

    assert(!obj->fontShader);

    obj->fontShader = new AminoFontShader(*path);
}

/**
 * Adds runtime property.
 */
void AminoGfx::addRuntimeProperty() {
    //OpenGL information
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

    // 3) texture units
    GLint maxTextureImageUnits;

    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);
    Nan::Set(obj, Nan::New("maxTextureImageUnits").ToLocalChecked(), Nan::New(maxTextureImageUnits));

    // 3) platform specific
    populateRuntimeProperties(obj);
}

/**
 * Add runtime specific properties.
 */
void AminoGfx::populateRuntimeProperties(v8::Local<v8::Object> &obj) {
    //empty: overwrite
}

/**
 * Start render cycle.
 *
 * Note: has to call ready.
 */
void AminoGfx::start() {
    //overwrite
}

/**
 * Ready to call JS start() callback.
 */
void AminoGfx::ready() {
    if (DEBUG_BASE) {
        printf("-> ready()\n");
    }

    //create scope
    Nan::HandleScope scope;

    started = true;

    //call callback
    if (startCallback) {
        if (!startCallback->IsEmpty()) {
            v8::Local<v8::Object> obj = handle();
            v8::Local<v8::Value> argv[] = { Nan::Null(), obj };

            startCallback->Call(obj, 2, argv);
        }

        //free callback
        delete startCallback;
        startCallback = NULL;
    }

    if (DEBUG_BASE) {
        printf("-> started\n");
    }

    //TODO create rendering thread cbx
}

/**
 * System has time to render a new scene.
 */
NAN_METHOD(AminoGfx::Tick) {
    //TODO check fps
    //TODO reduce updates to refreshRate
    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    obj->render();
}

/**
 * Render a scene (synchronous call).
 */
void AminoGfx::render() {
    bindContext();
    rendering = true;

    //updates
    processAsyncQueue();
    processAnimations();

    //viewport
    setupViewport();

    //root
    renderScene();

    //done
    renderingDone();
    rendering = false;

    //cbx move to main thread
    handleAsyncDeletes();
}

/**
 * Update all animated values.
 */
void AminoGfx::processAnimations() {
    double currentTime = getTime();
    int count = animations.size();

    for (int i = 0; i < count; i++) {
        animations[i]->update(currentTime);
    }
}

/**
 * Check if rendering scene right now.
 */
bool AminoGfx::isRendering() {
    return rendering;
}

void AminoGfx::setupViewport() {
    //set up the viewport (y-inversion, top-left origin)
    float width = propW->value;
    float height = propH->value;

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

    //prepare
    glViewport(0, 0, viewportW, viewportH);
    glClearColor(propR->value, propG->value, propB->value, propOpacity->value);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
}

/**
 * Render the root node and all its children.
 */
void AminoGfx::renderScene() {
    if (!root) {
        return;
    }

    SimpleRenderer *renderer = new SimpleRenderer(fontShader, colorShader, textureShader, modelView);

    renderer->startRender(root);
    delete renderer;
}

/**
 * Stop rendering and free resources.
 */
NAN_METHOD(AminoGfx::Destroy) {
    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    if (obj->started && !obj->destroyed) {
        obj->destroy();
    }
}

/**
 * Stop rendering and free resources.
 *
 * Note: has to run on main thread.
 */
void AminoGfx::destroy() {
    //cbx join renderer thread

    //bind context
    bindContext();

    //renderer (shader programs)
    if (colorShader) {
        colorShader->destroy();
        delete colorShader;
        colorShader = NULL;

        textureShader->destroy();
        delete textureShader;
        textureShader = NULL;

        delete[] modelView;
        modelView = NULL;
    }

    //unbind root
    setRoot(NULL);

    //free async
    clearAsyncQueue();
    handleAsyncDeletes();
}

/**
 * Size has changed, update internal and JS properties.
 *
 * Note: has to be called after window size has changed. Does not touch the window size.
 */
void AminoGfx::updateSize(int w, int h) {
    propW->setValue(w);
    propH->setValue(h);
}

/**
 * Position has changed, update internal and JS properties.
 *
 * Note: has to be called after window position has changed. Does not touch the window position.
 */
void AminoGfx::updatePosition(int x, int y) {
    propX->setValue(x);
    propY->setValue(y);
}

/**
 * Fire runtime specific event.
 *
 * Note: has to be called on main thread!
 */
void AminoGfx::fireEvent(v8::Local<v8::Object> &evt) {
    //event handler
    v8::Local<v8::Object> obj = handle();
    Nan::MaybeLocal<v8::Value> handleEventMaybe = Nan::Get(obj, Nan::New<v8::String>("handleEvent").ToLocalChecked());

    if (!handleEventMaybe.IsEmpty()) {
        v8::Local<v8::Value> value = handleEventMaybe.ToLocalChecked();

        if (value->IsFunction()) {
            v8::Local<v8::Function> func = value.As<v8::Function>();

            //call
            int argc = 1;
            v8::Local<v8::Value> argv[1] = { evt };

            func->Call(obj, argc, argv);
        }
    }
}

/**
 * Handle sync property updates.
 */
bool AminoGfx::handleSyncUpdate(AnyProperty *property, void *data) {
    //debug
    //printf("handleSyncUpdate() %s\n", property->name.c_str());

    //default: set value
    AminoJSObject::handleSyncUpdate(property, data);

    //size changes
    if (property == propW || property == propH) {
        property->setAsyncData(NULL, data);
        updateWindowSize();

        return true;
    }

    //position changes

    if (property == propX || property == propY) {
        property->setAsyncData(NULL, data);
        updateWindowPosition();

        return true;
    }

    //title
    if (property == propTitle) {
        property->setAsyncData(NULL, data);
        updateWindowTitle();

        return true;
    }

    return false;
}

/**
 * Create OpenGL bindings for JS.
 */
v8::Local<v8::Object> AminoGfx::createGLObject() {
    v8::Local<v8::Object> obj = Nan::New<v8::Object>();

    //functions
	Nan::Set(obj, Nan::New("glCreateShader").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glCreateShader)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glShaderSource").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glShaderSource)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glCompileShader").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glCompileShader)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glGetShaderiv").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glGetShaderiv)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glCreateProgram").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glCreateProgram)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glAttachShader").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glAttachShader)).ToLocalChecked());
    Nan::Set(obj, Nan::New("glDetachShader").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glDetachShader)).ToLocalChecked());
    Nan::Set(obj, Nan::New("glDeleteShader").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glDeleteShader)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glUseProgram").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glUseProgram)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glLinkProgram").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glLinkProgram)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glGetProgramiv").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glGetProgramiv)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glGetAttribLocation").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glGetAttribLocation)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glGetUniformLocation").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glGetUniformLocation)).ToLocalChecked());
	Nan::Set(obj, Nan::New("glGetProgramInfoLog").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(node_glGetProgramInfoLog)).ToLocalChecked());

    //constants
    Nan::Set(obj, Nan::New("GL_VERTEX_SHADER").ToLocalChecked(),    Nan::New(GL_VERTEX_SHADER));
	Nan::Set(obj, Nan::New("GL_FRAGMENT_SHADER").ToLocalChecked(),  Nan::New(GL_FRAGMENT_SHADER));
	Nan::Set(obj, Nan::New("GL_COMPILE_STATUS").ToLocalChecked(),   Nan::New(GL_COMPILE_STATUS));
	Nan::Set(obj, Nan::New("GL_LINK_STATUS").ToLocalChecked(),      Nan::New(GL_LINK_STATUS));

    return obj;
}

NAN_METHOD(AminoGfx::SetRoot) {
    //new value
    AminoGroup *group;

    if (info[0]->IsNull() || info[0]->IsUndefined()) {
        group = NULL;
    } else {
        group = Nan::ObjectWrap::Unwrap<AminoGroup>(info[0]->ToObject());
    }

    AminoGfx *obj= Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    obj->setRoot(group);
}

void AminoGfx::setRoot(AminoGroup *group) {
    //validate
    if (group && !group->checkRenderer(this)) {
        return;
    }

    //free old one
    if (root) {
        root->release();
    }

    //set
    root = group;

    if (group) {
        group->retain();
    }
}

/**
 * Add animation.
 *
 * Note: called on main thread.
 */
bool AminoGfx::addAnimationAsync(AminoAnim *anim) {
    if (destroyed) {
        return false;
    }

    //retains anim instance
    AminoJSObject::enqueueValueUpdate(anim, (asyncValueCallback)&AminoGfx::addAnimation);

    return true;
}

/**
 * Add an animation.
 *
 * Note: called on OpenGL thread.
 */
void AminoGfx::addAnimation(AsyncValueUpdate *update, int state) {
    if (state != AsyncValueUpdate::STATE_APPLY) {
        return;
    }

    AminoAnim *anim = (AminoAnim *)update->valueObj;

    //keep retained instance
    update->valueObj = NULL;

    animations.push_back(anim);
}

/**
 * Remove animation.
 *
 * Note: called on main thread.
 */
void AminoGfx::removeAnimationAsync(AminoAnim *anim) {
    if (destroyed) {
        return;
    }

    //so far retains another instance
    AminoJSObject::enqueueValueUpdate(anim, (asyncValueCallback)&AminoGfx::removeAnimation);
}

/**
 * Remove animation.
 */
void AminoGfx::removeAnimation(AsyncValueUpdate *update, int state) {
    if (state != AsyncValueUpdate::STATE_APPLY) {
        return;
    }

    AminoAnim *anim = (AminoAnim *)update->valueObj;
    std::vector<AminoAnim *>::iterator pos = std::find(animations.begin(), animations.end(), anim);

    if (pos != animations.end()) {
        animations.erase(pos);

        //free instance
        update->releaseLater = anim;
    }
}

/**
 * Delete texture.
 *
 * Note: has to be called on main thread.
 */
void AminoGfx::deleteTextureAsync(GLuint textureId) {
    if (destroyed) {
        return;
    }

    //enqueue
    AminoJSObject::enqueueValueUpdate(textureId, (asyncValueCallback)&AminoGfx::deleteTexture);
}

/**
 * Delete texture.
 */
void AminoGfx::deleteTexture(AsyncValueUpdate *update, int state) {
    if (state != AsyncValueUpdate::STATE_APPLY) {
        return;
    }

    GLuint textureId = update->valueUint32;

    if (DEBUG_RESOURCES) {
        printf("-> deleting texture %i\n", textureId);
    }

    glDeleteTextures(1, &textureId);
}

/**
 * Get texture for atlas.
 */
amino_atlas_t AminoGfx::getAtlasTexture(texture_atlas_t *atlas) {
    assert(fontShader);

    return fontShader->getAtlasTexture(atlas);
}

//
// AminoGroupFactory
//

/**
 * Group factory constructor.
 */
AminoGroupFactory::AminoGroupFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoGroup", callback) {
    //empty
}

AminoJSObject* AminoGroupFactory::create() {
    return new AminoGroup();
}

//
// AminoRectFactory
//

/**
 * AminoRect factory constructor.
 */
AminoRectFactory::AminoRectFactory(Nan::FunctionCallback callback, bool hasImage): AminoJSObjectFactory(hasImage ? "AminoImageView":"AminoRect", callback), hasImage(hasImage) {
    //empty
}

AminoJSObject* AminoRectFactory::create() {
    return new AminoRect(hasImage);
}

//
// AminoPolygonFactory
//

/**
 * AminoPolygon factory constructor.
 */
AminoPolygonFactory::AminoPolygonFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoPolygon", callback) {
    //empty
}

AminoJSObject* AminoPolygonFactory::create() {
    return new AminoPolygon();
}

//
// AminoAnimFactory
//

/**
 * Animation factory constructor.
 */
AminoAnimFactory::AminoAnimFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoAnim", callback) {
    //empty
}

AminoJSObject* AminoAnimFactory::create() {
    return new AminoAnim();
}

//
// AminoTextFactory
//

/**
 * Text factory constructor.
 */
AminoTextFactory::AminoTextFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoText", callback) {
    //empty
}

AminoJSObject* AminoTextFactory::create() {
    return new AminoText();
}

//
// AminoText
//

/**
 * Update texture (if modified).
 */
GLuint AminoText::updateTexture() {
    assert(fontSize);
    assert(fontSize->fontTexture);
    assert(fontSize->fontTexture->atlas);

    texture_atlas_t *atlas = fontSize->fontTexture->atlas;

    assert(atlas->depth == 1);

    if (texture.textureId == INVALID_TEXTURE) {
        //create texture (for atlas)
        texture_atlas_t *atlas = fontSize->fontTexture->atlas;

        texture = getAminoGfx()->getAtlasTexture(atlas);

        assert(texture.textureId != INVALID_TEXTURE);
    } else {
        if (texture.lastGlyphUpdate == fontSize->fontTexture->glyphs->size) {
            return texture.textureId;
        }
    }

    //update texture
    if (DEBUG_BASE) {
        printf("-> updateTexture()\n");
    }

    glBindTexture(GL_TEXTURE_2D, texture.textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlas->width, atlas->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas->data);

    texture.lastGlyphUpdate = fontSize->fontTexture->glyphs->size;

    //printf("font texture updated\n");
    //printf("updateTexture() done\n");

    return texture.textureId;
}

/**
 * Render text to vertices.
 */
static void add_text(vertex_buffer_t *buffer, texture_font_t *font,
               const char *text, vec2 *pen, int wrap, int width, int *lineNr) {
    //see https://github.com/rougier/freetype-gl/blob/master/demos/glyph.c
    size_t len = utf8_strlen(text);

    *lineNr = 1;

    size_t lineStart = 0; //start of current line
    size_t linePos = 0; //character pos current line
    float penXStart = pen->x;

    //debug
    //printf("add_text: wrap=%i width=%i\n", wrap, width);

    //add glyphs (by iterating Unicode characters)
    char *textPos = (char *)text;
    char *lastTextPos = NULL;
    uint32_t textUtf32[len];

    for (size_t i = 0; i < len; ++i) {
        texture_glyph_t *glyph = texture_font_get_glyph(font, textPos);

        if (glyph) {
            //store
            textUtf32[i] = glyph->codepoint;

            //kerning
            int kerning = 0;

            if (linePos > 0) {
                kerning = texture_glyph_get_kerning(glyph, lastTextPos);
            }

            //wrap
            if (wrap != AminoText::WRAP_NONE) {
                bool newLine = false;
                bool skip = false;
                bool wordWrap = false;

                //check new line
                if (glyph->codepoint == '\n') {
                    //next line
                    newLine = true;
                    skip = true;
                } else if (pen->x + kerning + glyph->offset_x + glyph->width > width) {
                    //have to wrap

                    //next line
                    newLine = true;
                    wordWrap = wrap == AminoText::WRAP_WORD;

                    //check space
                    if (iswspace((wchar_t)glyph->codepoint)) {
                        skip = true;
                    }
                }

                //check white space
                if (!skip && (newLine || lineStart == i) && iswspace(glyph->codepoint)) {
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
                            if (iswspace((wchar_t)textUtf32[j])) {
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
            if (glyph->codepoint == 0x9d) {
                //hide
                x1 = x0;
                y1 = y0;
                advance = 0;
            }

            GLuint indices[6] = {0,1,2, 0,2,3};
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
            //not enough space for glyph

            //store
            uint32_t ch32 = utf8_to_utf32(textPos);

            textUtf32[i] = ch32;

            //show error
            printf("no space for glyph: %lc\n", (wchar_t)ch32);
        }

        //next glyph pos
        size_t charLen = utf8_surrogate_len(textPos);

        lastTextPos = textPos;
        textPos += charLen;
    }
}

/**
 * Update the rendered text.
 */
bool AminoText::layoutText() {
    //printf("layoutText()\n");

    if (!fontSize) {
        //printf("-> no font\n");

        return false;
    }

    if (!updated) {
        //printf("layoutText() done\n");

        return true;
    }

    updated = false;

    //render text
    if (buffer) {
        vertex_buffer_clear(buffer);
    } else {
        buffer = vertex_buffer_new("vertex:3f,tex_coord:2f");
    }

    texture_font_t *f = fontSize->fontTexture;

    assert(f);

    vec2 pen;

    pen.x = 0;
    pen.y = 0;

    add_text(buffer, f, propText->value.c_str(), &pen, wrap, propW->value, &lineNr);

    if (DEBUG_BASE) {
        printf("-> layoutText() done\n");
    }

    return true;
}

NAN_METHOD(node_glCreateShader) {
  int type   = info[0]->Uint32Value();
  int shader = glCreateShader(type);

  info.GetReturnValue().Set(shader);
}

NAN_METHOD(node_glShaderSource) {
  int shader = info[0]->Uint32Value();
  int count  = info[1]->Uint32Value();
  v8::String::Utf8Value jsource(info[2]);
  const char *source = *jsource;

  glShaderSource(shader, count, &source, NULL);
}

NAN_METHOD(node_glCompileShader) {
  int shader = info[0]->Uint32Value();

  glCompileShader(shader);
}

NAN_METHOD(node_glGetShaderiv) {
  int shader = info[0]->Uint32Value();
  int flag   = info[1]->Uint32Value();
  GLint status;

  glGetShaderiv(shader, flag, &status);

  info.GetReturnValue().Set(status);
}

NAN_METHOD(node_glGetProgramiv) {
  int prog = info[0]->Uint32Value();
  int flag = info[1]->Uint32Value();
  GLint status;

  glGetProgramiv(prog, flag, &status);

  info.GetReturnValue().Set(status);
}

NAN_METHOD(node_glGetShaderInfoLog) {
  int shader = info[0]->Uint32Value();
  char buffer[513];

  glGetShaderInfoLog(shader, 512, NULL, buffer);

  info.GetReturnValue().Set(Nan::New(buffer, strlen(buffer)).ToLocalChecked());
}

NAN_METHOD(node_glGetProgramInfoLog) {
  int shader = info[0]->Uint32Value();
  char buffer[513];

  glGetProgramInfoLog(shader, 512, NULL, buffer);

  info.GetReturnValue().Set(Nan::New(buffer, strlen(buffer)).ToLocalChecked());
}

NAN_METHOD(node_glCreateProgram) {
  int prog = glCreateProgram();

  info.GetReturnValue().Set(prog);
}

NAN_METHOD(node_glAttachShader) {
  int prog   = info[0]->Uint32Value();
  int shader = info[1]->Uint32Value();

  glAttachShader(prog, shader);
}

NAN_METHOD(node_glDetachShader) {
  int prog   = info[0]->Uint32Value();
  int shader = info[1]->Uint32Value();

  glDetachShader(prog, shader);
}

NAN_METHOD(node_glDeleteShader) {
  int shader = info[0]->Uint32Value();

  glDeleteShader(shader);
}

NAN_METHOD(node_glLinkProgram) {
    GLuint prog = info[0]->Uint32Value();

    glLinkProgram(prog);
}

NAN_METHOD(node_glUseProgram) {
    GLuint prog = info[0]->Uint32Value();

    glUseProgram(prog);
}

NAN_METHOD(node_glGetAttribLocation) {
  int prog = info[0]->Uint32Value();
  v8::String::Utf8Value name(info[1]);
  int loc = glGetAttribLocation(prog, *name);

  info.GetReturnValue().Set(loc);
}

NAN_METHOD(node_glGetUniformLocation) {
    int prog = info[0]->Uint32Value();
    v8::String::Utf8Value name(info[1]);
    int loc = glGetUniformLocation(prog, *name);

    info.GetReturnValue().Set(loc);
}
