#ifndef _AMINOBASE_WEAK_H
#define _AMINOBASE_WEAK_H

#include "base_js.h"

/**
 * Weak reference holder.
 */
class AminoWeakReference {
public:
    AminoWeakReference(v8::Local<v8::Object> &value);
    ~AminoWeakReference();

    bool hasReference();
    v8::Local<v8::Object> getValue();

    //callback
    typedef void (AminoJSObject::*weakCallback)(AminoWeakReference *);

    void setCallback(AminoJSObject *obj, weakCallback callback);

private:
    Nan::Persistent<v8::Object> value;
    bool active = true;

    //callback
    AminoJSObject *obj;
    weakCallback callback;

    static void weakCallbackHandler(const Nan::WeakCallbackInfo<AminoWeakReference> &data);
    void handleReferenceLost();
};

#endif
