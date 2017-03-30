'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play H264 raw video.
 */

player.playVideo({
    //cbx FIXME only plays with 27 fps!
    src: path.join(__dirname, 'test.h264')
}, (err, video) => {
    //empty
});