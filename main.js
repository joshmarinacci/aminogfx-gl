'use strict';

const DEBUG = false;
const DEBUG_ERRORS = true;

if (DEBUG) {
    console.log('inside of the aminogfx main.js');
}

//load native module
const binary = require('node-pre-gyp');
const path = require('path');
const binding_path = binary.find(path.resolve(path.join(__dirname, 'package.json')));
const native = require(binding_path);

const fs = require('fs');
const util =  require('util');

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
function parseRGBString(fill) {
    if (typeof fill == 'string') {
        //strip off any leading #
        if (fill.substring(0, 1) == '#') {
            fill = fill.substring(1);
        }

        //pull out the components
        var r = parseInt(fill.substring(0, 2), 16);
        var g = parseInt(fill.substring(2, 4), 16);
        var b = parseInt(fill.substring(4, 6), 16);

        return {
            r: r / 255,
            g: g / 255,
            b: b / 255
        };
    } else if (Array.isArray(fill)) {
        return {
            r: fill[0],
            g: fill[1],
            b: fill[2]
        };
    }

    return fill;
}

/**
 * Start renderer.
 */
AminoGfx.prototype.start = function (done) {
    var self = this;

    //pass to native code
    this._start(function (err) {
        if (err) {
            done.call(self, err);
            return;
        }

        //ready (Note: this points to the instance)
        done.call(self, err);

        //check root
        if (!self.root) {
            throw new Error('Missing root!');
        }
    });
};

/**
 * Set the root node.
 */
AminoGfx.prototype.setRoot = function (root) {
    this.root = root;

    this._setRoot(root);
};

/**
 * Get the root node.
 */
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
 * Create pixel view element.
 */
AminoGfx.prototype.createPixelView = function () {
    return new AminoGfx.PixelView(this);
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
 * Create model element.
 */
AminoGfx.prototype.createModel = function () {
    return new AminoGfx.Model(this);
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
    //clear root copy
    this.root = null;

    this._destroy();

/*
    //call async (outside event handler callbacks)
    var self = this;

    setImmediate(function () {
        self._destroy();
    });
*/
};

/**
 * Set position.
 */
AminoGfx.prototype.setPosition = setPosition;

function setPosition(x, y, z) {
    this.x(x).y(y);

    if (z !== undefined) {
        this.z(z);
    }
}

/**
 * Set size.
 */
AminoGfx.prototype.setSize = setSize;

function setSize(w, h) {
    this.w(w).h(h);
};

/**
 * Get runtime system info.
 */
AminoGfx.prototype.getStats = function () {
    var stats = this._getStats();
    var mem = process.memoryUsage();

    stats.heapUsed = mem.heapUsed;
    stats.heapTotal = mem.heapTotal;

    return stats;
};

/**
 * Find node with id.
 */
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

    if (root.cliprect && root.clipRect()) {
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
        z: 0,

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
        clipRect: false,

        //3D rendering (depth test)
        depth: false
    });

    this.isGroup = true;
    this.children = [];
};

/**
 * Set position.
 */
Group.prototype.setPosition = setPosition;

/**
 * Set size.
 */
Group.prototype.setSize = setSize;

/**
 * Scale.
 */
Group.prototype.scale = scaleFunc;

function scaleFunc(sx, sy) {
    this.sx(sx).sy(sy);
}

/**
 * Rotate.
 */
Group.prototype.rotate = rotateFunc;

function rotateFunc(rx, ry, rz) {
    this.rx(rx).ry(ry).rz(rz);
}

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
 * Insert a child at a certain position.
 */
Group.prototype.insertAt = function (item, pos) {
    if (!item) {
        throw new Error('can\'t add a null child to a group');
    }

    if (this.children.indexOf(item) != -1) {
        throw new Error('child was added before');
    }

    if (item == this || item.parent) {
        throw new Error('already added to different group');
    }

    this._insert(item, pos);
    this.children.splice(pos, 0, item);
    item.parent = this;

    return this;
};

/**
 * Insert before a sibling.
 */
Group.prototype.insertBefore = function (item, sibling) {
    var pos = this.children.indexOf(sibling);

    if (pos == -1) {
        //add at end
        this.add(item);

        return false;
    }

    return this.insertAt(item, pos);
};

/**
 * Insert after a sibling.
 */
Group.prototype.insertAfter = function (item, sibling) {
    var pos = this.children.indexOf(sibling);

    if (pos == -1) {
        //add at end
        this.add(item);

        return false;
    }

    return this.insertAt(item, pos + 1);
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
        z: 0,

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

/**
 * Check if point is inside of rect.
 */
Rect.prototype.contains = contains;

/**
 * Set position.
 */
Rect.prototype.setPosition = setPosition;

/**
 * Set size.
 */
Rect.prototype.setSize = setSize;

/**
 * Scale.
 */
Rect.prototype.scale = scaleFunc;

/**
 * Rotate.
 */
Rect.prototype.rotate = rotateFunc;

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
        z: 0,

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

        size: 'resize',
        repeat: 'no-repeat'
    });

    var self = this;

    //actually load the image
    this.src.watch(setSrc);

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

/**
 * Set texture.
 */
function setImage(img, obj) {
    if (obj.image) {
        obj.image(img);
    } else {
        obj.texture(img);
    }
}

/**
 * Load and set texture.
 */
function loadTexture(obj, img) {
    var amino = obj.amino;
    var texture = amino.createTexture();

    texture.loadTextureFromImage(img, function (err, texture) {
        if (err) {
            if (DEBUG || DEBUG_ERRORS) {
                console.log('could not load texture: ' + err.message);
            }

            return;
        }

        //use texture
        setImage(texture, obj);
    });
}

/**
 * Src handler.
 *
 * Supports: local files, images, image buffers & textures
 */
function setSrc(src, prop, obj) {
    if (!src) {
        setImage(null, obj);
        return;
    }

    //check image (JPEG, PNG)
    if (src instanceof AminoImage) {
        //load texture
        loadTexture(obj, src);
        return;
    }

    //load image from source
    var img = new AminoImage();

    img.onload = function (err) {
        if (err) {
            if (DEBUG || DEBUG_ERRORS) {
                console.log('could not load image: ' + err.message);
            }

            //set texture to null
            setImage(null, obj);
            return;
        }

        //debug
        //console.log('image buffer: w=' + ibuf.w + ' h=' + ibuf.h + ' bpp=' + ibuf.bpp + ' len=' + ibuf.buffer.length);

        //load texture
        loadTexture(obj, img);
    };

    img.src = src;
}

/**
 * Check if point inside of image view.
 */
ImageView.prototype.contains = contains;

/**
 * Set position.
 */
ImageView.prototype.setPosition = setPosition;

/**
 * Set size.
 */
ImageView.prototype.setSize = setSize;

/**
 * Scale.
 */
ImageView.prototype.scale = scaleFunc;

/**
 * Rotate.
 */
ImageView.prototype.rotate = rotateFunc;

//
// PixelView
//

class PixelView extends ImageView {
    /**
     * Constructor.
     */
    constructor(amino) {
        super(amino);
    }
}

AminoGfx.PixelView = PixelView;

PixelView.prototype.init = function () {
    //get Rect properties
    ImageView.prototype.init.call(this);

    //bindings
    makeProps(this, {
        pw: 100,
        ph: 100,
        bpp: 4
    });
};

PixelView.prototype.initDone = function () {
    this.pw.watch(rebuildBuffer);
    this.ph.watch(rebuildBuffer);

    var self = this;

    function rebuildBuffer() {
        var w = self.pw();
        var h = self.ph();
        var len = w * h * 4;

        if (!self.buf || self.buf.length != len) {
            self.buf = new Buffer(len);
        }

        var c1 = [0, 0, 0];
        var c2 = [255, 255, 255];
        var buf = self.buf;

        for (var x = 0; x < w; x++) {
            for (var y = 0; y < h; y++) {
                var i = (x + y * w) * 4;
                var c;

                if (x % 3 == 0) {
                    c = c1;
                } else {
                    c = c2;
                }

                buf[i + 0] = c[0];
                buf[i + 1] = c[1];
                buf[i + 2] = c[2];
                buf[i + 3] = 255;
            }
        }

        self.updateTexture();
    };

    //texture
    this.texture = this.amino.createTexture();
    this.image(this.texture);

    rebuildBuffer();
};

PixelView.prototype.updateTexture = function () {
    this.texture.loadTextureFromBuffer({
        buffer: this.buf,
        w: this.pw(),
        h: this.ph(),
        bpp: this.bpp()
    }, function (err) {
        if (err) {
            if (DEBUG_ERRORS) {
                console.log('Could not create texture!');
            }

            return;
        }
    });
};

PixelView.prototype.setPixel = function (x, y, r, g, b, a) {
    var w = this.pw();
    var i = (x + y * w) * 4;

    var buf = this.buf;

    buf[i + 0] = r;
    buf[i + 1] = g;
    buf[i + 2] = b;
    buf[i + 3] = a;
};

PixelView.prototype.setPixeli32 = function (x, y, int) {
    var w = this.pw();
    var i = (x + y * w) * 4;

    this.buf.writeUInt32BE(int, i);
};

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
        z: 0,

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
        geometry: null
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

/**
 * Check if point inside of polygon.
 */
Polygon.prototype.contains = function () {
    //TODO check polygon coords
    return false;
};

/**
 * Set position.
 */
Polygon.prototype.setPosition = setPosition;

/**
 * Set size.
 */
Polygon.prototype.setSize = setSize;

/**
 * Scale.
 */
Polygon.prototype.scale = scaleFunc;

/**
 * Rotate.
 */
Polygon.prototype.rotate = rotateFunc;

//
// Model
//

var Model = AminoGfx.Model;

Model.prototype.init = function () {
    //bindings
    makeProps(this, {
        id: '',
        visible: true,

        //position
        x: 0,
        y: 0,
        z: 0,

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

        //fill (color mode)
        fill: '#ff0000',
        fillR: 1,
        fillG: 0,
        fillB: 0,
        opacity: 1.,

        //properties
        vertices: null,
        indices: null,
        normals: null, //enables lighting
        uvs: null, //enables texture (color used otherwise)

        src: null,
        texture: null
    });

    this.fill.watch(setFill);
    this.src.watch(setSrc);
};

/**
 * Check if point inside of model.
 */
Model.prototype.contains = function () {
    return false;
};

/**
 * Set position.
 */
Model.prototype.setPosition = setPosition;

/**
 * Set size.
 */
Model.prototype.setSize = setSize;

/**
 * Scale.
 */
Model.prototype.scale = scaleFunc;

/**
 * Rotate.
 */
Model.prototype.rotate = rotateFunc;

//
// Circle
//

class Circle extends Polygon {
    /**
     * Constructor.
     */
    constructor(amino) {
        super(amino);
    }
}

AminoGfx.Circle = Circle;

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
        var steps = self.steps();
        var points = new Float32Array(steps * 2);
        var pos = 0;

        for (var i = 0; i < steps; i++) {
            var theta = Math.PI * 2 / steps * i;

            points[pos++] = Math.sin(theta) * r;
            points[pos++] = Math.cos(theta) * r;
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

            //read file async
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
    //check existing font (immutable)
    if (this.fonts[font.name]) {
        return this;
    }

    //add new font
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

/**
 * Calculate text width.
 */
AminoFontSize.prototype.calcTextWidth = function (text, callback) {
    callback(null, this._calcTextWidth(text));
};

//
// AminoGfxTexture
//

var Texture = AminoGfx.Texture;

/**
 * Load texture.
 */
Texture.prototype.loadTexture = function (src, callback) {
    if (!src) {
        this.loadTextureFromBuffer(null, callback);
        return;
    }

    if (src instanceof AminoImage) {
        //image (JPEG, PNG)
        this.loadTextureFromImage(src, callback);
    } else if (Buffer.isBuffer(src)) {
        //pixel buffer
        this.loadTextureFromBuffer(src, callback);
    } else if (src instanceof AminoFontSize) {
        //font texture
        this.loadTextureFromFont(src, callback);
    } else {
        throw new Error('unknown source');
    }
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
        z: 0,

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
        align:  'left',
        vAlign: 'baseline',
        wrap:   'none',

        //lines
        maxLines: 0
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

/**
 * Set position.
 */
Text.prototype.setPosition = setPosition;

/**
 * Set size.
 */
Text.prototype.setSize = setSize;

/**
 * Scale.
 */
Text.prototype.scale = scaleFunc;

/**
 * Rotate.
 */
Text.prototype.rotate = rotateFunc;

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
        //handle errors
        if (err) {
            console.log('could not load font: ' + err.message);

            //try default font
            fonts.getFont({
                size: obj.fontSize()
            }, function (err, font) {
                if (err) {
                    if (DEBUG_ERRORS) {
                        console.log('could not load default font!');
                    }

                    return;
                }

                if (font && obj.latestFontId === id) {
                    obj.latestFontId = undefined;

                    obj.font(font);
                }
            });
            return;
        }

        //attach font

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

/**
 * Initialize instance.
 */
Anim.prototype.init = function () {
    this._from = null;
    this._to = null;
    this._pos = null;
    this._duration = 1000;
    this._loop = 1;
    this._delay = 0;
    this._autoreverse = false;
    this._timeFunc = 'cubicInOut';
    this._then_fun = null;

    this.started = false;
};

/**
 * From animation value.
 */
Anim.prototype.from = function (val) {
    this.checkStarted();

    this._from = val;

    return this;
};

/**
 * To animation value.
 */
Anim.prototype.to = function (val) {
    this.checkStarted();

    this._to = val;

    return this;
};

/**
 * Animation start position value.
 */
Anim.prototype.pos = function (val) {
    this.checkStarted();

    this._pos = val;

    return this;
};

/**
 * Animation duration.
 */
Anim.prototype.dur = function (val) {
    this.checkStarted();

    this._duration = val;

    return this;
};

/**
 * Animation delay.
 */
Anim.prototype.delay = function (val) {
    this.checkStarted();

    this._delay = val;

    return this;
};

/**
 * Animation loop count. Use -1 for forever.
 */
Anim.prototype.loop = function (val) {
    this.checkStarted();

    this._loop = val;

    return this;
};

/**
 * End callback.
 */
Anim.prototype.then = function (fun) {
    this.checkStarted();

    this._then_fun = fun;

    return this;
};

/**
 * Auto reverse animation.
 */
Anim.prototype.autoreverse = function(val) {
    this.checkStarted();

    this._autoreverse = val;

    return this;
};

//Time function values.
var timeFuncs = ['linear', 'cubicIn', 'cubicOut', 'cubicInOut'];

/**
 * Time function.
 */
Anim.prototype.timeFunc = function (value) {
    this.checkStarted();

    if (timeFuncs.indexOf(value) === -1) {
        throw new Error('unknown time function: ' + val);
    }

    this._timeFunc = value;

    return this;
};

/**
 * Internal: check started state.
 */
Anim.prototype.checkStarted = function () {
    if (this.started) {
        throw new Error('immutable after start() was called');
    }
};

/*
 * Start the animation.
 */
Anim.prototype.start = function (refTime) {
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
            pos: self._pos,
            duration: self._duration,
            refTime: refTime,
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
     * Remove all listeners.
     */
    prop.unwatchAll = function () {
        this.listeners = [];
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
    prop.bindTo = function (prop, fun) {
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

//
// AminoWeakReference
//
exports.AminoWeakReference = native.AminoWeakReference;

//input
var input = require('./src/core/aminoinput');

// initialize input handler
input.init();

exports.input = input;

//extended
//exports.RichTextView = amino_core.RichTextView;

//exports.PureImageView = amino.primitives.PureImageView;
//exports.ConstraintSolver = require('./src/ConstraintSolver');
