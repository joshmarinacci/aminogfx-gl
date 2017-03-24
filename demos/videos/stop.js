'use strict';

const player = require('./player');
const path = require('path');

/*
 * Play RTSP stream & stop after 4 seconds.
 */

player.playVideo({
    //local file
    src: path.join(__dirname, 'trailer_iphone.m4v'),

    //720p (h264)
    /*
    src: 'rtsp://mm2.pcslab.com/mm/7h1500.mp4',
    opts: 'rtsp_transport=tcp',
    */

    ready: video => {
        setTimeout(() => {
            player.measureStart();
            video.stop();
            player.measureEnd('stop()');
        }, 4000);

        setInterval(() => {
            console.log('media time: ' + video.getMediaTime());
        }, 1000);
    }
}, (err, video) => {
    //empty
});