
#include "base.h"
#include "SimpleRenderer.h"
#include "bcm_host.h"
#include <linux/input.h>
#include <dirent.h>
#include <stdio.h>

static float near = 150;
static float far = -300;
static float eye = 600;


typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;

// OpenGL|ES objects
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;

} PWindow;

static PWindow _state, *state=&_state;
const char* INPUT_DIR = "/dev/input";
static std::vector<int> fds;
static int mouse_x = 0;
static int mouse_y = 0;

static void init_ogl(PWindow *state) {
   int32_t success = 0;
   EGLBoolean result;
   EGLint num_config;

   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };

   static const EGLint context_attributes[] =
   {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

   EGLConfig config;

   // get an EGL display connection
   state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   assert(state->display!=EGL_NO_DISPLAY);

   // initialize the EGL display connection
   result = eglInitialize(state->display, NULL, NULL);
   assert(EGL_FALSE != result);

   // get an appropriate EGL frame buffer configuration
   result = eglChooseConfig(state->display, attribute_list, &config, 1, &num_config);
   assert(EGL_FALSE != result);

   // choose opengl 2
   result = eglBindAPI(EGL_OPENGL_ES_API);
   assert(EGL_FALSE != result);

   // create an EGL rendering context
   state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, context_attributes);
   assert(state->context!=EGL_NO_CONTEXT);

   // create an EGL window surface
   success = graphics_get_display_size(0 /* LCD */, &state->screen_width, &state->screen_height);
   assert( success >= 0 );
   printf("RPI: display size = %d x %d\n",state->screen_width, state->screen_height);

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = state->screen_width;
   dst_rect.height = state->screen_height;

   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = state->screen_width << 16;
   src_rect.height = state->screen_height << 16;

   dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
   dispman_update = vc_dispmanx_update_start( 0 );


   VC_DISPMANX_ALPHA_T         dispman_alpha;

   dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;
   dispman_alpha.opacity = 255;
   dispman_alpha.mask = 0;

   int LAYER = 0;
   dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
      LAYER/*layer*/, &dst_rect, 0/*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE,
      &dispman_alpha  /*alpha*/,
      0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);

   nativewindow.element = dispman_element;
   nativewindow.width = state->screen_width;
   nativewindow.height = state->screen_height;
   vc_dispmanx_update_submit_sync( dispman_update );

   state->surface = eglCreateWindowSurface( state->display, config, &nativewindow, NULL );
   assert(state->surface != EGL_NO_SURFACE);

   // connect the context to the surface
   result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
   assert(EGL_FALSE != result);

   printf("rpi.c: got to the real opengl context\n");
}


static void dump_event(struct input_event* event) {
    switch(event->type) {
        case EV_SYN: printf("EV_SYN  event separator\n"); break;
        case EV_KEY:
            printf("EV_KEY  keyboard or button \n");
            if(event ->code == KEY_A) printf("  A key\n");
            if(event ->code == KEY_B) printf("  B key\n");
            break;
        case EV_REL: printf("EV_REL  relative axis\n"); break;
        case EV_ABS: printf("EV_ABS  absolute axis\n"); break;
        case EV_MSC:
            printf("EV_MSC  misc\n");
            if(event->code == MSC_SERIAL) printf("  serial\n");
            if(event->code == MSC_PULSELED) printf("  pulse led\n");
            if(event->code == MSC_GESTURE) printf("  gesture\n");
            if(event->code == MSC_RAW) printf("  raw\n");
            if(event->code == MSC_SCAN) printf("  scan\n");
            if(event->code == MSC_MAX) printf("  max\n");
            break;
        case EV_LED: printf("EV_LED  led\n"); break;
        case EV_SND: printf("EV_SND  sound\n"); break;
        case EV_REP: printf("EV_REP  autorepeating\n"); break;
        case EV_FF:  printf("EV_FF   force feedback send\n"); break;
        case EV_PWR: printf("EV_PWR  power button\n"); break;
        case EV_FF_STATUS: printf("EV_FF_STATUS force feedback receive\n"); break;
        case EV_MAX: printf("EV_MAX  max value\n"); break;
    }
    printf("type = %d code = %d value = %d\n",event->type,event->code,event->value);
}


bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

#define test_bit(bit, array) (array[bit/8] & (1<<(bit%8)))


static void init_inputs() {
    if((getuid()) != 0) {
        printf("you are not root. this might not work\n");
    }
    DIR    *dir;
    struct dirent *file;
    dir = opendir(INPUT_DIR);
    if(dir) {
        while((file = readdir(dir)) != NULL) {
            if(!startsWith("event",file->d_name)) continue;
            printf("file = %s\n",file->d_name);
            printf("initing a device\n");
            char str[256];
            strcpy(str, INPUT_DIR);
            strcat(str, "/");
            strcat(str, file->d_name);
            int fd = -1;
            if((fd = open(str, O_RDONLY | O_NONBLOCK)) == -1) {
                printf("this is not a valid device %s\n", str);
                continue;
            }
            char name[256] = "Unknown";
            ioctl(fd, EVIOCGNAME(sizeof (name)), name);
            printf("Reading from: %s (%s)\n", str,name);
            ioctl(fd, EVIOCGPHYS(sizeof (name)), name);
            printf("Location %s (%s)\n", str,name);

            struct input_id device_info;
            ioctl(fd, EVIOCGID, &device_info);

            u_int8_t evtype_b[(EV_MAX+7)/8];
            memset(evtype_b, 0, sizeof(evtype_b));
            if(ioctl(fd, EVIOCGBIT(0, EV_MAX), evtype_b) < 0) {
                printf("error reading device info\n");
                continue;
            }
            for (int i = 0; i<EV_MAX; i++) {
                if(test_bit(i, evtype_b)) {
                    printf("event type 0x%02x ",i);
                    switch(i) {
                    case EV_SYN: printf("sync events\n"); break;
                    case EV_KEY: printf("key events\n"); break;
                    case EV_REL: printf("rel events\n"); break;
                    case EV_ABS: printf("abs events\n"); break;
                    case EV_MSC: printf("msc events\n"); break;
                    case EV_SW:  printf("sw events\n"); break;
                    case EV_LED: printf("led events\n"); break;
                    case EV_SND: printf("snd events\n"); break;
                    case EV_REP: printf("rep events\n"); break;
                    case EV_FF:  printf("ff events\n"); break;
                    }
                }
            }

            fds.push_back(fd);
        }
        closedir(dir);
    }
}
/*
static void GLFW_MOUSE_POS_CALLBACK_FUNCTION(int x, int y) {
    if(!eventCallbackSet) warnAbort("WARNING. Event callback not set");
    Local<Object> event_obj = Object::New();
    event_obj->Set(String::NewSymbol("type"), String::New("mouseposition"));
    event_obj->Set(String::NewSymbol("x"), Number::New(x));
    event_obj->Set(String::NewSymbol("y"), Number::New(y));
    Handle<Value> event_argv[] = {event_obj};
    NODE_EVENT_CALLBACK->Call(Context::GetCurrent()->Global(), 1, event_argv);
}

static void GLFW_KEY_CALLBACK_FUNCTION(int key, int action) {
    if(!eventCallbackSet) warnAbort("WARNING. Event callback not set");
    Local<Object> event_obj = Object::New();
    if(action == 0) {
        event_obj->Set(String::NewSymbol("type"), String::New("keyrelease"));
    }
    if(action == 1) {
        event_obj->Set(String::NewSymbol("type"), String::New("keypress"));
    }
    if(action == 2) {
        event_obj->Set(String::NewSymbol("type"), String::New("keyrepeat"));
    }
    event_obj->Set(String::NewSymbol("keycode"), Number::New(key));
    Handle<Value> event_argv[] = {event_obj};
    NODE_EVENT_CALLBACK->Call(Context::GetCurrent()->Global(), 1, event_argv);
}


static void GLFW_MOUSE_BUTTON_CALLBACK_FUNCTION(int button, int state) {
    if(!eventCallbackSet) warnAbort("ERROR. Event callback not set");
    Local<Object> event_obj = Object::New();
    event_obj->Set(String::NewSymbol("type"), String::New("mousebutton"));
    event_obj->Set(String::NewSymbol("button"), Number::New(button));
    event_obj->Set(String::NewSymbol("state"), Number::New(state));
    Handle<Value> event_argv[] = {event_obj};
    NODE_EVENT_CALLBACK->Call(Context::GetCurrent()->Global(), 1, event_argv);
}

static void GLFW_MOUSE_WHEEL_CALLBACK_FUNCTION(int wheel) {
    if(!eventCallbackSet) warnAbort("ERROR. Event callback not set");
    Local<Object> event_obj = Object::New();
    event_obj->Set(String::NewSymbol("type"), String::New("mousewheelv"));
    event_obj->Set(String::NewSymbol("position"), Number::New(wheel));
    Handle<Value> event_argv[] = {event_obj};
    NODE_EVENT_CALLBACK->Call(Context::GetCurrent()->Global(), 1, event_argv);
}
*/
static void handleEvent(input_event ev) {
    // relative event. probably mouse
    if(ev.type == EV_REL) {
        if(ev.code == 0) {
            // x axis
            mouse_x += ev.value;
        }
        if(ev.code == 1) {
            mouse_y += ev.value;
        }
        if(mouse_x < 0) mouse_x = 0;
        if(mouse_y < 0) mouse_y = 0;
        if(mouse_x > width)  { mouse_x = width;  }
        if(mouse_y > height) { mouse_y = height; }
//        GLFW_MOUSE_POS_CALLBACK_FUNCTION(mouse_x, mouse_y);
        return;
    }

    //mouse wheel
    if(ev.type == EV_REL && ev.code == 8) {
  //      GLFW_MOUSE_WHEEL_CALLBACK_FUNCTION(ev.value);
        return;
    }
    if(ev.type == EV_KEY) {
        printf("key or button pressed code = %d, state = %d\n",ev.code, ev.value);
        if(ev.code == BTN_LEFT) {
    //        GLFW_MOUSE_BUTTON_CALLBACK_FUNCTION(ev.code,ev.value);
            return;
        }
      //  GLFW_KEY_CALLBACK_FUNCTION(ev.code, ev.value);
        return;
    }
}

static void processInputs() {
    int size = sizeof(struct input_event);
    struct input_event ev[64];
    for(int i=0; i<fds.size(); i++) {
        int fd = fds[i];
        int rd = read(fd, ev, size*64);
        if(rd == -1) continue;
        if(rd < size) {
            printf("read too little!!!  %d\n",rd);
        }
        for(int i=0; i<(int)(rd/size); i++) {
            //dump_event(&(ev[i]));
            handleEvent(ev[i]);
        }
    }
}

NAN_METHOD(init) {
	matrixStack = std::stack<void *>();
    bcm_host_init();
    // Clear application state
    memset( state, 0, sizeof( *state ) );

    // Start OGLES
    init_ogl(state);

    //get screen size
    width = state->screen_width;
    height = state->screen_height;

    init_inputs();
}

NAN_METHOD(createWindow) {
    int w  = info[0]->Uint32Value();
    int h  = info[1]->Uint32Value();

    //Window already allocated at this point.
    //Values ignored.

    colorShader = new ColorShader();
    textureShader = new TextureShader();

    modelView = new GLfloat[16];
    globaltx = new GLfloat[16];
    make_identity_matrix(globaltx);

    window_fill_red = 0;
    window_fill_green = 0;
    window_fill_blue = 0;
    window_opacity = 1;
}

NAN_METHOD(setWindowSize) {
    //not supported
    printf("pretending to set the window size to: %d %d\n", width, height);
}

NAN_METHOD(getWindowSize) {
    v8::Local<v8::Object> obj = Nan::New<v8::Object>();

    //window size (is screen size)
    Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(width));
    Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(height));

    info.GetReturnValue().Set(obj);
}

void render() {
    DebugEvent de;
    double starttime = getTime();

    //process input
    processInputs();
    double postinput = getTime();
    de.inputtime = postinput-starttime;

    int updatecount = updates.size();
    //apply the processed updates
    for(int j=0; j<updates.size(); j++) {
        updates[j]->apply();
    }
    updates.clear();
    double postupdates = getTime();
    de.updatestime = postupdates-postvalidate;

    //apply the animations
    double currentTime = getTime();

    for(int j=0; j<anims.size(); j++) {
        anims[j]->update(currentTime);
    }
    double postanim = getTime();
    de.animationstime = postanim-postupdates;

    //set up the viewport
    GLfloat* scaleM = new GLfloat[16];
    make_scale_matrix(1,-1,1,scaleM);
    GLfloat* transM = new GLfloat[16];
    make_trans_matrix(-width/2,height/2,0,transM);
    GLfloat* m4 = new GLfloat[16];
    mul_matrix(m4, transM, scaleM);
    GLfloat* pixelM = new GLfloat[16];
    loadPixelPerfectMatrix(pixelM, width, height, eye, near, far);
    mul_matrix(modelView,pixelM,m4);
    make_identity_matrix(globaltx);
    glViewport(0,0,width, height);
    glClearColor(window_fill_red,window_fill_green,window_fill_blue,window_opacity);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    AminoNode *root = rects[rootHandle];
    SimpleRenderer *rend = new SimpleRenderer();
    double prerender = getTime();

    rend->startRender(root);
    delete rend;

    double postrender = getTime();
    de.rendertime = postrender-prerender;
    de.frametime = postrender-starttime;
    eglSwapBuffers(state->display, state->surface);
    double postswap = getTime();
    de.framewithsynctime = postswap-starttime;
    //    printf("input = %.2f update = %.2f update count %d ",  de.inputtime, de.updatestime, updatecount);
    //    printf("animtime = %.2f render = %.2f frame = %.2f, full frame = %.2f\n", de.animationstime, de.rendertime, de.frametime, de.framewithsynctime);
}

NAN_METHOD(tick) {
    render();
}

/*
Handle<Value> selfDrive(const Arguments& args) {
    HandleScope scope;
    for(int i =0; i<100; i++) {
        render();
    }
    return scope.Close(Undefined());
}


Handle<Value> runTest(const Arguments& args) {
    HandleScope scope;

    double startTime = getTime();
    int count = 100;
    Local<v8::Object> opts = args[0]->ToObject();
    count = (int)(opts
        ->Get(String::NewSymbol("count"))
        ->ToNumber()
        ->NumberValue()
        );


    bool sync = false;
    sync = opts
        ->Get(String::NewSymbol("sync"))
        ->ToBoolean()
        ->BooleanValue();

    printf("rendering %d times, vsync = %d\n",count,sync);

    printf("applying updates first\n");
    for(int j=0; j<updates.size(); j++) {
        updates[j]->apply();
    }
    updates.clear();

    printf("setting up the screen\n");
    GLfloat* scaleM = new GLfloat[16];
    make_scale_matrix(1,-1,1,scaleM);
    //make_scale_matrix(1,1,1,scaleM);
    GLfloat* transM = new GLfloat[16];
    make_trans_matrix(-width/2,height/2,0,transM);
    //make_trans_matrix(10,10,0,transM);
    //make_trans_matrix(0,0,0,transM);

    GLfloat* m4 = new GLfloat[16];
    mul_matrix(m4, transM, scaleM);


    GLfloat* pixelM = new GLfloat[16];
    //    loadPixelPerfectMatrix(pixelM, width, height, 600, 100, -150);
    loadPixelPerfect(pixelM, width, height, eye, near, far);
    //printf("eye = %f\n",eye);
    //loadPerspectiveMatrix(pixelM, 45, 1, 10, -100);

    GLfloat* m5 = new GLfloat[16];
    //transpose(m5,pixelM);

    mul_matrix(modelView,pixelM,m4);


    make_identity_matrix(globaltx);
    glViewport(0,0,width, height);
    glClearColor(1,1,1,1);


    glDisable(GL_DEPTH_TEST);
    printf("running %d times\n",count);
    for(int i=0; i<count; i++) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for(int j=0; j<anims.size(); j++) {
            anims[j]->update();
        }
        AminoNode* root = rects[rootHandle];
        SimpleRenderer* rend = new SimpleRenderer();
        rend->startRender(root);
        delete rend;
        if(sync) {
            eglSwapBuffers(state->display, state->surface);
        }
    }

    double endTime = getTime();
    Local<Object> ret = Object::New();
    ret->Set(String::NewSymbol("count"),Number::New(count));
    ret->Set(String::NewSymbol("totalTime"),Number::New(endTime-startTime));
    return scope.Close(ret);
}
*/

NAN_METHOD(setEventCallback) {
    eventCallbackSet = true;
    NODE_EVENT_CALLBACK = new Nan::Callback(info[0].As<v8::Function>());
}



NAN_MODULE_INIT(InitAll) {

    Nan::Set(target, Nan::New("init").ToLocalChecked(),             Nan::GetFunction(Nan::New<FunctionTemplate>(init)).ToLocalChecked());
    Nan::Set(target, Nan::New("createWindow").ToLocalChecked(),     Nan::GetFunction(Nan::New<FunctionTemplate>(createWindow)).ToLocalChecked());
    Nan::Set(target, Nan::New("setWindowSize").ToLocalChecked(),    Nan::GetFunction(Nan::New<FunctionTemplate>(setWindowSize)).ToLocalChecked());
    Nan::Set(target, Nan::New("getWindowSize").ToLocalChecked(),    Nan::GetFunction(Nan::New<FunctionTemplate>(getWindowSize)).ToLocalChecked());
	Nan::Set(target, Nan::New("createRect").ToLocalChecked(),       Nan::GetFunction(Nan::New<FunctionTemplate>(createRect)).ToLocalChecked());
    Nan::Set(target, Nan::New("createPoly").ToLocalChecked(),       Nan::GetFunction(Nan::New<FunctionTemplate>(createPoly)).ToLocalChecked());
    Nan::Set(target, Nan::New("createGroup").ToLocalChecked(),      Nan::GetFunction(Nan::New<FunctionTemplate>(createGroup)).ToLocalChecked());
    Nan::Set(target, Nan::New("createText").ToLocalChecked(),       Nan::GetFunction(Nan::New<FunctionTemplate>(createText)).ToLocalChecked());
    Nan::Set(target, Nan::New("createGLNode").ToLocalChecked(),     Nan::GetFunction(Nan::New<FunctionTemplate>(createGLNode)).ToLocalChecked());
    Nan::Set(target, Nan::New("createAnim").ToLocalChecked(),       Nan::GetFunction(Nan::New<FunctionTemplate>(createAnim)).ToLocalChecked());
    Nan::Set(target, Nan::New("stopAnim").ToLocalChecked(),         Nan::GetFunction(Nan::New<FunctionTemplate>(stopAnim)).ToLocalChecked());
    Nan::Set(target, Nan::New("updateProperty").ToLocalChecked(),     Nan::GetFunction(Nan::New<FunctionTemplate>(updateProperty)).ToLocalChecked());
    Nan::Set(target, Nan::New("updateWindowProperty").ToLocalChecked(),     Nan::GetFunction(Nan::New<FunctionTemplate>(updateWindowProperty)).ToLocalChecked());
    Nan::Set(target, Nan::New("updateAnimProperty").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(updateAnimProperty)).ToLocalChecked());
    Nan::Set(target, Nan::New("addNodeToGroup").ToLocalChecked(),   Nan::GetFunction(Nan::New<FunctionTemplate>(addNodeToGroup)).ToLocalChecked());
    Nan::Set(target, Nan::New("removeNodeFromGroup").ToLocalChecked(),   Nan::GetFunction(Nan::New<FunctionTemplate>(removeNodeFromGroup)).ToLocalChecked());
    Nan::Set(target, Nan::New("tick").ToLocalChecked(),             Nan::GetFunction(Nan::New<FunctionTemplate>(tick)).ToLocalChecked());
//    Nan::Set(target, Nan::New("selfDrive").ToLocalChecked(),        Nan::GetFunction(Nan::New<FunctionTemplate>(selfDrive)).ToLocalChecked());
	Nan::Set(target, Nan::New("setEventCallback").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(setEventCallback)).ToLocalChecked());
    Nan::Set(target, Nan::New("setRoot").ToLocalChecked(),          Nan::GetFunction(Nan::New<FunctionTemplate>(setRoot)).ToLocalChecked());
    Nan::Set(target, Nan::New("decodePngBuffer").ToLocalChecked(),   Nan::GetFunction(Nan::New<FunctionTemplate>(decodePngBuffer)).ToLocalChecked());
    Nan::Set(target, Nan::New("decodeJpegBuffer").ToLocalChecked(),  Nan::GetFunction(Nan::New<FunctionTemplate>(decodeJpegBuffer)).ToLocalChecked());
    Nan::Set(target, Nan::New("loadBufferToTexture").ToLocalChecked(),  Nan::GetFunction(Nan::New<FunctionTemplate>(loadBufferToTexture)).ToLocalChecked());
    Nan::Set(target, Nan::New("createNativeFont").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(createNativeFont)).ToLocalChecked());
    Nan::Set(target, Nan::New("getCharWidth").ToLocalChecked(),     Nan::GetFunction(Nan::New<FunctionTemplate>(getCharWidth)).ToLocalChecked());
    Nan::Set(target, Nan::New("getFontHeight").ToLocalChecked(),    Nan::GetFunction(Nan::New<FunctionTemplate>(getFontHeight)).ToLocalChecked());
    Nan::Set(target, Nan::New("getFontAscender").ToLocalChecked(),    Nan::GetFunction(Nan::New<FunctionTemplate>(getFontAscender)).ToLocalChecked());
    Nan::Set(target, Nan::New("getFontDescender").ToLocalChecked(),    Nan::GetFunction(Nan::New<FunctionTemplate>(getFontDescender)).ToLocalChecked());
//	Nan::Set(target, Nan::New("runTest").ToLocalChecked(),          Nan::GetFunction(Nan::New<FunctionTemplate>(runTest)).ToLocalChecked());
	Nan::Set(target, Nan::New("initColorShader").ToLocalChecked(),  Nan::GetFunction(Nan::New<FunctionTemplate>(initColorShader)).ToLocalChecked());
	Nan::Set(target, Nan::New("initTextureShader").ToLocalChecked(),Nan::GetFunction(Nan::New<FunctionTemplate>(initTextureShader)).ToLocalChecked());

	Nan::Set(target, Nan::New("GL_VERTEX_SHADER").ToLocalChecked(),    Nan::New(GL_VERTEX_SHADER));
	Nan::Set(target, Nan::New("GL_FRAGMENT_SHADER").ToLocalChecked(),  Nan::New(GL_FRAGMENT_SHADER));
	Nan::Set(target, Nan::New("GL_COMPILE_STATUS").ToLocalChecked(),   Nan::New(GL_COMPILE_STATUS));
	Nan::Set(target, Nan::New("GL_LINK_STATUS").ToLocalChecked(),      Nan::New(GL_LINK_STATUS));

	Nan::Set(target, Nan::New("glCreateShader").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glCreateShader)).ToLocalChecked());
	Nan::Set(target, Nan::New("glShaderSource").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glShaderSource)).ToLocalChecked());
	Nan::Set(target, Nan::New("glCompileShader").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glCompileShader)).ToLocalChecked());
	Nan::Set(target, Nan::New("glGetShaderiv").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetShaderiv)).ToLocalChecked());
	Nan::Set(target, Nan::New("glCreateProgram").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glCreateProgram)).ToLocalChecked());
	Nan::Set(target, Nan::New("glAttachShader").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glAttachShader)).ToLocalChecked());
	Nan::Set(target, Nan::New("glUseProgram").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glUseProgram)).ToLocalChecked());
	Nan::Set(target, Nan::New("glLinkProgram").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glLinkProgram)).ToLocalChecked());
	Nan::Set(target, Nan::New("glGetProgramiv").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetProgramiv)).ToLocalChecked());
	Nan::Set(target, Nan::New("glGetAttribLocation").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetAttribLocation)).ToLocalChecked());
	Nan::Set(target, Nan::New("glGetUniformLocation").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetUniformLocation)).ToLocalChecked());
	Nan::Set(target, Nan::New("glGetProgramInfoLog").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(node_glGetProgramInfoLog)).ToLocalChecked());
/*
    exports->Set(String::NewSymbol("init"),             FunctionTemplate::New(init)->GetFunction());
    exports->Set(String::NewSymbol("createWindow"),     FunctionTemplate::New(createWindow)->GetFunction());
    exports->Set(String::NewSymbol("setWindowSize"),    FunctionTemplate::New(setWindowSize)->GetFunction());
    exports->Set(String::NewSymbol("getWindowSize"),    FunctionTemplate::New(getWindowSize)->GetFunction());
    exports->Set(String::NewSymbol("createRect"),       FunctionTemplate::New(createRect)->GetFunction());
    exports->Set(String::NewSymbol("createPoly"),       FunctionTemplate::New(createPoly)->GetFunction());
    exports->Set(String::NewSymbol("createGroup"),      FunctionTemplate::New(createGroup)->GetFunction());
    exports->Set(String::NewSymbol("createText"),       FunctionTemplate::New(createText)->GetFunction());
    exports->Set(String::NewSymbol("createGLNode"),     FunctionTemplate::New(createGLNode)->GetFunction());
    exports->Set(String::NewSymbol("createAnim"),       FunctionTemplate::New(createAnim)->GetFunction());
    exports->Set(String::NewSymbol("stopAnim"),         FunctionTemplate::New(stopAnim)->GetFunction());
    exports->Set(String::NewSymbol("updateProperty"),   FunctionTemplate::New(updateProperty)->GetFunction());
    exports->Set(String::NewSymbol("updateAnimProperty"),FunctionTemplate::New(updateAnimProperty)->GetFunction());
    exports->Set(String::NewSymbol("updateWindowProperty"),  FunctionTemplate::New(updateWindowProperty)->GetFunction());
    exports->Set(String::NewSymbol("addNodeToGroup"),   FunctionTemplate::New(addNodeToGroup)->GetFunction());
    exports->Set(String::NewSymbol("removeNodeFromGroup"),   FunctionTemplate::New(removeNodeFromGroup)->GetFunction());
    exports->Set(String::NewSymbol("tick"),             FunctionTemplate::New(tick)->GetFunction());
    exports->Set(String::NewSymbol("selfDrive"),        FunctionTemplate::New(selfDrive)->GetFunction());
    exports->Set(String::NewSymbol("setEventCallback"), FunctionTemplate::New(setEventCallback)->GetFunction());
    exports->Set(String::NewSymbol("setRoot"),          FunctionTemplate::New(setRoot)->GetFunction());
    exports->Set(String::NewSymbol("decodePngBuffer"),  FunctionTemplate::New(decodePngBuffer)->GetFunction());
    exports->Set(String::NewSymbol("decodeJpegBuffer"),  FunctionTemplate::New(decodeJpegBuffer)->GetFunction());
    exports->Set(String::NewSymbol("loadBufferToTexture"),  FunctionTemplate::New(loadBufferToTexture)->GetFunction());
    exports->Set(String::NewSymbol("createNativeFont"), FunctionTemplate::New(createNativeFont)->GetFunction());
    exports->Set(String::NewSymbol("getCharWidth"),     FunctionTemplate::New(getCharWidth)->GetFunction());
    exports->Set(String::NewSymbol("getFontHeight"),    FunctionTemplate::New(getFontHeight)->GetFunction());
    exports->Set(String::NewSymbol("runTest"),          FunctionTemplate::New(runTest)->GetFunction());
    exports->Set(String::NewSymbol("initColorShader"),  FunctionTemplate::New(initColorShader)->GetFunction());
    exports->Set(String::NewSymbol("initTextureShader"),FunctionTemplate::New(initTextureShader)->GetFunction());

	exports->Set(String::NewSymbol("GL_VERTEX_SHADER"), Number::New(GL_VERTEX_SHADER));
	exports->Set(String::NewSymbol("GL_FRAGMENT_SHADER"), Number::New(GL_FRAGMENT_SHADER));
	exports->Set(String::NewSymbol("GL_COMPILE_STATUS"), Number::New(GL_COMPILE_STATUS));
	exports->Set(String::NewSymbol("GL_LINK_STATUS"), Number::New(GL_LINK_STATUS));

	exports->Set(String::NewSymbol("glCreateShader"), FunctionTemplate::New(node_glCreateShader)->GetFunction());
	exports->Set(String::NewSymbol("glShaderSource"), FunctionTemplate::New(node_glShaderSource)->GetFunction());
	exports->Set(String::NewSymbol("glCompileShader"), FunctionTemplate::New(node_glCompileShader)->GetFunction());
	exports->Set(String::NewSymbol("glGetShaderiv"), FunctionTemplate::New(node_glGetShaderiv)->GetFunction());
	exports->Set(String::NewSymbol("glCreateProgram"), FunctionTemplate::New(node_glCreateProgram)->GetFunction());
	exports->Set(String::NewSymbol("glAttachShader"), FunctionTemplate::New(node_glAttachShader)->GetFunction());
	exports->Set(String::NewSymbol("glUseProgram"), FunctionTemplate::New(node_glUseProgram)->GetFunction());
	exports->Set(String::NewSymbol("glLinkProgram"), FunctionTemplate::New(node_glLinkProgram)->GetFunction());
	exports->Set(String::NewSymbol("glGetProgramiv"), FunctionTemplate::New(node_glGetProgramiv)->GetFunction());
	exports->Set(String::NewSymbol("glGetAttribLocation"), FunctionTemplate::New(node_glGetAttribLocation)->GetFunction());
	exports->Set(String::NewSymbol("glGetUniformLocation"), FunctionTemplate::New(node_glGetUniformLocation)->GetFunction());
	exports->Set(String::NewSymbol("glGetProgramInfoLog"), FunctionTemplate::New(node_glGetProgramInfoLog)->GetFunction());
*/


}

NODE_MODULE(aminonative, InitAll)
