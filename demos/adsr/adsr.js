const amino = require('../../main.js');

function Adsr() {
    amino.makeProps(this, {
        a: 100,
        d: 200,
        s: 50,
        r: 300
    });

    return this;
}

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //create the model
    const adsr = new Adsr();

    //root for the whole window
    const root = this.createGroup();

    this.setRoot(root);

    //group for the polygons and controls (not the text labels)
    const g = this.createGroup();

    root.add(g);

    //4 polygons, each a different color
    const aPoly = this.createPolygon().fill('#00eecc');
    const dPoly = this.createPolygon().fill('#00cccc');
    const sPoly = this.createPolygon().fill('#00aacc');
    const rPoly = this.createPolygon().fill('#0088aa');

    g.add(aPoly, dPoly, sPoly, rPoly);
    g.find('Polygon').filled(true);

    //5th polygon for the border, not filled
    const border = this.createPolygon().fill('#ffffff').filled(false);

    g.add(border);

    //update the polygons when the model changes
    function updatePolys() {
        border.geometry([0, 200,
            adsr.a(), 50,
            adsr.d(), adsr.s(),
            adsr.r(), adsr.s(),
            300, 200
        ]);
        aPoly.geometry([
            0, 200,
            adsr.a(), 50,
            adsr.a(), 200
        ]);
        dPoly.geometry([
            adsr.a(), 200,
            adsr.a(), 50,
            adsr.d(), adsr.s(),
            adsr.d(), 200,
        ]);
        sPoly.geometry([
            adsr.d(), 200,
            adsr.d(), adsr.s(),
            adsr.r(), adsr.s(),
            adsr.r(), 200,
        ]);
        rPoly.geometry([
            adsr.r(), 200,
            adsr.r(), adsr.s(),
            300, 200
        ]);
    }

    adsr.a.watch(updatePolys);
    adsr.d.watch(updatePolys);
    adsr.s.watch(updatePolys);
    adsr.r.watch(updatePolys);

    //util function
    function minus(coeff) {
        return function (val) {
            return val - coeff;
        };
    }

    //make a handle bound to the adsr.a value
    const A = this.createRect().y(50 - 10);

    A.acceptsMouseEvents = true;

    A.x.bindTo(adsr.a, minus(10));

    this.on('press', A, function (_e) {
        //adsr.a(e.target.x());
    });

    this.on('drag', A, function (e) {
        adsr.a(adsr.a() + e.delta.x);
    });

    //make a handle bound to the adsr.d value
    const D = this.createRect();

    D.acceptsMouseEvents = true;
    D.x.bindTo(adsr.d, minus(10));
    D.y.bindTo(adsr.s, minus(10));

    this.on('press', D, function (_e) {
        //adsr.d(e.target.x());
        //adsr.s(e.target.y());
    });

    this.on('drag', D, function (e) {
        adsr.d(adsr.d() + e.delta.x);
        adsr.s(adsr.s() + e.delta.y);
    });

    //make a handle bound to the adsr.r value
    const R = this.createRect();

    R.acceptsMouseEvents = true;
    R.y.bindTo(adsr.s, minus(10));
    R.x.bindTo(adsr.r, minus(10));

    this.on('press', R, function (_e) {
        //adsr.s(e.target.y());
        //adsr.r(e.target.x());
    });

    this.on('drag', R, function (e) {
        adsr.s(adsr.s() + e.delta.y);
        adsr.r(adsr.r() + e.delta.x);
    });

    //add and style the handles
    g.add(A, D, R);
    g.find('Rect').fill('#00ffff').w(20).h(20);

    //util function for formatted strings
    function format(str) {
        return function (v) {
            return str.replace("%", v);
        };
    }

    //make 4 text labels, each bound to an adsr value
    const label1 = this.createText().y(50 * 1);

    label1.text.bindTo(adsr.a, format('A: %'));

    const label2 = this.createText().y(50 * 2);

    label2.text.bindTo(adsr.d, format('D: %'));

    const label3 = this.createText().y(50 * 3);

    label3.text.bindTo(adsr.s, format('S: %'));

    const label4 = this.createText().y(50 * 4);

    label4.text.bindTo(adsr.r, format('R: %'));

    //add them all and style them
    root.add(label1, label2, label3, label4);
    root.find('Text').x(10).fill('#ffffff');

    //move the whole thing 200px right
    g.x(200).y(0);

    //set intial values of the model, forces an update of everything
    adsr.a(50).d(100).s(100).r(250);
});
