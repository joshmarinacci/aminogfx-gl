#include "base_weak.h"

//
// AminoWeakReference
//

/**
 * Construct weak reference.
 */
AminoWeakReference::AminoWeakReference(v8::Local<v8::Object> &value) {
    this->value.Reset(value);

    //make weak
    //FIXME does not recognize kParameter: https://github.com/nodejs/nan/blob/master/nan_weak.h
    this->value.SetWeak(this, weakCallbackHandler, Nan::WeakCallbackType::kParameter);
}

/**
 * Destructor.
 */
AminoWeakReference::~AminoWeakReference() {
    value.Reset();
}

/**
 * Check if reference still exists.
 */
bool AminoWeakReference::hasReference() {
    return active;
}

/**
 * Get value.
 */
v8::Local<v8::Object> AminoWeakReference::getValue() {
    assert(active);

    return Nan::New(value);
}

/**
 * Callback function.
 */
void AminoWeakReference::weakCallbackHandler(const Nan::WeakCallbackInfo<AminoWeakReference> &data) {
    AminoWeakReference *obj = data.GetParameter();

    obj->handleReferenceLost();
}

/**
 * Value was garbage collected.
 */
void AminoWeakReference::handleReferenceLost() {
    active = false;
    value.Reset();

    //callback
    if (callback) {
        (obj->*callback)(this);
    }
}

/**
 * Set callback.
 */
void AminoWeakReference::setCallback(AminoJSObject *obj, weakCallback callback) {
    this->obj = obj;
    this->callback = callback;
}

//
// AminoJSWeakReference
//

/**
 * Constructor.
 */
AminoJSWeakReference::AminoJSWeakReference(): AminoJSObject(getFactory()->name) {
    //empty
}

/**
 * Destructor.
 */
AminoJSWeakReference::~AminoJSWeakReference()  {
    //empty
}

/**
 * Get the weak reference handler.
 */
AminoWeakReference *AminoJSWeakReference::getWeakReference() {
    return weak;
}

/**
 * Free all resources.
 */
void AminoJSWeakReference::destroy() {
    if (destroyed) {
        return;
    }

    AminoJSObject::destroy();

    if (weak) {
        delete weak;
        weak = NULL;
    }

    if (callback) {
        delete callback;
        callback = NULL;
    }

    //release instance
    if (retained) {
        release();
        retained = false;
    }
}

/**
 * Get factory instance.
 */
AminoWeakReferenceFactory* AminoJSWeakReference::getFactory() {
    static AminoWeakReferenceFactory *aminoWeakReferenceFactory = NULL;

    if (!aminoWeakReferenceFactory) {
        aminoWeakReferenceFactory = new AminoWeakReferenceFactory(New);
    }

    return aminoWeakReferenceFactory;
}

/**
 * Add class template to module exports.
 */
NAN_MODULE_INIT(AminoJSWeakReference::Init) {
    if (DEBUG_BASE) {
        printf("AminoJSWeakReference init\n");
    }

    AminoWeakReferenceFactory *factory = getFactory();
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(factory);

    //prototype methods
    Nan::SetPrototypeMethod(tpl, "hasReference", HasReference);
    Nan::SetPrototypeMethod(tpl, "getReference", GetReference);

    //global template instance
    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

/**
 * Handle constructor params.
 */
void AminoJSWeakReference::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    //weak reference
    if (!info[0]->IsObject()) {
        Nan::ThrowTypeError("expected object");
        return;
    }

    v8::Local<v8::Object> value = info[0]->ToObject();

    weak = new AminoWeakReference(value);

    //callback
    if (info[1]->IsFunction()) {
        callback = new Nan::Callback(info[1].As<v8::Function>());
    }

    //listener
    weak->setCallback(this, (AminoWeakReference::weakCallback)&AminoJSWeakReference::weakCallbackHandler);

    //retain instance
    retain();
    retained = true;
}

/**
 * Weak reference lost.
 */
void AminoJSWeakReference::weakCallbackHandler(AminoWeakReference *weak) {
    //fire callback
    if (callback) {
        //create scope
        Nan::HandleScope scope;

        v8::Local<v8::Object> obj = handle();
        v8::Local<v8::Value> argv[] = { Nan::Null(), obj };

        callback->Call(obj, 2, argv);
    }

    //free instance
    if (retained) {
        release();
        retained = false;
    }
}

/**
 * Check reference.
 */
NAN_METHOD(AminoJSWeakReference::HasReference) {
    AminoJSWeakReference *obj = Nan::ObjectWrap::Unwrap<AminoJSWeakReference>(info.This());

    assert(obj);

    info.GetReturnValue().Set(Nan::New(obj->getWeakReference()->hasReference()));
}

/**
 * Get the reference value.
 */
NAN_METHOD(AminoJSWeakReference::GetReference) {
    AminoJSWeakReference *obj = Nan::ObjectWrap::Unwrap<AminoJSWeakReference>(info.This());

    assert(obj);

    AminoWeakReference *weak = obj->getWeakReference();
    v8::Local<v8::Value> res;

    if (weak->hasReference()) {
        res = weak->getValue();
    } else {
        res = Nan::Undefined();
    }

    info.GetReturnValue().Set(res);

}

/**
 * JS object construction.
 */
NAN_METHOD(AminoJSWeakReference::New) {
    AminoJSObject::createInstance(info, getFactory());
}

//
// AminoWeakReferenceFactory
//

/**
 * Create AminoJSWeakReference factory.
 */
AminoWeakReferenceFactory::AminoWeakReferenceFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoWeakReference", callback) {
    //empty
}

/**
 * Create AminoImage instance.
 */
AminoJSObject* AminoWeakReferenceFactory::create() {
    return new AminoJSWeakReference();
}
