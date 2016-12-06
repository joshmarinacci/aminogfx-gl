#include "rpi.h"

#include "bcm_host.h"
#include "interface/vchiq_arm/vchiq_if.h"

#include <linux/input.h>
#include <dirent.h>
#include <stdio.h>
#include <semaphore.h>

#include <execinfo.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#define gettid() syscall(SYS_gettid)

#define DEBUG_GLES false
#define DEBUG_RENDER false
#define DEBUG_INPUT false
#define DEBUG_HDMI false
//cbx
#define DEBUG_OMX true
#define DEBUG_OMX_READ false

#define AMINO_EGL_SAMPLES 4
#define test_bit(bit, array) (array[bit / 8] & (1 << (bit % 8)))

/**
 * Raspberry Pi AminoGfx implementation.
 */
class AminoGfxRPi : public AminoGfx {
public:
    AminoGfxRPi(): AminoGfx(getFactory()->name) {
        //empty
    }

    ~AminoGfxRPi() {
        if (!destroyed) {
            destroyAminoGfxRPi();
        }
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
    static bool glESInitialized;

    //OpenGL ES
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLConfig config;
    uint32_t screenW = 0;
    uint32_t screenH = 0;

    //resolution
    static sem_t resSem;
    static bool resSemValid;

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

            //Note: tvservice and others are already initialized by bcm_host_init() call!
            //      see https://github.com/raspberrypi/userland/blob/master/host_applications/linux/libs/bcm_host/bcm_host.c

            /*
             * register callback
             *
             * Note: never called with "hdmi_force_hotplug=1".
             */
            vc_tv_register_callback(tvservice_cb, NULL);

            //handle preferred resolution
            if (!createParams.IsEmpty()) {
                v8::Local<v8::Object> obj = Nan::New(createParams);

                //resolution
                Nan::MaybeLocal<v8::Value> resolutionMaybe = Nan::Get(obj, Nan::New<v8::String>("resolution").ToLocalChecked());

                if (!resolutionMaybe.IsEmpty()) {
                    v8::Local<v8::Value> resolutionValue = resolutionMaybe.ToLocalChecked();

                    if (resolutionValue->IsString()) {
                        std::string prefRes = AminoJSObject::toString(resolutionValue);

                        //change resolution

                        if (prefRes == "720p@24") {
                            forceHdmiMode(HDMI_CEA_720p24);
                        } else if (prefRes == "720p@25") {
                            forceHdmiMode(HDMI_CEA_720p25);
                        } else if (prefRes == "720p@30") {
                            forceHdmiMode(HDMI_CEA_720p30);
                        } else if (prefRes == "720p@50") {
                            forceHdmiMode(HDMI_CEA_720p50);
                        } else if (prefRes == "720p@60") {
                            forceHdmiMode(HDMI_CEA_720p60);
                        } else if (prefRes == "1080p@24") {
                            forceHdmiMode(HDMI_CEA_1080p24);
                        } else if (prefRes == "1080p@25") {
                            forceHdmiMode(HDMI_CEA_1080p25);
                        } else if (prefRes == "1080p@30") {
                            forceHdmiMode(HDMI_CEA_1080p30);
                        } else if (prefRes == "1080p@50") {
                            forceHdmiMode(HDMI_CEA_1080p50);
                        } else if (prefRes == "1080p@60") {
                            forceHdmiMode(HDMI_CEA_1080p60);
                        } else {
                            printf("unknown resolution: %s\n", prefRes.c_str());
                        }
                    }
                }
            }

            glESInitialized = true;
        }

        //instance
        addInstance();

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
            //RGBA
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,

            //OpenGL ES 2.0
            EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,

            //buffers
            EGL_STENCIL_SIZE, 8,
            EGL_DEPTH_SIZE, 16,

            //sampling (quality)
            EGL_SAMPLE_BUFFERS, 1,
            EGL_SAMPLES, AMINO_EGL_SAMPLES, //4: 4x MSAA

            //window
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

    static TV_DISPLAY_STATE_T *getDisplayState() {
        /*
         * Get TV state.
         *
         * - https://github.com/bmx-ng/sdl.mod/blob/master/sdlgraphics.mod/rpi_glue.c
         * - https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_hdmi.h
         */
        TV_DISPLAY_STATE_T *tvstate = (TV_DISPLAY_STATE_T *)malloc(sizeof(TV_DISPLAY_STATE_T));

        if (vc_tv_get_display_state(tvstate) != 0) {
            free(tvstate);
            tvstate = NULL;
        } else {
            /*
             * Notes:
             *
             * - Raspberry Pi 3 default:
             *    - hdmi_force_hotplug=1 is being used in /boot/config.txt
             *    - 1920x1080@60Hz on HDMI (mode=16, group=1)
             */

            //debug
            if (DEBUG_HDMI) {
                printf("State: %i\n", tvstate->state);
            }

            //check HDMI
            if ((tvstate->state & VC_HDMI_UNPLUGGED) == VC_HDMI_UNPLUGGED) {
                if (DEBUG_HDMI) {
                    printf("-> unplugged\n");
                }

                free(tvstate);
                tvstate = NULL;
            } else if ((tvstate->state & (VC_HDMI_HDMI | VC_HDMI_DVI)) == 0) {
                if (DEBUG_HDMI) {
                    printf("-> no HDMI display\n");
                }

                free(tvstate);
                tvstate = NULL;
            }

            if (tvstate && DEBUG_HDMI) {
                printf("Currently outputting %ix%i@%iHz on HDMI (mode=%i, group=%i).\n", tvstate->display.hdmi.width, tvstate->display.hdmi.height, tvstate->display.hdmi.frame_rate, tvstate->display.hdmi.mode, tvstate->display.hdmi.group);
            }
        }

        return tvstate;
    }

    /**
     * HDMI tvservice callback.
     */
    static void tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2) {
        //http://www.m2x.nl/videolan/vlc/blob/1d2b56c68bbc3287e17f6140bdf8c8c3efe08fdc/modules/hw/mmal/vout.c

        if (DEBUG_HDMI) {
            //reasons seen: VC_HDMI_HDMI
            printf("-> tvservice state has changed: %s\n", vc_tv_notification_name((VC_HDMI_NOTIFY_T)reason));
        }

        //check resolution
        TV_DISPLAY_STATE_T *tvState = getDisplayState();

        if (tvState) {
            //Note: new resolution used by DispmanX calls
            free(tvState);
        }

        //check sem
        if (resSemValid) {
            sem_post(&resSem);
        }
    }

    /**
     * Destroy GLFW instance.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        //instance
        destroyAminoGfxRPi();

        //destroy basic instance (activates context)
        AminoGfx::destroy();
    }

    /**
     * Destroy GLFW instance.
     */
    void destroyAminoGfxRPi() {
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

        removeInstance();

        if (DEBUG_GLES) {
            printf("Destroyed OpenGL ES/EGL instance. Left=%i\n", instanceCount);
        }

        if (instanceCount == 0) {
            //tvservice
            vc_tv_unregister_callback(tvservice_cb);

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

        TV_DISPLAY_STATE_T *tvState = getDisplayState();

        //get display properties
        w = screenW;
        h = screenH;
        refreshRate = 0; //unknown
        fullscreen = true;

        if (tvState) {
            //depends on attached screen
            refreshRate = tvState->display.hdmi.frame_rate;
        }

        //free
        if (tvState) {
            free(tvState);
        }

        return true;
    }

    /**
     * Switch to HDMI mode.
     */
    void forceHdmiMode(uint32_t code) {
        if (DEBUG_HDMI) {
            printf("Changing resolution to CEA code %i\n", (int)code);
        }

        //Note: mode change takes some time (is asynchronous)
        //      see https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_hdmi.h
        sem_init(&resSem, 0, 0);
        resSemValid = true;

        if (vc_tv_hdmi_power_on_explicit(HDMI_MODE_HDMI, HDMI_RES_GROUP_CEA, code) != 0) {
            printf("-> failed\n");
            return;
        }

        //wait for change to occur before DispmanX is initialized
        sem_wait(&resSem);
        sem_destroy(&resSem);
        resSemValid = false;
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

        //base
        AminoGfx::initRenderer();

        //set display size & viewport
        updateSize(screenW, screenH);
        updatePosition(0, 0);

        viewportW = screenW;
        viewportH = screenH;
        viewportChanged = true;

        if (DEBUG_HDMI) {
            printf("-> init Dispmanx\n");
        }

        //Dispmanx init
        DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0); //LCD
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

        //Notes: works but seeing issues with font shader which uses transparent pixels! The background is visible at the border pixels of the font!
        /*
        dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;
        dispman_alpha.opacity = 255;
        dispman_alpha.mask = 0;
        */

        dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
        dispman_alpha.opacity = 0xFF;
        dispman_alpha.mask = 0;

        DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add(
            dispman_update,
            dispman_display,
            LAYER, //layer
            &dst_rect,
            0, //src
            &src_rect,
            DISPMANX_PROTECTION_NONE,
            &dispman_alpha, //alpha
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

        //swap interval
        if (swapInterval != 0) {
            res = eglSwapInterval(display, swapInterval);

            assert(res == EGL_TRUE);
        }

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

        //swap buffer
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

    /**
     * Shared atlas texture has changed.
     */
    void atlasTextureHasChanged(texture_atlas_t *atlas) override {
        //check single instance case
        if (instanceCount == 1) {
            return;
        }

        //run on main thread
        enqueueJSCallbackUpdate(static_cast<jsUpdateCallback>(&AminoGfxRPi::atlasTextureHasChangedHandler), NULL, atlas);
    }

    /**
     * Handle on main thread.
     */
    void atlasTextureHasChangedHandler(JSCallbackUpdate *update) {
        AminoGfx *gfx = static_cast<AminoGfx *>(update->obj);
        texture_atlas_t *atlas = (texture_atlas_t *)update->data;

        for (auto const &item : instances) {
            if (gfx == item) {
                continue;
            }

            static_cast<AminoGfxRPi *>(item)->updateAtlasTexture(atlas);
        }
    }

    /**
     * Create video player.
     */
    AminoVideoPlayer *createVideoPlayer(AminoTexture *texture, AminoVideo *video) override {
        return new AminoOmxVideoPlayer(texture, video);
    }

public:
    /**
     * Create EGL Image.
     */
    EGLImageKHR createEGLImage(GLuint textureId) {
        //TODO eglDestroyImageKHR(state->display, (EGLImageKHR) eglImage)
        return eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)textureId, 0);
    }
};

//static initializers
bool AminoGfxRPi::glESInitialized = false;
sem_t AminoGfxRPi::resSem;
bool AminoGfxRPi::resSemValid = false;

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

//
// AminoOmxVideoPlayer
//

AminoOmxVideoPlayer::AminoOmxVideoPlayer(AminoTexture *texture, AminoVideo *video): AminoVideoPlayer(texture, video) {
    //empty
}

/**
 * Initialize the video player.
 */
void AminoOmxVideoPlayer::init() {
    //we are on the OpenGL thread

    //debug
    //printf("file: %s\n", fileName.c_str());

    //check video file
    file = fopen(fileName.c_str(), "rb");

    if (!file) {
        lastError = "file not found";
        return;
    }

    //init texture
    if (!initTexture()) {
        return;
    }

    //create thread
    int res = uv_thread_create(&thread, omxThread, this);

    assert(res == 0);

    threadRunning = true;
}

bool AminoOmxVideoPlayer::initTexture() {
    glBindTexture(GL_TEXTURE_2D, texture->textureId);

    //size (not set here)
    //cbx Note: needs texture size or EGL image cannot be created!
    GLsizei textureW = 1920;
    GLsizei textureH = 1080;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureW, textureH, 0,GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    //create EGL Image
    AminoGfxRPi *gfx = static_cast<AminoGfxRPi *>(texture->getEventHandler());

    assert(texture->textureId != INVALID_TEXTURE);

    eglImage = gfx->createEGLImage(texture->textureId);

    if (eglImage == EGL_NO_IMAGE_KHR) {
        lastError = "eglCreateImageKHR failed";

        return false;
    }

    return true;
}

/**
 * OMX thread.
 */
void AminoOmxVideoPlayer::omxThread(void *arg) {
    AminoOmxVideoPlayer *player = static_cast<AminoOmxVideoPlayer *>(arg);

    assert(player);

    //init OMX
    player->initOmx();

    //done
    player->threadRunning = false;
}

/**
 * OMX buffer callback.
 */
void AminoOmxVideoPlayer::handleFillBufferDone(void *data, COMPONENT_T *comp) {
    AminoOmxVideoPlayer *player = static_cast<AminoOmxVideoPlayer *>(data);

    assert(player);

    if (DEBUG_OMX) {
        printf("OMX: handleFillBufferDone()\n");
    }

    if (OMX_FillThisBuffer(ilclient_get_handle(player->egl_render), player->eglBuffer) != OMX_ErrorNone) {
        player->bufferError = true;
        printf("OMX_FillThisBuffer failed in callback\n");
    }
}

/**
 * Initialize OpenMax.
 */
bool AminoOmxVideoPlayer::initOmx() {
    int status = 0;

    //init il client
    client = ilclient_init();

    if (!client) {
        fclose(file);

        lastError = "could not initialize ilclient";

        return false;
    }

    //init OMX
    if (OMX_Init() != OMX_ErrorNone) {
        ilclient_destroy(client);
        fclose(file);

        lastError = "could not initialize OMX";

        return false;
    }

    //buffer callback
    ilclient_set_fill_buffer_done_callback(client, handleFillBufferDone, this);

    //create video_decode
    COMPONENT_T *video_decode = NULL;

    if (ilclient_create_component(client, &video_decode, "video_decode", (ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS)) != 0) {
        lastError = "video_decode error";
        status = -14;
    }

    memset(list, 0, sizeof(list));
    list[0] = video_decode;

    //create egl_render
    if (status == 0 && ilclient_create_component(client, &egl_render, "egl_render", (ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_OUTPUT_BUFFERS)) != 0) {
        lastError = "egl_render error";
        status = -14;
    }

    list[1] = egl_render;

    //create clock
    COMPONENT_T *clock = NULL;

    if (status == 0 && ilclient_create_component(client, &clock, "clock", (ILCLIENT_CREATE_FLAGS_T)ILCLIENT_DISABLE_ALL_PORTS) != 0) {
        lastError = "clock error";
        status = -14;
    }

    list[2] = clock;

    if (clock) {
        OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;

        memset(&cstate, 0, sizeof(cstate));
        cstate.nSize = sizeof(cstate);
        cstate.nVersion.nVersion = OMX_VERSION;
        cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
        cstate.nWaitMask = 1;

        if (OMX_SetParameter(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone) {
            lastError = "could not set clock";
            status = -13;
        }
    }

    //create video_scheduler
    COMPONENT_T *video_scheduler = NULL;

    if (status == 0 && ilclient_create_component(client, &video_scheduler, "video_scheduler", (ILCLIENT_CREATE_FLAGS_T)ILCLIENT_DISABLE_ALL_PORTS) != 0) {
        lastError = "video_scheduler error";
        status = -14;
    }

    list[3] = video_scheduler;

    memset(tunnel, 0, sizeof(tunnel));

    set_tunnel(tunnel, video_decode, 131, video_scheduler, 10);
    set_tunnel(tunnel + 1, video_scheduler, 11, egl_render, 220);
    set_tunnel(tunnel + 2, clock, 80, video_scheduler, 12);

    //setup clock tunnel first
    if (status == 0 && ilclient_setup_tunnel(tunnel + 2, 0, 0) != 0) {
        lastError = "tunnel setup error";
        status = -15;
    } else {
        //switch to executing state (why?)
        ilclient_change_component_state(clock, OMX_StateExecuting);
    }

    if (status == 0) {
        //switch to idle state
        ilclient_change_component_state(video_decode, OMX_StateIdle);
    }

    //format
    if (status == 0) {
        OMX_VIDEO_PARAM_PORTFORMATTYPE format;

        memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
        format.nVersion.nVersion = OMX_VERSION;
        format.nPortIndex = 130;
        format.eCompressionFormat = OMX_VIDEO_CodingAVC;

        if (OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamVideoPortFormat, &format) != OMX_ErrorNone) {
            lastError = "could not set video format";
            status = -16;
        }
    }

    //video decode
    if (status == 0) {
        if (ilclient_enable_port_buffers(video_decode, 130, NULL, NULL, NULL) != 0) {
            lastError = "video decode port error";
            status = -17;
        }
    }

    //init done
    omxInitialized = true;

    //debug
    if (DEBUG_OMX) {
        printf("OMX init status: %i\n", status);
    }

    //loop
    if (status == 0) {
        OMX_BUFFERHEADERTYPE *buf;
        bool port_settings_changed = false;
        bool first_packet = true;

        //executing
        ilclient_change_component_state(video_decode, OMX_StateExecuting);

        //TODO cbx get video size & call callback
        videoW = 1920;
        videoH = 1080;
        ready = true;
        texture->videoPlayerInitDone();

        //data loop
        while ((buf = ilclient_get_input_buffer(video_decode, 130, 1)) != NULL) {
            //feed data and wait until we get port settings changed
            unsigned char *dest = buf->pBuffer;

            //loop if at end
            if (feof(file)) {
                if (DEBUG_OMX) {
                    //cbx FIXME seeing many rewinds!
                    printf("OMX: rewind file\n");
                }

                //cbx TODO play mode
                rewind(file);
            }

            //read from file
            unsigned int data_len = fread(dest, 1, buf->nAllocLen, file);

            if (DEBUG_OMX_READ) {
                printf("OMX: data read %i\n", (int)data_len);
            }

            if (!port_settings_changed &&
                ((data_len > 0 && ilclient_remove_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1) == 0) ||
                (data_len == 0 && ilclient_wait_for_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1, ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000) == 0))) {
                //process once
                port_settings_changed = true;

                if (DEBUG_OMX) {
                    printf("OMX: egl_render setup\n");
                }

                if (ilclient_setup_tunnel(tunnel, 0, 0) != 0) {
                    lastError = "video tunnel setup error";
                    status = -7;
                    break;
                }

                ilclient_change_component_state(video_scheduler, OMX_StateExecuting);

                //now setup tunnel to egl_render
                if (ilclient_setup_tunnel(tunnel + 1, 0, 1000) != 0) {
                    lastError = "egl_render tunnel setup error";
                    status = -12;
                    break;
                }

                //Set egl_render to idle
                ilclient_change_component_state(egl_render, OMX_StateIdle);

                //get video size
                OMX_PARAM_PORTDEFINITIONTYPE portdef;

                memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
                portdef.nVersion.nVersion = OMX_VERSION;
                portdef.nPortIndex = 131;

                if (OMX_GetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamPortDefinition, &portdef) != OMX_ErrorNone) {
                    lastError = "could not get video size";
                    status = -20;
                    break;
                }

                //cbx TODO get correct egl image buffer
                printf("video: %dx%d\n", portdef.format.video.nFrameWidth, portdef.format.video.nFrameHeight);

                //Enable the output port and tell egl_render to use the texture as a buffer
                //ilclient_enable_port(egl_render, 221); THIS BLOCKS SO CAN'T BE USED
                if (OMX_SendCommand(ILC_GET_HANDLE(egl_render), OMX_CommandPortEnable, 221, NULL) != OMX_ErrorNone) {
                    lastError = "OMX_CommandPortEnable failed.";
                    status = -21;
                    break;
                }

                if (OMX_UseEGLImage(ILC_GET_HANDLE(egl_render), &eglBuffer, 221, NULL, eglImage) != OMX_ErrorNone) {
                    lastError = "OMX_UseEGLImage failed.";
                    status = -22;
                    break;
                }

                if (DEBUG_OMX) {
                    printf("OMX: egl_render setup done\n");
                }

                //Set egl_render to executing
                ilclient_change_component_state(egl_render, OMX_StateExecuting);

                if (DEBUG_OMX) {
                    printf("OMX: executing\n");
                }

                //Request egl_render to write data to the texture buffer
                if (OMX_FillThisBuffer(ILC_GET_HANDLE(egl_render), eglBuffer) != OMX_ErrorNone) {
                    lastError = "OMX_FillThisBuffer failed.";
                    status = -23;
                    break;
                }
            }

            if (!data_len) {
                //read error occured
                break;
            }

            buf->nFilledLen = data_len;
            buf->nOffset = 0;

            if (first_packet) {
                buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
                first_packet = false;
            } else {
                buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
            }

            if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone) {
                lastError = "could not empty buffer";
                status = -6;
                break;
            }
        }

        buf->nFilledLen = 0;
        buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;

        if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone) {
            lastError = "could not empty buffer (2)";
            status = -20;

            //need to flush the renderer to allow video_decode to disable its input port
            ilclient_flush_tunnels(tunnel, 0);

            ilclient_disable_port_buffers(video_decode, 130, NULL, NULL, NULL);
        }
    } else {
        //init failed
        texture->videoPlayerInitDone();
    }

    //debug
    if (DEBUG_OMX) {
        printf("OMX done status: %i\n", status);
    }

    //done
    destroyOmx();

    return status == 0;
}

/**
 * Destroy OMX.
 */
void AminoOmxVideoPlayer::destroyOmx() {
    if (file) {
        fclose(file);
        file = NULL;
    }

    if (!omxInitialized) {
        return;
    }

    ilclient_disable_tunnel(tunnel);
    ilclient_disable_tunnel(tunnel + 1);
    ilclient_disable_tunnel(tunnel + 2);
    ilclient_teardown_tunnels(tunnel);

    ilclient_state_transition(list, OMX_StateIdle);
    ilclient_state_transition(list, OMX_StateLoaded);

    ilclient_cleanup_components(list);

    OMX_Deinit();

    ilclient_destroy(client);

    omxInitialized = false;
}

void crashHandler(int sig) {
    void *array[10];
    size_t size;

    //process & thread
    pid_t pid = getpid();
    pid_t tid = gettid();
    uv_thread_t threadId = uv_thread_self();

    //get void*'s for all entries on the stack
    size = backtrace(array, 10);

    //print out all the frames to stderr
    fprintf(stderr, "Error: signal %d (process=%d, thread=%d, uvThread=%lu):\n", sig, pid, tid, (unsigned long)threadId);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// ========== Event Callbacks ===========

NAN_MODULE_INIT(InitAll) {
    //crash handler
    signal(SIGSEGV, crashHandler);

    //main class
    AminoGfxRPi::Init(target);

    //amino classes
    AminoGfx::InitClasses(target);
}

//entry point
NODE_MODULE(aminonative, InitAll)
