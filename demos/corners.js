'use strict';

var path = require('path');
var amino = require('../main.js');

var gfx = new amino.AminoGfx({
    perspective: {
        /*
        src: [
            0.2, 0.,
            0.8, 0.,
            0., 1.,
            1., 1.
        ]
        */
        /*
        dst: [
            0.2, 0.,
            0.8, 0.,
            0., 1.,
            1., 1.
        ]
        */
    }
});

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    var w = this.w();
    var h = this.h();
    var rectW = w / 6;
    var rectH = h / 6;

    //white background
    this.fill('#ffffff');

    //setup root
    var root = this.createGroup();

    this.setRoot(root);

    //top-left (red)
    var rectTL = this.createRect().w(rectW).h(rectH).x(0).y(0).fill("#ff0000");

    root.add(rectTL);
    addClickHandler(this, rectTL, '#00ffff');

    //top-right (green)
    var rectTR = this.createRect().w(rectW).h(rectH).x(w - rectW).y(0).fill("#00ff00");

    root.add(rectTR);
    addClickHandler(this, rectTR, '#ff00ff');

    //bottom-left (green)
    var rectBL = this.createRect().w(rectW).h(rectH).x(0).y(h - rectH).fill("#00ff00");

    root.add(rectBL);
    addClickHandler(this, rectBL, '#ff00ff');

    //bottom-right (red)
    var rectBR = this.createRect().w(rectW).h(rectH).x(w - rectW).y(h - rectH).fill("#ff0000");

    root.add(rectBR);
    addClickHandler(this, rectBR, '#00ffff');

    //resize
    this.on('window.size', function (evt) {
        //move corners
        rectTR.x(gfx.w() - rectW);
        rectBL.y(gfx.h() - rectH);
        rectBR.x(gfx.w() - rectW).y(gfx.h() - rectH);

        //debug
        //console.log('resized ' + gfx.w() + '/' + gfx.h());
    });

});

/**
 * Add a click handler to change the rect's color while pressed.
 */
function addClickHandler(gfx, rect, selColor) {
    rect.acceptsMouseEvents = true;

    var color = rect.fill();

    gfx.on('press', rect, function () {
        rect.fill(selColor);
    });

    gfx.on('release', rect, function () {
        rect.fill(color);
    });
}