'use strict';

const amino = require('../../main.js');

//func call test (must fail)
//console.log('res: ' + amino.AminoGfx());

//basic tests
const gfx = new amino.AminoGfx();

console.log('instance: ' + gfx);

//screen
console.log('screen: ' + JSON.stringify(gfx.screen));

//default size
console.log('default size: ' + gfx.w() + 'x' + gfx.h());

//start
gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    console.log('started');

    //runtime
    console.log('runtime: ' + JSON.stringify(gfx.runtime));

    //show position
    console.log('position: ' + this.x() + '/' + this.y());
    //this.x(0).y(0);

    //modify size
    this.w(400);
    this.h(400);
    this.fill('#0000FF');

    //create group
    const g = this.createGroup();

    this.setRoot(g);

    //add rect
    const r = (this.createRect()).w(100).h(100).fill('#FF0000');

    g.add(r);

    //animation
    r.b.anim().from(0).to(1).dur(2000).autoreverse(true).loop(-1).start();
    r.b.watch((value) => {
        console.log('animation state: ' + value);
    });

    //TODO more cbx
});
