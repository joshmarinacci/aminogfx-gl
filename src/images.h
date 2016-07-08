#ifndef _AMINOIMAGES_H
#define _AMINOIMAGES_H

#include "gfx.h"
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

    static NAN_MODULE_INIT(Init);
    static NAN_METHOD(New);

private:
    //JS methods
    static NAN_METHOD(loadImage);
};

/**
 * AminoImage class factory.
 */
class AminoImageFactory : public AminoJSObjectFactory {
public:
    AminoImageFactory();

    AminoJSObject* create();
};

#endif