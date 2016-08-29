#ifndef _AMINOBASE_H
#define _AMINOBASE_H

#include "gfx.h"
#include "base_js.h"
#include "base_weak.h"
#include "fonts.h"
#include "images.h"

#include <uv.h>
#include "shaders.h"
#include "mathutils.h"
#include <stdio.h>
#include <vector>
#include <stack>
#include <stdlib.h>
#include <string>
#include <map>

#include "freetype-gl.h"
#include "mat4.h"
#include "shader.h"
#include "vertex-buffer.h"
#include "texture-font.h"

extern "C" {
    #include "nanojpeg.h"
    #include "upng.h"
}

const int GROUP = 1;
const int RECT  = 2;
const int TEXT  = 3;
const int ANIM  = 4;
const int POLY  = 5;
const int MODEL = 6;

class AminoGroup;
class AminoAnim;
class AminoRenderer;

/**
 * Amino main class to call from JavaScript.
 *
 * Note: abstract
 */
class AminoGfx : public AminoJSEventObject {
public:
    AminoGfx(std::string name);
    ~AminoGfx();

    static NAN_MODULE_INIT(InitClasses);

    bool addAnimationAsync(AminoAnim *anim);
    void removeAnimationAsync(AminoAnim *anim);

    void deleteTextureAsync(GLuint textureId);
    amino_atlas_t getAtlasTexture(texture_atlas_t *atlas);

protected:
    bool started = false;
    bool rendering = false;
    Nan::Callback *startCallback = NULL;

    //renderer
    AminoRenderer *renderer = NULL;
    AminoGroup *root = NULL;
    int viewportW;
    int viewportH;
    bool viewportChanged;

    //performance (FPS)
    double fpsStart = 0;
    double fpsCycleStart;
    double fpsCycleEnd;
    double fpsCycleMin;
    double fpsCycleMax;
    double fpsCycleAvg;
    int fpsCount;

    //thread
    uv_thread_t thread;
    bool threadRunning = false;
    uv_async_t asyncHandle;

    //properties
    FloatProperty *propX;
    FloatProperty *propY;

    FloatProperty *propW;
    FloatProperty *propH;

    FloatProperty *propR;
    FloatProperty *propG;
    FloatProperty *propB;

    FloatProperty *propOpacity;

    Utf8Property *propTitle;

    //animations
    std::vector<AminoAnim *> animations;

    //creation
    static void Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target, AminoJSObjectFactory* factory);

    void setup() override;

    //abstract methods
    virtual void initRenderer();
    void setupRenderer();
    void addRuntimeProperty();
    virtual void populateRuntimeProperties(v8::Local<v8::Object> &obj);

    virtual void start();
    void ready();

    void startRenderingThread();
    void stopRenderingThread();
    bool isRenderingThreadRunning();
    static void renderingThread(void *arg);
    static void handleRenderEvents(uv_async_t *handle);
    virtual void handleSystemEvents() = 0;

    virtual void render();
    void processAnimations();
    virtual bool bindContext() = 0;
    virtual void renderScene();
    virtual void renderingDone() = 0;
    bool isRendering();

    void destroy() override;

    virtual bool getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) { return false; };
    void updateSize(int w, int h); //call after size event
    void updatePosition(int x, int y); //call after position event

    void fireEvent(v8::Local<v8::Object> &obj);

    bool handleSyncUpdate(AnyProperty *prop, void *data) override;
    virtual void updateWindowSize() = 0;
    virtual void updateWindowPosition() = 0;
    virtual void updateWindowTitle() = 0;

    void setRoot(AminoGroup *group);

private:
    //JS methods
    static NAN_METHOD(Start);
    static NAN_METHOD(Destroy);

    static NAN_METHOD(SetRoot);
    static NAN_METHOD(GetStats);

    //animation
    void addAnimation(AsyncValueUpdate *update, int state);
    void removeAnimation(AsyncValueUpdate *update, int state);

    //texture
    void deleteTexture(AsyncValueUpdate *update, int state);

    //debug
    void measureRenderingStart();
    void measureRenderingEnd();
};

/**
 * Base class for all rendering nodes.
 *
 * Note: abstract.
 */
class AminoNode : public AminoJSObject {
public:
    int type;

    //location
    FloatProperty *propX;
    FloatProperty *propY;
    FloatProperty *propZ;

    //size (optional)
    FloatProperty *propW = NULL;
    FloatProperty *propH = NULL;

    //origin (optional)
    FloatProperty *propOriginX = NULL;
    FloatProperty *propOriginY = NULL;

    //zoom factor
    FloatProperty *propScaleX;
    FloatProperty *propScaleY;

    //rotation
    FloatProperty *propRotateX;
    FloatProperty *propRotateY;
    FloatProperty *propRotateZ;

    //opacity
    FloatProperty *propOpacity;

    //visibility
    BooleanProperty *propVisible;

    AminoNode(std::string name, int type): AminoJSObject(name), type(type) {
        //empty
    }

    ~AminoNode() {
        //see destroy
    }

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override {
        //set amino instance
        v8::Local<v8::Object> jsObj = info[0]->ToObject();
        AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(jsObj);

        assert(obj);

        //bind to queue
        this->setEventHandler(obj);
        Nan::Set(handle(), Nan::New("amino").ToLocalChecked(), jsObj);
    }

    void setup() override {
        AminoJSObject::setup();

        //register native properties
        propX = createFloatProperty("x");
        propY = createFloatProperty("y");
        propZ = createFloatProperty("z");

        propScaleX = createFloatProperty("sx");
        propScaleY = createFloatProperty("sy");

        propRotateX = createFloatProperty("rx");
        propRotateY = createFloatProperty("ry");
        propRotateZ = createFloatProperty("rz");

        propOpacity = createFloatProperty("opacity");
        propVisible = createBooleanProperty("visible");
    }

    /**
     * Free all resources.
     */
    void destroy() override {
        AminoJSObject::destroy();

        //to be overwritten
    }

    /**
     * Get AminoGfx instance.
     */
    AminoGfx* getAminoGfx() {
        assert(eventHandler);

        return (AminoGfx *)eventHandler;
    }

    /**
     * Validate renderer instance. Must be called in JS method handler.
     */
    bool checkRenderer(AminoNode *node) {
        return checkRenderer(node->getAminoGfx());
    }

    /**
     * Validate renderer instance. Must be called in JS method handler.
     */
    bool checkRenderer(AminoGfx *amino) {
        assert(eventHandler);

        if (eventHandler != amino) {
            Nan::ThrowTypeError("invalid renderer");
            return false;
        }

        return true;
    }
};

/**
 * Text factory.
 */
class AminoTextFactory : public AminoJSObjectFactory {
public:
    AminoTextFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Text node class.
 */
class AminoText : public AminoNode {
public:
    //text
    Utf8Property *propText;

    //color
    FloatProperty *propR;
    FloatProperty *propG;
    FloatProperty *propB;

    //box
    FloatProperty *propW;
    FloatProperty *propH;

    Utf8Property *propWrap;
    int wrap = WRAP_NONE;

    //font
    ObjectProperty *propFont;
    AminoFontSize *fontSize = NULL;
    vertex_buffer_t *buffer = NULL;
    bool updated = false;

    //alignment
    Utf8Property *propVAlign;
    int vAlign = VALIGN_BASELINE;
    int lineNr = 1;

    static const int VALIGN_BASELINE = 0x0;
    static const int VALIGN_TOP      = 0x1;
    static const int VALIGN_MIDDLE   = 0x2;
    static const int VALIGN_BOTTOM   = 0x3;

    static const int WRAP_NONE = 0x0;
    static const int WRAP_END  = 0x1;
    static const int WRAP_WORD = 0x2;

    AminoText(): AminoNode(getFactory()->name, TEXT) {
        //empty
    }

    ~AminoText() {
        //empty
    }

    void destroy() override {
        AminoNode::destroy();

        if (buffer) {
            vertex_buffer_delete(buffer);
            buffer = NULL;
        }

        //release object values
        propFont->destroy();
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propText = createUtf8Property("text");

        propR = createFloatProperty("r");
        propG = createFloatProperty("g");
        propB = createFloatProperty("b");

        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        propWrap = createUtf8Property("wrap");
        propVAlign = createUtf8Property("vAlign");
        propFont = createObjectProperty("font");
    }

    //creation

    /**
     * Create text factory.
     */
    static AminoTextFactory* getFactory() {
        static AminoTextFactory *textFactory = NULL;

        if (!textFactory) {
            textFactory = new AminoTextFactory(New);
        }

        return textFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //prototype methods
        // -> none

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * Handle async property updates.
     */
    void handleAsyncUpdate(AsyncPropertyUpdate *update) override {
        //default: set value
        AminoJSObject::handleAsyncUpdate(update);

        //check font updates
        AnyProperty *property = update->property;

        if (property == propW || property == propH || property == propText) {
            updated = true;
        }

        if (property == propWrap) {
            std::string str = propWrap->value;
            int oldWrap = wrap;

            if (str == "none") {
                wrap = WRAP_NONE;
            } else if (str == "word") {
                wrap = WRAP_WORD;
            } else if (str == "end") {
                wrap = WRAP_END;
            } else {
                //error
                printf("unknown wrap mode: %s\n", str.c_str());
            }

            if (wrap != oldWrap) {
                updated = true;
            }

            return;
        }

        if (property == propVAlign) {
            std::string str = propVAlign->value;
            int oldVAlign = vAlign;

            if (str == "top") {
                vAlign = AminoText::VALIGN_TOP;
            } else if (str == "middle") {
                vAlign = AminoText::VALIGN_MIDDLE;
            } else if (str == "baseline") {
                vAlign = AminoText::VALIGN_BASELINE;
            } else if (str == "bottom") {
                vAlign = AminoText::VALIGN_BOTTOM;
            } else {
                //error
                printf("unknown vAlign mode: %s\n", str.c_str());
            }

            if (vAlign != oldVAlign) {
                updated = true;
            }

            return;
        }

        if (property == propFont) {
            AminoFontSize *fs = (AminoFontSize *)propFont->value;

            if (fontSize == fs) {
                return;
            }

            //new font
            fontSize = fs;

            //debug
            //printf("-> use font: %s\n", fs->font->fontName.c_str());

            updated = true;
            return;
        }
    }

    /**
     * Update the rendered text.
     */
    bool layoutText();

    /**
     * Create or update a font texture.
     */
    GLuint updateTexture();

private:
    amino_atlas_t texture = { INVALID_TEXTURE, 0 };

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    static void addTextGlyphs(vertex_buffer_t *buffer, texture_font_t *font, const char *text, vec2 *pen, int wrap, int width, int *lineNr);
};

/**
 * Animation factory.
 */
class AminoAnimFactory : public AminoJSObjectFactory {
public:
    AminoAnimFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Animation class.
 */
class AminoAnim : public AminoJSObject {
private:
    AnyProperty *prop;

    bool started = false;
    bool ended = false;

    float start;
    float end;
    int count;
    float duration;
    bool autoreverse;
    int direction = FORWARD;
    int timeFunc = TF_CUBIC_IN_OUT;
    Nan::Callback *then = NULL;

    double startTime = 0;
    double lastTime  = 0;
    double pauseTime = 0;

    static const int FORWARD  = 1;
    static const int BACKWARD = 2;

    static const int FOREVER = -1;

public:
    static const int TF_LINEAR       = 0x0;
    static const int TF_CUBIC_IN     = 0x1;
    static const int TF_CUBIC_OUT    = 0x2;
    static const int TF_CUBIC_IN_OUT = 0x3;

    AminoAnim(): AminoJSObject(getFactory()->name) {
        //empty
    }

    ~AminoAnim() {
        //see destroy
    }

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override {
        //params
        AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info[0]->ToObject());
        AminoNode *node = Nan::ObjectWrap::Unwrap<AminoNode>(info[1]->ToObject());
        unsigned int propId = info[2]->Uint32Value();

        assert(obj);
        assert(node);

        if (!node->checkRenderer(obj)) {
            return;
        }

        //get property
        AnyProperty *prop = node->getPropertyWithId(propId);

        if (!prop || prop->type != PROPERTY_FLOAT) {
            Nan::ThrowTypeError("property cannot be animated");
            return;
        }

        //bind to queue (retains AminoGfx reference)
        this->setEventHandler(obj);
        this->prop = prop;

        //retain property
        prop->retain();

        //enqueue
        obj->addAnimationAsync(this);
    }

    void destroy() override {
        AminoJSObject::destroy();

        if (prop) {
            prop->release();
            prop = NULL;
        }

        if (then) {
            delete then;
            then = NULL;
        }
    }

    //creation

    /**
     * Create anim factory.
     */
    static AminoAnimFactory* getFactory() {
        static AminoAnimFactory *animFactory = NULL;

        if (!animFactory) {
            animFactory = new AminoAnimFactory(New);
        }

        return animFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //methods
        Nan::SetPrototypeMethod(tpl, "_start", Start);
        Nan::SetPrototypeMethod(tpl, "stop", Stop);

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    static NAN_METHOD(Start) {
        AminoAnim *obj = Nan::ObjectWrap::Unwrap<AminoAnim>(info.This());
        v8::Local<v8::Object> data = info[0]->ToObject();

        assert(obj);

        obj->handleStart(data);
    }

    void handleStart(v8::Local<v8::Object> &data) {
        if (started) {
            Nan::ThrowTypeError("already started");
            return;
        }

        //parameters
        start       = Nan::Get(data, Nan::New<v8::String>("from").ToLocalChecked()).ToLocalChecked()->NumberValue();
        end         = Nan::Get(data, Nan::New<v8::String>("to").ToLocalChecked()).ToLocalChecked()->NumberValue();
        duration    = Nan::Get(data, Nan::New<v8::String>("duration").ToLocalChecked()).ToLocalChecked()->NumberValue();
        count       = Nan::Get(data, Nan::New<v8::String>("count").ToLocalChecked()).ToLocalChecked()->IntegerValue();
        autoreverse = Nan::Get(data, Nan::New<v8::String>("autoreverse").ToLocalChecked()).ToLocalChecked()->BooleanValue();

        //time func
        v8::String::Utf8Value str(Nan::Get(data, Nan::New<v8::String>("timeFunc").ToLocalChecked()).ToLocalChecked());
        std::string tf = std::string(*str);

        if (tf == "cubicIn") {
            timeFunc = TF_CUBIC_IN;
        } else if (tf == "cubicOut") {
            timeFunc = TF_CUBIC_OUT;
        } else if (tf == "cubicInOut") {
            timeFunc = TF_CUBIC_IN_OUT;
        } else {
            timeFunc = TF_LINEAR;
        }

        //then
        v8::MaybeLocal<v8::Value> maybeThen = Nan::Get(data, Nan::New<v8::String>("then").ToLocalChecked());

        if (!maybeThen.IsEmpty()) {
            v8::Local<v8::Value> thenLocal = maybeThen.ToLocalChecked();

            if (thenLocal->IsFunction()) {
                then = new Nan::Callback(thenLocal.As<v8::Function>());
            }
        }

        //start
        started = true;
    }

    /**
     * Cubic-in time function.
     */
    static float cubicIn(float t) {
        return pow(t, 3);
    }

    /**
     * Cubic-out time function.
     */
    static float cubicOut(float t) {
        return 1 - cubicIn(1 - t);
    }

    /**
     * Cubic-in-out time function.
     */
    static float cubicInOut(float t) {
        if (t < 0.5) {
            return cubicIn(t * 2.0) / 2.0;
        }

        return 1 - cubicIn((1 - t) * 2) / 2;
    }

    /**
     * Call time function.
     */
    float timeToPosition(float t) {
        float t2 = 0;

        switch (timeFunc) {
            case TF_CUBIC_IN:
                t2 = cubicIn(t);
                break;

            case TF_CUBIC_OUT:
                t2 = cubicOut(t);
                break;

            case TF_CUBIC_IN_OUT:
                t2 = cubicInOut(t);
                break;

            case TF_LINEAR:
            default:
                t2 = t;
                break;
        }

        return start + (end - start) * t2;
    }

    /**
     * Toggle animation direction.
     *
     * Note: works only if autoreverse is enabled
     */
    void toggle() {
        if (autoreverse) {
            if (direction == FORWARD) {
                direction = BACKWARD;
            } else {
                direction = FORWARD;
            }
        }
    }

    /**
     * Apply animation value.
     *
     * @param value current property value.
     */
    void applyValue(float value) {
        if (!prop) {
            return;
        }

        FloatProperty *floatProp = (FloatProperty *)prop;

        floatProp->setValue(value);
    }

    //TODO pause
    //TODO resume
    //TODO reset (start from beginning)

    static NAN_METHOD(Stop) {
        AminoAnim *obj = Nan::ObjectWrap::Unwrap<AminoAnim>(info.This());

        assert(obj);

        obj->stop();
    }

    /**
     * Stop animation.
     *
     * Note: has to be called on main thread!
     */
    void stop() {
        if (!destroyed) {
            //remove animation
            if (eventHandler) {
                ((AminoGfx *)eventHandler)->removeAnimationAsync(this);
            }

            //free resources
            destroy();
        }
    }

    /**
     * End the animation.
     */
    void endAnimation() {
        if (ended) {
            return;
        }

        if (DEBUG_BASE) {
            printf("Anim: endAnimation()\n");
        }

        ended = true;

        //apply end state
        applyValue(end);

        //callback function
        if (then) {
            if (DEBUG_BASE) {
                printf("-> callback used\n");
            }

            enqueueJSCallbackUpdate((jsUpdateCallback)&AminoAnim::callThen, NULL, NULL);
        }

        //stop
        enqueueJSCallbackUpdate((jsUpdateCallback)&AminoAnim::callStop, NULL, NULL);
    }

    /**
     * Perform then() call on main thread.
     */
    void callThen(JSCallbackUpdate *update) {
        //create scope
        Nan::HandleScope scope;

        //call
        then->Call(handle(), 0, NULL);
    }

    /**
     * Perform stop() call on main thread.
     */
    void callStop(JSCallbackUpdate *update) {
        stop();
    }

    void update(double currentTime) {
        //check active
    	if (!started || ended) {
            return;
        }

        //check remaining loops
        if (count == 0) {
            endAnimation();
            return;
        }

        //handle first start
        if (startTime == 0) {
            startTime = currentTime;
            lastTime = currentTime;
            pauseTime = 0;
        }

        //validate time
        if (currentTime < startTime) {
            //smooth animation
            startTime = currentTime - (lastTime - startTime);
            lastTime = currentTime;
        }

        //process
        float t = (currentTime - startTime) / duration;

        lastTime = currentTime;

        if (t > 1) {
            //end reached
            bool doToggle = false;

            if (count == FOREVER) {
                doToggle = true;
            }

            if (count > 0) {
                count--;

                if (count > 0) {
                    doToggle = true;
                } else {
                    endAnimation();
                    return;
                }
            }

            if (doToggle) {
                //next cycle
                startTime = currentTime;
                t = 0;
                toggle();
            } else {
                //end position
                t = 1;
            }
        }

        if (direction == BACKWARD) {
            t = 1 - t;
        }

        //apply time function
        float value = timeToPosition(t);

        applyValue(value);
    }
};

/**
 * Rect factory.
 */
class AminoRectFactory : public AminoJSObjectFactory {
public:
    AminoRectFactory(Nan::FunctionCallback callback, bool hasImage);

    AminoJSObject* create() override;

private:
    bool hasImage;
};

/**
 * Rectangle node class.
 */
class AminoRect : public AminoNode {
public:
    bool hasImage;

    //color (no texture)
    FloatProperty *propR;
    FloatProperty *propG;
    FloatProperty *propB;

    //texture (image only)
    ObjectProperty *propTexture;

    //texture offset (image only)
    FloatProperty *propLeft;
    FloatProperty *propRight;
    FloatProperty *propTop;
    FloatProperty *propBottom;

    AminoRect(bool hasImage): AminoNode(hasImage ? getImageViewFactory()->name:getRectFactory()->name, RECT) {
        this->hasImage = hasImage;
    }

    ~AminoRect() {
        //empty
    }

    /**
     * Free resources.
     */
    void destroy() override {
        AminoNode::destroy();

        //release object values
        propTexture->destroy();
    }

    /**
     * Setup rect properties.
     */
    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        if (hasImage) {
            propTexture = createObjectProperty("image");

            propLeft = createFloatProperty("left");
            propRight = createFloatProperty("right");
            propTop = createFloatProperty("top");
            propBottom = createFloatProperty("bottom");
        } else {
            propR = createFloatProperty("r");
            propG = createFloatProperty("g");
            propB = createFloatProperty("b");
        }
    }

    //creation

    /**
     * Get rect factory.
     */
    static AminoRectFactory* getRectFactory() {
        static AminoRectFactory *rectFactory = NULL;

        if (!rectFactory) {
            rectFactory = new AminoRectFactory(NewRect, false);
        }

        return rectFactory;
    }

    /**
     * Initialize Rect template.
     */
    static v8::Local<v8::Function> GetRectInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getRectFactory());

        //no methods

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(NewRect) {
        AminoJSObject::createInstance(info, getRectFactory());
    }

    //ImageView

    //creation

    /**
     * Get image view factory.
     */
    static AminoRectFactory* getImageViewFactory() {
        static AminoRectFactory *rectFactory = NULL;

        if (!rectFactory) {
            rectFactory = new AminoRectFactory(NewImageView, true);
        }

        return rectFactory;
    }

    /**
     * Initialize ImageView template.
     */
    static v8::Local<v8::Function> GetImageViewInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getImageViewFactory());

        //no methods

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(NewImageView) {
        AminoJSObject::createInstance(info, getImageViewFactory());
    }
};

/**
 * Polygon factory.
 */
class AminoPolygonFactory : public AminoJSObjectFactory {
public:
    AminoPolygonFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * AminoPolygon node class.
 */
class AminoPolygon : public AminoNode {
public:
    //fill
    FloatProperty *propFillR;
    FloatProperty *propFillG;
    FloatProperty *propFillB;

    //dimension
    UInt32Property *propDimension;
    BooleanProperty *propFilled;

    //points
    FloatArrayProperty *propGeometry;

    AminoPolygon(): AminoNode(getFactory()->name, POLY) {
        //empty
    }

    ~AminoPolygon() {
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propFillR = createFloatProperty("fillR");
        propFillG = createFloatProperty("fillG");
        propFillB = createFloatProperty("fillB");

        propDimension = createUInt32Property("dimension");
        propFilled = createBooleanProperty("filled");

        propGeometry = createFloatArrayProperty("geometry");
    }

    //creation

    /**
     * Get polygon factory.
     */
    static AminoPolygonFactory* getFactory() {
        static AminoPolygonFactory *polygonFactory = NULL;

        if (!polygonFactory) {
            polygonFactory = new AminoPolygonFactory(New);
        }

        return polygonFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //no methods

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }
};

/**
 * Model factory.
 */
class AminoModelFactory : public AminoJSObjectFactory {
public:
    AminoModelFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * AminoModel node class.
 */
class AminoModel : public AminoNode {
public:
    //fill
    FloatProperty *propFillR;
    FloatProperty *propFillG;
    FloatProperty *propFillB;

    //arrays
    FloatArrayProperty *propVertices;

    AminoModel(): AminoNode(getFactory()->name, MODEL) {
        //empty
    }

    ~AminoModel() {
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        propFillR = createFloatProperty("fillR");
        propFillG = createFloatProperty("fillG");
        propFillB = createFloatProperty("fillB");

        propVertices = createFloatArrayProperty("vertices");
    }

    //creation

    /**
     * Get polygon factory.
     */
    static AminoModelFactory* getFactory() {
        static AminoModelFactory *modelFactory = NULL;

        if (!modelFactory) {
            modelFactory = new AminoModelFactory(New);
        }

        return modelFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //no methods

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }
};

/**
 * Group factory.
 */
class AminoGroupFactory : public AminoJSObjectFactory {
public:
    AminoGroupFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Group node.
 *
 * Special: supports clipping
 */
class AminoGroup : public AminoNode {
public:
    //internal
    std::vector<AminoNode *> children;

    //properties
    BooleanProperty *propCliprect;
    BooleanProperty *propDepth;

    AminoGroup(): AminoNode(getFactory()->name, GROUP) {
        //empty
    }

    ~AminoGroup() {
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        propCliprect = createBooleanProperty("cliprect");
        propDepth = createBooleanProperty("depth");
    }

    //creation

    /**
     * Get group factory.
     */
    static AminoGroupFactory* getFactory() {
        static AminoGroupFactory *groupFactory = NULL;

        if (!groupFactory) {
            groupFactory = new AminoGroupFactory(New);
        }

        return groupFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::Function> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //prototype methods
        Nan::SetPrototypeMethod(tpl, "_add", Add);
        Nan::SetPrototypeMethod(tpl, "_remove", Remove);

        //template function
        return Nan::GetFunction(tpl).ToLocalChecked();
    }

private:
    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    static NAN_METHOD(Add) {
        AminoGroup *group = Nan::ObjectWrap::Unwrap<AminoGroup>(info.This());
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(info[0]->ToObject());

        assert(group);
        assert(child);

        if (!child->checkRenderer(group)) {
            return;
        }

        //handle async
        group->enqueueValueUpdate(child, (asyncValueCallback)&AminoGroup::addChild);
    }

    /**
     * Add a child node.
     */
    void addChild(AsyncValueUpdate *update, int state) {
        if (state != AsyncValueUpdate::STATE_APPLY) {
            return;
        }

        AminoNode *node = (AminoNode *)update->valueObj;

        //keep retained instance
        update->valueObj = NULL;

        if (DEBUG_BASE) {
            printf("-> addChild()\n");
        }

        children.push_back(node);
    }

    static NAN_METHOD(Remove) {
        AminoGroup *group = Nan::ObjectWrap::Unwrap<AminoGroup>(info.This());
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(info[0]->ToObject());

        assert(group);
        assert(child);

        //handle async
        group->enqueueValueUpdate(child, (asyncValueCallback)&AminoGroup::removeChild);
    }

    void removeChild(AsyncValueUpdate *update, int state) {
        if (state != AsyncValueUpdate::STATE_APPLY) {
            return;
        }

        if (DEBUG_BASE) {
            printf("-> removeChild()\n");
        }

        AminoNode *node = (AminoNode *)update->valueObj;

        //remove pointer
        std::vector<AminoNode *>::iterator pos = std::find(children.begin(), children.end(), node);

        if (pos != children.end()) {
            children.erase(pos);

            //remove strong reference (on main thread)
            update->releaseLater = node;
        }
    }
};

//font shader

typedef struct {
    float x, y, z;    // position
    float s, t;       // texture pos
} vertex_t;

#endif
