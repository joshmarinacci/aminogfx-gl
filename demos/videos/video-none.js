'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play MP4 without video stream.
 */

player.playVideo({
    //Note: TCP stream needed (UDP timeout, retrying with TCP)
    src: 'http://www.opticodec.com/test/tropic.mp4'
}, (err, video) => {
    //empty
});