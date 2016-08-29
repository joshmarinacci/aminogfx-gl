#include "base.h"

#include <cwctype>
#include <algorithm>

#include "renderer.h"
#include "fonts/utf8-utils.h"

#define DEBUG_RENDERER false
//cbx deactivate later on
#define DEBUG_RENDERER_ERRORS true
#define DEBUG_FONT_TEXTURE false
//cbx deactivate later on
#define DEBUG_FPS true

//
//  AminoGfx
//

AminoGfx::AminoGfx(std::string name): AminoJSEventObject(name) {
    //empty
}

AminoGfx::~AminoGfx() {
    if (startCallback) {
        delete startCallback;
        startCallback = NULL;
    }

    //Note: properties are deleted by base class destructor
}

/**
 * Initialize basic amino classes.
 */
NAN_MODULE_INIT(AminoGfx::InitClasses) {
    //image class
    AminoImage::Init(target);

    //fonts class
    AminoFonts::Init(target);

    //weak reference class
    AminoJSWeakReference::Init(target);
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

    // group
    Nan::SetPrototypeMethod(tpl, "_setRoot", SetRoot);
    Nan::SetTemplate(tpl, "Group", AminoGroup::GetInitFunction());

    // primitives
    Nan::SetTemplate(tpl, "Rect", AminoRect::GetRectInitFunction());
    Nan::SetTemplate(tpl, "ImageView", AminoRect::GetImageViewInitFunction());
    Nan::SetTemplate(tpl, "Texture", AminoTexture::GetInitFunction());
    Nan::SetTemplate(tpl, "Polygon", AminoPolygon::GetInitFunction());
    Nan::SetTemplate(tpl, "Text", AminoText::GetInitFunction());
    Nan::SetTemplate(tpl, "Anim", AminoAnim::GetInitFunction());

    //stats
    Nan::SetPrototypeMethod(tpl, "_getStats", GetStats);

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

    if (DEBUG_BASE) {
        printf("getScreenInfo()\n");
    }

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

    assert(obj);

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
    viewportChanged = true;
}

/**
 * Shader and matrix setup.
 */
void AminoGfx::setupRenderer() {
    if (DEBUG_BASE) {
        printf("-> setupRenderer()\n");
    }

    assert(!renderer);

    renderer = new AminoRenderer();
    renderer->setup();
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

    //start rendering
    startRenderingThread();
}

/**
 * Render thread.
 */
void AminoGfx::renderingThread(void *arg) {
    AminoGfx *gfx = (AminoGfx *)arg;

    while (gfx->isRenderingThreadRunning()) {
        if (DEBUG_FPS) {
            gfx->measureRenderingStart();
        }

        gfx->render();

        if (DEBUG_FPS) {
            gfx->measureRenderingEnd();
        }

        //check errors
        if (DEBUG_RENDERER || DEBUG_RENDERER_ERRORS) {
            AminoRenderer::showGLErrors();
        }
    }
}

/**
 * Rendering has started.
 */
void AminoGfx::measureRenderingStart() {
    double time = getTime();

    if (fpsStart == 0) {
        fpsStart = time;
        fpsCount = 0;
        fpsCycleMin = 0;
        fpsCycleMax = 0;
        fpsCycleAvg = 0;
    }

    fpsCycleStart = time;
}

/**
 * Rendering done.
 */
void AminoGfx::measureRenderingEnd() {
    double time = getTime();
    double diff = time - fpsStart;

    //cycle
    fpsCount++;

    double cycle = fpsCycleEnd - fpsCycleStart;

    if (fpsCycleMin == 0 || cycle < fpsCycleMin) {
        fpsCycleMin = cycle;
    }

    if (cycle > fpsCycleMax) {
        fpsCycleMax = cycle;
    }

    fpsCycleAvg += cycle;

    //output every second
    if (diff >= 1000) {
        fpsStart = 0;

        printf("%i FPS (max: %i ms, min: %i ms, avg: %i ms)\n", (int)(fpsCount * 1000 / diff), (int)fpsCycleMax, (int)fpsCycleMin, (int)(fpsCycleAvg / fpsCount));
    }
}

/**
 * Start rendering in asynchronous thread.
 *
 * Note: called on main thread.
 */
void AminoGfx::startRenderingThread() {
    if (threadRunning) {
        return;
    }

    int res = uv_thread_create(&thread, renderingThread, this);

    assert(res == 0);

    threadRunning = true;

    asyncHandle.data = this;
    uv_async_init(uv_default_loop(), &asyncHandle, AminoGfx::handleRenderEvents);

    //retain instance (while thread is running)
    retain();
}

/**
 * Signal from rendering thread to check for async updates.
 */
void AminoGfx::handleRenderEvents(uv_async_t *handle) {
    if (DEBUG_RENDERER) {
        printf("-> renderer: handleRenderEvents()\n");
    }

    AminoGfx *gfx = (AminoGfx *)handle->data;

    if (DEBUG_BASE) {
        assert(gfx->isMainThread());
    }

    //create scope
    Nan::HandleScope scope;

    gfx->handleJSUpdates();
    gfx->handleAsyncDeletes();

    //handle events
    gfx->handleSystemEvents();

    if (DEBUG_RENDERER) {
        printf(" -> done\n");
    }
}

/**
 * Stop rendering thread.
 *
 * Note: has to be called on main thread!
 */
void AminoGfx::stopRenderingThread() {
    if (!threadRunning) {
        return;
    }

    threadRunning = false;

    int res = uv_thread_join(&thread);

    assert(res == 0);

    //process remaining events
    res = uv_async_send(&asyncHandle);

    assert(res == 0);

    //destroy handle
    uv_close((uv_handle_t *)&asyncHandle, NULL);

    //release instance
    release();
}

/**
 * Check OpenGL thread status.
 */
bool AminoGfx::isRenderingThreadRunning() {
    return threadRunning;
}

/**
 * Render a scene (synchronous call).
 */
void AminoGfx::render() {
    //context
    if (DEBUG_RENDERER) {
        printf("-> renderer: bindContext()\n");
    }

    if (destroyed || !bindContext()) {
        return;
    }

    rendering = true;

    //updates
    if (DEBUG_RENDERER) {
        printf("-> renderer: handle updates\n");
    }

    processAsyncQueue();
    processAnimations();

    //send signal to main thread to handle queues
    int res = uv_async_send(&asyncHandle);

    assert(res == 0);

    //render scene (root node)
    if (DEBUG_RENDERER) {
        printf("-> renderer: renderScene()\n");
    }

    renderScene();

    //done
    if (DEBUG_FPS) {
        fpsCycleEnd = getTime();
    }

    if (DEBUG_RENDERER) {
        printf("-> renderer: renderingDone()\n");
    }

    renderingDone();
    rendering = false;

    if (DEBUG_RENDERER) {
        printf("-> renderer: done\n");
    }
}

/**
 * Update all animated values.
 *
 * Note: called on rendering thread.
 */
void AminoGfx::processAnimations() {
    if (DEBUG_BASE) {
        assert(!isMainThread());
    }

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

/**
 * Render the root node and all its children.
 */
void AminoGfx::renderScene() {
    if (!root) {
        return;
    }

    if (viewportChanged) {
        viewportChanged = false;

        renderer->updateViewport(propW->value, propH->value, viewportW, viewportH);
    }

    renderer->initScene(propR->value, propG->value, propB->value, propOpacity->value);
    renderer->renderScene(root);
}

/**
 * Stop rendering and free resources.
 */
NAN_METHOD(AminoGfx::Destroy) {
    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    assert(obj);

    if (!obj->destroyed) {
        obj->destroy();
    }
}

/**
 * Stop rendering and free resources.
 *
 * Note: has to run on main thread.
 */
void AminoGfx::destroy() {
    //stop thread
    stopRenderingThread();

    //bind context (to main thread)
    if (started) {
        started = false;

        bool res = bindContext();

        assert(res);

        //renderer
        if (renderer) {
            delete renderer;
            renderer = NULL;
        }
    }

    //unbind root
    setRoot(NULL);

    //free async
    clearAsyncQueue();
    handleAsyncDeletes();

    //base destroy
    AminoJSEventObject::destroy();

    //debug
    if (DEBUG_BASE) {
        printf("AminoGfx destroyed -> %i references left\n", getReferenceCount());
    }
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
        //set new value
        property->setAsyncData(NULL, data);

        //update native window
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

NAN_METHOD(AminoGfx::SetRoot) {
    //new value
    AminoGroup *group;

    if (info[0]->IsNull() || info[0]->IsUndefined()) {
        group = NULL;
    } else {
        group = Nan::ObjectWrap::Unwrap<AminoGroup>(info[0]->ToObject());

        assert(group);
    }

    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    assert(obj);

    obj->setRoot(group);
}

/**
 * Set the root node.
 */
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
 * Get runtime statistics.
 */
NAN_METHOD(AminoGfx::GetStats) {
    AminoGfx *gfx = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());
    v8::Local<v8::Object> obj = Nan::New<v8::Object>();

    Nan::Set(obj, Nan::New("activeInstances").ToLocalChecked(), Nan::New(activeInstances));
    Nan::Set(obj, Nan::New("totalInstances").ToLocalChecked(), Nan::New(totalInstances));
    Nan::Set(obj, Nan::New("animations").ToLocalChecked(), Nan::New((uint32_t)gfx->animations.size()));

    gfx->getStats(obj);

    info.GetReturnValue().Set(obj);
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
 *
 * Note: has to be called on OpenGL thread.
 */
amino_atlas_t AminoGfx::getAtlasTexture(texture_atlas_t *atlas) {
    assert(renderer);

    return renderer->getAtlasTexture(atlas);
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

/**
 * Create group instance.
 */
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

/**
 * Create rect instance.
 */
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

/**
 * Create polygon instance.
 */
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

/**
 * Create anim instance.
 */
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

/**
 * Create text instance.
 */
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

    if (DEBUG_FONT_TEXTURE) {
        //output as ASCII art
        int maxW = std::min(80, (int)atlas->width);
        int maxH = atlas->height;

        //find last set pixels
        unsigned char *first = atlas->data;
        unsigned char *pos = first + atlas->width * atlas->height - 1;

        while (pos > first && *pos == 0) {
            pos--;
        }

        maxH = (pos - first) / atlas->height + 1;

        //output
        for (int j = 0; j < maxH; j++) {
            for (int i = 0; i < maxW; i++) {
                int d = atlas->data[i + j * atlas->width];

                if (d == 0) {
                    printf(" ");
                } else {
                    printf("X");
                }
            }

            printf("\n");
        }

        printf("\n");
    }

    glBindTexture(GL_TEXTURE_2D, texture.textureId);
//cbx check font bottleneck
    if (atlas->depth == 1) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlas->width, atlas->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas->data);
    } else if (atlas->depth == 3) {
        //Note: not supported so far
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, atlas->width, atlas->height, 0, GL_RGB, GL_UNSIGNED_BYTE, atlas->data);
    }

    texture.lastGlyphUpdate = fontSize->fontTexture->glyphs->size;

    //printf("font texture updated\n");
    //printf("updateTexture() done\n");

    return texture.textureId;
}

/**
 * Render text to vertices.
 */
void AminoText::addTextGlyphs(vertex_buffer_t *buffer, texture_font_t *font, const char *text, vec2 *pen, int wrap, int width, int *lineNr) {
    //see https://github.com/rougier/freetype-gl/blob/master/demos/glyph.c
    size_t len = utf8_strlen(text);
//cbx still too slow, even when all glyphs are in atlas (> 50 ms on RPi)
    *lineNr = 1;

    size_t lineStart = 0; //start of current line
    size_t linePos = 0; //character pos current line
    float penXStart = pen->x;

    //debug
    //printf("addTextGlyphs: wrap=%i width=%i\n", wrap, width);

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
            bool skip = false;

            if (wrap != AminoText::WRAP_NONE) {
                bool newLine = false;
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
                    if (std::iswspace((wchar_t)glyph->codepoint)) {
                        skip = true;
                    }
                }

                //check white space
                if (!skip && (newLine || (lineStart == i && *lineNr > 1)) && std::iswspace(glyph->codepoint)) {
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
                            if (std::iswspace((wchar_t)textUtf32[j])) {
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
                }
            }

            //add glyph
            if (!skip) {
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

                GLushort indices[6] = {0,1,2, 0,2,3};
                vertex_t vertices[4] = { { x0, y0, 0,  s0, t0 },
                                         { x0, y1, 0,  s0, t1 },
                                         { x1, y1, 0,  s1, t1 },
                                         { x1, y0, 0,  s1, t0 } };

                //append
                vertex_buffer_push_back(buffer, vertices, 4, indices, 6);
                linePos++;

                //next
                pen->x += advance;
            }
        } else {
            //not enough space for glyph

            //store
            uint32_t ch32 = utf8_to_utf32(textPos);

            textUtf32[i] = ch32;

            //show error
            printf("no space for glyph: %lc\n", (wchar_t)ch32);

            //continue with glyphs in atlas
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
        //vertex & texture coordinates
        buffer = vertex_buffer_new("pos:3f,tex_coord:2f");
    }

    texture_font_t *f = fontSize->fontTexture;

    assert(f);

    vec2 pen;

    pen.x = 0;
    pen.y = 0;

    //Note: consider using async task to avoid performance issues
    addTextGlyphs(buffer, f, propText->value.c_str(), &pen, wrap, propW->value, &lineNr);

    if (DEBUG_BASE) {
        printf("-> layoutText() done\n");
    }

    return true;
}
