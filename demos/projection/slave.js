'use strict';

console.log('I am the slave. :(');

var amino = require('../../main.js');

var gfx = new amino.AminoGfx();

gfx.start(function (err) {
    if (err) {
        console.log('Start failed: ' + err.message);
        return;
    }

    var root = this.createGroup();

    this.w(400);
    this.h(400);
    this.setRoot(root);

    function configure(m) {
        console.log('SLAVE: configuring', m);

        if (m.props.w) {
            gfx.w(m.props.w);
            gfx.h(m.props.h);
            gfx.x(m.props.x);
            gfx.y(m.props.y);
        }

        if (m.props.dx) {
            root.x(m.props.dx);
        }

        if (m.props.dy) {
            root.y(m.props.dy);
        }
    }

    function make(m) {
        if (m.target == 'amino.Rect') {
            var obj = gfx.createRect();

            for (var name in m.props) {
                obj[name](m.props[name]);
            }

            root.add(obj);
        }
    }

    function find(root, id) {
        for (var i = 0; i < root.children.length; i++) {
            var child = root.children[i];

            if (child.id() == id) {
                return child;
            }
        }

        return null;
    }

    /**
     * Update properties.
     */
    function update(m) {
        var obj = find(root, m.id);

        if (!obj) {
            throw new Error('Node not found: ' + m.id);
        }

        for (var name in m.props) {
            //call setter
            obj[name](m.props[name]);
        }
    }

    /**
     * Animate property of node.
     */
    function anim(m) {
        var obj = find(root, m.id);
        var prop = obj[m.prop];
        var anim = prop.anim().from(m.anim.from).to(m.anim.to).dur(m.anim.dur).loop(m.anim.loop);

        anim.start();
    }

    //process messages
    console.log('waiting for messages');

    process.on('message', function (m) {
        console.log('message: ' + JSON.stringify(m));

        switch (m.command) {
            case 'configure':
                configure(m);
                break;

            case 'make':
                make(m);
                break;

            case 'update':
                update(m);
                break;

            case 'anim':
                anim(m);
                break;

            default:
                console.log('SLAVE: unknown command!', m);
                break;
        }
    });

    //send ready
    process.send({ command: 'ready' });
});
