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
    const rect = this.createRect().w(300).h(50).fill('#cccccc').x(50).y(50).id('bg');

    rect.acceptsMouseEvents = true;
    rect.acceptsKeyboardEvents = true;
    root.add(rect);

    this.on('focus.gain', rect, function () {
        rect.fill('#ffffff');
    });

    this.on('focus.lose', rect, function () {
        rect.fill('#cccccc');
    });

    //cursor
    const cursor = this.createRect().w(3).h(40).fill('#ff0000').x(50 + 20).y(55).id('cursor');

    root.add(cursor);

    //label
    const label = this.createText().x(50).y(50 + 40).fill('#000000').id('text').text('F').fontSize(40);

    root.add(label);

    this.on('key.press', rect, function (e) {
        console.log('got keyboard event', e.printable, e.key, e.keycode, e.char);

        if (e.printable === true) {
            label.text(label.text() + e.char);

            label.font().calcTextWidth(label.text(), function (err, w) {
                cursor.x(50 + w);
            });
        }

        if (e.key == 'BACK_DELETE') {
            let text = label.text();

            if (text) {
                text = text.substring(0, text.length - 1);
                label.text(text);

                label.font().calcTextWidth(text, function (err, w) {
                    cursor.x(50 + w);
                });
            }
        }

    });
});
