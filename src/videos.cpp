#include "videos.h"
#include "base.h"
#include "images.h"

#define DEBUG_VIDEO_FRAMES false

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

    //support network calls
    avformat_network_init();

    if (DEBUG_VIDEOS) {
        printf("using libav %u.%u.%u\n", LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
    }

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/**
 * Load a video from a file.
 */
bool VideoDemuxer::loadFile(std::string filename) {
    //close previous instances
    close(false);

    this->filename = filename;

    if (DEBUG_VIDEOS) {
        printf("loading video: %s\n", filename.c_str());
    }

    //init
    const char *file = filename.c_str();

    context = avformat_alloc_context();

    AVDictionary *opts = NULL;

    //options
    //av_dict_set(&opts, "rtsp_transport", "tcp", 0); //TCP instead of UDP

    int res = avformat_open_input(&context, file, NULL, &opts);

    av_dict_free(&opts);

    if (res != 0) {
        lastError = "file open error";
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
    if (DEBUG_VIDEOS) {
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

    /*
    //Note: not available in libav!
    if (codecCtx->framerate.num > 0 && codecCtx->framerate.den > 0) {
        fps = codecCtx->framerate.num / (float)codecCtx->framerate.den;
    }
    */

    if (codecCtx->time_base.num > 0 && codecCtx->time_base.den > 0) {
        fps = 1 / (codecCtx->time_base.num / (float)codecCtx->time_base.den);
    }

    width = codecCtx->width;
    height = codecCtx->height;
    isH264 = codecCtx->codec_id == AV_CODEC_ID_H264;

    //debug
    if (DEBUG_VIDEOS) {
        printf("video found: duration=%i s, fps=%f\n", (int)durationSecs, fps);

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
 * Save raw video stream to file (H264 only).
 *
 * See http://stackoverflow.com/questions/13290413/why-decoding-frames-from-avi-container-and-encode-them-to-h264-mp4-doesnt-work.
 */
bool VideoDemuxer::saveStream(std::string filename) {
    //check if ready
    if (!context || !codecCtx || !isH264) {
        return false;
    }

    //prepare
    AVOutputFormat *outFmt = av_guess_format("h264", NULL, NULL);
    int err;

    if (!outFmt) {
        return false;
    }

    /*
     * avformat_alloc_output_context2() is not supported by libav!
     *
     * FFMPEG implementation:
     *
     * https://www.ffmpeg.org/doxygen/3.0/mux_8c_source.html
     */

#ifndef USING_LIBAV
    AVFormatContext *outCtx = NULL;

    err = avformat_alloc_output_context2(&outCtx, outFmt, NULL, NULL);

    if (err < 0 || !outCtx) {
        return false;
    }
#else
    AVFormatContext *outCtx = avformat_alloc_context();

    if (!outCtx) {
        return false;
    }

    outCtx->oformat = outFmt;
#endif

    AVStream *outStrm = avformat_new_stream(outCtx, stream->codec->codec);
    bool res = false;

    //settings
    avcodec_copy_context(outStrm->codec, stream->codec);

    //prevent warning on macOS
    outStrm->time_base = stream->time_base;

    err = avio_open(&outCtx->pb, filename.c_str(), AVIO_FLAG_WRITE);

    if (err < 0) {
        goto done;
    }

#if (LIBAVFORMAT_VERSION_MAJOR == 53)
    AVFormatParameters params = { 0 };

    err = av_set_parameters(outCtx, &params);

    if (err < 0) {
        goto done;
    }

    av_write_header(outFmtCtx);
#else
    if (avformat_write_header(outCtx, NULL) < 0) {
        goto done;
    }
#endif

    while (true) {
        AVPacket packet;

        av_init_packet(&packet);

        err = AVERROR(EAGAIN);

        while (AVERROR(EAGAIN) == err) {
            err = av_read_frame(context, &packet);
        }

        if (err < 0) {
            if (AVERROR_EOF != err && AVERROR(EIO) != err) {
                goto done;
            } else {
                //end of file
                break;
            }
        }

        if (packet.stream_index == videoStream) {
            packet.stream_index = 0;
            packet.pos = -1;

            err = av_interleaved_write_frame(outCtx, &packet);

            if (err < 0) {
                goto done;
            }
        }

        av_free_packet(&packet);
    }

    av_write_trailer(outCtx);

    //done
    res = true;

    //debug
    //printf("-> wrote file\n");

done:
    if (!(outCtx->oformat->flags & AVFMT_NOFILE) && outCtx->pb) {
        avio_close(outCtx->pb);
    }

    avformat_free_context(outCtx);

    return res;
}

/**
 * Read a video frame.
 */
 READ_FRAME_RESULT VideoDemuxer::readFrame(double &time) {
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

            buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
        }

        //fill buffer
        //Note: deprecated warning on macOS

        avpicture_fill((AVPicture *)frameRGB, buffer, AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height);
        //av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height, 1);

        //initialize SWS context for software scaling
        sws_ctx = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt, codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
    }

    //read frames
    AVPacket packet;
    READ_FRAME_RESULT res = READ_OK;

    while (true) {
        int status = av_read_frame(context, &packet);

        //check end of video
        if (status == AVERROR_EOF) {
            res = READ_END_OF_VIDEO;
            goto done;
        }

        //check error
        if (status < 0) {
            res = READ_ERROR;
            goto done;
        }

        //is this a packet from the video stream?
        if (packet.stream_index == videoStream) {
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
#ifndef USING_LIBAV
                    pts = av_frame_get_best_effort_timestamp(frame) * av_q2d(stream->time_base);
#else
                    pts = frame->pts * av_q2d(stream->time_base);
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
        }

        //process next frame
        continue;

done:
        //free the packet that was allocated by av_read_frame
        av_free_packet(&packet);

        break;
    }

    return res;
}

#pragma GCC diagnostic pop

/**
 * Rewind playback (go back to first frame).
 */
bool VideoDemuxer::rewind(double &time) {
    //reload & verify
    size_t prevW = width;
    size_t prevH = height;

    if (!loadFile(filename) || prevW != width || prevH != height) {
        return false;
    }

    //load first frame
    return initStream() && readFrame(time) == READ_OK;

    //FIXME did not work
    /*
    if (!context) {
        return false;
    }

    return av_seek_frame(context, videoStream, 0, AVSEEK_FLAG_FRAME) >= 0;
    */
}

/**
 * Get frame data.
 */
uint8_t *VideoDemuxer::getFrameData(int &id) {
    id = frameRGBCount;

    return buffer; //same as frameRGB->data[0]
}

/**
 * Close handlers.
 */
void VideoDemuxer::close(bool destroy) {
    if (context) {
        avformat_close_input(&context);
        context = NULL;
    }

    if (codecCtx) {
        avcodec_free_context(&codecCtx);
        codecCtx = NULL;
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

    //Note: kept until demuxer is destroyed
    if (buffer && destroy) {
        av_free(buffer);
        buffer = NULL;
    }

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = NULL;
    }
}

/**
 * Get the last error.
 */
std::string VideoDemuxer::getLastError() {
    return lastError;
}