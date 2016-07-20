'use strict';

const amino = require('../../main.js');
const path = require('path');

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

    r.originX(.5).originY(.5).rz(45);

    g.add(r);

    //animation
//    r.b.anim().from(0).to(1).dur(2000).autoreverse(true).loop(1).then(animDone).start();
    r.b.watch((value) => {
        console.log('animation state: ' + value);
    });

    function animDone() {
        console.log('animation done');
    }

    //polygon
    var poly = this.createPolygon().dimension(2).fill('#00FF00');

    poly.geometry([100, 10, 150, 300, 200, 10]);
    g.add(poly);

    //circle
    var circle = this.createCircle().radius(50)
        .fill('#ffcccc').filled(true)
        .x(200).y(200).opacity(0.2);

    g.add(circle);

    //image
    var iv = this.createImageView().x(300).y(300).sx(2).sy(2);

    iv.src(path.join(__dirname, '../images/tree.png'));
    g.add(iv);

    //TODO more cbx
});
