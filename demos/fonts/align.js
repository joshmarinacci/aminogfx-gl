'use strict';

const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    this.fill('#000000');

    //root
    const root = this.createGroup();

    this.setRoot(root);

    //rect
    const rect = this.createRect().w(200).h(200).fill('#FFFFFF');

    root.add(rect);

    //text

    //right
    var text = this.createText().align('right').w(200).vAlign('top').text('Sample Text').fill('#FF0000');

    root.add(text);
});