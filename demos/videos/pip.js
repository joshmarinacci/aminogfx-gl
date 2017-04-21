'use strict';

const amino = require('../../main.js');
const path = require('path');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    //video 1
    const video1 = new amino.AminoVideo();

    // 720p
    /*
    video1.src = 'rtsp://mm2.pcslab.com/mm/7h1500.mp4';
    video1.opts = 'rtsp_transport=tcp';
    */

    //1080p
    video1.src = path.join(__dirname, 'test.h264');

    const iv1 = this.createImageView().position('center').size('contain').src(video1);

    iv1.w.bindTo(this.w);
    iv1.h.bindTo(this.h);

    this.root.add(iv1);

    //video 2 (low res)
    const video2 = new amino.AminoVideo();

    video2.src = 'rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov';
    video2.opts = 'rtsp_transport=tcp';

    const iv2 = this.createImageView().position('center').size('conver').src(video2);

    function updateVideo2Pos() {
        const dispW = gfx.w();
        const dispH = gfx.h();
        const videoW = dispW / 3;
        const videoH = dispH / 3;

        iv2.x(dispW - videoW).y(dispH - videoH).w(videoW).h(videoH);
    }

    updateVideo2Pos();

    iv2.originX(.5).originY(.5);
    iv2.ry.anim().from(0).to(360).dur(4000).loop(-1).start();

    this.w.watch(updateVideo2Pos);
    this.h.watch(updateVideo2Pos);

    this.root.add(iv2);

    //display info
    console.log('display size: ' + this.w() + 'x' + this.h());

    //show media time
    iv1.image.watch(video => {
        if (video) {
            setInterval(() => {
                console.log('time: ' + video.getMediaTime() + ' of ' + video.getDuration());
            }, 1000);
        }
    });
});
