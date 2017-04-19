'use strict';

/*
 * launch: node --expose-gc demos/videos/stresstest.js
 *
 * Rasperry Pi 3 results (28.03.2017):
 *
 *  - CPU: 12 - 18%
 *  - Memory: 5.8%
 *
 *  At least 1'500 cycles stable without any leaks.
 */

//cbx FIXME hangs on RPi (deadlock)!
const path = require('path');
const amino = require('../../main.js');
const player = require('./player');

/*
 * Load & play videos.
 */

let count = 0;
const src = path.join(__dirname, 'trailer_iphone.m4v'); //use FFmpeg/libav

player.playVideo({
    src: src
}, (err, video, iv) => {
    //scene ready
    iv.image.watch(texture => {
        //check errors
        if (!texture) {
            console.log('no texture!');
            return;
        }

        //count
        count++;
        console.log('videos: ' + count);

        //JS objects
        if (global.gc) {
            global.gc();
        }

        const stats = iv.amino.getStats();

        console.log('JS objects: active=' + stats.activeInstances + ' total=' + stats.totalInstances);

        //play for one second
        setTimeout(() => {
            //free old video
            iv.image(null);
            texture.destroy();

            //play video again (new video texture)
            const video2 = new amino.AminoVideo();

            video2.src = src;
            iv.src(video2);
        }, 1000);
    });
});