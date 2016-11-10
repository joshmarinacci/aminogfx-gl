#include "base.h"

#include <cwctype>
#include <algorithm>

#include "renderer.h"
#include "fonts/utf8-utils.h"

#define DEBUG_RENDERER false
#define DEBUG_FONT_TEXTURE false
#define DEBUG_FONT_UPDATES false

#define MEASURE_FPS true
#define SHOW_RENDERER_ERRORS true

//
//  AminoGfx
//

AminoGfx::AminoGfx(std::string name): AminoJSEventObject(name) {
    //recursive mutex needed
    pthread_mutexattr_t attr;

    int res = pthread_mutexattr_init(&attr);

    assert(res == 0);

    res = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    assert(res == 0);

    // animLock
    res = pthread_mutex_init(&animLock, &attr);
    assert(res == 0);
}

AminoGfx::~AminoGfx() {
    //callback
    if (startCallback) {
        delete startCallback;
        startCallback = NULL;
    }

    //mutex
    int res = pthread_mutex_destroy(&animLock);

    assert(res == 0);

    //Note: properties are deleted by base class destructor
}

/**
 * Add new instance.
 */
void AminoGfx::addInstance() {
    instanceCount++;
    instances.push_back(this);
}

/**
 * Remove instance.
 */
void AminoGfx::removeInstance() {
    //remove instance
    std::vector<AminoGfx *>::iterator pos = std::find(instances.begin(), instances.end(), this);

    if (pos != instances.end()) {
        instances.erase(pos);
        instanceCount--;
    }
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
    Nan::SetTemplate(tpl, "Polygon", AminoPolygon::GetInitFunction());
    Nan::SetTemplate(tpl, "Model", AminoModel::GetInitFunction());
    Nan::SetTemplate(tpl, "Text", AminoText::GetInitFunction());

    Nan::SetTemplate(tpl, "Texture", AminoTexture::GetInitFunction());
    Nan::SetTemplate(tpl, "Anim", AminoAnim::GetInitFunction());

    // animations
    Nan::SetPrototypeMethod(tpl, "clearAnimations", ClearAnimations);
    Nan::SetPrototypeMethod(tpl, "getTime", GetTime);
    Nan::SetMethod(tpl, "getTime", GetTime);

    // stats
    Nan::SetPrototypeMethod(tpl, "_getStats", GetStats);

    //global template instance
    v8::Local<v8::Function> func = Nan::GetFunction(tpl).ToLocalChecked();

    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), func);
}

/**
 * Initialize constructor parameters.
 */
void AminoGfx::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    AminoJSObject::preInit(info);

    //check params
    if (info.Length() >= 1 && info[0]->IsObject()) {
        v8::Local<v8::Object> obj = info[0]->ToObject();

        //store
        createParams.Reset(obj);
    }
}

/**
 * JS object was initialized.
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

    propShowFPS = createBooleanProperty("showFPS");

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
    assert(info.Length() == 1);

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

    //debug
    //AminoRenderer::checkTexturePerformance();

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

    //params
    if (!createParams.IsEmpty()) {
        v8::Local<v8::Object> obj = Nan::New(createParams);

        //swap interval
        Nan::MaybeLocal<v8::Value> swapIntervalMaybe = Nan::Get(obj, Nan::New<v8::String>("swapInterval").ToLocalChecked());

        if (!swapIntervalMaybe.IsEmpty()) {
            v8::Local<v8::Value> swapIntervalValue = swapIntervalMaybe.ToLocalChecked();

            if (swapIntervalValue->IsInt32()) {
                swapInterval = swapIntervalValue->Int32Value();
            }
        }
    }
}

/**
 * Shader and matrix setup.
 */
void AminoGfx::setupRenderer() {
    if (DEBUG_BASE) {
        printf("-> setupRenderer()\n");
    }

    assert(!renderer);

    renderer = new AminoRenderer(this);
    renderer->setup();

    if (!createParams.IsEmpty()) {
        v8::Local<v8::Object> obj = Nan::New(createParams);

        //perspective
        Nan::MaybeLocal<v8::Value> perspectiveMaybe = Nan::Get(obj, Nan::New<v8::String>("perspective").ToLocalChecked());

        if (!perspectiveMaybe.IsEmpty()) {
            v8::Local<v8::Value> perspectiveValue = perspectiveMaybe.ToLocalChecked();

            if (perspectiveValue->IsObject()) {
                v8::Local<v8::Object> perspective = perspectiveValue->ToObject();

                renderer->setupPerspective(perspective);
            }
        }
    }
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
    Nan::Set(obj, Nan::New("extensions").ToLocalChecked(), Nan::New(std::string((char *)glGetString(GL_EXTENSIONS))).ToLocalChecked());

    // 2) texture size
    GLint maxTextureSize;

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    Nan::Set(obj, Nan::New("maxTextureSize").ToLocalChecked(), Nan::New(maxTextureSize));

    // 3) texture units
    GLint maxTextureImageUnits;

    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);
    Nan::Set(obj, Nan::New("maxTextureImageUnits").ToLocalChecked(), Nan::New(maxTextureImageUnits));

    // 4) platform specific
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
    AminoGfx *gfx = static_cast<AminoGfx *>(arg);

    assert(gfx);

    while (gfx->isRenderingThreadRunning()) {
        if (DEBUG_THREADS) {
            uv_thread_t threadId = uv_thread_self();

            printf("rendering: cycle start (thread=%lu)\n", (unsigned long)threadId);
        }

        if (MEASURE_FPS) {
            gfx->measureRenderingStart();
        }

        gfx->render();

        if (MEASURE_FPS) {
            gfx->measureRenderingEnd();
        }

        //check errors
        if (DEBUG_RENDERER || SHOW_RENDERER_ERRORS) {
            int errors = AminoRenderer::showGLErrors();

            gfx->rendererErrors += errors;
        }

        if (DEBUG_THREADS) {
            printf("rendering: cycle done\n");
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
        //values
        lastFPS = fpsCount * 1000 / diff;
        lastCycleStart = fpsStart;
        lastCycleMax = fpsCycleMax;
        lastCycleMin = fpsCycleMin;
        lastCycleAvg = fpsCycleAvg / fpsCount;

        //reset
        fpsStart = 0;

        //optional output on screen
        if (propShowFPS->value) {
            printf("%i FPS (max: %i ms, min: %i ms, avg: %i ms)\n", (int)lastFPS, (int)fpsCycleMax, (int)fpsCycleMin, (int)lastCycleAvg);
        }
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
    if (DEBUG_RENDERER || DEBUG_THREADS) {
        printf("-> renderer: handleRenderEvents()\n");
    }

    AminoGfx *gfx = static_cast<AminoGfx *>(handle->data);

    assert(gfx);

    if (DEBUG_BASE) {
        assert(gfx->isMainThread());
    }

    //create scope
    Nan::HandleScope scope;

    gfx->handleJSUpdates();
    gfx->handleAsyncDeletes();

    //handle events
    gfx->handleSystemEvents();

    if (DEBUG_RENDERER || DEBUG_THREADS) {
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

    //update texts
    updateTextNodes();

    //render scene (root node)
    if (DEBUG_RENDERER) {
        printf("-> renderer: renderScene()\n");
    }

    renderScene();

    //done
    fpsCycleEnd = getTime();

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

    int res = pthread_mutex_lock(&animLock);

    assert(res == 0);

    double currentTime = getTime();
    int count = animations.size();

    //debug timer
    //printf("timer timestamp: %f\n", currentTime);

    for (int i = 0; i < count; i++) {
        animations[i]->update(currentTime);
    }

    res = pthread_mutex_unlock(&animLock);
    assert(res == 0);
}

/**
 * Clear all animations.
 *
 * Note: stops gets not called.
 */
NAN_METHOD(AminoGfx::ClearAnimations) {
    AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info.This());

    assert(obj);

    obj->clearAnimations();
}

/**
 * Get current timer time (monotonic time, not current system time!).
 */
NAN_METHOD(AminoGfx::GetTime) {
    info.GetReturnValue().Set(getTime());
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
    clearAnimations();
    handleAsyncDeletes();

    //params
    createParams.Reset();

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

    //default: does nothing
    bool res = AminoJSObject::handleSyncUpdate(property, data);

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

    return res;
}

NAN_METHOD(AminoGfx::SetRoot) {
    //new value
    AminoGroup *group;

    if (info.Length() == 0 || info[0]->IsNull() || info[0]->IsUndefined()) {
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

    assert(gfx);

    v8::Local<v8::Object> obj = Nan::New<v8::Object>();

    gfx->getStats(obj);

    info.GetReturnValue().Set(obj);
}

/**
 * Get runtime statistics.
 */
void AminoGfx::getStats(v8::Local<v8::Object> &obj) {
    //JS objects
    Nan::Set(obj, Nan::New("activeInstances").ToLocalChecked(), Nan::New(activeInstances));
    Nan::Set(obj, Nan::New("totalInstances").ToLocalChecked(), Nan::New(totalInstances));

    //animations
    Nan::Set(obj, Nan::New("animations").ToLocalChecked(), Nan::New((uint32_t)animations.size()));

    //textures
    Nan::Set(obj, Nan::New("textures").ToLocalChecked(), Nan::New(textureCount));

    //rendering performance (FPS)
    if (MEASURE_FPS && lastFPS) {
        v8::Local<v8::Object> fpsObj = Nan::New<v8::Object>();

        Nan::Set(fpsObj, Nan::New("fps").ToLocalChecked(), Nan::New(lastFPS));
        Nan::Set(fpsObj, Nan::New("start").ToLocalChecked(), Nan::New(lastCycleStart));
        Nan::Set(fpsObj, Nan::New("max").ToLocalChecked(), Nan::New(lastCycleMax));
        Nan::Set(fpsObj, Nan::New("min").ToLocalChecked(), Nan::New(lastCycleMin));
        Nan::Set(fpsObj, Nan::New("avg").ToLocalChecked(), Nan::New(lastCycleAvg));
        Nan::Set(obj, Nan::New("fps").ToLocalChecked(), fpsObj);
    }

    //renderer

    if (SHOW_RENDERER_ERRORS) {
        Nan::Set(obj, Nan::New("errors").ToLocalChecked(), Nan::New(rendererErrors));
    }

    //base class
    AminoJSEventObject::getStats(obj);
}

/**
 * Add animation.
 *
 * Note: called on main thread.
 */
bool AminoGfx::addAnimation(AminoAnim *anim) {
    if (destroyed) {
        return false;
    }

    //retain anim instance
    anim->retain();

    //add
    int res = pthread_mutex_lock(&animLock);

    assert(res == 0);

    animations.push_back(anim);

    //check total
    if (animations.size() % 100 == 0) {
        printf("warning: %i animations reached!\n", (int)animations.size());
    }

    res = pthread_mutex_unlock(&animLock);
    assert(res == 0);

    return true;
}

/**
 * Remove animation.
 *
 * Note: called on main thread.
 */
void AminoGfx::removeAnimation(AminoAnim *anim) {
    if (destroyed) {
        return;
    }

    //remove
    int res = pthread_mutex_lock(&animLock);

    assert(res == 0);

    std::vector<AminoAnim *>::iterator pos = std::find(animations.begin(), animations.end(), anim);

    if (pos != animations.end()) {
        animations.erase(pos);

        //free instance
        anim->release();
    }

    res = pthread_mutex_unlock(&animLock);
    assert(res == 0);
}

/**
 * Clear all animations now.
 *
 * Note: called on main thread.
 */
void AminoGfx::clearAnimations() {
    if (destroyed) {
        return;
    }

    //release all instances
    int res = pthread_mutex_lock(&animLock);

    assert(res == 0);

    std::size_t count = animations.size();

    for (std::size_t i = 0; i < count; i++) {
        AminoAnim *item = animations[i];

        item->release();
    }

    animations.clear();

    res = pthread_mutex_unlock(&animLock);
    assert(res == 0);
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

    if (DEBUG_BASE) {
        printf("enqueue: delete texture\n");
    }

    //enqueue
    AminoJSObject::enqueueValueUpdate(textureId, NULL, static_cast<asyncValueCallback>(&AminoGfx::deleteTexture));
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
    textureCount--;
}

/**
 * Delete buffer.
 *
 * Note: has to be called on main thread.
 */
void AminoGfx::deleteBufferAsync(GLuint bufferId) {
    if (destroyed) {
        return;
    }

    if (DEBUG_BASE) {
        printf("enqueue: delete buffer\n");
    }

    //enqueue
    AminoJSObject::enqueueValueUpdate(bufferId, NULL, static_cast<asyncValueCallback>(&AminoGfx::deleteBuffer));
}

/**
 * Delete buffer.
 */
void AminoGfx::deleteBuffer(AsyncValueUpdate *update, int state) {
    if (state != AsyncValueUpdate::STATE_APPLY) {
        return;
    }

    GLuint bufferId = update->valueUint32;

    if (DEBUG_RESOURCES) {
        printf("-> deleting buffer %i\n", bufferId);
    }

    glDeleteBuffers(1, &bufferId);
}

/**
 * Collect text updates.
 */
void AminoGfx::textUpdateNeeded(AminoText *text) {
    if (std::find(textUpdates.begin(), textUpdates.end(), text) == textUpdates.end()) {
        textUpdates.push_back(text);
    }
}

/**
 * Update all modified text nodes.
 */
void AminoGfx::updateTextNodes() {
    std::size_t count = textUpdates.size();

    if (count == 0) {
        return;
    }

#if (DEBUG_FONT_PERFORMANCE == 1)
    //debug
    double startTime = getTime(), diff;
#endif

    //layout modified texts
    std::vector<AminoText *> textureUpdates;

    for (std::size_t i = 0; i < count; i++) {
        AminoText *item = textUpdates[i];

        if (item->layoutText()) {
            //find texture
            std::size_t textureCount = textureUpdates.size();
            GLuint textureId = item->getTextureId();
            bool found = false;

            for (std::size_t j = 0; j < textureCount; j++) {
                AminoText *item2 = textureUpdates[j];

                if (item2->getTextureId() == textureId) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                textureUpdates.push_back(item);
            }
        }
    }

    textUpdates.clear();

#if (DEBUG_FONT_PERFORMANCE == 1)
    //debug
    diff = getTime() - startTime;
    if (diff > 5) {
        printf("layoutText: %i ms\n", (int)diff);
    }
#endif

    //update textures
    std::size_t textureCount = textureUpdates.size();

#if (DEBUG_FONT_PERFORMANCE == 1)
    //debug
    startTime = getTime();
#endif

    for (std::size_t i = 0; i < textureCount; i++) {
        AminoText *item = textureUpdates[i];

        item->updateTexture();

        //inform other amino instances to update shared texture
        texture_atlas_t *atlas = item->fontSize->fontTexture->atlas;

        atlasTextureHasChanged(atlas);
    }

#if (DEBUG_FONT_PERFORMANCE == 1)
    //debug
    diff = getTime() - startTime;
    if (diff > 5) {
        printf("updateTexture: %i ms\n", (int)diff);
    }
#endif
}

/**
 * Shared atlas texture has changed.
 *
 * Note: called on rendering thread.
 */
void AminoGfx::atlasTextureHasChanged(texture_atlas_t *atlas) {
    //overwrite
}

/**
 * Shared atlas texture has to be updated.
 *
 * Note: called on main thread
 */
void AminoGfx::updateAtlasTexture(texture_atlas_t *atlas) {
    //check if texture exists
    bool newTexture;
    amino_atlas_t texture = getAtlasTexture(atlas, false, newTexture);

    if (texture.textureId != INVALID_TEXTURE) {
        if (DEBUG_BASE) {
            printf("enqueue: atlas texture update\n");
        }

        //switch to rendering thread
        AminoJSObject::enqueueValueUpdate(texture.textureId, atlas, static_cast<asyncValueCallback>(&AminoGfx::updateAtlasTextureHandler));
    }
}

/**
 * Update atlas texture.
 */
void AminoGfx::updateAtlasTextureHandler(AsyncValueUpdate *update, int state) {
    if (state != AsyncValueUpdate::STATE_APPLY) {
        return;
    }

    //debug
    //printf("%p: texture update %i\n", this, (int)update->valueUint32);

    texture_atlas_t *atlas = (texture_atlas_t *)update->data;

    AminoText::updateTextureFromAtlas(update->valueUint32, atlas);
}

/**
 * Get texture for atlas.
 *
 * Note: has to be called on OpenGL thread (if createIfMissing is true).
 */
amino_atlas_t AminoGfx::getAtlasTexture(texture_atlas_t *atlas, bool createIfMissing, bool &newTexture) {
    assert(renderer);

    return renderer->getAtlasTexture(atlas, createIfMissing, newTexture);
}

/**
 * A new texture was created.
 */
void AminoGfx::notifyTextureCreated() {
    textureCount++;
}

/**
 * Update atlas textures in all instances.
 *
 * Note: called on main thread.
 */
void AminoGfx::updateAtlasTextures(texture_atlas_t *atlas) {
    for (auto const &item : instances) {
        item->updateAtlasTexture(atlas);
    }
}

//static initializers
int AminoGfx::instanceCount = 0;
std::vector<AminoGfx *> AminoGfx::instances;

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
// AminoModelFactory
//

/**
 * AminoModel factory constructor.
 */
AminoModelFactory::AminoModelFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoPolygon", callback) {
    //empty
}

/**
 * Create polygon instance.
 */
AminoJSObject* AminoModelFactory::create() {
    return new AminoModel();
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
 * Update texture.
 */
void AminoText::updateTexture() {
    if (DEBUG_FONT_UPDATES) {
        printf("-> update font texture: %s\n", fontSize->font->getFontInfo().c_str());
    }

    assert(texture.textureId != INVALID_TEXTURE);
    assert(fontSize);
    assert(fontSize->fontTexture);

    texture_atlas_t *atlas = fontSize->fontTexture->atlas;

    assert(atlas);

    updateTextureFromAtlas(texture.textureId, atlas);
}

/**
 * Update texture from atlas.
 */
void AminoText::updateTextureFromAtlas(GLuint textureId, texture_atlas_t *atlas) {
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

    glBindTexture(GL_TEXTURE_2D, textureId);

    if (atlas->depth == 1) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlas->width, atlas->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas->data);
    } else if (atlas->depth == 3) {
        //Note: not supported so far
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, atlas->width, atlas->height, 0, GL_RGB, GL_UNSIGNED_BYTE, atlas->data);
    }

    //printf("font texture updated\n");
    //printf("updateTexture() done\n");
}

/**
 * Get the font texture.
 */
GLuint AminoText::getTextureId() {
    return texture.textureId;
}

/**
 * Render text to vertices.
 */
void AminoText::addTextGlyphs(vertex_buffer_t *buffer, texture_font_t *font, const char *text, vec2 *pen, int wrap, int width, int *lineNr, int maxLines, float *lineW) {
    //see https://github.com/rougier/freetype-gl/blob/master/demos/glyph.c
    size_t len = utf8_strlen(text);

    *lineNr = 1;
    *lineW = 0;

    size_t lineStart = 0; //start of current line
    size_t linePos = 0; //character pos current line
    float penXStart = pen->x;

    //debug
    //printf("addTextGlyphs: wrap=%i width=%i\n", wrap, width);

    //add glyphs (by iterating Unicode characters)
    char *textPos = (char *)text;
    char *lastTextPos = NULL;
    uint32_t textUtf32[len];
    bool done = false;

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
                    //line break needed (check which one)
                    linePos = 0;

                    //check word wrapping
                    bool wrapped = false;
                    bool isLastLine = maxLines > 0 && *lineNr == maxLines;

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
                            *lineW = std::max(*lineW, xOffset);
                            lineStart = i - linePos;
                            skip = false;

                            wrapped = true;

                            if (isLastLine) {
                                //remove characters left
                                for (size_t j = start; j < count; j++) {
                                    vertex_buffer_erase(buffer, start);
                                }
                            }
                        }
                    }

                    //wrap new character
                    if (!wrapped) {
                        //wrap at current position
                        *lineW = std::max(*lineW, pen->x - penXStart);
                        pen->x = penXStart;
                        kerning = 0;
                        lineStart = i;
                    }

                    //debug
                    //printf("pen.x=%f lineStart=%lc\n", pen->x, text[lineStart]);

                    if (isLastLine) {
                        //skip any character
                        skip = true;
                        done = true;
                    } else {
                        (*lineNr)++;
                    }

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
                float x0  = pen->x + glyph->offset_x;
                float y0  = pen->y + glyph->offset_y;
                float x1  = x0 + glyph->width;
                float y1  = y0 - glyph->height;
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

                GLushort indices[6] = { 0,1,2, 0,2,3 };
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

        //check end condition
        if (done) {
            break;
        }

        //next glyph pos
        size_t charLen = utf8_surrogate_len(textPos);

        lastTextPos = textPos;
        textPos += charLen;
    }

    *lineW = std::max(*lineW, pen->x - penXStart);
}

/**
 * Update the rendered text.
 *
 * Returns true if texture has changed and must be updated.
 */
bool AminoText::layoutText() {
    //printf("layoutText()\n");

    if (!fontSize) {
        //printf("-> no font\n");

        return false;
    }

    if (DEBUG_FONT_UPDATES) {
        printf("->layoutText() render text (%s)\n", fontSize->font->fontName.c_str());
    }

    //Note: FreeType glyph code is not thread-safe, using lock to prevent crash on macOS if multiple AminoGfx instances are active
    uv_mutex_lock(&freeTypeMutex);

    //create texture (needed in next calls)
    assert(fontSize->fontTexture);

    texture_atlas_t *atlas = fontSize->fontTexture->atlas;
    bool newTexture;

    assert(atlas);
    assert(atlas->depth == 1);

    if (texture.textureId == INVALID_TEXTURE) {
        //create or use existing texture (for atlas)
        texture = getAminoGfx()->getAtlasTexture(atlas, true, newTexture);

        assert(texture.textureId != INVALID_TEXTURE);
    }

    //render text
    if (buffer) {
        vertex_buffer_clear(buffer);
    } else {
        //vertex & texture coordinates
        buffer = vertex_buffer_new("pos:3f,texCoord:2f");
    }

    texture_font_t *fontTexture = fontSize->fontTexture;
    size_t lastGlyphCount = fontTexture->glyphs->size;

    assert(fontTexture);

    vec2 pen;

    pen.x = 0;
    pen.y = 0;

    //Note: consider using async task to avoid performance issues
    addTextGlyphs(buffer, fontTexture, propText->value.c_str(), &pen, wrap, propW->value, &lineNr, propMaxLines->value, &lineW);

    if (DEBUG_BASE) {
        printf("-> layoutText() done\n");
    }

    size_t newGlyphCount = fontTexture->glyphs->size;
    bool glyphsChanged = lastGlyphCount != newGlyphCount || (newTexture && newGlyphCount > 0);

    uv_mutex_unlock(&freeTypeMutex);

    //debug
    //printf("glyphs changed: %i\n", glyphsChanged);

    return glyphsChanged;
}

uv_mutex_t AminoText::freeTypeMutex;
bool AminoText::freeTypeMutexInitialized = false;