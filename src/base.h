#ifndef _AMINOBASE_H
#define _AMINOBASE_H

#include "gfx.h"
#include "base_js.h"
#include "fonts.h"
#include "images.h"

using namespace node;

#include <uv.h>
#include "shaders.h"
#include "mathutils.h"
#include <stdio.h>
#include <string.h>
#include <vector>
#include <stack>
#include <stdlib.h>
#include <string>
#include <string.h>
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
const int RECT = 2;
const int TEXT = 3;
const int ANIM = 4;
const int POLY = 5;
const int INVALID = -1;

static const int FOREVER = -1;

//properties

static const int SCALE_X_PROP  =  2;
static const int SCALE_Y_PROP  =  3;
static const int ROTATE_Z_PROP =  4;
static const int R_PROP        =  5;
static const int G_PROP        =  6;
static const int B_PROP        =  7;
static const int TEXID_PROP    =  8;
static const int TEXT_PROP     =  9;
static const int W_PROP        = 10;
static const int H_PROP        = 11;
static const int FONTSIZE_PROP = 12;

static const int LERP_PROP = 16;

static const int VISIBLE_PROP = 18;
static const int ROTATE_X_PROP = 19;
static const int ROTATE_Y_PROP = 20;

static const int X_PROP = 21;
static const int Y_PROP = 22;
static const int GEOMETRY_PROP = 24;
static const int FILLED_PROP = 25;

static const int OPACITY_PROP = 27;
static const int FONTID_PROP = 28;

static const int COUNT_PROP = 29;

static const int TEXTURE_LEFT_PROP   = 30;
static const int TEXTURE_RIGHT_PROP  = 31;
static const int TEXTURE_TOP_PROP    = 32;
static const int TEXTURE_BOTTOM_PROP = 33;

static const int CLIPRECT_PROP = 34;
static const int AUTOREVERSE_PROP = 35;
static const int DIMENSION_PROP = 36;
static const int THEN_PROP = 37;
static const int STOP_PROP = 38;

static const int TEXT_VALIGN_PROP = 40;
static const int TEXT_WRAP_PROP   = 41;

//property values

static const int VALIGN_BASELINE = 0x0;
static const int VALIGN_TOP      = 0x1;
static const int VALIGN_MIDDLE   = 0x2;
static const int VALIGN_BOTTOM   = 0x3;

static const int WRAP_NONE = 0x0;
static const int WRAP_END  = 0x1;
static const int WRAP_WORD = 0x2;

extern std::map<int, AminoFont *> fontmap;

#define ID_ADD_CHILD    100
#define ID_REMOVE_CHILD 101

class Group;
class Anim;

/**
 * Amino main class to call from JavaScript.
 *
 * Note: abstract
 */
class AminoGfx : public AminoJSObject {
public:
    AminoGfx(std::string name);
    virtual ~AminoGfx();

    void addAnimation(Anim *anim);
    void removeAnimation(Anim *anim);

protected:
    bool started = false;
    bool destroyed = false;
    bool rendering = false;
    Nan::Callback *startCallback = NULL;

    //renderer
    Group *root = NULL;
    int viewportW;
    int viewportH;
    ColorShader *colorShader;
    TextureShader *textureShader;
    GLfloat *modelView;

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
    std::vector<Anim *> animations;

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

    virtual void render();
    void processAnimations();
    virtual void bindContext() = 0;
    virtual void setupViewport();
    virtual void renderScene();
    virtual void renderingDone() = 0;
    bool isRendering();

    virtual void destroy();

    virtual bool getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) { return false; };
    void updateSize(int w, int h); //call after size event
    void updatePosition(int x, int y); //call after position event

    void fireEvent(v8::Local<v8::Object> &obj);

    void handleAsyncUpdate(AnyProperty *property, v8::Local<v8::Value> value) override;
    virtual void updateWindowSize() = 0;
    virtual void updateWindowPosition() = 0;
    virtual void updateWindowTitle() = 0;

    void setRoot(Group *group);

private:
    //JS methods
    static NAN_METHOD(Start);
    static NAN_METHOD(Destroy);
    static NAN_METHOD(Tick);
    static NAN_METHOD(InitColorShader);
    static NAN_METHOD(InitTextureShader);
    static NAN_METHOD(SetRoot);

    //GL
    static v8::Local<v8::Object> createGLObject();
};

/**
 * Base class for all rendering nodes.
 *
 * Note: abstract.
 */
class AminoNode : public AminoJSObject {
public:
    AminoGfx *amino = NULL;
    int type;

    //location
    FloatProperty *propX;
    FloatProperty *propY;

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

    virtual ~AminoNode() {
        //destroy (if not called before)
        destroy();
    }

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override {
        //set amino instance
        v8::Local<v8::Object> jsObj = info[0]->ToObject();
        AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(jsObj);

        this->amino = obj;
        Nan::Set(handle(), Nan::New("amino").ToLocalChecked(), jsObj);

        //bind to queue
        this->attachToAsyncQueue(obj);
    }

    void setup() override {
        AminoJSObject::setup();

        //register native properties
        propX = createFloatProperty("x");
        propY = createFloatProperty("y");
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
    virtual void destroy() {
        //to be overwritten
    }

    /**
     * Validate renderer instance. Must be called in JS method handler.
     */
    bool checkRenderer(AminoNode *node) {
        return checkRenderer(node->amino);
    }

    /**
     * Validate renderer instance. Must be called in JS method handler.
     */
    bool checkRenderer(AminoGfx *amino) {
        if (this->amino != amino) {
            Nan::ThrowTypeError("invalid renderer");
            return false;
        }

        return true;
    }
};

/**
 * Convert a v8::String to a (char*).
 *
 * Note: Any call to this should later be free'd. Never returns null.
 */
static inline char *TO_CHAR(v8::Handle<v8::Value> val) {
    v8::String::Utf8Value utf8(val);
    int len = utf8.length() + 1;
    char *str = (char *)calloc(sizeof(char), len);

    strncpy(str, *utf8, len);

    return str;
}

/**
 * Get wide char string.
 *
 * Note: any call to this should later be free'd
 */
static wchar_t* GetWC(const char *c) {
    const size_t cSize = strlen(c) + 1;
    wchar_t *wc = new wchar_t[cSize];

    mbstowcs (wc, c, cSize);

    return wc;
}

extern std::vector<AminoNode *> rects;

/**
 * Display a warning and exit application.
 */
static void warnAbort(char const *str) {
    printf("%s\n", str);
    exit(-1);
}

/**
 * Text node class.
 */
class TextNode : public AminoNode {
public:
    //text
    std::wstring text;

    //color
    float r;
    float g;
    float b;

    //box
    float w;
    float h;
    int wrap;

    //font
    int fontid;
    int fontsize;
    vertex_buffer_t *buffer;
    int vAlign;
    int lineNr;

    TextNode(std::string name): AminoNode(name, TEXT) {
        //white color
        r = 1.0;
        g = 1.0;
        b = 1.0;

        //box
        w = 0;
        h = 0;
        wrap = WRAP_NONE;

        //properties
        text = L"";
        fontsize = 20;
        fontid = INVALID;
        buffer = NULL;
        vAlign = VALIGN_BASELINE;
    }

    virtual ~TextNode() {
    }

    /**
     * Update the rendered text.
     */
    void refreshText();

    void destroy() {
        if (DEBUG_BASE) {
            printf("TextNode: destroy()\n");
        }

        AminoNode::destroy();

        if (buffer) {
            vertex_buffer_delete(buffer);
            buffer = NULL;
        }
    }
};

/**
 * Animation factory.
 */
class AnimFactory : public AminoJSObjectFactory {
public:
    AnimFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Animation class.
 */
class Anim : public AminoJSObject {
private:
    AminoGfx *amino;
    AnyProperty *prop;

    bool started = false;
    bool destroyed = false;

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

public:
    static const int TF_LINEAR       = 0x0;
    static const int TF_CUBIC_IN     = 0x1;
    static const int TF_CUBIC_OUT    = 0x2;
    static const int TF_CUBIC_IN_OUT = 0x3;

    Anim(): AminoJSObject(getFactory()->name) {
        //empty
    }

    ~Anim() {
        if (!destroyed) {
            destroy();
        }
    }

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override {
        //params
        AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(info[0]->ToObject());
        AminoNode *node = Nan::ObjectWrap::Unwrap<AminoNode>(info[1]->ToObject());
        int propId = info[2]->Uint32Value();
//cbx check destroyed
        if (!node->checkRenderer(obj)) {
            return;
        }

        //get property
        AnyProperty *prop = node->getPropertyWithId(propId);

        if (!prop || prop->type != PROPERTY_FLOAT) {
            Nan::ThrowTypeError("property cannot be animated");
            return;
        }

        this->amino = obj;
        this->prop = prop;

        //retain property
        prop->retain();

        //enqueue
        obj->addAnimation(this);
    }

    virtual void destroy() {
        destroyed = true;

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
    static AnimFactory* getFactory() {
        static AnimFactory *animFactory;

        if (!animFactory) {
            animFactory = new AnimFactory(New);
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
        Anim *obj = Nan::ObjectWrap::Unwrap<Anim>(info.This());
        v8::Local<v8::Object> data = info[0]->ToObject();

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
        Anim *obj = Nan::ObjectWrap::Unwrap<Anim>(info.This());

        obj->stop();
    }

    void stop() {
        if (!destroyed) {
            //remove animation
            if (amino) {
                amino->removeAnimation(this);
            }

            //free resources
            destroy();
        }
    }

    void endAnimation() {
        if (DEBUG_BASE) {
            printf("Anim: endAnimation()\n");
        }

        //apply end state
        applyValue(end);

        //callback function
        if (then) {
            if (DEBUG_BASE) {
                printf("-> callback used\n");
            }

            //create scope
            Nan::HandleScope scope;

            //call
            then->Call(handle(), 0, NULL);
        }

        //stop
        stop();
    }

    void update(double currentTime) {
        //check active
    	if (!started) {
            return;
        }

        //check remaining loops
        if (count == 0) {
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
class RectFactory : public AminoJSObjectFactory {
public:
    RectFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Rectangle node class.
 */
class Rect : public AminoNode {
public:
    //size
    FloatProperty *propW;
    FloatProperty *propH;

    //color
    FloatProperty *propR;
    FloatProperty *propG;
    FloatProperty *propB;

    //offset
    FloatProperty *propLeft;
    FloatProperty *propRight;
    FloatProperty *propTop;
    FloatProperty *propBottom;

    //image
    bool hasImage;
    int texid;

    Rect(bool hasImage): AminoNode(getFactory()->name, RECT) {
        //image
        texid = INVALID;
        this->hasImage = hasImage;
    }

    virtual ~Rect() {
        //empty
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");
        propR = createFloatProperty("r");
        propG = createFloatProperty("g");
        propB = createFloatProperty("b");

        if (hasImage) {
            propLeft = createFloatProperty("left");
            propRight = createFloatProperty("right");
            propTop = createFloatProperty("top");
            propBottom = createFloatProperty("bottom");
        }
    }

    //creation
    static RectFactory* getFactory() {
        static RectFactory *rectFactory;

        if (!rectFactory) {
            rectFactory = new RectFactory(New);
        }

        return rectFactory;
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
 * Polygon node class.
 */
class PolyNode : public AminoNode {
private:
    std::vector<float> *geometry;

public:
    float r;
    float g;
    float b;
    int dimension;
    int filled;

    PolyNode(std::string name): AminoNode(name, POLY) {
        //color: green
        r = 0;
        g = 1;
        b = 0;

        //not filled
        filled = 0;

        //default: 2D, empty
        dimension = 2;
        geometry = new std::vector<float>();
    }

    virtual ~PolyNode() {
    }

    /**
     * Set list of coordinates.
     */
    void setGeometry(std::vector<float> *arr) {
        if (geometry == arr) {
            return;
        }

        if (geometry) {
            delete geometry;
        }

        geometry = arr;
    }

    /**
     * Get list of coordinates.
     */
    std::vector<float>* getGeometry() {
        return geometry;
    }

    /**
     * Free memory.
     */
    void destroy() {
        if (DEBUG_BASE) {
            printf("PolyNode: destroy()\n");
        }

        //super
        AminoNode::destroy();

        //this
        if (geometry) {
            delete geometry;
            geometry = NULL;
        }
    }
};

/**
 * Group factory.
 */
class GroupFactory : public AminoJSObjectFactory {
public:
    GroupFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Group node.
 *
 * Special: supports clipping
 */
class Group : public AminoNode {
public:
    //internal
    std::vector<AminoNode *> children;

    //properties
    FloatProperty *propW;
    FloatProperty *propH;
    BooleanProperty *propCliprect;

    Group(): AminoNode(getFactory()->name, GROUP) {
        //empty
    }

    ~Group() {
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");
        propCliprect = createBooleanProperty("cliprect");
    }

    //creation
    static GroupFactory* getFactory() {
        static GroupFactory *groupFactory;

        if (!groupFactory) {
            groupFactory = new GroupFactory(New);
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
        Group *group = Nan::ObjectWrap::Unwrap<Group>(info.This());
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(info[0]->ToObject());

        if (!child->checkRenderer(group)) {
            return;
        }

        //handle async
        group->enqueueValueUpdate(ID_ADD_CHILD, child);
    }

    /**
     * Add a child node.
     */
    void addChild(AminoNode *node) {
        if (DEBUG_BASE) {
            printf("-> addChild()\n");
        }

        children.push_back(node);

        //strong reference
        node->retain();
    }

    static NAN_METHOD(Remove) {
        Group *group = Nan::ObjectWrap::Unwrap<Group>(info.This());
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(info[0]->ToObject());

        //handle async
        group->enqueueValueUpdate(ID_REMOVE_CHILD, child);
    }

    void removeChild(AminoNode *node) {
        if (DEBUG_BASE) {
            printf("-> removeChild()\n");
        }

        //remove pointer
        std::vector<AminoNode *>::iterator pos = std::find(children.begin(), children.end(), node);

        if (pos != children.end()) {
            children.erase(pos);

            //remove strong reference
            node->release();
        }
    }

    /**
     * Async value handling.
     */
    bool handleAsyncUpdate(AsyncValueUpdate *update) override {
        if (AminoJSObject::handleAsyncUpdate(update)) {
            return true;
        }

        switch (update->id) {
            case ID_ADD_CHILD:
                addChild((AminoNode *)update->valueObj);
                return true;

            case ID_REMOVE_CHILD:
                removeChild((AminoNode *)update->valueObj);
                return true;
        }

        return false;
    }
};

/**
 * Updates for next animation cycle.
 *
 * Note: destroy() has to be called to free memory if parameters were not passed!
 *
 * TODO use better OOP way
 */
class Update {
public:
    int type;
    int node;
    int property;

    //values
    float value;
    std::wstring text;
    std::vector<float> *arr;
    Nan::Callback *callback;

    Update(int Type, int Node, int Property, float Value, std::wstring Text, std::vector<float> *Arr, Nan::Callback *Callback) {
        type = Type;
        node = Node;
        property = Property;
        value = Value;
        text = Text;
        arr = Arr;
        callback = Callback;
    }

    ~Update() { }

    /**
     * Free memory if update() was not called.
     */
    void destroy() {
        if (DEBUG_BASE) {
            printf("Update: destroy()\n");
        }

        if (arr) {
            delete[] arr;
            arr = NULL;
        }

        if (callback) {
            delete callback;
            callback = NULL;
        }
    }

    void apply() {
        //node
        AminoNode *target = rects[node];

        if (target->type == TEXT) {
            //text
            TextNode *textnode = (TextNode *)target;
            bool refresh = false;

            switch (property) {
                case R_PROP:
                    textnode->r = value;
                    break;

                case G_PROP:
                    textnode->g = value;
                    break;

                case B_PROP:
                    textnode->b = value;
                    break;

                case W_PROP:
                    textnode->w = value;
                    refresh = true;
                    break;

                case H_PROP:
                    textnode->h = value;
                    break;

                case TEXT_PROP:
                    textnode->text = text;
                    refresh = true;
                    break;

                case FONTSIZE_PROP:
                    textnode->fontsize = value;
                    refresh = true;
                    break;

                case FONTID_PROP:
                    textnode->fontid = value;
                    refresh = true;
                    break;

                case TEXT_VALIGN_PROP:
                    textnode->vAlign = (int)value;
                    break;

                case TEXT_WRAP_PROP:
                    textnode->wrap = (int)value;
                    refresh = true;
                    break;

                default:
                    printf("Unknown anim text update: %i\n", property);
                    break;
            }

            if (refresh) {
                textnode->refreshText();
            }
        } else if (target->type == POLY) {
            //poly
            PolyNode *polynode = (PolyNode *)target;

            switch (property) {
                case R_PROP:
                    polynode->r = value;
                    break;

                case G_PROP:
                    polynode->g = value;
                    break;

                case B_PROP:
                    polynode->b = value;
                    break;

                case GEOMETRY_PROP:
                    polynode->setGeometry(arr);
                    break;

                case DIMENSION_PROP:
                    polynode->dimension = value;
                    break;

                case FILLED_PROP:
                    polynode->filled = value;
                    break;

                default:
                    printf("Unknown anim poly update: %i\n", property);
                    break;
            }
        }
    }
};

NAN_METHOD(createPoly);
NAN_METHOD(createText);

/**
 * Get wstring from v8 string.
 *
 * Note: automatically free'd
 */
static std::wstring GetWString(v8::Handle<v8::String> str) {
    std::wstring wstr = L"";
    int len = str->Length();
    uint16_t *buf = new uint16_t[len + 1];

    str->Write(buf);

    for (int i = 0; i < len + 1; i++) {
        wstr.push_back(buf[i]);
    }

    delete[] buf;

    return wstr;
}

/**
 * Convert v8 float array to float vector.
 *
 * Note: delete after use
 */
static std::vector<float>* GetFloatArray(v8::Handle<v8::Array> obj) {
    v8::Handle<v8::Array> oarray = v8::Handle<v8::Array>::Cast(obj);
    std::vector<float>* carray = new std::vector<float>();

    for (std::size_t i = 0; i < oarray->Length(); i++) {
        carray->push_back((float)(oarray->Get(i)->ToNumber()->NumberValue()));
    }

    return carray;
}

//JavaScript bindings

NAN_METHOD(loadBufferToTexture);
NAN_METHOD(getFontHeight);
NAN_METHOD(getFontAscender);
NAN_METHOD(getFontDescender);
NAN_METHOD(getCharWidth);
NAN_METHOD(createNativeFont);
NAN_METHOD(getTextLineCount);
NAN_METHOD(getTextHeight);

typedef struct {
    float x, y, z;    // position
    float s, t;       // texture
} vertex_t;

//OpenGL JavaScript bindings

NAN_METHOD(node_glCreateShader);
NAN_METHOD(node_glShaderSource);
NAN_METHOD(node_glCompileShader);
NAN_METHOD(node_glGetShaderiv);
NAN_METHOD(node_glGetProgramiv);
NAN_METHOD(node_glGetShaderInfoLog);
NAN_METHOD(node_glGetProgramInfoLog);
NAN_METHOD(node_glCreateProgram);
NAN_METHOD(node_glAttachShader);
NAN_METHOD(node_glDetachShader);
NAN_METHOD(node_glDeleteShader);
NAN_METHOD(node_glLinkProgram);
NAN_METHOD(node_glUseProgram);
NAN_METHOD(node_glGetAttribLocation);
NAN_METHOD(node_glGetUniformLocation);

#endif
