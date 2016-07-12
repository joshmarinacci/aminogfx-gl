#include "shaders.h"

ColorShader::ColorShader() {
    //empty
}

void ColorShader::destroy() {
    if (prog != -1) {
        glDeleteProgram(prog);
        prog = -1;
    }
}

TextureShader::TextureShader() {
    //empty
}

void TextureShader::destroy() {
    if (prog != -1) {
        glDeleteProgram(prog);
        prog = -1;
    }
}
