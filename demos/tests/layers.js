'use strict';

if (process.argv.length == 2) {
    console.log('Missing integer parameter!');
    return;
}

/*
 * Results:
 *
 *  1) Raspberry Pi: 1080p (2016-09-22)
 *
 *    - 5:  60 fps (10.3 MP)
 *    - 6:  52 fps (12.4 MP)
 *    - 10: 32 fps (20.7 MP)
 *    - 20: 13 fps (41.4 MP)
 *    - 40:  6 fps (82.9 MP)
 *
 *    => limit at 5 fullscreen layers (ca. 10 MP)
 *
 *  2) Raspberry Pi: 720p (2016-09-22)
 *
 *    - 12: 60 fps (11.1 MP)
 *    - 14: 50 fps (12.9 MP
 *    - 20: 32 fps (18.4 MP)
 *    - 40: 15 fps (36.9 MP)
 *
 *    => limit at 12 fullscreen layers (ca. 11 MP)
 */

const layers = process.argv[2];
const amino = require('../../main.js');

const gfx = new amino.AminoGfx({
    //resolution: '1080p@60',
    //resolution: '720p@60',

    //swapInterval: 3,

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

    //Note: not seeing any difference
    //root.depth(true);

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