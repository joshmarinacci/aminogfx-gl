'use strict';

var async = require('async');

var amino = require('./amino');
var PImage = require('../../pureimage/pureimage');
var comp = require('../../richtext/component');

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
