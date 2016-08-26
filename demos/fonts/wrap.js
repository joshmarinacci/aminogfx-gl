'use strict';

var path = require('path');
var amino = require('../../main.js');

var gfx = new amino.AminoGfx();

//fonts
amino.fonts.registerFont({
    name: 'Oswald',
    path: path.join(__dirname, 'resources/oswald/'),
    weights: {
        200: {
            normal: 'Oswald-Light.ttf'
        },
        400: {
            normal: 'Oswald-Regular.ttf'
        },
        800: {
            normal: 'Oswald-Bold.ttf'
        }
    }
});

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //root
    var root = this.createGroup();

    this.w(600);
    this.h(800);

    this.setRoot(root);

    //rects
    var rect = this.createRect().x(0).y(0).w(600).h(80).fill('#ffffff');
    var rect2 = this.createRect().x(0).y(80).w(600).h(80).fill('#dddddd');

    root.add(rect, rect2);

    //text
    var text = this.createText().fontName('Oswald')
        .text('This is a very long text which is wrapped.\nNew line here.\n  white space.  ')
        //.text('This is a very long text which is wrapped. ')
        //.text('Aaaaaaaaa_ Bbbbbbbbb_ Ccccccccc_ Ddddddddd_ Eeeeeeeee_ Fffffffff_')
        .fontSize(80)
        .fontWeight(200)
        .x(0).y(0)
        .vAlign('top')
        //.vAlign('bottom')
        //.vAlign('middle')
        .w(600)
        .h(160)
        //.wrap('end')
        .wrap('word')
        .fill('#ffff00');

    root.add(text);

    //anim
    setInterval(() => {
        var value = text.text();

        text.text(value.substring(1) + value[0]);
    }, 1000);

});
