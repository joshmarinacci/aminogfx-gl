'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play M4V video file.
 */

player.playVideo({
    src: path.join(__dirname, 'trailer_iphone.m4v'),
    opts: 'amino_dump_format=1'
}, (_err, _video) => {
    //empty
});