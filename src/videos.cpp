#include "videos.h"
#include "base.h"
#include "images.h"

#include <sstream>

#define DEBUG_VIDEO_FRAMES false
#define DEBUG_VIDEO_STREAM false

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
 * Get playback loop value.
 */
bool AminoVideo::getPlaybackLoop(int &loop) {
    Nan::MaybeLocal<v8::Value> loopValue = Nan::Get(handle(), Nan::New<v8::String>("loop").ToLocalChecked());

    if (loopValue.IsEmpty()) {
        return false;
    }

    v8::Local<v8::Value> loopLocal = loopValue.ToLocalChecked();

    if (loopLocal->IsBoolean()) {
        if (loopLocal->BooleanValue()) {
            loop = -1;
        } else {
            loop = 0;
        }

        return true;
    } else if (loopLocal->IsInt32()) {
        loop = loopLocal->Int32Value();

        if (loop < 0) {
            loop = -1;
        }

        return true;
    }

    return false;
}

/**
 * Get local file name.
 *
 * Note: must be called on main thread!
 */
std::string AminoVideo::getPlaybackSource() {
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
 * Get playback options (FFmpeg/libav).
 *
 * Note: must be called on main thread!
 */
std::string AminoVideo::getPlaybackOptions() {
    Nan::MaybeLocal<v8::Value> optsValue = Nan::Get(handle(), Nan::New<v8::String>("opts").ToLocalChecked());

    if (optsValue.IsEmpty()) {
        return "";
    }

    v8::Local<v8::Value> optsLocal = optsValue.ToLocalChecked();

    if (optsLocal->IsString()) {
        //playback options
        return AminoJSObject::toString(optsLocal);
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
    //playback settings
    video->getPlaybackLoop(loop);

    //keep instance
    video->retain();
}

/**
 * Destructor.
 */
AminoVideoPlayer::~AminoVideoPlayer() {
    //destroy player instance
    destroyAminoVideoPlayer();
}

/**
 * Get amount of textures needed by player.
 */
int AminoVideoPlayer::getNeededTextures() {
    return 1;
}

/**
 * Destroy the video player.
 */
void AminoVideoPlayer::destroy() {
    if (destroyed) {
        return;
    }

    destroyed = true;

    destroyAminoVideoPlayer();

    //overwrite

    //end of destroy chain
}

/**
 * Destroy the video player.
 */
void AminoVideoPlayer::destroyAminoVideoPlayer() {
    if (video) {
        video->release();
        video = NULL;
    }
}

/**
 * Check if player is ready.
 */
bool AminoVideoPlayer::isReady() {
    return ready;
}

/**
 * Check if playing.
 */
bool AminoVideoPlayer::isPlaying() {
    return playing;
}

/**
 * Check if paused.
 */
bool AminoVideoPlayer::isPaused() {
    return paused;
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
 * Get player state.
 */
std::string AminoVideoPlayer::getState() {
    if (!initDone) {
        return "loading";
    }

    if (failed) {
        return "error";
    } else if (playing) {
        return "playing";
    } else if (paused) {
        return "paused";
    }

    return "stopped";
}

/**
 * Playback ended.
 */
void AminoVideoPlayer::handlePlaybackDone() {
    if (destroyed) {
        return;
    }

    if (playing || paused) {
        playing = false;
        paused = false;

        if (DEBUG_VIDEOS) {
            printf("video: playback done\n");
        }

        if (!failed) {
            fireEvent("ended");
        }
    }
}

/**
 * Playback failed.
 */
void AminoVideoPlayer::handlePlaybackError() {
    if (destroyed) {
        return;
    }

    //set state
    if ((playing || paused) && !failed) {
        failed = true;

        if (DEBUG_VIDEOS) {
            printf("video: playback error\n");
        }

        fireEvent("error");
    }

    //done
    handlePlaybackDone();
}

/**
 * Playback was stopped.
 */
void AminoVideoPlayer::handlePlaybackStopped() {
    if (destroyed) {
        return;
    }

    //send event
    if (playing || paused) {
        if (DEBUG_VIDEOS) {
            printf("video: playback stopped\n");
        }

        //Note: not part of HTML5
        fireEvent("stop");
    }

    //done
    handlePlaybackDone();
}

/**
 * Playback was paused.
 */
void AminoVideoPlayer::handlePlaybackPaused() {
    if (destroyed) {
        return;
    }

    if (playing) {
        playing = false;
        paused = true;

        if (DEBUG_VIDEOS) {
            printf("video: playback paused\n");
        }

        fireEvent("pause");
    }
}

/**
 * Playback was resumed.
 */
void AminoVideoPlayer::handlePlaybackResumed() {
    if (destroyed) {
        return;
    }

    if (paused) {
        paused = false;
        playing = true;

        if (DEBUG_VIDEOS) {
            printf("video: playback resumed\n");
        }

        fireEvent("play");
    }
}

/**
 * Video is ready for playback.
 */
void AminoVideoPlayer::handleInitDone(bool ready) {
    if (destroyed || initDone) {
        return;
    }

    initDone = true;
    this->ready = ready;

    if (DEBUG_VIDEOS) {
        printf("video: init done (ready: %s)\n", ready ? "true":"false");
    }

    assert(texture);

    //set state
    playing = ready;
    paused = false;

    //send event

    //texture is ready
    texture->videoPlayerInitDone();

    if (ready) {
        //metadata available
        fireEvent("loadedmetadata");

        //playing
        fireEvent("playing");
    } else {
        //failed
        failed = true;
        fireEvent("error");
    }
}

/**
 * Video was rewinded.
 */
void AminoVideoPlayer::handleRewind() {
    fireEvent("rewind");
}

/**
 * Fire video player event.
 */
void AminoVideoPlayer::fireEvent(std::string event) {
    texture->fireVideoEvent(event);
}

//
// VideoDemuxer
//

/**
 * See http://dranger.com/ffmpeg/tutorial01.html.
 */
VideoDemuxer::VideoDemuxer() {
    //empty
}

VideoDemuxer::~VideoDemuxer() {
    //free resources
    close(true);
}

/**
 * Initialize the demuxer.
 */
bool VideoDemuxer::init() {
    //register all codecs and formats
    av_register_all();
    avcodec_register_all();

    //support network calls
    avformat_network_init();

    //log level
    if (DEBUG_VIDEOS) {
        av_log_set_level(AV_LOG_INFO);
    } else {
        av_log_set_level(AV_LOG_QUIET);
    }

    if (DEBUG_VIDEOS) {
        printf("using libav %u.%u.%u\n", LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
    }

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/**
 * Timeout callback. Returns true if timeout occured.
 */
static int interruptCallback(void *opaque) {
    VideoDemuxer *demuxer = static_cast<VideoDemuxer *>(opaque);

    if (demuxer->isTimeout()) {
        if (DEBUG_VIDEOS) {
            printf("-> timeout occured\n");
        }

        return 1;
    }

    return 0;
}

/**
 * Load a video from a file.
 */
bool VideoDemuxer::loadFile(std::string filename, std::string options) {
    //close previous instances
    close(false);

    this->filename = filename;
    this->options = options;

    if (DEBUG_VIDEOS) {
        printf("loading video: %s\n", filename.c_str());
    }

    //init
    const char *file = filename.c_str();

    context = avformat_alloc_context();

    if (!context) {
        lastError = "could not allocate context";
        close(false);

        return false;
    }

    //register timeout handler
    context->interrupt_callback.callback = interruptCallback;
    context->interrupt_callback.opaque = this;

    //test: disable UDP re-ordering
    //context->max_delay = 0;

    //options
    AVDictionary *opts = NULL;

    //av_dict_set(&opts, "rtsp_transport", "tcp", 0); //TCP instead of UDP (must be supported by server)
    //av_dict_set(&opts, "user_agent", "AminoGfx", 0);

    if (!options.empty()) {
        av_dict_parse_string(&opts, options.c_str(), "=", " ", 0);

        if (DEBUG_VIDEOS) {
            //show dictionary
            AVDictionaryEntry *t = NULL;

            printf("Options:\n");

            while ((t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX))) {
                printf(" %s: %s\n", t->key, t->value);
            }
        }
    }

    //check realtime
    AVDictionaryEntry *entry = av_dict_get(opts, "amino_realtime", NULL, AV_DICT_MATCH_CASE);

    if (entry) {
        //use setting
        realtime = strcmp(entry->value, "0") != 0 && strcmp(entry->value, "false") != 0;
    } else {
        //check RTSP
        realtime = filename.find("rtsp://") == 0;
    }

    //timeout settings
    entry = av_dict_get(opts, "amino_timeout_open", NULL, AV_DICT_MATCH_CASE);

    if (entry) {
        timeoutOpen = std::stoi(entry->value);
    }

    entry = av_dict_get(opts, "amino_timeout_read", NULL, AV_DICT_MATCH_CASE);

    if (entry) {
        timeoutRead = std::stoi(entry->value);
    }

    //dump format setting
    bool dumpFormat = false;

    entry = av_dict_get(opts, "amino_dump_format", NULL, AV_DICT_MATCH_CASE);

    if (entry) {
        //any value accepted
        dumpFormat = true;

        //change log level
        av_log_set_level(AV_LOG_INFO);
    }

    //open
    int res;

    resetTimeout(timeoutOpen);
    res = avformat_open_input(&context, file, NULL, &opts);

    av_dict_free(&opts);

    if (res < 0) {
        std::stringstream stream;

        stream << "file open error (-0x" << std::hex << res << ")";
        lastError = stream.str();
        close(false);

        return false;
    }

    //find stream
    if (avformat_find_stream_info(context, NULL) < 0) {
        lastError = "could not find streams";
        close(false);

        return false;
    }

    //debug
    if (DEBUG_VIDEOS || dumpFormat) {
        //output video format details
        av_dump_format(context, 0, file, 0);
    }

    //get video stream
    videoStream = -1;

    for (unsigned int i = 0; i < context->nb_streams; i++) {
        //Note: warning on macOS (codecpar not available on RPi)
        if (context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            //found video stream
            videoStream = i;
            break;
        }
    }

    if (videoStream == -1) {
        lastError = "not a video";
        close(false);

        return false;
    }

    //calc duration
    stream = context->streams[videoStream];

    int64_t duration = stream->duration;

    durationSecs = duration * av_q2d(stream->time_base);

    if (durationSecs < 0) {
        durationSecs = -1;
    }

    //check H264
    codecCtx = stream->codec;

    //Note: not available in libav!
    /*
    if (codecCtx->framerate.num > 0 && codecCtx->framerate.den > 0) {
        fps = codecCtx->framerate.num / (float)codecCtx->framerate.den;
    }
    */

    if (codecCtx->time_base.num > 0 && codecCtx->time_base.den > 0) {
        fps = codecCtx->time_base.den / (float)codecCtx->time_base.num / codecCtx->ticks_per_frame;

        //debug
        //printf("framerate: %f\n", fps);
    }

    width = codecCtx->width;
    height = codecCtx->height;
    isH264 = codecCtx->codec_id == AV_CODEC_ID_H264;

    //debug
    if (DEBUG_VIDEOS) {
        printf("video found: duration=%i s, fps=%f, realtime=%s\n", (int)durationSecs, fps, realtime ? "true":"false");

        //Note: warning on macOS (codecpar not available on RPi)
        if (isH264) {
            printf(" -> H264\n");
        }
    }

    return true;
}

/**
 * Initialize decoder.
 */
bool VideoDemuxer::initStream() {
    if (!codecCtx) {
        return false;
    }

    AVCodec *codec = NULL;

    //find the decoder for the video stream
    codec = avcodec_find_decoder(codecCtx->codec_id);

    if (!codec) {
        lastError = "unsupported codec";
        return false;
    }

    //copy context
    AVCodecContext *codecCtxOrig = codecCtx;

    codecCtx = avcodec_alloc_context3(codec);
    codecCtxAlloc = true;

    //Note: deprecated warning on macOS
    if (avcodec_copy_context(codecCtx, codecCtxOrig) != 0) {
        lastError = "could not copy codec context";
        return false;
    }

    //open codec
    AVDictionary *opts = NULL;

    if (avcodec_open2(codecCtx, codec, &opts) < 0) {
        lastError = "could not open codec";
        return false;
    }

    if (DEBUG_VIDEOS) {
        //show dictionary
        AVDictionaryEntry *t = NULL;

        while ((t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX))) {
            printf(" -> %s: %s\n", t->key, t->value);
        }
    }

    av_dict_free(&opts);

    //debug
    if (DEBUG_VIDEOS) {
        printf(" -> stream initialized\n");
    }

    return true;
}

/**
 * Check if NALU start codes are used (or format is annex b).
 */
bool VideoDemuxer::hasH264NaluStartCodes() {
    if (!context || !codecCtx || !isH264) {
        return false;
    }

    //Note: avcC atom  starts with 0x1
    return codecCtx->extradata_size < 7 || !codecCtx->extradata || *codecCtx->extradata != 0x1;
}

/**
 * Read the optional video header.
 */
bool VideoDemuxer::getHeader(uint8_t **data, int *size) {
    if (!context || !codecCtx) {
        return false;
    }

    if (codecCtx->extradata && codecCtx->extradata_size > 0) {
        *data = codecCtx->extradata;
        *size = codecCtx->extradata_size;

        return true;
    }

    return false;
}

/**
 * Read a video packet.
 *
 * Note: freeFrame() has to be called after the packet is consumed.
 */
READ_FRAME_RESULT VideoDemuxer::readFrame(AVPacket *packet) {
    if (!context || !codecCtx) {
        return READ_ERROR;
    }

    while (true) {
        //check state
        if (DEBUG_VIDEOS && paused) {
            printf("-> readFrame() while paused!\n");
        }

        //read
        int status;

        resetTimeout(timeoutRead);
        status = av_read_frame(context, packet);

        //check end of video
        if (status == AVERROR_EOF) {
            if (DEBUG_VIDEOS) {
                printf("-> end of file\n");
            }

            //FIXME getting EOF on RPi (libav) if animated gif is played!

            return READ_END_OF_VIDEO;
        }

        //check error
        if (status < 0) {
            if (DEBUG_VIDEOS) {
                printf("-> read frame error: %i\n", status);
            }

            return READ_ERROR;
        }

        //is this a packet from the video stream?
        if (packet->stream_index == videoStream) {
            return READ_OK;
        }

        //process next frame
        av_free_packet(packet);
    }
}

/**
 * Get the presentation time (in seconds).
 */
double VideoDemuxer::getFramePts(AVPacket *packet) {
    if (!stream || !packet) {
        return 0;
    }

    if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
        return packet->pts * av_q2d(stream->time_base);
    }

    return 0;
}

/**
 * Free a video packet.
 */
void VideoDemuxer::freeFrame(AVPacket *packet) {
    //free the packet that was allocated by av_read_frame
    av_free_packet(packet);

    packet->data = NULL;
    packet->size = 0;
}

/**
 * Read a video frame in RGB format.
 */
 READ_FRAME_RESULT VideoDemuxer::readRGBFrame(double &time) {
    if (!context || !codecCtx) {
        return READ_ERROR;
    }

    //initialize
    if (!frame) {
        //allocate video frame
        frame = av_frame_alloc();

        //allocate an AVFrame structure
        frameRGB = av_frame_alloc();

        if (!frame || !frameRGB) {
            lastError = "could not allocate frame";
            return READ_ERROR;
        }

        if (!buffer) {
            //determine required buffer size and allocate buffer
            //Note: deprecated warning on macOS
            int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height);
            //int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height, 1);

            bufferSize = numBytes * sizeof(uint8_t);
            buffer = (uint8_t *)av_malloc(bufferSize);
            memset(buffer, 0, bufferSize);

            if (!bufferCurrent) {
                bufferCurrent = (uint8_t *)av_malloc(bufferSize);
            }
        }

        //fill buffer
        //Note: deprecated warning on macOS

        avpicture_fill((AVPicture *)frameRGB, buffer, AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height);
        //av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height, 1);

        switchRGBFrame();

        //initialize SWS context for software scaling
        sws_ctx = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt, codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
    }

    //read frame
    AVPacket packet;
    READ_FRAME_RESULT res;

    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    while (true) {
        res = readFrame(&packet);

        if (res == READ_OK) {
            //decode video frame
            int frameFinished;

            avcodec_decode_video2(codecCtx, frame, &frameFinished, &packet);

            //did we get a video frame?
            if (frameFinished) {
                //convert the image from its native format to RGB
                sws_scale(sws_ctx, (uint8_t const * const *)frame->data, frame->linesize, 0, codecCtx->height, frameRGB->data, frameRGB->linesize);

                //frameRGB is ready
                frameRGBCount++;

                //timing
                double pts;

                if (packet.dts != (int64_t)AV_NOPTS_VALUE) {
#ifdef MAC
                    pts = av_frame_get_best_effort_timestamp(frame) * av_q2d(stream->time_base);
#else
                    //fallback (currently not yet used)
                    if (frame->pts != (int64_t)AV_NOPTS_VALUE) {
                        pts = frame->pts * av_q2d(stream->time_base);
                    } else {
                        pts = 0;
                    }
#endif
                } else {
                    pts = 0;
                }

                if (pts != 0) {
                    //store last value
                    lastPts = pts;
                } else {
                    //use last value
                    pts = lastPts;

                    if (DEBUG_VIDEO_FRAMES) {
                        printf("-> using internal timer\n");
                    }
                }

                //calc next value
                double frameDelay = av_q2d(stream->codec->time_base);

                frameDelay += frame->repeat_pict * (frameDelay * .5); //support repeating frames
                lastPts += frameDelay;

                time = pts;

                //debug
                if (DEBUG_VIDEO_FRAMES) {
                    printf("frame read: time=%f s\n", pts);
                }

                goto done;
            }

            //next frame
            freeFrame(&packet);
            continue;
        }

done:
        freeFrame(&packet);
        break;
    }

    return res;
}

#pragma GCC diagnostic pop

/**
 * Switch active RGB frame.
 */
void VideoDemuxer::switchRGBFrame() {
    memcpy(bufferCurrent, buffer, bufferSize);
    bufferCount = frameRGBCount;
}

/**
 * Pause reading frames.
 */
void VideoDemuxer::pause() {
    if (paused) {
        return;
    }

    paused = true;

    if (context) {
        if (DEBUG_VIDEOS) {
            printf("pausing stream\n");
        }

        av_read_pause(context);
    }
}

/**
 * Resume reading frames.
 */
void VideoDemuxer::resume() {
    if (!paused) {
        return;
    }

    paused = false;

    if (context) {
        if (DEBUG_VIDEOS) {
            printf("resuming stream\n");
        }

        av_read_play(context);
    }
}

/**
 * Rewind playback (go back to first frame).
 */
bool VideoDemuxer::rewind() {
    //reload & verify
    size_t prevW = width;
    size_t prevH = height;

    if (!loadFile(filename, options) || prevW != width || prevH != height) {
        return false;
    }

    //prepare stream
    return initStream();
}

/**
 * Rewind RGB stream.
 */
bool VideoDemuxer::rewindRGB(double &time) {
    if (!rewind()) {
        return false;
    }

    //load first frame
    return readRGBFrame(time) == READ_OK;
}

/**
 * Get frame data.
 */
uint8_t *VideoDemuxer::getFrameData(int &id) {
    id = bufferCount;

    return bufferCurrent;
}

/**
 * Close handlers.
 */
void VideoDemuxer::close(bool destroy) {
    if (context) {
        avformat_close_input(&context);
        context = NULL;
    }

    if (codecCtx && codecCtxAlloc) {
        avcodec_free_context(&codecCtx);
        codecCtx = NULL;
        codecCtxAlloc = false;
    }

    videoStream = -1;
    stream = NULL;

    durationSecs = -1;
    fps = -1;
    width = 0;
    height = 0;
    isH264 = false;

    //close read
    closeReadFrame(destroy);
}

/**
 * Free readFrame() resources.
 */
void VideoDemuxer::closeReadFrame(bool destroy) {
    if (frame) {
        av_frame_free(&frame);
        frame = NULL;
    }

    if (frameRGB) {
        av_frame_free(&frameRGB);
        frameRGB = NULL;
    }

    frameRGBCount = -1;
    lastPts = 0;

    if (buffer) {
        av_free(buffer);
        buffer = NULL;
    }

    //Note: kept until demuxer is destroyed
    if (destroy && bufferCurrent) {
        av_free(bufferCurrent);
        bufferCurrent = NULL;
    }

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = NULL;
    }
}

/**
 * Set timeout.
 */
void VideoDemuxer::resetTimeout(int timeoutMS) {
    timeout = getTime() + timeoutMS;
}

/**
 * Check if timeout occured.
 */
bool VideoDemuxer::isTimeout() {
    return getTime() > timeout;
}

/**
 * Get the last error.
 */
std::string VideoDemuxer::getLastError() {
    return lastError;
}

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

VideoFileStream::VideoFileStream(std::string filename, std::string options): AnyVideoStream(), filename(filename), options(options) {
    if (DEBUG_VIDEOS) {
        printf("create video file stream\n");
    }

    //empty
}

VideoFileStream::~VideoFileStream() {
    if (file) {
        fclose(file);
        file = NULL;
    }

    if (hasPacket) {
        demuxer->freeFrame(&packet);
        hasPacket = false;
    }

    if (demuxer) {
        delete demuxer;
        demuxer = NULL;
    }
}

/**
 * Open the file.
 */
bool VideoFileStream::init() {
    if (DEBUG_VIDEOS) {
        printf("-> init video file stream\n");
    }

    //check local H264 (direct file access)
    std::string postfix = ".h264";
    bool isLocalH264 = false;

    if (filename.compare(filename.length() - postfix.length(), postfix.length(), postfix) == 0 && filename.find("://") == std::string::npos) {
        isLocalH264 = true;
    }

    if (isLocalH264) {
        //local file
        file = fopen(filename.c_str(), "rb");

        if (!file) {
            lastError = "file not found";
            return false;
        }

        if (DEBUG_VIDEOS) {
            printf("-> local H264 file\n");
        }
    } else {
        //any stream

        //init demuxer
        demuxer = new VideoDemuxer();

        if (!demuxer->init() || !demuxer->loadFile(filename, options) || !demuxer->initStream()) {
            lastError = demuxer->getLastError();
            return false;
        }

        //init packet
        av_init_packet(&packet);
        packet.data = NULL;
        packet.size = 0;

        if (DEBUG_VIDEOS) {
            printf("-> video stream\n");
        }
    }

    return true;
}

/**
 * Rewind the file stream.
 */
bool VideoFileStream::rewind() {
    if (DEBUG_VIDEOS) {
        printf("-> rewind video file stream\n");
    }

    if (file) {
        //file
        std::rewind(file);
    } else if (demuxer) {
        //stream
        if (hasPacket) {
            demuxer->freeFrame(&packet);
            hasPacket = false;
        }

        eof = false;
        failed = false;
        headerRead = false;

        //rewind stream
        return demuxer->rewind();
    }

    return true;
}

/**
 * Read from the stream.
 */
unsigned int VideoFileStream::read(unsigned char *dest, unsigned int length, omx_metadata_t &omxData) {
    //reset OMX data
    omxData.flags = 0;
    omxData.timeStamp = 0;

    if (file) {
        //read block from file (Note: ferror() returns error state)
        return fread(dest, 1, length, file);
    } else if (demuxer) {
        //header
        if (!headerRead) {
            uint8_t *data;
            int size;

            if (!demuxer->getHeader(&data, &size)) {
                //no header available
                headerRead = true;
            }

            //fill
            unsigned int dataLeft = size - packetOffset;
            unsigned int dataLen = std::min(dataLeft, length);

            if (DEBUG_VIDEO_STREAM) {
                printf("-> writing header: %i\n", dataLen);
            }

            memcpy(dest, data + packetOffset, dataLen);

            //check packet
            if (dataLeft == dataLen) {
                //all consumed
                packetOffset = 0;
                headerRead = true;
                omxData.flags |= 0x80; //OMX_BUFFERFLAG_CODECCONFIG
            } else {
                //data left
                packetOffset += dataLen;
            }

            return dataLen;
        }

        //check existing packet
        unsigned int offset = 0;

        if (hasPacket) {
            unsigned int dataLeft = packet.size - packetOffset;
            unsigned int dataLen = std::min(dataLeft, length);

            if (DEBUG_VIDEO_STREAM) {
                printf("-> filling read buffer (prev packet): %i of %i\n", dataLen, length);
            }

            memcpy(dest, packet.data + packetOffset, dataLen);
            offset += dataLen;

            //check packet
            if (dataLeft == dataLen) {
                //all consumed
                demuxer->freeFrame(&packet);
                hasPacket = false;
                packetOffset = 0;
            } else {
                //data left
                packetOffset += dataLen;
                return offset;
            }

            //check buffer
            if (offset == length) {
                //buffer full
                return offset;
            }

            //read next packet
        }

        //check EOF
        if (eof || failed) {
            return offset;
        }

        //read new packet
        READ_FRAME_RESULT res = demuxer->readFrame(&packet);

        if (res == READ_END_OF_VIDEO) {
            eof = true;
            demuxer->freeFrame(&packet);

            return offset;
        }

        if (res != READ_OK) {
            failed = true;
            demuxer->freeFrame(&packet);

            return offset;
        }

        //fill buffer
        hasPacket = true;

        unsigned int dataLeft = length - offset;
        unsigned int dataLen = std::min(dataLeft, (unsigned int)packet.size);

        if (DEBUG_VIDEO_STREAM) {
            printf("-> filling read buffer (new packet): %i of %i\n", dataLen, dataLeft);
        }

        memcpy(dest + offset, packet.data, dataLen);
        offset += dataLen;

        //timing
        omxData.timeStamp = demuxer->getFramePts(&packet) * 1000000;

        //check state
        if (dataLen < (unsigned int)packet.size) {
            //data left
            packetOffset = dataLen;
        } else {
            //all consumed
            demuxer->freeFrame(&packet);
            hasPacket = false;
            omxData.flags |= 0x10; //OMX_BUFFERFLAG_ENDOFFRAME
        }

        return offset;
    }

    return 0;
}

/**
 * Pause reading.
 */
void VideoFileStream::pause() {
    if (demuxer) {
        demuxer->pause();
    }
}

/**
 * Resume reading.
 */
void VideoFileStream::resume() {
    if (demuxer) {
        demuxer->resume();
    }
}

/**
 * Check end of stream.
 */
bool VideoFileStream::endOfStream() {
    if (file) {
        return feof(file);
    } else if (demuxer) {
        return eof;
    }

    assert(false);

    return true;
}

/**
 * Check if H264 video stream.
 */
bool VideoFileStream::isH264() {
    return file || (demuxer && demuxer->isH264);
}

/**
 * Check if NALU start codes are used (or annex b).
 */
bool VideoFileStream::hasH264NaluStartCodes() {
    return demuxer && demuxer->hasH264NaluStartCodes();
}

/**
 * Get playback duration.
 */
double VideoFileStream::getDuration() {
    if (demuxer) {
        return demuxer->durationSecs;
    }

    return -1;
}

/**
 * Get the framerate (0 if unknown).
 */
double VideoFileStream::getFramerate() {
    if (demuxer) {
        return demuxer->fps;
    }

    return 0;
}

/**
 * Get demuxer instance.
 */
VideoDemuxer* VideoFileStream::getDemuxer() {
    return demuxer;
}