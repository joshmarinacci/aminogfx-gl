#include "videos.h"
#include "base.h"
#include "images.h"

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
 */
bool AminoVideo::isLocalFile() {
    std::string fileName = getLocalFile();

    return fileName.length() > 0;
}

/**
 * Get local file name.
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

AminoVideoPlayer::AminoVideoPlayer(AminoTexture *texture, AminoVideo *video): texture(texture), video(video) {
    //check source
    if (video->isLocalFile()) {
        fileName = video->getLocalFile();
    } else {
        lastError = "unknown source";
    }
}

AminoVideoPlayer::~AminoVideoPlayer() {
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
void destroyAminoVideoPlayer() {
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
