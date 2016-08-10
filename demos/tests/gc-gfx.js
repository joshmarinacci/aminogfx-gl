'use strict';

//launch: node --expose-gc demos/tests/gc-gfx.js

var amino = require('../../main.js');

//GC test
console.log('create temporary instance');

var gfx = new amino.AminoGfx();
var gfxCollected = false;
var weak = new amino.AminoWeakReference(gfx, () => {
    gfxCollected = true;
    console.log('-> gfx garbage collected');
});

//unref
gfx.destroy();
gfx = null;

//create more instances
var instanceCount = 0;

for (let i = 0; i < 10; i++) {
    console.log('instance: ' + i);

    //create
    let gfx = new amino.AminoGfx();

    instanceCount++;

    new amino.AminoWeakReference(gfx, () => {
        instanceCount--;
    });

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
let done = false;
let cycle = 0;

setInterval(function () {
    if (global.gc) {
        //console.log('forcing GC');
        global.gc();
        cycle++;
        //console.log('done GC');

        if (!done && instanceCount == 0) {
            done = true;
            console.log('-> all AminoGfx instances freed (in ' + cycle + ' GC steps)');
        }
    }
}, 1000);

/*

You should get this output:

  -> gfx garbage collected
  -> all AminoGfx instances freed (in 2 GC steps)

*/