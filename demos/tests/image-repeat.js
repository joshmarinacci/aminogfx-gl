'use strict';

const amino = require('../../main.js');
const path = require('path');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    //root
    const root = gfx.createGroup();

    gfx.setRoot(root);

    //image (16x16)

    // 1) no-repeat
    const iv1 = gfx.createImageView().w(32).h(160);

    iv1.src(path.join(__dirname, '../images/tree.png'));
    iv1.right(2).bottom(10);
    iv1.size('stretch');

    root.add(iv1);

    // 2) repeat-x
    const iv2 = gfx.createImageView().x(50).w(32).h(160);

    iv2.src(path.join(__dirname, '../images/tree.png'));
    iv2.right(2).bottom(10);
    iv2.size('stretch');
    iv2.repeat('repeat-x');

    root.add(iv2);

    // 3) repeat-y
    const iv3 = gfx.createImageView().x(100).w(32).h(160);

    iv3.src(path.join(__dirname, '../images/tree.png'));
    iv3.right(2).bottom(10);
    iv3.size('stretch');
    iv3.repeat('repeat-y');

    root.add(iv3);

    // 3) repeat
    const iv4 = gfx.createImageView().x(150).w(32).h(160);

    iv4.src(path.join(__dirname, '../images/tree.png'));
    iv4.right(2).bottom(10);
    iv4.size('stretch');
    iv4.repeat('repeat');

    root.add(iv4);
});
