'use strict';

/*
 * Shader Tools
 */

var DEBUG = false;

var fs = require('fs');
var path = require('path');

var Shader = {
    /**
     * Compile and create a vertex shader.
     */
    compileVertShader: function (text) {
        var GL = this.GL;
        var vertShader = GL.glCreateShader(GL.GL_VERTEX_SHADER);

        GL.glShaderSource(vertShader, 1, text, null);
        GL.glCompileShader(vertShader);

        var stat = GL.glGetShaderiv(vertShader, GL.GL_COMPILE_STATUS);

        if (!stat) {
            //exit
            console.log('Error: vertex shader did not compile!\n');
            process.exit(1);
        }

        return vertShader;
    },
    /**
     * Compile and create a fragment shader.
     */
    compileFragShader: function (text) {
        var GL = this.GL;
        var fragShader = GL.glCreateShader(GL.GL_FRAGMENT_SHADER);

        GL.glShaderSource(fragShader, 1, text, null);
        GL.glCompileShader(fragShader);

        var stat = GL.glGetShaderiv(fragShader, GL.GL_COMPILE_STATUS);

        if (!stat) {
            //exit
            console.log('Error: fragment shader did not compile!\n');
            process.exit(1);
        }

        return fragShader;
    },
    /**
     * Compile and create shader program.
     */
    compileProgram: function (shader) {
        var GL = this.GL;
        var program = GL.glCreateProgram();

        //verify params
        if (!shader.frag) {
            throw Error('missing shader.frag');
        }

        if (!shader.vert) {
            throw Error('missing shader.vert');
        }

        GL.glAttachShader(program, shader.vert);
        GL.glAttachShader(program, shader.frag);
        GL.glLinkProgram(program);

        var stat = this.GL.glGetProgramiv(program, this.GL.GL_LINK_STATUS);

        if (!stat) {
            //exit
            var log = this.GL.glGetProgramInfoLog(program, 1000); //, &len, log);

            console.log('error linking ', log);
            process.exit(1);
        }
        return program;
    },

    /**
     * Build shader program.
     */
    build: function () {
        this.vert = this.compileVertShader(this.vertText);
        this.frag = this.compileFragShader(this.fragText);
        this.prog = this.compileProgram(this);
    },

    /**
     * Activate shader program.
     */
    useProgram: function() {
        this.GL.glUseProgram(this.prog);
    },
    attribs:{},
    uniforms:{},

    locateAttrib: function (name) {
        this.attribs[name] = this.GL.glGetAttribLocation(this.prog, name);

        if (this.attribs[name] == -1) {
            console.log('WARNING. got -1 for location of ' + name + ' attrib');
        }
    },

    locateUniform: function (name) {
        this.uniforms[name] = this.GL.glGetUniformLocation(this.prog, name);

        if (this.uniforms[name] == -1) {
            console.log('WARNING. got -1 for location of ' + name + ' uniform');
        }
    }

};

/**
 * Load shader code from file system.
 */
function loadShaderCode(path, OS) {
    var src = fs.readFileSync(path).toString();

    if (OS == 'RPI') {
        //Raspberry Pi need version number in shader
        src = '#version 100\n' + src;
    }

    return src;
}

/**
 * Initialize the basic aminogfx shaders.
 */
exports.init = function (sgtest, OS) {
    var cshader = Object.create(Shader);

    cshader.GL = sgtest;

    if (DEBUG) {
        console.log('__dirname = ', __dirname);
    }

    //color shader
    cshader.vertText = loadShaderCode(path.join(__dirname, '/shaders/color.vert'), OS);
    cshader.fragText = loadShaderCode(path.join(__dirname, '/shaders/color.frag'), OS);
    cshader.build();

    cshader.useProgram();
    cshader.locateAttrib('pos');
    cshader.locateUniform('modelviewProjection');
    cshader.locateUniform('trans');
    cshader.locateUniform('opacity');
    cshader.locateAttrib('color');

    sgtest.initColorShader(cshader.prog,
        cshader.uniforms.modelviewProjection,
        cshader.uniforms.trans,
        cshader.uniforms.opacity,
        cshader.attribs.pos,
        cshader.attribs.color);

    //texture shader
    var tshader = Object.create(Shader);

    tshader.GL = sgtest;
    tshader.vertText = loadShaderCode(path.join(__dirname, '/shaders/texture.vert'), OS);
    tshader.fragText = loadShaderCode(path.join(__dirname, '/shaders/texture.frag'), OS);
    tshader.build();

    tshader.useProgram();

    tshader.locateUniform('modelviewProjection');
    tshader.locateUniform('trans');
    tshader.locateUniform('opacity');

    tshader.locateAttrib('pos');
    tshader.locateAttrib('texcoords');

    sgtest.initTextureShader(tshader.prog,
        tshader.uniforms.modelviewProjection,
        tshader.uniforms.trans,
        tshader.uniforms.opacity,
        tshader.attribs.pos,
        tshader.attribs.texcoords,
        tshader.attribs.tex);
};
