#ifndef _AMINOFONTS_H
#define _AMINOFONTS_H

#include "freetype-gl.h"
#include "vertex-buffer.h"

#include <map>

#include "base_js.h"
#include "gfx.h"
#include "shaders.h"

class AminoFontsFactory;

/**
 * AminoFonts class.
 */
class AminoFonts : public AminoJSObject {
public:
    AminoFonts();
    ~AminoFonts();

    //creation
    static AminoFontsFactory* getFactory();

    //init
    static NAN_MODULE_INIT(Init);

private:
    //JS constructor
    static NAN_METHOD(New);
};

/**
 * AminoFonts class factory.
 */
class AminoFontsFactory : public AminoJSObjectFactory {
public:
    AminoFontsFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

class AminoFontFactory;

/**
 * AminoFont class.
 */
class AminoFont : public AminoJSObject {
public:
    std::string fontName;
    int fontWeight;
    std::string fontStyle;

    AminoFont();
    ~AminoFont();

    texture_font_t *getFontWithSize(int size);
    std::string getFontInfo();

    //creation
    static AminoFontFactory* getFactory();

    //init
    static v8::Local<v8::Function> GetInitFunction();

private:
    //Note: instance kept
    static FT_Library library;

    //JS constructor
    static NAN_METHOD(New);

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override;

protected:
    AminoFonts *fonts = NULL;
    texture_atlas_t *atlas = NULL;
    Nan::Persistent<v8::Object> fontData;
    std::map<int, texture_font_t *> fontSizes;

    void destroy() override;
};

/**
 * AminoFont class factory.
 */
class AminoFontFactory : public AminoJSObjectFactory {
public:
    AminoFontFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

class AminoFontSizeFactory;

/**
 * AminoFontSize class.
 */
class AminoFontSize : public AminoJSObject {
public:
    texture_font_t *fontTexture = NULL;
    AminoFont *font = NULL;

    AminoFontSize();
    ~AminoFontSize();

    float getTextWidth(const char *text);

    //creation
    static AminoFontSizeFactory* getFactory();

    //init
    static v8::Local<v8::Function> GetInitFunction();

private:
    //JS constructor
    static NAN_METHOD(New);

    //JS methods
    static NAN_METHOD(CalcTextWidth);
    static NAN_METHOD(GetFontMetrics);

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override;
};

/**
 * AminoFontSize class factory.
 */
class AminoFontSizeFactory : public AminoJSObjectFactory {
public:
    AminoFontSizeFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Atlas texture (per AminoGfx instance).
 */
struct amino_atlas_t {
    GLuint textureId;
};

/**
 * Font Shader.
 */
class AminoFontShader : public TextureShader {
public:
    AminoFontShader();

    void setColor(GLfloat color[3]);

    amino_atlas_t getAtlasTexture(texture_atlas_t *atlas, bool createIfMissing);

protected:
    GLint uColor;

    //textures (Note: never destroyed)
    std::map<texture_atlas_t *, amino_atlas_t> atlasTextures;

    void initShader() override;
};

#endif