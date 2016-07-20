#ifndef _AMINOIMAGES_H
#define _AMINOIMAGES_H

#include "base_js.h"
#include "gfx.h"

#define INVALID_TEXTURE 0

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
    virtual ~AminoImage();

    bool hasImage();
    void destroy() override;
    GLuint createTexture(GLuint textureId);

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
    GLuint textureId = 0;
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
    static NAN_METHOD(loadTexture);

    void createTexture(AsyncValueUpdate *update);
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