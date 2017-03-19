'use strict';

const player = require('./player');

/*
 * Play RTSP stream & stop after 4 seconds.
 */

player.playVideo({
    //720p (h264)
    src: 'rtsp://mm2.pcslab.com/mm/7h1500.mp4',
    opts: 'rtsp_transport=tcp',
    ready: video => {
        setTimeout(() => {
            //first pause
            video.pause();

            //wait & stop
            setTimeout(() => {
                video.stop();
            }, 1000);
        }, 4000);

        setInterval(() => {
            console.log('media time: ' + video.getMediaTime());
        }, 1000);
    }
}, (err, video) => {
    //empty
});