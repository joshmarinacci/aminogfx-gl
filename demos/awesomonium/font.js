'use strict';

const amino = require('../../main.js');
const path = require('path');

amino.fonts.registerFont({
    name: 'mech',
    path: path.join(__dirname, 'resources/'),
    weights: {
        400: {
            normal: 'MechEffects1BB_reg.ttf',
            italic: 'MechEffects1BB_ital.ttf'
        }
    }
});

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    const group = this.createGroup();

    //mech
    group.add(gfx.createText()
        .text('sample text')
        .fill('#fcfbcf')
        .fontName('mech')
        .fontSize(30)
        .x(0)
        .y(30)
    );

    group.add(gfx.createText()
        .text('more...')
        .fill('#fcfbcf')
        .fontName('mech')
        .fontSize(30)
        .x(200)
        .y(30)
    );

    //awesome
    group.add(gfx.createText()
        .text('\uf071')
        .fill('#fcfbcf')
        .fontName('awesome')
        .fontSize(20)
        .x(0)
        .y(60)
    );

    this.setRoot(group);
});
