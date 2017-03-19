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
// AminoOmxVideoPlayer
//

AminoOmxVideoPlayer::AminoOmxVideoPlayer(AminoTexture *texture, AminoVideo *video): AminoVideoPlayer(texture, video) {
    //init memory buffers
    memset(list, 0, sizeof(list));
    memset(tunnel, 0, sizeof(tunnel));

    //lock
    uv_mutex_init(&omxLock);
}

AminoOmxVideoPlayer::~AminoOmxVideoPlayer() {
    destroyAminoOmxVideoPlayer();

    //lock
    uv_mutex_destroy(&omxLock);
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

    int res = uv_thread_create(&thread, omxThread, this);

    assert(res == 0);
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

    destroyed = true;

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
    if (eglImage) {
        AminoGfxRPi *gfx = static_cast<AminoGfxRPi *>(texture->getEventHandler());

        gfx->destroyEGLImage(eglImage);
        eglImage = NULL;
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

    //check state
    if (player->omxDestroyed) {
        //Note: last call happens while destroying OMX instance!
        return;
    }

    //write to texture buffer
    if (OMX_FillThisBuffer(ilclient_get_handle(player->egl_render), player->eglBuffer) != OMX_ErrorNone) {
        player->bufferError = true;
        printf("OMX_FillThisBuffer failed in callback\n");
    } else {
        player->bufferFilled = true;
    }
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

    //init il client
    client = ilclient_init();

    if (!client) {
        lastError = "could not initialize ilclient";
        status = -1;
        goto end;
    }

    //init OMX
    if (OMX_Init() != OMX_ErrorNone) {
        lastError = "could not initialize OMX";
        status = -2;
        goto end;
    }

    //buffer callback
    ilclient_set_fill_buffer_done_callback(client, handleFillBufferDone, this);

    //create video_decode
    if (ilclient_create_component(client, &video_decode, "video_decode", (ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS)) != 0) {
        lastError = "video_decode error";
        status = -10;
        goto end;
    }

    list[0] = video_decode;

    //create egl_render
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

    //config clock
    OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;

    memset(&cstate, 0, sizeof(cstate));
    cstate.nSize = sizeof(cstate);
    cstate.nVersion.nVersion = OMX_VERSION;
    cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
    cstate.nWaitMask = 1;

    if (OMX_SetParameter(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone) {
        lastError = "could not set clock";
        status = -13;
        goto end;
    }

    //create video_scheduler
    if (ilclient_create_component(client, &video_scheduler, "video_scheduler", (ILCLIENT_CREATE_FLAGS_T)ILCLIENT_DISABLE_ALL_PORTS) != 0) {
        lastError = "video_scheduler error";
        status = -14;
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
        status = -15;
        goto end;
    }

    //switch clock to executing state
    ilclient_change_component_state(clock, OMX_StateExecuting);

    //switch video decoder to idle state
    ilclient_change_component_state(video_decode, OMX_StateIdle);

    //video format
    OMX_VIDEO_PARAM_PORTFORMATTYPE format;

    memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
    format.nVersion.nVersion = OMX_VERSION;
    format.nPortIndex = 130;
    format.eCompressionFormat = OMX_VIDEO_CodingAVC; //H264
    //TODO xFramerate -> 25 * (1 << 16)

    /*
     * TODO more formats
     *
     *   - OMX_VIDEO_CodingMPEG4          non-H264 MP4 formats (H263, DivX, ...)
     *   - OMX_VIDEO_CodingMPEG2          needs license
     *   - OMX_VIDEO_CodingTheora         Theora
     */

    if (OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamVideoPortFormat, &format) != OMX_ErrorNone) {
        lastError = "could not set video format";
        status = -16;
        goto end;
    }

    //video decode
    if (ilclient_enable_port_buffers(video_decode, 130, NULL, NULL, NULL) != 0) {
        lastError = "video decode port error";
        status = -17;
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
        status = -18;
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
        nsft.nPortIndex = 130;
        nsft.eNaluFormat = OMX_NaluFormatStartCodes;

        if (OMX_SetParameter(ILC_GET_HANDLE(video_decode), (OMX_INDEXTYPE)OMX_IndexParamNalStreamFormatSelect, &nsft) != OMX_ErrorNone) {
            lastError = "NAL selection error";
            status = -19;
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
    bool first_packet = true;

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
            return 0;
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
                return -30;
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

        //handle port settings changes
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
                return -31;
            }

            //start scheduler
            ilclient_change_component_state(video_scheduler, OMX_StateExecuting);

            //now setup tunnel to egl_render
            if (ilclient_setup_tunnel(tunnel + 1, 0, 1000) != 0) {
                lastError = "egl_render tunnel setup error";
                return -32;
            }

            //set egl_render to idle
            ilclient_change_component_state(egl_render, OMX_StateIdle);

            //get video size
            OMX_PARAM_PORTDEFINITIONTYPE portdef;

            memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
            portdef.nVersion.nVersion = OMX_VERSION;
            portdef.nPortIndex = 131;

            if (OMX_GetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamPortDefinition, &portdef) != OMX_ErrorNone) {
                lastError = "could not get video size";
                return -33;
            }

            videoW = portdef.format.video.nFrameWidth;
            videoH = portdef.format.video.nFrameHeight;

            if (DEBUG_OMX) {
                float fps = portdef.format.video.xFramerate / (float)(1 << 16);

                printf("video: %dx%d@%.2f\n", videoW, videoH, fps);
            }

            //switch to renderer thread (switches to playing state)
            texture->initVideoTexture();
        }

        if (!data_len) {
            //read error occured
            lastError = "IO error";
            return -40;
        }

        buf->nFilledLen = data_len;
        buf->nOffset = 0;
        buf->nFlags = omxData.flags;

        if (omxData.timeStamp) {
            //in microseconds
            buf->nTimeStamp.nLowPart = omxData.timeStamp;
            buf->nTimeStamp.nHighPart = omxData.timeStamp >> 32;
        }

        if (first_packet && (omxData.flags & OMX_BUFFERFLAG_CODECCONFIG) != OMX_BUFFERFLAG_CODECCONFIG) {
            buf->nFlags |= OMX_BUFFERFLAG_STARTTIME;
            first_packet = false;
        } else {
            //TODO should we pass the timing information from FFmpeg/libav?
            buf->nFlags |= OMX_BUFFERFLAG_TIME_UNKNOWN;
        }

        if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone) {
            lastError = "could not empty buffer";
            return -41;
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
        return -50;
    }

    if (DEBUG_OMX) {
        printf("OMX: decoder EOS\n");
    }

    //wait for EOS from render

    //Note: the following code is not working, getting a timeout after 10 s!
    //ilclient_wait_for_event(egl_render, OMX_EventBufferFlag, 220, 0, OMX_BUFFERFLAG_EOS, 0, ILCLIENT_BUFFER_FLAG_EOS, 10000);

    // -> monitor buffer update
    while (bufferFilled && !omxDestroyed && !doStop) {
        //wait 100 ms (enough time to show the next frame)
        bufferFilled = false;
        usleep(100 * 1000);
    }

    if (DEBUG_OMX) {
        printf("OMX: renderer EOS\n");
    }

    //need to flush the renderer to allow video_decode to disable its input port
    ilclient_flush_tunnels(tunnel, 0);

    ilclient_disable_port_buffers(video_decode, 130, NULL, NULL, NULL);

    return 0;
}

/**
 * Stops the OMX playback thread.
 */
void AminoOmxVideoPlayer::stopOmx() {
    //cbx TODO check stop while paused
    if (!omxDestroyed) {
        if (DEBUG_OMX) {
            printf("stopping OMX\n");
        }

        doStop = true;

        if (threadRunning) {
            int res = uv_thread_join(&thread);

            assert(res == 0);
        }
    }
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

    //run on thread (do not block rendering thread)
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
 * Init texture (on rendering thread).
 */
bool AminoOmxVideoPlayer::initTexture() {
    glBindTexture(GL_TEXTURE_2D, texture->textureId);

    //size (has to be equal to video dimension!)
    GLsizei textureW = videoW;
    GLsizei textureH = videoH;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureW, textureH, 0,GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
 * Update the video texture.
 */
void AminoOmxVideoPlayer::updateVideoTexture() {
    //not needed
}

/**
 * Destroy OMX.
 */
void AminoOmxVideoPlayer::destroyOmx() {
    uv_mutex_lock(&omxLock);

    if (omxDestroyed) {
        uv_mutex_unlock(&omxLock);
        return;
    }

    omxDestroyed = true;

    if (DEBUG_OMX) {
        printf("OMX: destroying instance\n");
    }

    //close tunnels
    ilclient_disable_tunnel(tunnel);
    ilclient_disable_tunnel(tunnel + 1);
    ilclient_disable_tunnel(tunnel + 2);
    ilclient_teardown_tunnels(tunnel);

    memset(tunnel, 0, sizeof(tunnel));

    if (DEBUG_OMX) {
        printf("-> tunnels closed\n");
    }

    //list
    ilclient_state_transition(list, OMX_StateIdle);

    //Note: blocks forever!!! Anyway, we are not re-using this player instance.
    //ilclient_state_transition(list, OMX_StateLoaded);

    if (eglBuffer) {
        OMX_FreeBuffer(ILC_GET_HANDLE(egl_render), 221, eglBuffer);
        eglBuffer = NULL;
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

    uv_mutex_unlock(&omxLock);
}

/**
 * Get current media time.
 */
double AminoOmxVideoPlayer::getMediaTime() {
    if ((!playing && !paused) || !list) {
        return -1;
    }

    COMPONENT_T *clock = list[2];
    OMX_TIME_CONFIG_TIMESTAMPTYPE ts;

    memset(&ts, 0, sizeof(ts));
    ts.nSize = sizeof(ts);
    ts.nVersion.nVersion = OMX_VERSION;
    ts.nPortIndex = OMX_ALL;

    if (OMX_GetConfig(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeCurrentMediaTime, &ts) != OMX_ErrorNone) {
        return -1;
    }

    int64_t timestamp = ts.nTimestamp.nLowPart | ((int64_t)ts.nTimestamp.nHighPart << 32);

    return timestamp / 1000000.f;
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
 * Stop playback.
 */
void AminoOmxVideoPlayer::stopPlayback() {
    if (!playing && !paused) {
        return;
    }

    //stop
    //cbx TODO use thread
    stopOmx();
}

/**
 * Pause playback.
 */
bool AminoOmxVideoPlayer::pausePlayback() {
    if (!playing) {
        return true;
    }

    //pause OMX
    if (!setOmxSpeed(0)) {
        return false;
    }

    //pause stream
    stream->pause();

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

    //resume OMX
    setOmxSpeed(1 << 16);

    //resume stream
    stream->resume();

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

    memset(&st, 0, sizeof(st));
    st.nSize = sizeof(st);
    st.nVersion.nVersion = OMX_VERSION;
    st.xScale = 0;

    return OMX_SetConfig(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeScale, &st) == OMX_ErrorNone;
}