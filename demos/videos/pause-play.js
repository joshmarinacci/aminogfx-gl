'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play RTSP stream, pause after 4 seconds & resume after 4 more seconds.
 */

player.playVideo({
    //low res
    src: path.join(__dirname, 'test.h264'),
    loop: false,

    //720p (h264)
    //Note: getting EOF on play()
    /*
    src: 'rtsp://mm2.pcslab.com/mm/7h1500.mp4',
    opts: 'rtsp_transport=tcp',
    */

    ready: video => {
        setTimeout(() => {
            console.log('pausing playback');

            player.measureStart();
            video.pause();
            player.measureEnd('pause()');

            setTimeout(() => {
                console.log('resuming playback');

                player.measureStart();
                video.play();
                player.measureEnd('play()');
            }, 4000);
        }, 4000);

        setInterval(() => {
            console.log('media time: ' + video.getMediaTime());
        }, 1000);
    }
}, (err, video) => {
    //empty
});