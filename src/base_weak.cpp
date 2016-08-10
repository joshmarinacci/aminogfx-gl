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
    this->callback;
}
