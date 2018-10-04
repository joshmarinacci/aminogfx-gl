const amino = require('../main.js');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //clipped group as root
    const root = this.createGroup()
        .x(100).y(100).w(100).h(100).clipRect(true);

    this.setRoot(root);

    //add circle (0/0 to 50/50; bottom-right quadrant visible)
    const circle = this.createCircle().radius(50)
        .fill('#ffcccc').filled(true)
        .x(0).y(0);

    root.add(circle);
});
