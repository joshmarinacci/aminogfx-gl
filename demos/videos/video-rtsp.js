'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play RTSP stream.
 */

player.playVideo({
    //low res
    //src: 'rtsp://mpv.cdn3.bigCDN.com:554/bigCDN/definst/mp4:bigbuckbunnyiphone_400.mp4',
    //src: 'rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov',

    //720p
    //src: 'rtsp://mm2.pcslab.com/mm/7m1000.mp4',
    src: 'rtsp://mm2.pcslab.com/mm/7m2000.mp4',

    //Note: TCP stream needed (UDP timeout, retrying with TCP)
    opts: 'rtsp_transport=tcp'
}, (err, video) => {
    //empty
});