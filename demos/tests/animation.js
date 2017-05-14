'use strict';

const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    this.fill('#000000');

    //create group
    const g = this.createGroup();

    this.setRoot(g);

    //animation
    const r = this.createRect().x(0).y(0).w(100).h(100);

    r.fill('#FFFFFF');

    r.b.anim().from(0).to(1).dur(2000).autoreverse(true).loop(-1).start();
    r.b.watch((value) => {
        console.log('animation state: ' + value);
    });

    g.add(r);
});

function testAsync() {
    //wait 1 second
    setTimeout(() => {
        //block forever (attention: block Ctrl+C on macOS, have to kill task)
        //while (true) {}

        //block for 1 second
        setInterval(() => {
            block(1000);
        }, 2000);

    }, 1000);
}

function block(max) {
    const time = new Date().getTime();

    while (new Date().getTime() - time < max) {
        //loop
    }
}

testAsync();
