'use strict';

const amino = require('../../main.js');
const FeedParser = require('feedparser');
const http = require('http');

let sw = 1280;
let sh = 720;

parseFeed('http://www.npr.org/rss/rss.php?id=1001', function (titles) {
    titles = new CircularBuffer(titles);

    const gfx = new amino.AminoGfx();

    gfx.start(function (err) {
        if (err) {
            console.log('Start failed: ' + err.message);
            return;
        }
        //setup
        console.log("stage = ", this.w(), this.h());

        if (this.w() > 100) {
            sw = this.w();
            sh = this.h();
        }

        //root
        const root = this.createGroup();

        this.w(sw);
        this.h(sh);
        this.setRoot(root);

        //background
        const bg = this.createGroup();

        root.add(bg);

        //text group
        const textgroup = this.createGroup();

        root.add(textgroup);

        const line1 = this.createText().x(50).y(200).fill('#ffffff').fontSize(80);
        const line2 = this.createText().x(50).y(300).fill('#ffffff').fontSize(80);

        textgroup.add(line1, line2);

        function rotateOut() {
            textgroup.ry.anim().delay(5000).from(0).to(140).dur(2000).then(rotateIn).start();
        }

        function rotateIn() {
            setHeadlines(titles.next(), line1, line2);
            textgroup.ry.anim().from(220).to(360).dur(2000).then(rotateOut).start();
        }

        function setHeadlines(headline, t1, t2) {
            const max = 34;

            //simple wrapping (does not really work)
            if (headline.length > max) {
                t1.text(headline.substring(0, max));
                t2.text(headline.substring(max));
            } else {
                t1.text(headline);
                t2.text('');
            }
        }

        rotateIn();

        //three rects that fill the screen: red, green, blue.  50% translucent
        const rect1 = this.createRect().w(sw).h(sh).opacity(0.5).fill('#ff0000');
        const rect2 = this.createRect().w(sw).h(sh).opacity(0.5).fill('#00ff00');
        const rect3 = this.createRect().w(sw).h(sh).opacity(0.5).fill('#0000ff');

        bg.add(rect1, rect2, rect3);

        //animate the back two rects
        rect1.x(-1000);
        rect2.x(-1000);
        rect1.x.anim().from(-1000).to(1000).dur(5000)
            .loop(-1).autoreverse(true).start();
        rect2.x.anim().from(-1000).to(1000).dur(3000)
            .loop(-1).autoreverse(true).delay(5000).start();
    });
});

function parseFeed(url, cb) {
    const headlines = [];

    http.get(url, function (res) {
        res.pipe(new FeedParser())
            .on('meta',function (_meta) {
                //console.log('the meta is',meta);
            })
            .on('data',function (article) {
                console.log('title = ', article.title);
                headlines.push(article.title);
            })
            .on('end',function () {
                console.log('ended');
                cb(headlines);
            });
    });
}

/**
 * Circular buffer.
 */
function CircularBuffer(arr) {
    this.arr = arr;
    this.index = -1;
    this.next = function () {
        this.index = (this.index + 1) % this.arr.length;

        return this.arr[this.index];
    };
}
