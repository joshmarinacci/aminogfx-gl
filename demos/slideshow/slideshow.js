/*
 * Sample: node demos/slideshow/slideshow.js demos/slideshow/images/
 */

const amino = require('../../main.js');
const fs = require('fs');
const path = require('path');

if (process.argv.length < 3) {
    console.log('you must provide a directory to use');
    return;
}

const dir = process.argv[2];
const filelist = fs.readdirSync(dir).map(function (file) {
    return path.join(dir, file);
});

function CircularBuffer(arr) {
    this.arr = arr;
    this.index = -1;
    this.next = function() {
        this.index = (this.index + 1) % this.arr.length;

        return this.arr[this.index];
    }
}

//wrap files in a circular buffer
const files = new CircularBuffer(filelist);

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    //setup size
    this.w(800);
    this.h(600);

    //init root
    const root = this.createGroup();

    this.setRoot(root);

    const sw = this.w();
    const sh = this.h();

    //create two image views
    const iv1 = this.createImageView().x(0);
    const iv2 = this.createImageView().x(1000);

    //add to the scene
    root.add(iv1, iv2);

    //auto scale them
    function scaleImage(img, prop, obj) {
        if (!img) {
            console.log('missing image!');
            return;
        }

        const scale = Math.min(sw / img.w, sh / img.h);

        obj.sx(scale).sy(scale);
    }

    iv1.image.watch(scaleImage);
    iv2.image.watch(scaleImage);

    //load the first two images
    iv1.src(files.next());
    iv2.src(files.next());

    //animate out and in
    function swap() {
        //move left (out)
        iv1.x.anim().delay(1000).from(0).to(-sw).dur(3000).start();

        //move left (in)
        iv2.x.anim().delay(1000).from(sw).to(0).dur(3000)
            .then(afterAnim).start();
    }
    swap();

    function afterAnim() {
        //swap images and move views back in place
        iv1.x(0);
        iv2.x(sw);
        iv1.image(iv2.image());
        iv2.src(files.next());

        //recurse
        swap();
    }

});
