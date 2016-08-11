#include "shaders.h"

ColorShader::ColorShader() {
    //empty
}

void ColorShader::destroy() {
    if (prog != INVALID_PROGRAM) {
        glDeleteProgram(prog);
        prog = INVALID_PROGRAM;
    }
}

TextureShader::TextureShader() {
    //empty
}

void TextureShader::destroy() {
    if (prog != INVALID_PROGRAM) {
        glDeleteProgram(prog);
        prog = INVALID_PROGRAM;
    }
}
