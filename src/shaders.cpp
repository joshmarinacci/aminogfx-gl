#include "shaders.h"

#define INVALID_SHADER 0

#define DEBUG_SHADER_ERRORS true

//
// AnyShader
//

AnyShader::AnyShader() {
    //empty
}

AnyShader::~AnyShader() {
    //empty
}

/**
 * Create the shader program.
 */
bool AnyShader::create() {
    if (failed) {
        return false;
    }

    if (prog != INVALID_PROGRAM) {
        return true;
    }

    //compile
    GLuint handle = glCreateProgram();

    if (handle == INVALID_PROGRAM) {
        error = "glCreateProgram failed.";

        return false;
    }

    if (!vertexShader.empty()) {
        GLuint vertShader = compileShader(vertexShader, GL_VERTEX_SHADER);

        if (vertShader == INVALID_SHADER) {
            //cleanup
            glDeleteProgram(handle);

            return false;
        }

        glAttachShader(handle, vertShader);
        glDeleteShader(vertShader);
    }

    if (!fragmentShader.empty()) {
        GLuint fragShader = compileShader(fragmentShader, GL_FRAGMENT_SHADER);

        if (fragShader == INVALID_SHADER) {
            //cleanup
            glDeleteProgram(handle);

            return false;
        }

        glAttachShader(handle, fragShader);
        glDeleteShader(fragShader);
    }

    //link
    glLinkProgram(handle);

    GLint linkStatus;

    glGetProgramiv(handle, GL_LINK_STATUS, &linkStatus);

    if (linkStatus == GL_FALSE) {
        //cleanup
        glDeleteProgram(handle);

        //get error
        GLchar messages[256];

        glGetProgramInfoLog(handle, sizeof(messages), 0, &messages[0]);

        if (DEBUG_SHADER_ERRORS) {
            printf("shader linking failed: %s\n", messages);
        }

        return false;
    }

    prog = handle;

    //initialize
    initShader();

    return true;
}

/**
 * Compile a shader.
 *
 * Note: sets error on failure.
 */
GLuint AnyShader::compileShader(std::string source, const GLenum type) {
    GLint compileStatus;
    GLuint handle = glCreateShader(type);

    if (handle == INVALID_SHADER) {
        error = "glCreateShader() failed";
        return -1;
    }

#ifdef RPI
    //add GLSL version
    source = "#version 100\n" + source;
#endif

    char *src = (char *)source.c_str();

    glShaderSource(handle, 1, &src, NULL);
    glCompileShader(handle);

    //get status
    glGetShaderiv(handle, GL_COMPILE_STATUS, &compileStatus);

    if (compileStatus == GL_FALSE) {
        //free
        glDeleteShader(handle);
        handle = INVALID_SHADER;

        //get error
        GLchar messages[256];

        glGetShaderInfoLog(handle, sizeof(messages), 0, &messages[0]);

        if (DEBUG_SHADER_ERRORS) {
            printf("shader compilation error: %s\n", messages);
        }

        error = std::string(messages);
    }

    return handle;
}

/**
 * Destroy the shader.
 *
 * Note: has to be called in OpenGL thread.
 */
void AnyShader::destroy() {
    if (prog != INVALID_PROGRAM) {
        glDeleteProgram(prog);
        prog = INVALID_PROGRAM;
    }

    //reset failed
    failed = false;
}

/**
 * Get attribute location.
 */
GLint AnyShader::getAttributeLocation(std::string name) {
    GLint loc = glGetAttribLocation(prog, name.c_str());

    if (DEBUG_SHADER_ERRORS) {
        if (loc == -1) {
            printf("could not get location of attribute %s\n", name.c_str());
        }
    }

    return loc;
}

/**
 * Get uniform location.
 */
GLint AnyShader::getUniformLocation(std::string name) {
    GLint loc = glGetUniformLocation(prog, name.c_str());

    if (DEBUG_SHADER_ERRORS) {
        if (loc == -1) {
            printf("could not get location of uniform %s\n", name.c_str());
        }
    }

    return loc;
}

/**
 * Use the shader.
 *
 * Note: has to be called before any setters are used!
 */
void AnyShader::useShader() {
    glUseProgram(prog);
}

//
// AnyAminoShader
//

AnyAminoShader::AnyAminoShader() : AnyShader() {
    //default vertex shader
    vertexShader = R"(
        uniform mat4 mvp;
        uniform mat4 trans;

        attribute vec4 pos;

        void main() {
            gl_Position = mvp * trans * pos;
        }
    )";
}

/**
 * Initialize the shader.
 */
void AnyAminoShader::initShader() {
    useShader();

    //attributes
    aPos = getAttributeLocation("pos");

    //uniforms
    uMVP = getUniformLocation("mvp");
    uTrans = getUniformLocation("trans");
}

/**
 * Set transformation matrix.
 */
void AnyAminoShader::setTransformation(GLfloat modelView[16], GLfloat transition[16]) {
    glUniformMatrix4fv(uMVP, 1, GL_FALSE, modelView);
    glUniformMatrix4fv(uTrans, 1, GL_FALSE, transition);
}

/**
 * Draw triangles.
 */
void AnyAminoShader::drawTriangles(GLfloat *verts, GLsizei dim, GLsizei vertices, GLenum mode) {
    //x/y coords per vertex
    glVertexAttribPointer(aPos, dim, GL_FLOAT, GL_FALSE, 0, verts);

    //draw
    glEnableVertexAttribArray(aPos);

    glDrawArrays(mode, 0, vertices);

    glDisableVertexAttribArray(aPos);
}

//
// ColorShader
//

/**
 * Create color shader.
 */
ColorShader::ColorShader() : AnyAminoShader() {
    //shaders
    fragmentShader = R"(
        uniform vec4 color;

        void main() {
            gl_FragColor = color;
        }
    )";
}

/**
 * Initialize the color shader.
 */
void ColorShader::initShader() {
    AnyAminoShader::initShader();

    //uniforms
    uColor = getUniformLocation("color");
}

/**
 * Set color.
 */
void ColorShader::setColor(GLfloat color[4]) {
    glUniform4f(uColor, color[0], color[1], color[2], color[3]);
}

//
// TextureShader
//

TextureShader::TextureShader() : AnyAminoShader() {
    //shader
    vertexShader = R"(
        uniform mat4 mvp;
        uniform mat4 trans;

        attribute vec4 pos;
        attribute vec2 tex_coord;

        varying vec2 uv;

        void main() {
            gl_Position = mvp * trans * pos;
            uv = tex_coord;
        }
    )";

    //Note: supports clamp to border, using transparent texture
    fragmentShader = R"(
        varying vec2 uv;

        uniform float opacity;
        uniform sampler2D tex;

        float clamp_to_border_factor(vec2 coords) {
            bvec2 out1 = greaterThan(coords, vec2(1, 1));
            bvec2 out2 = lessThan(coords, vec2(0,0));
            bool do_clamp = (any(out1) || any(out2));

            return float(!do_clamp);
        }

        void main() {
            vec4 pixel = texture2D(tex, uv);

            gl_FragColor = vec4(pixel.rgb, pixel.a * opacity * clamp_to_border_factor(uv));
        }
    )";
}

/**
 * Initialize the color shader.
 */
void TextureShader::initShader() {
    AnyAminoShader::initShader();

    //attributes
    aTexCoord = getAttributeLocation("tex_coord");

    //uniforms
    uOpacity = getUniformLocation("opacity");
    uTex = getUniformLocation("tex");

    //default values
    glUniform1i(uTex, 0); //GL_TEXTURE0
}

/**
 * Set opacity.
 */
void TextureShader::setOpacity(GLfloat opacity) {
    glUniform1f(uOpacity, opacity);
}

/**
 * Draw texture.
 */
void TextureShader::drawTexture(GLfloat *verts, GLsizei dim, GLfloat texcoords[][2], GLsizei vertices, GLenum mode) {
    glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
    glEnableVertexAttribArray(aTexCoord);

    glActiveTexture(GL_TEXTURE0);

    drawTriangles(verts, dim, vertices, mode);

    glDisableVertexAttribArray(aTexCoord);
}
