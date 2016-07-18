'use strict';

var amino = exports;
var input = require('./aminoinput');
var prims = require('./primitives');

exports.input = input;
exports.primitives = prims;

//basic
amino.Text = prims.Text;
amino.ImageView = prims.ImageView;

//extended
amino.PixelView    = prims.PixelView;
amino.RichTextView = prims.RichTextView;
