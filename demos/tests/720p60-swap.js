'use strict';

const amino = require('../../main.js');

//create instance
const gfx = new amino.AminoGfx({
    resolution: '720p@60',
    swapInterval: 2 //30 fps
});

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    this.fill('#FF0000');

    //some info
    console.log('screen: ' + JSON.stringify(gfx.screen));
    console.log('window size: ' + this.w() + 'x' + this.h());
    console.log('runtime: ' + JSON.stringify(gfx.runtime));
});
