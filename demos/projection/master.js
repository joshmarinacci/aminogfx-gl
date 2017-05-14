'use strict';

const cp = require('child_process');
const amino = require('../../main.js');

console.log('I am the master!');

//create children
const children = [];
let readyCount = 0;

function createChild() {
    const child = cp.fork('demos/projection/slave.js');

    children.push(child);

    //receive child messages
    child.on('message', function (m) {
        console.log('Message from child: ' + JSON.stringify(m));

        if (m.command == 'ready') {
            readyCount++;

            if (readyCount == 2) {
                start();
            }
        }
    });
}

createChild();
createChild();

function projectProp(val, prop, obj) {
    const props = {};

    props[prop.propName] = obj[prop.propName]();

    children.forEach(function (ch) {
        ch.send({
            command: 'update',
            id: obj.id(),
            props: props
        });
    });
}

function Rect() {
    amino.makeProps(this, {
        id: 'rect1',
        w: 100,
        h: 65,
        fill: '#ffffff'
    });

    this.w.watch(projectProp);
    this.h.watch(projectProp);
    this.fill.watch(projectProp);
}

function make(target, props) {
    children.forEach(function (ch) {
        ch.send({
            command: 'make',
            target: target,
            props: props
        });
    });
}

/**
 * Animation proxy.
 */
function anim(id, prop) {
    return {
        from: function (val) {
            this._from = val;
            return this;
        },
        to: function (val) {
            this._to = val;
            return this;
        },
        dur: function (val) {
            this._dur = val;
            return this;
        },
        loop: function (val) {
            this._loop = val;
            return this;
        },
        send: function (_val) {
            const self = this;

            children.forEach(function (ch) {
                ch.send({
                    command: 'anim',
                    id:      id,
                    prop:    prop,
                    anim: {
                        from: self._from,
                        to:   self._to,
                        dur:  self._dur,
                        loop: self._loop,
                    }
                });
            });
        },
    };
}

function start() {
    //configure the child windows
    console.log('configuring...');

    for (let i = 0; i < children.length; i++) {
        children[i].send({
            command: 'configure',
            props: {
                w: 400,
                h: 400,
                dx: i * -400,
                dy: 0,
                x: i * 400,
                y: 0
            }
        });
    }

    //create rect
    const rect = new Rect();

    make('amino.Rect', { id: rect.id() });

    setTimeout(function () {
        rect.w(200);
        rect.h(200);
        rect.fill('#00ffcc');
        anim(rect.id(), 'x').from(0).to(1000).dur(1000).loop(-1).send();
    }, 3000);
}
