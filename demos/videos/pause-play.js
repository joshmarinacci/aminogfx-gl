'use strict';

const player = require('./player');

/*
 * Play RTSP stream, pause after 4 seconds & resume after 4 more seconds.
 */

player.playVideo({
    //720p (h264)
    src: 'rtsp://mm2.pcslab.com/mm/7h1500.mp4',
    opts: 'rtsp_transport=tcp',
    ready: video => {
        setTimeout(() => {
            console.log('pausing playback');
            video.pause();

            setTimeout(() => {
                //cbx does not work on RPi
                console.log('resuming playback');
                video.play();
            }, 4000);
        }, 4000);

        setInterval(() => {
            console.log('media time: ' + video.getMediaTime());
        }, 1000);
    }
}, (err, video) => {
    //empty
});