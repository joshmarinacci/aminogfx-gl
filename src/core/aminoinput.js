'use strict';

/*
 * the job of this class is to route input events and calculate semantic events.
 * it does not map from platform specific events to generic ones. that is the job
 * of the inputevents module
 */

var DEBUG = false;

var IE = require('./inputevents');

/**
 * Create a point.
 */
function makePoint(x, y) {
    return {
        x: x,
        y: y,
        minus: function () {
            if (arguments.length == 1) {
                var pt = arguments[0];

                return makePoint(this.x - pt.x, this.y - pt.y);
            }

            if (arguments.length == 2) {
                var xy = arguments;

                return makePoint(this.x - xy[0], this.y - xy[1]);
            }
        },
        divide: function (x, y) {
            return makePoint(this.x / x, this.y / y);
        }
    };
}

exports.makePoint = makePoint;

/**
 * Initialize this module.
 */
exports.init = function (OS) {
    if (DEBUG) {
        console.log('initializing input for OS', OS);
    }

    this.OS = OS;

    IE.init();
};

var listeners = {};
var focusobjects = {
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

var statusobjects = {
    pointer: {
        pt: makePoint(-1, -1),
        prevpt: makePoint(-1, -1)
    },
    keyboard: {
        state: {}
    }
};

var handlers = {
    'mouse.position': function (gfx, evt) {
        var s = statusobjects.pointer;

        s.prevpt = s.pt;
        s.pt = makePoint(evt.x, evt.y);

        var target = focusobjects.pointer.target;

        if (target != null && statusobjects.pointer.state == 1) {
            sendDragEvent(gfx, evt);
        }
    },

    'mouse.button': function (gfx, evt) {
        if (evt.button == 0) {
            if (DEBUG) {
                console.log('left mousebutton event');
            }

            statusobjects.pointer.state = evt.state;

            if (evt.state == 1) {
                var pts = statusobjects.pointer;

                if (DEBUG) {
                    console.log('-> pressed');
                }

                setupPointerFocus(gfx, pts.pt);
                sendPressEvent(gfx,evt);
                return;
            }

            if (evt.state == 0) {
                if (DEBUG) {
                    console.log('-> released');
                }

                sendReleaseEvent(gfx, evt);
                stopPointerFocus();
                return;
            }
        }
    },

    'mousewheel.v': function (gfx, evt) {
        var pts = statusobjects.pointer;

        setupScrollFocus(gfx, pts.pt);
        sendScrollEvent(gfx, evt);
    },

    'key.press': function (gfx, evt) {
        statusobjects.keyboard.state[evt.keycode] = true;

        var evt2;

        if (this.OS == 'BROWSER') {
            evt2 = IE.fromBrowserKeyboardEvent(evt, statusobjects.keyboard.state);
        } else {
            evt2 = IE.fromAminoKeyboardEvent(evt, statusobjects.keyboard.state);
        }

        sendKeyboardPressEvent(gfx, evt2);
    },

    'key.release': function (gfx, evt) {
        statusobjects.keyboard.state[evt.keycode] = false;
        sendKeyboardReleaseEvent(gfx, IE.fromAminoKeyboardEvent(evt, statusobjects.keyboard.state));
    },

    'window.close': function (gfx, evt) {
        fireEventAtTarget(null, evt);
    }
};

/**
 * Process an event.
 */
exports.processEvent = function (gfx, evt) {
    if (DEBUG) {
        console.log('processEvent() ' + JSON.stringify(evt));
    }

    var handler = handlers[evt.type];

    if (handler) {
        return handler(gfx, evt);
    }

    //console.log('unhandled event', evt);
};

/**
 * Register event handler.
 */
exports.on = function (name, target, listener) {
    //name
    if (!name) {
        throw new Error('missing name');
    }

    name = name.toLowerCase();

    if (!listeners[name]) {
        listeners[name] = [];
    }

    //special case (e.g. window.size handler)
    if (!listener) {
        //two parameters set
        listener = target;
        target = null;
    }

    //add
    listeners[name].push({
        target: target,
        func: listener
    });
};

/**
 * Set pointer focus on node.
 */
function setupPointerFocus(gfx, pt) {
    if (DEBUG) {
        console.log('setupPointerFocus()');
    }

    var nodes = gfx.findNodesAtXY(pt, function (node) {
        if (node.children && node.acceptsMouseEvents === false) {
            return false;
        }

        return true;
    });

    var pressnodes = nodes.filter(function (n) {
        return n.acceptsMouseEvents === true;
    });

    if (pressnodes.length > 0) {
        focusobjects.pointer.target = pressnodes[0];
    } else {
        focusobjects.pointer.target = null;
    }

    var keyboardnodes = nodes.filter(function (n) {
        return n.acceptsKeyboardEvents === true;
    });

    if (keyboardnodes.length > 0) {
        if (focusobjects.keyboard.target) {
            fireEventAtTarget(focusobjects.keyboard.target, {
                type: 'focus.lose',
                target: focusobjects.keyboard.target,
            });
        }

        focusobjects.keyboard.target = keyboardnodes[0];
        fireEventAtTarget(focusobjects.keyboard.target, {
            type: 'focus.gain',
            target: focusobjects.keyboard.target,
        });
    } else {
        if (focusobjects.keyboard.target) {
            fireEventAtTarget(focusobjects.keyboard.target, {
                type: 'focus.lose',
                target: focusobjects.keyboard.target,
            });
        }

        focusobjects.keyboard.target = null;
    }

}

function stopPointerFocus() {
    focusobjects.pointer.target = null;
}

function sendPressEvent(gfx, e) {
    var node = focusobjects.pointer.target;

    if (node == null) {
        return;
    }

    var pt = gfx.globalToLocal(statusobjects.pointer.pt, node);

    fireEventAtTarget(node, {
        type: 'press',
        button: e.button,
        point: pt,
        target: node
    });
}

function sendReleaseEvent(gfx, e) {
    var node = focusobjects.pointer.target;

    if (node == null) {
        return;
    }

    var pt = gfx.globalToLocal(statusobjects.pointer.pt, node);

    fireEventAtTarget(node, {
        type: 'release',
        button: e.button,
        point: pt,
        target: node,
    });

    if (node.contains(pt)) {
        fireEventAtTarget(node, {
            type: 'click',
            button: e.button,
            point: pt,
            target: node,
        });
    }
}

function sendDragEvent(gfx, e) {
    var node = focusobjects.pointer.target;
    var s = statusobjects.pointer;

    if (node == null) {
        return;
    }

    var localpt = gfx.globalToLocal(s.pt, node);
    var localprev = gfx.globalToLocal(s.prevpt, node);

    fireEventAtTarget(node, {
        type: 'drag',
        button: e.button,
        point: localpt,
        delta: localpt.minus(localprev),
        target: node
    });
}

function setupScrollFocus(gfx, pt) {
    var nodes = gfx.findNodesAtXY(pt);
    var nodes = nodes.filter(function (n) {
        return n.acceptsScrollEvents === true;
    });

    if (nodes.length > 0) {
        focusobjects.scroll.target = nodes[0];
    } else {
        focusobjects.scroll.target = null;
    }
}

function sendScrollEvent(gfx, e) {
    var target = focusobjects.scroll.target;

    if (target == null) {
        return;
    }

    fireEventAtTarget(target, {
        type: 'scroll',
        target: target,
        position: e.position,
    });
}

function sendKeyboardPressEvent(gfx, event) {
    if (focusobjects.keyboard.target === null) {
        return;
    }

    event.type = 'key.press';
    event.target = focusobjects.keyboard.target;
    fireEventAtTarget(event.target, event);
}

function sendKeyboardReleaseEvent(gfx, event) {
    if (focusobjects.keyboard.target === null) {
        return;
    }

    event.type = 'key.release';
    event.target = focusobjects.keyboard.target;
    fireEventAtTarget(event.target, event);
}

function fireEventAtTarget(target, event) {
    if (DEBUG) {
        console.log('firing an event at target:', event.type, target ? target.id():'');
    }

    if (!event.type) {
        console.log('Warning: event has no type!');
    }

    var funcs = listeners[event.type];

    if (funcs) {
        funcs.forEach(function (l) {
            if (l.target == target) {
                l.func(event);
            }
        });
    }
};
