#include "mac.h"

#define DEBUG_GLFW false
#define DEBUG_RENDER false

/**
 * Mac AminoGfx implementation.
 */
class AminoGfxMac : public AminoGfx {
public:
    AminoGfxMac(): AminoGfx(getFactory()->name) {
        //empty
    }

    /**
     * Get factory instance.
     */
    static AminoGfxMacFactory* getFactory() {
        static AminoGfxMacFactory *instance = NULL;

        if (!instance) {
            instance = new AminoGfxMacFactory(New);
        }

        return instance;
    }

    /**
     * Add class template to module exports.
     */
    static NAN_MODULE_INIT(Init) {
        AminoGfxMacFactory *factory = getFactory();

        AminoGfx::Init(target, factory);
    }

private:
    static bool glfwInitialized;
    static int instanceCount;
    static std::map<GLFWwindow *, AminoGfxMac *> *windowMap;

    //GLFW window
    GLFWwindow *window = NULL;

    static AminoGfxMac* windowToInstance(GLFWwindow *window) {
        std::map<GLFWwindow *, AminoGfxMac *>::iterator it = windowMap->find(window);

        if (it != windowMap->end()) {
            return it->second;
        }

        return NULL;
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    /**
     * Setup JS instance.
     */
    void setup() override {
        if (DEBUG_GLFW) {
            printf("AminoGfxMac.setup()\n");
        }

        //init GLFW
        if (!glfwInitialized) {
            if (!glfwInit()) {
                //exit on error
                printf("error. quitting\n");

                glfwTerminate();
                exit(EXIT_FAILURE);
            }

            glfwInitialized = true;
        }

        instanceCount++;

        //base class
        AminoGfx::setup();
    }

    /**
     * Destroy GLFW instance.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        //destroy basic instance
        AminoGfx::destroy();

        //GLFW
        if (window) {
            glfwDestroyWindow(window);
            windowMap->erase(window);
            window = NULL;
        }

        instanceCount--;

        if (DEBUG_GLFW) {
            printf("Destroyed GLFW instance. Left=%i\n", instanceCount);
        }

        if (instanceCount == 0) {
            glfwTerminate();
            glfwInitialized = false;
        }
    }

    /**
     * Get default monitor resolution.
     */
    bool getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) override {
        //debug
        //printf("getScreenInfo\n");

        //get monitor properties
        GLFWmonitor *primary = glfwGetPrimaryMonitor();
        const GLFWvidmode *vidmode = glfwGetVideoMode(primary);

        w = vidmode->width;
        h = vidmode->height;
        refreshRate = vidmode->refreshRate;
        fullscreen = false;

        return true;
    }

    /**
     * Add GLFW properties.
     */
    void populateRuntimeProperties(v8::Local<v8::Object> &obj) override {
        //debug
        //printf("populateRuntimeProperties\n");

        AminoGfx::populateRuntimeProperties(obj);

        //GLFW
        Nan::Set(obj, Nan::New("glfwVersion").ToLocalChecked(), Nan::New(std::string(glfwGetVersionString())).ToLocalChecked());

        //retina scale factor
        Nan::Set(obj, Nan::New("scale").ToLocalChecked(), Nan::New<v8::Integer>(viewportW / (int)propW->value));
    }

    /**
     * Initialize GLFW window.
     */
    void initRenderer() override {
        AminoGfx::initRenderer();

        /*
         * use OpenGL 2.x
         *
         * OpenGL 3.2 changes:
         *
         *   - https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/UpdatinganApplicationtoSupportOpenGL3/UpdatinganApplicationtoSupportOpenGL3.html#//apple_ref/doc/uid/TP40001987-CH3-SW1
         *
         * Thread safety: http://www.glfw.org/docs/latest/intro.html#thread_safety
         */

        //create window
        glfwWindowHint(GLFW_DEPTH_BITS, 32); //default

        window = glfwCreateWindow(propW->value, propH->value, propTitle->value.c_str(), NULL, NULL);

        if (!window) {
            //exit on error
            printf("couldn't open a window. quitting\n");

            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        //store
        windowMap->insert(std::pair<GLFWwindow *, AminoGfxMac *>(window, this));

        //check window size
        int windowW;
        int windowH;

        glfwGetWindowSize(window, &windowW, &windowH);

        if (DEBUG_GLFW) {
            printf("window size: requested=%ix%i, got=%ix%i\n", (int)propW->value, (int)propH->value, windowW, windowH);
        }

        updateSize(windowW, windowH);

        //check window pos
        if (propX->value != -1 && propY->value != -1) {
            //use initial position
            glfwSetWindowPos(window, propX->value, propY->value);
        }

        int windowX;
        int windowY;

        glfwGetWindowPos(window, &windowX, &windowY);

        updatePosition(windowX, windowY);

        //get framebuffer size
        glfwGetFramebufferSize(window, &viewportW, &viewportH);
        viewportChanged = true;

        //check framebuffer size
        if (DEBUG_GLFW) {
            printf("framebuffer size: %ix%i\n", viewportW, viewportH);
        }

        //activate context (needed by JS code to create shaders)
        glfwMakeContextCurrent(window);

        //set bindings
        glfwSetKeyCallback(window, handleKeyEvents);
        glfwSetCursorPosCallback(window, handleMouseMoveEvents);
        glfwSetMouseButtonCallback(window, handleMouseClickEvents);
        glfwSetScrollCallback(window, handleMouseWheelEvents);
        glfwSetWindowSizeCallback(window, handleWindowSizeChanged);
        glfwSetWindowPosCallback(window, handleWindowPosChanged);
        glfwSetWindowCloseCallback(window, handleWindowCloseEvent);
    }

    /**
     * Key event.
     *
     * Note: called on main thread.
     */
    static void handleKeyEvents(GLFWwindow *window, int key, int scancode, int action, int mods) {
        //TODO Unicode character support: http://www.glfw.org/docs/latest/group__input.html#gabf24451c7ceb1952bc02b17a0d5c3e5f

        AminoGfxMac *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //debug
        //printf("key event: key=%i scancode=%i\n", key, scancode);

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        if (action == GLFW_RELEASE) {
            //release
            Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("key.release").ToLocalChecked());
        } else if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            //press or repeat
            Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("key.press").ToLocalChecked());
        }

        //key codes
        Nan::Set(event_obj, Nan::New("keycode").ToLocalChecked(), Nan::New(key));
        Nan::Set(event_obj, Nan::New("scancode").ToLocalChecked(), Nan::New(scancode));

        obj->fireEvent(event_obj);
    }

    /**
     * Mouse moved.
     *
     * Note: called on main thread.
     */
    static void handleMouseMoveEvents(GLFWwindow *window, double x, double y) {
        AminoGfxMac *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //debug
        //printf("mouse moved event: %f %f\n", x, y);

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),  Nan::New("mouse.position").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("x").ToLocalChecked(),     Nan::New(x));
        Nan::Set(event_obj, Nan::New("y").ToLocalChecked(),     Nan::New(y));

        obj->fireEvent(event_obj);
    }

    /**
     * Mouse click event.
     *
     * Note: called on main thread.
     */
    static void handleMouseClickEvents(GLFWwindow *window, int button, int action, int mods) {
        AminoGfxMac *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //debug
        if (DEBUG_GLFW) {
            printf("mouse clicked event: %i %i\n", button, action);
        }

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("mouse.button").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("button").ToLocalChecked(), Nan::New(button));
        Nan::Set(event_obj, Nan::New("state").ToLocalChecked(),  Nan::New(action));

        obj->fireEvent(event_obj);
    }

    /**
     * Mouse wheel event.
     *
     * Note: called on main thread.
     */
    static void handleMouseWheelEvents(GLFWwindow *window, double xoff, double yoff) {
        AminoGfxMac *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("mousewheel.v").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("xoff").ToLocalChecked(),   Nan::New(xoff));
        Nan::Set(event_obj, Nan::New("yoff").ToLocalChecked(),   Nan::New(yoff));

        obj->fireEvent(event_obj);
    }

    /**
     * Window size has changed.
     */
    static void handleWindowSizeChanged(GLFWwindow *window, int newWidth, int newHeight) {
        if (DEBUG_GLFW) {
            printf("handleWindowSizeChanged() %ix%i\n", newWidth, newHeight);
        }

        AminoGfxMac *obj = windowToInstance(window);

        assert(obj);

        obj->handleWindowSizeChanged(newWidth, newHeight);
    }

    /**
     * Internal event handler.
     *
     * Note: called on main thread.
     */
    void handleWindowSizeChanged(int newWidth, int newHeight) {
    	//check size
        if (propW->value == newWidth && propH->value == newHeight) {
            return;
        }

        //create scope
        Nan::HandleScope scope;

        //update
        updateSize(newWidth, newHeight);

        //debug
        if (DEBUG_GLFW) {
            printf("window size: %ix%i\n", newWidth, newHeight);
        }

        //get framebuffer size
        assert(window);

        glfwGetFramebufferSize(window, &viewportW, &viewportH);
        viewportChanged = true;

        //check framebuffer size
        if (DEBUG_GLFW) {
            printf("framebuffer size: %ix%i\n", viewportW, viewportH);
        }

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("window.size").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("width").ToLocalChecked(),  Nan::New(propW->value));
        Nan::Set(event_obj, Nan::New("height").ToLocalChecked(), Nan::New(propH->value));

        //fire
        fireEvent(event_obj);
    }

    /**
     * Window position has changed.
     */
    static void handleWindowPosChanged(GLFWwindow *window, int newX, int newY) {
        if (DEBUG_GLFW) {
            printf("handleWindowPosChanged() %ix%i\n", newX, newY);
        }

        AminoGfxMac *obj = windowToInstance(window);

        assert(obj);

        obj->handleWindowPosChanged(newX, newY);
    }

    /**
     * Internal event handler.
     *
     * Note: called on main thread.
     */
    void handleWindowPosChanged(int newX, int newY) {
        //check size
        if (propX->value == newX && propY->value == newY) {
            return;
        }

        //create scope
        Nan::HandleScope scope;

        //update
        updatePosition(newX, newY);

        //debug
        if (DEBUG_GLFW) {
            printf("window position: %ix%i\n", newX, newY);
        }

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("window.pos").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("x").ToLocalChecked(),    Nan::New(propX->value));
        Nan::Set(event_obj, Nan::New("y").ToLocalChecked(),    Nan::New(propY->value));

        //fire
        fireEvent(event_obj);
    }

    /**
     * Window close event.
     *
     * Note: called on main thread.
     * Note: window stays open.
     */
    static void handleWindowCloseEvent(GLFWwindow *window) {
        AminoGfxMac *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("window.close").ToLocalChecked());

        //fire
        obj->fireEvent(event_obj);
    }

    void start() override {
        //ready to get control back to JS code
        ready();

        //detach context from main thread
        glfwMakeContextCurrent(NULL);
    }

    bool bindContext() override {
        //bind OpenGL context
        if (!window) {
            return false;
        }

        glfwMakeContextCurrent(window);

        return true;
    }

    void renderingDone() override {
        //debug
        //printf("renderingDone()\n");

        //swap
        assert(window);

        glfwSwapBuffers(window);
    }

    void handleSystemEvents() override {
        //handle events
        glfwPollEvents();
    }

    /**
     * Update the window size.
     *
     * Note: has to be called on main thread
     */
    void updateWindowSize() override {
        //debug
        //printf("updateWindowSize()\n");

        //ignore size changes before window is created
        if (!window) {
            return;
        }

        //Note: getting size changed event
        glfwSetWindowSize(window, propW->value, propH->value);

        //get framebuffer size
        glfwGetFramebufferSize(window, &viewportW, &viewportH);
        viewportChanged = true;

        //check framebuffer size
        if (DEBUG_GLFW) {
            printf("framebuffer size: %ix%i\n", viewportW, viewportH);
        }
    }

    /**
     * Update the window position.
     *
     * Note: has to be called on main thread
     */
    void updateWindowPosition() override {
        //ignore position changes before window is created
        if (!window) {
            return;
        }

        if (DEBUG_GLFW) {
            printf("updateWindowPosition() %i %i\n", (int)propX->value, (int)propY->value);
        }

        glfwSetWindowPos(window, propX->value, propY->value);
    }

    /**
     * Update the title.
     *
     * Note: has to be called on main thread
     */
    void updateWindowTitle() override {
        //ignore title changes before window is created
        if (!window) {
            return;
        }

        glfwSetWindowTitle(window, propTitle->value.c_str());
    }
};

//static initializers
int AminoGfxMac::instanceCount = 0;
bool AminoGfxMac::glfwInitialized = false;
std::map<GLFWwindow *, AminoGfxMac *> *AminoGfxMac::windowMap = new std::map<GLFWwindow *, AminoGfxMac *>();

//
// AminoGfxMacFactory
//

/**
 * Create AminoGfx factory.
 */
AminoGfxMacFactory::AminoGfxMacFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoGfx", callback) {
    //empty
}

/**
 * Create AminoGfx instance.
 */
AminoJSObject* AminoGfxMacFactory::create() {
    return new AminoGfxMac();
}

void exitHandler(void *arg) {
    //Note: not called on Ctrl-C (use process.on('SIGINT', ...))
    if (DEBUG_BASE) {
        printf("app exiting\n");
    }
}

// ========== Event Callbacks ===========

NAN_MODULE_INIT(InitAll) {
    //main class
    AminoGfxMac::Init(target);

    //amino classes
    AminoGfx::InitClasses(target);

    //exit handler
    node::AtExit(exitHandler);
}

//entry point
NODE_MODULE(aminonative, InitAll)
