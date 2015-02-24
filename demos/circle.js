var amino = require('../main.js');

amino.start(function(core, stage) {
    //stage.setRoot(rect);
    var root = new amino.Group();
    stage.setRoot(root);
    var rect = new amino.Rect().w(200).h(250).x(0).y(0).fill("#0000ff");
    root.add(rect);

    var circle = new amino.Circle().radius(50)
        .fill('#ffcccc').filled(true)
        .x(100).y(100);
    //root.x(100);
    root.add(circle);
});
