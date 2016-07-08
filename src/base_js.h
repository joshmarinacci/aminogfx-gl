#ifndef _AMINOBASE_JS_H
#define _AMINOBASE_JS_H

#include <node.h>
#include <node_buffer.h>
#include <nan.h>

//FIXME
#define DEBUG_BASE true
#define DEBUG_RESOURCES true

class AminoJSObject;

/**
 * Factory object to create JS instance.
 */
class AminoJSObjectFactory {
public:
    std::string name;
    Nan::FunctionCallback callback;

    AminoJSObjectFactory(std::string name, Nan::FunctionCallback callback);

    virtual AminoJSObject* create();
};

/**
 * Basic JS object wrapper for Amino classes.
 *
 * Note: abstract.
 */
class AminoJSObject : public Nan::ObjectWrap {
protected:
    std::string name;

    AminoJSObject(std::string name);
    virtual ~AminoJSObject();

    virtual void setup();

    //properties
    bool addPropertyWatcher(std::string name, Nan::FunctionCallback callback);
    void updateProperty(std::string name, v8::Local<v8::Value> value);
    void updateProperty(std::string name, int value);

    //static methods
    static v8::Local<v8::FunctionTemplate> createTemplate(AminoJSObjectFactory* factory);
    static void createInstance(Nan::NAN_METHOD_ARGS_TYPE info, AminoJSObjectFactory* factory);

public:
    /**
     * Get JS class name.
     *
     * Note: abstract
     */
    std::string getName() {
        return name;
    }
};

#endif