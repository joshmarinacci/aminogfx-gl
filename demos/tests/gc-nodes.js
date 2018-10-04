'use strict';

//launch: node --expose-gc demos/tests/gc-nodes.js

/*eslint no-unused-vars: 0*/

const amino = require('../../main.js');

//create single instance
const gfx = new amino.AminoGfx();

//start
gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    //Note: 3 AminoJSObject instances so far
    console.log('-> started');

    //stress tests
    //generateWeakReferenceGarbage();
    generateRectGarbage(5 * 1000);
});

function callGC(log) {
    if (global.gc) {
        if (log) {
            console.log('forcing GC');
        }

        global.gc();

        if (log) {
            console.log('done GC');
        }
    } else {
        console.log('launch test with --expose-gc parameter');
    }
}

//monitor heap
function checkMemory() {
    let maxHeapUsed = 0;
    let lastGC = null;
    let lastHeapUsed = 0;
    let step  = 0;

    setInterval(() => {
        const stats = gfx.getStats();

        //max heap
        if (stats.heapUsed > maxHeapUsed) {
            maxHeapUsed = stats.heapUsed;
        }

        //GC
        if (stats.heapUsed < lastHeapUsed) {
            lastGC = Date.now();
        }

        //show raw value
        console.log(JSON.stringify(stats));

        //more details
        if (step % 5 == 0) {
            if (lastGC) {
                console.log('Last GC: ' + (Date.now() - lastGC) + ' ms');
            }

            console.log('Below max heap: ' + ((maxHeapUsed - stats.heapUsed) / 1024 / 1024) + ' MB' + ' (heap: ' + (stats.heapUsed / 1024 / 1024) + ' MB)');
        }

        lastHeapUsed = stats.heapUsed;
        step++;
    }, 1000);
}

checkMemory();

/**
 * Generate huge amount of AminoWeakReference instances.
 *
 * Note: those instances are independent of AminoGfx
 */
function generateWeakReferenceGarbage() {
    const count = 10000;

    setInterval(() => {
        let obj = {};

        for (let i = 0; i < count; i++) {
            new amino.AminoWeakReference(obj);
        }

        //GC (Note: v8 keeps 10k instances alive)
        callGC(false);
    }, 100);
}

function generateRectGarbage(maxTime) {
    const startTime = Date.now();
    const timer = setInterval(() => {
        for (let i = 0; i < 10000; i++) {
            gfx.createRect();
        }

        //check end
        if (Date.now() > startTime + maxTime) {
            clearInterval(timer);
            callGC(true);
        }
    }, 100);
}