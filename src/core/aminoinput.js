'use strict';

/*
 * the job of this class is to route input events and calculate semantic events.
 * it does not map from platform specific events to generic ones. that is the job
 * of the inputevents module
 */

const DEBUG = false;

const IE = require('./inputevents');

/**
 * Create a point.
 */
function makePoint(x, y) {
    return {
        x: x,
        y: y,
        minus: function () {
            if (arguments.length == 1) {
                const pt = arguments[0];

                return makePoint(this.x - pt.x, this.y - pt.y);
            }

            if (arguments.length == 2) {
                const xy = arguments;

                return makePoint(this.x - xy[0], this.y - xy[1]);
            }
        },
        divide: function (x, y) {
            return makePoint(this.x / x, this.y / y);
        },
        multiply: function (x, y) {
            return makePoint(this.x * x, this.y * y);
        }
    };
}

exports.makePoint = makePoint;

/**
 * Initialize this module.
 */
exports.init = function () {
    if (DEBUG) {
        console.log('initializing input');
    }

    IE.init();
};

//
// AminoEvents
//

function AminoEvents(gfx) {
    this.gfx = gfx;

    //properties
    this.listeners = {};
    this.focusObjects = {
        pointer: {
            target: null
        },
        scroll: {
            target: null
        },
        keyboard: {
            target: null
        }
    };

    this.statusObjects = {
        pointer: {
            pt: makePoint(-1, -1),
            prevPt: makePoint(-1, -1)
        },
        keyboard: {
            state: {}
        }
    };
}

/**
 * Create new instance.
 */
exports.createEventHandler = function (gfx) {
    if (DEBUG) {
        console.log('createEventHandler()');
    }

    return new AminoEvents(gfx);
};

const handlers = {
    'mouse.position': function (obj, evt) {
        const s = obj.statusObjects.pointer;

        s.prevPt = s.pt;
        s.pt = makePoint(evt.x, evt.y);

        const target = obj.focusObjects.pointer.target;

        if (target != null && obj.statusObjects.pointer.state == 1) {
            obj.sendDragEvent(evt);
        }
    },
    'mouse.button': function (obj, evt) {
        if (evt.button == 0) {
            //left mouse button
            if (DEBUG) {
                console.log('left mousebutton event');
            }

            obj.statusObjects.pointer.state = evt.state;

            //pressed
            if (evt.state == 1) {
                const pts = obj.statusObjects.pointer;

                if (DEBUG) {
                    console.log('-> pressed');
                }

                obj.setupPointerFocus(pts.pt);
                obj.sendPressEvent(evt);
                return;
            }

            //released
            if (evt.state == 0) {
                if (DEBUG) {
                    console.log('-> released');
                }

                obj.sendReleaseEvent(evt);
                obj.stopPointerFocus();
                return;
            }
        }
    },
    'mousewheel.v': function (obj, evt) {
        const pts = obj.statusObjects.pointer;

        obj.setupScrollFocus(pts.pt);
        obj.sendScrollEvent(evt);
    },
    'key.press': function (obj, evt) {
        obj.statusObjects.keyboard.state[evt.keycode] = true;

        const evt2 = IE.fromAminoKeyboardEvent(evt, obj.statusObjects.keyboard.state);

        obj.sendKeyboardPressEvent(evt2);
    },
    'key.release': function (obj, evt) {
        obj.statusObjects.keyboard.state[evt.keycode] = false;
        obj.sendKeyboardReleaseEvent(IE.fromAminoKeyboardEvent(evt, obj.statusObjects.keyboard.state));
    },
    'window.size': function (obj, evt) {
        obj.fireEventAtTarget(null, evt);
    },
    'window.close': function (obj, evt) {
        obj.fireEventAtTarget(null, evt);
    }
};

/**
 * Process an event.
 */
AminoEvents.prototype.processEvent = function (evt) {
    if (DEBUG) {
        console.log('processEvent() ' + JSON.stringify(evt));
    }

    const handler = handlers[evt.type];

    if (handler) {
        return handler(this, evt);
    }

    //console.log('unhandled event', evt);
};

/**
 * Register event handler.
 */
AminoEvents.prototype.on = function (name, target, listener) {
    //name
    if (!name) {
        throw new Error('missing name');
    }

    name = name.toLowerCase();

    if (!this.listeners[name]) {
        this.listeners[name] = [];
    }

    //special case (e.g. window.size handler)
    if (!listener) {
        //two parameters set
        listener = target;
        target = null;
    }

    //add
    this.listeners[name].push({
        target: target,
        func: listener
    });
};

/**
 * Set pointer focus on node.
 */
AminoEvents.prototype.setupPointerFocus = function (pt) {
    if (DEBUG) {
        console.log('setupPointerFocus()');
    }

    //get all nodes
    const nodes = this.gfx.findNodesAtXY(pt, function (node) {
        //filter opt-out
        if (node.children && node.acceptsMouseEvents === false) {
            return false;
        }

        return true;
    });

    //get nodes accepting mouse events
    const pressNodes = nodes.filter(function (n) {
        return n.acceptsMouseEvents === true;
    });

    if (pressNodes.length > 0) {
        this.focusObjects.pointer.target = pressNodes[0];
    } else {
        this.focusObjects.pointer.target = null;
    }

    const keyboardNodes = nodes.filter(function (n) {
        return n.acceptsKeyboardEvents === true;
    });

    if (keyboardNodes.length > 0) {
        if (this.focusObjects.keyboard.target) {
            this.fireEventAtTarget(this.focusObjects.keyboard.target, {
                type: 'focus.lose',
                target: this.focusObjects.keyboard.target,
            });
        }

        this.focusObjects.keyboard.target = keyboardNodes[0];

        this.fireEventAtTarget(this.focusObjects.keyboard.target, {
            type: 'focus.gain',
            target: this.focusObjects.keyboard.target,
        });
    } else {
        if (this.focusObjects.keyboard.target) {
            this.fireEventAtTarget(this.focusObjects.keyboard.target, {
                type: 'focus.lose',
                target: this.focusObjects.keyboard.target,
            });
        }

        this.focusObjects.keyboard.target = null;
    }
};

AminoEvents.prototype.stopPointerFocus = function () {
    this.focusObjects.pointer.target = null;
};

AminoEvents.prototype.sendPressEvent = function (e) {
    const node = this.focusObjects.pointer.target;

    if (node == null) {
        return;
    }

    const pt = this.gfx.globalToLocal(this.statusObjects.pointer.pt, node);

    this.fireEventAtTarget(node, {
        type: 'press',
        button: e.button,
        point: pt,
        target: node
    });
};

AminoEvents.prototype.sendReleaseEvent = function (e) {
    const node = this.focusObjects.pointer.target;

    if (node == null) {
        return;
    }

    //release
    const pt = this.gfx.globalToLocal(this.statusObjects.pointer.pt, node);

    this.fireEventAtTarget(node, {
        type: 'release',
        button: e.button,
        point: pt,
        target: node,
    });

    //click
    if (node.contains(pt)) {
        this.fireEventAtTarget(node, {
            type: 'click',
            button: e.button,
            point: pt,
            target: node,
        });
    }
};

AminoEvents.prototype.sendDragEvent = function (e) {
    const node = this.focusObjects.pointer.target;
    const s = this.statusObjects.pointer;

    if (node == null) {
        return;
    }

    const localpt = this.gfx.globalToLocal(s.pt, node);
    const localprev = this.gfx.globalToLocal(s.prevPt, node);

    this.fireEventAtTarget(node, {
        type: 'drag',
        button: e.button,
        point: localpt,
        delta: localpt.minus(localprev),
        target: node
    });
};

AminoEvents.prototype.setupScrollFocus = function (pt) {
    let nodes = this.gfx.findNodesAtXY(pt);

    nodes = nodes.filter(function (n) {
        return n.acceptsScrollEvents === true;
    });

    if (nodes.length > 0) {
        this.focusObjects.scroll.target = nodes[0];
    } else {
        this.focusObjects.scroll.target = null;
    }
};

AminoEvents.prototype.sendScrollEvent = function (e) {
    const target = this.focusObjects.scroll.target;

    if (target == null) {
        return;
    }

    this.fireEventAtTarget(target, {
        type: 'scroll',
        target: target,
        position: e.position,
    });
};

AminoEvents.prototype.sendKeyboardPressEvent = function (event) {
    if (this.focusObjects.keyboard.target === null) {
        return;
    }

    event.type = 'key.press';
    event.target = this.focusObjects.keyboard.target;

    this.fireEventAtTarget(event.target, event);
};

AminoEvents.prototype.sendKeyboardReleaseEvent = function (event) {
    if (this.focusObjects.keyboard.target === null) {
        return;
    }

    event.type = 'key.release';
    event.target = this.focusObjects.keyboard.target;

    this.fireEventAtTarget(event.target, event);
};

AminoEvents.prototype.fireEventAtTarget = function (target, event) {
    if (DEBUG) {
        console.log('firing an event at target:', event.type, target ? target.id():'');
    }

    if (!event.type) {
        console.log('Warning: event has no type!');
    }

    const funcs = this.listeners[event.type];

    if (funcs) {
        funcs.forEach(function (l) {
            if (l.target == target) {
                l.func(event);
            }
        });
    }
};
