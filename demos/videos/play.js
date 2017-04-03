'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play video file.
 */

if (process.argv.length !== 3) {
    console.log('missing video file parameter');
    return;
}

const file = process.argv[2];

player.playVideo({
    src: path.join(process.cwd(), file),
    opts: 'amino_dump_format=1',
    //cbx check
    gfxOpts: {
        //resolution: '1080p@25'
        resolution: '1080p@30'
    }
}, (err, video) => {
    //empty
});