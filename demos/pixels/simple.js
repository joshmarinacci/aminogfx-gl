'use strict';

var amino = require('../../main.js');

var gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    var root = this.createGroup().x(100).y(100);
    var pv = this.createPixelView().pw(300).w(300).ph(300).h(300);

    root.add(pv);

    this.setRoot(root);
});
