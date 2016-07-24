#include "base_js.h"

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

    if (eventHandler) {
        eventHandler->release();
        eventHandler = NULL;
    }
}

/**
 * Retain JS reference.
 */
void AminoJSObject::retain() {
    Ref();

    if (DEBUG_REFERENCES) {
        printf("--- %s referenes: %i (+1)\n", name.c_str(), refs_);
    }
}

/**
 * Release JS reference.
 */
void AminoJSObject::release() {
    Unref();

    if (DEBUG_REFERENCES) {
        printf("--- %s referenes: %i (-1)\n", name.c_str(), refs_);
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

    //create scope
    Nan::HandleScope scope;

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

void AminoJSObject::addProperty(AnyProperty *prop) {
    int id = prop->id;
    propertyMap.insert(std::pair<int, AnyProperty *>(id, prop));

    v8::Local<v8::Value> value;

    if (addPropertyWatcher(prop->name, id, value)) {
        prop->connected = true;

        //set default value
        prop->setValue(value);
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

    obj->enqueuePropertyUpdate(id, value);
}

/**
 * Set the event handler instance.
 *
 * Note: retains the object instance.
 */
void AminoJSObject::setEventHandler(AminoJSEventObject *handler) {
    if (this->eventHandler != handler) {
        if (this->eventHandler) {
            this->eventHandler->release();
        }

        this->eventHandler = handler;
        handler->retain();
    }
}

/**
 * This is not an event handeler.
 */
bool AminoJSObject::isEventHandler() {
    return false;
}

/**
 * Enqueue a value update.
 */
bool AminoJSObject::enqueueValueUpdate(AminoJSObject *value, asyncValueCallback callback) {
    return enqueueValueUpdate(new AsyncValueUpdate(this, value, callback));
}

/**
 * Enqueue a value update.
 */
bool AminoJSObject::enqueueValueUpdate(unsigned int value, asyncValueCallback callback) {
    return enqueueValueUpdate(new AsyncValueUpdate(this, value, callback));
}

/**
 * Enqueue a value update.
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
 * Default implementation sets the value received from JS.
 */
void AminoJSObject::handleAsyncUpdate(AnyProperty *property, v8::Local<v8::Value> value) {
    //overwrite for extended handling

    //default: update value
    property->setValue(value);

    if (DEBUG_BASE) {
        v8::String::Utf8Value str(value);

        printf("-> updated %s in %s to %s\n", property->name.c_str(), property->obj->name.c_str(), *str);
    }
}

/**
 * Custom handler for implementation specific async update.
 */
bool AminoJSObject::handleAsyncUpdate(AsyncValueUpdate *update) {
    //overwrite

    if (update->callback) {
        (this->*update->callback)(update);

        return true;
    }

    return false;
}

/**
 * Enqueue a property update.
 */
bool AminoJSObject::enqueuePropertyUpdate(int id, v8::Local<v8::Value> value) {
    //check queue exists
    AminoJSEventObject *eventHandler = this->eventHandler;

    if (!eventHandler) {
        if (!isEventHandler()) {
            printf("missing queue: %s\n", name.c_str());

            return false;
        }

        eventHandler = (AminoJSEventObject *)this;
    }

    //find property
    AnyProperty *prop = getPropertyWithId(id);

    if (!prop) {
        //property not found
        printf("property with id=%i not found!\n", id);

        return false;
    }

    //enqueue
    if (DEBUG_BASE) {
        printf("enqueuePropertyUpdate: %s (id=%i)\n", prop->name.c_str(), id);
    }

    return eventHandler->enqueuePropertyUpdate(prop, value);
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
 */
void AminoJSObject::updateProperty(std::string name, v8::Local<v8::Value> value) {
    if (DEBUG_BASE) {
        printf("updateProperty(): %s\n", name.c_str());
    }

    //create scope
    Nan::HandleScope scope;

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
        printf("-> updated property: %s\n", name.c_str());
    }
}

/**
 * Update JS property value.
 */
void AminoJSObject::updateProperty(std::string name, double value) {
    //create scope
    Nan::HandleScope scope;

    updateProperty(name, Nan::New<v8::Number>(value));
}

/**
 * Update JS property value.
 */
void AminoJSObject::updateProperty(std::string name, std::vector<float> value) {
    //create scope
    Nan::HandleScope scope;

    v8::Local<v8::Array> arr = Nan::New<v8::Array>();
    std::size_t count = value.size();

    for (unsigned int i = 0; i < count; i++) {
        Nan::Set(arr, Nan::New<v8::Uint32>(i), Nan::New<v8::Number>(value[i]));
    }

    updateProperty(name, arr);
}

/**
 * Update JS property value.
 */
void AminoJSObject::updateProperty(std::string name, int value) {
    //create scope
    Nan::HandleScope scope;

    updateProperty(name, Nan::New<v8::Int32>(value));
}

/**
 * Update JS property value.
 */
void AminoJSObject::updateProperty(std::string name, unsigned int value) {
    //create scope
    Nan::HandleScope scope;

    updateProperty(name, Nan::New<v8::Uint32>(value));
}

/**
 * Update JS property value.
 */
void AminoJSObject::updateProperty(std::string name, bool value) {
    //create scope
    Nan::HandleScope scope;

    updateProperty(name, Nan::New<v8::Boolean>(value));
}

/**
 * Update JS property value.
 */
void AminoJSObject::updateProperty(std::string name, std::string value) {
    //create scope
    Nan::HandleScope scope;

    updateProperty(name, Nan::New<v8::String>(value).ToLocalChecked());
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

//
// AminoJSObject::AnyProperty
//

/**
 * AnyProperty constructor.
 */
AminoJSObject::AnyProperty::AnyProperty(int type, AminoJSObject *obj, std::string name, int id): type(type), obj(obj), name(name), id(id) {
    //empty
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
 * Set value.
 */
void AminoJSObject::FloatProperty::setValue(v8::Local<v8::Value> &value) {
    if (value->IsNumber()) {
        //double to float
        this->value = value->NumberValue();

        //Note: do not call updateProperty(), change is from JS side
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }
    }
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
            obj->updateProperty(name, value);
        }
    }
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
 * Set value.
 */
void AminoJSObject::FloatArrayProperty::setValue(v8::Local<v8::Value> &value) {
    v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(value);

    this->value.clear();

    std::size_t count = arr->Length();

    for (std::size_t i = 0; i < count; i++) {
        this->value.push_back((float)(arr->Get(i)->NumberValue()));
    }
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
            obj->updateProperty(name, value);
        }
    }
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
 * Set value.
 */
void AminoJSObject::Int32Property::setValue(v8::Local<v8::Value> &value) {
    if (value->IsNumber()) {
        //UInt32
        this->value = value->Int32Value();

        //Note: do not call updateProperty(), change is from JS side
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }
    }
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
            obj->updateProperty(name, value);
        }
    }
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
 * Set value.
 */
void AminoJSObject::UInt32Property::setValue(v8::Local<v8::Value> &value) {
    if (value->IsNumber()) {
        //UInt32
        this->value = value->Uint32Value();

        //Note: do not call updateProperty(), change is from JS side
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }
    }
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
            obj->updateProperty(name, value);
        }
    }
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
 * Set value.
 */
void AminoJSObject::BooleanProperty::setValue(v8::Local<v8::Value> &value) {
    if (value->IsBoolean()) {
        this->value = value->BooleanValue();

        //Note: do not call updateProperty(), change is from JS side
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a boolean!\n");
        }
    }
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
            obj->updateProperty(name, value);
        }
    }
}

//
// AminoJSObject::BooleanProperty
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
 * Set value.
 */
void AminoJSObject::Utf8Property::setValue(v8::Local<v8::Value> &value) {
    //convert to string
    this->value = AminoJSObject::toString(value);
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
            obj->updateProperty(name, value);
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

//
// AminoJSObject::ObjectProperty
//

/**
 * ObjectProperty constructor.
 */
AminoJSObject::ObjectProperty::ObjectProperty(AminoJSObject *obj, std::string name, int id): AnyProperty(PROPERTY_OBJECT, obj, name, id) {
    //empty
}

/**
 * Utf8Property destructor.
 */
AminoJSObject::ObjectProperty::~ObjectProperty() {
    value.Reset();
}

/**
 * Set value.
 */
void AminoJSObject::ObjectProperty::setValue(v8::Local<v8::Value> &value) {
    if (value->IsObject()) {
        this->value.Reset(value->ToObject());
    } else {
        this->value.Reset();
    }
}

/**
 * Update the object value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::ObjectProperty::setValue(v8::Local<v8::Object> &newValue) {
    v8::Local<v8::Object> value = Nan::New(this->value);

    if (value != newValue) {
        this->value.Reset(newValue);

        if (connected) {
            obj->updateProperty(name, newValue);
        }
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
    obj->retain();

    if (value) {
        value->retain();
    }
}

AminoJSObject::AsyncValueUpdate::AsyncValueUpdate(AminoJSObject *obj, unsigned int value, asyncValueCallback callback): AnyAsyncUpdate(ASYNC_UPDATE_VALUE), obj(obj), valueUint32(value), callback(callback) {
    obj->retain();
}

AminoJSObject::AsyncValueUpdate::~AsyncValueUpdate() {
    obj->release();

    if (valueObj) {
        valueObj->release();
    }
}

//
// AminoJSEventObject
//

AminoJSEventObject::AminoJSEventObject(std::string name): AminoJSObject(name) {
    asyncUpdates = new std::vector<AnyAsyncUpdate *>();

    //recursive mutex needed
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&asyncLock, &attr);
}

AminoJSEventObject::~AminoJSEventObject() {
    std::size_t count = asyncUpdates->size();

    for (std::size_t i = 0; i < count; i++) {
        AnyAsyncUpdate *item = (*asyncUpdates)[i];

        delete item;
    }

    delete asyncUpdates;
    pthread_mutex_destroy(&asyncLock);
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

    //create scope
    Nan::HandleScope scope;

    //iterate
    pthread_mutex_lock(&asyncLock);

    for (std::size_t i = 0; i < asyncUpdates->size(); i++) {
        AnyAsyncUpdate *item = (*asyncUpdates)[i];

        switch (item->type) {
            case ASYNC_UPDATE_PROPERTY:
                //property update
                {
                    AsyncPropertyUpdate *propItem = static_cast<AsyncPropertyUpdate *>(item);

                    propItem->property->obj->handleAsyncUpdate(propItem->property, propItem->getValue());
                }
                break;

            case ASYNC_UPDATE_VALUE:
                //custom value update
                {
                    AsyncValueUpdate *valueItem = static_cast<AsyncValueUpdate *>(item);

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
        delete item;
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
        return false;
    }

    //enqueue
    if (DEBUG_BASE) {
        printf("enqueueValueUpdate\n");
    }

    pthread_mutex_lock(&asyncLock);
    asyncUpdates->push_back(update);
    pthread_mutex_unlock(&asyncLock);

    return true;
}

/**
 * Enqueue a property update.
 */
bool AminoJSEventObject::enqueuePropertyUpdate(AnyProperty *prop, v8::Local<v8::Value> value) {
    if (destroyed) {
        return false;
    }

    pthread_mutex_lock(&asyncLock);
    asyncUpdates->push_back(new AsyncPropertyUpdate(prop, value));
    pthread_mutex_unlock(&asyncLock);

    return true;
}

//
// AminoJSObject::AsyncPropertyUpdate
//

/**
 * Constructor.
 */
AminoJSEventObject::AsyncPropertyUpdate::AsyncPropertyUpdate(AnyProperty *property, v8::Local<v8::Value> value): AnyAsyncUpdate(ASYNC_UPDATE_PROPERTY), property(property) {
    //store persistent reference
    this->value.Reset(value);

    //retain instance
    property->retain();
}

/**
 * Destructor.
 */
AminoJSEventObject::AsyncPropertyUpdate::~AsyncPropertyUpdate() {
    //free persistent reference
    value.Reset();

    //release instance
    property->release();
}

/**
 * Get local value.
 */
v8::Local<v8::Value> AminoJSEventObject::AsyncPropertyUpdate::getValue() {
    return Nan::New(value);
}
