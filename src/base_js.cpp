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
 */
bool AminoJSObject::addPropertyWatcher(std::string name, Nan::FunctionCallback callback) {
    if (DEBUG_BASE) {
        printf("addPropertyWatcher()\n", name.c_str());
    }

    //create scope
    Nan::HandleScope scope;

    //get property object
    Nan::MaybeLocal<v8::Value> prop = Nan::Get(handle(), Nan::New<v8::String>(name).ToLocalChecked());

    if (prop.IsEmpty()) {
        if (DEBUG_BASE) {
            printf("property not defined: %s\n", name.c_str());
        }

        return false;
    }

    v8::Local<v8::Value> propLocal = prop.ToLocalChecked();

    if (!propLocal->IsObject()) {
        if (DEBUG_BASE) {
            printf("property not an object: %s\n", name.c_str());
        }

        return false;
    }

    v8::Local<v8::Object> obj = propLocal.As<v8::Object>();

    //get watch function
    Nan::MaybeLocal<v8::Value> propWatch = Nan::Get(obj, Nan::New<v8::String>("watch").ToLocalChecked());

    if (propWatch.IsEmpty()) {
        if (DEBUG_BASE) {
            printf("watch property not found: %s\n", name.c_str());
        }

        return false;
    }

    v8::Local<v8::Value> propWatchLocal = propWatch.ToLocalChecked();

    if (!propWatchLocal->IsFunction()) {
        if (DEBUG_BASE) {
            printf("watch property not a function: %s\n", name.c_str());
        }

        return false;
    }

    v8::Local<v8::Function> watchFunc = propWatchLocal.As<v8::Function>();

    //call to add watcher
    int argc = 1;
    //FIXME wrong -> trap in later execution: Assertion failed: (object->InternalFieldCount() > 0), function Unwrap, file ../node_modules/nan/nan_object_wrap.h, line 33.
    v8::Local<v8::Value> argv[] = { Nan::GetFunction(Nan::New<v8::FunctionTemplate>(callback)).ToLocalChecked() };

    watchFunc->Call(obj, argc, argv);

    if (DEBUG_BASE) {
        printf("added property watcher: %s\n", name.c_str());
    }

    //TODO return default value

    return true;
}

/**
 * Update JS property value.
 */
void AminoJSObject::updateProperty(std::string name, v8::Local<v8::Value> value) {
    if (DEBUG_BASE) {
        printf("updateProperty()\n", name.c_str());
    }

    //create scope
    Nan::HandleScope scope;

    //get property function
    v8::Local<v8::Object> obj = handle();
    Nan::MaybeLocal<v8::Value> prop = Nan::Get(obj, Nan::New<v8::String>(name).ToLocalChecked());

    if (prop.IsEmpty()) {
        if (DEBUG_BASE) {
            printf("property not defined: %s\n", name.c_str());
        }

        return;
    }

    v8::Local<v8::Value> propLocal = prop.ToLocalChecked();

    if (!propLocal->IsFunction()) {
        if (DEBUG_BASE) {
            printf("property not a function: %s\n", name.c_str());
        }

        return;
    }

    v8::Local<v8::Function> updateFunc = propLocal.As<v8::Function>();

    //call
    int argc = 1;
    v8::Local<v8::Value> argv[] = { value };

    updateFunc->Call(obj, argc, argv);

    if (DEBUG_BASE) {
        printf("updated property: %s\n", name.c_str());
    }
}

/**
 * Update JS property value.
 */
void AminoJSObject::updateProperty(std::string name, int value) {
    //create scope
    Nan::HandleScope scope;

    updateProperty(name, Nan::New(value));
}

//TODO getProperty(std::string name, Nan::Persistent<v8::Object> &persistent)
//TODO updateProperty(Nan::Persistent<v8::Object> &persistent, v8::Local<v8::Value> value)