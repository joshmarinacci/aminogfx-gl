'use strict';

//Attention: increase GPU memory! With 64 MB the rendering suddenly gets very slow while drawing polygons!!!

//Raspberry Pi 3 1080p@60: 13 fps (polygons could be improved)

const amino = require('../../main.js');
const data = require('./countries.js');
const onecolor = require('onecolor');

const fs = require('fs');
const path = require('path');
const cities = JSON.parse(fs.readFileSync(path.join(__dirname, 'cities.json')).toString());

let w = 1280; //1920;
let h = 768;  //1080;
const radius = w / 6;
const animated = true;

amino.fonts.registerFont({
    name: 'mech',
    path: path.join(__dirname, 'resources/'),
    weights: {
        400: {
            normal: 'MechEffects1BB_reg.ttf',
            italic: 'MechEffects1BB_ital.ttf'
        }
    }
});

const gfx = new amino.AminoGfx({
    //resolution: '1080p@24'
    //resolution: '720p@50'
    //resolution: '720p@60'
});

gfx.w(w);
gfx.h(h);

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //setup
    if (this.w() > 100) {
        w = this.w();
        h = this.h();
    }

    this.w(w);
    this.h(h);

    //root
    const root = this.createGroup();

    this.setRoot(root);

    //the globe
    const group = this.createGroup();

    root.add(group);

    buildGlobe(group);

    //lower left bar charts
    root.add(createBar1(200, 50, 15, '#3333ff').x(5).y(500));
    root.add(createBar1(200, 50, 15, '#ffff33').x(5).y(700).sy(-1));

    makeHeader(root);

    makeFooterSymbols(root);

    //root.add(createParticles().x(w-250).y(h));

    root.add(buildDashboard().y(130));

});

function buildDashboard() {
    const group = gfx.createGroup();

    function addLine(text, x, y, glyph) {
        group.add(gfx.createRect()
            .x(x + 5)
            .y(y - 25)
            .w(200)
            .h(32)
            .fill('#ea5036')
        );
        group.add(gfx.createText()
            .text(text)
            .fill('#fcfbcf')
            .fontName('mech')
            .fontSize(30)
            .x(x+10)
            .y(y)
        );
        if (glyph) {
            group.add(gfx.createText()
                .text(glyph)
                .fill('#fcfbcf')
                .fontName('awesome')
                .fontSize(20)
                .x(x + 200 - 25)
                .y(y - 2)
            );
        }
    }

    function addSmallLine(text, x, y) {
        group.add(gfx.createText()
            .text(text)
            .fill('#fcfbcf')
            .fontName('mech')
            .fontSize(20)
            .x(x + 15)
            .y(y)
        );
    }

    {
        let x = 0;
        addLine("ENDO SYS OS / MAKR PROC", x, 0);
        addSmallLine("RDOZ - 25889", x, 30);
        addSmallLine("ZODR - 48639", x, 50);
        addSmallLine("FEEA - 92651", x, 70);
        addSmallLine("DEAD - 02833", x, 90);
    }

    {
        let x = 0;
        let y = 130;
        addLine("FOO_MAR.TCX", x, y + 0, '\uf071');
        addSmallLine("analysis - 48%", x, y + 30);
        addSmallLine("actualizing - 99%", x, y + 50);
        addSmallLine("rentrance - 0.02%", x, y + 70);
    }

    {
        let x = w - 200 - 10;
        let y = 0;
        addLine("Core Extraction", x, y + 0, '\uf0e4');
        addSmallLine("pulverton - 143.888", x, y + 30);
        addSmallLine("minotaur - 105%", x, y + 50);
        addSmallLine("gravitation 26.8%", x, y + 70);
    }

    {
        let x = w - 200 - 10;
        let y = 120;
        addLine("FEED CR55X \\ Analysis", x, y + 0);
        addSmallLine("reconst - 48%", x, y + 30);
        addSmallLine("fargonite - 99%", x, y + 50);
        addSmallLine("sleestack - 0.02%", x, y + 70);

    }
    {
        let x = w - 200 - 10;
        let y = 250;
        addLine("$SHIP_CAM$.FXD", x, y + 0, '\uf06d');
        addSmallLine("fracturizing - 128%", x, y + 30);
        addSmallLine("detox - 43%", x, y + 50);
        addSmallLine("xantos 45% 8%", x, y + 70);
    }
    {
        let x = w - 200 - 10;
        let y = 350;
        addLine("XenoPhage", x, y + 0, '\uf126');
        addSmallLine("scent analysis - 128%", x, y + 30);
        addSmallLine("oxygenize - 43%", x, y + 50);
        addSmallLine("heliotrop **", x, y + 70);
    }

    return group;
}

function makeFooterSymbols(root) {
    for (let i = 0; i < 7; i++)  {
        const sun = gfx.createGroup().x(w / 2 - 300 + i * 100).y(h - 25).rz(30);

        //see http://fontawesome.io/icon/caret-up/
        sun.add(gfx.createText()
            .fontName('awesome').fontSize(80).text('\uf0d8')
            .x(-25).y(25).fill('#fcfbcf')
        );

        const start = Math.random() * 90 - 45;
        const len = Math.random() * 5000 + 5000;

        if (animated) {
            //rotate
            sun.rz.anim().from(start).to(start + 90).dur(len).loop(-1).autoreverse(true).start();
        }

        root.add(sun);
    }
}

function makeHeader(root) {
    const fontH = 70;

    root.add(gfx.createRect().fill('#ff0000').w(w).h(100).opacity(0.5));
    root.add(gfx.createText()
        .text('Awesomonium Levels')
        .fontSize(fontH)
        .fontName('mech')
        .x(20)
        .y(70)
        .fill('#fcfbcf')
    );
    root.add(createBar1(50, 100, 5, '#fcfbcf').x(400).y(75).rz(-90));
    /*
    root.add(gfx.createText()
        .text('Atomization')
        .fontSize(fontH)
        .fontName('mech')
        .x(w - 320)
        .y(70)
        .fill('#fcfbcf')
    );
    */

    //beaker symbol
    root.add(gfx.createText()
        .fontName('awesome').text('\uf0c3').fontSize(80)
        .x(w - 85).y(70).fill('#fcfbcf')
    );
}

function buildGlobe(group) {
    const cos = Math.cos;
    const sin = Math.sin;
    const PI = Math.PI;

    function latlon2xyz(lat,lon, rad) {
        const el = lat / 180.0 * PI;
        const az = lon / 180.0 * PI;
        const x = rad * cos(el) * sin(az);
        const y = rad * cos(el) * cos(az);
        const z = rad * sin(el);

        return [x, y, z];
    }

    function addCountry(nz) {
        //make the geometry
        for (let i = 0; i < nz.borders.length; i++) {
            const border = nz.borders[i];
            const points = [];
            const poly = gfx.createPolygon();

            for (let j = 0; j < border.length; j++) {
                const point = border[j];
                const pts = latlon2xyz(point.lat, point.lng, radius);

                points.push(pts[0]);
                points.push(pts[1]);
                points.push(pts[2]);
            }

            poly.fill('#80ff80');
            poly.geometry(points);
            poly.dimension(3);
            group.add(poly);
        }
    }

    for (let i = 0; i < data.countries.length; i++) {
        addCountry(data.countries[i]);
    }

    // NOTE: on Raspberry pi we can't just make a line.
    // A polygon needs at least two segments.
    function addLine(lat, lon, el, color) {
        const poly = gfx.createPolygon();
        const pt1 = latlon2xyz(lat, lon, radius);
        const pt2 = latlon2xyz(lat, lon, radius + el);
        const pt3 = latlon2xyz(lat, lon, radius);
        const points = pt1.concat(pt2).concat(pt3);

        poly.fill(color);
        poly.geometry(points);
        poly.dimension(3);
        group.add(poly);
    }

    //add a line at portland
    cities.features.forEach(function (city) {
        //const color = '#ff00ff';
        const hue = city.properties.city.length / 20;

        addLine(city.geometry.coordinates[1],
                city.geometry.coordinates[0],
                100 * hue,
                onecolor('red').hue(hue).hex());
    });

    // center
    group.x(w / 2).y(h / 2);

    //turn earth upright
    group.rx(90);
    group.ry(0);
    group.rz(0);

    // spin it forever
    if (animated) {
        //rotate the globe
        group.rz.anim().from(0).to(360).dur(60 * 1000).loop(-1).start();
    }
}

function createBar1(w, h, count, color) {
    const gr = gfx.createGroup();
    const rects = [];
    const barw = w / count;

    for (let i = 0; i < count; i++) {
        const rect = gfx.createRect()
            .x(i * barw).y(0)
            .w(barw - 5)
            .h(30)
            .fill(color);

        rects.push(rect);
        gr.add(rect);
    }

    function update() {
        rects.forEach(function (rect) {
            rect.h(20 + Math.random() * (h - 20));
        });
    }

    if (animated) {
        //resize bars (10x a second)
        setInterval(update, 100);
    }

    return gr;
}

/*
function frand(min, max) {
    return Math.random() * (max - min) + min;
}
*/
