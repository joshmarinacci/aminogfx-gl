'use strict';

/*eslint no-unused-vars: 0*/

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

//fonts
testFont();

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

    g.w.bindTo(this.w);
    g.h.bindTo(this.h);

    this.setRoot(g);

    //add rect
    const r = this.createRect().w(100).h(100).fill('#FF0000');

    r.originX(.5).originY(.5).rz(45);

    g.add(r);

    //animation
    r.b.anim().from(0).to(1).dur(2000).autoreverse(true).loop(1).then(animDone).start();
    //r.b.anim().from(0).to(1).dur(2000).autoreverse(true).loop(-1).then(animDone).start(); //Note: no extra garbage seen
    r.b.watch((value) => {

        console.log('animation state: ' + value);
    });

    function animDone() {
        console.log('animation done');
    }

    //polygon
    const poly = this.createPolygon().dimension(2).fill('#00FF00');

    poly.geometry([100, 10, 150, 300, 200, 10]);
    g.add(poly);

    //circle
    const circle = this.createCircle().radius(50)
        .fill('#ffcccc').filled(true)
        .x(200).y(200).opacity(0.2);

    g.add(circle);

    //image
    const iv = this.createImageView().x(300).y(300);

    iv.src(path.join(__dirname, '../images/tree.png'));

    // 1) stretch
    iv.size('stretch');

    // 2) resize
    //iv.size('resize');

    // 3) cover
    /*
    iv.w(100).h(50);
    iv.size('cover');
    */

    // 4) contain
    iv.w(100).h(50);
    //iv.w(50).h(100); //clamp test
    iv.size('contain');

    // 5) position
    //iv.position('left');
    //iv.position('right');
    //iv.position('right top');
    iv.position('right center');

    g.add(iv);

    //GC tests
//    testImages(g);
    //testAnimGC();
    testGC();
    //testProps();

    //text
    const text = this.createText()
        .text('This is a very long text which is wrapped.\nNew line here.\n  white space.  ')
        .fontSize(80)
        .x(0).y(0)
        .vAlign('top')
        .w(300)
        .h(160)
        .wrap('word')
        .fill('#ffff00');

    //text.text('äöü');

    g.add(text);
});

function testImages(g) {
    let items = [];
    let w = g.w();
    let h = g.h();
    let img = path.join(__dirname, '../images/tree.png');

    //FIXME 10000: Too many open files in system
    const count = 1000;

    for (let i = 0; i < count; i++) {
        const iv = gfx.createImageView().x(Math.random() * w).y(Math.random() * h);

        iv.src(img);

        items.push(iv);
        g.add(iv);
    }

    setTimeout(() => {
        items.forEach((item) => {
            g.remove(item);
        });
    }, 4000);

    /*
     * References:
     *
     * - ImageView (gets zero -> deallocated)
     * - Group (gets reduced)
     * - AminoImage (temporary, will be freed first -> deallocated)
     * - Texture (-> deallocated last)
     */
}

function testFont() {
    //default font
    amino.fonts.getFont(null, function (err, font) {
        if (err) {
            console.log('could not load font: ' + err.message);
            return;
        }

        console.log('default font: ' + JSON.stringify(font));
        console.log('font metrics: ' + JSON.stringify(font.getFontMetrics()));

        //width
        const startTime = (new Date()).getTime();

        font.calcTextWidth('The quick brown fox jumps over the lazy dog. Pack meinen Kasten mit fünf Dutzend Alkoholkrügen.', function (err, width) {
            if (err) {
                console.log('could not get text width: ' + err.message);
                return;
            }

            console.log('width: ' + width);

            const endTime = (new Date()).getTime();

            console.log(' -> in ' + (endTime - startTime) + ' ms');
        });

    });
}

function testAnimGC() {
    //not visible
    const r = gfx.createRect().x(0).y(0).w(100).h(100);

    // 1) keep & release
    /*
    const anims = [];

    for (let i = 0; i < 1000; i++) {
        let anim = r.x.anim().from(0).to(1).dur(2000).start();

        anims.push(anim);
    }

    setInterval(() => {
        //free 100
        for (let i = 0; i < 100; i++) {
            let anim = anims.pop();

            if (anim) {
                //stop now (or wait for 2 s duration)
                //anim.stop();
            }
        }
    }, 100);
    */

    // 2) create large amount
    setInterval(() => {
        //create some garbage
        for (let i = 0; i < 1000; i++) {
            let anim = r.x.anim().from(0).to(1).dur(2000).start();

            //Note: stopped after 2 seconds
        }

        //result: no leaks seen
    }, 10000);
}

function testGC() {
    setInterval(() => {
        //create some garbage
        for (let i = 0; i < 1000; i++) {
            gfx.createCircle();
            gfx.createRect();

            //Note: without object creation seeing memory increase of 2 KB/s which is reset after about 3 MB are collected. The GC cycle takes quite some time!
        }
    }, 10000);
}

function testProps() {
    //result: no extra garbage
    const r = gfx.createRect();
    const g = gfx.createGroup();

    setInterval(() => {
        for (let i = 0; i < 1000; i++) {
            //value change
            r.x(i);

            //call
            if (i & 0x1) {
                g.add(r);
            } else {
                g.remove(r);
            }
        }
    }, 10000);
}

function testWindow(title) {
    const gfx = new amino.AminoGfx();

    gfx.w(200);
    gfx.h(200);
    gfx.title(title);

    gfx.start(function (err) {
        if (err) {
            console.log('start failed: ' + err.message);
        }

        const g = this.createGroup();
        const text = this.createText().text(title).vAlign('top');

        g.add(text);
        this.setRoot(g);
    });

    gfx.on('window.close', function () {
        console.log('closing window');

        gfx.destroy();
    });

    return gfx;
}

if (!gfx.screen.fullscreen) {
    testWindow('Window 1').x(0).y(0);
    testWindow('Window 2').x(200).y(0);
}

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
            lastGC = new Date();
        }

        //show raw value
        console.log(JSON.stringify(stats));

        //more details
        if (step % 5 == 0) {
            if (lastGC) {
                console.log('Last GC: ' + (new Date().getTime() - lastGC) + ' ms');
            }

            console.log('Below max heap: ' + ((maxHeapUsed - stats.heapUsed) / 1024 / 1024) + ' MB' + ' (heap: ' + (stats.heapUsed / 1024 / 1024) + ' MB)');
        }

        lastHeapUsed = stats.heapUsed;
        step++;
    }, 1000);
}

checkMemory();
