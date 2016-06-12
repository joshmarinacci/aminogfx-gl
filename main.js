'use strict';

var DEBUG = false;
var DEBUG_FPS = false;

if (DEBUG) {
    console.log('inside of the aminogfx main.js');
}

//load native module
var binary = require('node-pre-gyp');
var path = require('path');
var binding_path = binary.find(path.resolve(path.join(__dirname, './package.json')));
var sgtest = require(binding_path);

//detect platform
var OS = 'KLAATU';

if (process.arch == 'arm') {
    OS = 'RPI';
}
if (process.platform == 'darwin') {
    OS ='MAC';
}

//var amino_core = require('aminogfx');
var amino_core = require('./src/core/main'); //FIXME
var Core = amino_core.Core;
var Shaders = require('./src/shaders.js');
var fs = require('fs');

//Fonts

var fontmap = {};
var defaultFonts = {
    'source': {
        name: 'source',
        weights: {
            200: {
                normal: 'SourceSansPro-ExtraLight.ttf',
                italic: 'SourceSansPro-ExtraLightItalic.ttf'
            },
            300: {
                normal: 'SourceSansPro-Light.ttf',
                italic: 'SourceSansPro-LightItalic.ttf'
            },
            400: {
                normal: 'SourceSansPro-Regular.ttf',
                italic: 'SourceSansPro-Italic.ttf'
            },

            600: {
                normal: 'SourceSansPro-Semibold.ttf',
                italic: 'SourceSansPro-SemiboldItalic.ttf'
            },
            700: {
                normal: 'SourceSansPro-Bold.ttf',
                italic: 'SourceSansPro-BoldItalic.ttf'
            },
            900: {
                normal: 'SourceSansPro-Black.ttf',
                italic: 'SourceSansPro-BlackItalic.ttf'
            }
        }
    },
    'awesome': {
        name: 'awesome',
        weights: {
            400: {
                normal: 'fontawesome-webfont.ttf'
            }
        }
    }
}

var propsHash = {

    //general
    "visible": 18,
    "opacity": 27,
    "r": 5,
    "g": 6,
    "b": 7,
    "texid": 8,
    "w": 10,
    "h": 11,
    "x": 21,
    "y": 22,

    //transforms  (use x and y for translate in X and Y)
    "sx": 2,
    "sy": 3,
    "rz": 4,
    "rx": 19,
    "ry": 20,

    //text
    "text": 9,
    "fontSize": 12,
    "fontId": 28,

    //animation
    "count": 29,
    "lerplinear": 13,
    "lerpcubicin": 14,
    "lerpcubicout": 15,
    "lerpprop": 16,
    "lerpcubicinout": 17,
    "autoreverse": 35,
    "then": 37,

    //geometry
    "geometry":  24,
    "filled":    25,
    "closed":    26,
    "dimension": 36,

    //rectangle texture
    "textureLeft":   30,
    "textureRight":  31,
    "textureTop":    32,
    "textureBottom": 33,

    //clipping
    "cliprect": 34
};

var timeFuncsHash = {
    //time function
    'linear':     13,
    'cubicIn':    14,
    'cubicOut':   15,
    'cubicInOut': 17,
};

/**
 * JSFont constructor.
 */
function JSFont(desc) {
    this.name = desc.name;

    var reg = desc.weights[400];

    this.desc = desc;
    this.weights = {};
    this.filepaths = {};

    var dir = process.cwd();

    process.chdir(__dirname + '/..'); // chdir such that fonts (and internal shaders) may be found

    var aminodir = __dirname + '/resources/';

    if (desc.path) {
        aminodir = desc.path;
    }

    for (var weight in desc.weights) {
        var filepath = aminodir + desc.weights[weight].normal;

        if (!fs.existsSync(filepath)) {
            throw new Error('WARNING. File not found: ' + filepath);
        }

        this.weights[weight] = Core.getCore().getNative().createNativeFont(filepath);
        this.filepaths[weight] = filepath;
    }

    process.chdir(dir);

    this.getNative = function (size, weight, style) {
        if (this.weights[weight] != undefined) {
            return this.weights[weight];
        }

        console.log('ERROR. COULDN\'T find the native for ' + size + ' ' + weight + ' ' + style);

        return this.weights[400];
    }

    /**
     * @func calcStringWidth(string, size)
     *
     * returns the width of the specified string rendered at the specified size
     */
    this.calcStringWidth = function(str, size, weight, style) {
        amino.GETCHARWIDTHCOUNT++;
        return amino.sgtest.getCharWidth(str, size, this.getNative(size, weight, style));
    }

    /**
     * Get font height.
     */
    this.getHeight = function(size, weight, style) {
        amino.GETCHARHEIGHTCOUNT++;

        if (size == undefined) {
            throw new Error("SIZE IS UNDEFINED");
        }

        return amino.sgtest.getFontHeight(size, this.getNative(size, weight, style));
    }

    /**
     * Get font height metrics.
     */
    this.getHeightMetrics = function(size, weight, style) {
        if (size == undefined) {
            throw new Error("SIZE IS UNDEFINED");
        }

        return {
            ascender: amino.sgtest.getFontAscender(size, this.getNative(size, weight, style)),
            descender: amino.sgtest.getFontDescender(size, this.getNative(size, weight, style))
        };
    }
}

/**
 * Animated property.
 */
function JSPropAnim(target, name) {
    this._from = null;
    this._to = null;
    this._duration = 1000;
    this._loop = 1;
    this._delay = 0;
    this._autoreverse = 0;
    this._lerpprop = 17; //cubicInOut
    this._then_fun = null;

    //setters
    this.from  = function(val) {  this._from = val;        return this;  }
    this.to    = function(val) {  this._to = val;          return this;  }
    this.dur   = function(val) {  this._duration = val;    return this;  }
    this.delay = function(val) {  this._delay = val;       return this;  }
    this.loop  = function(val) {  this._loop = val;        return this;  }
    this.then  = function(fun) {  this._then_fun = fun;    return this;  }
    this.autoreverse = function(val) { this._autoreverse = val ? 1:0; return this;  }
    this.timeFunc = function (val) {
        var tf = timeFuncsHash[val];

        if (!tf) {
            throw new Error('unknown time function: ' + val);
        }

        this._lerpprop = tf;

        return this;
    }

    //start the animation
    this.start = function () {
        if (DEBUG) {
            console.log('startin anim');
        }

        var self = this;

        setTimeout(function () {
            if (DEBUG) {
                console.log('after delay. making it.');
            }

            //create native instance
            var core = Core.getCore();
            var nat = core.getNative();

            self.handle = nat.createAnim(target.handle, name, self._from,self._to,self._duration);

            if (DEBUG) {
                console.log('handle is: ', self.handle);
            }

            //set params
            nat.updateAnimProperty(self.handle, 'count', self._loop);
            nat.updateAnimProperty(self.handle, 'autoreverse', self._autoreverse);
            nat.updateAnimProperty(self.handle, 'lerpprop', self._lerpprop);
            nat.updateAnimProperty(self.handle, 'then', self._then_fun);

            //add
            core.anims.push(self);
        }, this._delay);

        return this;
    }

    //TODO more features from native

}

//native bindings

var PNG_HEADER = new Buffer([ 137, 80, 78, 71, 13, 10, 26, 10 ]); //PNG Header

var gl_native = {
    createNativeFont: function (path) {
        return sgtest.createNativeFont(path, __dirname + '/resources/shaders');
    },
    registerFont:function (args) {
        fontmap[args.name] = new JSFont(args);
    },
    init: function (core) {
        return sgtest.init();
    },
    startEventLoop: function () {
        if (DEBUG) {
            console.log('starting the event loop');
        }

        var self = this;

        function immediateLoop() {
            try {
                self.tick(Core.getCore());
            } catch (ex) {
                //report
                console.log(ex);
                console.log(ex.stack);
                console.log("EXCEPTION. QUITTING!");
                return;
            }

            //debug: fps
            if (DEBUG_FPS) {
                var time = (new Date()).getTime();

                if (self.lastTime) {
                    console.log('fps: ' + (1000 / (time - self.lastTime)));
                }
                self.lastTime = time;
            }

            //next cycle
            self.setImmediate(immediateLoop);
        }

        //1 ms interval
        setTimeout(immediateLoop, 1);
    },
    createWindow: function (core, w, h) {
        //create window
        sgtest.createWindow(w * core.DPIScale, h * core.DPIScale);

        //init shaders
        Shaders.init(sgtest, OS);

        //fonts
        fontmap['source']  = new JSFont(defaultFonts['source']);
        fontmap['awesome'] = new JSFont(defaultFonts['awesome']);

        core.defaultFont = fontmap['source'];

        //root
        this.rootWrapper = this.createGroup();
        sgtest.setRoot(this.rootWrapper);

        //scale
        this.updateProperty(this.rootWrapper, 'sx', core.DPIScale);
        this.updateProperty(this.rootWrapper, 'sy', core.DPIScale);
    },
    getFont: function(name) { return fontmap[name]; },
    updateProperty: function(handle, name, value) {
        if (handle == undefined) {
            throw new Error('Can\'t set a property on an undefined handle!!');
        }

        //console.log('setting', handle, name, propsHash[name], value, typeof value);

        var hash = propsHash[name];

        if (!hash) {
            throw new Error('Unknown update property: ' + name);
        }

        sgtest.updateProperty(handle, hash, value);
    },
    setRoot: function (handle) { return  sgtest.addNodeToGroup(handle,this.rootWrapper);  },
    tick: function() {
        sgtest.tick();
    },
    setImmediate: function (loop) {
        //see https://nodejs.org/api/timers.html#timers_setimmediate_callback_arg
        setImmediate(loop);
    },
//    setEventCallback: function(cb) {   return sgtest.setEventCallback(cb);   },
    setEventCallback: function (cb) {   return sgtest.setEventCallback(function(){
        //console.log("got some stuff",arguments);
        cb(arguments[1]);
    });   },
    createRect: function ()  {          return sgtest.createRect();           },
    createGroup: function () {          return sgtest.createGroup();          },
    createPoly: function ()  {          return sgtest.createPoly();           },
    createText: function () {           return sgtest.createText();           },
    createGLNode: function (cb)  {      return sgtest.createGLNode(cb);       },
    addNodeToGroup: function (h1,h2) {
        return sgtest.addNodeToGroup(h1,h2);
    },
    removeNodeFromGroup: function (h1, h2) {
        return sgtest.removeNodeFromGroup(h1, h2);
    },
    loadImage: function (src, cb) {
        var buffer;
        var type;

        //check buffer
        if (Buffer.isBuffer(src)) {
            buffer = src;

            //check PNG
            if (buffer.slice(0, 8).equals(PNG_HEADER)) {
                //PNG
                type = 'png';
            } else {
                //JPEG
                type = 'jpg';
            }
        } else if (typeof src === 'string') {
            //load file (blocking)
            var buffer = fs.readFileSync(src);

            if (!buffer) {
                if (DEBUG) {
                    console.log('File not found: ' + src);
                }

                cb();
                return;
            }

            //check file name
            var name = src.toLowerCase();

            if (name.endsWith('.png')) {
                type = 'png';
            } else if (name.endsWith(".jpg")) {
                type = 'jpg';
            } else {
                if (DEBUG) {
                    console.log('Unsupported file format: ' + src);
                }

                cb();
                return;
            }
        } else {
            if (DEBUG) {
                console.log('Invalid src format!');
            }

            cb();
            return;
        }

        //debug
        if (DEBUG) {
            console.log('loadImage() type=' + type + ' length=' + buffer.length);
        }

        //decode
        var nat = Core.getCore().getNative();

        function bufferToTexture(ibuf) {
            //check error
            if (!ibuf) {
                if (DEBUG) {
                    console.log('could not load image: type='  + type);
                }

                cb();
                return;
            }

            //load texture
            nat.loadBufferToTexture(-1, ibuf.w, ibuf.h, ibuf.bpp, ibuf.buffer, function (texture) {
                cb(texture);
            });
        }

        switch (type) {
            case 'png':
                nat.decodePngBuffer(buffer, bufferToTexture);
                break;

            case 'jpg':
                nat.decodeJpegBuffer(buffer, bufferToTexture);
                break;
        }

    },
    decodePngBuffer: function (fbuf, cb) {  return cb(sgtest.decodePngBuffer(fbuf));  },
    decodeJpegBuffer: function (fbuf, cb) { return cb(sgtest.decodeJpegBuffer(fbuf)); },
    loadBufferToTexture: function (texid, w, h, bpp, buf, cb) {
        return cb(sgtest.loadBufferToTexture(texid, w,h, bpp, buf));
    },
    setWindowSize: function (w, h) {
        sgtest.setWindowSize(w * Core.getCore().DPIScale, h * Core.getCore().DPIScale);
    },
    getWindowSize: function (w, h) {
        var dpi = Core.getCore().DPIScale;
        var size = sgtest.getWindowSize(w, h);

        return {
            w: size.w / dpi,
            h: size.h / dpi
        };
    },
    createAnim: function (handle, prop, start, end, dur) {
        var hash = propsHash[prop];

        if (!hash) {
            throw new Error('invalid native property name', prop);
        }

        return sgtest.createAnim(handle, hash, start, end, dur);
    },
    createPropAnim: function (obj, name) {
        return new JSPropAnim(obj, name);
    },
    updateAnimProperty: function (handle, prop, type) {
        var hash = propsHash[prop];

        if (!hash) {
            throw new Error('invalid animattion property: ' + prop);
        }

        sgtest.updateAnimProperty(handle, hash, type);
    },
    updateWindowProperty: function (stage, prop, value) {
        var hash = propsHash[prop];

        if (!hash) {
            throw new Error('invalid native property name', prop);
        }

        return sgtest.updateWindowProperty(-1, hash, value);
    }
}

exports.input = amino_core.input;
exports.start = function(cb) {
    if (!cb) {
        throw new Error('CB parameter missing to start app');
    }

    //init core
    Core.setCore(new Core());

    var core = Core.getCore();

    core.native = gl_native;
    amino_core.native = gl_native;
    core.init();

    //create stage
    var stage = Core._core.createStage(600, 600);

    stage.fill.watch(function () {
        var color = amino_core.ParseRGBString(stage.fill());

        gl_native.updateWindowProperty(stage, 'r', color.r);
        gl_native.updateWindowProperty(stage, 'g', color.g);
        gl_native.updateWindowProperty(stage, 'b', color.b);
    });

    stage.opacity.watch(function () {
        gl_native.updateWindowProperty(stage, 'opacity', stage.opacity());
    });

    //mirror fonts to PureImage
    /*
    var source_font = exports.getRegisteredFonts().source;
    var fnt = PImage.registerFont(source_font.filepaths[400],source_font.name);
    fnt.load(function() {
        cb(Core._core,stage);
        Core._core.start();
    });
    */
    cb(core, stage);
    core.start();
};

exports.makeProps = amino_core.makeProps; //FIXME undefined!

exports.Rect      = amino_core.Rect;
exports.Group     = amino_core.Group;
exports.Circle    = amino_core.Circle;
exports.Polygon   = amino_core.Polygon;
exports.Text      = amino_core.Text;
exports.ImageView = amino_core.ImageView;

exports.input.init(OS);

//exports.PureImageView = amino.primitives.PureImageView;
//exports.ConstraintSolver = require('./src/ConstraintSolver');
