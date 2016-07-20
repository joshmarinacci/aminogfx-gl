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
        static AminoGfxMacFactory *instance;

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
    GLFWwindow *window;
    bool resizing = false;

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
        AminoGfx::destroy();

        //GLFW
        if (window) {
            glfwDestroyWindow(window);
            windowMap->erase(window);
            window = NULL;
        }

        instanceCount--;

        if (instanceCount == 0) {
            glfwTerminate();
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

        //check framebuffer size
        if (DEBUG_GLFW) {
            printf("framebuffer size: %ix%i\n", viewportW, viewportH);
        }

        //activate context
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
     */
    static void handleKeyEvents(GLFWwindow *window, int key, int scancode, int action, int mods) {
        //TODO Unicode character support: http://www.glfw.org/docs/latest/group__input.html#gabf24451c7ceb1952bc02b17a0d5c3e5f

        AminoGfxMac *obj = windowToInstance(window);

        if (!obj) {
            return;
        }

        //create scope
        Nan::HandleScope scope;

        //debug
        //printf("key event: key=%i scancode=%i\n", key, scancode);

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        if (action == GLFW_RELEASE) {
            //release
            Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("keyrelease").ToLocalChecked());
        } else if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            //press or repeat
            Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("keypress").ToLocalChecked());
        }

        //key codes
        Nan::Set(event_obj, Nan::New("keycode").ToLocalChecked(), Nan::New(key));
        Nan::Set(event_obj, Nan::New("scancode").ToLocalChecked(), Nan::New(scancode));

        obj->fireEvent(event_obj);
    }

    /**
     * Mouse moved.
     */
    static void handleMouseMoveEvents(GLFWwindow *window, double x, double y) {
        AminoGfxMac *obj = windowToInstance(window);

        if (!obj) {
            return;
        }

        //create scope
        Nan::HandleScope scope;

        //debug
        //printf("mouse moved event: %f %f\n", x, y);

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),  Nan::New("mouseposition").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("x").ToLocalChecked(),     Nan::New(x));
        Nan::Set(event_obj, Nan::New("y").ToLocalChecked(),     Nan::New(y));

        obj->fireEvent(event_obj);
    }

    /**
     * Mouse click event.
     */
    static void handleMouseClickEvents(GLFWwindow *window, int button, int action, int mods) {
        AminoGfxMac *obj = windowToInstance(window);

        if (!obj) {
            return;
        }

        //create scope
        Nan::HandleScope scope;

        //debug
        if (DEBUG_GLFW) {
            printf("mouse clicked event: %i %i\n", button, action);
        }

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("mousebutton").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("button").ToLocalChecked(), Nan::New(button));
        Nan::Set(event_obj, Nan::New("state").ToLocalChecked(),  Nan::New(action));

        obj->fireEvent(event_obj);
    }

    /**
     * Mouse wheel event.
     */
    static void handleMouseWheelEvents(GLFWwindow *window, double xoff, double yoff) {
        AminoGfxMac *obj = windowToInstance(window);

        if (!obj) {
            return;
        }

        //create scope
        Nan::HandleScope scope;

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("mousewheelv").ToLocalChecked());
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

        if (!obj) {
            return;
        }

        obj->handleWindowSizeChanged(newWidth, newHeight);
    }

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
        glfwGetFramebufferSize(window, &viewportW, &viewportH);

        //check framebuffer size
        if (DEBUG_GLFW) {
            printf("framebuffer size: %ix%i\n", viewportW, viewportH);
        }

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("windowsize").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("width").ToLocalChecked(),  Nan::New(propW->value));
        Nan::Set(event_obj, Nan::New("height").ToLocalChecked(), Nan::New(propH->value));

        //render
        resizing = true;
        render();
        resizing = false;

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

        if (!obj) {
            return;
        }

        obj->handleWindowPosChanged(newX, newY);
    }

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

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("windowpos").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("x").ToLocalChecked(),    Nan::New(propX->value));
        Nan::Set(event_obj, Nan::New("y").ToLocalChecked(),    Nan::New(propY->value));

        //fire
        fireEvent(event_obj);
    }

    /**
     * Window close event.
     *
     * Note: window stays open.
     */
    static void handleWindowCloseEvent(GLFWwindow *window) {
        AminoGfxMac *obj = windowToInstance(window);

        if (!obj) {
            return;
        }

        //create scope
        Nan::HandleScope scope;

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),     Nan::New("windowclose").ToLocalChecked());

        //fire
        obj->fireEvent(event_obj);
    }

    void start() override {
        //ready to get control back to JS code
        ready();
    }

    void bindContext() override {
        //bind OpenGL context
        glfwMakeContextCurrent(window);
    }

    void renderingDone() override {
        //debug
        //printf("renderingDone()\n");

        //swap
        glfwSwapBuffers(window);

        //handle events
        if (!resizing) {
            glfwPollEvents();
        }
    }

    /**
     * Update the window size.
     */
    void updateWindowSize() override {
        //debug
        //printf("updateWindowSize\n");

        //Note: getting size changed event
        glfwSetWindowSize(window, propW->value, propH->value);

        //get framebuffer size
        glfwGetFramebufferSize(window, &viewportW, &viewportH);

        //check framebuffer size
        if (DEBUG_GLFW) {
            printf("framebuffer size: %ix%i\n", viewportW, viewportH);
        }
    }

    /**
     * Update the window position.
     */
    void updateWindowPosition() override {
        glfwSetWindowPos(window, propX->value, propY->value);
    }

    /**
     * Update the title.
     */
    void updateWindowTitle() override {
        glfwSetWindowTitle(window, propTitle->value.c_str());
    }
};

int AminoGfxMac::instanceCount;
bool AminoGfxMac::glfwInitialized;
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

// ========== Event Callbacks ===========

NAN_MODULE_INIT(InitAll) {
    //main class
    AminoGfxMac::Init(target);

    //image class
    AminoImage::Init(target);

    //TODO cleanup
    Nan::Set(target, Nan::New("createText").ToLocalChecked(),       Nan::GetFunction(Nan::New<v8::FunctionTemplate>(createText)).ToLocalChecked());
    Nan::Set(target, Nan::New("createNativeFont").ToLocalChecked(), Nan::GetFunction(Nan::New<v8::FunctionTemplate>(createNativeFont)).ToLocalChecked());
    Nan::Set(target, Nan::New("getCharWidth").ToLocalChecked(),     Nan::GetFunction(Nan::New<v8::FunctionTemplate>(getCharWidth)).ToLocalChecked());
    Nan::Set(target, Nan::New("getFontHeight").ToLocalChecked(),    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(getFontHeight)).ToLocalChecked());
    Nan::Set(target, Nan::New("getFontAscender").ToLocalChecked(),    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(getFontAscender)).ToLocalChecked());
    Nan::Set(target, Nan::New("getFontDescender").ToLocalChecked(),    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(getFontDescender)).ToLocalChecked());
    //TODO other platforms
    Nan::Set(target, Nan::New("getTextLineCount").ToLocalChecked(),    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(getTextLineCount)).ToLocalChecked());
    Nan::Set(target, Nan::New("getTextHeight").ToLocalChecked(),    Nan::GetFunction(Nan::New<v8::FunctionTemplate>(getTextHeight)).ToLocalChecked());
}

//entry point
NODE_MODULE(aminonative, InitAll)
