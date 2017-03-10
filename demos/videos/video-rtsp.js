'use strict';

const path = require('path');
const player = require('./player');

/*
 * Play RTSP stream.
 */

player.playVideo({
    //Note: TCP stream needed (UDP timeout, retrying with TCP)
    src: 'rtsp://mpv.cdn3.bigCDN.com:554/bigCDN/definst/mp4:bigbuckbunnyiphone_400.mp4'

    //Digoo M1Q (default password, on local network => modify parameters; see https://www.ispyconnect.com/man.aspx?n=Digoo#)
    //src: 'rtsp://admin:20160404@192.168.1.89/onvif1'
}, (err, video) => {
    //empty
});