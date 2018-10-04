'use strict';

const player = require('./player');
const path = require('path');

/*
 * Play RTSP stream, pause after 4 seconds & resume after 4 more seconds.
 */

player.playVideo({
    //720p (h264)
    /*
    src: 'rtsp://mm2.pcslab.com/mm/7h1500.mp4',
    opts: 'rtsp_transport=tcp',
    */

    //1080p
    src: path.join(__dirname, 'test.h264'),
    loop: false,

    ready: video => {
        setTimeout(() => {
            console.log('pausing playback');
            video.pause();
        }, 4000);

        setInterval(() => {
            console.log('media time: ' + video.getMediaTime());
        }, 1000);
    }
}, (_err, _video) => {
    //empty
});