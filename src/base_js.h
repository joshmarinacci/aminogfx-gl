#ifndef _AMINOBASE_JS_H
#define _AMINOBASE_JS_H

#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <map>
#include <memory>

#define ASYNC_UPDATE_PROPERTY   0
#define ASYNC_UPDATE_VALUE      1
#define ASYNC_UPDATE_CUSTOM   100

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

    virtual void preInit(Nan::NAN_METHOD_ARGS_TYPE info);
    virtual void setup();

    //properties
    bool addPropertyWatcher(std::string name, int id, v8::Local<v8::Value> &jsValue);

    void updateProperty(std::string name, v8::Local<v8::Value> value);
    void updateProperty(std::string name, double value);
    void updateProperty(std::string name, int value);
    void updateProperty(std::string name, bool value);
    void updateProperty(std::string name, std::string value);

    class AnyProperty {
    public:
        AminoJSObject *obj;
        std::string name;
        int id;
        bool connected = false;

        AnyProperty(AminoJSObject *obj, std::string name, int id);
        virtual ~AnyProperty();

        virtual void setValue(v8::Local<v8::Value> &value) = 0;

        //weak reference control
        void retain();
        void release();
    };

    class FloatProperty : public AnyProperty {
    public:
        float value = 0;

        FloatProperty(AminoJSObject *obj, std::string name, int id);
        ~FloatProperty();

        void setValue(v8::Local<v8::Value> &value) override;
        void setValue(float newValue);
    };

    class BooleanProperty : public AnyProperty {
    public:
        bool value = false;

        BooleanProperty(AminoJSObject *obj, std::string name, int id);
        ~BooleanProperty();

        void setValue(v8::Local<v8::Value> &value) override;
        void setValue(bool newValue);
    };

    class Utf8Property : public AnyProperty {
    public:
        std::string value;

        Utf8Property(AminoJSObject *obj, std::string name, int id);
        ~Utf8Property();

        void setValue(v8::Local<v8::Value> &value) override;
        void setValue(std::string newValue);
        void setValue(char *newValue);
    };

    FloatProperty* createFloatProperty(std::string name);
    BooleanProperty* createBooleanProperty(std::string name);
    Utf8Property* createUtf8Property(std::string name);

    //async updates
    void createAsyncQueue();
    void attachToAsyncQueue(AminoJSObject *obj);
    void detachFromAsyncQueue();

    void processAsyncQueue();
    virtual void handleAsyncUpdate(AnyProperty *property, v8::Local<v8::Value> value);

    class AnyAsyncUpdate {
    public:
        int type;

        AnyAsyncUpdate(int type);
        virtual ~AnyAsyncUpdate();
    };

    class AsyncValueUpdate : public AnyAsyncUpdate {
    public:
        int id;
        AminoJSObject *obj;
        AminoJSObject *valueObj = NULL;

        AsyncValueUpdate(int id, AminoJSObject *obj, AminoJSObject *value);
        ~AsyncValueUpdate();
    };

    bool enqueueValueUpdate(int id, AminoJSObject *value);
    bool enqueueValueUpdate(AsyncValueUpdate *update);

    virtual bool handleAsyncUpdate(AsyncValueUpdate *update);

    //static methods
    static v8::Local<v8::FunctionTemplate> createTemplate(AminoJSObjectFactory* factory);
    static void createInstance(Nan::NAN_METHOD_ARGS_TYPE info, AminoJSObjectFactory* factory);

private:
    //properties
    int lastPropertyId = 0;
    std::map<int, AnyProperty *> propertyMap;

    void addProperty(AnyProperty *prop);
    void enqueuePropertyUpdate(int id, v8::Local<v8::Value> value);
    static NAN_METHOD(PropertyUpdated);

    //async updates
    class AsyncPropertyUpdate : public AnyAsyncUpdate {
    public:
        AnyProperty *property;

        AsyncPropertyUpdate(AnyProperty *property, v8::Local<v8::Value> value);
        ~AsyncPropertyUpdate();

        v8::Local<v8::Value> getValue();

    private:
        Nan::Persistent<v8::Value> value;
    };

    std::vector<AnyAsyncUpdate *> *asyncUpdates = NULL;
    bool localAsyncUpdatesInstance = false;

public:

    std::string getName();

    void retain();
    void release();
};

#endif