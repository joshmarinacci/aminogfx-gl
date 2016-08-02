var amino = require('../main.js');

var gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //root
    var root = this.createGroup();

    this.setRoot(root);

    //rect
    var rect = this.createRect().fill('#00ff00').opacity(1.0);

    root.add(rect);
    rect.opacity.anim().from(1.0).to(0.0).dur(1000).loop(-1).start();

    //text
    var text = this.createText().fill('#ff0000').opacity(1.0).x(100).y(200);

    text.text('Sample Text');
    text.opacity.anim().from(0.0).to(1.0).dur(1000).loop(-1).start();
    root.add(text);

    //circle
    var circle = this.createCircle().radius(50)
        .fill('#0000ff').filled(true)
        .opacity(0.5)
        .x(200).y(50);

    root.add(circle);
});
