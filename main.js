'use strict';

var DEBUG = false;
var DEBUG_FPS = false;

if (DEBUG) {
    console.log('inside of the aminogfx main.js');
}

//var amino_core = require('aminogfx'); //NPM module
var amino_core = require('./src/core/main'); //modified code

//load native module
var binary = require('node-pre-gyp');
var path = require('path');
var binding_path = binary.find(path.resolve(path.join(__dirname, 'package.json')));
var sgtest = require(binding_path);

var shaders = require('./src/shaders.js');
var fs = require('fs');
var util =  require('util');

//detect platform
var OS;

if (process.arch == 'arm') {
    OS = 'RPI';
}

if (process.platform == 'darwin') {
    OS ='MAC';
}

//
//  AminoGfx
//
var AminoGfx = sgtest.AminoGfx;

/**
 * Initialize AminoGfx instance.
 */
AminoGfx.prototype.init = function () {
    if (DEBUG) {
        console.log('AminoGfx.init()');
    }

    //initialize bindings
    this.amino = this;

    makeProps(this, {
        //position (auto set)
        x: -1,
        y: -1,

        //QHD size
        w: 640,
        h: 360,

        //color
        opacity: 1,
        fill: '#000000',
        r: 0,
        g: 0,
        b: 0,

        //title
        title: 'AminoGfx OpenGL'
    });

    this.fill.watch(watchFill);

    //TODO more
    //cbx

    //fonts
    /*
    fontmap['source']  = new JSFont(defaultFonts['source']);
    fontmap['awesome'] = new JSFont(defaultFonts['awesome']);

    core.defaultFont = fontmap['source'];
    */

    //root wrapper
    this.setRoot(this.createGroup());
};

/**
 * Fill value has changed.
 */
function watchFill(value, prop, obj) {
    var color = parseRGBString(value);

    obj.r(color.r);
    obj.g(color.g);
    obj.b(color.b);
}

/**
 * Convert RGB expression to object.
 *
 * Supports:
 *
 *  - hex strings
 *  - array: r, g, b (0..1)
 *  - objects: r, g, b (0..1)
 */
function parseRGBString(Fill) {
    if (typeof Fill == 'string') {
        //strip off any leading #
        if (Fill.substring(0, 1) == '#') {
            Fill = Fill.substring(1);
        }

        //pull out the components
        var r = parseInt(Fill.substring(0, 2), 16);
        var g = parseInt(Fill.substring(2, 4), 16);
        var b = parseInt(Fill.substring(4, 6), 16);

        return {
            r: r / 255,
            g: g / 255,
            b: b / 255
        };
    } else if (Array.isArray(Fill)) {
        return {
            r: Fill[0],
            g: Fill[1],
            b: Fill[2]
        };
    }

    return Fill;
}

AminoGfx.prototype.start = function (done) {
    var self = this;

    //preload shaders
    shaders.preloadShaders(OS, function (err) {
        if (err) {
            done(err);
            return;
        }

        //pass to native code
        self._start(function (err) {
            if (err) {
                done.call(self, err);
                return;
            }

            //init shaders
            self.GL = AminoGfx.GL;
            shaders.init(self, OS);

            //ready (Note: this points to the instance)
            done.call(self, err);

            //check root
            if (!self.root) {
                throw new Error('Missing root!');
            }

            //start rendering loop
            self.startTimer();
        });
    });
};

AminoGfx.prototype.setRoot = function (root) {
    this.root = root;

    this._setRoot(root);
};

AminoGfx.prototype.getRoot = function () {
    return this.root;
};

/**
 * Create group element.
 */
AminoGfx.prototype.createGroup = function () {
    return new AminoGfx.Group(this);
};

/**
 * Create rect element.
 */
AminoGfx.prototype.createRect = function () {
    return new AminoGfx.Rect(this);
};

/**
 * Create polygon element.
 */
AminoGfx.prototype.createPolygon = function () {
    return new AminoGfx.Polygon(this);
};

/**
 * Create circle element.
 */
AminoGfx.prototype.createCircle = function () {
    return new AminoGfx.Circle(this);
};

AminoGfx.prototype.startTimer = function () {
    if (this.timer) {
        return;
    }

    var self = this;

    function immediateLoop() {
        //debug
        //console.log('tick()');

        self.tick();

        //debug: fps
        if (DEBUG_FPS) {
            var time = (new Date()).getTime();

            if (self.lastTime) {
                console.log('fps: ' + (1000 / (time - self.lastTime)));
            }

            self.lastTime = time;
        }

        //next cycle
        self.timer = setImmediate(immediateLoop);
    }

    //see https://nodejs.org/api/timers.html#timers_setimmediate_callback_arg
    this.timer = setImmediate(immediateLoop);
};

AminoGfx.prototype.handleEvent = function (evt) {
    //debug
    //console.log('Event: ' + JSON.stringify(evt));

    evt.time = new Date().getTime();

    exports.input.processEvent(this, evt);
};

AminoGfx.prototype.destroy = function () {
    if (this.timer) {
        clearImmediate(this.timer);
        this.timer = null;
    }

    this._destroy();
};

AminoGfx.prototype.find = function (id) {
    function findNodeById(id, node) {
        if (node.id && node.id == id) {
            return node;
        }

        if (node.isGroup) {
            var count = node.getChildCount();

            for (var i = 0; i < count; i++) {
                var ret = findNodeById(id, node.getChild(i));

                if (ret != null) {
                    return ret;
                }
            }
        }

        return null;
    }

    return findNodeById(id, this.getRoot());
};

exports.AminoGfx = AminoGfx;

//
// AminoGfx.Group
//

var Group = AminoGfx.Group;

Group.prototype.init = function () {
    if (DEBUG) {
        console.log('Group.init()');
    }

    //bindings
    makeProps(this, {
        id: '',

        //visibility
        visible: true,
        opacity: 1,

        //position
        x: 0,
        y: 0,

        //size
        w: 100,
        h: 100,

        //scaling
        sx: 1,
        sy: 1,

        //rotation
        rx: 0,
        ry: 0,
        rz: 0,

        //clipping
        cliprect: false
    });

    this.isGroup = true;
    this.children = [];
};

Group.prototype.add = function () {
    var count = arguments.length;

    for (var i = 0; i < count; i++) {
        var node = arguments[i];

        if (!node) {
            throw new Error('can\'t add a null child to a group');
        }

        if (this.children.indexOf(node) != -1) {
            throw new Error('child was added before');
        }

        if (node == this || node.parent) {
            throw new Error('already added to different group');
        }

        this._add(node);
        this.children.push(node);
        node.parent = this;
    }

    return this;
};

Group.prototype.remove = function (child) {
    var n = this.children.indexOf(child);

    if (n >=  0) {
        this._remove(child);
        this.children.splice(n, 1);
        child.parent = null;
    }

    return this;
};

Group.prototype.clear = function () {
    var count = this.children.length;

    for (var i = 0; i < count; i++) {
        this._remove(this.children[i]);
    }

    this.children = [];

    return this;
};

Group.prototype.raiseToTop = function (node) {
    if (!node) {
        throw new Error('can\'t move a null child');
    }

    this.remove(node);
    this.add(node);

    return this;
};

Group.prototype.find = function (pattern) {
    var results = new FindResults();

    if (pattern.indexOf('#') == 0) {
        //id
        var id = pattern.substring(1);

        results.children = treeSearch(this, false, function (child) {
            return (child.id().toLowerCase() == id);
        });
    } else {
        results.children = treeSearch(this, false, function (child) {
            return child.constructor.name.toLowerCase() == pattern.toLowerCase();
        });
    }

    return results;
};

function treeSearch (root, considerRoot, filter) {
    var res = [];

    if (root.isGroup) {
        var count = root.children.length;

        for (var i = 0; i < count; i++) {
            res = res.concat(treeSearch(root.children[i], true, filter));
        }
    }

    if (considerRoot && filter(root)) {
        return res.concat([root]);
    }

    return res;
}

function FindResults() {
    this.children = [];

    function makefindprop(obj, name) {
        obj[name] = function (val) {
            this.children.forEach(function (child) {
                if (child[name]) {
                    child[name](val);
                }
            });

            return this;
        };
    }

    //TODO review
    makefindprop(this, 'visible');
    makefindprop(this, 'fill');
    makefindprop(this, 'filled');
    makefindprop(this, 'x');
    makefindprop(this, 'y');
    makefindprop(this, 'w');
    makefindprop(this, 'h');

    this.length = function () {
        return this.children.length;
    };
}

//
// Rect
//

var Rect = AminoGfx.Rect;

Rect.prototype.init = function () {
    if (DEBUG) {
        console.log('Rect.init()');
    }

    //properties
    makeProps(this, {
        id: '',
        visible: true,

        //position
        x: 0,
        y: 0,

        //size
        w: 100,
        h: 100,

        //rotation
        rx: 0,
        ry: 0,
        rz: 0,

        //scaling
        sx: 1,
        sy: 1,

        //white
        fill: '#ffffff',
        r: 1,
        g: 1,
        b: 1,
        opacity: 1.0
    });

    //special
    this.fill.watch(watchFill);

    //methods
    this.contains = contains;
};

/**
 * Check if a given point is inside this node.
 *
 * Note: has to be used in object.
 *
 * @param pt coordinate relative to origin of node
 */
function contains(pt) {
    var x = this.x();
    var y = this.y();

    return pt.x >= 0 && pt.x < this.w() &&
           pt.y >= 0 && pt.y < this.h();
}

//
// Polygon
//

var Polygon = AminoGfx.Polygon;

Polygon.prototype.init = function () {
    //bindings
    makeProps(this, {
        id: '',
        visible: true,

        //position
        x: 0,
        y: 0,

        //scaling
        sx: 1,
        sy: 1,

        //fill
        fill: '#ff0000',
        fillR: 1,
        fillG: 0,
        fillB: 0,
        opacity: 1.,

        //properties
        //closed: true,
        filled: true,

        dimension: 2, //2D
        geometry: []
    });

    this.fill.watch(setFill);
};

/**
 * Fill value has changed.
 */
function setFill(val, prop, obj) {
    var color = parseRGBString(val);

    obj.fillR(color.r);
    obj.fillG(color.g);
    obj.fillB(color.b);
}

Polygon.prototype.contains = function () {
    //TODO check polygon coords
    return false;
};

//
// Circle
//

function Circle(amino) {
    //call super
    Polygon.call(this, amino);
}

util.inherits(Circle, Polygon);

Circle.prototype.init = function () {
    //bindings
    makeProps(this, {
        radius: 50,
        steps: 30
    });

    //Monitor radius updates.
    this.radius.watch(function (r) {
        var points = [];
        var steps = self.steps();

        for (var i = 0; i < steps; i++) {
            var theta = Math.PI * 2 / steps * i;

            points.push(Math.sin(theta) * r);
            points.push(Math.cos(theta) * r);
        }

        self.geometry(points);
    });
}

AminoGfx.Circle = Circle;

/**
 * Special case for circle.
 */
Circle.prototype.contains = function (pt) {
    var radius = this.radius();
    var dist = Math.sqrt(pt.x * pt.x + pt.y * pt.y);

    return dist < radius;
};

//
// AminoImage
//

var AminoImage = sgtest.AminoImage;

Object.defineProperty(AminoImage.prototype, 'src', {
    set: function (src) {
        //check file
        if (typeof src == 'string') {
            var self = this;

            fs.readFile(src, function (err, data) {
                //check error
                if (err) {
                    if (self.onload) {
                        self.onload(err);
                    }

                    return;
                }

                //get image
                self.loadImage(data, function (err, img) {
                    //call onload
                    if (self.onload) {
                        self.onload(err, img);
                    }
                });
            });

            return;
        }

        //convert buffer
        if (!Buffer.isBuffer(src)) {
            if (this.onload) {
                this.onload(new Exception('buffer expected!'));
            }

            return;
        }

        //native call
        this.loadImage(src, this.onload);
    }
});

exports.AminoImage = AminoImage;

var Core = amino_core.Core;

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
};

/**
 * Native property ids.
 *
 * Note: value has to match native code value
 */
var propsHash = {

    //general
    visible: 18,
    opacity: 27,
    r: 5,
    g: 6,
    b: 7,
    texid: 8,
    w: 10,
    h: 11,
    x: 21,
    y: 22,

    //transforms  (use x and y for translate in X and Y)
    sx: 2,
    sy: 3,
    rz: 4,
    rx: 19,
    ry: 20,

    //text
    text: 9,
    fontSize: 12,
    fontId: 28,
    vAlign: 40,
    wrap: 41,

    //animation
    count: 29,
    lerpprop: 16,
    autoreverse: 35,
    then: 37,
    stop: 38,

    //geometry
    geometry:  24,
    filled:    25,
    closed:    26,
    dimension: 36,

    //rectangle texture
    textureLeft:   30,
    textureRight:  31,
    textureTop:    32,
    textureBottom: 33,

    //clipping
    cliprect: 34
};

/**
 * Vertial text alignment property values.
 */
var textVAlignHash = {
    baseline: 0x0,
    top:      0x1,
    middle:   0x2,
    bottom:   0x3
};

/**
 * Text wrapping property values.
 */
var textWrapHash = {
    none: 0x0,
    end:  0x1,
    word: 0x2
};

/**
 * JSFont constructor.
 */
function JSFont(desc) {
    this.name = desc.name;

    //regular
    var reg = desc.weights[400];

    //properties
    this.desc = desc;
    this.weights = {};
    this.filepaths = {};

    //path
    var aminodir = path.join(__dirname, 'resources/');

    if (desc.path) {
        aminodir = desc.path;
    }

    //iterate weights
    for (var weight in desc.weights) {
        //load normal style
        //FIXME style not used
        var filepath = path.join(aminodir, desc.weights[weight].normal);

        if (!fs.existsSync(filepath)) {
            throw new Error('WARNING. File not found: ' + filepath);
        }

        this.weights[weight] = Core.getCore().getNative().createNativeFont(filepath);
        this.filepaths[weight] = filepath;
    }

    this.getNative = function (size, weight, style) {
        //FIXME style not used; size not used
        if (this.weights[weight]) {
            return this.weights[weight];
        }

        console.log('ERROR. COULDN\'T find the native for ' + size + ' ' + weight + ' ' + style);

        //return regular
        return this.weights[400];
    };

    /**
     * @func calcStringWidth(string, size, weight, style)
     *
     * returns the width of the specified string rendered at the specified size
     */
    this.calcStringWidth = function (str, size, weight, style) {
        return sgtest.getCharWidth(str, size, this.getNative(size, weight, style));
    };

    /**
     * Get font height.
     */
    this.getHeight = function (size, weight, style) {
        if (!size) {
            throw new Error('SIZE IS UNDEFINED');
        }

        return sgtest.getFontHeight(size, this.getNative(size, weight, style));
    };

    /**
     * Get font height metrics.
     *
     * Returns ascender & descender values.
     */
    this.getHeightMetrics = function (size, weight, style) {
        if (!size) {
            throw new Error('SIZE IS UNDEFINED');
        }

        return {
            ascender: sgtest.getFontAscender(size, this.getNative(size, weight, style)),
            descender: sgtest.getFontDescender(size, this.getNative(size, weight, style))
        };
    };
}

//
// Anim
//

var Anim = AminoGfx.Anim;

Anim.prototype.init = function () {
    this._from = null;
    this._to = null;
    this._duration = 1000;
    this._loop = 1;
    this._delay = 0;
    this._autoreverse = false;
    this._timeFunc = 'cubicInOut';
    this._then_fun = null;
};

Anim.prototype.from = function (val) {
    this._from = val;

    return this;
};

Anim.prototype.to = function (val) {
    this._to = val;

    return this;
};

Anim.prototype.dur = function (val) {
    this._duration = val;

    return this;
};

Anim.prototype.delay = function (val) {
    this._delay = val;

    return this;
};

Anim.prototype.loop = function (val) {
    this._loop = val;

    return this;
};

Anim.prototype.then = function (fun) {
    this._then_fun = fun;

    return this;
};

Anim.prototype.autoreverse = function(val) {
    this._autoreverse = val;

    return this;
};

//Time function values.
var timeFuncs = ['linear', 'cubicIn', 'cubicOut', 'cubicInOut'];

Anim.prototype.timeFunc = function (val) {
    if (timeFuncs.indexOf(value) === -1) {
        throw new Error('unknown time function: ' + val);
    }

    this._timeFunc = tf;

    return this;
};

/*
 * Start the animation.
 */
Anim.prototype.start = function () {
    if (DEBUG) {
        console.log('starting anim');
    }

    var self = this;

    setTimeout (function () {
        if (DEBUG) {
            console.log('after delay. making it.');
        }

        //validate
        if (self._from == null) {
            throw new Error('missing from value');
        }

        if (self._to == null) {
            throw new Error('missing to value');
        }

        //native start
        self._start({
            from: self._from,
            to: self._to,
            duration : self._duration,
            count: self._loop,
            autoreverse: self._autoreverse,
            timeFunc: self._timeFunc,
            then: self._then_fun
        });
    }, this._delay);

    return this;
};

//native bindings

var gl_native = {
    createNativeFont: function (filename) {
        return sgtest.createNativeFont(filename, path.join(__dirname, '/resources/shaders'));
    },
    registerFont: function (args) {
        fontmap[args.name] = new JSFont(args);
    },
    getRegisteredFonts: function () {
        return fontmap;
    },
    getFont: function (name) {
        return fontmap[name];
    },
    textVAlignHash: textVAlignHash,
    textWrapHash: textWrapHash,
    getTextLineCount: function (handle) {
        if (handle == undefined) {
            throw new Error('Can\'t set a property on an undefined handle!!');
        }

        return sgtest.getTextLineCount(handle);
    },
    getTextHeight: function (handle) {
        if (handle == undefined) {
            throw new Error('Can\'t set a property on an undefined handle!!');
        }

        return sgtest.getTextHeight(handle);
    },
    createPoly: function ()  {          return sgtest.createPoly();           },
    createText: function () {           return sgtest.createText();           },
    loadImage: function (src, cb) {
        var img = new AminoImage();

        img.onload = function (err) {
            if (err) {
                if (DEBUG) {
                    console.log('could not load image');
                }

                cb();
                return;
            }

            //debug
            //console.log('image buffer: w=' + ibuf.w + ' h=' + ibuf.h + ' bpp=' + ibuf.bpp + ' len=' + ibuf.buffer.length);

            //load texture
            var nat = Core.getCore().getNative();

            //TODO refactor (async, must be called in OpenGL loop)
            nat.loadBufferToTexture(-1, img.w, img.h, img.bpp, img.buffer, function (texture) {
                cb(texture);
            });
        };

        img.src = src;
    },
    loadBufferToTexture: function (texid, w, h, bpp, buf, cb) {
        return cb(sgtest.loadBufferToTexture(texid, w,h, bpp, buf));
    }
};

exports.input = amino_core.input;

/**
 * Create properties.
 */
function makeProps(obj, props) {
    for (var name in props) {
        makeProp(obj, name, props[name]);
    }

    return obj;
};

/**
 * Create property handlers.
 *
 * @param obj object reference.
 * @param name property name.
 * @param val default value.
 */
function makeProp(obj, name, val) {
    /**
     * Property function.
     *
     * Getter and setter.
     */
    var prop = function (v, nativeCall) {
        if (v != undefined) {
            return prop.set(v, obj, nativeCall);
        } else {
            return prop.get();
        }
    };

    prop.value = val;
    prop.propName = name;
    prop.readonly = false;
    prop.nativeListener = null;
    prop.listeners = [];

    /**
     * Add watch callback.
     *
     * Callback: (value, property, object)
     */
    prop.watch = function (fun) {
        if (!fun) {
            throw new Error('function undefined for property ' + name + ' on object with value ' + val);
        }

        this.listeners.push(fun);

        return this;
    };

    /**
     * Unwatch a registered function.
     */
    prop.unwatch = function (fun) {
        var n = this.listeners.indexOf(fun);

        if (n == -1) {
            throw new Error('function was not registered');
        }

        this.listeners.splice(n, 1);
    };

    /**
     * Getter function.
     */
    prop.get = function () {
        return this.value;
    };

    /**
     * Setter function.
     */
    prop.set = function (v, obj, nativeCall) {
        //check readonly
        if (this.readonly) {
            //ignore any changes
            return obj;
        }

        //check if modified
        if (v === this.value) {
            //debug
            //console.log('not changed: ' + name);

            return obj;
        }

        //update
        this.value = v;

        //native listener
        if (this.nativeListener && !nativeCall) {
            //prevent recursion in case of updates from native side
            this.nativeListener(this.value, this.propId, obj);
        }

        //fire listeners
        for (var i = 0; i < this.listeners.length; i++) {
            this.listeners[i](this.value, this, obj);
        }

        return obj;
    };

    /**
     * Create animation.
     */
    prop.anim = function () {
        if (!obj.amino) {
            throw new Error('not an amino object');
        }

        if (!this.propId) {
            throw new Error('property cannot be animated');
        }

        return new AminoGfx.Anim(obj.amino, obj, this.propId);
    };

    /**
     * Bind to other property.
     *
     * Optional: callback to modify value.
     */
    prop.bindto = function (prop, fun) {
        var set = this;

        prop.listeners.push(function (v) {
            if (fun) {
                set(fun(v));
            } else {
                set(v);
            }
        });

        return this;
    };

    //attach
    obj[name] = prop;
};

exports.makeProps = makeProps;

//basic
exports.Polygon   = amino_core.Polygon;
exports.Circle    = amino_core.Circle;
exports.Text      = amino_core.Text;
exports.ImageView = amino_core.ImageView;

//extended
exports.PixelView    = amino_core.PixelView;
exports.RichTextView = amino_core.RichTextView;

//initialize input handler
exports.input.init(OS);

//exports.PureImageView = amino.primitives.PureImageView;
//exports.ConstraintSolver = require('./src/ConstraintSolver');
