'use strict';

const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //root
    const root = this.createGroup().id('group');

    root.acceptsMouseEvents = true;
    this.setRoot(root);

    //rect
    const rect = this.createRect().w(100).h(50).fill('#ccccff').x(50).y(50).id('clickrect');

    rect.acceptsMouseEvents = true;
    root.add(rect);

    this.on('press', rect, function () {
        rect.fill('#ffffff');
        console.log('pressed');
    });

    this.on('release', rect, function () {
        rect.fill('#ccccff');
        console.log('released');
    });

    this.on('click', rect, function () {
        console.log('clicked');
    });

    //rect 2
    const r2 = this.createRect().w(30).h(30).fill('#ff6666').x(300).y(50).id('dragrect');

    root.add(r2);
    r2.acceptsMouseEvents = true;

    this.on('drag', r2, function (e) {
        r2.x(r2.x() + e.delta.x);
        r2.y(r2.y() + e.delta.y);
    });

    //overlay
    const overlay = this.createRect().w(300).h(300).fill('#00ff00').x(20).y(20).opacity(0.2).id('overlay');

    overlay.acceptsMouseEvents = false;
    root.add(overlay);

    //scroll
    const scroll = this.createRect().w(50).h(200).fill('#0000ff').x(400).y(50).id('scroll');

    root.add(scroll);
    scroll.acceptsScrollEvents = true;

    this.on('scroll', scroll, function (e) {
        scroll.y(scroll.y() + e.position);
    });

    overlay.acceptsKeyboardEvents = true;

    this.on('key.press', overlay, function (e) {
        console.log('key.press event', e.keycode, e.printable, e.char);
    });

});
