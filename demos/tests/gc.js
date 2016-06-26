'use strict';

//launch: node --expose-gc demos/tests/gc.js

var amino = require('../../main.js');

//GC test
var gfx = new amino.AminoGfx();

//unref
gfx = null;

//create two more instances
new amino.AminoGfx();
amino.AminoGfx();

//force GC
if (global.gc) {
    global.gc();
} else {
    console.log('launch test with --expose-gc parameter');
}

setTimeout(function () {
    console.log('30s gone');
}, 30 * 1000);
