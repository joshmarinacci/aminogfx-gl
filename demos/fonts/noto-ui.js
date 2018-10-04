'use strict';

const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

gfx.start(err => {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //root
    const root = gfx.createGroup();

    gfx.w(600);
    gfx.h(800);

    gfx.setRoot(root);

    //text
    const text = gfx.createText().fontName('noto-ui')
        .text('This is a very long text which is wrapped.\nNew line here.\n  white space.  ')
        //.text('This is a very long text which is wrapped. ')
        //.text('Aaaaaaaaa_ Bbbbbbbbb_ Ccccccccc_ Ddddddddd_ Eeeeeeeee_ Fffffffff_')
        .fontSize(80)
        .fontWeight(200)
        .x(0).y(0)
        .vAlign('top')
        //.vAlign('bottom')
        //.vAlign('middle')
        .w(600)
        .h(160)
        //.wrap('end')
        .wrap('word')
        //.maxLines(1)
        //.maxLines(2)
        .fill('#ffff00');

    root.add(text);
});
