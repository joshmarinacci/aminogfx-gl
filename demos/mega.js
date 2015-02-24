var moment = require('moment');
var amino = require('../main.js');
exports.go = function(canvas) {
	//amino.setCanvas(canvas);
	amino.start(function(core,stage){
		console.log("width = ", stage.w());
        stage.smooth(false);
	    core.registerFont({
	        name:'Oswald',
	        path:__dirname+'/oswald/',
	        weights: {
	            200: {
	                normal:'Oswald-Light.ttf',
	            },
	            400: {
	                normal:'Oswald-Regular.ttf',
	            },
	            800: {
	                normal:'Oswald-Bold.ttf',
	            }
	        }
	    });

		var root = new amino.Group();

		function simpleTests() {
			var rect1 = new amino.Rect().w(100).h(50).fill("#ff00ff");
			root.add(rect1);
			//loop animate the rect position

			//loop animate the opacity of the rect

			//plain text
			var text1 = new amino.Text().text("some green text")
				.fontSize(20)
				.fill("#00ff00").x(20).y(20);
			root.add(text1);

			//text with custom font
			var text2 = new amino.Text().text("a custom font")
				.fontSize(30)
				.fontName("Oswald")
				.x(20).y(120);
			root.add(text2);
		}
		simpleTests();




		//press drag release a rect
		function pressDragReleaseTest() {
			var g = new amino.Group();
			var t = new amino.Text().text("press to turn blue, then drag").x(20).y(20).fontSize(20);
            g.add(t);
			var r = new amino.Rect().w(50).h(50).fill("#00ff00").x(50).y(50);
			r.acceptsMouseEvents = true;
			core.on('press',r,function() {
				r.fill("#0000ff");
			});
			core.on('drag',r,function(e) {
				r.x(r.x()+e.delta.x);
				r.y(r.y()+e.delta.y);
			});
			core.on('release',r,function(){
				r.fill("#00ff00");
			});



			g.add(r);


			var overlay = new amino.Rect().w(100).h(100).fill("#cccccc").x(20).y(100);
			overlay.opacity(0.6);
			overlay.acceptsMouseEvents = true;
			g.add(overlay);
			var tt = new amino.Text().text("mouse blocking overlay").x(20).y(100).fontSize(20);
			g.add(tt);

			g.x(250).y(20);
			root.add(g);
		}
		pressDragReleaseTest();
		//click a rect
		//press drag under an overlay


		function imageSwappingTests() {
			var iv = new amino.ImageView();
			iv.src(__dirname+'/images/tree.png');
			iv.sx(4).sy(4);
			var g = new amino.Group();
			g.add(iv);
			g.x(20).y(300);
			root.add(g);

			var iv2 = new amino.ImageView();
			iv2.src(__dirname+'/images/bridge.png');
			//console.log("image = ", iv2.image(),iv.image());
			var im1 = null;
			iv.image.watch(function() {
				if(im1 != null) return;
				//console.log("i1 loaded");
				im1 = iv.image();
			});
			var im2 = null;
			iv2.image.watch(function() {
				if(im2 != null) return;
				im2 = iv2.image();
				//console.log("i2 loaded",im2);
			});
			setInterval(function(){
				if(im1 == null) return;
				if(im2 == null) return;
				if(iv.image() == im1) {
					iv.image(im2);
				} else {
					iv.image(im1);
				}
			},500);
		}
		imageSwappingTests();

		function imageAnimTests() {
			var i1 = new amino.ImageView().src(__dirname+'/images/tree.png');
			i1.x.anim().from(0).to(100).dur(1000).loop(-1).start();
			i1.opacity.anim().from(0).to(1.0).dur(1000).loop(-1).start();

			var g = new amino.Group().x(20).y(500).add(i1);
			root.add(g);
		}
		imageAnimTests();


		stage.setRoot(root);


		setTimeout(function(){
			stage.fill("#ff0000");
		},2000)

		setTimeout(function() {
			//stage.transparent(true);
		},4000);

	});
}




exports.go();