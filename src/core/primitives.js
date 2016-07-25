'use strict';

var async = require('async');

var amino = require('./amino');
var PImage = require('../../pureimage/pureimage');
var comp = require('../../richtext/component');

/**
 * PixelView object.
 */
exports.PixelView = function () {
    amino.makeProps(this, {
        id: '',
        visible: true,
        x: 0,
        y: 0,
        sx: 1,
        sy: 1,
        w: 100,
        h: 100,
        pw: 100,
        ph: 100,
        opacity: 1.0,
        fill: '#ffffff',
        textureLeft: 0,
        textureRight: 1,
        textureTop:  0,
        textureBottom: 1
    });

    var self = this;

    //native
    this.handle = amino.getCore().getNative().createRect(true);

    applyNativeBinding(this);

    //methods
    this.contains = contains;

    function rebuildBuffer() {
        var w = self.pw();
        var h = self.ph();

        self.buf = new Buffer(w * h * 4);

        var c1 = [0, 0, 0];
        var c2 = [255, 255, 255];

        for (var x = 0; x < w; x++) {
            for (var y = 0; y < h; y++) {
                var i = (x + y * w) * 4;
                var c;

                if (x % 3 == 0) {
                    c = c1;
                } else {
                    c = c2;
                }

                self.buf[i + 0] = c[0];
                self.buf[i + 1] = c[1];
                self.buf[i + 2] = c[2];
                self.buf[i + 3] = 255;
            }
        }

        self.updateTexture();
    }

    var texid = -1;

    this.updateTexture = function () {
        //when the image is loaded, update the texture id and dimensions
        var img = amino.getCore().getNative().loadBufferToTexture(texid, self.pw(), self.ph(), 4, self.buf, function (image) {
	        texid = image.texid;
            amino.getCore().getNative().updateProperty(self.handle, 'texid', image.texid);
        });
    };

    this.setPixel = function(x, y, r, g, b, a) {
        var w = self.pw();
        var i = (x + y * w)*4;

        self.buf[i + 0] = r;
        self.buf[i + 1] = g;
        self.buf[i + 2] = b;
        self.buf[i + 3] = a;
    };

    this.setPixeli32 = function(x, y, int) {
        var w = self.pw();
        var i = (x + y * w) * 4;

        self.buf.writeUInt32BE(int, i);
    };

    this.pw.watch(rebuildBuffer);
    this.ph.watch(rebuildBuffer);

    rebuildBuffer();
};

exports.PureImageView = function () {
    var piv = new exports.PixelView();
    var img = PImage.make(800, 600);
    var ctx = img.getContext('2d');

    ctx.fillStyle = '#00FF00';

    piv.getContext = function () {
        return ctx;
    };

    piv.sync = function () {
        //copy pixels
        for (var i = 0; i < img.width; i++) {
            for (var j = 0; j < img.height; j++) {
                var pixel = ctx.getPixeli32(i, j);

                if (i >= this.pw() || j >= this.ph()) {
                    continue;
                }

                this.setPixeli32(i, j, pixel);
            }
        }

        this.updateTexture();
    };

    return piv;
};

var pureimageFontsRegistered = false;

/**
 * Rich text view.
 */
exports.RichTextView = function () {
    //default view size
    var piv = new exports.PureImageView().pw(100).w(100).ph(100).h(100);

    amino.makeProps(piv, {
        multiline: true,
        enterAction: null
    });

    piv.acceptsKeyboardEvents = true;

    /**
     * Build the document.
     */
    piv.build = function (frame, done) {
        var ctx = piv.getContext();
        var config = {
            context: ctx,
            frame: frame,
            width:  piv.pw(),
            height: piv.ph(),
            multiline: piv.multiline(),
            enterAction: piv.enterAction(),
            charWidth: function (ch,
                font_size,
                font_family,
                font_weight,
                font_style
            ) {
                ctx.setFont(font_family, font_size);

                return ctx.measureText(ch).width;
            },
            requestAnimationFrame: function (redraw) {
                redraw();
                piv.sync();
            }
        };

        //register fonts
        if (!pureimageFontsRegistered) {
            pureimageFontsRegistered = true;

            var fonts = amino.getCore().getNative().getRegisteredFonts();
            var regFonts = [];

            for (var name in fonts) {
                var font = fonts[name];

                //iterate weights
                for (var weight in font.weights) {
                    var binary = font.filepaths[weight];
                    var family = font.desc.name;
                    var style = 'normal';
                    var variant = null;

                    //debug
                    //FIXME style handling
                    //console.log('font: ' + binary + ' ' + family + ' ' + style);

                    var regFont = PImage.registerFont(binary, family, weight, style, variant);

                    if (regFont) {
                        regFonts.push(regFont);
                    }
                }
            }

            //load
            async.each(regFonts, function(font, done) {
                font.load(done);
            }, function (err) {
                ready();
            });
        } else {
            ready();
        }

        //create view
        function ready() {
            var rte = comp.makeRichTextView(config);

            piv.editor = rte;
            rte.relayout();
            rte.redraw();

            amino.getCore().on('key.press', piv, function (e) {
                rte.processKeyEvent(e);
            });

            //ready
            done();
        }
    };

    return piv;
};
