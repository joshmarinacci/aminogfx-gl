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
    addClickHandler(core, rectTL, '#00ffff');

    //top-right (green)
    var rectTR = new amino.Rect().w(rectW).h(rectH).x(w - rectW).y(0).fill("#00ff00");

    root.add(rectTR);
    addClickHandler(core, rectTR, '#ff00ff');

    //bottom-left (green)
    var rectBL = new amino.Rect().w(rectW).h(rectH).x(0).y(h - rectH).fill("#00ff00");

    root.add(rectBL);
    addClickHandler(core, rectBL, '#ff00ff');

    //bottom-right (red)
    var rectBR = new amino.Rect().w(rectW).h(rectH).x(w - rectW).y(h - rectH).fill("#ff0000");

    root.add(rectBR);
    addClickHandler(core, rectBR, '#00ffff');

    //resize
    core.on('window.size', function () {
        //move corners
        rectTR.x(stage.w() - rectW);
        rectBL.y(stage.h() - rectH);
        rectBR.x(stage.w() - rectW).y(stage.h() - rectH);

        //debug
        //console.log('resized ' + stage.w() + '/' + stage.h());
    });

});

/**
 * Add a click handler to change the rect's color while pressed.
 */
function addClickHandler(core, rect, selColor) {
    rect.acceptsMouseEvents = true;

    var color = rect.fill();

    core.on('press', rect, function () {
        rect.fill(selColor);
    });

    core.on('release', rect, function () {
        rect.fill(color);
    });
}