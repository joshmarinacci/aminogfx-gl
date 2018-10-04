'use strict';

const path = require('path');
const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

//fonts
amino.fonts.registerFont({
    name: 'Oswald',
    path: path.join(__dirname, 'resources/oswald/'),
    weights: {
        200: {
            normal: 'Oswald-Light.ttf'
        },
        400: {
            normal: 'Oswald-Regular.ttf'
        },
        800: {
            normal: 'Oswald-Bold.ttf'
        }
    }
});

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //root
    const root = this.createGroup();

    this.w(600);
    this.h(800);

    this.setRoot(root);

    //rects
    const rect = this.createRect().x(50).y(150).w(600).h(80);
    const rect2 = this.createRect().x(0).y(0).w(600).h(60);

    root.add(rect, rect2);

    //text
    const text = this.createText().fontName('Oswald')
        .text('Oswald Regular')
        .fontSize(80)
        .fontWeight(200)
        .x(50).y(150) //baseline position
        .fill('#ffff00');

    root.add(text);

    //text 2
    const text2 = this.createText().fontName('Oswald')
        .text('Oswald Regular')
        .fontSize(60)
        .fontWeight(200)
        .x(0).y(0) //baseline position
        .h(60)
        .vAlign('top')
        .fill('#ffff00');

    root.add(text2);

    //text 3
    const text3 = this.createText().fontName('awesome')
        .text('\uf241 \uf140')
        .fontSize(40)
        .x(10).y(300)
        .fill('#0000ff');

    root.add(text3);
});
