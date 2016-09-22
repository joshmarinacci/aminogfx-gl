'use strict';

if (process.argv.length == 2) {
    console.log('Missing integer parameter!');
    return;
}

/*
 * Results:
 *
 *  1) Raspberry Pi: 1080p
 *
 *    TODO
 */

const layers = process.argv[2];
const amino = require('../../main.js');

const gfx = new amino.AminoGfx({
    perspective: {
        orthographic: false
    }
});

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    //root
    let root = this.createGroup();

    this.setRoot(root);

    //layers
    for (let i = 0; i < layers; i++) {
        root.add(createRect());
    }

    //some info
    console.log('screen: ' + JSON.stringify(gfx.screen));
    console.log('window size: ' + this.w() + 'x' + this.h());
    //console.log('runtime: ' + JSON.stringify(gfx.runtime));

    //pixels
    const pixels = this.w() * this.h() * layers;

    console.log('Pixels: ' + (pixels / 1000000) + ' MP')
});

/**
 * Create a layer.
 */
function createRect(z, textColor) {
    //rect
    let rect = gfx.createRect().fill('#0000FF');

    rect.w.bindTo(gfx.w);
    rect.h.bindTo(gfx.h);

    return rect;
}