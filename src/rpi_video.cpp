#include "rpi_video.h"
#include "rpi.h"

#include "bcm_host.h"
#include "interface/vchiq_arm/vchiq_if.h"

#include <linux/input.h>
#include <dirent.h>
#include <stdio.h>
#include <semaphore.h>

#include <sstream>

#define DEBUG_OMX false
#define DEBUG_OMX_READ false
#define DEBUG_OMX_BUFFER false
#define DEBUG_OMX_ERRORS true

#define OMX_EGL_BUFFERS 4

//
// AminoOmxVideoPlayer
//

AminoOmxVideoPlayer::AminoOmxVideoPlayer(AminoTexture *texture, AminoVideo *video): AminoVideoPlayer(texture, video) {
    //init memory buffers
    memset(list, 0, sizeof list);
    memset(tunnel, 0, sizeof tunnel);

    //semaphore
    int res = uv_sem_init(&pauseSem, 0);
    int res2 = uv_sem_init(&textureSem, 0);

    assert(res == 0);
    assert(res2 == 0);

    //egl
    eglImages = new EGLImageKHR[OMX_EGL_BUFFERS];
    eglBuffers = new OMX_BUFFERHEADERTYPE*[OMX_EGL_BUFFERS];

    for (int i = 0; i < OMX_EGL_BUFFERS; i++) {
        eglImages[i] = EGL_NO_IMAGE_KHR;
        eglBuffers[i] = NULL;

        if (i > 1) {
            textureNew.push(i);
        }
    }

    //lock
    uv_mutex_init(&bufferLock);
}

AminoOmxVideoPlayer::~AminoOmxVideoPlayer() {
    destroyAminoOmxVideoPlayer();

    //semaphore
    uv_sem_destroy(&pauseSem);
    uv_sem_destroy(&textureSem);

    //eglImages
    delete[] eglImages;
    delete[] eglBuffers;

    //lock
    uv_mutex_destroy(&bufferLock);
}

/**
 * Initialize the video player.
 */
void AminoOmxVideoPlayer::init() {
    //we are on the OpenGL thread

    if (DEBUG_OMX) {
        printf("-> init OMX video player\n");
    }

    //stream
    assert(stream);

    if (!stream->init()) {
        lastError = stream->getLastError();
        delete stream;
        stream = NULL;

        handleInitDone(false);

        return;
    }

    //check format
    if (!stream->isH264()) {
        lastError = "unsupported format";
        delete stream;
        stream = NULL;

        handleInitDone(false);

        return;
    }

    //create OMX thread
    threadRunning = true;

#ifdef USE_OMX_VCOS_THREAD
    VCOS_STATUS_T res = vcos_thread_create(&thread, "OMX thread", NULL, &omxThread, this);

    assert(res == VCOS_SUCCESS);
#else
    int res = uv_thread_create(&thread, omxThread, this);

    assert(res == 0);
#endif
}

/**
 * Init video stream (on main stream).
 */
bool AminoOmxVideoPlayer::initStream() {
    if (DEBUG_OMX) {
        printf("-> init stream\n");
    }

    assert(video);
    assert(!stream);

    stream = new VideoFileStream(video->getPlaybackSource(), video->getPlaybackOptions());

    return stream != NULL;
}

/**
 * Destroy placer.
 */
void AminoOmxVideoPlayer::destroy() {
    if (destroyed) {
        return;
    }

    //instance
    destroyAminoOmxVideoPlayer();

    //base class
    AminoVideoPlayer::destroy();
}

/**
 * Destroy OMX player instance.
 */
void AminoOmxVideoPlayer::destroyAminoOmxVideoPlayer() {
    //stop playback
    stopOmx();

    //free EGL texture
    AminoGfxRPi *gfx = static_cast<AminoGfxRPi *>(texture->getEventHandler());

    assert(gfx);

    for (int i = 0; i < OMX_EGL_BUFFERS; i++) {
        EGLImageKHR eglImage = eglImages[i];

        if (eglImage != EGL_NO_IMAGE_KHR) {
            gfx->destroyEGLImage(eglImage);
            eglImages[i] = EGL_NO_IMAGE_KHR;
        }
    }
}

/**
 * Close the stream.
 */
void AminoOmxVideoPlayer::closeStream() {
    if (stream) {
        delete stream;
        stream = NULL;
    }
}

/**
 * OMX thread.
 */
void* AminoOmxVideoPlayer::omxThread(void *arg) {
    AminoOmxVideoPlayer *player = static_cast<AminoOmxVideoPlayer *>(arg);

    assert(player);

    //init OMX
    player->initOmx();

    //close stream
    player->closeStream();

    //done
    player->threadRunning = false;

    return NULL;
}

/**
 * OMX buffer callback.
 *
 * Note: egl_render buffer output buffer was filled.
 */
void AminoOmxVideoPlayer::handleFillBufferDone(void *data, COMPONENT_T *comp) {
    AminoOmxVideoPlayer *player = static_cast<AminoOmxVideoPlayer *>(data);

    assert(player);

    if (DEBUG_OMX_BUFFER) {
        printf("OMX: handleFillBufferDone() %p\n", comp);
    }

    //check state
    if (player->omxDestroyed) {
        //Note: last call happens while destroying OMX instance!
        return;
    }

    //fill the next buffer (and write to texture)
    uv_mutex_lock(&player->bufferLock);

    //debug
    //printf("-> filled: %i\n", player->textureFilling);

    player->textureReady.push(player->textureFilling);

    if (!player->textureNew.empty()) {
        //use new
        player->textureFilling = player->textureNew.front();
        player->textureNew.pop();
    } else {
        //stop filling
        player->textureFilling = -1;

        //debug
        //printf("-> buffering paused\n");
    }

    uv_mutex_unlock(&player->bufferLock);

    player->omxFillNextEglBuffer();
}

/**
 * Get an OMX error as string.
 */
std::string AminoOmxVideoPlayer::getOmxError(OMX_S32 err) {
    switch (err) {
        case OMX_ErrorNone:
            //No error.
            return "OMX_ErrorNone";

        case OMX_ErrorInsufficientResources:
            //There were insufficient resources to perform the requested operation.
            return "OMX_ErrorInsufficientResources";

        case OMX_ErrorUndefined:
            //There was an error, but the cause of the error could not be determined.
            return "OMX_ErrorUndefined";

        case OMX_ErrorInvalidComponentName:
            //The component name string was not valid.
            return "OMX_ErrorInvalidComponentName";

        case OMX_ErrorComponentNotFound:
            //No component with the specified name string was found.
            return "OMX_ErrorComponentNotFound";

        case OMX_ErrorInvalidComponent:
            //The component specified did not have a "OMX_ComponentInit" or "OMX_ComponentDeInit entry point.
            return "OMX_ErrorInvalidComponent";

        case OMX_ErrorBadParameter:
            //One or more parameters were not valid.
            return "OMX_ErrorBadParameter";

        case OMX_ErrorNotImplemented:
            //The requested function is not implemented.
            return "OMX_ErrorNotImplemented";

        case OMX_ErrorUnderflow:
            //The buffer was emptied before the next buffer was ready.
            return "OMX_ErrorUnderflow";

        case OMX_ErrorOverflow:
            //The buffer was not available when it was needed.
            return "OMX_ErrorOverflow";

        case OMX_ErrorHardware:
            //The hardware failed to respond as expected.
            return "OMX_ErrorHardware";

        case OMX_ErrorInvalidState:
            //The component is in the state OMX_StateInvalid.
            return "OMX_ErrorInvalidState";

        case OMX_ErrorStreamCorrupt:
            //Stream is found to be corrupt.
            return "OMX_ErrorStreamCorrupt";

        case OMX_ErrorPortsNotCompatible:
            //Ports being connected are not compatible.
            return "OMX_ErrorPortsNotCompatible";

        case OMX_ErrorResourcesLost:
            //Resources allocated to an idle component have been lost resulting in the component returning to the loaded state.
            return "OMX_ErrorResourcesLost";

        case OMX_ErrorNoMore:
            //No more indicies can be enumerated.
            return "OMX_ErrorNoMore";

        case OMX_ErrorVersionMismatch:
            //The component detected a version mismatch.
            return "OMX_ErrorVersionMismatch";

        case OMX_ErrorNotReady:
            //The component is not ready to return data at this time.
            return "OMX_ErrorNotReady";

        case OMX_ErrorTimeout:
            //There was a timeout that occurred.
            return "OMX_ErrorTimeout";

        case OMX_ErrorSameState:
            //This error occurs when trying to transition into the state you are already in.
            return "OMX_ErrorSameState";

        case OMX_ErrorResourcesPreempted:
            //Resources allocated to an executing or paused component have been preempted, causing the component to return to the idle state.
            return "OMX_ErrorResourcesPreempted";

        case OMX_ErrorPortUnresponsiveDuringAllocation:
            //A non-supplier port sends this error to the IL client (via the EventHandler callback) during the allocation of buffers (on a transition from the LOADED to the IDLE state or on a port restart) when it deems that it has waited an unusually long time for the supplier to send it an allocated buffer via a UseBuffer call.
            return "OMX_ErrorPortUnresponsiveDuringAllocation";

        case OMX_ErrorPortUnresponsiveDuringDeallocation:
            //A non-supplier port sends this error to the IL client (via the EventHandler callback) during the deallocation of buffers (on a transition from the IDLE to LOADED state or on a port stop) when it deems that it has waited an unusually long time for the supplier to request the deallocation of a buffer header via a FreeBuffer call.
            return "OMX_ErrorPortUnresponsiveDuringDeallocation";

        case OMX_ErrorPortUnresponsiveDuringStop:
            //A supplier port sends this error to the IL client (via the EventHandler callback) during the stopping of a port (either on a transition from the IDLE to LOADED state or a port stop) when it deems that it has waited an unusually long time for the non-supplier to return a buffer via an EmptyThisBuffer or FillThisBuffer call.
            return "OMX_ErrorPortUnresponsiveDuringStop";

        case OMX_ErrorIncorrectStateTransition:
            //Attempting a state transtion that is not allowed.
            return "OMX_ErrorIncorrectStateTransition";

        case OMX_ErrorIncorrectStateOperation:
            //Attempting a command that is not allowed during the present state.
            return "OMX_ErrorIncorrectStateOperation";

        case OMX_ErrorUnsupportedSetting:
            //The values encapsulated in the parameter or config structure are not supported.
            return "OMX_ErrorUnsupportedSetting";

        case OMX_ErrorUnsupportedIndex:
            //The parameter or config indicated by the given index is not supported.
            return "OMX_ErrorUnsupportedIndex";

        case OMX_ErrorBadPortIndex:
            //The port index supplied is incorrect.
            return "OMX_ErrorBadPortIndex";

        case OMX_ErrorPortUnpopulated:
            //The port has lost one or more of its buffers and it thus unpopulated.
            return "OMX_ErrorPortUnpopulated";

        case OMX_ErrorComponentSuspended:
            //Component suspended due to temporary loss of resources.
            return "OMX_ErrorComponentSuspended";

        case OMX_ErrorDynamicResourcesUnavailable:
            //Component suspended due to an inability to acquire dynamic resources.
            return "OMX_ErrorDynamicResourcesUnavailable";

        case OMX_ErrorMbErrorsInFrame:
            //When the macroblock error reporting is enabled the component returns new error for every frame that has errors.
            return "OMX_ErrorMbErrorsInFrame";

        case OMX_ErrorFormatNotDetected:
            //A component reports this error when it cannot parse or determine the format of an input stream.
            return "OMX_ErrorFormatNotDetected";

        case OMX_ErrorContentPipeOpenFailed:
            //The content open operation failed.
            return "OMX_ErrorContentPipeOpenFailed";

        case OMX_ErrorContentPipeCreationFailed:
            //The content creation operation failed.
            return "OMX_ErrorContentPipeCreationFailed";

        case OMX_ErrorSeperateTablesUsed:
            //Separate table information is being used.
            return "OMX_ErrorSeperateTablesUsed";

        case OMX_ErrorTunnelingUnsupported:
            //Tunneling is unsupported by the component.
            return "OMX_ErrorTunnelingUnsupported";

        //OMX_ErrorKhronosExtensions 0x8F000000 (Reserved region for introducing Khronos Standard Extensions.)
        //OMX_ErrorVendorStartUnused 0x90000000 (Reserved region for introducing Vendor Extensions)

        case OMX_ErrorDiskFull:
            //Disk Full error.
            return "OMX_ErrorDiskFull";

        case OMX_ErrorMaxFileSize:
            //Max file size is reached.
            return "OMX_ErrorMaxFileSize";

        case OMX_ErrorDrmUnauthorised:
            //Unauthorised to play a DRM protected file.
            return "OMX_ErrorDrmUnauthorised";

        case OMX_ErrorDrmExpired:
            //The DRM protected file has expired.
            return "OMX_ErrorDrmExpired";

        case OMX_ErrorDrmGeneral:
            //Some other DRM library error.
            return "OMX_ErrorDrmGeneral";
    }

    //unknown error
    std::stringstream ss;

    ss << "unknown OMX error: 0x" << std::hex << err;

    return ss.str();
}

/**
 * IL client reported errors.
 */
void AminoOmxVideoPlayer::omxErrorHandler(void *userData, COMPONENT_T *comp, OMX_U32 data) {
    //see http://maemo.org/api_refs/5.0/beta/libomxil-bellagio/_o_m_x___core_8h.html

    //filter harmless errors
    OMX_S32 err = data;

    if (err == OMX_ErrorSameState) {
        return;
    }

    //output
    std::string error = getOmxError(err);

    fprintf(stderr, "OMX error: %s\n", error.c_str());
}

/**
 * Initialize OpenMax.
 */
bool AminoOmxVideoPlayer::initOmx() {
    int status = 0;
    COMPONENT_T *video_decode = NULL;
    COMPONENT_T *clock = NULL;
    COMPONENT_T *video_scheduler = NULL;

    if (DEBUG_OMX) {
        printf("-> init OMX\n");
    }

    //VCOS log statements (Note: VCOS_LOG_CATEGORY has to be defined first)
    //vcos_log_set_level(VCOS_LOG_CATEGORY, VCOS_LOG_TRACE);

    //init il client
    client = ilclient_init();

    if (!client) {
        lastError = "could not initialize ilclient";
        status = -1;
        goto end;
    }

    //set error handler
    if (DEBUG_OMX_ERRORS) {
        ilclient_set_error_callback(client, omxErrorHandler, this);
    }

    //set EOS callback
    //TODO check if call works as expected
    //ilclient_set_eos_callback(handle, omxEosHandler, NULL);

    //init OMX
    if (OMX_Init() != OMX_ErrorNone) {
        lastError = "could not initialize OMX";
        status = -2;
        goto end;
    }

    //buffer callback
    ilclient_set_fill_buffer_done_callback(client, handleFillBufferDone, this);

    //create video_decode (input buffer)
    if (ilclient_create_component(client, &video_decode, "video_decode", (ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS)) != 0) {
        lastError = "video_decode error";
        status = -10;
        goto end;
    }

    list[0] = video_decode;

    //create egl_render (output buffer)
    if (ilclient_create_component(client, &egl_render, "egl_render", (ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_OUTPUT_BUFFERS)) != 0) {
        lastError = "egl_render error";
        status = -11;
        goto end;
    }

    list[1] = egl_render;

    //create clock
    if (ilclient_create_component(client, &clock, "clock", (ILCLIENT_CREATE_FLAGS_T)ILCLIENT_DISABLE_ALL_PORTS) != 0) {
        lastError = "clock error";
        status = -12;
        goto end;
    }

    list[2] = clock;

    //config clock (waiting for start time)
    OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;

    memset(&cstate, 0, sizeof cstate);
    cstate.nSize = sizeof cstate;
    cstate.nVersion.nVersion = OMX_VERSION;
    cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
    cstate.nWaitMask = OMX_CLOCKPORT0;

    cstate.nOffset.nLowPart = -200 * 1000; //200 ms
    cstate.nOffset.nHighPart = 0;
    //cbx check -> we never set the start time value

    if (OMX_SetParameter(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone) {
        lastError = "could not set clock";
        status = -13;
        goto end;
    }

    //HDMI sync (Note: not well documented; FIXME cbx seeing no effect, probably not needed)
    OMX_CONFIG_LATENCYTARGETTYPE lt;

    memset(&lt, 0, sizeof lt);
    lt.nSize = sizeof lt;
    lt.nVersion.nVersion = OMX_VERSION;

    lt.nPortIndex = OMX_ALL;
    lt.bEnabled = OMX_TRUE;
    lt.nFilter = 10;
    lt.nTarget = 0;
    lt.nShift = 3;
    lt.nSpeedFactor = -60;
    lt.nInterFactor = 100;
    lt.nAdjCap = 100;

    /*
    lt.nPortIndex = OMX_ALL;
    lt.bEnabled = OMX_TRUE;
    lt.nFilter = 2;
    lt.nTarget = 4000;
    lt.nShift = 3;
    lt.nSpeedFactor = -135;
    lt.nInterFactor = 500;
    lt.nAdjCap = 20;
    */

    if (OMX_SetConfig(ILC_GET_HANDLE(clock), OMX_IndexConfigLatencyTarget, &lt) != OMX_ErrorNone) {
        lastError = "could not set clock latency";
        status = -14;
        goto end;
    }

    //create video_scheduler
    if (ilclient_create_component(client, &video_scheduler, "video_scheduler", (ILCLIENT_CREATE_FLAGS_T)ILCLIENT_DISABLE_ALL_PORTS) != 0) {
        lastError = "video_scheduler error";
        status = -15;
        goto end;
    }

    list[3] = video_scheduler;

    //set tunnels (source & sink ports)
    set_tunnel(tunnel, video_decode, 131, video_scheduler, 10);
    set_tunnel(tunnel + 1, video_scheduler, 11, egl_render, 220);
    set_tunnel(tunnel + 2, clock, 80, video_scheduler, 12);

    //setup clock tunnel first
    if (ilclient_setup_tunnel(tunnel + 2, 0, 0) != 0) {
        lastError = "tunnel setup error";
        status = -16;
        goto end;
    }

    //video reference clock
    OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE arct;

    memset(&arct, 0, sizeof arct);
    arct.nSize = sizeof arct;
    arct.nVersion.nVersion = OMX_VERSION;
    arct.eClock = OMX_TIME_RefClockVideo;

    if (OMX_SetConfig(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeActiveRefClock, &arct) != OMX_ErrorNone) {
        lastError = "could not set reference clock type";
        status = -160;
        goto end;
    }

    //set speed
    setOmxSpeed(1 << 16);

    //switch clock to executing state
    ilclient_change_component_state(clock, OMX_StateExecuting);

    //switch video decoder to idle state
    ilclient_change_component_state(video_decode, OMX_StateIdle);

    //video format
    OMX_VIDEO_PARAM_PORTFORMATTYPE format;

    memset(&format, 0, sizeof format);
    format.nSize = sizeof format;
    format.nVersion.nVersion = OMX_VERSION;
    format.nPortIndex = 130; //input
    format.eCompressionFormat = OMX_VIDEO_CodingAVC; //H264
    format.xFramerate = getOmxFramerate();

    /*
     * TODO more formats
     *
     *   - OMX_VIDEO_CodingMPEG4          non-H264 MP4 formats (H263, DivX, ...)
     *   - OMX_VIDEO_CodingMPEG2          needs license
     *   - OMX_VIDEO_CodingTheora         Theora
     */

    if (OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamVideoPortFormat, &format) != OMX_ErrorNone) {
        lastError = "could not set video format";
        status = -17;
        goto end;
    }

    //show video_decode buffer sizes
    /*
    printf("video_decode input buffers:\n");
    showOmxBufferInfo(video_decode, 130); //buffers=20 minBuffer=1 bufferSize=81920)

    printf("video_decode output buffers:\n");
    showOmxBufferInfo(video_decode, 131); //buffers=1 minBuffer=1 bufferSize=115200
    */

    //free extra buffers
    OMX_PARAM_U32TYPE eb;

    memset(&eb, 0, sizeof eb);
    eb.nSize = sizeof eb;
    eb.nVersion.nVersion = OMX_VERSION;
    eb.nU32 = 0;

    if (OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamBrcmExtraBuffers, &eb) != OMX_ErrorNone) {
        lastError = "could not set extra buffers";
        status = -172;
        goto end;
    }

    //create video decode input buffers
    if (ilclient_enable_port_buffers(video_decode, 130, NULL, NULL, NULL) != 0) {
        lastError = "video decode port error";
        status = -18;
        goto end;
    }

    //frame validation (see https://www.raspberrypi.org/forums/viewtopic.php?f=70&t=15983)
    OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE ec;

    memset(&ec, 0, sizeof ec);
    ec.nSize = sizeof ec;
    ec.nVersion.nVersion = OMX_VERSION;
    ec.bStartWithValidFrame = OMX_FALSE;

    if (OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamBrcmVideoDecodeErrorConcealment, &ec) != OMX_ErrorNone) {
        lastError = "error concealment type";
        status = -19;
        goto end;
    }

    //NALU (see https://www.khronos.org/registry/OpenMAX-IL/extensions/KHR/OpenMAX_IL_1_1_2_Extension%20NAL%20Unit%20Packaging.pdf)
    if (stream->hasH264NaluStartCodes()) {
        if (DEBUG_OMX) {
            printf("-> set OMX_NaluFormatStartCodes\n");
        }

        OMX_NALSTREAMFORMATTYPE nsft;

        memset(&nsft, 0, sizeof nsft);
        nsft.nSize = sizeof nsft;
        nsft.nVersion.nVersion = OMX_VERSION;
        nsft.nPortIndex = 130; //input
        nsft.eNaluFormat = OMX_NaluFormatStartCodes;

        if (OMX_SetParameter(ILC_GET_HANDLE(video_decode), (OMX_INDEXTYPE)OMX_IndexParamNalStreamFormatSelect, &nsft) != OMX_ErrorNone) {
            lastError = "NAL selection error";
            status = -20;
            goto end;
        }
    }

    //debug
    if (DEBUG_OMX) {
        printf("OMX init done\n");
    }

    //playback
    status = playOmx();

    //done
end:

    //debug
    if (DEBUG_OMX) {
        printf("OMX done status: %i\n", status);
    }

    //check status
    if (status != 0) {
        //add error code
        std::ostringstream ss;

        ss << lastError << " (" << status << ")";
        lastError = ss.str();

        //report error
        if (!initDone) {
            handleInitDone(false);
        } else {
            if (doStop) {
                handlePlaybackStopped();
            } else {
                handlePlaybackError();
            }
        }
    }

    destroyOmx();

    if (status == 0) {
        if (!initDone) {
            handleInitDone(true);
        }

        if (doStop) {
            handlePlaybackStopped();
        } else {
            handlePlaybackDone();
        }
    }

    return status == 0;
}

/**
 * OMX playback loop.
 */
int AminoOmxVideoPlayer::playOmx() {
    COMPONENT_T *video_decode = list[0];
    COMPONENT_T *video_scheduler = list[3];

    //start decoding
    OMX_BUFFERHEADERTYPE *buf;
    bool port_settings_changed = false;
    bool firstPacket = true;
    int res = 0;

    ilclient_change_component_state(video_decode, OMX_StateExecuting);

    /*
     * Works:
     *
     *   - Digoo M1Q
     *     - h264 (Main), yuv420p, 1280x960
     *   - RTSP Bigbuckbunny
     *     - h264 (Constrained Baseline), yuv420p, 320x180
     *     - Note: playback issues seen on RPi and Mac, other RTSP examples work fine
     *   - M4V
     *     - h264 (Constrained Baseline), yuv420p, 480x270
     *   - HTTPS
     *     - h264 (Main), yuv420p, 1920x1080
     *
     */

    //data loop (Note: never returns NULL)
    while ((buf = ilclient_get_input_buffer(video_decode, 130, 1)) != NULL) {
        //check stop
        if (doStop) {
            break;
        }

        //check pause (prevent reading while paused; might not be called)
        if (doPause) {
            if (DEBUG_OMX) {
                printf("pausing OMX thread\n");
            }

            //wait
            doPause = false; //signal we are waiting
            uv_sem_wait(&pauseSem);

            if (doStop) {
                break;
            }
        }

        //feed data and wait until we get port settings changed
        unsigned char *dest = buf->pBuffer;

        //read from file
        omx_metadata_t omxData;
        unsigned int data_len = stream->read(dest, buf->nAllocLen, omxData);

        //check end
        if (data_len == 0 && stream->endOfStream()) {
            //check if stream contained video data
            if (!ready) {
                //case: no video in stream
                lastError = "stream without valid video data";
                res = -30;
                break;
            }

            //loop
            if (loop > 0) {
                loop--;
            }

            if (loop == 0) {
                //end playback
                if (DEBUG_OMX) {
                    printf("OMX: end playback (EOS)\n");
                }

                break;
            }

            if (DEBUG_OMX) {
                printf("OMX: rewind stream\n");
            }

            if (!stream->rewind()) {
                //could not rewind -> end playback
                if (DEBUG_OMX) {
                    printf("-> rewind failed!\n");
                }

                break;
            }

            handleRewind();

            //read next block
            data_len = stream->read(dest, buf->nAllocLen, omxData);
        }

        if (DEBUG_OMX_READ) {
            printf("OMX: data read %i\n", (int)data_len);
        }

        //handle decoder output port settings changes
        if (!port_settings_changed &&
            ((data_len > 0 && ilclient_remove_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1) == 0) ||
            (data_len == 0 && ilclient_wait_for_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1, ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000) == 0))) {
            //process once
            port_settings_changed = true;

            if (DEBUG_OMX) {
                printf("OMX: egl_render setup\n");
            }

            //setup video_scheduler tunnel
            if (ilclient_setup_tunnel(tunnel, 0, 0) != 0) {
                lastError = "video tunnel setup error";
                res = -31;
                break;
            }

            //debug video_scheduler buffers
            /*
            printf("video_scheduler buffers:\n");
            showOmxBufferInfo(video_scheduler, 10); //buffers=0 minBuffer=0 bufferSize=3133440
            showOmxBufferInfo(video_scheduler, 11); //buffers=1 minBuffer=1 bufferSize=3133440
            showOmxBufferInfo(video_scheduler, 12); //buffers=1 minBuffer=1 bufferSize=48
            */

            //start scheduler
            ilclient_change_component_state(video_scheduler, OMX_StateExecuting);

            //now setup tunnel to egl_render
            if (ilclient_setup_tunnel(tunnel + 1, 0, 1000) != 0) {
                lastError = "egl_render tunnel setup error";
                res = -32;
                break;
            }

            //set egl_render to idle
            ilclient_change_component_state(egl_render, OMX_StateIdle);

            //get video size
            OMX_PARAM_PORTDEFINITIONTYPE portdef;

            memset(&portdef, 0, sizeof portdef);
            portdef.nSize = sizeof portdef;
            portdef.nVersion.nVersion = OMX_VERSION;
            portdef.nPortIndex = 131; //output buffer

            if (OMX_GetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamPortDefinition, &portdef) != OMX_ErrorNone) {
                lastError = "could not get video size";
                res = -33;
                break;
            }

            videoW = portdef.format.video.nFrameWidth;
            videoH = portdef.format.video.nFrameHeight;

            if (DEBUG_OMX) {
                //Note: buffer count not set.
                double fps = portdef.format.video.xFramerate / (float)(1 << 16);

                printf("video: %dx%d@%.2f bitrate=%i minBuffers=%i buffer=%i bufferSize=%i\n", videoW, videoH, fps, (int)portdef.format.video.nBitrate, portdef.nBufferCountMin, portdef.nBufferCountActual, portdef.nBufferSize);
            }

            //show egl_render buffer sizes
            /*
            printf("egl_render input buffers:\n"); //buffers=0 minBuffer=0 bufferSize=3133440
            showOmxBufferInfo(egl_render, 220);

            printf("egl_render output buffers:\n"); //buffers=1 minBuffer=1 bufferSize=0
            showOmxBufferInfo(egl_render, 221);
            */

            //set egl render buffers (max 8; https://github.com/raspberrypi/firmware/issues/718)
            setOmxBufferCount(egl_render, 221, OMX_EGL_BUFFERS);

            //do not discard input buffers
            OMX_CONFIG_PORTBOOLEANTYPE dm;

            memset(&dm, 0, sizeof dm);
            dm.nSize = sizeof dm;
            dm.nVersion.nVersion = OMX_VERSION;
            dm.nPortIndex = 220; //input buffer
            dm.bEnabled = OMX_FALSE;

            if (OMX_SetParameter(ILC_GET_HANDLE(egl_render), OMX_IndexParamBrcmVideoEGLRenderDiscardMode, &dm) != OMX_ErrorNone) {
                lastError = "could not disable discard mode";
                res = -330;
                break;
            }

            //switch to renderer thread (switches to playing state)
            texture->initVideoTexture();

            //wait for texture
            uv_sem_wait(&textureSem);

            if (!eglImagesReady || doStop) {
                //failed to create texture
                handleInitDone(false);
                break;
            }

            //setup OMX texture
            if (!setupOmxTexture()) {
                res = -34;
                break;
            }

            //texture ready
            handleInitDone(true);
        }

        if (!data_len) {
            //read error occured
            lastError = "IO error";
            res = -40;
            break;
        }

        //empty buffer
        buf->nFilledLen = data_len;
        buf->nOffset = 0;
        buf->nFlags = omxData.flags;

        if (omxData.timeStamp) {
            //in microseconds
            buf->nTimeStamp.nLowPart = omxData.timeStamp;
            buf->nTimeStamp.nHighPart = omxData.timeStamp >> 32;
        }

        //debug (Note: in presentation time order)
        //printf("timestamp: %i %i\n", buf->nTimeStamp.nHighPart, buf->nTimeStamp.nLowPart);

        if (firstPacket && (omxData.flags & OMX_BUFFERFLAG_CODECCONFIG) != OMX_BUFFERFLAG_CODECCONFIG) {
            //first packet (contains start time)
            buf->nFlags |= OMX_BUFFERFLAG_STARTTIME;
            firstPacket = false;
        } else {
            //video packet
            if (omxData.timeStamp) {
                //Note: time is always in PTS (DTS not used)
                //buf->nFlags |= OMX_BUFFERFLAG_TIME_IS_DTS;
            } else {
                buf->nFlags |= OMX_BUFFERFLAG_TIME_UNKNOWN;
            }
        }

        if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone) {
            lastError = "could not empty buffer";
            res = -41;
            break;
        }
    }

    //end of feeding loop (end of stream case)

    //safety check
    assert(buf);

    //send end of stream
    buf->nFilledLen = 0;
    buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;

    if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone) {
        lastError = "could not empty buffer (2)";
        res = -50;
    }

    if (DEBUG_OMX) {
        printf("OMX: decoder EOS\n");
    }

    //wait for EOS from renderer

    //Note: the following code is not working, getting a timeout after 10 s!
    //ilclient_wait_for_event(egl_render, OMX_EventBufferFlag, 220, 0, OMX_BUFFERFLAG_EOS, 0, ILCLIENT_BUFFER_FLAG_EOS, 10000);

    // -> monitor buffer update (last image was successfully shown)
    while (bufferFilled && !doStop && res == 0) {
        //wait 100 ms (enough time to show the next frame)
        bufferFilled = false;
        usleep(100 * 1000);
    }

    if (DEBUG_OMX) {
        printf("OMX: renderer EOS\n");
    }

    //need to flush the renderer to allow video_decode to disable its input port
    if (DEBUG_OMX) {
        printf("OMX: flushing tunnels\n");
    }

    ilclient_flush_tunnels(tunnel, 0);

    return res;
}

/**
 * Display buffer details on given port of component.
 */
bool AminoOmxVideoPlayer::showOmxBufferInfo(COMPONENT_T *comp, int port) {
    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    memset(&portdef, 0, sizeof portdef);
    portdef.nSize = sizeof portdef;
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = port;

    if (OMX_GetParameter(ILC_GET_HANDLE(comp), OMX_IndexParamPortDefinition, &portdef) != OMX_ErrorNone) {
        printf("-> Could not get port definition!\n");
        return false;
    }

    //show
    printf("-> buffers=%i minBuffer=%i bufferSize=%i\n", portdef.nBufferCountActual, portdef.nBufferCountMin, portdef.nBufferSize);

    return true;
}

/**
 * Set the buffer count.
 */
bool AminoOmxVideoPlayer::setOmxBufferCount(COMPONENT_T *comp, int port, int count) {
    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    memset(&portdef, 0, sizeof portdef);
    portdef.nSize = sizeof portdef;
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = port;

    //get current settings
    if (OMX_GetParameter(ILC_GET_HANDLE(comp), OMX_IndexParamPortDefinition, &portdef) != OMX_ErrorNone) {
        printf("-> Could not get port definition!\n");
        return false;
    }

    //change values
    portdef.nPortIndex = port;
    portdef.nBufferCountActual = count;

    if (OMX_SetParameter(ILC_GET_HANDLE(comp), OMX_IndexParamPortDefinition, &portdef) != OMX_ErrorNone) {
        printf("-> Could not set port definition!\n");
        return false;
    }

    return true;
}

/**
 * Stops the OMX playback thread.
 */
void AminoOmxVideoPlayer::stopOmx() {
    if (!omxDestroyed) {
        if (DEBUG_OMX) {
            printf("stopping OMX\n");
        }

        doStop = true;

        if (paused && !doPause) {
            //resume thread
            uv_sem_post(&pauseSem);
            uv_sem_post(&textureSem);
        } else {
            doPause = false;
        }

        if (threadRunning) {
            if (DEBUG_OMX) {
                printf("waiting for OMX thread to end\n");
            }

#ifdef USE_OMX_VCOS_THREAD
            vcos_thread_join(&thread, NULL);
#else
            int res = uv_thread_join(&thread);

            assert(res == 0);
#endif
        }
    }
}


/**
 * Needed OpenGL textures.
 */
int AminoOmxVideoPlayer::getNeededTextures() {
    return OMX_EGL_BUFFERS;
}

/**
 * Init video texture on OpenGL thread.
 */
void AminoOmxVideoPlayer::initVideoTexture() {
    if (DEBUG_VIDEOS) {
        printf("video: init video texture\n");
    }

    if (!initTexture()) {
        //stop thread
        doStop = true;
        uv_sem_post(&textureSem);

        //signal error
        handleInitDone(false);
        return;
    }

    //ready
    eglImagesReady = true;
    uv_sem_post(&textureSem);
}

/**
 * Setup texture.
 */
bool AminoOmxVideoPlayer::setupOmxTexture() {
    //enable the output port and tell egl_render to use the texture as a buffer
    OMX_HANDLETYPE eglHandle = ILC_GET_HANDLE(egl_render);

    //Note: OMX_IndexConfigLatencyTarget not supported by egl_render (port 220)

    //ilclient_enable_port(egl_render, 221); THIS BLOCKS SO CAN'T BE USED
    if (OMX_SendCommand(eglHandle, OMX_CommandPortEnable, 221, NULL) != OMX_ErrorNone) {
        lastError = "OMX_CommandPortEnable failed.";
        return false;
    }

    for (int i = 0; i < OMX_EGL_BUFFERS; i++) {
        //Note: pAppPrivate not used
        if (OMX_UseEGLImage(eglHandle, &eglBuffers[i], 221, NULL, eglImages[i]) != OMX_ErrorNone) {
            lastError = "OMX_UseEGLImage failed.";
            return false;
        }

        if (!eglBuffers[i]) {
            lastError = "could not get buffer";
            return false;
        }
    }

    if (DEBUG_OMX) {
        printf("OMX: egl_render setup done\n");
    }

    //set egl_render to executing
    ilclient_change_component_state(egl_render, OMX_StateExecuting);

    if (DEBUG_OMX) {
        printf("OMX: executing\n");
    }

    //request egl_render to write data to the texture buffer
    if (!omxFillNextEglBuffer()) {
        lastError = "OMX_FillThisBuffer failed.";
        return false;
    }

    return true;
}

/**
 * Fill the next egl_render buffer.
 */
void AminoOmxVideoPlayer::omxFillNextEglBuffer() {
    if (textureFilling == -1) {
        return false;
    }

    OMX_BUFFERHEADERTYPE *eglBuffer = eglBuffers[textureFilling];

    eglBuffer->nFlags = 0;
    eglBuffer->nFilledLen = 0;

    if (OMX_FillThisBuffer(ilclient_get_handle(egl_render), eglBuffer) != OMX_ErrorNone) {
        bufferError = true;
        printf("OMX_FillThisBuffer failed in callback\n");

        return false;
    }

    bufferFilled = true;

    return true;
}

/**
 * Init texture (on rendering thread).
 */
bool AminoOmxVideoPlayer::initTexture() {
    texture->activeTexture = 0;

    for (int i = 0; i < OMX_EGL_BUFFERS; i++) {
        GLuint textureId = texture->textureIds[i];

        assert(textureId != INVALID_TEXTURE);

        glBindTexture(GL_TEXTURE_2D, textureId);

        //size (has to be equal to video dimension!)
        GLsizei textureW = videoW;
        GLsizei textureH = videoH;

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureW, textureH, 0,GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        //Note: no performance hit seen
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        /*
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        */

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        //create EGL Image
        AminoGfxRPi *gfx = static_cast<AminoGfxRPi *>(texture->getEventHandler());
        EGLImageKHR eglImage = gfx->createEGLImage(textureId);

        eglImages[i] = eglImage;

        if (eglImage == EGL_NO_IMAGE_KHR) {
            lastError = "eglCreateImageKHR failed";

            return false;
        }
    }

    return true;
}

/**
 * Update the video texture.
 */
void AminoOmxVideoPlayer::updateVideoTexture(GLContext *ctx) {
    if (paused || stopped) {
        return;
    }

    uv_mutex_lock(&bufferLock);

    bool newBuffer = false;

    if (!textureReady.empty()) {
        //new frame available

        //check media time
        int nextFrame = textureReady.front();
        OMX_BUFFERHEADERTYPE *eglBuffer = eglBuffers[nextFrame];
        int64_t timestamp = eglBuffer->nTimeStamp.nLowPart | ((int64_t)eglBuffer->nTimeStamp.nHighPart << 32);
        double timeSecs = timestamp / 1000000.f;
        double timeNowSys = getTime() / 1000;
        double playTime;

        if (timeStartSys == -1) {
            playTime = 0;
            timeStartSys = timeNowSys;
        } else {
            playTime = timeNowSys - timeStartSys;
        }

        if (playTime >= timeSecs) {
            //switch to next frame
            textureNew.push(textureActive);

            textureActive = nextFrame;
            textureReady.pop();

            //ctx->unbindTexture(); //cbx not necessary
            texture->activeTexture = textureActive;

            //update media time
            mediaTime = timeSecs;

            //check buffer state
            if (textureFilling == -1) {
                //fill next buffer
                textureFilling = textureNew.front();
                textureNew.pop();

                newBuffer = true;
            }

            //debug cbx
            printf("-> displaying: %i (pos: %f s)\n", textureActive, timeSecs);
        }
    } else {
        //no new frame

        //debug
        //printf("-> underflow\n");
    }

    uv_mutex_unlock(&bufferLock);

    if (newBuffer) {
        omxFillNextEglBuffer();
    }
}

/**
 * Destroy OMX.
 */
void AminoOmxVideoPlayer::destroyOmx() {
    if (DEBUG_OMX) {
        printf("OMX: destroying instance\n");
    }

    omxDestroyed = true;

    //disable tunnels
    ilclient_disable_tunnel(tunnel);
    ilclient_disable_tunnel(tunnel + 1);
    ilclient_disable_tunnel(tunnel + 2);

    if (DEBUG_OMX) {
        printf("-> tunnels disabled\n");
    }

    //free buffers
    COMPONENT_T *video_decode = list[0];

    ilclient_disable_port_buffers(video_decode, 130, NULL, NULL, NULL); //input

    if (DEBUG_OMX) {
        printf("-> buffers disabled\n");
    }

    //close tunnels
    ilclient_teardown_tunnels(tunnel);

    memset(tunnel, 0, sizeof tunnel);

    if (DEBUG_OMX) {
        printf("-> tunnels closed\n");
    }

    //idle state
    ilclient_state_transition(list, OMX_StateIdle);

    //Note: blocks forever!!! Anyway, we are not re-using this player instance.
    //ilclient_state_transition(list, OMX_StateLoaded);

    for (int i = 0; i < OMX_EGL_BUFFERS; i++) {
        if (eglBuffers[i]) {
            OMX_FreeBuffer(ILC_GET_HANDLE(egl_render), 221, eglBuffers[i]);
            eglBuffers[i] = NULL;
        }
    }

    egl_render = NULL;

    ilclient_cleanup_components(list);

    if (DEBUG_OMX) {
        printf("-> components closed\n");
    }

    //destroy OMX
    OMX_Deinit(); //Note: decreases instance counter

    if (client) {
        ilclient_destroy(client);
        client = NULL;
    }

    if (DEBUG_OMX) {
        printf("OMX: instance destroyed\n");
    }
}

/**
 * Get current media time.
 */
double AminoOmxVideoPlayer::getMediaTime() {
    if ((!playing && !paused) || !list) {
        return -1;
    }

    return mediaTime;

    /*
    COMPONENT_T *clock = list[2];
    OMX_TIME_CONFIG_TIMESTAMPTYPE ts;

    memset(&ts, 0, sizeof ts);
    ts.nSize = sizeof ts;
    ts.nVersion.nVersion = OMX_VERSION;
    ts.nPortIndex = OMX_ALL;

    if (OMX_GetConfig(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeCurrentMediaTime, &ts) != OMX_ErrorNone) {
        return -1;
    }

    //microseconds
    int64_t timestamp = ts.nTimestamp.nLowPart | ((int64_t)ts.nTimestamp.nHighPart << 32);

    return timestamp / 1000000.f;
    */
}

/**
 * Get video duration (-1 if unknown).
 */
double AminoOmxVideoPlayer::getDuration() {
    if (stream) {
        return stream->getDuration();
    }

    return -1;
}

/**
 * Get the framerate (0 if unknown).
 */
double AminoOmxVideoPlayer::getFramerate() {
    if (stream) {
        return stream->getFramerate();
    }

    return 0;
}

/**
 * Get OMX framerate.
 */
OMX_U32 AminoOmxVideoPlayer::getOmxFramerate() {
    double framerate = getFramerate();

    if (framerate > 0) {
        return framerate * (1 << 16);
    }

    //default: 25 fps
    return 25 * (1 << 16);
}

/**
 * Stop playback.
 */
void AminoOmxVideoPlayer::stopPlayback() {
    if (!playing && !paused) {
        return;
    }

    //stop (currently blocking implementation)
    stopOmx();
}

/**
 * Pause playback.
 */
bool AminoOmxVideoPlayer::pausePlayback() {
    if (!playing) {
        return true;
    }

    //pause OMX (stops reading)
    if (DEBUG_OMX) {
        printf("pausing OMX\n");
    }
//cbx FIXME pause timer
    doPause = true; //signal thread to pause

    if (!setOmxSpeed(0)) {
        return false;
    }

    if (DEBUG_OMX) {
        printf("paused OMX\n");
    }

    //pause stream
    stream->pause();

    if (DEBUG_OMX) {
        printf("paused stream\n");
    }

    //set state
    handlePlaybackPaused();

    return true;
}

/**
 * Resume (stopped) playback.
 */
bool AminoOmxVideoPlayer::resumePlayback() {
    if (!paused) {
        return true;
    }

    //resume thread
    if (!doPause) {
        uv_sem_post(&pauseSem);
    } else {
        doPause = false;
    }

    //resume stream
    stream->resume();

    //resume OMX
    if (DEBUG_OMX) {
        printf("resume OMX\n");
    }

    setOmxSpeed(1 << 16);

    //set state
    handlePlaybackResumed();

    return true;
}

/**
 * Set OMX playback speed.
 */
bool AminoOmxVideoPlayer::setOmxSpeed(OMX_S32 speed) {
    COMPONENT_T *clock = list[2];
    OMX_TIME_CONFIG_SCALETYPE st;

    memset(&st, 0, sizeof st);
    st.nSize = sizeof st;
    st.nVersion.nVersion = OMX_VERSION;
    st.xScale = speed;

    bool res = OMX_SetConfig(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeScale, &st) == OMX_ErrorNone;

    if (DEBUG_OMX && !res) {
        printf("-> could not set OMX time scale!\n");
    }

    return res;
}