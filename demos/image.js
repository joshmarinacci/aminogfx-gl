var amino = require('../main.js');

amino.start(function (core, stage) {
    //root
    var root = new amino.Group();

    stage.setRoot(root);

    //add image view
    var iv = new amino.ImageView().opacity(1.0).w(200).h(200);

    iv.src('demos/slideshow/images/iTermScreenSnapz001.png');
    root.add(iv);

    var rect = new amino.Rect().w(20).h(30).fill('#ff00ff');

    root.add(rect);
});
