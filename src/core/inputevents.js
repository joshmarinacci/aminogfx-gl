'use strict';

const DEBUG = false;

const KEYS = {
    RIGHT: "RIGHT",
    LEFT: "LEFT",
    DOWN: 'DOWN',
    UP: 'UP',
    ENTER: 'ENTER',
    BACK_DELETE: 'BACK_DELETE',
    FORWARD_DELETE: 'FORWARD_DELETE',
    SHIFT: 'SHIFT'
};


//for browser
const browsercodes_to_keys = {
    39: KEYS.RIGHT,
    37: KEYS.LEFT,
    40: 'DOWN',
    38: 'UP',
    8: 'BACK_DELETE',
    46: 'FORWARD_DELETE',
    13: 'ENTER',
    16: KEYS.SHIFT
};

const aminocodes_to_keys = {
    286: KEYS.RIGHT,
    285: KEYS.LEFT,
    283: KEYS.UP,
    284: KEYS.DOWN,
    295: KEYS.BACK_DELETE,
    297: KEYS.FORWARD_DELETE,
    294: KEYS.ENTER,
    287: KEYS.SHIFT,
    288: KEYS.SHIFT,

    //macOS (GLFW; see http://www.glfw.org/docs/latest/group__keys.html)
    262: KEYS.RIGHT,
    263: KEYS.LEFT,
    265: KEYS.UP,
    264: KEYS.DOWN,
    259: KEYS.BACK_DELETE,
    257: KEYS.ENTER,
    344: KEYS.SHIFT
};

//American keyboard
const shift_keys = [ 287, 288, 344 ];
const aminoshift_map = { };
const aminocodes_to_chars = {  32: ' ' };
const browsercodes_to_chars = { };
const browsershift_map = { };

exports.init = function () {
    function insertMap(map, start, len, str) {
        for (let i = 0; i < len; i++) {
            map[start + i] = str[i];
        }
    }

    insertMap(aminoshift_map, 48, 10, ')!@#$%^&*(');

    function addKeysWithShift(start, str) {
        if (str.length % 2 != 0) {
            throw new Error('not an even number of chars');
        }

        for (let i = 0; i < str.length / 2; i++) {
            aminocodes_to_chars[start + i] = str[i * 2 + 0];
            aminoshift_map[start + i] = str[i * 2 + 1];
        }
    }

    addKeysWithShift(186, ';:=+,<-_.>/?`~');
    addKeysWithShift(219, '[{\\|]}\'"');
    addKeysWithShift(48, '0)1!2@3#4$5%6^7&8*9(');

    //on my imac keyboard
    aminocodes_to_chars[91] = '[';
    aminoshift_map[91] = '{';
    aminocodes_to_chars[93] = ']';
    aminoshift_map[93] = '}';

    browsercodes_to_chars[49] = '1';
    browsershift_map[49]      = '!';
};

/**
 * Browser key event.
 */
exports.fromBrowserKeyboardEvent = function (evt) {
    if (DEBUG) {
        console.log('processing browser event', evt);
    }

    let shift = false;

    shift = (evt.shift == 1);

    const key = browsercodes_to_keys[evt.keycode];

    if (key) {
        return {
            recognized: true,
            key: key,
            shift: false,
            printable: false,
        };
    }

    if (browsercodes_to_chars[evt.keycode]) {
        let char = browsercodes_to_chars[evt.keycode];

        if (shift && browsershift_map[evt.keycode]) {
            char = browsershift_map[evt.keycode];
        }

        return {
            recognized: true,
            keycode: evt.keycode,
            printable: true,
            char: char
        };
    }

    if (evt.keycode >= 65 && evt.keycode <= 90) {
        let char = String.fromCharCode(evt.keycode);

        if (shift) {
            char = char.toUpperCase();
        } else {
            char = char.toLowerCase();
        }

        return {
            recognized: true,
            keycode: evt.keycode,
            printable: true,
            char: char
        };
    }
};

/**
 * Handle amino native keyboard events.
 */
exports.fromAminoKeyboardEvent = function (evt, states) {
    const keycode = evt.keycode;

    if (DEBUG) {
        console.log('keycode: ' + keycode);
    }

    //check shift
    let shift = false;

    for (let i = 0; i < shift_keys.length; i++) {
        if (states[shift_keys[i]] === true) {
            shift = true;
            break;
        }
    }

    //named function keys
    const key = aminocodes_to_keys[keycode];

    if (key) {
        return {
            recognized: true,
            key: key,
            shift: false,
            printable: false
        };
    }

    //character mappings
    if (aminocodes_to_chars[keycode]) {
        let char = aminocodes_to_chars[keycode];

        if (shift && aminoshift_map[keycode]) {
            char = aminoshift_map[keycode];
        }

        return {
            recognized: true,
            keycode: keycode,
            printable: true,
            char: char
        };
    }

    //save latin1 values
    if (keycode >= 65 && keycode <= 90) {
        let char = String.fromCharCode(keycode);

        if (shift) {
            char = char.toUpperCase();
        } else {
            char = char.toLowerCase();
        }

        return {
            recognized: true,
            keycode: keycode,
            printable: true,
            char: char
        };
    }

    if (DEBUG) {
        console.log('Unknown char ' + keycode + ' ' + String.fromCharCode(keycode));
    }

    /*
    if(evt.printable) {
        return {
            recognized:true,
            printable:evt.printable,
            char: evt.printableChar,
            shift: evt.shift,
        }
    }*/

    return {
        recognized:false,
    };
};

/*
function cookKeyboardEvent(e) {
    if (codes_to_keys[e.keyCode]) {
        return {
            recognized: true,
            KEYS: KEYS,
            key: codes_to_keys[e.keyCode],
            printable: false,
            control: e.ctrlKey,
            shift: e.shiftKey,
            meta: e.metaKey,
        };
    }

    //American keyboard
    const shift_map = { };

    function insertMap(map, start, len, str) {
        for (let i = 0; i < len; i++) {
            map[start +i ] = str[i];
        }
    }

    insertMap(shift_map, 48, 10, ')!@#$%^&*(');

    const codes_to_chars = {  32:' ' };

    function addKeysWithShift(start, str) {
        if (str.length % 2 != 0) {
            throw new Error("not an even number of chars");
        }

        for (let i = 0; i < str.length / 2; i++) {
            codes_to_chars[start + i] = str[i * 2 + 0];
            shift_map[start + i] = str[i * 2 + 1];
        }
    }

    addKeysWithShift(186, ';:=+,<-_.>/?`~');
    addKeysWithShift(219, '[{\\|]}\'"');
    addKeysWithShift(48, '0)1!2@3#4$5%6^7&8*9(');

    // /console.log("keycode = ", e.keyCode);

    if (codes_to_chars[e.keyCode]) {
        let char = codes_to_chars[e.keyCode];

        if (e.shiftKey && shift_map[e.keyCode]) {
            char = shift_map[e.keyCode];
        }

        return {
            recognized: true,
            KEYS: KEYS,
            printable: true,
            char: char,
            control: e.ctrlKey,
            shift: e.shiftKey,
            meta: e.metaKey
        };
    }

    //alphabet
    if (e.keyCode >= 65 && e.keyCode <= 90) {
        let char = String.fromCharCode(e.keyCode);

        if (e.shiftKey) {
            char = char.toUpperCase();
        } else {
            char = char.toLowerCase();
        }

        return {
            recognized: true,
            KEYS: KEYS,
            printable: true,
            char: char,
            control: e.ctrlKey,
            shift: e.shiftKey,
            meta: e.metaKey
        };
    }

    return {
        recognized: false
    };
}
*/