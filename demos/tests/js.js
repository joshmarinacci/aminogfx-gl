'use strict';

/*
 * Inheritance Test.
 *
 *  - extend a native class
 *  - overload constructor and method
 */

//
// String
//

/**
 * Extend String.
 */
class MyString extends String {
   constructor(text) {
       super(text);

       console.log('MyString: constructor');

       this.indexOf('a');
   }

   indexOf(ch) {
       console.log('MyString.indexOf() ' + ch);

       return super.indexOf(ch);
   }
}

const str1 = new MyString('test');

str1.indexOf('.');

//
// AminoGfx.Polygon
//

const amino = require('../../main.js');

const AminoGfx = amino.AminoGfx;
const Polygon = AminoGfx.Polygon;

/**
 * Extend AminoGfx Class.
 */
class Circle extends Polygon {
   constructor(amino) {
       super(amino);

       console.log('Circle: constructor');

       /*
        * Issues:
        *
        *   - Node v6.9.1
        *     - TypeError: this.setup is not a function
        *
        * Works fine:
        *
        *   - Node v4.x
        *   - Node v7.0.0
        */
       this.setup();
   }

   setup() {
       console.log('Circle.setup()');
   }
}

const gfx = new AminoGfx();

new Circle(gfx);