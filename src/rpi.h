#ifndef _AMINO_RPI_H
#define _AMINO_RPI_H

#include "base.h"
#include "renderer.h"

extern "C" {
    #include "ilclient/ilclient.h"
    #include "EGL/eglext.h"
}

class AminoGfxRPiFactory : public AminoJSObjectFactory {
public:
    AminoGfxRPiFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

class AminoOmxVideoPlayer : public AminoVideoPlayer {
public:
    AminoOmxVideoPlayer(AminoTexture *texture, AminoVideo *video);

    void init() override;
    bool initTexture();

    static void omxThread(void *arg);
    static void handleFillBufferDone(void *data, COMPONENT_T *comp);
    bool initOmx();
    void destroyOmx();

private:
    void *eglImage = NULL;
    ILCLIENT_T *client = NULL;
    FILE *file = NULL;

    uv_thread_t thread;
    bool threadRunning = false;

    TUNNEL_T tunnel[4];
    COMPONENT_T *list[5];
    bool omxInitialized = false;

public:
    COMPONENT_T *egl_render = NULL;
    OMX_BUFFERHEADERTYPE *eglBuffer = NULL;
    bool bufferError = false;
};

#endif
