#ifndef _AMINOFONTS_H
#define _AMINOFONTS_H

#include "freetype-gl.h"
#include "vertex-buffer.h"

#include <map>

#include "base_js.h"
#include "gfx.h"

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
    std::string name;
    int weight;
    std::string style;

    AminoFont();
    ~AminoFont();

    texture_font_t *getFontWithSize(int size);

    //creation
    static AminoFontFactory* getFactory();

    //init
    static v8::Local<v8::Function> GetInitFunction();

private:
    //JS constructor
    static NAN_METHOD(New);

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override;

protected:
    AminoFonts *parent = NULL;
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
    AminoFontSize();
    ~AminoFontSize();

    float getTextWidth(const char *text);

    //creation
    static AminoFontSizeFactory* getFactory();

    //init
    static v8::Local<v8::Function> GetInitFunction();

private:
    AminoFont *parent = NULL;
    texture_font_t *fontTexture = NULL;

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

class AminoGLFonts {
    //TODO
};

/**
 * Font class.
 */
class AminoFontOld {
public:
    int id;

    //font
    texture_atlas_t *atlas;
    std::map<int, texture_font_t *> fonts; //by font size
    const char *filename;
//cbx
    //shader
    GLuint shader;
    GLint texuni;
    GLint mvpuni;
    GLint transuni;
    GLint coloruni;

    AminoFontOld() {
        texuni = -1;
    }

    virtual ~AminoFontOld() {
        //destroy (if not called before)
        destroy();
    }

    /**
     * Free all resources.
     */
    virtual void destroy() {
        //textures
        for (std::map<int, texture_font_t *>::iterator it = fonts.begin(); it != fonts.end(); it++) {
            if (DEBUG_RESOURCES) {
                printf("freeing font texture\n");
            }

            texture_font_delete(it->second);
        }

        fonts.clear();

        //atlas
        if (atlas) {
            if (DEBUG_RESOURCES) {
                printf("freeing font\n");
            }

            texture_atlas_delete(atlas);
            atlas = NULL;
        }
    }
};

#endif