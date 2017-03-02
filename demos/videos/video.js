'use strict';

const amino = require('../../main.js');
const path = require('path');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    //video
    const video = new amino.AminoVideo();

    video.src = path.join(__dirname, 'trailer_iphone.m4v');
    //video.src = path.join(__dirname, 'test.h264');

    //rect
    const dispW = this.w();
    const dispH = this.h();
    const rect = this.createImageView().w(dispW).h(dispH).position('center top').size('contain').src(video);

    console.log('display size: ' + dispW + 'x' + dispH);

    //cbx more

    this.root.add(rect);
});
