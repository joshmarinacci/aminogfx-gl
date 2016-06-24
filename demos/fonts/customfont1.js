'use strict';

var path = require('path');
var amino = require('../../main.js');

amino.start(function (core, stage) {
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
    var rect = new amino.Rect().x(50).y(150).w(600).h(80);
    var rect2 = new amino.Rect().x(0).y(0).w(600).h(60);

    root.add(rect, rect2);

    //text
    var text = new amino.Text().fontName('Oswald')
        .text('Oswald Regular')
        .fontSize(80)
        .fontWeight(200)
        .x(50).y(150) //baseline position
        .fill('#ffff00');

    root.add(text);

    //text 2
    var text2 = new amino.Text().fontName('Oswald')
        .text('Oswald Regular')
        .fontSize(60)
        .fontWeight(200)
        .x(0).y(0) //baseline position
        .h(60)
        .vAlign('top')
        .fill('#ffff00');

    root.add(text2);

    //text 3
    var text3 = new amino.Text().fontName('awesome')
        .text('\uf241 \uf140')
        .fontSize(40)
        .x(10).y(300)
        .fill('#0000ff');

    root.add(text3);

});
