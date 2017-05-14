'use strict';

const amino = require('../../main.js');

const gfx = new amino.AminoGfx({
    perspective: {
        orthographic: false
    }
});

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    this.fill('#000000');

    //root
    const root = this.createGroup().originX(.5).originY(.5).w(500).h(500);

    this.setRoot(root);

    //add background
    const bgRect = this.createRect().w(500).h(500).fill('#FFFFFF');

    root.add(bgRect);

    //depth rect
    const depthRect = this.createGroup().originX(.5).originY(.5).w(500).h(500);

    depthRect.depth(true);

    root.add(depthRect);

    //add two rect
    const rect1 = this.createRect().w(100).h(100).x(200).y(200).fill('#FF0000').originX(.5).originY(.5);
    const rect2 = this.createRect().w(100).h(100).x(200).y(200).fill('#0000FF').originX(.5).originY(.5);

    //debug: overlap
    //rect1.x(0).y(0);

    rect2.rx(90);

    depthRect.add(rect1, rect2);

    //animate
    depthRect.ry.anim().from(0).to(360).dur(5000).loop(-1).start();

    //overlay rect
    const overlayRect = this.createRect().w(500).h(500).fill('#00FF00').opacity(.1);

    root.add(overlayRect);

    //text
    const text = this.createText().text('3D Demo').vAlign('top').fill('#0000FF');

    root.add(text);
});
