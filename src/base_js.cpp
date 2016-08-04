#include "base_js.h"

#include <sstream>

//
//  AminoJSObjectFactory
//

/**
 * Create factory for AminoJSObject creation.
 */
AminoJSObjectFactory::AminoJSObjectFactory(std::string name, Nan::FunctionCallback callback) {
    this->name = name;
    this->callback = callback;
}

/**
 * Create new object instance.
 */
AminoJSObject* AminoJSObjectFactory::create() {
    return NULL;
}

//
//  AminoJSObject
//

/**
 * Constructor.
 */
AminoJSObject::AminoJSObject(std::string name) {
    this->name = name;

    if (DEBUG_BASE) {
        printf("%s constructor\n", name.c_str());
    }
}

/**
 * Destructor.
 */
AminoJSObject::~AminoJSObject() {
    if (DEBUG_BASE) {
        printf("%s destructor\n", name.c_str());
    }

    if (!destroyed) {
        destroy();
    }

    //free properties
    for (std::map<int, AnyProperty *>::iterator iter = propertyMap.begin(); iter != propertyMap.end(); iter++) {
        delete iter->second;
    }
}

/**
 * Get JS class name.
 *
 * Note: abstract
 */
std::string AminoJSObject::getName() {
    return name;
}

/**
 * Initialize the native object with passed parameters. Called before JS init().
 */
void AminoJSObject::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    //empty
}

/**
 * Initialize the native object.
 *
 * Called after the JS init() method.
 */
void AminoJSObject::setup() {
    //empty
}

/**
 * Free all resources.
 */
void AminoJSObject::destroy() {
    destroyed = true;

    //event handler
    clearEventHandler();
}

/**
 * Retain JS reference.
 */
void AminoJSObject::retain() {
    Ref();

    if (DEBUG_REFERENCES) {
        printf("--- %s references: %i (+1)\n", name.c_str(), refs_);
    }
}

/**
 * Release JS reference.
 */
void AminoJSObject::release() {
    Unref();

    if (DEBUG_REFERENCES) {
        printf("--- %s references: %i (-1)\n", name.c_str(), refs_);
    }
}

/**
 * Get reference counter.
 */
int AminoJSObject::getReferenceCount() {
    return refs_;
}

/**
 * Create JS function template from factory.
 */
v8::Local<v8::FunctionTemplate> AminoJSObject::createTemplate(AminoJSObjectFactory* factory) {
    if (DEBUG_BASE) {
        printf("%s template created\n", factory->name.c_str());
    }

    //initialize template (bound to New method)
    v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(factory->callback);

    tpl->SetClassName(Nan::New(factory->name).ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1); //object reference only stored

    return tpl;
}

/**
 * Create JS and native instance from factory.
 */
void AminoJSObject::createInstance(Nan::NAN_METHOD_ARGS_TYPE info, AminoJSObjectFactory* factory) {
    if (DEBUG_BASE) {
        printf("-> new %s()\n", factory->name.c_str());
    }

    //check constructor call
    if (!info.IsConstructCall()) {
        //called as plain function (e.g. in extended class)
        Nan::ThrowTypeError("please use new() instead of function call");
        return;
    }

    //new AminoObj()

    //create new instance
    AminoJSObject *obj = factory->create();

    assert(obj);

    obj->Wrap(info.This());

    //pre-init
    obj->preInit(info);

    //call init (if available)
    Nan::MaybeLocal<v8::Value> initValue = Nan::Get(info.This(), Nan::New<v8::String>("init").ToLocalChecked());

    if (!initValue.IsEmpty()) {
        v8::Local<v8::Value> initLocal = initValue.ToLocalChecked();

        if (initLocal->IsFunction()) {
            v8::Local<v8::Function> initFunc = initLocal.As<v8::Function>();

            //call
            int argc = 0;
            v8::Local<v8::Value> argv[0];

            initFunc->Call(info.This(), argc, argv);
        }
    }

    //native setup
    obj->setup();

    //call initDone (if available)
    Nan::MaybeLocal<v8::Value> initDoneValue = Nan::Get(info.This(), Nan::New<v8::String>("initDone").ToLocalChecked());

    if (!initDoneValue.IsEmpty()) {
        v8::Local<v8::Value> initDoneLocal = initDoneValue.ToLocalChecked();

        if (initDoneLocal->IsFunction()) {
            v8::Local<v8::Function> initDoneFunc = initDoneLocal.As<v8::Function>();

            //call
            int argc = 0;
            v8::Local<v8::Value> argv[0];

            initDoneFunc->Call(info.This(), argc, argv);
        }
    }

    info.GetReturnValue().Set(info.This());
}

/**
 * Watch JS property changes.
 *
 * Note: has to be called in JS scope of setup()!
 */
bool AminoJSObject::addPropertyWatcher(std::string name, int id, v8::Local<v8::Value> &jsValue) {
    if (DEBUG_BASE) {
        printf("addPropertyWatcher(): %s\n", name.c_str());
    }

    //get property object
    Nan::MaybeLocal<v8::Value> prop = Nan::Get(handle(), Nan::New<v8::String>(name).ToLocalChecked());

    if (prop.IsEmpty()) {
        if (DEBUG_BASE) {
            printf("-> property not defined: %s\n", name.c_str());
        }

        return false;
    }

    v8::Local<v8::Value> propLocal = prop.ToLocalChecked();

    if (!propLocal->IsObject()) {
        if (DEBUG_BASE) {
            printf("-> property not an object: %s in %s\n", name.c_str(), this->name.c_str());
        }

        return false;
    }

    v8::Local<v8::Object> obj = propLocal.As<v8::Object>();

    //set nativeListener value
    Nan::Set(obj, Nan::New<v8::String>("nativeListener").ToLocalChecked(), Nan::New<v8::Function>(PropertyUpdated));

    //set propId value
    Nan::Set(obj, Nan::New<v8::String>("propId").ToLocalChecked(), Nan::New<v8::Integer>(id));

    //default JS value
    Nan::MaybeLocal<v8::Value> valueMaybe = Nan::Get(obj, Nan::New<v8::String>("value").ToLocalChecked());

    if (valueMaybe.IsEmpty()) {
        jsValue = Nan::Undefined();
    } else {
        jsValue = valueMaybe.ToLocalChecked();

        if (DEBUG_BASE) {
            v8::String::Utf8Value str(jsValue);

            printf("-> default value: %s\n", *str);
        }
    }

    return true;
}

/**
 * Create float property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::FloatProperty* AminoJSObject::createFloatProperty(std::string name) {
    int id = ++lastPropertyId;
    FloatProperty *prop = new FloatProperty(this, name, id);

    addProperty(prop);

    return prop;
}

AminoJSObject::FloatArrayProperty* AminoJSObject::createFloatArrayProperty(std::string name) {
    int id = ++lastPropertyId;
    FloatArrayProperty *prop = new FloatArrayProperty(this, name, id);

    addProperty(prop);

    return prop;
}

AminoJSObject::Int32Property* AminoJSObject::createInt32Property(std::string name) {
    int id = ++lastPropertyId;
    Int32Property *prop = new Int32Property(this, name, id);

    addProperty(prop);

    return prop;
}

AminoJSObject::UInt32Property* AminoJSObject::createUInt32Property(std::string name) {
    int id = ++lastPropertyId;
    UInt32Property *prop = new UInt32Property(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create boolean property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::BooleanProperty* AminoJSObject::createBooleanProperty(std::string name) {
    int id = ++lastPropertyId;
    BooleanProperty *prop = new BooleanProperty(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create UTF8 string property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::Utf8Property* AminoJSObject::createUtf8Property(std::string name) {
    int id = ++lastPropertyId;
    Utf8Property *prop = new Utf8Property(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create object property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::ObjectProperty* AminoJSObject::createObjectProperty(std::string name) {
    int id = ++lastPropertyId;
    ObjectProperty *prop = new ObjectProperty(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Bind a property to a watcher.
 *
 * Note: has to be called in JS scope of setup()!
 */
void AminoJSObject::addProperty(AnyProperty *prop) {
    assert(prop);

    int id = prop->id;
    propertyMap.insert(std::pair<int, AnyProperty *>(id, prop));

    v8::Local<v8::Value> value;

    if (addPropertyWatcher(prop->name, id, value)) {
        prop->connected = true;

        //set default value
        void *data = prop->getAsyncData(value);

        if (data) {
            prop->setAsyncData(NULL, data);
            prop->freeAsyncData(data);
        }
    }
}

/**
 * Callback from property watcher to update native value.
 */
NAN_METHOD(AminoJSObject::PropertyUpdated) {
    //params: value, propId, object
    int id = info[1]->IntegerValue();

    //pass to object instance
    v8::Local<v8::Value> value = info[0];
    v8::Local<v8::Object> jsObj = info[2].As<v8::Object>();
    AminoJSObject *obj = Nan::ObjectWrap::Unwrap<AminoJSObject>(jsObj);

    assert(obj);

    obj->enqueuePropertyUpdate(id, value);
}

/**
 * Set the event handler instance.
 *
 * Note: retains the new event handler object instance.
 */
void AminoJSObject::setEventHandler(AminoJSEventObject *handler) {
    if (eventHandler != handler) {
        if (eventHandler) {
            eventHandler->release();
        }

        eventHandler = handler;

        //retain handler instance (in addition to 'amino' JS property)
        if (handler) {
            assert(!destroyed);

            handler->retain();
        }
    }
}

/**
 * Remove event handler.
 */
void AminoJSObject::clearEventHandler() {
    setEventHandler(NULL);
}

/**
 * This is not an event handeler.
 */
bool AminoJSObject::isEventHandler() {
    return false;
}

/**
 * Enqueue a value update.
 *
 * Note: has to be called on main thread.
 */
bool AminoJSObject::enqueueValueUpdate(AminoJSObject *value, asyncValueCallback callback) {
    return enqueueValueUpdate(new AsyncValueUpdate(this, value, callback));
}

/**
 * Enqueue a value update.
 *
 * Note: has to be called on main thread.
 */
bool AminoJSObject::enqueueValueUpdate(unsigned int value, asyncValueCallback callback) {
    return enqueueValueUpdate(new AsyncValueUpdate(this, value, callback));
}

/**
 * Enqueue a value update.
 *
 * Note: has to be called on main thread.
 */
bool AminoJSObject::enqueueValueUpdate(v8::Local<v8::Value> value, void *data, asyncValueCallback callback) {
    return enqueueValueUpdate(new AsyncValueUpdate(this, value, data, callback));
}

/**
 * Enqueue a value update.
 *
 * Note: called on main thread.
 */
bool AminoJSObject::enqueueValueUpdate(AsyncValueUpdate *update) {
    if (eventHandler) {
        return eventHandler->enqueueValueUpdate(update);
    }

    printf("missing queue: %s\n", name.c_str());

    delete update;

    return false;
}

/**
 * Handly property update on main thread (optional).
 */
bool AminoJSObject::handleSyncUpdate(AnyProperty *property, void *data) {
    //no default handling
    return false;
}

/**
 * Default implementation sets the value received from JS.
 */
void AminoJSObject::handleAsyncUpdate(AsyncPropertyUpdate *update) {
    //overwrite for extended handling

    assert(update);

    //default: update value
    update->apply();

    if (DEBUG_BASE) {
        AnyProperty *property = update->property;
        std::string str = property->toString();

        printf("-> updated %s in %s to %s\n", property->name.c_str(), property->obj->name.c_str(), str.c_str());
    }
}

/**
 * Custom handler for implementation specific async update.
 */
bool AminoJSObject::handleAsyncUpdate(AsyncValueUpdate *update) {
    //overwrite

    assert(update);

    if (update->callback) {
        update->apply();

        return true;
    }

    return false;
}

/**
 * Enqueue a property update.
 *
 * Note: called on main thread.
 */
bool AminoJSObject::enqueuePropertyUpdate(int id, v8::Local<v8::Value> &value) {
    //check queue exists
    AminoJSEventObject *eventHandler = this->eventHandler;

    if (!eventHandler) {
        assert(isEventHandler());

        eventHandler = (AminoJSEventObject *)this;
    }

    //find property
    AnyProperty *prop = getPropertyWithId(id);

    if (!prop) {
        //property not found
        printf("Property with id=%i not found!\n", id);

        return false;
    }

    //enqueue (in event handler)
    if (DEBUG_BASE) {
        printf("enqueuePropertyUpdate: %s (id=%i)\n", prop->name.c_str(), id);
    }

    return eventHandler->enqueuePropertyUpdate(prop, value);
}

/**
 * Enqueue a JS property update (update JS value).
 *
 * Note: thread-safe.
 */
bool AminoJSObject::enqueueJSPropertyUpdate(AnyProperty *prop) {
    //check queue exists
    if (eventHandler) {
        return eventHandler->enqueueJSPropertyUpdate(prop);
    }

    if (DEBUG_BASE) {
        printf("Missing event handler in %s\n", name.c_str());
    }

    return false;
}

/**
 * Enqueue JS callback update.
 *
 * Note: thread-safe.
 */
bool AminoJSObject::enqueueJSCallbackUpdate(jsUpdateCallback callbackApply, jsUpdateCallback callbackDone, void *data) {
    //check queue exists
    if (eventHandler) {
        return eventHandler->enqueueJSUpdate(new JSCallbackUpdate(this, callbackApply, callbackDone, data));
    }

    if (DEBUG_BASE) {
        printf("Missing event handler in %s\n", name.c_str());
    }

    return false;
}

/**
 * Get property with id.
 */
AminoJSObject::AnyProperty* AminoJSObject::getPropertyWithId(int id) {
    std::map<int, AnyProperty *>::iterator iter = propertyMap.find(id);

    if (iter == propertyMap.end()) {
        //property not found
        return NULL;
    }

    return iter->second;
}

/**
 * Update JS property value.
 *
 * Note: has to be called on main thread.
 */
void AminoJSObject::updateProperty(std::string name, v8::Local<v8::Value> value) {
    if (DEBUG_BASE) {
        printf("updateProperty(): %s\n", name.c_str());
    }

    //get property function
    v8::Local<v8::Object> obj = handle();
    Nan::MaybeLocal<v8::Value> prop = Nan::Get(obj, Nan::New<v8::String>(name).ToLocalChecked());

    if (prop.IsEmpty()) {
        if (DEBUG_BASE) {
            printf("-> property not defined: %s\n", name.c_str());
        }

        return;
    }

    v8::Local<v8::Value> propLocal = prop.ToLocalChecked();

    if (!propLocal->IsFunction()) {
        if (DEBUG_BASE) {
            printf("-> property not a function: %s\n", name.c_str());
        }

        return;
    }

    v8::Local<v8::Function> updateFunc = propLocal.As<v8::Function>();

    //call
    int argc = 2;
    v8::Local<v8::Value> argv[] = { value, Nan::True() };

    updateFunc->Call(obj, argc, argv);

    if (DEBUG_BASE) {
        std::string str = toString(value);

        printf("-> updated property: %s to %s\n", name.c_str(), str.c_str());
    }
}

/**
 * Update a property value.
 *
 * Note: call is thread-safe.
 */
void AminoJSObject::updateProperty(AnyProperty *property) {
    //debug
    //printf("updateProperty() %s of %s\n", property->name.c_str(), name.c_str());

    assert(property);

    AminoJSEventObject *eventHandler = this->eventHandler;

    if (!eventHandler && isEventHandler()) {
        eventHandler = (AminoJSEventObject *)this;
    }

    assert(eventHandler);

    if (eventHandler->isMainThread()) {
        //create scope
        Nan::HandleScope scope;

        property->obj->updateProperty(property->name, property->toValue());
    } else {
        enqueueJSPropertyUpdate(property);
    }
}

/**
 * Convert a JS value to a string.
 */
std::string AminoJSObject::toString(v8::Local<v8::Value> &value) {
    //convert anything to a string
    v8::String::Utf8Value str(value);

    //convert it to string
    return std::string(*str);
}

/**
 * Convert a JS value to a string.
 *
 * Note: instance has to be deleted!
 */
std::string* AminoJSObject::toNewString(v8::Local<v8::Value> &value) {
    //convert anything to a string
    v8::String::Utf8Value str(value);

    //convert it to string
    return new std::string(*str);
}

//
// AminoJSObject::AnyProperty
//

/**
 * AnyProperty constructor.
 */
AminoJSObject::AnyProperty::AnyProperty(int type, AminoJSObject *obj, std::string name, int id): type(type), obj(obj), name(name), id(id) {
    //empty

    assert(obj);
}

AminoJSObject::AnyProperty::~AnyProperty() {
    //empty
}

/**
 * Retain base object instance.
 *
 * Note: has to be called on v8 thread!
 */
void AminoJSObject::AnyProperty::retain() {
    obj->retain();
}

/**
 * Release base object instance.
 *
 * Note: has to be called on v8 thread!
 */
void AminoJSObject::AnyProperty::release() {
    obj->release();
}

//
// AminoJSObject::FloatProperty
//

/**
 * FloatProperty constructor.
 */
AminoJSObject::FloatProperty::FloatProperty(AminoJSObject *obj, std::string name, int id): AnyProperty(PROPERTY_FLOAT, obj, name, id) {
    //empty
}

/**
 * FloatProperty destructor.
 */
AminoJSObject::FloatProperty::~FloatProperty() {
    //empty
}

/**
 * Update the float value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::FloatProperty::setValue(float newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::FloatProperty::toString() {
    return std::to_string(value);
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::FloatProperty::toValue() {
    return Nan::New<v8::Number>(value);
}

/**
 * Get async data representation.
 */
void* AminoJSObject::FloatProperty::getAsyncData(v8::Local<v8::Value> &value) {
    if (value->IsNumber()) {
        //double to float
        float f = value->NumberValue();
        float *res = new float[1];

        *res = f;

        return res;
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }

        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::FloatProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    value = *((float *)data);
}

/**
 * Free async data.
 */
void AminoJSObject::FloatProperty::freeAsyncData(void *data) {
    delete[] (float *)data;
}

//
// AminoJSObject::FloatArrayProperty
//

/**
 * FloatArrayProperty constructor.
 */
AminoJSObject::FloatArrayProperty::FloatArrayProperty(AminoJSObject *obj, std::string name, int id): AnyProperty(PROPERTY_FLOAT_ARRAY, obj, name, id) {
    //empty
}

/**
 * FloatProperty destructor.
 */
AminoJSObject::FloatArrayProperty::~FloatArrayProperty() {
    //empty
}

/**
 * Update the float value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::FloatArrayProperty::setValue(std::vector<float> newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::FloatArrayProperty::toString() {
    std::ostringstream ss;
    std::size_t count = value.size();

    ss << "[";

    for (unsigned int i = 0; i < count; i++) {
        if (i > 0) {
            ss << ", ";
        }

        ss << value[i];
    }

    ss << "]";

    return std::string(ss.str());
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::FloatArrayProperty::toValue() {
    v8::Local<v8::Array> arr = Nan::New<v8::Array>();
    std::size_t count = value.size();

    for (unsigned int i = 0; i < count; i++) {
        Nan::Set(arr, Nan::New<v8::Uint32>(i), Nan::New<v8::Number>(value[i]));
    }

    return arr;
}

/**
 * Get async data representation.
 */
void* AminoJSObject::FloatArrayProperty::getAsyncData(v8::Local<v8::Value> &value) {
    if (value->IsNull()) {
        return NULL;
    }

    v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(value);
    std::vector<float> *vector = new std::vector<float>();
    std::size_t count = arr->Length();

    for (std::size_t i = 0; i < count; i++) {
        vector->push_back((float)(arr->Get(i)->NumberValue()));
    }

    return vector;
}

/**
 * Apply async data.
 */
void AminoJSObject::FloatArrayProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    if (!data) {
        value.clear();
        return;
    }

    value = *((std::vector<float> *)data);
}

/**
 * Free async data.
 */
void AminoJSObject::FloatArrayProperty::freeAsyncData(void *data) {
    delete (std::vector<float> *)data;
}

//
// AminoJSObject::Int32Property
//

/**
 * Int32Property constructor.
 */
AminoJSObject::Int32Property::Int32Property(AminoJSObject *obj, std::string name, int id): AnyProperty(PROPERTY_INT32, obj, name, id) {
    //empty
}

/**
 * Int32Property destructor.
 */
AminoJSObject::Int32Property::~Int32Property() {
    //empty
}

/**
 * Update the unsigned int value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::Int32Property::setValue(int newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::Int32Property::toString() {
    return std::to_string(value);
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::Int32Property::toValue() {
    return Nan::New<v8::Int32>(value);
}

/**
 * Get async data representation.
 */
void* AminoJSObject::Int32Property::getAsyncData(v8::Local<v8::Value> &value) {
    if (value->IsNumber()) {
        //UInt32
        int i = value->Int32Value();
        int *res = new int[1];

        *res = i;

        return res;
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }

        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::Int32Property::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    value = *((int *)data);
}

/**
 * Free async data.
 */
void AminoJSObject::Int32Property::freeAsyncData(void *data) {
    delete[] (int *)data;
}

//
// AminoJSObject::UInt32Property
//

/**
 * UInt32Property constructor.
 */
AminoJSObject::UInt32Property::UInt32Property(AminoJSObject *obj, std::string name, int id): AnyProperty(PROPERTY_UINT32, obj, name, id) {
    //empty
}

/**
 * UInt32Property destructor.
 */
AminoJSObject::UInt32Property::~UInt32Property() {
    //empty
}

/**
 * Update the unsigned int value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::UInt32Property::setValue(unsigned int newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::UInt32Property::toString() {
    return std::to_string(value);
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::UInt32Property::toValue() {
    return Nan::New<v8::Uint32>(value);
}

/**
 * Get async data representation.
 */
void* AminoJSObject::UInt32Property::getAsyncData(v8::Local<v8::Value> &value) {
    if (value->IsNumber()) {
        //UInt32
        unsigned int ui = value->Uint32Value();
        unsigned int *res = new unsigned int[1];

        *res = ui;

        return res;
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }

        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::UInt32Property::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    value = *((unsigned int *)data);
}

/**
 * Free async data.
 */
void AminoJSObject::UInt32Property::freeAsyncData(void *data) {
    delete[] (unsigned int *)data;
}

//
// AminoJSObject::BooleanProperty
//

/**
 * BooleanProperty constructor.
 */
AminoJSObject::BooleanProperty::BooleanProperty(AminoJSObject *obj, std::string name, int id): AnyProperty(PROPERTY_BOOLEAN, obj, name, id) {
    //empty
}

/**
 * BooleanProperty destructor.
 */
AminoJSObject::BooleanProperty::~BooleanProperty() {
    //empty
}

/**
 * Update the float value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::BooleanProperty::setValue(bool newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::BooleanProperty::toString() {
    return value ? "true":"false";
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::BooleanProperty::toValue() {
    return Nan::New<v8::Boolean>(value);
}

/**
 * Get async data representation.
 */
void* AminoJSObject::BooleanProperty::getAsyncData(v8::Local<v8::Value> &value) {
    if (value->IsBoolean()) {
        bool b = value->BooleanValue();
        bool *res = new bool[1];

        *res = b;

        return res;
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a boolean!\n");
        }

        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::BooleanProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    value = *((bool *)data);
}

/**
 * Free async data.
 */
void AminoJSObject::BooleanProperty::freeAsyncData(void *data) {
    delete[] (bool *)data;
}

//
// AminoJSObject::Utf8Property
//

/**
 * Utf8Property constructor.
 */
AminoJSObject::Utf8Property::Utf8Property(AminoJSObject *obj, std::string name, int id): AnyProperty(PROPERTY_UTF8, obj, name, id) {
    //empty
}

/**
 * Utf8Property destructor.
 */
AminoJSObject::Utf8Property::~Utf8Property() {
    //empty
}

/**
 * Update the string value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::Utf8Property::setValue(std::string newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Update the string value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::Utf8Property::setValue(char *newValue) {
    setValue(std::string(newValue));
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::Utf8Property::toString() {
    return value;
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::Utf8Property::toValue() {
    return Nan::New<v8::String>(value).ToLocalChecked();
}

/**
 * Get async data representation.
 */
void* AminoJSObject::Utf8Property::getAsyncData(v8::Local<v8::Value> &value) {
    //convert to string
    std::string *str = AminoJSObject::toNewString(value);

    return str;
}

/**
 * Apply async data.
 */
void AminoJSObject::Utf8Property::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    value = *((std::string *)data);
}

/**
 * Free async data.
 */
void AminoJSObject::Utf8Property::freeAsyncData(void *data) {
    delete (std::string *)data;
}

//
// AminoJSObject::ObjectProperty
//

/**
 * ObjectProperty constructor.
 *
 * Note: retains the reference automatically.
 */
AminoJSObject::ObjectProperty::ObjectProperty(AminoJSObject *obj, std::string name, int id): AnyProperty(PROPERTY_OBJECT, obj, name, id) {
    //empty
}

/**
 * Utf8Property destructor.
 */
AminoJSObject::ObjectProperty::~ObjectProperty() {
    //release instance
    if (value) {
        value->release();
        value = NULL;
    }
}

/**
 * Free retained object value.
 *
 * Note: has to be called on main thread.
 */
void AminoJSObject::ObjectProperty::destroy() {
    if (value) {
        value->release();
        value = NULL;
    }
}

/**
 * Update the object value.
 *
 * Note: only updates the JS value if modified!
 *
 * !!! Attention: object must be retained and previous value must be released before calling this function!!!
 */
void AminoJSObject::ObjectProperty::setValue(AminoJSObject *newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::ObjectProperty::toString() {
    return value ? value->name.c_str():"null";
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::ObjectProperty::toValue() {
    if (value) {
        return value->handle();
    } else {
        return Nan::Null();
    }
}

/**
 * Get async data representation.
 */
void* AminoJSObject::ObjectProperty::getAsyncData(v8::Local<v8::Value> &value) {
    if (value->IsObject()) {
        v8::Local<v8::Object> jsObj = value->ToObject();
        AminoJSObject *obj = Nan::ObjectWrap::Unwrap<AminoJSObject>(jsObj);

        assert(obj);

        //retain reference on main thread
        obj->retain();

        return obj;
    } else {
        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::ObjectProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    AminoJSObject *obj = (AminoJSObject *)data;

    if (obj != value) {
        //release old instance
        if (value) {
            if (update) {
                update->releaseLater = value;
            } else {
                //on main thread
                value->release();
            }
        }

        //keep new instance
        if (obj) {
            if (update) {
                update->retainLater = obj;
            } else {
                //on main thread
                obj->retain();
            }
        }

        value = obj;
    }
}

/**
 * Free async data.
 */
void AminoJSObject::ObjectProperty::freeAsyncData(void *data) {
    AminoJSObject *obj = (AminoJSObject *)data;

    //release reference on main thread
    if (obj) {
        obj->release();
    }
}

//
// AminoJSObject::AnyAsyncUpdate
//

AminoJSObject::AnyAsyncUpdate::AnyAsyncUpdate(int type): type(type) {
    //empty
}

AminoJSObject::AnyAsyncUpdate::~AnyAsyncUpdate() {
    //empty
}

//
// AminoJSObject::AsyncValueUpdate
//

AminoJSObject::AsyncValueUpdate::AsyncValueUpdate(AminoJSObject *obj, AminoJSObject *value, asyncValueCallback callback): AnyAsyncUpdate(ASYNC_UPDATE_VALUE), obj(obj), valueObj(value), callback(callback) {
    assert(obj);

    //retain objects on main thread
    obj->retain();

    if (value) {
        value->retain();
    }

    //init (on main thread)
    if (callback) {
        (obj->*callback)(this, STATE_CREATE);
    }
}

AminoJSObject::AsyncValueUpdate::AsyncValueUpdate(AminoJSObject *obj, unsigned int value, asyncValueCallback callback): AnyAsyncUpdate(ASYNC_UPDATE_VALUE), obj(obj), valueUint32(value), callback(callback) {
    assert(obj);

    //retain objects on main thread
    obj->retain();

    //init (on main thread)
    if (callback) {
        (obj->*callback)(this, STATE_CREATE);
    }
}

AminoJSObject::AsyncValueUpdate::AsyncValueUpdate(AminoJSObject *obj, v8::Local<v8::Value> &value, void *data, asyncValueCallback callback): AnyAsyncUpdate(ASYNC_UPDATE_VALUE), obj(obj), data(data), callback(callback) {
    assert(obj);

    //retain objects on main thread
    obj->retain();

    //value
    valuePersistent = new Nan::Persistent<v8::Value>();
    valuePersistent->Reset(value);

    //init (on main thread)
    if (callback) {
        (obj->*callback)(this, STATE_CREATE);
    }
}

AminoJSObject::AsyncValueUpdate::~AsyncValueUpdate() {
    //additional retains
    if (releaseLater) {
        releaseLater->release();
    }

    //call cleanup handler
    if (callback) {
        (obj->*callback)(this, STATE_DELETE);
    }

    //release objects on main thread
    obj->release();

    if (valueObj) {
        valueObj->release();
    }

    //peristent
    if (valuePersistent) {
        valuePersistent->Reset();
        delete valuePersistent;
    }
}

/**
 * Apply async value.
 */
void AminoJSObject::AsyncValueUpdate::apply() {
    //on async thread
    if (callback) {
        (obj->*callback)(this, AsyncValueUpdate::STATE_APPLY);
    }
}

//
// AminoJSObject::JSPropertyUpdate
//

AminoJSObject::JSPropertyUpdate::JSPropertyUpdate(AnyProperty *property): AnyAsyncUpdate(ASYNC_JS_UPDATE_PROPERTY), property(property) {
    //empty
}

AminoJSObject::JSPropertyUpdate::~JSPropertyUpdate() {
    //empty
}

/**
 * Update JS property on main thread.
 */
void AminoJSObject::JSPropertyUpdate::apply() {
    property->obj->updateProperty(property->name, property->toValue());
}

//
// AminoJSObject::JSCallbackUpdate
//

AminoJSObject::JSCallbackUpdate::JSCallbackUpdate(AminoJSObject *obj, jsUpdateCallback callbackApply, jsUpdateCallback callbackDone, void *data): AnyAsyncUpdate(ASYNC_JS_UPDATE_CALLBACK), obj(obj), callbackApply(callbackApply), callbackDone(callbackDone), data(data) {
    //empty
}

AminoJSObject::JSCallbackUpdate::~JSCallbackUpdate() {
    if (callbackDone) {
        (obj->*callbackDone)(this);
    }
}

void AminoJSObject::JSCallbackUpdate::apply() {
    if (callbackApply) {
        (obj->*callbackApply)(this);
    }
}

//
// AminoJSEventObject
//

AminoJSEventObject::AminoJSEventObject(std::string name): AminoJSObject(name) {
    asyncUpdates = new std::vector<AnyAsyncUpdate *>();
    asyncDeletes = new std::vector<AnyAsyncUpdate *>();
    jsUpdates = new std::vector<AnyAsyncUpdate *>();

    //main thread
    mainThread = uv_thread_self();

    //recursive mutex needed
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    // asyncLock
    pthread_mutex_init(&asyncLock, &attr);
}

AminoJSEventObject::~AminoJSEventObject() {
    //JS updates
    handleJSUpdates();
    delete jsUpdates;

    //asyncUpdates
    clearAsyncQueue();
    delete asyncUpdates;

    //asyncDeletes
    handleAsyncDeletes();
    delete asyncDeletes;

    //mutex
    pthread_mutex_destroy(&asyncLock);
}

/**
 * Clear async updates.
 */
void AminoJSEventObject::clearAsyncQueue() {
    assert(asyncUpdates);

    std::size_t count = asyncUpdates->size();

    for (std::size_t i = 0; i < count; i++) {
        AnyAsyncUpdate *item = (*asyncUpdates)[i];

        delete item;
    }
}

/**
 * Free async updates on main thread.
 */
void AminoJSEventObject::handleAsyncDeletes() {
    assert(asyncDeletes);

    std::size_t count = asyncDeletes->size();

    if (count > 0) {
        //create scope
        Nan::HandleScope scope;

        for (std::size_t i = 0; i < count; i++) {
            AnyAsyncUpdate *item = (*asyncDeletes)[i];

            delete item;
        }

        asyncDeletes->clear();
    }
}

/**
 * Process all JS updates on main thread.
 */
void AminoJSEventObject::handleJSUpdates() {
    assert(jsUpdates);

    std::size_t count = jsUpdates->size();

    if (count > 0) {
        //create scope
        Nan::HandleScope scope;

        pthread_mutex_lock(&asyncLock);

        for (std::size_t i = 0; i < count; i++) {
            AnyAsyncUpdate *item = (*jsUpdates)[i];

            item->apply();
            delete item;
        }

        pthread_mutex_unlock(&asyncLock);

        jsUpdates->clear();
    }
}

/**
 * Check if running on main thread.
 */
bool AminoJSEventObject::isMainThread() {
    uv_thread_t thread = uv_thread_self();

    return uv_thread_equal(&mainThread, &thread);
}

/**
 * This is an event handler.
 */
bool AminoJSEventObject::isEventHandler() {
    return true;
}

/**
 * Process all queued updates.
 */
void AminoJSEventObject::processAsyncQueue() {
    if (destroyed) {
        return;
    }

    //iterate
    assert(asyncUpdates);

    pthread_mutex_lock(&asyncLock);

    for (std::size_t i = 0; i < asyncUpdates->size(); i++) {
        AnyAsyncUpdate *item = (*asyncUpdates)[i];

        assert(item);

        switch (item->type) {
            case ASYNC_UPDATE_PROPERTY:
                //property update
                {
                    AsyncPropertyUpdate *propItem = static_cast<AsyncPropertyUpdate *>(item);

                    //call local handler
                    propItem->property->obj->handleAsyncUpdate(propItem);
                }
                break;

            case ASYNC_UPDATE_VALUE:
                //custom value update
                {
                    AsyncValueUpdate *valueItem = static_cast<AsyncValueUpdate *>(item);

                    //call local handler
                    if (!valueItem->obj->handleAsyncUpdate(valueItem)) {
                        printf("unhandled async update by %s\n", valueItem->obj->getName().c_str());
                    }
                }
                break;

            default:
                printf("unknown async type: %i\n", item->type);
                break;
        }

        //free item
        asyncDeletes->push_back(item);
    }

    //clear
    asyncUpdates->clear();

    pthread_mutex_unlock(&asyncLock);
}

/**
 * Enqueue a value update.
 */
bool AminoJSEventObject::enqueueValueUpdate(AsyncValueUpdate *update) {
    if (!update || destroyed) {
        //free local objects
        if (update->obj == this) {
            delete update;
        }

        return false;
    }

    //enqueue
    if (DEBUG_BASE) {
        printf("enqueueValueUpdate\n");
    }

    assert(asyncUpdates);

    pthread_mutex_lock(&asyncLock);
    asyncUpdates->push_back(update);
    pthread_mutex_unlock(&asyncLock);

    return true;
}

/**
 * Enqueue a property update (value change from JS code).
 *
 * Note: called on main thread.
 */
bool AminoJSEventObject::enqueuePropertyUpdate(AnyProperty *prop, v8::Local<v8::Value> &value) {
    if (destroyed) {
        return false;
    }

    assert(prop);

    //create
    void *data = prop->getAsyncData(value);

    if (!data) {
        return false;
    }

    //call sync handler
    if (prop->obj->handleSyncUpdate(prop, data)) {
        if (DEBUG_BASE) {
            printf("-> sync update (value=%s)\n", toString(value).c_str());
        }

        prop->freeAsyncData(data);
        return true;
    }

    //async handling
    pthread_mutex_lock(&asyncLock);
    asyncUpdates->push_back(new AsyncPropertyUpdate(prop, data));
    pthread_mutex_unlock(&asyncLock);

    return true;
}

bool AminoJSEventObject::enqueueJSPropertyUpdate(AnyProperty *prop) {
    return enqueueJSUpdate(new JSPropertyUpdate(prop));
}

bool AminoJSEventObject::enqueueJSUpdate(AnyAsyncUpdate *update) {
    if (destroyed) {
        delete update;

        return false;
    }

    pthread_mutex_lock(&asyncLock);
    jsUpdates->push_back(update);
    pthread_mutex_unlock(&asyncLock);
}

//
// AminoJSEventObject::AsyncPropertyUpdate
//

/**
 * Constructor.
 */
AminoJSEventObject::AsyncPropertyUpdate::AsyncPropertyUpdate(AnyProperty *property, void *data): AnyAsyncUpdate(ASYNC_UPDATE_PROPERTY), property(property), data(data) {
    assert(property);

    //retain instance to target object
    property->retain();
}

/**
 * Destructor.
 *
 * Note: called on main thread.
 */
AminoJSEventObject::AsyncPropertyUpdate::~AsyncPropertyUpdate() {
    //retain/release
    if (retainLater) {
        retainLater->retain();
    }

    if (releaseLater) {
        releaseLater->release();
    }

    //free data
    property->freeAsyncData(data);

    //release instance to target object
    property->release();
}

/**
 * Set new value.
 */
void AminoJSEventObject::AsyncPropertyUpdate::apply() {
    property->setAsyncData(this, data);
}
