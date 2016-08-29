'use strict';

const amino = require('../../main.js');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Amino error: ' + err.message);
        return;
    }

    //root
    const root = gfx.createGroup();

    gfx.setRoot(root);

    //triangle
    const model1 = gfx.createModel();

    //showTriangle(model1);
    showTriangles(model1);

    root.add(model1);

    //some info
    console.log('screen: ' + JSON.stringify(gfx.screen));
    console.log('window size: ' + this.w() + 'x' + this.h());
});


function showTriangle(model) {
    model1.vertices([ 0,0,0, 100,50,0, 200, 200,0 ]);
}

function showTriangles(model) {
    const count = 1000;
    const vertexCount = count * 3;
    const vertices = [];
    const w = gfx.w();
    const h = gfx.h();

    for (let i  = 0; i < vertexCount; i++) {
        vertices.push(Math.random() * w);
        vertices.push(Math.random() * h);
        vertices.push((Math.random() - 0.5) * 50);
    }

    model.vertices(vertices);
}
