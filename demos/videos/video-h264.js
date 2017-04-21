'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play H264 raw video.
 */

player.playVideo({
    //1920x1080, 25 fps
    src: path.join(__dirname, 'test.h264'),
    opts: 'amino_dump_format=1'
}, (err, video) => {
    //empty
});