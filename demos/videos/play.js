'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play video file.
 *
 * Usage:
 *
 *   - node play.js <video file> <parameters>
 */
const params = process.argv.length - 2;

if (params <= 0) {
    console.log('missing video file parameter');
    return;
}

const file = process.argv[2];
const data = {
    src: path.join(process.cwd(), file),
    opts: 'amino_dump_format=1',
    //rot90: true,
    gfxOpts: {
        //resolution: '1080p@25'
        //resolution: '1080p@30'
        //resolution: '1080p@60'
    }
};

for (let i = 1; i < params; i++) {
    const param = process.argv[2 + i];

    if (param.indexOf('@') > 0) {
        //resolution
        data.gfxOpts.resolution = param;
    } else if (param === 'rot90') {
        //rotate 90°
        data.rot90 = true;
    } else {
        //center
        data.center = param;
    }
}

player.playVideo(data, (err, video) => {
    //empty
});