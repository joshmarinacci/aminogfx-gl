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

    //free properties
    for (std::map<int, AnyProperty *>::iterator iter = propertyMap.begin(); iter != propertyMap.end(); iter++) {
        delete iter->second;
    }

    //free async updates
    if (localAsyncUpdatesInstance) {
        delete asyncUpdates;
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
 * Retain JS reference.
 */
void AminoJSObject::retain() {
    Ref();
}

/**
 * Release JS reference.
 */
void AminoJSObject::release() {
    Unref();
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
            printf("-> property not an object: %s\n", name.c_str());
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

//TODO int
//TODO string
/*
        if (value->IsString()) {
            v8::String::Utf8Value str(value);
            std::string stdStr(*str);

            jsValue = stdStr;
*/

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

void AminoJSObject::enqueuePropertyUpdate(int id, v8::Local<v8::Value> value) {
    //check queue exists
    if (!asyncUpdates) {
        printf("missing queue: %s\n", name.c_str());

        return;
    }

    //find property
    std::map<int, AnyProperty *>::iterator iter = propertyMap.find(id);

    if (iter == propertyMap.end()) {
        //property not found
        printf("property with id=%i not found!\n", id);

        return;
    }

    //enqueue
    AnyProperty *prop = iter->second;

    if (DEBUG_BASE) {
        printf("enqueuePropertyUpdate: %s (id=%i)\n", prop->name.c_str(), id);
    }

    asyncUpdates->push_back(new AsyncUpdate(prop, value));
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
void AminoJSObject::updateProperty(std::string name, int value) {
    //create scope
    Nan::HandleScope scope;

    updateProperty(name, Nan::New<v8::Integer>(value));
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
 * Create async updates queue.
 */
void AminoJSObject::createAsyncQueue() {
    if (!asyncUpdates) {
        asyncUpdates = new std::vector<AsyncUpdate *>();
        localAsyncUpdatesInstance = true;
    }
}

/**
 * Bind to global queue.
 */
void AminoJSObject::attachToAsyncQueue(AminoJSObject *obj) {
    //detach
    if (asyncUpdates && localAsyncUpdatesInstance) {
        delete asyncUpdates;
    }

    asyncUpdates = obj->asyncUpdates;
    localAsyncUpdatesInstance = false;
}

/**
 * Unbind from global queue.
 */
void AminoJSObject::detachFromAsyncQueue() {
    if (!localAsyncUpdatesInstance) {
        asyncUpdates = NULL;
    }
}

/**
 * Process all queued updates.
 */
void AminoJSObject::processAsyncQueue() {
    //check if global queue
    if (!asyncUpdates || !localAsyncUpdatesInstance) {
        return;
    }

    //create scope
    Nan::HandleScope scope;

    //iterate
    for (std::size_t i = 0; i < asyncUpdates->size(); i++) {
        AsyncUpdate *item = (*asyncUpdates)[i];

        handleAsyncUpdate(item->property, item->getValue());
    }

    //clear
    asyncUpdates->clear();
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

//
// AminoJSObject::AnyProperty
//

/**
 * AnyProperty constructor.
 */
AminoJSObject::AnyProperty::AnyProperty(AminoJSObject *obj, std::string name, int id): obj(obj), name(name), id(id) {
    //empty
}

AminoJSObject::AnyProperty::~AnyProperty() {
    //empty
}

/**
 * Retain base object instance.
 */
void AminoJSObject::AnyProperty::retain() {
    obj->Ref();
}

/**
 * Release base object instance.
 */
void AminoJSObject::AnyProperty::release() {
    obj->Unref();
}

//
// AminoJSObject::FloatProperty
//

/**
 * FloatProperty constructor.
 */
AminoJSObject::FloatProperty::FloatProperty(AminoJSObject *obj, std::string name, int id): AnyProperty(obj, name, id) {
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
// AminoJSObject::BooleanProperty
//

/**
 * BooleanProperty constructor.
 */
AminoJSObject::BooleanProperty::BooleanProperty(AminoJSObject *obj, std::string name, int id): AnyProperty(obj, name, id) {
    //empty
}

/**
 * FloatProperty destructor.
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
// AminoJSObject::AsyncUpdate
//

/**
 * Constructor.
 */
AminoJSObject::AsyncUpdate::AsyncUpdate(AnyProperty *property, v8::Local<v8::Value> value): property(property) {
    //store persistent reference
    this->value.Reset(value);

    //retain instance
    property->retain();
}

/**
 * Destructor.
 */
AminoJSObject::AsyncUpdate::~AsyncUpdate() {
    //free persistent reference
    value.Reset();

    //release instance
    property->release();
}

/**
 * Get local value.
 */
v8::Local<v8::Value> AminoJSObject::AsyncUpdate::getValue() {
    return Nan::New(value);
}
