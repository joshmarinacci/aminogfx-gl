'use strict';

const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

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
        root.add(createRect(-400));
        root.add(createRect(-300));
        root.add(createRect(-200));
        root.add(createRect(-100));
    }

    //on screen
    root.add(createRect(0));

    //above
    if (showAbove) {
        root.add(createRect(200));
        root.add(createRect(400));
        root.add(createRect(600));
        root.add(createRect(800));
    }

    //some info
    console.log('screen: ' + JSON.stringify(gfx.screen));
    console.log('window size: ' + this.w() + 'x' + this.h());
    //console.log('runtime: ' + JSON.stringify(gfx.runtime));
});

function createRect(z) {
    //rect
    let rect = gfx.createRect().fill('#0000FF');

    rect.w.bindto(gfx.w);
    rect.h.bindto(gfx.h);

    //text
    let text = gfx.createText().text('z: ' + z).fontSize(60).align('center').vAlign('middle').fill('#FFFFFF');

    text.w.bindto(gfx.w);
    text.h.bindto(gfx.h);

    //group
    let group = gfx.createGroup().opacity(0.2).z(z);

    group.add(rect, text);

    return group;
}