'use strict';

var path = require('path');
var amino = require('../main.js');

amino.start(function (core, stage) {
    var w = stage.w();
    var h = stage.h();
    var rectW = w / 6;
    var rectH = h / 6;

    //white background
    stage.fill('#ffffff');

    //setup root
    var root = new amino.Group();

    stage.setRoot(root);

    //top-left (red)
    var rectTL = new amino.Rect().w(rectW).h(rectH).x(0).y(0).fill("#ff0000");

    root.add(rectTL);

    //top-right (green)
    var rectTR = new amino.Rect().w(rectW).h(rectH).x(w - rectW).y(0).fill("#00ff00");

    root.add(rectTR);

    //bottom-left (green)
    var rectBL = new amino.Rect().w(rectW).h(rectH).x(0).y(h - rectH).fill("#00ff00");

    root.add(rectBL);

    //bottom-right (red)
    var rectBR = new amino.Rect().w(rectW).h(rectH).x(w - rectW).y(h - rectH).fill("#ff0000");

    root.add(rectBR);

    //TODO click handlers

    //resize
    core.on('windowsize', function () {
        //move corners
        rectTR.x(stage.w() - rectW);
        rectBL.y(stage.h() - rectH);
        rectBR.x(stage.w() - rectW).y(stage.h() - rectH);

        //debug
        //console.log('resized ' + stage.w() + '/' + stage.h());
    });

});
