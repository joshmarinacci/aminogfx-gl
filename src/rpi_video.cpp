#include "rpi_video.h"
#include "rpi.h"

#include "bcm_host.h"
#include "interface/vchiq_arm/vchiq_if.h"

#include <linux/input.h>
#include <dirent.h>
#include <stdio.h>
#include <semaphore.h>

//cbx
#define DEBUG_OMX true
#define DEBUG_OMX_READ false
#define DEBUG_OMX_BUFFER false

//
// AnyVideoStream
//

AnyVideoStream::AnyVideoStream() {
    //empty
}

AnyVideoStream::~AnyVideoStream() {
    //empty
}

/**
 * Get last error.
 */
std::string AnyVideoStream::getLastError() {
    return lastError;
}

//
// VideoFileStream
//

VideoFileStream::VideoFileStream(std::string filename): filename(filename) {
    if (DEBUG_OMX) {
        printf("create video file stream\n");
    }

    //empty
}

VideoFileStream::~VideoFileStream() {
    if (file) {
        fclose(file);
        file = NULL;
    }
}

/**
 * Open the file.
 */
bool VideoFileStream::init() {
    if (DEBUG_OMX) {
        printf("-> init video file stream\n");
    }

    file = fopen(filename.c_str(), "rb");

    if (!file) {
        lastError = "file not found";
        return false;
    }

    return true;
}

/**
 * Rewind the file stream.
 */
bool VideoFileStream::rewind() {
    if (DEBUG_OMX) {
        printf("-> rewind video file stream\n");
    }

    std::rewind(file);

    return true;
}

/**
 * Read from the stream.
 */
unsigned int VideoFileStream::read(unsigned char *dest, unsigned int length) {
    return fread(dest, 1, length, file);
}

/**
 * Check end of stream.
 */
bool VideoFileStream::endOfStream() {
    return feof(file);
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

    if (DEBUG_OMX) {
        printf("-> init OMX video player\n");
    }

    //stream
    if (!initStream()) {
        handleInitDone(false);
        return;
    }

    //create OMX thread
    int res = uv_thread_create(&thread, omxThread, this);

    assert(res == 0);

    threadRunning = true;
}

/**
 * Init video stream.
 */
bool AminoOmxVideoPlayer::initStream() {
    if (DEBUG_OMX) {
        printf("-> init stream\n");
    }

    assert(video);
    assert(!stream);

    if (video->isLocalFile()) {
        //local file
        stream = new VideoFileStream(video->getLocalFile());
    } else {
        //error
        lastError = "unknown source";
    }

    if (stream) {
        if (!stream->init()) {
            lastError = stream->getLastError();
            delete stream;
            stream = NULL;
        }
    }

    return stream != NULL;
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
void AminoOmxVideoPlayer::omxThread(void *arg) {
    AminoOmxVideoPlayer *player = static_cast<AminoOmxVideoPlayer *>(arg);

    assert(player);

    //init OMX
    player->initOmx();

    //close stream
    player->closeStream();

    //done
    player->threadRunning = false;
}

/**
 * OMX buffer callback.
 */
void AminoOmxVideoPlayer::handleFillBufferDone(void *data, COMPONENT_T *comp) {
    AminoOmxVideoPlayer *player = static_cast<AminoOmxVideoPlayer *>(data);

    assert(player);

    if (DEBUG_OMX_BUFFER) {
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

    if (DEBUG_OMX) {
        printf("-> init OMX\n");
    }

    //init il client
    client = ilclient_init();

    if (!client) {
        lastError = "could not initialize ilclient";

        return false;
    }

    //init OMX
    if (OMX_Init() != OMX_ErrorNone) {
        ilclient_destroy(client);

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
        format.eCompressionFormat = OMX_VIDEO_CodingAVC; //H264

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

        //data loop
        while ((buf = ilclient_get_input_buffer(video_decode, 130, 1)) != NULL) {
            //feed data and wait until we get port settings changed
            unsigned char *dest = buf->pBuffer;

            //check end
            if (stream->endOfStream()) {
                //check if stream contains video
                if (!ready) {
                    //case: no video in stream
                    lastError = "stream without valid video data";
                    handleInitDone(false);
                    break;
                }

                //loop
                if (DEBUG_OMX) {
                    printf("OMX: rewind stream\n");
                }

                if (!loop) {
                    handlePlaybackDone();
                    break;
                }

                if (!stream->rewind()) {
                    break;
                }
            }

            //read from file
            unsigned int data_len = stream->read(dest, buf->nAllocLen);

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

                videoW = portdef.format.video.nFrameWidth;
                videoH = portdef.format.video.nFrameHeight;

                if (DEBUG_OMX) {
                    printf("video: %dx%d\n", videoW, videoH);
                }

                //switch to renderer thread
                texture->initVideoTexture();
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

        //check status
        if (status != 0) {
            //report error
            if (!initDone) {
                handleInitDone(false);
            } else {
                handlePlaybackError();
            }
        }
    } else {
        //init failed
        handleInitDone(false);
    }

    //debug
    if (DEBUG_OMX) {
        printf("OMX done status: %i\n", status);
    }

    //done
    destroyOmx();
    handlePlaybackDone();

    return status == 0;
}

/**
 * Init video texture on OpenGL thread.
 */
void AminoOmxVideoPlayer::initVideoTexture() {
    if (DEBUG_VIDEOS) {
        printf("video: init video texture\n");
    }

    if (!initTexture()) {
        handleInitDone(false);
        return;
    }

    //ready

    //run on thread
    uv_thread_t thread;
    int res = uv_thread_create(&thread, textureThread, this);

    assert(res == 0);
}

/**
 * Texture setup thread.
 */
void AminoOmxVideoPlayer::textureThread(void *arg) {
    AminoOmxVideoPlayer *player = static_cast<AminoOmxVideoPlayer *>(arg);

    assert(player);

    bool res = player->useTexture();

    player->handleInitDone(res);
}

/**
 * Setup texture.
 */
bool AminoOmxVideoPlayer::useTexture() {
    //Enable the output port and tell egl_render to use the texture as a buffer
    //ilclient_enable_port(egl_render, 221); THIS BLOCKS SO CAN'T BE USED
    if (OMX_SendCommand(ILC_GET_HANDLE(egl_render), OMX_CommandPortEnable, 221, NULL) != OMX_ErrorNone) {
        lastError = "OMX_CommandPortEnable failed.";
        return false;
    }

    if (OMX_UseEGLImage(ILC_GET_HANDLE(egl_render), &eglBuffer, 221, NULL, eglImage) != OMX_ErrorNone) {
        lastError = "OMX_UseEGLImage failed.";
        return false;
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
    if (OMX_FillThisBuffer(ILC_GET_HANDLE(egl_render), eglBuffer) != OMX_ErrorNone) {
        lastError = "OMX_FillThisBuffer failed.";
        return false;
    }

    return true;
}

/**
 * Init texture.
 */
bool AminoOmxVideoPlayer::initTexture() {
    glBindTexture(GL_TEXTURE_2D, texture->textureId);

    //size (has to be equal to video dimension!)
    GLsizei textureW = videoW;
    GLsizei textureH = videoH;

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
 * Destroy OMX.
 */
void AminoOmxVideoPlayer::destroyOmx() {
    if (stream) {
        delete stream;
        stream = NULL;
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