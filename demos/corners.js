'use strict';

const amino = require('../main.js');

const gfx = new amino.AminoGfx({
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

    const w = this.w();
    const h = this.h();
    const rectW = w / 6;
    const rectH = h / 6;

    //white background
    this.fill('#ffffff');

    //setup root
    const root = this.createGroup();

    this.setRoot(root);

    //top-left (red)
    const rectTL = this.createRect().w(rectW).h(rectH).x(0).y(0).fill("#ff0000");

    root.add(rectTL);
    addClickHandler(this, rectTL, '#00ffff');

    //top-right (green)
    const rectTR = this.createRect().w(rectW).h(rectH).x(w - rectW).y(0).fill("#00ff00");

    root.add(rectTR);
    addClickHandler(this, rectTR, '#ff00ff');

    //bottom-left (green)
    const rectBL = this.createRect().w(rectW).h(rectH).x(0).y(h - rectH).fill("#00ff00");

    root.add(rectBL);
    addClickHandler(this, rectBL, '#ff00ff');

    //bottom-right (red)
    const rectBR = this.createRect().w(rectW).h(rectH).x(w - rectW).y(h - rectH).fill("#ff0000");

    root.add(rectBR);
    addClickHandler(this, rectBR, '#00ffff');

    //resize
    this.on('window.size', function (_evt) {
        //move corners
        rectTR.x(gfx.w() - rectW);
        rectBL.y(gfx.h() - rectH);
        rectBR.x(gfx.w() - rectW).y(gfx.h() - rectH);

        //debug
        //console.log('resized ' + gfx.w() + '/' + gfx.h());
    });

    //perspective
    /*
    setTimeout(() => {
        this.updatePerspective({
            src: [
               0.2, 0.,
                0.8, 0.,
                0., 1.,
                1., 1.
            ]
        });
    }, 5 * 1000);
    */
});

/**
 * Add a click handler to change the rect's color while pressed.
 */
function addClickHandler(gfx, rect, selColor) {
    rect.acceptsMouseEvents = true;

    const color = rect.fill();

    gfx.on('press', rect, function () {
        rect.fill(selColor);
    });

    gfx.on('release', rect, function () {
        rect.fill(color);
    });
}