'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play RTSP stream.
 */

player.playVideo({
    src: 'rtsp://mpv.cdn3.bigCDN.com:554/bigCDN/definst/mp4:bigbuckbunnyiphone_400.mp4',

    //Note: TCP stream needed (UDP timeout, retrying with TCP)
    opts: 'rtsp_transport=tcp'
}, (err, video) => {
    //empty
});