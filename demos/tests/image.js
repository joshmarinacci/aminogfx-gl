'use strict';

const amino = require('../../main.js');
const path = require('path');

//basic image tests
let img = new amino.AminoImage();

img.onload = function (err, img) {
    if (err) {
        console.log('could not load image: ' + err.message);
        return;
    }

    console.log('image loaded: ' + this.w + 'x' + this.h + ' (' + img.w + 'x' + img.h + ')');
};

img.src = path.join(__dirname, '../slideshow/images/iTermScreenSnapz001.png');
