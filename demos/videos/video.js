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

    //video.src = path.join(__dirname, 'trailer_iphone.m4v');
    video.src = path.join(__dirname, 'test.h264');

    //Note: TCP stream needed (UDP timeout, retrying with TCP)
    //video.src = 'rtsp://mpv.cdn3.bigCDN.com:554/bigCDN/definst/mp4:bigbuckbunnyiphone_400.mp4';

    //audio only
    //video.src = 'http://www.opticodec.com/test/tropic.mp4';

    //720p
    //video.src = 'https://rrr.sz.xlcdn.com/?account=streamzilla&file=Streamzilla_Demo_HD_Ready.mp4&type=download&service=apache&protocol=https&output=mp4';

    //1080p
    //video.src = 'https://rrr.sz.xlcdn.com/?account=streamzilla&file=Streamzilla_Demo_HD.mp4&type=download&service=apache&protocol=https&output=mp4';

    //rect
    const dispW = this.w();
    const dispH = this.h();
    const rect = this.createImageView().w(dispW).h(dispH).position('center top').size('contain').src(video);

    rect.w.bindTo(this.w);
    rect.h.bindTo(this.h);

    console.log('display size: ' + dispW + 'x' + dispH);

    //cbx more

    this.root.add(rect);
});
