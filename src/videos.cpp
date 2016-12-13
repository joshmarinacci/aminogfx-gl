#include "videos.h"
#include "base.h"
#include "images.h"

extern "C" {
    #include "libavformat/avformat.h"
}

//
// AminoVideo
//

/**
 * Constructor.
 */
AminoVideo::AminoVideo(): AminoJSObject(getFactory()->name) {
    //empty
}

/**
 * Destructor.
 */
AminoVideo::~AminoVideo()  {
    //empty
}

/**
 * Check if video is available.
 *
 * Note: must be called on main thread.
 */
bool AminoVideo::hasVideo() {
    //check src
    return isLocalFile();
}

/**
 * Check if local file.
 *
 * Note: must be called on main thread!
 */
bool AminoVideo::isLocalFile() {
    std::string fileName = getLocalFile();

    return fileName.length() > 0;
}

/**
 * Get local file name.
 *
 * Note: must be called on main thread!
 */
std::string AminoVideo::getLocalFile() {
    Nan::MaybeLocal<v8::Value> srcValue = Nan::Get(handle(), Nan::New<v8::String>("src").ToLocalChecked());

    if (srcValue.IsEmpty()) {
        return "";
    }

    v8::Local<v8::Value> srcLocal = srcValue.ToLocalChecked();

    if (srcLocal->IsString()) {
        //file or URL
        return AminoJSObject::toString(srcLocal);
    }

    return "";
}

/**
 * Get factory instance.
 */
AminoVideoFactory* AminoVideo::getFactory() {
    static AminoVideoFactory *aminoVideoFactory = NULL;

    if (!aminoVideoFactory) {
        aminoVideoFactory = new AminoVideoFactory(New);
    }

    return aminoVideoFactory;
}

/**
 * Add class template to module exports.
 */
NAN_MODULE_INIT(AminoVideo::Init) {
    if (DEBUG_VIDEOS) {
        printf("AminoVideo init\n");
    }

    AminoVideoFactory *factory = getFactory();
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(factory);

    //prototype methods
    // none

    //global template instance
    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoVideo::New) {
    AminoJSObject::createInstance(info, getFactory());
}

//
//  AminoVideoFactory
//

/**
 * Create AminoVideo factory.
 */
AminoVideoFactory::AminoVideoFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoVideo", callback) {
    //empty
}

/**
 * Create AminoVideo instance.
 */
AminoJSObject* AminoVideoFactory::create() {
    return new AminoVideo();
}

//
//  AminoVideoPlayer
//

/**
 * Constructor.
 *
 * Note: has to be called on main thread!
 */
AminoVideoPlayer::AminoVideoPlayer(AminoTexture *texture, AminoVideo *video): texture(texture), video(video) {
    //check source
    if (!video->isLocalFile()) {
        lastError = "unknown source";
    }
}

/**
 * Destructor.
 */
AminoVideoPlayer::~AminoVideoPlayer() {
    //destroy player instance
    destroyAminoVideoPlayer();
}

/**
 * Destroy the video player.
 */
void AminoVideoPlayer::destroy() {
    destroyAminoVideoPlayer();

    //overwrite
}

/**
 * Destroy the video player.
 */
void AminoVideoPlayer::destroyAminoVideoPlayer() {
    //empty
}

/**
 * Check if player is ready.
 */
bool AminoVideoPlayer::isReady() {
    return ready;
}

/**
 * Get last error.
 */
std::string AminoVideoPlayer::getLastError() {
    return lastError;
}

/**
 * Get video dimensions.
 */
void AminoVideoPlayer::getVideoDimension(int &w, int &h) {
    w = videoW;
    h = videoH;
}

/**
 * Playback ended.
 */
void AminoVideoPlayer::handlePlaybackDone() {
    if (playing) {
        playing = false;

        if (DEBUG_VIDEOS) {
            printf("video: playback done\n");
        }

        //TODO send event
    }
}

/**
 * Playback failed.
 */
void AminoVideoPlayer::handlePlaybackError() {
    //stop
    handlePlaybackDone();

    //TODO send event
}

/**
 * Video is ready for playback.
 */
void AminoVideoPlayer::handleInitDone(bool ready) {
    if (initDone) {
        return;
    }

    initDone = true;
    this->ready = ready;

    if (DEBUG_VIDEOS) {
        printf("video: init done (ready: %s)\n", ready ? "true":"false");
    }

    assert(texture);

    texture->videoPlayerInitDone();
}

//
// VideoDemuxer
//

VideoDemuxer::VideoDemuxer() {
    //empty
}

VideoDemuxer::~VideoDemuxer() {
    //empty
}

/**
 * Initialize the demuxer.
 */
bool VideoDemuxer::init() {
    //register all codecs and formats
    av_register_all();

    return true;
}

/**
 * Load a video from a file.
 */
bool VideoDemuxer::loadFile(std::string filename) {
    const char *file = filename.c_str();
    AVFormatContext *context = avformat_alloc_context();

    if (avformat_open_input(&context, file, 0, NULL) != 0) {
        lastError = "file open error";
        avformat_close_input(&context);
        return false;
    }

    //find stream
    if (avformat_find_stream_info(context, NULL) < 0) {
        lastError = "could not find streams";
        avformat_close_input(&context);
        return false;
    }

    //debug
    if (DEBUG_VIDEOS) {
        av_dump_format(context, 0, file, 0);
    }

    //get video stream
    int videoStream = -1;

    for (unsigned int i = 0; i < context->nb_streams; i++) {
        if (context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }

    if (videoStream == -1) {
        lastError = "not a video";
        avformat_close_input(&context);
        return false;
    }

    //debug
    if (DEBUG_VIDEOS) {
        AVStream *stream = context->streams[videoStream];
        int64_t duration = stream->duration;

        printf("video found: duration=%i s\n", (int)duration * stream->time_base.num / stream->time_base.den);

        if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
            printf(" -> H264\n");
        }
    }

    //cbx

    avformat_close_input(&context);

    return true;
}

/**
 * Get the last error.
 */
std::string VideoDemuxer::getLastError() {
    return lastError;
}