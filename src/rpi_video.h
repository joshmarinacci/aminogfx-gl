#ifndef _AMINO_RPI_VIDEO_H
#define _AMINO_RPI_VIDEO_H

#include "base.h"

extern "C" {
    #include "ilclient/ilclient.h"
    #include "EGL/eglext.h"
}

/**
 * OMX Video Player.
 */
class AminoOmxVideoPlayer : public AminoVideoPlayer {
public:
    AminoOmxVideoPlayer(AminoTexture *texture, AminoVideo *video);
    ~AminoOmxVideoPlayer();

    void init() override;
    bool initTexture();

    void destroy() override;
    void destroyAminoOmxVideoPlayer();

    //stream
    bool initStream() override;
    void closeStream();

    //OMX
    static void omxThread(void *arg);
    static void handleFillBufferDone(void *data, COMPONENT_T *comp);
    bool initOmx();
    int playOmx();
    void stopOmx();
    void destroyOmx();

    void initVideoTexture() override;
    void updateVideoTexture() override;
    static void textureThread(void *arg);
    bool useTexture();

    //metadata
    double getMediaTime() override;
    double getDuration() override;
    void stopPlayback() override;
    bool pausePlayback() override;
    bool resumePlayback() override;

    bool setOmxSpeed(OMX_S32 speed);

private:
    EGLImageKHR eglImage = EGL_NO_IMAGE_KHR;
    ILCLIENT_T *client = NULL;
    AnyVideoStream *stream = NULL;

    //cbx
    //uv_thread_t thread;
    VCOS_THREAD_T thread;
    bool threadRunning = false;

    TUNNEL_T tunnel[4];
    COMPONENT_T *list[5];
    bool omxDestroyed = false;
    bool doStop = false;
    bool doPause = false;
    uv_sem_t pauseSem;

public:
    COMPONENT_T *egl_render = NULL;
    OMX_BUFFERHEADERTYPE *eglBuffer = NULL;
    bool bufferFilled = false;
    bool bufferError = false;
};

#endif