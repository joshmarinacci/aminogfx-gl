#include "rpi.h"

#include "bcm_host.h"

#include <linux/input.h>
#include <dirent.h>
#include <stdio.h>

#define DEBUG_GLES false
#define DEBUG_RENDER false
#define DEBUG_INPUT false

#define test_bit(bit, array) (array[bit / 8] & (1 << (bit % 8)))

/**
 * Raspberry Pi AminoGfx implementation.
 */
class AminoGfxRPi : public AminoGfx {
public:
    AminoGfxRPi(): AminoGfx(getFactory()->name) {
        //empty
    }

    /**
     * Get factory instance.
     */
    static AminoGfxRPiFactory* getFactory() {
        static AminoGfxRPiFactory *instance = NULL;

        if (!instance) {
            instance = new AminoGfxRPiFactory(New);
        }

        return instance;
    }

    /**
     * Add class template to module exports.
     */
    static NAN_MODULE_INIT(Init) {
        AminoGfxRPiFactory *factory = getFactory();

        AminoGfx::Init(target, factory);
    }

private:
    static int instanceCount;
    static bool glESInitialized;

    //OpenGL ES
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLConfig config;
    uint32_t screenW = 0;
    uint32_t screenH = 0;

    //input
    std::vector<int> fds;
    int mouse_x = 0;
    int mouse_y = 0;

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
        if (DEBUG_GLES) {
            printf("AminoGfxRPi.setup()\n");
        }

        //init OpenGL ES
        if (!glESInitialized) {
            //VideoCore IV
            bcm_host_init();

            glESInitialized = true;
        }

        instanceCount++;

        //basic EGL to get screen size
        initEGL();

        //base class
        AminoGfx::setup();
    }

    /**
     * Initialize EGL and get display size.
     */
    void initEGL() {
        //get an EGL display connection
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

        assert(display != EGL_NO_DISPLAY);

        //initialize the EGL display connection
        EGLBoolean res = eglInitialize(display, NULL, NULL);

        assert(EGL_FALSE != res);

        //get an appropriate EGL frame buffer configuration
        //this uses a BRCM extension that gets the closest match, rather than standard which returns anything that matches
        static const EGLint attribute_list[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_NONE
        };

        EGLint num_config;

        res = eglSaneChooseConfigBRCM(display, attribute_list, &config, 1, &num_config);

        assert(EGL_FALSE != res);

        //choose OpenGL ES 2
        res = eglBindAPI(EGL_OPENGL_ES_API);

        assert(EGL_FALSE != res);

        //create an EGL rendering context
        static const EGLint context_attributes[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

        context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);

        assert(context != EGL_NO_CONTEXT);

        //get display size (see http://elinux.org/Raspberry_Pi_VideoCore_APIs#graphics_get_display_size)
        int32_t success = graphics_get_display_size(0 /* LCD */, &screenW, &screenH);

        assert(success >= 0);
        assert(screenW > 0);
        assert(screenH > 0);

        if (DEBUG_GLES) {
            printf("RPI: display size = %d x %d\n", screenW, screenH);
        }
    }

    /**
     * Destroy GLFW instance.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        //destroy basic instance (activates context)
        AminoGfx::destroy();

        //OpenGL ES
        if (display != EGL_NO_DISPLAY) {
            if (context != EGL_NO_CONTEXT) {
                eglDestroyContext(display, context);
                context = EGL_NO_CONTEXT;
            }

            if (surface != EGL_NO_SURFACE) {
                eglDestroySurface(display, surface);
                surface = EGL_NO_SURFACE;
            }

            eglTerminate(display);
            display = EGL_NO_DISPLAY;
        }

        instanceCount--;

        if (DEBUG_GLES) {
            printf("Destroyed OpenGL ES/EGL instance. Left=%i\n", instanceCount);
        }

        if (instanceCount == 0) {
            //VideoCore IV
            bcm_host_deinit();
            glESInitialized = false;
        }
    }

    /**
     * Get default monitor resolution.
     */
    bool getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) override {
        if (DEBUG_GLES) {
            printf("getScreenInfo\n");
        }

        //get display properties
        w = screenW;
        h = screenH;
        refreshRate = 0; //unknown
        fullscreen = true;

        return true;
    }

    /**
     * Add VideoCore IV properties.
     */
    void populateRuntimeProperties(v8::Local<v8::Object> &obj) override {
        if (DEBUG_GLES) {
            printf("populateRuntimeProperties\n");
        }

        AminoGfx::populateRuntimeProperties(obj);

        //GLES
        Nan::Set(obj, Nan::New("eglVendor").ToLocalChecked(), Nan::New(std::string(eglQueryString(display, EGL_VENDOR))).ToLocalChecked());
        Nan::Set(obj, Nan::New("eglVersion").ToLocalChecked(), Nan::New(std::string(eglQueryString(display, EGL_VERSION))).ToLocalChecked());
    }

    /**
     * Initialize OpenGL ES.
     */
    void initRenderer() override {
        if (DEBUG_GLES) {
            printf("initRenderer()\n");
        }

        AminoGfx::initRenderer();

        //set display size & viewport
        updateSize(screenW, screenH);
        updatePosition(0, 0);

        viewportW = screenW;
        viewportH = screenH;
        viewportChanged = true;

        //Dispmanx init
        DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0 /* LCD */);
        DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);

        int LAYER = 0;
        VC_RECT_T dst_rect;
        VC_RECT_T src_rect;

        dst_rect.x = 0;
        dst_rect.y = 0;
        dst_rect.width = screenW;
        dst_rect.height = screenH;

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = screenW << 16;
        src_rect.height = screenH << 16;

        VC_DISPMANX_ALPHA_T dispman_alpha;

        dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;
        dispman_alpha.opacity = 255;
        dispman_alpha.mask = 0;

        DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
            LAYER, //layer
            &dst_rect,
            0, //src
            &src_rect,
            DISPMANX_PROTECTION_NONE,
            &dispman_alpha,  //alpha
            0, //clamp,
            (DISPMANX_TRANSFORM_T)0 //transform
        );

        vc_dispmanx_update_submit_sync(dispman_update);

        //create EGL surface
        static EGL_DISPMANX_WINDOW_T native_window;

        native_window.element = dispman_element;
        native_window.width = screenW;
        native_window.height = screenH;

        surface = eglCreateWindowSurface(display, config, &native_window, NULL);

        assert(surface != EGL_NO_SURFACE);

        //activate context (needed by JS code to create shaders)
        EGLBoolean res = eglMakeCurrent(display, surface, surface, context);

        assert(EGL_FALSE != res);

        //input
        initInput();
    }

    const char* INPUT_DIR = "/dev/input";

    bool startsWith(const char *pre, const char *str) {
        size_t lenpre = strlen(pre);
        size_t lenstr = strlen(str);

        return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
    }

    void initInput() {
        if ((getuid()) != 0) {
            printf("you are not root. this might not work\n");
        }

        DIR *dir;
        struct dirent *file;

        dir = opendir(INPUT_DIR);

        if (dir) {
            while ((file = readdir(dir)) != NULL) {
                if (!startsWith("event", file->d_name)) {
                    continue;
                }

                if (DEBUG_INPUT) {
                    printf("file = %s\n", file->d_name);
                    printf("initing a device\n");
                }

                char str[256];

                strcpy(str, INPUT_DIR);
                strcat(str, "/");
                strcat(str, file->d_name);

                int fd = -1;
                if ((fd = open(str, O_RDONLY | O_NONBLOCK)) == -1) {
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

                if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evtype_b) < 0) {
                    printf("error reading device info\n");
                    continue;
                }

                for (int i = 0; i < EV_MAX; i++) {
                    if (test_bit(i, evtype_b)) {
                        printf("event type 0x%02x ", i);

                        switch (i) {
                            case EV_SYN:
                                printf("sync events\n");
                                break;

                            case EV_KEY:
                                printf("key events\n");
                                break;

                            case EV_REL:
                                printf("rel events\n");
                                break;

                            case EV_ABS:
                                printf("abs events\n");
                                break;

                            case EV_MSC:
                                printf("msc events\n");
                                break;

                            case EV_SW:
                                printf("sw events\n");
                                break;

                            case EV_LED:
                                printf("led events\n");
                                break;

                            case EV_SND:
                                printf("snd events\n");
                                break;

                            case EV_REP:
                                printf("rep events\n");
                                break;

                            case EV_FF:
                                printf("ff events\n");
                                break;
                        }
                    }
                }

                fds.push_back(fd);
            }

            closedir(dir);
        }
    }

    void start() override {
        //ready to get control back to JS code
        ready();

        //detach context from main thread
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    bool bindContext() override {
        //bind OpenGL context
        if (surface == EGL_NO_SURFACE) {
            return false;
        }

        EGLBoolean res = eglMakeCurrent(display, surface, surface, context);

        assert(res == EGL_TRUE);

        return true;
    }

    void renderingDone() override {
        if (DEBUG_GLES) {
            printf("renderingDone()\n");
        }

        //swap
        EGLBoolean res = eglSwapBuffers(display, surface);

        assert(res == EGL_TRUE);
    }

    void handleSystemEvents() override {
        //handle events
        processInputs();
    }

    void processInputs() {
        if (DEBUG_GLES) {
            printf("processInputs()\n");
        }

        int size = sizeof(struct input_event);
        struct input_event ev[64];

        for (unsigned int i = 0; i < fds.size(); i++) {
            int fd = fds[i];
            int rd = read(fd, ev, size*64);

            if (rd == -1) {
                continue;
            }

            if (rd < size) {
                printf("read too little!!!  %d\n", rd);
            }

            for (int i = 0; i < (int)(rd / size); i++) {
                //dump_event(&(ev[i]));
                handleEvent(ev[i]);
            }
        }

        if (DEBUG_GLES) {
            printf("-> done\n");
        }
    }

    void handleEvent(input_event ev) {
        //relative event. probably mouse
        if (ev.type == EV_REL) {
            if (ev.code == 0) {
                //x axis
                mouse_x += ev.value;
            }

            if (ev.code == 1) {
                mouse_y += ev.value;
            }

            if (mouse_x < 0) {
                mouse_x = 0;
            }

            if (mouse_y < 0) {
                mouse_y = 0;
            }

            if (mouse_x >= (int)screenW)  {
                mouse_x = (int)screenW - 1;
            }

            if (mouse_y >= (int)screenH) {
                mouse_y = (int)screenH - 1;
            }

            //TODO GLFW_MOUSE_POS_CALLBACK_FUNCTION(mouse_x, mouse_y);

            return;
        }

        //mouse wheel
        if (ev.type == EV_REL && ev.code == 8) {
            //TODO GLFW_MOUSE_WHEEL_CALLBACK_FUNCTION(ev.value);
            return;
        }

        if (ev.type == EV_KEY) {
            if (DEBUG_GLES) {
                printf("key or button pressed code = %d, state = %d\n", ev.code, ev.value);
            }

            if (ev.code == BTN_LEFT) {
                //TODO GLFW_MOUSE_BUTTON_CALLBACK_FUNCTION(ev.code, ev.value);
                return;
            }

            //TODO GLFW_KEY_CALLBACK_FUNCTION(ev.code, ev.value);

            return;
        }
    }

    void dump_event(struct input_event *event) {
        switch(event->type) {
            case EV_SYN:
                printf("EV_SYN  event separator\n");
                break;

            case EV_KEY:
                printf("EV_KEY  keyboard or button \n");

                if (event ->code == KEY_A) {
                    printf("  A key\n");
                }

                if (event ->code == KEY_B) {
                    printf("  B key\n");
                }
                break;

            case EV_REL:
                printf("EV_REL  relative axis\n");
                break;

            case EV_ABS:
                printf("EV_ABS  absolute axis\n");
                break;

            case EV_MSC:
                printf("EV_MSC  misc\n");
                if (event->code == MSC_SERIAL) {
                    printf("  serial\n");
                }

                if (event->code == MSC_PULSELED) {
                    printf("  pulse led\n");
                }

                if (event->code == MSC_GESTURE) {
                    printf("  gesture\n");
                }

                if (event->code == MSC_RAW) {
                    printf("  raw\n");
                }

                if (event->code == MSC_SCAN) {
                    printf("  scan\n");
                }

                if (event->code == MSC_MAX) {
                    printf("  max\n");
                }
                break;

            case EV_LED:
                printf("EV_LED  led\n");
                break;

            case EV_SND:
                printf("EV_SND  sound\n");
                break;

            case EV_REP:
                printf("EV_REP  autorepeating\n");
                break;

            case EV_FF:
                printf("EV_FF   force feedback send\n");
                break;

            case EV_PWR:
                printf("EV_PWR  power button\n");
                break;

            case EV_FF_STATUS:
                printf("EV_FF_STATUS force feedback receive\n");
                break;

            case EV_MAX:
                printf("EV_MAX  max value\n");
                break;
        }

        printf("type = %d code = %d value = %d\n",event->type,event->code,event->value);
    }

    /**
     * Update the window size.
     *
     * Note: has to be called on main thread
     */
    void updateWindowSize() override {
        //not supported

        //reset to screen values
        propW->setValue(screenW);
        propH->setValue(screenH);
    }

    /**
     * Update the window position.
     *
     * Note: has to be called on main thread
     */
    void updateWindowPosition() override {
        //not supported
        propX->setValue(0);
        propY->setValue(0);
    }

    /**
     * Update the title.
     *
     * Note: has to be called on main thread
     */
    void updateWindowTitle() override {
        //not supported
    }
};

int AminoGfxRPi::instanceCount = 0;
bool AminoGfxRPi::glESInitialized = false;

//
// AminoGfxRPiFactory
//

/**
 * Create AminoGfx factory.
 */
AminoGfxRPiFactory::AminoGfxRPiFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoGfx", callback) {
    //empty
}

/**
 * Create AminoGfx instance.
 */
AminoJSObject* AminoGfxRPiFactory::create() {
    return new AminoGfxRPi();
}

// ========== Event Callbacks ===========

NAN_MODULE_INIT(InitAll) {
    //main class
    AminoGfxRPi::Init(target);

    //amino classes
    AminoGfx::InitClasses(target);
}

//entry point
NODE_MODULE(aminonative, InitAll)
