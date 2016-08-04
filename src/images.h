#ifndef _AMINOIMAGES_H
#define _AMINOIMAGES_H

#include "base_js.h"
#include "gfx.h"

class AminoImageFactory;

/**
 * Amino Image Loader.
 *
 * Convert image binary data to pixel array.
 */
class AminoImage : public AminoJSObject {
public:
    int w = 0;
    int h = 0;
    bool alpha = 0;
    int bpp = 0;

    AminoImage();
    ~AminoImage();

    bool hasImage();
    void destroy() override;
    GLuint createTexture(GLuint textureId);
    static GLuint createTexture(GLuint textureId, char *bufferData, size_t bufferLength, int w, int h, int bpp);

    void imageLoaded(v8::Local<v8::Object> &buffer, int w, int h, bool alpha, int bpp);

    //creation
    static AminoImageFactory* getFactory();

    //init
    static NAN_MODULE_INIT(Init);

private:
    Nan::Persistent<v8::Object> buffer;

    //JS constructor
    static NAN_METHOD(New);

    //JS methods
    static NAN_METHOD(loadImage);
};

/**
 * AminoImage class factory.
 */
class AminoImageFactory : public AminoJSObjectFactory {
public:
    AminoImageFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

class AminoTextureFactory;

/**
 * Amino Texture class.
 */
class AminoTexture : public AminoJSObject {
public:
    GLuint textureId = INVALID_TEXTURE;
    int w = 0;
    int h = 0;

    AminoTexture();
    virtual ~AminoTexture();

    void destroy() override;

    //creation
    static AminoTextureFactory* getFactory();

    //init
    static v8::Local<v8::Function> GetInitFunction();

private:
    Nan::Callback *callback = NULL;

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override;

    //JS constructor
    static NAN_METHOD(New);

    //JS methods
    static NAN_METHOD(LoadTextureFromImage);
    static NAN_METHOD(LoadTextureFromBuffer);

    void createTexture(AsyncValueUpdate *update, int state);
    void createTextureFromBuffer(AsyncValueUpdate *update, int state);
};

/**
 * AminoTexture class factory.
 */
class AminoTextureFactory : public AminoJSObjectFactory {
public:
    AminoTextureFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

#endif