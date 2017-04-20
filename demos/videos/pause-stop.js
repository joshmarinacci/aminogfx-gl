'use strict';

const player = require('./player');
const path = require('path');

/*
 * Play RTSP stream & stop after 4 seconds.
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
            //first pause
            player.measureStart();
            video.pause();
            player.measureEnd('pause()');

            //wait & stop
            setTimeout(() => {
                player.measureStart();
                video.stop();
                player.measureEnd('stop()');
            }, 1000);
        }, 4000);

        setInterval(() => {
            console.log('media time: ' + video.getMediaTime());
        }, 1000);
    }
}, (err, video) => {
    //empty
});