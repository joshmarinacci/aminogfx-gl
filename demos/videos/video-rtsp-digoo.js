'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play RTSP stream.
 */
const ip = '192.168.1.89';

player.playVideo({
    //
    //  Digoo M1Q (default password, on local network => modify parameters; see https://www.ispyconnect.com/man.aspx?n=Digoo#)
    //

    //FIXME cbx lost packages
    src: 'rtsp://admin:20160404@' + ip + '/onvif1'
    //src: 'rtsp://admin:20160404@' + ip + '/onvif2' //low res

    //not working
    //src: 'rtsp://admin:20160404@' + ip + '/live/ch00_0' //HD
    //src: 'rtsp://admin:20160404@' + ip + '/live/ch00_1' //SD
}, (err, video) => {
    //empty
});