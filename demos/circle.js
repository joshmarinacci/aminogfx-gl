var amino = require('../main.js');

amino.start(function(core, stage) {
    var rect = new amino.Rect().w(150).h(250).x(0).y(0).fill("#ff0000");
    stage.setRoot(rect);
    //var root = new amino.Group();
    //stage.setRoot(root);
    //var circle = new amino.Circle().radius(50)
    //    .fill('#ffcccc').filled(true)
    //    .x(100).y(100);
    //root.add(circle);
});
