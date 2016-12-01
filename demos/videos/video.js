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

    //rect
    const rect = this.createImageView().w(400).h(400).src(video);

    //cbx more

    this.root.add(rect);
});
