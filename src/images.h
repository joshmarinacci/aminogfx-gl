#ifndef _AMINOIMAGES_H
#define _AMINOIMAGES_H

#include "gfx.h"

/**
 * Amino Image Loader.
 *
 * Convert image binary data to pixel array.
 */
class AminoImage : public Nan::ObjectWrap {
public:
    static NAN_MODULE_INIT(Init);

private:
    static Nan::Persistent<v8::Function> constructor;

    AminoImage();
    virtual ~AminoImage();

    //creation
    static NAN_METHOD(New);

    //JS methods
    static NAN_METHOD(loadImage);
};

#endif