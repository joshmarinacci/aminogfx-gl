'use strict';

const path = require('path');
const amino = require('../../main.js');
const child = require('child_process');

const Workman = {
    count: 4,
    chs:[],
    init: function (chpath, cb, count) {
        if (typeof count === 'number') {
            this.count = count;
        }

        console.log('using thread count', this.count);

        for (let i = 0; i < this.count; i++) {
            this.chs[i] = child.fork(chpath);
            this.chs[i].on('message', cb);
        }
    },
    sendcount: 0,
    sendWork: function (msg) {
        this.chs[this.sendcount % this.chs.length].send(msg);
        this.sendcount++;
    }
};

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    const root = this.createGroup().x(0).y(0);
    const pv = this.createPixelView().pw(500).w(500).ph(500).h(500);

    root.add(pv);
    this.setRoot(root);
    this.w(500);
    this.h(500);

    function generateMandlebrot() {
        const w = pv.pw();
        const h = pv.ph();

        function handleRow(m) {
            const y = m.iy;

            for (let x = 0; x < m.row.length; x++) {
                const c = lookupColor(m.row[x]);

                pv.setPixel(x, y, c[0], c[1], c[2], 255);
            }

            pv.updateTexture();
        }

        const workman = Workman;

        workman.init(path.join(__dirname, 'mandle_child.js'), handleRow);

        for (let y = 0; y < h; y++) {
            const py = (y - h / 2) * 0.01;
            const msg = {
                x0: (- w / 2) * 0.01,
                x1: (+ w / 2) * 0.01,
                y: py,
                iw: w,
                iy: y,
                iter: 100000,
            };

            workman.sendWork(msg);
        }
    }

    const lut = [];

    for (let i = 0; i < 10; i++) {
        const s = (255 / 10) * i;

        lut.push([0, s, s]);
    }

    function lookupColor(iter) {
        return lut[iter % lut.length];
    }

    generateMandlebrot();
});
