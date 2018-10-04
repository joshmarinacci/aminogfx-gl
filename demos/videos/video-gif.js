'use strict';

const player = require('./player');

/*
 * Play animated gif.
 *
 * Note: not supported on RPi yet.
 *
 * FIXME FFmpeg errors
 * FIXME libav reports EOF
 */

player.playVideo({
    //timer
    //FIXME error during playback (IO Error: -9806)
    src: 'https://4.bp.blogspot.com/-yaHmYH_Lb48/VtnZzJVSYkI/AAAAAAAAQIM/EcqeCm_Tea0/s1600/07%2Bcountdown%2Bdown%2Bup%2B02b.gif',

    //dots
    //FIXME ends with error
    //src: 'https://media.giphy.com/media/vPmJKooQbFTOw/giphy.gif',

    //Buckminster fullerene
    //FIXME ends with error
    //src: 'https://upload.wikimedia.org/wikipedia/commons/3/3b/Buckminsterfullerene_animated.gif',

    //firework
    //FIXME ends with error (LZW decode failed)
    //src: 'http://bestanimations.com/Holidays/Fireworks/fireworks/fireworks-animated-gif-23.gif',

    //geometrie
    //FIXME ends with error (IO Error: -9806)
    //src: 'http://www.clicktorelease.com/code/gif/1.gif',

    //meme
    //FIXME loop reload gif again and again (delay & wasted resources)
    //src: 'https://media.giphy.com/media/rF2Cplr6KCDG8/giphy.gif',

    opts: 'amino_dump_format=1'
}, (_err, _video) => {
    //empty
});