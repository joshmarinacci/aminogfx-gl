'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play 720p video stream.
 */

player.playVideo({
    //Note: TCP stream needed (UDP timeout, retrying with TCP)
    src: 'https://rrr.sz.xlcdn.com/?account=streamzilla&file=Streamzilla_Demo_HD_Ready.mp4&type=download&service=apache&protocol=https&output=mp4',
//cbx not working
    opts: 'amino_dump_format=1'
}, (err, video) => {
    //empty
});