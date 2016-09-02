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

    GLchar *src = (GLchar *)source.c_str();

    glShaderSource(handle, 1, (const GLchar **)&src, NULL);
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
            std::string typeStr;

            if (type == GL_VERTEX_SHADER) {
                typeStr = "vertex shader";
            } else if (type == GL_FRAGMENT_SHADER) {
                typeStr = "fragment shader";
            }

            printf("shader compilation error: %s\ntype: %s\n", messages, typeStr.c_str());
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
void AnyShader::useShader(bool active) {
    if (!active) {
        glUseProgram(prog);
    }
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
    useShader(false);

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
 * Set vertex data.
 */
void AnyAminoShader::setVertexData(GLsizei dim, GLfloat *vertices) {
    /*
     * Coords per vertex (2 or 3).
     *
     * Note: vertices is NULL in case of VBO usage
     */

    glVertexAttribPointer(aPos, dim, GL_FLOAT, GL_FALSE, 0, vertices);
}

/**
 * Draw triangles.
 */
void AnyAminoShader::drawTriangles(GLsizei vertices, GLenum mode) {
    glEnableVertexAttribArray(aPos);

    glDrawArrays(mode, 0, vertices);

    glDisableVertexAttribArray(aPos);
}

/**
 * Draw elements.
 */
void AnyAminoShader::drawElements(GLushort *indices, GLsizei elements, GLenum mode) {
    glEnableVertexAttribArray(aPos);

    //Note: indices is offset in case of VBO
    glDrawElements(mode, elements, GL_UNSIGNED_SHORT, indices);

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
// ColorLightingShader
//

/**
 * Create color lighting shader.
 */
ColorLightingShader::ColorLightingShader() : ColorShader() {
    //shaders
    vertexShader = R"(
        uniform mat4 mvp;
        uniform mat4 trans;
        uniform vec3 lightDir;

        attribute vec4 pos;
        attribute vec3 normal;

        varying float lightFac;

        void main() {
            gl_Position = mvp * trans * pos;

            vec4 normalTrans = trans * vec4(normal, 0.);

            lightFac = max(dot(normalTrans.xyz, -lightDir), 0.);
        }
    )";

    fragmentShader = R"(
        varying float lightFac;

        uniform vec4 color;

        void main() {
            gl_FragColor = vec4(color.rgb * lightFac, color.a);
        }
    )";
}

/**
 * Initialize the color lighting shader.
 */
void ColorLightingShader::initShader() {
    ColorShader::initShader();

    //attributes
    aNormal = getAttributeLocation("normal");

    //uniforms
    uLightDir = getUniformLocation("lightDir");

    //default values
    GLfloat lightDir[3] = { 0, 0, -1 }; //parallel light on screen

    setLightDirection(lightDir);
}

/**
 * Set light direction.
 */
void ColorLightingShader::setLightDirection(GLfloat dir[3]) {
    glUniform3f(uLightDir, dir[0], dir[1], dir[2]);
}

/**
 * Set normal vectors.
 */
void ColorLightingShader::setNormalVectors(GLfloat *normals) {
    glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, 0, normals);
}

/**
 * Draw triangles.
 */
void ColorLightingShader::drawTriangles(GLsizei vertices, GLenum mode) {
    glEnableVertexAttribArray(aNormal);

    AnyAminoShader::drawTriangles(vertices, mode);

    glDisableVertexAttribArray(aNormal);
}

/**
 * Draw elements.
 */
void ColorLightingShader::drawElements(GLushort *indices, GLsizei elements, GLenum mode) {
    glEnableVertexAttribArray(aNormal);

    AnyAminoShader::drawElements(indices, elements, mode);

    glDisableVertexAttribArray(aNormal);
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

    fragmentShader = R"(
        varying vec2 uv;

        uniform float opacity;
        uniform sampler2D tex;

        void main() {
            vec4 pixel = texture2D(tex, uv);

            gl_FragColor = vec4(pixel.rgb, pixel.a * opacity);
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
 * Set texture coordinates.
 */
void TextureShader::setTextureCoordinates(GLfloat uv[][2]) {
    glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 0, uv);
}

/**
 * Draw texture.
 */
void TextureShader::drawTriangles(GLsizei vertices, GLenum mode) {
    glEnableVertexAttribArray(aTexCoord);

    glActiveTexture(GL_TEXTURE0);

    AnyAminoShader::drawTriangles(vertices, mode);

    glDisableVertexAttribArray(aTexCoord);
}

//
// TextureClampToBorderShader
//


TextureClampToBorderShader::TextureClampToBorderShader() : TextureShader() {
    //Note: supports clamp to border, using transparent texture
    fragmentShader = R"(
        varying vec2 uv;

        uniform float opacity;
        uniform sampler2D tex;

        float clamp_to_border_factor(vec2 coords) {
            bvec2 out1 = greaterThan(coords, vec2(1, 1));
            bvec2 out2 = lessThan(coords, vec2(0, 0));
            bool do_clamp = (any(out1) || any(out2));

            return float(!do_clamp);
        }

        void main() {
            vec4 pixel = texture2D(tex, uv);

            gl_FragColor = vec4(pixel.rgb, pixel.a * opacity * clamp_to_border_factor(uv));
        }
    )";
}
