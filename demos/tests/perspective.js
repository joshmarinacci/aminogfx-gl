'use strict';

//TODO check why only 40 fps on RPi

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

    //root
    let root = this.createGroup();

    this.setRoot(root);

    //setting
    const showBelow = true;
    const showAbove = true;

    //below
    if (showBelow) {
        root.add(createRect(-400, '#00FF00'));
        root.add(createRect(-300, '#00FF00'));
        root.add(createRect(-200, '#00FF00'));
        root.add(createRect(-100, '#00FF00'));
    }

    //on screen
    root.add(createRect(0, '#FFFFFF'));

    //above
    if (showAbove) {
        root.add(createRect(200, '#FF0000'));
        root.add(createRect(400, '#FF0000'));
        root.add(createRect(600, '#FF0000'));
        root.add(createRect(800, '#FF0000'));
    }

    //some info
    console.log('screen: ' + JSON.stringify(gfx.screen));
    console.log('window size: ' + this.w() + 'x' + this.h());
    //console.log('runtime: ' + JSON.stringify(gfx.runtime));
});

function createRect(z, textColor) {
    //rect
    let rect = gfx.createRect().fill('#0000FF');

    rect.w.bindTo(gfx.w);
    rect.h.bindTo(gfx.h);

    //text
    let text = gfx.createText().text('z: ' + z).fontSize(60).align('center').vAlign('middle').fill(textColor);

    text.w.bindTo(gfx.w);
    text.h.bindTo(gfx.h);

    //group
    let group = gfx.createGroup().opacity(0.2).z(z);

    group.add(rect, text);

    return group;
}