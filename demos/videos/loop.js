'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play H264 raw video.
 */

player.playVideo({
    src: path.join(__dirname, 'test.h264'),

    //play once
    //loop: 1
    //loop: 0
    loop: false

    //play forcever
    //loop: true
    //loop: -1
}, (_err, _video) => {
    //empty
});