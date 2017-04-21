'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play webm (vp8) video.
 *
 * Note: not supported on RPi.
 */

player.playVideo({
    src: path.join(__dirname, 'big-buck-bunny_trailer.webm'),
    opts: 'amino_dump_format=1'
}, (err, video) => {
    //empty
});