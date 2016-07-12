#include "images.h"

extern "C" {
    #include "nanojpeg.h"
    #include "upng.h"
}

#define DEBUG_IMAGES false
#define DEBUG_IMAGES_CONSOLE true

//
// AsyncImageWorker
//

/**
 * Asynchronous image loader.
 */
class AsyncImageWorker : public Nan::AsyncWorker {
private:
    //input buffer
    char *buffer;
    size_t bufferLen;

    //image
    char *imgData;
    int imgDataLen;
    int imgW;
    int imgH;
    bool imgAlpha;
    int imgBPP;

    //temp buffers
    upng_t *upng = NULL;
    bool isJpeg = false;

public:
    AsyncImageWorker(Nan::Callback *callback, v8::Local<v8::Object> &obj, v8::Local<v8::Value> &bufferObj) : AsyncWorker(callback) {
        SaveToPersistent("object", obj);

        //process buffer
        SaveToPersistent("buffer", bufferObj);

        buffer = node::Buffer::Data(bufferObj);
        bufferLen = node::Buffer::Length(bufferObj);
    }

    /**
     * Async running code.
     */
    void Execute() {
        if (DEBUG_IMAGES) {
            printf("-> async image loading started\n");
        }

        //check image type

        // 1) PNG (header)
        bool isPng = bufferLen > 8 &&
            buffer[0] == (char)137 &&
            buffer[1] == (char)80 &&
            buffer[2] == (char)78 &&
            buffer[3] == (char)71 &&
            buffer[4] == (char)13 &&
            buffer[5] == (char)10 &&
            buffer[6] == (char)26 &&
            buffer[7] == (char)10;

        //decode image
        if (isPng) {
            decodePng();
        } else {
            decodeJpeg();
        }
    }

    /**
     * Decode PNG image.
     *
     * Note: this code is thread-safe.
     */
    void decodePng() {
        if (DEBUG_IMAGES) {
            printf("decodePng()\n");
        }
        upng = upng_new_from_bytes((const unsigned char *)buffer, bufferLen);

        if (upng == NULL) {
            SetErrorMessage("error decoding PNG file");

            //console
            if (DEBUG_IMAGES_CONSOLE) {
                printf("error decoding PNG file\n");
            }

            return;
        }

        upng_decode(upng);

        //    printf("width = %d %d\n",upng_get_width(upng), upng_get_height(upng));
        //    printf("bytes per pixel = %d\n",upng_get_pixelsize(upng));

        char *image = (char *)upng_get_buffer(upng);
        int lengthout = upng_get_size(upng);

        //    printf("length of uncompressed buffer = %d\n", lengthout);

        //metadata
        imgData = image;
        imgDataLen = lengthout;
        imgW = upng_get_width(upng);
        imgH = upng_get_height(upng);
        imgAlpha = upng_get_components(upng) == 4; //RGBA
        imgBPP = upng_get_bpp(upng) / 8;
    }

    /**
     * Decode JPEG image.
     *
     * Note: NOT thread-safe! Has to be called in single queue.
     */
    void decodeJpeg() {
        if (DEBUG_IMAGES) {
            printf("decodeJpeg()\n");
        }

        njInit();

        if (njDecode(buffer, bufferLen)) {
            SetErrorMessage("error decoding PNG file");

            //console
            if (DEBUG_IMAGES_CONSOLE) {
                printf("Error decoding the PNG file.\n");
            }

            njDone();

            return;
        }

        if (DEBUG_IMAGES) {
            printf("-> got an image %d %d\n", njGetWidth(), njGetHeight());
            printf("-> size = %d\n", njGetImageSize());
        }

        imgDataLen = njGetImageSize();
        imgData = (char *)njGetImage();

        imgW = njGetWidth();
        imgH = njGetHeight();
        imgAlpha = false;
        imgBPP = njIsColor() ? 1:3;

        isJpeg = true;
    }

    /**
     * Free all allocated buffers.
     */
    void freeImageBuffers() {
        //free image and buffers
        if (upng) {
            upng_free(upng);
        }

        if (isJpeg) {
            //free memory
            njDone();
        }
    }

    /**
     * Back in main thread with JS access.
     */
    void HandleOKCallback() {
        if (DEBUG_IMAGES) {
            printf("-> async image loading done\n");
        }

        //Note: scope already created (but needed in HandleErrorCallback())

        //result
        v8::Local<v8::Object> obj = GetFromPersistent("object")->ToObject();
        Nan::MaybeLocal<v8::Object> buff = Nan::CopyBuffer(imgData, imgDataLen);

        //create object
        Nan::Set(obj, Nan::New("w").ToLocalChecked(),      Nan::New(imgW));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(),      Nan::New(imgH));
        Nan::Set(obj, Nan::New("alpha").ToLocalChecked(),  Nan::New(imgAlpha));
        Nan::Set(obj, Nan::New("bpp").ToLocalChecked(),    Nan::New(imgBPP));
        Nan::Set(obj, Nan::New("buffer").ToLocalChecked(), buff.ToLocalChecked());

        //free memory
        freeImageBuffers();

        //call callback
        v8::Local<v8::Value> argv[] = { Nan::Null(), obj };

        callback->Call(2, argv);
    }
};

//
// AminoImage
//

/**
 * Constructor.
 */
AminoImage::AminoImage(): AminoJSObject(getFactory()->name) {
    if (DEBUG_IMAGES) {
        printf("AminoImage constructor\n");
    }
}

/**
 * Destructor.
 */
AminoImage::~AminoImage()  {
    if (DEBUG_IMAGES || DEBUG_RESOURCES) {
        printf("AminoImage destructor\n");
    }
}

/**
 * Get factory instance.
 */
AminoImageFactory* AminoImage::getFactory() {
    static AminoImageFactory *aminoImageFactory;

    if (!aminoImageFactory) {
        aminoImageFactory = new AminoImageFactory(New);
    }

    return aminoImageFactory;
}

/**
 * Add class template to module exports.
 */
NAN_MODULE_INIT(AminoImage::Init) {
    if (DEBUG_IMAGES) {
        printf("AminoImage init\n");
    }

    AminoImageFactory *factory = getFactory();
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(factory);

    //prototype methods
    Nan::SetPrototypeMethod(tpl, "loadImage", loadImage);

    //global template instance
    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoImage::New) {
    AminoJSObject::createInstance(info, getFactory());
}

/**
 * Load image asynchronously.
 */
NAN_METHOD(AminoImage::loadImage) {
    v8::Local<v8::Value> bufferObj = info[0];
    Nan::Callback *callback = new Nan::Callback(info[1].As<v8::Function>());
    v8::Local<v8::Object> obj = info.This();

    //async loading
    AsyncQueueWorker(new AsyncImageWorker(callback, obj, bufferObj));
}

//
//  AminoImageFactory
//

/**
 * Create AminoImage factory.
 */
AminoImageFactory::AminoImageFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoImage", callback) {
    //empty
}

/**
 * Create AminoGfx instance.
 */
AminoJSObject* AminoImageFactory::create() {
    return new AminoImage();
}