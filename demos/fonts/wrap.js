'use strict';

var path = require('path');
var amino = require('../../main.js');

amino.start(function (core, stage) {
    //fonts
    core.registerFont({
        name: 'Oswald',
        path: path.join(__dirname, 'resources/oswald/'),
        weights: {
            200: {
                normal: 'Oswald-Light.ttf',
            },
            400: {
                normal: 'Oswald-Regular.ttf',
            },
            800: {
                normal: 'Oswald-Bold.ttf',
            }
        }
    });

    //root
    var root = new amino.Group();

    stage.w(600);
    stage.h(800);

    stage.setRoot(root);

    //rects
    var rect = new amino.Rect().x(0).y(0).w(600).h(80).fill('#ffffff');
    var rect2 = new amino.Rect().x(0).y(80).w(600).h(80).fill('#dddddd');

    root.add(rect, rect2);

    //text
    var text = new amino.Text().fontName('Oswald')
        .text('This is a very long text which is wrapped.\nNew line here.\n  white space.  ')
        //.text('Aaaaaaaaa_ Bbbbbbbbb_ Ccccccccc_ Ddddddddd_ Eeeeeeeee_ Fffffffff_')
        .fontSize(80)
        .fontWeight(200)
        .x(0).y(0)
        .vAlign('top')
        //.vAlign('bottom') //FIXME too high
        //.vAlign('middle') //FIXME too high
        .w(600)
        .h(160)
        //.wrap('end')
        .wrap('word')
        .fill('#ffff00');

    root.add(text);

});
