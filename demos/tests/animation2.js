'use strict';

const amino = require('../../main.js');
const path = require('path');

const gfx = new amino.AminoGfx();
const refTime = gfx.getTime();
const items = [];

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    this.fill('#FF0000');

    //animation
    const group = this.createGroup();

    this.setRoot(group);

    addAnim(group, 0);

    //add more
    let y = 50;
    let count = 0;

    addAnimLater();

    function addAnimLater() {
        setTimeout(() => {
            count++;
            addAnim(group, y);
            y += 50;

            if (count > 10) {
                return;
            }

            addAnimLater();
        }, Math.random() * 3000);
    }

    //verify
    setInterval(() => {
        let values = '';
        let count = items.length;

        for (let i = 0; i < count; i++) {
            if (i > 0) {
                values += ', ';
            }

            values += items[i].x();
        }

        //Note: all values should have the exact same x-value (numeric errors on last digits possible)
        console.log(values);
    }, 1000);

    //some info
    console.log('screen: ' + JSON.stringify(gfx.screen));
    console.log('window size: ' + this.w() + 'x' + this.h());
});

function addAnim(group, y) {
    //create image view
    const iv = gfx.createImageView().y(y).src(path.join(__dirname, '../images/tree.png'));

    items.push(iv);

	//animate x position
    const anim = iv.x.anim().from(0).to(300).dur(10000).loop(-1).autoreverse(true);

    //tests
    //anim.pos(100);

    if (refTime) {
        anim.start(refTime);
    } else {
        anim.start();
    }

    group.add(iv);
}