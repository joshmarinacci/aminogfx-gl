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

    //
    // RTSP (UDP)
    //

    //1280x960
    src: 'rtsp://admin:20160404@' + ip + '/onvif1',

    //320x240
    //src: 'rtsp://admin:20160404@' + ip + '/onvif2',

    //
    // Tests
    //

    //not working (method DESCRIBE failed: 401 Unauthorized)
    //src: 'rtsp://' + ip + '/live/ch00_0', //HD
    //src: 'rtsp://' + ip + '/live/ch00_1', //SD

    //not working (1) CSeq 4 expected, 0 received. 2) Server returned 400 Bad Request)
    //src: 'rtsp://admin:20160404@' + ip + '/live/ch00_0', //HD
    //src: 'rtsp://admin:20160404@' + ip + '/live/ch00_1' //SD
    //src: 'rtsp://admin:20160404@' + ip + '/tcp/av0_0',
    //src: 'rtsp://admin:20160404@' + ip + '/tcp/av0_1',

    //
    // Options
    //

    //opts: 'amino_realtime=0' //packet based timing (do not use!)
    //opts: 'rtsp_transport=udp pkt_size=65535 buffer_size=256000' //UDP
    //opts: 'rtsp_transport=tcp pkt_size=65535 buffer_size=256000' //TCP
}, (err, video) => {
    //empty
});