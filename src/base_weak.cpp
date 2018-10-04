#include "base_weak.h"

#define DEBUG_WEAK false

//
// AminoWeakReference
//

/**
 * Construct weak reference.
 */
AminoWeakReference::AminoWeakReference(v8::Local<v8::Object> &value) {
    if (DEBUG_WEAK) {
        printf("AminoWeakReference()\n");
    }

    this->value.Reset(value);

    //make weak
    //Note: does not recognize kParameter and only supports objects: https://github.com/nodejs/nan/blob/master/nan_weak.h
    this->value.SetWeak(this, weakCallbackHandler, Nan::WeakCallbackType::kParameter);
}

/**
 * Destructor.
 */
AminoWeakReference::~AminoWeakReference() {
    if (DEBUG_WEAK) {
        printf("~AminoWeakReference()\n");
    }

    value.Reset();

    assert(!active);
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
    if (DEBUG_WEAK) {
        printf("AminoWeakReference::getValue()\n");
    }

    assert(active);
    assert(!value.IsEmpty());

    //new strong reference
    return Nan::New(value);
}

/**
 * Callback function.
 */
void AminoWeakReference::weakCallbackHandler(const Nan::WeakCallbackInfo<AminoWeakReference> &data) {
    if (DEBUG_WEAK) {
        printf("AminoWeakReference::weakCallbackHandler()\n");
    }

    AminoWeakReference *obj = data.GetParameter();

    assert(obj);

    obj->handleReferenceLost();
}

/**
 * Value was garbage collected.
 */
void AminoWeakReference::handleReferenceLost() {
    active = false;

    //Note: do not call value.Reset() here! Leads to crash.

    //callback
    if (callback) {
        (obj->*callback)(this);
    }

    if (DEBUG_WEAK) {
        printf("-> done\n");
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
    if (DEBUG_WEAK) {
        printf("~AminoJSWeakReference\n");
    }

    assert(!weak || !weak->hasReference());

    if (!destroyed) {
        destroyAminoJSWeakReference();
    }
}

/**
 * Get the weak reference handler.
 */
AminoWeakReference *AminoJSWeakReference::getWeakReference() {
    assert(weak);

    return weak;
}

/**
 * Free all resources.
 */
void AminoJSWeakReference::destroy() {
    if (destroyed) {
        return;
    }

    //instance
    destroyAminoJSWeakReference();

    //base class
    AminoJSObject::destroy();
}

/**
 * Free all resources.
 */
void AminoJSWeakReference::destroyAminoJSWeakReference() {
    if (DEBUG_WEAK) {
        printf("AminoJSWeakReference::destroy()\n");
    }

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
    if (info.Length() == 0 || !info[0]->IsObject()) {
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
    if (DEBUG_WEAK) {
        printf("AminoJSWeakReference::weakCallbackHandler()\n");
    }

    //fire callback
    if (callback) {
        //create scope
        Nan::HandleScope scope;

        v8::Local<v8::Object> obj = handle();
        v8::Local<v8::Value> argv[] = { Nan::Null(), obj };

        Nan::Call(*callback, obj, 2, argv);
    }

    //free instance
    if (retained) {
        release();
        retained = false;
    }

    if (DEBUG_WEAK) {
        printf("-> done\n");
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
 * Create AminoJSWeakReference instance.
 */
AminoJSObject* AminoWeakReferenceFactory::create() {
    return new AminoJSWeakReference();
}
