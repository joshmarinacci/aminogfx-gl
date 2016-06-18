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

    //text
    var text = new amino.Text().fontName('Oswald')
        .text('Oswald Regular')
        .fontSize(80)
        .fontWeight(200)
        .x(50).y(150)
        .fill('#ffff00');
    root.add(text);

});
