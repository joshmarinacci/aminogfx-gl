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
extern Nan::Callback* NODE_EVENT_CALLBACK;




class AminoNode {
public:
    float x;
    float y;
    float scalex;
    float scaley;
    float rotatex;
    float rotatey;
    float rotatez;
    float opacity;
    int type;
    int visible;
    AminoNode() {
        x = 0;
        y = 0;
        scalex = 1;
        scaley = 1;
        rotatex = 0;
        rotatey = 0;
        rotatez = 0;
        type = 0;
        visible = 1;
        opacity = 1;
    }
    virtual ~AminoNode() {
    }

};

// convert a v8::String to a (char*) -- any call to this should later be free'd
static inline char *TO_CHAR(Handle<Value> val) {
    String::Utf8Value utf8(val->ToString());

    int len = utf8.length() + 1;
    char *str = (char *) calloc(sizeof(char), len);
    strncpy(str, *utf8, len);

    return str;
}

static wchar_t *GetWC(const char *c)
{
    const size_t cSize = strlen(c)+1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs (wc, c, cSize);

    return wc;
}

extern std::vector<AminoNode*> rects;

static void warnAbort(char * str) {
    printf("%s\n",str);
    exit(-1);
}

class TextNode : public AminoNode {
public:
    float r;
    float g;
    float b;
    int fontid;
    int fontsize;
    std::wstring text;
    vertex_buffer_t * buffer;
    TextNode() {
        r = 1.0; g = 1.0; b = 1.0;
        type = TEXT;
        text = L"foo";
        fontsize = 40;
        fontid = INVALID;
        buffer = vertex_buffer_new( "vertex:3f,tex_coord:2f,color:4f" );
        opacity = 1;
    }
    virtual ~TextNode() {
    }
    void refreshText();
};


class Anim {
public:
    AminoNode* target;
    float start;
    float end;
    int property;
    int id;
    bool started;
    bool active;
    double startTime;
    int loopcount;
    float duration;
    bool autoreverse;
    int direction;
    static const int FORWARD = 1;
    static const int BACKWARD = 2;
    int lerptype;
    Anim(AminoNode* Target, int Property, float Start, float End,
            float Duration) {
        id = -1;
        target = Target;
        start = Start;
        end = End;
        property = Property;
        started = false;
        duration = Duration;
        loopcount = 1;
        autoreverse = false;
        direction = FORWARD;
        lerptype = LERP_LINEAR;
        active = true;
    }

    float cubicIn(float t) {
        return pow(t,3);
    }
    float cubicOut(float t) {
        return 1-cubicIn(1-t);
    }
    float cubicInOut(float t) {
        if(t < 0.5) return cubicIn(t*2.0)/2.0 ;
        return 1-cubicIn((1-t)*2)/2;
    }

    float lerp(float t) {
        if(lerptype != LERP_LINEAR) {
            float t2 = 0;
            if(lerptype == LERP_CUBIC_IN) { t2 = cubicIn(t); }
            if(lerptype == LERP_CUBIC_OUT) { t2 = cubicOut(t); }
            if(lerptype == LERP_CUBIC_IN_OUT) { t2 = cubicInOut(t); }
            return start + (end-start)*t2;
        } else {
            return start + (end-start)*t;
        }
    }

    void toggle() {
        if(autoreverse) {
            if(direction == FORWARD) {
                direction = BACKWARD;
            } else {
                direction = FORWARD;
            }
        }
    }
    void applyValue(float value) {
        if(property == X_PROP) target->x = value;
        if(property == Y_PROP) target->y = value;
        if(property == SCALEX) target->scalex = value;
        if(property == SCALEY) target->scaley = value;
        if(property == ROTATEX) target->rotatex = value;
        if(property == ROTATEY) target->rotatey = value;
        if(property == ROTATEZ) target->rotatez = value;
        if(property == OPACITY_PROP) {
            target->opacity = value;
            if(target->type == TEXT) {
                TextNode* textnode = (TextNode*)target;
                textnode->refreshText();
            };
        }
    }

/*NAN_METHOD(endAnimation) {
     applyValue(end);
     if(!eventCallbackSet) warnAbort("WARNING. Event callback not set");

     v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();
     //Local<Object> event_obj = Object::New();
     Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("animend").ToLocalChecked());
     //event_obj->Set(String::NewSymbol("type"), String::New("animend"));
     Nan::Set(event_obj, Nan::New("id").ToLocalChecked(), Nan::New(id));
     //event_obj->Set(String::NewSymbol("id"), Number::New(id));

     //Handle<Value> event_argv[] = {event_obj};
     //NODE_EVENT_CALLBACK->Call(Context::GetCurrent()->Global(), 1, event_argv);
     info.GetReturnValue().Set(event_obj);
}
*/

void update() {
    	if(!active) return;
        if(loopcount == 0) {
            return;
        }
        if(!started) {
            started = true;
            startTime = getTime();
        }
        double currentTime = getTime();
        float t = (currentTime-startTime)/duration;
        if(t > 1) {
            if(loopcount == FOREVER) {
                startTime = getTime();
                t = 0;
                toggle();
            }
            if(loopcount > 0) {
                loopcount--;
                if(loopcount > 0) {
                    t = 0;
                    startTime = getTime();
                    toggle();
                } else {
                    //endAnimation();
                    return;
                }
            }
        }

        if(direction == BACKWARD) {
            t = 1-t;
        }
        float value = lerp(t);
        applyValue(value);
    }
};

static std::vector<Anim*> anims;



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
        w = 100; h = 100;
        r = 0; g = 1; b = 0;
        opacity = 1;
        type = RECT;
        texid = INVALID;
        left = 0;
        bottom = 1;
        right = 1;
        top = 0;
    }
    virtual ~Rect() {
    }
};

class PolyNode : public AminoNode {
public:
    float r;
    float g;
    float b;
    std::vector<float>* geometry;
    int dimension;
    int filled;

    PolyNode() {
        r = 0; g = 1; b = 0;
        filled = 0;
        geometry = new std::vector<float>();
        geometry->push_back(0);
        geometry->push_back(0);
        geometry->push_back(50);
        geometry->push_back(0);
        geometry->push_back(50);
        geometry->push_back(50);
        opacity = 1;
        type = POLY;
        dimension = 2;
    }
    virtual ~PolyNode() {
    }
};

class Group : public AminoNode {
public:
    std::vector<AminoNode*> children;
    float w;
    float h;
    int cliprect;
    Group() {
        w = 50;
        h = 50;
        cliprect = 0;
        type = GROUP;
    }
    ~Group() {
    }
};

class GLNode : public AminoNode {
public:
    Persistent<Function> callback;
    GLNode() {
        type = GLNODE;
    }
    ~GLNode() {
    }
};

class Update {
public:
    int type;
    int node;
    int property;
    float value;
    std::wstring text;
    std::vector<float>* arr;
    Update(int Type, int Node, int Property, float Value, std::wstring Text, std::vector<float>* Arr) {
        type = Type;
        node = Node;
        property = Property;
        value = Value;
        text = Text;
        arr = Arr;
    }
    ~Update() { }
    void apply() {
        if(type == ANIM) {
            Anim* anim = anims[node];
            if(property == LERP_PROP) {
                anim->lerptype = value;
            }
            if(property == COUNT) {
                anim->loopcount = value;
            }
            if(property == AUTOREVERSE) {
                anim->autoreverse = value;
            }
            return;
        }
        if(type == WINDOW) {
            if(property == R) {
                window_fill_red = value;
            }
            if(property == G) {
                window_fill_green = value;
            }
            if(property == B) {
                window_fill_blue = value;
            }
            if(property == OPACITY_PROP) {
                window_opacity = value;
            }
            return;
        }
        AminoNode* target = rects[node];
        if(property == X_PROP) target->x = value;
        if(property == Y_PROP) target->y = value;
        if(property == SCALEX) target->scalex = value;
        if(property == SCALEY) target->scaley = value;
        if(property == ROTATEX) target->rotatex = value;
        if(property == ROTATEY) target->rotatey = value;
        if(property == ROTATEZ) target->rotatez = value;
        if(property == VISIBLE) target->visible = value;
        if(property == OPACITY_PROP) target->opacity = value;

        if(target->type == RECT) {
            Rect* rect = (Rect*)target;
            if(property == R) rect->r = value;
            if(property == G) rect->g = value;
            if(property == B) rect->b = value;
            if(property == W_PROP) rect->w = value;
            if(property == H_PROP) rect->h = value;
            if(property == TEXID) rect->texid = value;
            if(property == TEXTURELEFT_PROP)   rect->left = value;
            if(property == TEXTURERIGHT_PROP)  rect->right = value;
            if(property == TEXTURETOP_PROP)    rect->top = value;
            if(property == TEXTUREBOTTOM_PROP) rect->bottom = value;
        }

        if(target->type == GROUP) {
            Group* group = (Group*)target;
            if(property == W_PROP) group->w = value;
            if(property == H_PROP) group->h = value;
            if(property == CLIPRECT_PROP) group->cliprect = value;
        }

        if(target->type == TEXT) {
            TextNode* textnode = (TextNode*)target;
            if(property == R) textnode->r = value;
            if(property == G) textnode->g = value;
            if(property == B) textnode->b = value;
            if(property == TEXT_PROP) textnode->text = text;
            if(property == FONTSIZE_PROP) textnode->fontsize = value;
            if(property == FONTID_PROP) textnode->fontid = value;
            textnode->refreshText();
        }

        if(target->type == POLY) {
            PolyNode* polynode = (PolyNode*)target;
            if(property == R) polynode->r = value;
            if(property == G) polynode->g = value;
            if(property == B) polynode->b = value;
            if(property == GEOMETRY) {
                polynode->geometry = arr;
            }
            if(property == DIMENSION) {
                polynode->dimension = value;
            }
            if(property == FILLED) {
                polynode->filled = value;
            }
        }
    }
};

extern std::vector<Update*> updates;

NAN_METHOD(createRect);
NAN_METHOD(createPoly);
NAN_METHOD(createText);
NAN_METHOD(createGroup);
NAN_METHOD(createGLNode);
NAN_METHOD(createAnim);
NAN_METHOD(stopAnim);


static std::wstring GetWString(v8::Handle<v8::String> str) {
    std::wstring wstr = L"";
    uint16_t* buf = new uint16_t[str->Length()+1];
    str->Write(buf);
    for(int i=0; i<str->Length()+1; i++) {
        wstr.push_back(buf[i]);
    }
    return wstr;
}

static std::vector<float>* GetFloatArray(v8::Handle<v8::Array> obj) {
    Handle<Array>  oarray = Handle<Array>::Cast(obj);
    std::vector<float>* carray = new std::vector<float>();
    for(std::size_t i=0; i<oarray->Length(); i++) {
        carray->push_back((float)(oarray->Get(i)->ToNumber()->NumberValue()));
    }
    return carray;
}

NAN_METHOD(updateProperty);
NAN_METHOD(updateWindowProperty);
NAN_METHOD(updateAnimProperty);
NAN_METHOD(addNodeToGroup);
NAN_METHOD(removeNodeFromGroup);
NAN_METHOD(setRoot);
NAN_METHOD(loadBufferToTexture);
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


/*
static void sendValidate() {
    if(!eventCallbackSet) warnAbort("WARNING. Event callback not set");
    Local<Object> event_obj = Object::New();
    event_obj->Set(String::NewSymbol("type"), String::New("validate"));
    Handle<Value> event_argv[] = {event_obj};
    NODE_EVENT_CALLBACK->Call(Context::GetCurrent()->Global(), 1, event_argv);
}
*/





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
    double validatetime;
    double updatestime;
    double animationstime;
    double rendertime;
    double frametime;
    double framewithsynctime;
};

#endif
