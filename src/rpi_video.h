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

    void setOmxSpeed(OMX_S32 speed);

private:
    void *eglImage = NULL;
    ILCLIENT_T *client = NULL;
    AnyVideoStream *stream = NULL;

    uv_thread_t thread;
    bool threadRunning = false;

    TUNNEL_T tunnel[4];
    COMPONENT_T *list[5];
    bool omxDestroyed = false;
    uv_mutex_t omxLock;
    bool doStop = false;

public:
    COMPONENT_T *egl_render = NULL;
    OMX_BUFFERHEADERTYPE *eglBuffer = NULL;
    bool bufferFilled = false;
    bool bufferError = false;
};

#endif