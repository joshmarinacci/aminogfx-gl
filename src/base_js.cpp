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
        return Nan::ThrowTypeError("please use new() instead of function call");
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
