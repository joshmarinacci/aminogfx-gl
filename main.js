console.log("inside of the aminogfx main.js");

var binary = require('node-pre-gyp');
var path = require('path');
var binding_path = binary.find(path.resolve(path.join(__dirname,'./package.json')));
var sgtest = require(binding_path);
console.log('sgtest = ', sgtest);

var OS = "KLAATU";
if(process.arch == 'arm') {
    OS = "RPI";
}
if(process.platform == "darwin") {
    OS = "MAC";
}


//var amino = require('aminogfx');
var amino_core = require('aminogfx');
var Core = amino_core.Core;
var Shaders = require('./src/shaders.js');
var fs = require('fs');

//amino_core.OS = OS;

var fontmap = {};
var defaultFonts = {
    'source': {
        name:'source',
        weights: {
            200: {
                normal: "SourceSansPro-ExtraLight.ttf",
                italic: "SourceSansPro-ExtraLightItalic.ttf"
            },
            300: {
                normal: "SourceSansPro-Light.ttf",
                italic: "SourceSansPro-LightItalic.ttf"
            },
            400: {
                normal: "SourceSansPro-Regular.ttf",
                italic: "SourceSansPro-Italic.ttf"
            },

            600: {
                normal: "SourceSansPro-Semibold.ttf",
                italic: "SourceSansPro-SemiboldItalic.ttf"
            },
            700: {
                normal: "SourceSansPro-Bold.ttf",
                italic: "SourceSansPro-BoldItalic.ttf"
            },
            900: {
                normal: "SourceSansPro-Black.ttf",
                italic: "SourceSansPro-BlackItalic.ttf"
            }
        }
    },
    'awesome': {
        name:'awesome',
        weights: {
            400: {
                normal: "fontawesome-webfont.ttf",
            }
        }
    }
}

function JSFont(desc) {
    this.name = desc.name;
    var reg = desc.weights[400];
    this.desc = desc;
    this.weights = {};
    this.filepaths = {};

    var dir = process.cwd();
    process.chdir(__dirname+'/..'); // chdir such that fonts (and internal shaders) may be found
    var aminodir = __dirname+'/resources/';
    if(desc.path) {
        aminodir = desc.path;
    }
    for(var weight in desc.weights) {
        var filepath = aminodir+desc.weights[weight].normal;
        if(!fs.existsSync(filepath)) {
            console.log("WARNING. File not found",filepath);
            throw new Error();
        }
        this.weights[weight] = Core.getCore().getNative().createNativeFont(filepath);
        this.filepaths[weight] = filepath;
    }
    process.chdir(dir);


    this.getNative = function(size, weight, style) {
        if(this.weights[weight] != undefined) {
            return this.weights[weight];
        }
        console.log("ERROR. COULDN'T find the native for " + size + " " + weight + " " + style);
        return this.weights[400];
    }

    /** @func calcStringWidth(string, size)  returns the width of the specified string rendered at the specified size */
    this.calcStringWidth = function(str, size, weight, style) {
        amino.GETCHARWIDTHCOUNT++;
        return amino.sgtest.getCharWidth(str,size,this.getNative(size,weight,style));
    }
    this.getHeight = function(size, weight, style) {
        amino.GETCHARHEIGHTCOUNT++;
        if(size == undefined) {
            throw new Error("SIZE IS UNDEFINED");
        }
        return amino.sgtest.getFontHeight(size, this.getNative(size, weight, style));
    }
    this.getHeightMetrics = function(size, weight, style) {
        if(size == undefined) {
            throw new Error("SIZE IS UNDEFINED");
        }
        return {
            ascender: amino.sgtest.getFontAscender(size, this.getNative(size, weight, style)),
            descender: amino.sgtest.getFontDescender(size, this.getNative(size, weight, style))
        };
    }
}

var propsHash = {

    //general
    "visible":18,
    "opacity":27,
    "r":5,
    "g":6,
    "b":7,
    "texid":8,
    "w":10,
    "h":11,
    "x":21,
    "y":22,

    //transforms  (use x and y for translate in X and Y)
    "scalex":2,
    "scaley":3,
    "rotateZ":4,
    "rotateX":19,
    "rotateY":20,

    //text
    "text":9,
    "fontSize":12,
    "fontId":28,

    //animation
    "count":29,
    "lerplinear":13,
    "lerpcubicin":14,
    "lerpcubicout":15,
    "lerpprop":16,
    "lerpcubicinout":17,
    "autoreverse":35,


    //geometry
    "geometry":24,
    "filled":25,
    "closed":26,
    "dimension": 36,

    //rectangle texture
    "textureLeft":  30,
    "textureRight": 31,
    "textureTop":   32,
    "textureBottom":33,

    //clipping
    "cliprect": 34


};

var gl_native = {
    createNativeFont: function(path) {
        var shaderpath = __dirname+'/resources/shaders';
        console.log("invoking native font. shader path = ",shaderpath);
        return sgtest.createNativeFont(path,shaderpath);
    },
    registerFont:function(args) {
        fontmap[args.name] = new JSFont(args);
    },
    init: function(core) {
        sgtest.init();
    },
    startEventLoop : function() {
        console.log("starting the event loop");
        var self = this;
        function immediateLoop() {
            try {
                self.tick(Core.getCore());
            } catch (ex) {
                console.log(ex);
                console.log(ex.stack);
                console.log("EXCEPTION. QUITTING!");
                return;
            }
            self.setImmediate(immediateLoop);
        }
        setTimeout(immediateLoop,1);
    },
    createWindow: function(core,w,h) {
        sgtest.createWindow(w* core.DPIScale,h*core.DPIScale);
        Shaders.init(sgtest,OS);
        fontmap['source']  = new JSFont(defaultFonts['source']);
        fontmap['awesome'] = new JSFont(defaultFonts['awesome']);
        core.defaultFont = fontmap['source'];
        this.rootWrapper = this.createGroup();
        this.updateProperty(this.rootWrapper, "scalex", core.DPIScale);
        this.updateProperty(this.rootWrapper, "scaley", core.DPIScale);
        sgtest.setRoot(this.rootWrapper);
    },
    getFont: function(name) {
        return fontmap[name];
    },
    updateProperty: function(handle, name, value) {
        if(handle == undefined) throw new Error("Can't set a property on an undefined handle!!");
        //console.log('setting', name, propsHash[name], value, typeof value);
        sgtest.updateProperty(handle, propsHash[name], value);
    },
    setRoot: function(handle) {
        sgtest.addNodeToGroup(handle,this.rootWrapper);
    },
    tick: function() {
        sgtest.tick();
    },
    setImmediate: function(loop) {
        setImmediate(loop);
    },
    setEventCallback: function(cb) {
        sgtest.setEventCallback(cb);
    },
    createRect: function()  {          return sgtest.createRect();    },
    createGroup: function() {          return sgtest.createGroup();   },
    createPoly: function()  {          return sgtest.createPoly();    },
    createText: function() {           return sgtest.createText(); },
    createGLNode: function(cb)  {      return sgtest.createGLNode(cb);  },
    addNodeToGroup: function(h1,h2) {
        sgtest.addNodeToGroup(h1,h2);
    },
    removeNodeFromGroup: function(h1, h2) {
        sgtest.removeNodeFromGroup(h1, h2);
    },
    decodePngBuffer: function(fbuf, cb) {
        cb(sgtest.decodePngBuffer(fbuf));
    },
    decodeJpegBuffer: function(fbuf, cb) {
        cb(sgtest.decodeJpegBuffer(fbuf));
    },
    loadBufferToTexture: function(texid, w, h, bpp, buf, cb) {
        var img = sgtest.loadBufferToTexture(texid, w,h, bpp, buf)
        cb(img);
    },
    setWindowSize: function(w,h) { sgtest.setWindowSize(w*Core.getCore().DPIScale,h*Core.getCore().DPIScale); },
    getWindowSize: function(w,h) {
        var dpi = Core.getCore().DPIScale;
        var size = sgtest.getWindowSize(w,h);
        return {
            w: size.w/dpi,
            h: size.h/dpi
        };
    },
    createAnim: function(handle,prop,start,end,dur,count,rev) {
        return amino.sgtest.createAnim(handle,propsHash[prop],start,end,dur,count,rev);
    },
    updateAnimProperty: function(handle, prop, type) {
        amino.sgtest.updateAnimProperty(handle, propsHash[prop], type);
    },

    runTest: function(opts) {
        return amino.sgtest.runTest(opts);
    }

}

exports.input = amino_core.input;
exports.start = function(cb) {
    if(!cb) throw new Error("CB parameter missing to start app");
    console.log("core = ", Core);
    Core.setCore(new Core());
    var core = Core.getCore();
    core.native = gl_native;
    amino_core.native = gl_native;
    core.init();
    var stage = Core._core.createStage(600,600);
    //mirror fonts
    /*
    var source_font = exports.getRegisteredFonts().source;
    var fnt = PImage.registerFont(source_font.filepaths[400],source_font.name);
    fnt.load(function() {
        cb(Core._core,stage);
        Core._core.start();
    });
    */
    cb(core,stage);
    core.start();
};


exports.makeProps = amino_core.makeProps;

exports.Rect     = amino_core.Rect;
exports.Group    = amino_core.Group;
exports.Circle   = amino_core.Circle;
exports.Polygon  = amino_core.Polygon;
exports.Text     = amino_core.Text;

exports.input.init(OS);


//exports.input = amino.input;


//exports.makeProps = amino.makeProps;
//exports.getCore = amino.getCore;
//exports.native = amino.native;
//exports.PixelView = amino.primitives.PixelView;
//exports.ImageView = amino.primitives.ImageView;
//exports.RichTextView = amino.primitives.RichTextView;
//exports.registerFont = amino.registerFont;

//exports.ParseRGBString = amino.primitives.ParseRGBString;
//exports.PureImageView = amino.primitives.PureImageView;
//exports.ConstraintSolver = require('./src/ConstraintSolver');
