#include "images.h"
#include "base.h"

extern "C" {
    #include "nanojpeg.h"
    #include "upng.h"
}

#define DEBUG_IMAGES true
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
        v8::Local<v8::Object> buff = Nan::CopyBuffer(imgData, imgDataLen).ToLocalChecked();

        //create object
        Nan::Set(obj, Nan::New("w").ToLocalChecked(),      Nan::New(imgW));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(),      Nan::New(imgH));
        Nan::Set(obj, Nan::New("alpha").ToLocalChecked(),  Nan::New(imgAlpha));
        Nan::Set(obj, Nan::New("bpp").ToLocalChecked(),    Nan::New(imgBPP));
        Nan::Set(obj, Nan::New("buffer").ToLocalChecked(), buff);

        //free memory
        freeImageBuffers();

        //store local values
        AminoImage *img = Nan::ObjectWrap::Unwrap<AminoImage>(obj);

        img->imageLoaded(buff, imgW, imgH, imgAlpha, imgBPP);

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
 * Check if image is available.
 */
bool AminoImage::hasImage() {
    return w > 0;
}

/**
 * Free all resources.
 */
void AminoImage::destroy() {
    AminoJSObject::destroy();

    buffer.Reset();
}

/**
 * Create texture.
 *
 * Note: only call from async handler!
 */
GLuint AminoImage::createTexture(GLuint textureId) {
    if (!hasImage()) {
        return INVALID_TEXTURE;
    }

    //buffer
    v8::Local<v8::Object> bufferObj = Nan::New(buffer);
    char *bufferData = node::Buffer::Data(bufferObj);
    size_t bufferLength = node::Buffer::Length(bufferObj);

    //debug
    //printf("buffer length = %d\n", bufferLength);

    assert(w * h * bpp == (int)bufferLength);

    GLuint texture;

    if (textureId != INVALID_TEXTURE) {
        //use existing texture
	    texture = textureId;
    } else {
        //create new texture
	    glGenTextures(1, &texture);
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (bpp == 3) {
        //RGB (24-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, bufferData);
    } else if (bpp == 4) {
        //RGBA (32-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferData);
    } else if (bpp == 1) {
        //grayscale (8-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bufferData);
    } else if (bpp == 2) {
        //grayscale & alpha (16-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w, h, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, bufferData);
    } else {
        //unsupported
        printf("unsupported texture format: bpp=%d\n", bpp);
    }

    //linear scaling
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //clamp to border
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float color[] = { 0.0f, 0.0f, 0.0f, 0.0f };

    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);

    return texture;
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

/**
 * Create local copy of JS values.
 */
void AminoImage::imageLoaded(v8::Local<v8::Object> &buffer, int w, int h, bool alpha, int bpp) {
    this->buffer.Reset(buffer);
    this->w = w;
    this->h = h;
    this->alpha = alpha;
    this->bpp = bpp;
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

//
// AminoTexture
//

/**
 * Constructor.
 */
AminoTexture::AminoTexture(): AminoJSObject(getFactory()->name) {
    if (DEBUG_IMAGES) {
        printf("AminoTexture constructor\n");
    }
}

/**
 * Destructor.
 */
AminoTexture::~AminoTexture()  {
    if (DEBUG_IMAGES || DEBUG_RESOURCES) {
        printf("AminoTexture destructor\n");
    }
}

void AminoTexture::destroy() {
    AminoJSObject::destroy();

    if (callback) {
        delete callback;
        callback = NULL;
    }

    if (textureId != INVALID_TEXTURE && eventHandler) {
        ((AminoGfx *)eventHandler)->deleteTextureAsync(textureId);
    }
}

/**
 * Get factory instance.
 */
AminoTextureFactory* AminoTexture::getFactory() {
    static AminoTextureFactory *aminoTextureFactory;

    if (!aminoTextureFactory) {
        aminoTextureFactory = new AminoTextureFactory(New);
    }

    return aminoTextureFactory;
}

/**
 * Initialize Texture template.
 */
v8::Local<v8::Function> AminoTexture::GetInitFunction() {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

    //methods
    Nan::SetPrototypeMethod(tpl, "loadTexture", loadTexture);

    //template function
    return Nan::GetFunction(tpl).ToLocalChecked();
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoTexture::New) {
    AminoJSObject::createInstance(info, getFactory());
}

/**
 * Init amino binding.
 */
void AminoTexture::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    if (DEBUG_IMAGES) {
        printf("-> preInit()\n");
    }

    //set amino instance
    v8::Local<v8::Object> jsObj = info[0]->ToObject();
    AminoJSEventObject *obj = Nan::ObjectWrap::Unwrap<AminoJSEventObject>(jsObj);

    //bind to queue
    setEventHandler(obj);
    Nan::Set(handle(), Nan::New("amino").ToLocalChecked(), jsObj);
}

/**
 * Load texture asynchronously.
 *
 * loadTexte(img, callback)
 */
NAN_METHOD(AminoTexture::loadTexture) {
    if (DEBUG_IMAGES) {
        printf("-> loadTexture()\n");
    }

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());
    v8::Local<v8::Function> callback = info[1].As<v8::Function>();

    if (obj->callback || obj->textureId != INVALID_TEXTURE) {
        //already set
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("already loading") };

        callback->Call(info.This(), argc, argv);
        return;
    }

    //image
    AminoImage *img = Nan::ObjectWrap::Unwrap<AminoImage>(info[0]->ToObject());

    if (!img->hasImage()) {
        //missing image
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("image not loaded") };

        callback->Call(info.This(), argc, argv);
        return;
    }

    //async loading
    obj->callback = new Nan::Callback(callback);
    obj->enqueueValueUpdate(img, (asyncValueCallback)&AminoTexture::createTexture);
}

/**
 * Create texture.
 */
void AminoTexture::createTexture(AsyncValueUpdate *update) {
    if (DEBUG_IMAGES) {
        printf("-> createTexture()\n");
    }

    AminoImage *img = (AminoImage *)update->valueObj;
    GLuint textureId = img->createTexture(INVALID_TEXTURE);

    if (textureId == INVALID_TEXTURE) {
        //failed
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("could not create texture") };

        callback->Call(handle(), argc, argv);
        return;
    }

    //set values
    this->textureId = textureId;
    w = img->w;
    h = img->h;

    v8::Local<v8::Object> obj = handle();

    Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
    Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

    //callback
    if (callback) {
        int argc = 2;
        v8::Local<v8::Value> argv[2] = { Nan::Null(), obj };

        callback->Call(obj, argc, argv);
        delete callback;
        callback = NULL;
    }
}

//
//  AminoTextureFactory
//

/**
 * Create AminoTexture factory.
 */
AminoTextureFactory::AminoTextureFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("Texture", callback) {
    //empty
}

/**
 * Create AminoGfx instance.
 */
AminoJSObject* AminoTextureFactory::create() {
    return new AminoTexture();
}
