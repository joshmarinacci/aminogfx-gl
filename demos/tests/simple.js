'use strict';

const amino = require('../../main.js');

//basic tests
const gfx = new amino.AminoGfx();

//methods
gfx.method = function () {
    console.log('method() called');

    //check this
    if (this != gfx) {
        console.log('invalid this! ' + JSON.stringify(this));
    }

    //call method2
    this.method2();
};

gfx.method2 = function () {
    console.log('method 2');
};

//method call test
console.log('test: ' + gfx.test());

//same from C++ code
gfx.test2();