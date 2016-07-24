#include "fonts.h"

#include <cmath>

//
// AminoFonts
//

AminoFonts::AminoFonts(): AminoJSObject(getFactory()->name) {
    //empty
}

AminoFonts::~AminoFonts() {
    //empty
}

/**
 * Get factory instance.
 */
AminoFontsFactory* AminoFonts::getFactory() {
    static AminoFontsFactory *aminoFontsFactory;

    if (!aminoFontsFactory) {
        aminoFontsFactory = new AminoFontsFactory(New);
    }

    return aminoFontsFactory;
}

/**
 * Add class template to module exports.
 */
NAN_MODULE_INIT(AminoFonts::Init) {
    AminoFontsFactory *factory = getFactory();
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(factory);

    //prototype methods

    // -> no native methods
    Nan::SetTemplate(tpl, "Font", AminoFont::GetInitFunction());
    Nan::SetTemplate(tpl, "FontSize", AminoFontSize::GetInitFunction());

    //global template instance
    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoFonts::New) {
    AminoJSObject::createInstance(info, getFactory());
}

//
//  AminoFontsFactory
//

/**
 * Create AminoFonts factory.
 */
AminoFontsFactory::AminoFontsFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoFonts", callback) {
    //empty
}

/**
 * Create AminoFonts instance.
 */
AminoJSObject* AminoFontsFactory::create() {
    return new AminoFonts();
}

//
// AminoFont
//

AminoFont::AminoFont(): AminoJSObject(getFactory()->name) {
    //empty
}

AminoFont::~AminoFont() {
    //empty
}

void AminoFont::destroy() {
    AminoJSObject::destroy();

    if (atlas) {
        //Note: frees all sizes too
        texture_atlas_delete(atlas);
    }

    fontData.Reset();

    fontSizes.clear();
}

/**
 * Get factory instance.
 */
AminoFontFactory* AminoFont::getFactory() {
    static AminoFontFactory *aminoFontFactory;

    if (!aminoFontFactory) {
        aminoFontFactory = new AminoFontFactory(New);
    }

    return aminoFontFactory;
}

/**
 * Initialize Group template.
 */
v8::Local<v8::Function> AminoFont::GetInitFunction() {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

    //no methods

    //template function
    return Nan::GetFunction(tpl).ToLocalChecked();
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoFont::New) {
    AminoJSObject::createInstance(info, getFactory());
}

void AminoFont::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    AminoFonts *parent = Nan::ObjectWrap::Unwrap<AminoFonts>(info[0]->ToObject());
    v8::Local<v8::Object> fontData = info[1]->ToObject();

    this->parent = parent;

    //store font (in memory)
    v8::Local<v8::Object> bufferObj =  Nan::Get(fontData, Nan::New<v8::String>("data").ToLocalChecked()).ToLocalChecked()->ToObject();

    this->fontData.Reset(bufferObj);

    //create atlas
    atlas = texture_atlas_new(512, 512, 1);

    if (!atlas) {
        Nan::ThrowTypeError("could not create atlas");
        return;
    }

    //metadata
    v8::Local<v8::Value> nameValue = Nan::Get(fontData, Nan::New<v8::String>("name").ToLocalChecked()).ToLocalChecked();
    v8::Local<v8::Value> styleValue = Nan::Get(fontData, Nan::New<v8::String>("style").ToLocalChecked()).ToLocalChecked();

    name = AminoJSObject::toString(nameValue);
    weight = Nan::Get(fontData, Nan::New<v8::String>("weight").ToLocalChecked()).ToLocalChecked()->NumberValue();
    style = AminoJSObject::toString(styleValue);
}

/**
 * Load font size.
 *
 * Note: has to be called in v8 thread.
 */
texture_font_t *AminoFont::getFontWithSize(int size) {
    //check cache
    std::map<int, texture_font_t *>::iterator it = fontSizes.find(size);
    texture_font_t *fontSize;

    if (it == fontSizes.end()) {
        //add new size
        v8::Local<v8::Object> bufferObj = Nan::New(fontData);
        char *buffer = node::Buffer::Data(bufferObj);
        size_t bufferLen = node::Buffer::Length(bufferObj);

        fontSize = texture_font_new_from_memory(atlas, size, buffer, bufferLen);
        fontSizes[size] = fontSize;
    } else {
        fontSize = it->second;
    }

    return fontSize;
}

//
//  AminoFontFactory
//

/**
 * Create AminoFont factory.
 */
AminoFontFactory::AminoFontFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoFont", callback) {
    //empty
}

/**
 * Create AminoFonts instance.
 */
AminoJSObject* AminoFontFactory::create() {
    return new AminoFont();
}

//
// AminoFontSize
//

AminoFontSize::AminoFontSize(): AminoJSObject(getFactory()->name) {
    //empty
}

AminoFontSize::~AminoFontSize() {
    //empty
}

/**
 * Get factory instance.
 */
AminoFontSizeFactory* AminoFontSize::getFactory() {
    static AminoFontSizeFactory *aminoFontSizeFactory;

    if (!aminoFontSizeFactory) {
        aminoFontSizeFactory = new AminoFontSizeFactory(New);
    }

    return aminoFontSizeFactory;
}

/**
 * Initialize Group template.
 */
v8::Local<v8::Function> AminoFontSize::GetInitFunction() {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

    //methods
    /* TODO cbx
    Nan::SetPrototypeMethod(tpl, "calcWidth", Start);
    Nan::SetPrototypeMethod(tpl, "getHeight", Stop);
    Nan::SetPrototypeMethod(tpl, "getMetrics", Stop);
    */

    //template function
    return Nan::GetFunction(tpl).ToLocalChecked();
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoFontSize::New) {
    AminoJSObject::createInstance(info, getFactory());
}

void AminoFontSize::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    AminoFont *parent = Nan::ObjectWrap::Unwrap<AminoFont>(info[0]->ToObject());
    int size = (int)round(info[1]->NumberValue());

    this->parent = parent;
    fontTexture = parent->getFontWithSize(size);

    if (!fontTexture) {
        Nan::ThrowTypeError("could not create font size");
    }

    //metrics
    v8::Local<v8::Object> obj = handle();

    Nan::Set(obj, Nan::New("name").ToLocalChecked(), Nan::New<v8::String>(parent->name).ToLocalChecked());
    Nan::Set(obj, Nan::New("size").ToLocalChecked(), Nan::New<v8::Number>(size));
    Nan::Set(obj, Nan::New("weight").ToLocalChecked(), Nan::New<v8::Number>(parent->weight));
    Nan::Set(obj, Nan::New("style").ToLocalChecked(), Nan::New<v8::String>(parent->style).ToLocalChecked());

    //cbx metrics
}

//
//  AminoFontSizeFactory
//

/**
 * Create AminoFontSize factory.
 */
AminoFontSizeFactory::AminoFontSizeFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoFontSize", callback) {
    //empty
}

/**
 * Create AminoFonts instance.
 */
AminoJSObject* AminoFontSizeFactory::create() {
    return new AminoFontSize();
}
