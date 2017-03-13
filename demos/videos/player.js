'use strict';

const amino = require('../../main.js');

/**
 * Play video.
 *
 * @param {object} opts
 * @param {function} done
 */
function playVideo(opts, done) {
    const gfx = new amino.AminoGfx();

    gfx.start(function (err) {
        if (err) {
            console.log('Amino error: ' + err.message);
            done(err);
            return;
        }

        //video
        const video = new amino.AminoVideo();

        video.src = opts.src;
        video.opts = opts.opts;

        //rect
        const dispW = this.w();
        const dispH = this.h();
        const rect = this.createImageView().w(dispW).h(dispH).position('center top').size('contain').src(video);

        rect.w.bindTo(this.w);
        rect.h.bindTo(this.h);

        console.log('display size: ' + dispW + 'x' + dispH);

        done(null, video);

        this.root.add(rect);
    });
}

exports.playVideo = playVideo;