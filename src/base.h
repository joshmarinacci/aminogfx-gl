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
static const int SCALEX = 2;
static const int SCALEY = 3;
static const int ROTATEZ = 4;
static const int R = 5;
static const int G = 6;
static const int B = 7;
static const int TEXID = 8;
static const int TEXT_PROP = 9;
static const int W_PROP = 10;
static const int H_PROP = 11;
static const int FONTSIZE_PROP = 12;

static const int LERP_LINEAR = 13;
static const int LERP_CUBIC_IN = 14;
static const int LERP_CUBIC_OUT = 15;
static const int LERP_PROP = 16;
static const int LERP_CUBIC_IN_OUT = 17;

static const int VISIBLE = 18;
static const int ROTATEX = 19;
static const int ROTATEY = 20;

static const int X_PROP = 21;
static const int Y_PROP = 22;
static const int GEOMETRY = 24;
static const int FILLED = 25;

static const int OPACITY_PROP = 27;
static const int FONTID_PROP = 28;

static const int COUNT = 29;

static const int TEXTURELEFT_PROP   = 30;
static const int TEXTURERIGHT_PROP  = 31;
static const int TEXTURETOP_PROP    = 32;
static const int TEXTUREBOTTOM_PROP = 33;

static const int CLIPRECT_PROP = 34;
static const int AUTOREVERSE = 35;
static const int DIMENSION = 36;
static const int THEN = 37;

using namespace v8;

static bool eventCallbackSet = false;
extern int width;
extern int height;

extern ColorShader* colorShader;
extern TextureShader* textureShader;
extern GLfloat* modelView;
extern GLfloat* globaltx;
extern float window_fill_red;
extern float window_fill_green;
extern float window_fill_blue;
extern float window_opacity;

extern std::stack<void*> matrixStack;
extern int rootHandle;
extern std::map<int,AminoFont*> fontmap;
extern Nan::Callback *NODE_EVENT_CALLBACK;

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
    float r;
    float g;
    float b;
    int fontid;
    int fontsize;
    std::wstring text;
    vertex_buffer_t *buffer;

    TextNode() {
        type = TEXT;

        //white color
        r = 1.0; g = 1.0; b = 1.0;
        opacity = 1;

        //properties
        text = L"";
        fontsize = 40;
        fontid = INVALID;
        buffer = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");
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
    Nan::Callback *then;
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
    }

    virtual ~Anim() {
        if (DEBUG_BASE) {
            printf("Anim: destructor()\n");
        }

        if (then) {
            delete then;
        }
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
            case SCALEX:
                target->scalex = value;
                break;

            case SCALEY:
                target->scaley = value;
                break;

            //rotation
            case ROTATEX:
                target->rotatex = value;
                break;

            case ROTATEY:
                target->rotatey = value;
                break;

            case ROTATEZ:
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
    float w;
    float h;
    float r;
    float g;
    float b;
    float left;
    float right;
    float top;
    float bottom;
    int texid;

    Rect() {
        type = RECT;

        //size
        w = 100;
        h = 100;

        //color (green)
        r = 0;
        g = 1;
        b = 0;

        opacity = 1;
        texid = INVALID;
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
public:
    float r;
    float g;
    float b;
    std::vector<float>* geometry;
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

        //default: 2D triangle
        dimension = 2;
        geometry = new std::vector<float>();
        geometry->push_back(0);
        geometry->push_back(0);
        geometry->push_back(50);
        geometry->push_back(0);
        geometry->push_back(50);
        geometry->push_back(50);
    }

    virtual ~PolyNode() {
    }

    void destroy() {
        if (DEBUG_BASE) {
            printf("PolyNode: destroy()\n");
        }

        AminoNode::destroy();

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

                case COUNT:
                    anim->loopcount = value;
                    break;

                case AUTOREVERSE:
                    anim->autoreverse = value;
                    break;

                case THEN:
                    //FIXME memory leak, old value
                    anim->then = callback;
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
                case R:
                    window_fill_red = value;
                    break;

                case G:
                    window_fill_green = value;
                    break;

                case B:
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
            case SCALEX:
                target->scalex = value;
                return;

            case SCALEY:
                target->scaley = value;
                return;

            //rotation
            case ROTATEX:
                target->rotatex = value;
                return;

            case ROTATEY:
                target->rotatey = value;
                return;

            case ROTATEZ:
                target->rotatez = value;
                return;

            //visibility
            case VISIBLE:
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
                case R:
                    rect->r = value;
                    break;

                case G:
                    rect->g = value;
                    break;

                case B:
                    rect->b = value;
                    break;

                case W_PROP:
                    rect->w = value;
                    break;

                case H_PROP:
                    rect->h = value;
                    break;

                case TEXID:
                    rect->texid = value;
                    break;

                case TEXTURELEFT_PROP:
                    rect->left = value;
                    break;

                case TEXTURERIGHT_PROP:
                    rect->right = value;
                    break;

                case TEXTURETOP_PROP:
                    rect->top = value;
                    break;

                case TEXTUREBOTTOM_PROP:
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
                case R:
                    textnode->r = value;
                    break;

                case G:
                    textnode->g = value;
                    break;

                case B:
                    textnode->b = value;
                    break;

                case TEXT_PROP:
                    //FIME memory leak
                    textnode->text = text;
                    break;

                case FONTSIZE_PROP:
                    textnode->fontsize = value;
                    break;

                case FONTID_PROP:
                    textnode->fontid = value;
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
                case R:
                    polynode->r = value;
                    break;

                case G:
                    polynode->g = value;
                    break;

                case B:
                    polynode->b = value;
                    break;

                case GEOMETRY:
                    //FIXME memory leak
                    polynode->geometry = arr;
                    break;

                case DIMENSION:
                    polynode->dimension = value;
                    break;

                case FILLED:
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
