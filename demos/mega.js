'use strict';

var moment = require('moment');
var path = require('path');
var amino = require('../main.js');

exports.go = function (canvas) {
	//amino.setCanvas(canvas);
	amino.start(function (core, stage){
		//output width
		console.log('width = ', stage.w());

		//register font
	    core.registerFont({
	        name: 'Oswald',
	        path: path.join(__dirname, '/oswald/'),
	        weights: {
	            200: {
	                normal: 'Oswald-Light.ttf'
	            },
	            400: {
	                normal: 'Oswald-Regular.ttf'
	            },
	            800: {
	                normal: 'Oswald-Bold.ttf'
	            }
	        }
	    });

		//root
		var root = new amino.Group();

		function simpleTests() {
			//add rect
			var rect1 = new amino.Rect().w(100).h(50).fill('#ff00ff');

			root.add(rect1);

			//loop animate the rect position

			//loop animate the opacity of the rect

			//plain text
			var text1 = new amino.Text().text('some green text')
				.fontSize(20)
				.fill('#00ff00').x(20).y(20);

			root.add(text1);

			//text with custom font
			var text2 = new amino.Text().text('a custom font')
				.fontSize(30)
				.fontName('Oswald')
				.x(20).y(120);

			root.add(text2);
		}

		simpleTests();

		/**
		 * press drag release a rect
		 * click a rect
		 * press drag under an overlay
		 */
		function pressDragReleaseTest() {
			var g = new amino.Group();

			//text
			var t = new amino.Text().text('press to turn blue, then drag').x(20).y(20).fontSize(20);

            g.add(t);

			//green rect
			var r = new amino.Rect().w(50).h(50).fill('#00ff00').x(50).y(50);

			r.acceptsMouseEvents = true;

			core.on('press', r, function () {
				//turn blue
				r.fill('#0000ff');
			});

			core.on('drag', r, function (e) {
				//move
				r.x(r.x() + e.delta.x);
				r.y(r.y() + e.delta.y);
			});

			core.on('release', r, function () {
				//reset color
				r.fill('#00ff00');
			});

			g.add(r);

			//overlay rect
			var overlay = new amino.Rect().w(100).h(100).fill('#cccccc').x(20).y(100);

			overlay.opacity(0.6);
			overlay.acceptsMouseEvents = true;
			g.add(overlay);

			//text
			var tt = new amino.Text().text('mouse blocking overlay').x(20).y(100).fontSize(20);

			g.add(tt);
			g.x(250).y(20);

			root.add(g);
		}

		pressDragReleaseTest();

		/**
		 * Animated images.
		 */
		function imageSwappingTests() {
			//image 1
			var iv = new amino.ImageView();
			var im1 = null;

			iv.image.watch(function () {
				if (im1) {
					return;
				}

				im1 = iv.image();

				//console.log('i1 loaded', im1);
			});

			iv.src(path.join(__dirname, '/images/tree.png'));
			iv.sx(4).sy(4);

			//group (animation: rotation)
			var g = new amino.Group();

			g.add(iv);
            g.rz.anim().from(0).to(360 * 4).dur(10000).loop(-1).start();
			g.x(20).y(300);
			root.add(g);

			//image 2
			var iv2 = new amino.ImageView();
			var im2 = null;

			iv2.image.watch(function () {
				if (im2) {
					return;
				}

				im2 = iv2.image();

				//console.log('i2 loaded', im2);
			});

			iv2.src(path.join(__dirname, '/images/bridge.png'));

			//console.log("image = ", iv2.image(), iv.image());

			//image change timer
			setInterval(function () {
				if (!im1 || !im2) {
					return;
				}

				//swap image view 1 image
				if (iv.image() == im1) {
					iv.image(im2);
				} else {
					iv.image(im1);
				}
			}, 500);
		}

		imageSwappingTests();

		/**
		 * Reversed animation.
		 */
		function imageAnimTests() {
			//create image view
            var i1 = new amino.ImageView().src(path.join(__dirname, '/images/tree.png'));

			//animate x position
			i1.x.anim().from(0).to(100).dur(1000).loop(-1).autoreverse(true).start();

			//animate opacity
			i1.opacity.anim().from(0).to(1.0).dur(1000).loop(-1).autoreverse(true).start();

			//group
			var g = new amino.Group().x(20).y(500).add(i1);

			root.add(g);
		}

		imageAnimTests();

		//done
		stage.setRoot(root);

		//stage background & opacity
		setTimeout(function () {
			stage.fill('#ff0000');
		}, 2000);

		setTimeout(function () {
            stage.opacity(0.1);
		}, 4000);

	});
};

exports.go();