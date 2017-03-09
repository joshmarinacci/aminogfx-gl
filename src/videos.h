#ifndef _AMINOVIDEOS_H
#define _AMINOVIDEOS_H

#include "base_js.h"
#include "gfx.h"

extern "C" {
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
}

//cbx FIXME
#define DEBUG_VIDEOS true

class AminoVideoFactory;
class AminoTexture;

/**
 * Amino Video Loader.
 *
 */
class AminoVideo : public AminoJSObject {
public:
    AminoVideo();
    ~AminoVideo();

    bool hasVideo();

    bool isLocalFile();
    std::string getLocalFile();

    //creation
    static AminoVideoFactory* getFactory();

    //init
    static NAN_MODULE_INIT(Init);

private:
    //JS constructor
    static NAN_METHOD(New);
};

/**
 * AminoImage class factory.
 */
class AminoVideoFactory : public AminoJSObjectFactory {
public:
    AminoVideoFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Amino Video Player.
 */
class AminoVideoPlayer {
public:
    AminoVideoPlayer(AminoTexture *texture, AminoVideo *video);
    virtual ~AminoVideoPlayer();

    virtual bool initStream() = 0; //called on main thread
    virtual void init() = 0; //called on OpenGL thread
    virtual void initVideoTexture() = 0;
    virtual void updateVideoTexture() = 0;
    virtual void destroy();
    void destroyAminoVideoPlayer();

    bool isReady();
    std::string getLastError();
    void getVideoDimension(int &w, int &h);

protected:
    AminoTexture *texture;
    AminoVideo *video;

    //state
    bool initDone = false;
    bool ready = false;
    bool playing = false;
    bool destroyed = false;
    std::string lastError;

    //settings
    int loop = -1;

    //video
    int videoW = 0;
    int videoH = 0;

    void handlePlaybackDone();
    void handlePlaybackError();

    void handleInitDone(bool ready);
};

enum READ_FRAME_RESULT {
    READ_OK = 0,
    READ_ERROR,
    READ_END_OF_VIDEO
};

/**
 * Demux a video container stream.
 */
class VideoDemuxer {
public:
    size_t width = 0;
    size_t height = 0;
    float fps = -1;
    float durationSecs = -1.f;
    bool isH264 = false;

    VideoDemuxer();
    virtual ~VideoDemuxer();

    bool init();
    bool loadFile(std::string filename);
    bool initStream();

    bool saveStream(std::string filename);
    READ_FRAME_RESULT readFrame(double &time);
    bool rewind(double &time);
    uint8_t *getFrameData(int &id);

    std::string getLastError();

private:
    std::string lastError;

    //context
    AVFormatContext *context = NULL;
    AVCodecContext *codecCtx = NULL;

    //stream info
    std::string filename;
    int videoStream = -1;
    AVStream *stream = NULL;

    //read
    AVFrame *frame = NULL;
    AVFrame *frameRGB = NULL;
    int frameRGBCount = -1;
    double lastPts = 0;
    uint8_t *buffer = NULL;
    struct SwsContext *sws_ctx = NULL;

    void close(bool destroy);
    void closeReadFrame(bool destroy);
};

#endif