'use strict';

//launch: node --expose-gc demos/tests/gc.js

var amino = require('../../main.js');

//GC test
console.log('create temporary instance');

var gfx = new amino.AminoGfx();

//unref
gfx.destroy();
gfx = null;

//create more instances
for (let i = 0; i < 10; i++) {
    console.log('instance: ' + i);

    //create
    let gfx = new amino.AminoGfx();

    //modify root
    let group = gfx.createGroup();
    let rect = gfx.createRect();

    group.add(rect);
    gfx.setRoot(group);

    //destroy
    gfx.destroy();
}

//force GC (frees root node)
if (global.gc) {
    console.log('forcing GC');

    global.gc();

    console.log('done GC');
} else {
    console.log('launch test with --expose-gc parameter');
}

//force GC (free AminoGfx instances)
setInterval(function () {
    if (global.gc) {
        console.log('forcing GC');
        global.gc();
        console.log('done GC');
    }
}, 1000);
