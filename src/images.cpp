#include "images.h"
#include "base.h"

#include <uv.h>

extern "C" {
    //#include "nanojpeg.h"
    //#include "upng.h"

    #include <jpeglib.h>

    #define PNG_SKIP_SETJMP_CHECK
    #include <png.h>
}

#define DEBUG_IMAGES false
#define DEBUG_IMAGES_CONSOLE true

//
// libjpeg error handler
//
// See https://github.com/ellzey/libjpeg/blob/master/example.c
//

struct myjpeg_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct myjpeg_error_mgr *myjpeg_error_ptr;

METHODDEF(void) myjpeg_error_exit(j_common_ptr cinfo) {
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  myjpeg_error_ptr myerr = (myjpeg_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message)(cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

//
// libpng handlers
//

typedef struct {
    const png_byte *data;
    const png_size_t size;
} DataHandle;

typedef struct {
    const DataHandle data;
    png_size_t offset;
} ReadDataHandle;

static void read_png_data_callback(png_structp png_ptr, png_byte *raw_data, png_size_t read_length) {
    ReadDataHandle *handle = (ReadDataHandle *)png_get_io_ptr(png_ptr);
    const png_byte *png_src = handle->data.data + handle->offset;

    memcpy(raw_data, png_src, read_length);
    handle->offset += read_length;
}

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
    char *imgData = NULL;
    int imgDataLen = 0;
    int imgW;
    int imgH;
    bool imgAlpha;
    int imgBPP;

    //temp buffers
    //upng_t *upng = NULL;
    bool isJpeg = false;

    //mutex
    static uv_mutex_t jpegMutex;
    static bool jpegMutexInitialized;

public:
    AsyncImageWorker(Nan::Callback *callback, v8::Local<v8::Object> &obj, v8::Local<v8::Value> &bufferObj) : AsyncWorker(callback) {
        SaveToPersistent("object", obj);

        //process buffer
        SaveToPersistent("buffer", bufferObj);

        buffer = node::Buffer::Data(bufferObj);
        bufferLen = node::Buffer::Length(bufferObj);

        //mutex
        if (!jpegMutexInitialized) {
            jpegMutexInitialized = true;

            int res = uv_mutex_init(&jpegMutex);

            assert(res == 0);
        }
    }

    /**
     * Async running code.
     */
    void Execute() {
        if (DEBUG_THREADS) {
            uv_thread_t threadId = uv_thread_self();

            printf("async image loading: start (thread=%lu)\n", (unsigned long)threadId);
        }

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

            /*
            //Note: libuv uses thread-pool, therefore use mutex to prevent parallel execution of JPEG decoder (parallel execution would be better in future using thread-safe code)
            uv_mutex_lock(&jpegMutex);
            decodeJpeg2();
            uv_mutex_unlock(&jpegMutex);
            */
        }

        if (DEBUG_THREADS) {
            printf("async image loading: done\n");
        }
    }

    /**
     * Decode PNG image (using libpng).
     *
     * See http://www.learnopengles.com/loading-a-png-into-memory-and-displaying-it-as-a-texture-with-opengl-es-2-using-almost-the-same-code-on-ios-android-and-emscripten/.
     */
    void decodePng() {
        //Note: header already checked

        //init
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr) {
            SetErrorMessage("could not init PNG decoder");
            return;
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);

        if (!info_ptr) {
            SetErrorMessage("could not init PNG decoder");

            png_destroy_read_struct(&png_ptr, NULL, NULL);

            return;
        }

        //data handler
        ReadDataHandle png_data_handle = (ReadDataHandle){{ (unsigned char *)buffer, bufferLen }, 0};

        png_set_read_fn(png_ptr, &png_data_handle, read_png_data_callback);

        //error handler
        if (setjmp(png_jmpbuf(png_ptr))) {
            SetErrorMessage("could not decode PNG");

            png_read_end(png_ptr, info_ptr);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

            if (imgData) {
                free(imgData);
                imgData = NULL;
            }

            return;
        }

        //PNG header
        png_uint_32 width, height;
        int bit_depth, color_type;

        png_read_info(png_ptr, info_ptr);
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

        if (DEBUG_IMAGES) {
            printf("-> got an image %dx%d (depth=%i, type=%i)\n", (int)width, (int)height, (int)bit_depth, (int)color_type);
            printf("-> data size = %d\n", imgDataLen);
        }

        //convert transparency to full alpha
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(png_ptr);
        }

        //convert grayscale, if needed.
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
            png_set_expand_gray_1_2_4_to_8(png_ptr);
        }

        //convert paletted images, if needed.
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_palette_to_rgb(png_ptr);
        }

        //add alpha channel, if there is none.
        //rationale: GL_RGBA is faster than GL_RGB on many GPUs)
        /*
        if (color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_RGB) {
            png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
        }
        */

        //ensure 8-bit packing
        if (bit_depth < 8) {
            png_set_packing(png_ptr);
        } else if (bit_depth == 16) {
            png_set_scale_16(png_ptr);
        }

        //get final info
        png_read_update_info(png_ptr, info_ptr);

        const png_byte colorType = png_get_color_type(png_ptr, info_ptr);
        const png_size_t rowSize = png_get_rowbytes(png_ptr, info_ptr);
        const png_byte bitDepth = png_get_bit_depth(png_ptr, info_ptr);

        assert(rowSize > 0);

        imgW = width;
        imgH = height;
        imgAlpha = (colorType == PNG_COLOR_TYPE_RGB_ALPHA) || (colorType == PNG_COLOR_TYPE_GRAY_ALPHA);

        switch (colorType) {
            case PNG_COLOR_TYPE_GRAY:
                assert(bitDepth == 8);
                imgBPP = 1;
                break;

            case PNG_COLOR_TYPE_RGB:
                assert(bitDepth == 8);
                imgBPP = 3;
                break;

            case PNG_COLOR_TYPE_RGB_ALPHA:
                assert(bitDepth == 8);
                imgBPP = 4;
                break;

            case PNG_COLOR_TYPE_GRAY_ALPHA:
                assert(bitDepth == 8);
                imgBPP = 2;
                break;

            default:
                //fallback
                printf("unknown color type: %i\n", (int)colorType);

                imgBPP = rowSize / width;
                break;
        }

        if (DEBUG_IMAGES) {
            printf("-> output image %dx%d (bpp=%i, alpha=%i, type=%i)\n", (int)width, (int)height, (int)imgBPP, (int)imgAlpha, (int)colorType);
        }

        //decode

        //printf("-> row=%i calc=%i\n", (int)rowSize, width * imgBPP);
        //printf("-> size=%ix%i, alpha=%i, bpp=%i\n", imgW, imgH, imgAlpha ? 1:0, imgBPP);

        imgDataLen = rowSize * height;
        imgData = (char *)malloc(imgDataLen);

        assert(imgData != NULL);

        png_byte *row_ptrs[height];

        for (png_uint_32 i = 0; i < height; i++) {
            row_ptrs[i] = (unsigned char *)imgData + i * rowSize;
        }

        png_read_image(png_ptr, &row_ptrs[0]);

        //done
        png_read_end(png_ptr, info_ptr);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

        if (DEBUG_IMAGES) {
            printf("-> size=%ix%i, alpha=%i, bpp=%i\n", imgW, imgH, imgAlpha ? 1:0, imgBPP);
        }
    }

    /**
     * Decode PNG image (using upng).
     *
     * Note: this code is thread-safe.
     */
     /*
    void decodePng2() {
        if (DEBUG_IMAGES) {
            printf("decodePng()\n");
        }

        upng = upng_new_from_bytes((const unsigned char *)buffer, bufferLen);

        if (upng == NULL) {
            SetErrorMessage("error decoding PNG file");

            if (DEBUG_IMAGES) {
                printf("-> failed\n");
            }

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

        if (DEBUG_IMAGES) {
            printf("-> size=%ix%i, alpha=%i, bpp=%i\n", imgW, imgH, imgAlpha ? 1:0, imgBPP);
        }
    }
    */

    /**
     * Decode JPEG image (using libjpeg).
     *
     * Note: NOT thread-safe! Has to be called in single queue.
     */
    void decodeJpeg() {
        if (DEBUG_IMAGES) {
            printf("decodeJpeg()\n");
        }

        //check thread
        /*
        uv_thread_t threadId = uv_thread_self();

        printf("JPEG thread: %lu\n", threadId);
        */

        struct jpeg_decompress_struct cinfo;

        //error handler
        struct myjpeg_error_mgr jerr;

        cinfo.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = myjpeg_error_exit;

        if (setjmp(jerr.setjmp_buffer)) {
            SetErrorMessage("error decoding JPEG file");

            jpeg_destroy_decompress(&cinfo);

            if (imgData) {
                free(imgData);
                imgData = NULL;
            }

            return;
        }

        //init
        jpeg_create_decompress(&cinfo);

        //input data
        jpeg_mem_src(&cinfo, (unsigned char *)buffer, bufferLen);

        //decompress
        int rc = jpeg_read_header(&cinfo, TRUE);

        if (rc != 1) {
            SetErrorMessage("error not a JPEG file");

            jpeg_destroy_decompress(&cinfo);
            return;
        }

        jpeg_start_decompress(&cinfo);

        //get JPEG data
        imgW = cinfo.output_width;
	    imgH = cinfo.output_height;
        imgAlpha = false;
        imgBPP = cinfo.output_components;

        //read pixels
        imgDataLen = imgW * imgH * imgBPP;
	    imgData = (char *)malloc(imgDataLen); //gets transferred to buffer

        assert(imgData != NULL);

        if (DEBUG_IMAGES) {
            printf("-> got an image %dx%d\n", imgW, imgH);
            printf("-> data size = %d\n", imgDataLen);
        }

        int rowStride = cinfo.output_width * imgBPP;

        while (cinfo.output_scanline < cinfo.output_height) {
            unsigned char *bufferArray[1];

		    bufferArray[0] = (unsigned char *)imgData + cinfo.output_scanline * rowStride;

    		jpeg_read_scanlines(&cinfo, bufferArray, 1);
        }

        //done
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        if (DEBUG_IMAGES) {
            printf("-> size=%ix%i, alpha=%i, bpp=%i\n", imgW, imgH, imgAlpha ? 1:0, imgBPP);
        }

        isJpeg = true;
    }

    /**
     * Decode JPEG image (unsing nanojpeg).
     *
     * Note: NOT thread-safe! Has to be called in single queue.
     */
     /*
    void decodeJpegOld() {
        if (DEBUG_IMAGES) {
            printf("decodeJpeg()\n");
        }

        //check thread
        / *
        uv_thread_t threadId = uv_thread_self();

        printf("JPEG thread: %lu\n", threadId);
        * /

        njInit();

        if (njDecode(buffer, bufferLen)) {
            SetErrorMessage("error decoding JPEG file");

            if (DEBUG_IMAGES) {
                printf("-> failed\n");
            }

            //console
            if (DEBUG_IMAGES_CONSOLE) {
                printf("Error decoding the JPEG file.\n");
            }

            njDone();

            return;
        }

        if (DEBUG_IMAGES) {
            printf("-> got an image %dx%d\n", njGetWidth(), njGetHeight());
            printf("-> data size = %d\n", njGetImageSize());
        }

        imgDataLen = njGetImageSize();
        imgData = (char *)njGetImage();

        imgW = njGetWidth();
        imgH = njGetHeight();
        imgAlpha = false;
        imgBPP = njGetBPP();

        //get ownership
        njUnlinkImageData();

        //free instance
        njDone();

        if (DEBUG_IMAGES) {
            printf("-> size=%ix%i, alpha=%i, bpp=%i\n", imgW, imgH, imgAlpha ? 1:0, imgBPP);
        }

        isJpeg = true;
    }
    */

    /**
     * Free all allocated buffers.
     */
    void freeImageBuffers() {
        //free image and buffers
        /*
        if (upng) {
            upng_free(upng);
        }
        */
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
        v8::Local<v8::Object> buff;

        if (isJpeg) {
            //transfer ownership
            buff = Nan::NewBuffer(imgData, imgDataLen).ToLocalChecked();
        } else {
            //transfer ownership (libpng)
            buff = Nan::NewBuffer(imgData, imgDataLen).ToLocalChecked();

            //create copy (upng)
            //buff = Nan::CopyBuffer(imgData, imgDataLen).ToLocalChecked();
        }

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

        assert(img);

        img->imageLoaded(buff, imgW, imgH, imgAlpha, imgBPP);

        //call callback
        v8::Local<v8::Value> argv[] = { Nan::Null(), obj };

        callback->Call(2, argv);
    }
};

uv_mutex_t AsyncImageWorker::jpegMutex;
bool AsyncImageWorker::jpegMutexInitialized = false;

//
// AminoImage
//

/**
 * Constructor.
 */
AminoImage::AminoImage(): AminoJSObject(getFactory()->name) {
    //empty
}

/**
 * Destructor.
 */
AminoImage::~AminoImage()  {
    //empty
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
    bufferData = NULL;
    bufferLength = 0;
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

    //debug
    if (DEBUG_IMAGES) {
        printf("createTexture(): buffer=%d, size=%ix%i, bpp=%i\n", (int)bufferLength, w, h, bpp);
    }

    return createTexture(textureId, bufferData, bufferLength, w, h, bpp);
}

/**
 * Create texture.
 *
 * Note: only call from async handler!
 */
GLuint AminoImage::createTexture(GLuint textureId, char *bufferData, size_t bufferLength, int w, int h, int bpp) {
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

    /*
     * clamp to border
     *
     * Note: GL_CLAMP_TO_BORDER not supported by OpenGL ES 2.0
     *
     * - https://lists.cairographics.org/archives/cairo/2011-February/021715.html
     * - https://wiki.linaro.org/WorkingGroups/Middleware/Graphics/GLES2PortingTips
     *
     * Workaround: use GL_CLAMP_TO_EDGE and clip pixels outside in shader
     */

    /*
    //code below works on macOS
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float color[] = { 0.0f, 0.0f, 0.0f, 0.0f };

    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
    */

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return texture;
}

/**
 * Get factory instance.
 */
AminoImageFactory* AminoImage::getFactory() {
    static AminoImageFactory *aminoImageFactory = NULL;

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
    assert(info.Length() == 2);

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

    //get buffer data for OpenGL thread
    bufferData = node::Buffer::Data(buffer);
    bufferLength = node::Buffer::Length(buffer);
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
 * Create AminoImage instance.
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
    //empty
}

/**
 * Destructor.
 */
AminoTexture::~AminoTexture()  {
    //empty
}

/**
 * Free resources.
 */
void AminoTexture::destroy() {
    if (destroyed) {
        return;
    }

    if (callback) {
        delete callback;
        callback = NULL;
    }

    if (textureId != INVALID_TEXTURE) {
        //Note: we are on the main thread
        if (eventHandler && ownTexture) {
            (static_cast<AminoGfx *>(eventHandler))->deleteTextureAsync(textureId);
        }

        textureId = INVALID_TEXTURE;
        w = 0;
        h = 0;

        v8::Local<v8::Object> obj = handle();

        Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::Undefined());
        Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::Undefined());
    }

    //Note: frees eventHandler
    AminoJSObject::destroy();
}

/**
 * Get factory instance.
 */
AminoTextureFactory* AminoTexture::getFactory() {
    static AminoTextureFactory *aminoTextureFactory = NULL;

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
    Nan::SetPrototypeMethod(tpl, "loadTextureFromImage", LoadTextureFromImage);
    Nan::SetPrototypeMethod(tpl, "loadTextureFromBuffer", LoadTextureFromBuffer);
    Nan::SetPrototypeMethod(tpl, "loadTextureFromFont", LoadTextureFromFont);
    Nan::SetPrototypeMethod(tpl, "destroy", Destroy);

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

    assert(info.Length() == 1);

    //set amino instance
    v8::Local<v8::Object> jsObj = info[0]->ToObject();
    AminoJSEventObject *obj = Nan::ObjectWrap::Unwrap<AminoJSEventObject>(jsObj);

    assert(obj);

    //bind to queue
    setEventHandler(obj);
    Nan::Set(handle(), Nan::New("amino").ToLocalChecked(), jsObj);
}

/**
 * Load texture asynchronously.
 *
 * loadTextureFromImage(img, callback)
 */
NAN_METHOD(AminoTexture::LoadTextureFromImage) {
    if (DEBUG_IMAGES) {
        printf("-> loadTextureFromImage()\n");
    }

    assert(info.Length() == 2);

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());
    v8::Local<v8::Function> callback = info[1].As<v8::Function>();

    assert(obj);

    if (obj->callback || obj->textureId != INVALID_TEXTURE) {
        //already set
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("already loading") };

        callback->Call(info.This(), argc, argv);
        return;
    }

    //image
    AminoImage *img = Nan::ObjectWrap::Unwrap<AminoImage>(info[0]->ToObject());

    assert(img);

    if (!img->hasImage()) {
        //missing image
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("image not loaded") };

        callback->Call(info.This(), argc, argv);
        return;
    }

    if (DEBUG_BASE) {
        printf("enqueue: create texture\n");
    }

    //async loading
    obj->callback = new Nan::Callback(callback);
    obj->enqueueValueUpdate(img, static_cast<asyncValueCallback>(&AminoTexture::createTexture));
}

/**
 * Create texture.
 */
void AminoTexture::createTexture(AsyncValueUpdate *update, int state) {
    if (state == AsyncValueUpdate::STATE_APPLY) {
        //create texture on OpenGL thread

        if (DEBUG_IMAGES) {
            printf("-> createTexture()\n");
        }

        AminoImage *img = static_cast<AminoImage *>(update->valueObj);
        bool newTexture = this->textureId == INVALID_TEXTURE;
        GLuint textureId = img->createTexture(this->textureId);

        if (textureId != INVALID_TEXTURE) {
            //set values
            this->textureId = textureId;
            ownTexture = true;

            w = img->w;
            h = img->h;

            if (newTexture) {
               (static_cast<AminoGfx *>(eventHandler))->notifyTextureCreated();
            }
        }
    } else if (state == AsyncValueUpdate::STATE_DELETE) {
        //on main thread

        if (this->textureId == INVALID_TEXTURE) {
            //failed
            int argc = 1;
            v8::Local<v8::Value> argv[1] = { Nan::Error("could not create texture") };

            callback->Call(handle(), argc, argv);
            return;
        }

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
}

typedef struct {
    char *bufferData;
    size_t bufferLen;
    int w;
    int h;
    int bpp;
    Nan::Callback *callback;
} amino_texture_t;

/**
 * Load texture from pixel buffer.
 */
NAN_METHOD(AminoTexture::LoadTextureFromBuffer) {
    if (DEBUG_IMAGES) {
        printf("-> loadTextureFromBuffer()\n");
    }

    assert(info.Length() == 2);

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);
    assert(obj->ownTexture);

    //data
    v8::Local<v8::Value> data = info[0];
    v8::Local<v8::Object> dataObj = data->ToObject();
    v8::Local<v8::Object> bufferObj = Nan::Get(dataObj, Nan::New<v8::String>("buffer").ToLocalChecked()).ToLocalChecked()->ToObject();
    amino_texture_t *textureData = new amino_texture_t();

    textureData->bufferData = node::Buffer::Data(bufferObj);
    textureData->bufferLen = node::Buffer::Length(bufferObj);

    textureData->w = Nan::Get(dataObj, Nan::New<v8::String>("w").ToLocalChecked()).ToLocalChecked()->IntegerValue();
    textureData->h = Nan::Get(dataObj, Nan::New<v8::String>("h").ToLocalChecked()).ToLocalChecked()->IntegerValue();
    textureData->bpp = Nan::Get(dataObj, Nan::New<v8::String>("bpp").ToLocalChecked()).ToLocalChecked()->IntegerValue();

    //callback
    v8::Local<v8::Function> callback = info[1].As<v8::Function>();

    textureData->callback = new Nan::Callback(callback);

    if (DEBUG_BASE) {
        printf("enqueue: create texture from buffer\n");
    }

    //async loading
    obj->enqueueValueUpdate(data, textureData, static_cast<asyncValueCallback>(&AminoTexture::createTextureFromBuffer));
}

/**
 * Create texture.
 */
void AminoTexture::createTextureFromBuffer(AsyncValueUpdate *update, int state) {
    if (state == AsyncValueUpdate::STATE_APPLY) {
        //create texture on OpenGL thread

        if (DEBUG_IMAGES) {
            printf("-> createTextureFromBuffer()\n");
        }

        amino_texture_t *textureData = (amino_texture_t *)update->data;

        assert(textureData);

        bool newTexture = this->textureId == INVALID_TEXTURE;
        GLuint textureId = AminoImage::createTexture(this->textureId, textureData->bufferData, textureData->bufferLen, textureData->w, textureData->h, textureData->bpp);

        if (textureId != INVALID_TEXTURE) {
            //set values
            this->textureId = textureId;

            w = textureData->w;
            h = textureData->h;

            if (newTexture) {
                (static_cast<AminoGfx *>(eventHandler))->notifyTextureCreated();
            }
        }
    } else if (state == AsyncValueUpdate::STATE_DELETE) {
        //on main thread
        amino_texture_t *textureData = (amino_texture_t *)update->data;

        assert(textureData);

        if (this->textureId == INVALID_TEXTURE) {
            //failed

            if (textureData->callback) {
                int argc = 1;
                v8::Local<v8::Value> argv[1] = { Nan::Error("could not create texture") };

                textureData->callback->Call(handle(), argc, argv);
                delete textureData->callback;
            }

            return;
        }

        v8::Local<v8::Object> obj = handle();

        Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

        //callback
        if (textureData->callback) {
            int argc = 2;
            v8::Local<v8::Value> argv[2] = { Nan::Null(), obj };

            textureData->callback->Call(obj, argc, argv);
            delete textureData->callback;
        }

        //free
        delete textureData;
        update->data = NULL;
    }
}

/**
 * Load texture from font.
 */
NAN_METHOD(AminoTexture::LoadTextureFromFont) {
    if (DEBUG_IMAGES) {
        printf("-> loadTextureFromBuffer()\n");
    }

    assert(info.Length() == 2);

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    //callback
    v8::Local<v8::Function> callback = info[1].As<v8::Function>();

    if (obj->callback || obj->textureId != INVALID_TEXTURE) {
        //already set
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("already loading") };

        callback->Call(info.This(), argc, argv);
        return;
    }

    //font
    AminoFontSize *fontSize = Nan::ObjectWrap::Unwrap<AminoFontSize>(info[0]->ToObject());

    if (DEBUG_BASE) {
        printf("enqueue: create texture from font\n");
    }

    //async loading
    obj->callback = new Nan::Callback(callback);
    obj->enqueueValueUpdate(fontSize, static_cast<asyncValueCallback>(&AminoTexture::createTextureFromFont));
}

/**
 * Create texture.
 */
void AminoTexture::createTextureFromFont(AsyncValueUpdate *update, int state) {
    if (state == AsyncValueUpdate::STATE_APPLY) {
        //create texture on OpenGL thread

        if (DEBUG_IMAGES) {
            printf("-> createTextureFromFont()\n");
        }

        AminoFontSize *fontSize = static_cast<AminoFontSize *>(update->valueObj);

        assert(fontSize);
        assert(eventHandler);

        //use current font texture
        texture_atlas_t *atlas = fontSize->fontTexture->atlas;
        bool newTexture;
        GLuint textureId = (static_cast<AminoGfx *>(eventHandler))->getAtlasTexture(atlas, true, newTexture).textureId;

        if (textureId != INVALID_TEXTURE) {
            //set values
            this->textureId = textureId;
            ownTexture = false;

            w = atlas->width;
            h = atlas->height;
        }
    } else if (state == AsyncValueUpdate::STATE_DELETE) {
        //on main thread
        AminoFontSize *fontSize = static_cast<AminoFontSize *>(update->valueObj);

        assert(fontSize);

        if (this->textureId == INVALID_TEXTURE) {
            //failed

            if (callback) {
                int argc = 1;
                v8::Local<v8::Value> argv[1] = { Nan::Error("could not create texture") };

                callback->Call(handle(), argc, argv);
                delete callback;
            }

            return;
        }

        v8::Local<v8::Object> obj = handle();

        Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

        //callback
        if (callback) {
            int argc = 2;
            v8::Local<v8::Value> argv[2] = { Nan::Null(), obj };

            callback->Call(obj, argc, argv);
            delete callback;
        }
    }
}

/**
 * Free texture.
 */
NAN_METHOD(AminoTexture::Destroy) {
    //debug
    //printf("-> destroy()\n");

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    obj->destroy();
}

//
//  AminoTextureFactory
//

/**
 * Create AminoTexture factory.
 */
AminoTextureFactory::AminoTextureFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoTexture", callback) {
    //empty
}

/**
 * Create AminoTexture instance.
 */
AminoJSObject* AminoTextureFactory::create() {
    return new AminoTexture();
}
