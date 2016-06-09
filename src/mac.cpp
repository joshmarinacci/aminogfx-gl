

#include "base.h"
#include "SimpleRenderer.h"

// ========== Event Callbacks ===========

static bool windowSizeChanged = true;
static void GLFW_WINDOW_SIZE_CALLBACK_FUNCTION(GLFWwindow *window, int newWidth, int newHeight) {
	width = newWidth;
	height = newHeight;
	windowSizeChanged = true;

    if (!eventCallbackSet) {
        warnAbort("WARNING. Event callback not set");
    }

    //create object
    v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

    Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),     Nan::New("windowsize").ToLocalChecked());
    Nan::Set(event_obj, Nan::New("width").ToLocalChecked(),    Nan::New(width));
    Nan::Set(event_obj, Nan::New("height").ToLocalChecked(),   Nan::New(height));

    Local<Value> argv[] = { Nan::Null(), event_obj };

    NODE_EVENT_CALLBACK->Call(2, argv);
}

static void GLFW_WINDOW_CLOSE_CALLBACK_FUNCTION(GLFWwindow * window) {
    if (!eventCallbackSet) {
        warnAbort("WARNING. Event callback not set");
    }

    //create object
    v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

    Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),     Nan::New("windowclose").ToLocalChecked());

    Local<Value> argv[] = { Nan::Null(), event_obj };

    NODE_EVENT_CALLBACK->Call(2, argv);
}

static float near = 150;
static float far = -300;
static float eye = 600;

static void GLFW_KEY_CALLBACK_FUNCTION(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (!eventCallbackSet) {
        warnAbort("WARNING. Event callback not set");
    }

    //create object
    v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

    if (action == 0) {
        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),     Nan::New("keyrelease").ToLocalChecked());
    } else if (action == 1) {
        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),     Nan::New("keypress").ToLocalChecked());
    }

    Nan::Set(event_obj, Nan::New("keycode").ToLocalChecked(),     Nan::New(key));

    Local<Value> argv[] = { Nan::Null(), event_obj };

    NODE_EVENT_CALLBACK->Call(2, argv);
}

static void GLFW_MOUSE_POS_CALLBACK_FUNCTION(GLFWwindow *window, double x, double y) {
    if (!eventCallbackSet) {
        warnAbort("WARNING. Event callback not set");
    }

    //create object
    v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

    Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),  Nan::New("mouseposition").ToLocalChecked());
    Nan::Set(event_obj, Nan::New("x").ToLocalChecked(),     Nan::New(x));
    Nan::Set(event_obj, Nan::New("y").ToLocalChecked(),     Nan::New(y));

    Local<Value> argv[] = { Nan::Null(), event_obj };

    NODE_EVENT_CALLBACK->Call(2, argv);
}

static void GLFW_MOUSE_BUTTON_CALLBACK_FUNCTION(GLFWwindow *window, int button, int action, int mods) {
    if (!eventCallbackSet) {
        warnAbort("ERROR. Event callback not set");
    }

    v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

    Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("mousebutton").ToLocalChecked());
    Nan::Set(event_obj, Nan::New("button").ToLocalChecked(), Nan::New(button));
    Nan::Set(event_obj, Nan::New("state").ToLocalChecked(),  Nan::New(action));

    Local<Value> argv[] = { Nan::Null(), event_obj };

    NODE_EVENT_CALLBACK->Call(2, argv);
}

static void GLFW_MOUSE_WHEEL_CALLBACK_FUNCTION(GLFWwindow *window, double xoff, double yoff) {
    if (!eventCallbackSet) {
        warnAbort("ERROR. Event callback not set");
    }

    v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

    Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("mousewheelv").ToLocalChecked());
    Nan::Set(event_obj, Nan::New("xoff").ToLocalChecked(),   Nan::New(xoff));
    Nan::Set(event_obj, Nan::New("yoff").ToLocalChecked(),   Nan::New(yoff));

    Local<Value> argv[] = { Nan::Null(), event_obj };

    NODE_EVENT_CALLBACK->Call(2, argv);
}

NAN_METHOD(init) {
	matrixStack = std::stack<void *>();

    if (!glfwInit()) {
        printf("error. quitting\n");

        glfwTerminate();
        exit(EXIT_FAILURE);
    }
}

GLFWwindow *window;

NAN_METHOD(createWindow) {
    int w  = info[0]->Uint32Value();
    int h  = info[1]->Uint32Value();

    width = w;
    height = h;

//    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 2);
//    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 1);

    //glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    window = glfwCreateWindow(width, height, "simple window", NULL, NULL);

    if (!window) {
        printf("couldn't open a window. quitting\n");

        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, GLFW_KEY_CALLBACK_FUNCTION);
    glfwSetCursorPosCallback(window, GLFW_MOUSE_POS_CALLBACK_FUNCTION);
    glfwSetMouseButtonCallback(window, GLFW_MOUSE_BUTTON_CALLBACK_FUNCTION);
    glfwSetScrollCallback(window, GLFW_MOUSE_WHEEL_CALLBACK_FUNCTION);
    glfwSetWindowSizeCallback(window, GLFW_WINDOW_SIZE_CALLBACK_FUNCTION);
    glfwSetWindowCloseCallback(window, GLFW_WINDOW_CLOSE_CALLBACK_FUNCTION);

	colorShader = new ColorShader();
	textureShader = new TextureShader();
    modelView = new GLfloat[16];

    globaltx = new GLfloat[16];
    window_fill_red = 0;
    window_fill_green = 0;
    window_fill_blue = 0;
    window_opacity = 1;
    make_identity_matrix(globaltx);

    glViewport(0, 0, width, height);
}

NAN_METHOD(setWindowSize) {
    int w  = info[0]->Uint32Value();
    int h  = info[1]->Uint32Value();

    width = w;
    height = h;

    glfwSetWindowSize(window, width, height);
}

NAN_METHOD(getWindowSize) {
    //create object
    v8::Local<v8::Object> obj = Nan::New<v8::Object>();

    //TODO add screen size & window size
    Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(width));
    Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(height));

    info.GetReturnValue().Set(obj);
}

static int FPS_LEN = 100;
static double frametimes[100];
static double avg_frametime = 0;
static int currentFrame = 0;

void render() {
    DebugEvent de;
    double starttime = getTime();
    //input updates happen at any time
    double postinput = getTime();

    de.inputtime = postinput - starttime;

    //send the validate event

//    sendValidate();
    double postvalidate = getTime();

    de.validatetime = postvalidate - postinput;

    //std::size_t updatecount = updates.size();
    //apply processed updates
    for (std::size_t j = 0; j < updates.size(); j++) {
        updates[j]->apply();
    }

    updates.clear();

    double postupdates = getTime();

    de.updatestime = postupdates - postvalidate;

    //apply animations
    for (std::size_t j = 0; j < anims.size(); j++) {
        anims[j]->update();
    }

    double postanim = getTime();

    de.animationstime = postanim-postupdates;

    //set up the viewport
    GLfloat* scaleM = new GLfloat[16];

    make_scale_matrix(1, -1, 1, scaleM);

    GLfloat* transM = new GLfloat[16];

    make_trans_matrix(-((float)width) / 2,((float)height) / 2, 0, transM);

    GLfloat* m4 = new GLfloat[16];

    mul_matrix(m4, transM, scaleM);

    GLfloat* pixelM = new GLfloat[16];

    loadPixelPerfect(pixelM, width, height, eye, near, far);
    mul_matrix(modelView, pixelM, m4);
    make_identity_matrix(globaltx);
    glViewport(0, 0, width, height);
    glClearColor(window_fill_red, window_fill_green, window_fill_blue, window_opacity);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    //draw
    AminoNode *root = rects[rootHandle];
    SimpleRenderer* rend = new SimpleRenderer();
    double prerender = getTime();

    rend->modelViewChanged = windowSizeChanged;
    windowSizeChanged = false;
    rend->startRender(root);
    delete rend;

    double postrender = getTime();

    de.rendertime = postrender - prerender;
    de.frametime = postrender - starttime;

    //swap
    glfwSwapBuffers(window);

    double postswap = getTime();

    de.framewithsynctime = postswap-starttime;
    frametimes[currentFrame] = de.framewithsynctime;

    if (currentFrame == FPS_LEN - 1) {
        double total = 0;

        for (int i = 0; i < FPS_LEN; i++) {
            total += frametimes[i];
        }

        //printf("avg frame len = %f \n",(total/FPS_LEN));
        avg_frametime = total/FPS_LEN;
    }

    currentFrame = (currentFrame + 1) % FPS_LEN;

    glfwPollEvents();
//    printf("input = %.2f validate = %.2f update = %.2f update count %d ",  de.inputtime, de.validatetime, de.updatestime, updatecount);
//    printf("animtime = %.2f render = %.2f frame = %.2f, full frame = %.2f\n", de.animationstime, de.rendertime, de.frametime, de.framewithsynctime);
}

NAN_METHOD(tick) {
    render();
}

//Handle<Value> selfDrive(const Arguments& args) {
//    HandleScope scope;
//    for(int i =0; i<100; i++) {
//        render();
//    }
//    return scope.Close(Undefined());
//}
//
//Handle<Value> runTest(const Arguments& args) {
//    HandleScope scope;
//
//    double startTime = getTime();
//    int count = 100;
//    Local<v8::Object> opts = args[0]->ToObject();
//    count = (int)(opts
//        ->Get(String::NewSymbol("count"))
//        ->ToNumber()
//        ->NumberValue()
//        );
//
//
//    bool sync = false;
//    sync = opts
//        ->Get(String::NewSymbol("sync"))
//        ->ToBoolean()
//        ->BooleanValue();
//
//    printf("rendering %d times, vsync = %d\n",count,sync);
//
//    printf("applying updates first\n");
//    for(std::size_t j=0; j<updates.size(); j++) {
//        updates[j]->apply();
//    }
//    updates.clear();
//
//    printf("setting up the screen\n");
//    GLfloat* scaleM = new GLfloat[16];
//    make_scale_matrix(1,-1,1,scaleM);
//    GLfloat* transM = new GLfloat[16];
//    make_trans_matrix(-width/2,height/2,0,transM);
//    GLfloat* m4 = new GLfloat[16];
//    mul_matrix(m4, transM, scaleM);
//    GLfloat* pixelM = new GLfloat[16];
//    loadPixelPerfect(pixelM, width, height, eye, near, far);
//    mul_matrix(modelView,pixelM,m4);
//    make_identity_matrix(globaltx);
//    glViewport(0,0,width, height);
//    glClearColor(1,1,1,1);
//    glDisable(GL_DEPTH_TEST);
//    printf("running %d times\n",count);
//    for(int i=0; i<count; i++) {
//        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//        AminoNode* root = rects[rootHandle];
//        SimpleRenderer* rend = new SimpleRenderer();
//        rend->startRender(root);
//        delete rend;
//        if(sync) {
//            glfwSwapBuffers();
//        }
//    }
//
//    double endTime = getTime();
//    Local<Object> ret = Object::New();
//    ret->Set(String::NewSymbol("count").ToLocalChecked(),Number::New(count));
//    ret->Set(String::NewSymbol("totalTime").ToLocalChecked(),Number::New(endTime-startTime));
//    return scope.Close(ret);
//}


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

}

NODE_MODULE(aminonative, InitAll)
