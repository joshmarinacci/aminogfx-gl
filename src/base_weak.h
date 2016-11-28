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
    AminoJSObject *obj = NULL;
    weakCallback callback = NULL;

    static void weakCallbackHandler(const Nan::WeakCallbackInfo<AminoWeakReference> &data);
    void handleReferenceLost();
};

class AminoWeakReferenceFactory;

/**
 * Weak reference tool for JavaScript.
 */
class AminoJSWeakReference : public AminoJSObject {
public:
    AminoJSWeakReference();
    ~AminoJSWeakReference();

    AminoWeakReference *getWeakReference();

    //creation
    static AminoWeakReferenceFactory* getFactory();

    static NAN_MODULE_INIT(Init);

    //init
    static v8::Local<v8::Function> GetInitFunction();

protected:
    void destroy() override;

private:
    AminoWeakReference *weak = NULL;
    Nan::Callback *callback = NULL;
    bool retained = false;

    void weakCallbackHandler(AminoWeakReference *weak);

    //JS constructor
    static NAN_METHOD(New);

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override;

    //JS methods
    static NAN_METHOD(HasReference);
    static NAN_METHOD(GetReference);
};

/**
 * AminoJSWeakReference class factory.
 */
class AminoWeakReferenceFactory : public AminoJSObjectFactory {
public:
    AminoWeakReferenceFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

#endif
