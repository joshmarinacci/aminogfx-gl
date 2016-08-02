'use strict';

var DEBUG = false;
var DEBUG_FPS = false;

if (DEBUG) {
    console.log('inside of the aminogfx main.js');
}

//load native module
var binary = require('node-pre-gyp');
var path = require('path');
var binding_path = binary.find(path.resolve(path.join(__dirname, 'package.json')));
var native = require(binding_path);

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
var AminoGfx = native.AminoGfx;

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

    //root wrapper
    this.setRoot(this.createGroup());

    //input handler
    this.inputHandler = input.createEventHandler(this);
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

/**
 * Start renderer.
 */
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

            //init shaders (in current context)
            self.GL = AminoGfx.GL;
            shaders.init(self, OS);
            self.initFontShader(path.join(__dirname, '/resources/shaders'));

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
 * Create image view element.
 */
AminoGfx.prototype.createImageView = function () {
    return new AminoGfx.ImageView(this);
};

/**
 * Create image view element.
 */
AminoGfx.prototype.createTexture = function () {
    return new AminoGfx.Texture(this);
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

/**
 * Create text element.
 */
AminoGfx.prototype.createText = function () {
    return new AminoGfx.Text(this);
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

/**
 * Handle an event.
 */
AminoGfx.prototype.handleEvent = function (evt) {
    //debug
    //console.log('Event: ' + JSON.stringify(evt));

    //add timestamp
    evt.time = new Date().getTime();

    //pass to event processor
    this.inputHandler.processEvent(evt);
};

/**
 * Destroy renderer.
 */
AminoGfx.prototype.destroy = function () {
    if (this.timer) {
        clearImmediate(this.timer);
        this.timer = null;
    }

    this._destroy();

/*
    //call async (outside event handler callbacks)
    var self = this;

    setImmediate(function () {
        self._destroy();
    });
*/
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

/**
 * Find a node at a certain position with an optional filter callback.
 */
AminoGfx.prototype.findNodesAtXY = function (pt, filter) {
    return findNodesAtXY_helper(this.root, pt, filter, '');
};

function findNodesAtXY_helper(root, pt, filter, tab) {
    //verify
    if (!root) {
        return [];
    }

    if (!root.visible()) {
        return [];
    }

    //console.log(tab + "   xy",pt.x,pt.y, root.id());

    var tpt = pt.minus(root.x(), root.y()).divide(root.sx(), root.sy());

    //handle children first, then the parent/root
    var res = [];

    if (filter) {
        if (!filter(root)) {
            return res;
        }
    }

    if (root.children && root.children.length && root.children.length > 0) {
        for (var i = root.children.length - 1; i >= 0; i--) {
            var node = root.children[i];
            var found = findNodesAtXY_helper(node, tpt, filter, tab + '  ');

            res = res.concat(found);
        }
    }

    if (root.contains && root.contains(tpt)) {
        res = res.concat([root]);
    }

    return res;
}

/**
 * Find a node at a certain position.
 */
AminoGfx.prototype.findNodeAtXY = function (x, y) {
    return findNodeAtXY_helper(this.root, x, y, '');
};

function findNodeAtXY_helper(root, x, y, tab) {
    if (!root) {
        return null;
    }

    if (!root.visible()) {
        return null;
    }

    var tx = x - root.x();
    var ty = y - root.y();

    tx /= root.sx();
    ty /= root.sy();

    //console.log(tab + "   xy="+tx+","+ty);

    if (root.cliprect && root.cliprect()) {
        if (tx < 0) {
            return false;
        }

        if (tx > root.w()) {
            return false;
        }

        if (ty < 0) {
            return false;
        }

        if (ty > root.h()) {
            return false;
        }
    }

    if (root.children) {
        //console.log(tab+"children = ",root.children.length);

        for (var i = root.children.length - 1; i >= 0; i--) {
            var node = root.children[i];
            var found = this.findNodeAtXY_helper(node, tx, ty, tab+ '  ');

            if (found) {
                return found;
            }
        }
    }

    //console.log(tab+"contains " + tx+' '+ty);

    if (root.contains && root.contains(tx, ty)) {
        //console.log(tab,"inside!",root.getId());

        return root;
    }

    return null;
};

AminoGfx.prototype.globalToLocal = function (pt, node) {
    return globalToLocal_helper(pt,node);
};

function globalToLocal_helper(pt, node) {
    if (node.parent) {
    	pt = globalToLocal_helper(pt,node.parent);
    }

    return input.makePoint(
        (pt.x - node.x()) / node.sx(),
        (pt.y - node.y()) / node.sy()
    );
};

/*
function calcGlobalToLocalTransform(node) {
    if (node.parent) {
        var trans = calcGlobalToLocalTransform(node.parent);

        if (node.getScalex() != 1) {
            trans.x /= node.sx();
            trans.y /= node.sy();
        }

        trans.x -= node.x();
        trans.y -= node.y();

        return trans;
    }

    return {
        x: -node.x(),
        y: -node.y()
    };
}
*/

AminoGfx.prototype.localToGlobal = function (pt, node) {
    pt = {
        x: pt.x + node.x() * node.sx(),
        y: pt.y + node.y() * node.sx()
    };

    if (node.parent) {
        return this.localToGlobal(pt, node.parent);
    } else {
        return pt;
    }
};

AminoGfx.prototype.on = function (name, target, listener) {
    this.inputHandler.on(name, target, listener);
};

exports.AminoGfx = AminoGfx;

//
// AminoGfx.Group
//

var Group = AminoGfx.Group;

/**
 * Initializer.
 */
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

        //origin
        originX: 0,
        originY: 0,

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

/**
 * Add one or more children to this group.
 */
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

/**
 * Remove one or more children from this group.
 */
Group.prototype.remove = function (child) {
    var n = this.children.indexOf(child);

    if (n >=  0) {
        this._remove(child);
        this.children.splice(n, 1);
        child.parent = null;
    }

    return this;
};

/**
 * Remove all children.
 */
Group.prototype.clear = function () {
    var count = this.children.length;

    for (var i = 0; i < count; i++) {
        this._remove(this.children[i]);
    }

    this.children = [];

    return this;
};

/**
 * Bring child to top.
 */
Group.prototype.raiseToTop = function (node) {
    if (!node) {
        throw new Error('can\'t move a null child');
    }

    this.remove(node);
    this.add(node);

    return this;
};

/**
 * Find children.
 */
Group.prototype.find = function (pattern) {
    var results = new FindResults();

    if (pattern.indexOf('#') == 0) {
        //id
        var id = pattern.substring(1);

        results.children = treeSearch(this, false, function (child) {
            return (child.id().toLowerCase() == id);
        });
    } else {
        //look for class name (e.g. AminoRect)
        pattern = 'amino' + pattern.toLowerCase();

        results.children = treeSearch(this, false, function (child) {
            return child.constructor.name.toLowerCase() == pattern;
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

        //origin
        originX: 0,
        originY: 0,

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
};

Rect.prototype.contains = contains;

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
// ImageView
//

var ImageView = AminoGfx.ImageView;

ImageView.prototype.init = function () {
    makeProps(this, {
        id: '',
        visible: true,

        //positon
        x: 0,
        y: 0,

        //size
        w: 100,
        h: 100,

        //origin
        originX: 0,
        originY: 0,

        //scaling
        sx: 1,
        sy: 1,

        //rotation
        rx: 0,
        ry: 0,
        rz: 0,

        //texture coordinates
        left:   0,
        right:  1,
        top:    0,
        bottom: 1,

        //image
        src: null,
        image: null,
        opacity: 1.0,
        size: 'resize'
    });

    var self = this;

    //actually load the image
    this.src.watch(function (src) {
        if (!src) {
            self.image(null);
            return;
        }

        //load image from source
        var img = new AminoImage();

        img.onload = function (err) {
            if (err) {
                if (DEBUG) {
                    console.log('could not load image: ' + err.message);
                }

                cb();
                return;
            }

            //debug
            //console.log('image buffer: w=' + ibuf.w + ' h=' + ibuf.h + ' bpp=' + ibuf.bpp + ' len=' + ibuf.buffer.length);

            //load texture
            var amino = self.amino;
            var texture = amino.createTexture(amino);

            texture.loadTexture(img, function (err, texture) {
                if (err) {
                    if (DEBUG) {
                        console.log('could not load texture: ' + err.message);
                    }

                    return;
                }

                //use texture
                self.image(texture);
            });
        };

        img.src = src;
    });

    //when the image is loaded, update the dimensions
    this.image.watch(function (texture) {
        //set size
        applySize(texture, self.size());
    });

    this.size.watch(function (mode) {
        applySize(self.image(), mode);
    });

    function applySize(texture, mode) {
        switch (mode) {
            case 'resize':
                if (texture) {
                    self.w(texture.w);
                    self.h(texture.h);
                }
                break;

            case 'stretch':
                //keep size
                break;

            case 'cover':
            case 'contain':
                //cover
                if (texture) {
                    var w = self.w();
                    var h = self.h();
                    var imgW = texture.w;
                    var imgH = texture.h;

                    //fit width
                    imgH *= w / imgW;
                    imgW = w;

                    if (mode == 'cover') {
                        if (imgH < h) {
                            //stretch height
                            imgW *= h / imgH;
                            imgH = h;
                        }
                    } else {
                        if (imgH > h) {
                            var fac = h / imgH;

                            imgW *= fac;
                            imgH *= fac;
                        }
                    }

                    //debug
                    //console.log('fit ' + imgW + 'x' + imgH + ' to ' + w + 'x' + h);

                    //center texture
                    var horz = (imgW - w) / 2 / imgW;

                    self.left(horz);
                    self.right(1 - horz);

                    var vert = (imgH - h) / 2 / imgH;

                    self.top(vert);
                    self.bottom(1 - vert);
                }
                break;
        }
    }
};

ImageView.prototype.contains = contains;

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

        //rotation
        rx: 0,
        ry: 0,
        rz: 0,

        //fill
        fill: '#ff0000',
        fillR: 1,
        fillG: 0,
        fillB: 0,
        opacity: 1.,

        //properties
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

var Circle = extendNative(Polygon);

AminoGfx.Circle = Circle;

/**
 * Extend a native rendering object.
 *
 * Note: JavaScript does not allow extending native template. Therefore a new template instance has to be created first.
 */
function extendNative(constr) {
    var templ = constr.newTemplate();

    for (var item in constr.prototype) {
        //debug
        //console.log('copying: ' + item);

        templ[item] = constr.prototype[item];
    }

    return templ;
}

Circle.prototype.init = function () {
    //get Polygon properties
    Polygon.prototype.init.call(this);

    //bindings
    makeProps(this, {
        radius: 0,
        steps: 30
    });

    this.dimension(2);

    //monitor radius updates
    var self = this;

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
};

Circle.prototype.initDone = function () {
    this.radius(50);
};

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

var AminoImage = native.AminoImage;

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
                this.onload(new Error('buffer expected!'));
            }

            return;
        }

        //native call
        this.loadImage(src, this.onload);
    }
});

exports.AminoImage = AminoImage;

//
// AminoFonts
//

var AminoFonts = native.AminoFonts;

AminoFonts.prototype.init = function () {
    this.fonts = {};
    this.cache = {};

    //default fonts
    this.registerFont({
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
    });

    this.registerFont({
        name: 'awesome',
        weights: {
            400: {
                normal: 'fontawesome-webfont.ttf'
            }
        }
    });

    //default
    this.defaultFont = this.fonts.source;
};

/**
 * Register a font.
 */
AminoFonts.prototype.registerFont = function (font) {
    this.fonts[font.name] = font;

    //collect weights
    var weightList = [];

    for (var weight in font.weights) {
        weightList.push(weight);
    }

    weightList.sort();
    font.weightList = weightList;

    return this;
};

/**
 * Get a font.
 */
AminoFonts.prototype.getFont = function (descr, callback) {
    if (!descr) {
        descr = {};
    }

    var name = descr.name || this.defaultFont.name;
    var size = Math.round(descr.size || 20);
    var weight = descr.weight || 400;
    var style = descr.style || 'normal';

    //console.log('getFont() ' + name + ' ' + size + ' ' + weight + ' ' + style);

    //get font descriptor
    var font = this.fonts[name];

    if (!font) {
        callback(new Error('font ' + name + ' not found'));
        return this;
    }

    if (!size) {
        callback(new Error('invalid font size'));
        return this;
    }

    //find weight
    var weightDesc = font.weights[weight];

    if (!weightDesc) {
        //find closest
        var closest = null;
        var diff = 0;

        for (var item in font.weights) {
            var diff2 = Math.abs(weight - item);

            if (!closest || diff2 < diff) {
                closest = item;
                diff = diff2;
            }
        }

        if (!closest) {
            callback(new Error('no font weights found for ' + name));
            return this;
        }

        weightDesc = font.weights[closest];
        weight = closest;
    }

    //find style
    var styleDesc = weightDesc[style];

    if (!styleDesc) {
        //fallback to normal
        styleDesc = weightDesc.normal;

        if (!styleDesc) {
            callback(new Error('no normal style found: ' + name + ' weight=' + weight + ' style=' + style));
            return this;
        }

        style = 'normal';
    }

    //check cache
    var key = name + '-' + weight + '-' + style;
    var cached = this.cache[key];

    if (cached) {
        if (cached instanceof Promise) {
            cached.then(function (font) {
                font.getSize(size, callback);
            }, function (err) {
                callback(err);
            });
        } else {
            cached.getSize(size, callback);
        }

        return this;
    }

    //path
    var dir = font.path;

    if (!dir) {
        //internal default path
        dir = path.join(__dirname, 'resources/');
    }

    //load file
    var file = path.join(dir, styleDesc);
    var self = this;

    var promise = new Promise(function (resolve, reject) {
        fs.readFile(file, function (err, data) {
            if (err) {
                reject(err);
                return;
            }

            //throws exception on error
            var font = new AminoFonts.Font(self, {
                data: data,
                file: file,

                name: name,
                weight: weight,
                style: style
            });

            resolve(font);
        });
    });

    this.cache[key] = promise;

    promise.then(function (font) {
        self.cache[key] = font;

        font.getSize(size, callback);
    }, function (err) {
        callback(err);
    });

    return this;
};

var fonts = new AminoFonts();

exports.fonts = fonts;

//
// AminoFonts.Font
//

var AminoFont = AminoFonts.Font;

AminoFont.prototype.init = function () {
    this.fontSizes = {};
};

/**
 * Load font size.
 */
AminoFont.prototype.getSize = function (size, callback) {
    //check cache
    var fontSize = this.fontSizes[size];

    if (!fontSize) {
        fontSize = new AminoFonts.FontSize(this, size);
        this.fontSizes[size] = fontSize;
    }

    callback(null, fontSize);
};

//
// AminoFonts.FontSize
//

var AminoFontSize = AminoFonts.FontSize;

AminoFontSize.prototype.calcTextWidth = function (text, callback) {
    callback(null, this._calcTextWidth(text));
};

//
// AminoGfx.Text
//

var Text = AminoGfx.Text;

Text.prototype.init = function () {
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
        w: 0,
        h: 0,

        //origin
        originX: 0,
        originY: 0,

        //scaling
        sx: 1,
        sy: 1,

        //rotation
        rx: 0,
        ry: 0,
        rz: 0,

        //font
        text:       '',
        fontSize:   20,
        fontName:   'source',
        fontWeight: 400,
        fontStyle:  'normal',
        font:       null,

        //color
        r: 1,
        g: 1,
        b: 1,
        opacity: 1.0,
        fill: '#ffffff',

        //alignment
        vAlign: 'baseline',
        wrap:   'none'
    });

    this.fill.watch(watchFill);

    //TODO lines
    //TODO textHeight
};

Text.prototype.initDone = function () {
    //update font
    this.updateFont(null, null, this);

    //watchers
    this.fontName.watch(this.updateFont);
    this.fontWeight.watch(this.updateFont);
    this.fontSize.watch(this.updateFont);
};

var fontId = 0;

/**
 * Load the font.
 */
Text.prototype.updateFont = function (val, prop, obj) {
    //prevent race condition (earlier font is loaded later)
    var id = fontId++;

    obj.latestFontId = id;

    //get font
    fonts.getFont({
        name: obj.fontName(),
        size: obj.fontSize(),
        weight: obj.fontWeight(),
        style: obj.fontStyle(),
    }, function (err, font) {
        if (err) {
            console.log('could not load font: ' + err.message);

            //try default font
            fonts.getFont({
                size: obj.fontSize()
            }, function (err, font) {
                if (font && obj.latestFontId === id) {
                    obj.latestFontId = undefined;

                    obj.font(font);
                }
            });
            return;
        }

        //console.log('got font: ' + JSON.stringify(font));

        if (obj.latestFontId === id) {
            obj.latestFontId = undefined;

            obj.font(font);
        }
    });

    fontId++;
};

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

    this.started = false;
};

Anim.prototype.from = function (val) {
    this.checkStarted();

    this._from = val;

    return this;
};

Anim.prototype.to = function (val) {
    this.checkStarted();

    this._to = val;

    return this;
};

Anim.prototype.dur = function (val) {
    this.checkStarted();

    this._duration = val;

    return this;
};

Anim.prototype.delay = function (val) {
    this.checkStarted();

    this._delay = val;

    return this;
};

Anim.prototype.loop = function (val) {
    this.checkStarted();

    this._loop = val;

    return this;
};

Anim.prototype.then = function (fun) {
    this.checkStarted();

    this._then_fun = fun;

    return this;
};

Anim.prototype.autoreverse = function(val) {
    this.checkStarted();

    this._autoreverse = val;

    return this;
};

//Time function values.
var timeFuncs = ['linear', 'cubicIn', 'cubicOut', 'cubicInOut'];

Anim.prototype.timeFunc = function (val) {
    this.checkStarted();

    if (timeFuncs.indexOf(value) === -1) {
        throw new Error('unknown time function: ' + val);
    }

    this._timeFunc = tf;

    return this;
};

Anim.prototype.checkStarted = function () {
    if (this.started) {
        throw new Error('immutable after start() was called');
    }
};

/*
 * Start the animation.
 */
Anim.prototype.start = function () {
    if (this.started) {
        throw new Error('animation already started');
    }

    if (DEBUG) {
        console.log('starting anim');
    }

    var self = this;

    this.started = true;

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

        function watcher(v) {
            if (fun) {
                set(fun(v));
            } else {
                set(v);
            }
        }

        prop.listeners.push(watcher);

        //apply current value
        watcher(prop());

        return this;
    };

    //attach
    obj[name] = prop;
};

exports.makeProps = makeProps;

//input
var input = require('./src/core/aminoinput');

// initialize input handler
input.init(OS);

exports.input = input;

//extended
//exports.PixelView    = amino_core.PixelView;
//exports.RichTextView = amino_core.RichTextView;

//exports.PureImageView = amino.primitives.PureImageView;
//exports.ConstraintSolver = require('./src/ConstraintSolver');
