const amino = require('../main.js');

const gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    //root
    const root = this.createGroup();

    this.setRoot(root);

    //add image view
    const iv = this.createImageView().opacity(1.0).w(200).h(200);

    iv.src('demos/slideshow/images/iTermScreenSnapz001.png');
    root.add(iv);

    const rect = this.createRect().w(20).h(30).fill('#ff00ff');

    root.add(rect);
});
