#ifndef AMINOBASE
#define AMINOBASE

#include "gfx.h"

#include <node.h>
#include <node_buffer.h>
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

#define DEBUG_BASE false

const int GROUP = 1;
const int RECT = 2;
const int TEXT = 3;
const int ANIM = 4;
const int POLY = 5;
const int GLNODE = 6;
const int INVALID = -1;
const int WINDOW = 7;

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

static const int TEXT_VALIGN_PROP = 40;
static const int TEXT_WRAP_PROP   = 41;

//property values

static const int LERP_LINEAR       = 0x0;
static const int LERP_CUBIC_IN     = 0x1;
static const int LERP_CUBIC_OUT    = 0x2;
static const int LERP_CUBIC_IN_OUT = 0x3;

static const int VALIGN_BASELINE = 0x0;
static const int VALIGN_TOP      = 0x1;
static const int VALIGN_MIDDLE   = 0x2;
static const int VALIGN_BOTTOM   = 0x3;

static const int WRAP_NONE = 0x0;
static const int WRAP_END  = 0x1;
static const int WRAP_WORD = 0x2;

using namespace v8;

static bool eventCallbackSet = false;
extern int width;
extern int height;

extern ColorShader *colorShader;
extern TextureShader *textureShader;
extern GLfloat *modelView;
extern GLfloat *globaltx;
extern float window_fill_red;
extern float window_fill_green;
extern float window_fill_blue;
extern float window_opacity;

extern std::stack<void *> matrixStack;
extern int rootHandle;
extern std::map<int, AminoFont *> fontmap;
extern Nan::Callback *NODE_EVENT_CALLBACK;

/**
 * Amino main class to call from JavaScript.
 */
class AminoGfx : public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Init) {
        printf("AminoGfx init\n");

        //initialize template
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);

        tpl->SetClassName(Nan::New("AminoGfx").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        //prototype methods
        //TODO SetPrototypeMethod(tpl, "getValue", GetValue);

        constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("AminoGfx").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
    }

private:
    AminoGfx() {
        printf("AminoGfx constructor\n"); //FIXME
    }

    virtual ~AminoGfx() {
        printf("AminoGfx destructor\n"); //FIXME
    }

    static NAN_METHOD(New) {
        if (info.IsConstructCall()) {
            //new AminoGfx()

            //create new instance
            AminoGfx *obj = new AminoGfx();

            obj->Wrap(info.This());

            info.GetReturnValue().Set(info.This());
        } else {
            //direct AminoGfx() call
            const int argc = 0;
            v8::Local<v8::Value> argv[argc] = {};
            v8::Local<v8::Function> cons = Nan::New(constructor());

            info.GetReturnValue().Set(cons->NewInstance(argc, argv));
        }
    }

    static inline Nan::Persistent<v8::Function> & constructor() {
        static Nan::Persistent<v8::Function> my_constructor;

        return my_constructor;
    }
};

/**
 * Base class for all rendering nodes.
 */
class AminoNode {
public:
    int type;

    //location
    float x;
    float y;

    //zoom factor
    float scalex;
    float scaley;

    //rotation
    float rotatex;
    float rotatey;
    float rotatez;

    //opacity
    float opacity;

    //visibility
    int visible;

    AminoNode() {
        type = 0;

        //location
        x = 0;
        y = 0;

        //zoom factor
        scalex = 1;
        scaley = 1;

        //rotation
        rotatex = 0;
        rotatey = 0;
        rotatez = 0;

        //opacity
        opacity = 1;

        //visibility
        visible = 1;
    }

    virtual ~AminoNode() {
        //destroy (if not called before)
        destroy();
    }

    /**
     * Free all resources.
     */
    virtual void destroy() {
        //to be overwritten
    }

};

/**
 * Convert a v8::String to a (char*).
 *
 * Note: Any call to this should later be free'd. Never returns null.
 */
static inline char *TO_CHAR(Handle<Value> val) {
    String::Utf8Value utf8(val->ToString());
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
static wchar_t* GetWC(const char *c)
{
    const size_t cSize = strlen(c) + 1;
    wchar_t *wc = new wchar_t[cSize];

    mbstowcs (wc, c, cSize);

    return wc;
}

extern std::vector<AminoNode*> rects;

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

    //box TODO wrap text
    float w;
    float h;
    int wrap;

    //font
    int fontid;
    int fontsize;
    vertex_buffer_t *buffer;
    int vAlign;
    int lineNr;

    TextNode() {
        type = TEXT;

        //white color
        r = 1.0;
        g = 1.0;
        b = 1.0;
        opacity = 1;

        //box
        w = 0;
        h = 0;
        wrap = WRAP_NONE;

        //properties
        text = L"";
        fontsize = 40;
        fontid = INVALID;
        buffer = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");
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
 * Animation class.
 */
class Anim {
private:
    Nan::Callback *then;
public:
    AminoNode *target;
    float start;
    float end;
    int property;
    int id;
    bool started;
    bool active;
    double startTime;
    double lastTime;
    double pauseTime;
    int loopcount;
    float duration;
    bool autoreverse;
    int direction;
    int lerptype;

    static const int FORWARD = 1;
    static const int BACKWARD = 2;

    Anim(AminoNode *Target, int Property, float Start, float End,
            float Duration) {
        id = -1;

        //properties
        target = Target;
        property = Property;

        start = Start;
        end = End;
        duration = Duration;

        //state
        started = false;
        loopcount = 1;
        autoreverse = false;
        direction = FORWARD;
        lerptype = LERP_LINEAR;
        then = NULL;
        active = true;

        if (DEBUG_RESOURCES) {
            printf("created animation\n");
        }
    }

    virtual ~Anim() {
        if (DEBUG_BASE || DEBUG_RESOURCES) {
            printf("Anim: destructor()\n");
        }

        if (then) {
            delete then;
            then = NULL;
        }
    }

    /**
     * Set then callback value.
     */
    void setThen(Nan::Callback *then) {
        if (this->then == then) {
            return;
        }

        if (this->then) {
            delete this->then;
        }

        this->then = then;
    }

    /**
     * Cubic-in time function.
     */
    float cubicIn(float t) {
        return pow(t, 3);
    }

    /**
     * Cubic-out time function.
     */
    float cubicOut(float t) {
        return 1 - cubicIn(1 - t);
    }

    /**
     * Cubic-in-out time function.
     */
    float cubicInOut(float t) {
        if (t < 0.5) {
            return cubicIn(t * 2.0) / 2.0;
        }

        return 1 - cubicIn((1 - t) * 2) / 2;
    }

    /**
     * Call time function.
     */
    float lerp(float t) {
        float t2 = 0;

        switch (lerptype) {
            case LERP_CUBIC_IN:
                t2 = cubicIn(t);
                break;

            case LERP_CUBIC_OUT:
                t2 = cubicOut(t);
                break;

            case LERP_CUBIC_IN_OUT:
                t2 = cubicInOut(t);
                break;

            //TODO JS callback

            case LERP_LINEAR:
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
        switch (property) {
            //translation
            case X_PROP:
                target->x = value;
                break;

            case Y_PROP:
                target->y = value;
                break;

            //zoom
            case SCALE_X_PROP:
                target->scalex = value;
                break;

            case SCALE_Y_PROP:
                target->scaley = value;
                break;

            //rotation
            case ROTATE_X_PROP:
                target->rotatex = value;
                break;

            case ROTATE_Y_PROP:
                target->rotatey = value;
                break;

            case ROTATE_Z_PROP:
                target->rotatez = value;
                break;

            //opacity
            case OPACITY_PROP:
                target->opacity = value;

                //handle text opacity
                if (target->type == TEXT) {
                    //TODO use fragment shader with opacity value
                    TextNode *textnode = (TextNode *)target;

                    textnode->refreshText();
                };
                break;

            //TODO js callback

            default:
                printf("Unknown animation property: %i\n", property);
                break;
        }
    }

    //TODO pause
    //TODO resume
    //TODO stop (set active)
    //TODO reset (start from beginning)

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

            then->Call(0, NULL);
        }

        //TODO remove instance (needs refactoring; memory leak)

        //deactivate
        active = false;
    }

    void update(double currentTime) {
        //check active
    	if (!active) {
            return;
        }

        //check remaining loops
        if (loopcount == 0) {
            return;
        }

        //handle first start
        if (!started) {
            started = true;
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

            if (loopcount == FOREVER) {
                doToggle = true;
            }

            if (loopcount > 0) {
                loopcount--;

                if (loopcount > 0) {
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
        float value = lerp(t);

        applyValue(value);
    }
};

extern std::vector<Anim *> anims;

/**
 * Rectangle node class.
 */
class Rect : public AminoNode {
public:
    //size
    float w;
    float h;

    //color
    float r;
    float g;
    float b;

    //offset
    float left;
    float right;
    float top;
    float bottom;

    //image
    bool hasImage;
    int texid;

    Rect(bool hasImage) {
        type = RECT;

        //size
        w = 100;
        h = 100;

        //color (white; rect only)
        r = 1;
        g = 1;
        b = 1;

        //opacity
        opacity = 1;

        //image
        texid = INVALID;
        this->hasImage = hasImage;

        //offset (world coordinates: 0..1)
        left = 0;
        bottom = 1;
        right = 1;
        top = 0;
    }

    virtual ~Rect() {
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

    PolyNode() {
        type = POLY;

        //color: green
        r = 0;
        g = 1;
        b = 0;

        opacity = 1;

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
 * Group node.
 *
 * Special: supports clipping
 */
class Group : public AminoNode {
public:
    std::vector<AminoNode *> children;
    float w;
    float h;
    int cliprect;

    Group() {
        type = GROUP;

        //default size
        w = 50;
        h = 50;

        //clipping: off
        cliprect = 0;
    }

    ~Group() {
    }
};

/**
 * OpenGL node.
 *
 * Uses callback.
 */
class GLNode : public AminoNode {
public:
    Persistent<Function> callback;

    GLNode() {
        type = GLNODE;
    }

    ~GLNode() {
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
        //animation
        if (type == ANIM) {
            Anim *anim = anims[node];

            switch (property) {
                case LERP_PROP:
                    anim->lerptype = value;
                    break;

                case COUNT_PROP:
                    anim->loopcount = value;
                    break;

                case AUTOREVERSE_PROP:
                    anim->autoreverse = value;
                    break;

                case THEN_PROP:
                    anim->setThen(callback);
                    break;

                default:
                    printf("Unknown anim update: %i\n", property);
                    break;
            }
            return;
        }

        //window
        if (type == WINDOW) {
            switch (property) {
                case R_PROP:
                    window_fill_red = value;
                    break;

                case G_PROP:
                    window_fill_green = value;
                    break;

                case B_PROP:
                    window_fill_blue = value;
                    break;

                case OPACITY_PROP:
                    window_opacity = value;
                    break;

                default:
                    printf("Unknown anim window update: %i\n", property);
                    break;
            }
            return;
        }

        //node
        AminoNode *target = rects[node];

        switch (property) {
            //origin
            case X_PROP:
                target->x = value;
                return;

            case Y_PROP:
                target->y = value;
                return;

            //scaling factor
            case SCALE_X_PROP:
                target->scalex = value;
                return;

            case SCALE_Y_PROP:
                target->scaley = value;
                return;

            //rotation
            case ROTATE_X_PROP:
                target->rotatex = value;
                return;

            case ROTATE_Y_PROP:
                target->rotatey = value;
                return;

            case ROTATE_Z_PROP:
                target->rotatez = value;
                return;

            //visibility
            case VISIBLE_PROP:
                target->visible = value;
                return;

            case OPACITY_PROP:
                target->opacity = value;
                return;
        }

        if (target->type == RECT) {
            //rect
            Rect *rect = (Rect *)target;

            switch (property) {
                case R_PROP:
                    rect->r = value;
                    break;

                case G_PROP:
                    rect->g = value;
                    break;

                case B_PROP:
                    rect->b = value;
                    break;

                case W_PROP:
                    rect->w = value;
                    break;

                case H_PROP:
                    rect->h = value;
                    break;

                case TEXID_PROP:
                    rect->texid = value;
                    break;

                case TEXTURE_LEFT_PROP:
                    rect->left = value;
                    break;

                case TEXTURE_RIGHT_PROP:
                    rect->right = value;
                    break;

                case TEXTURE_TOP_PROP:
                    rect->top = value;
                    break;

                case TEXTURE_BOTTOM_PROP:
                    rect->bottom = value;
                    break;

                default:
                    printf("Unknown anim rect update: %i\n", property);
                    break;
            }
        } else if (target->type == GROUP) {
            //group
            Group *group = (Group *)target;

            switch (property) {
                case W_PROP:
                    group->w = value;
                    break;

                case H_PROP:
                    group->h = value;
                    break;

                case CLIPRECT_PROP:
                    group->cliprect = value;
                    break;

                default:
                    printf("Unknown anim group update: %i\n", property);
                    break;
            }
        } else if (target->type == TEXT) {
            //text
            TextNode *textnode = (TextNode *)target;

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
                    break;

                case H_PROP:
                    textnode->h = value;
                    break;

                case TEXT_PROP:
                    textnode->text = text;
                    break;

                case FONTSIZE_PROP:
                    textnode->fontsize = value;
                    break;

                case FONTID_PROP:
                    textnode->fontid = value;
                    break;

                case TEXT_VALIGN_PROP:
                    textnode->vAlign = (int)value;
                    break;

                case TEXT_WRAP_PROP:
                    textnode->wrap = (int)value;
                    break;

                default:
                    printf("Unknown anim text update: %i\n", property);
                    break;
            }

            textnode->refreshText();
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

extern std::vector<Update *> updates;

NAN_METHOD(createRect);
NAN_METHOD(createPoly);
NAN_METHOD(createText);
NAN_METHOD(createGroup);
NAN_METHOD(createGLNode);
NAN_METHOD(createAnim);
NAN_METHOD(stopAnim);

/**
 * Get wstring from v8 string.
 *
 * Note: automatically free'd
 */
static std::wstring GetWString(v8::Handle<v8::String> str) {
    std::wstring wstr = L"";
    uint16_t *buf = new uint16_t[str->Length() + 1];

    str->Write(buf);

    for (int i = 0; i < str->Length() + 1; i++) {
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
    Handle<Array>  oarray = Handle<Array>::Cast(obj);
    std::vector<float>* carray = new std::vector<float>();

    for (std::size_t i = 0; i < oarray->Length(); i++) {
        carray->push_back((float)(oarray->Get(i)->ToNumber()->NumberValue()));
    }

    return carray;
}

//JavaScript bindings

NAN_METHOD(updateProperty);
NAN_METHOD(updateWindowProperty);
NAN_METHOD(updateAnimProperty);
NAN_METHOD(addNodeToGroup);
NAN_METHOD(removeNodeFromGroup);
NAN_METHOD(setRoot);
NAN_METHOD(loadBufferToTexture);
NAN_METHOD(decodeJpegBuffer);
NAN_METHOD(decodePngBuffer);
NAN_METHOD(getFontHeight);
NAN_METHOD(getFontAscender);
NAN_METHOD(getFontDescender);
NAN_METHOD(getCharWidth);
NAN_METHOD(createNativeFont);
NAN_METHOD(getTextLineCount);
NAN_METHOD(getTextHeight);
NAN_METHOD(initColorShader);
NAN_METHOD(initTextureShader);

typedef struct {
    float x, y, z;    // position
    float s, t;       // texture
    float r, g, b, a; // color
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
NAN_METHOD(node_glLinkProgram);
NAN_METHOD(node_glUseProgram);
NAN_METHOD(node_glGetAttribLocation);
NAN_METHOD(node_glGetUniformLocation);


struct DebugEvent {
    double inputtime;
    double updatestime;
    double animationstime;
    double rendertime;
    double frametime;
    double framewithsynctime;
};

#endif
