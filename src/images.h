#ifndef _AMINOIMAGES_H
#define _AMINOIMAGES_H

#include "base_js.h"

class AminoImageFactory;

/**
 * Amino Image Loader.
 *
 * Convert image binary data to pixel array.
 */
class AminoImage : public AminoJSObject {
public:
    AminoImage();
    virtual ~AminoImage();

    //creation
    static AminoImageFactory* getFactory();

    //init
    static NAN_MODULE_INIT(Init);
private:
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

    AminoJSObject* create();
};

#endif