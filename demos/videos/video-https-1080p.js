'use strict';

const player = require('./player');

/*
 * Play 1080p video stream.
 */

player.playVideo({
    //Note: TCP stream needed (UDP timeout, retrying with TCP)
    src: 'https://rrr.sz.xlcdn.com/?account=streamzilla&file=Streamzilla_Demo_HD.mp4&type=download&service=apache&protocol=https&output=mp4',
    opts: 'amino_dump_format=1'
}, (_err, _video) => {
    //empty
});