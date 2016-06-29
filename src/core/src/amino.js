'use strict';

var amino = exports;
var input = require('./aminoinput');
var prims = require('./primitives');

exports.input = input;
exports.primitives = prims;

/**
 * Create properties.
 */
amino.makeProps = function (obj, props) {
    for (var name in props) {
        amino.makeProp(obj, name, props[name]);
    }

    return obj;
};

/**
 * Create property handlers.
 *
 * @param obj object reference.
 * @param name property name.
 * @param val default value.
 */
amino.makeProp = function (obj, name, val) {
    /**
     * Property function.
     *
     * Getter and setter.
     */
    var prop = function (v) {
        if (v != undefined) {
            return prop.set(v, obj);
        } else {
            return prop.get();
        }
    };

    prop.value = val;
    prop.propname = name;
    prop.listeners = [];

    /**
     * Add watch callback.
     *
     * Callback: (value, property, object)
     */
    prop.watch = function (fun) {
        if (fun === undefined) {
            throw new Error('function undefined for property ' + name + ' on object with value ' + val);
        }

        this.listeners.push(fun);

        return this;
    };

    /**
     * Getter function.
     */
    prop.get = function () {
        return this.value;
    };

    /**
     * Setter function.
     */
    prop.set = function (v, obj) {
        //check if modified
        if (v === this.value) {
            //debug
            //console.log('not changed: ' + name);

            return obj;
        }

        //update & fire listeners
        this.value = v;

        for (var i = 0; i < this.listeners.length; i++) {
            this.listeners[i](this.value, this, obj);
        }

        return obj;
    };

    /**
     * Create animation.
     */
    prop.anim = function () {
        return amino.getCore().getNative().createPropAnim(obj, name);
    };

    /**
     * Bind to other property.
     *
     * Optional: callback to modify value.
     */
    prop.bindto = function (prop, fun) {
        var set = this;

        prop.listeners.push(function (v) {
            if (fun) {
                set(fun(v));
            } else {
                set(v);
            }
        });

        return this;
    };

    //attach
    obj[name] = prop;
};

//String extension (polyfill for < ES 6)
if (typeof String.prototype.endsWith !== 'function') {
    String.prototype.endsWith = function (suffix) {
        return this.indexOf(suffix, this.length - suffix.length) !== -1;
    };
}

amino.getCore = function () {
    return Core._core;
};

//basic
amino.Group = prims.Group;
amino.Rect = prims.Rect;
amino.Text = prims.Text;
amino.Polygon = prims.Polygon;
amino.ImageView = prims.ImageView;
amino.Circle = prims.Circle;

//extended
amino.PixelView    = prims.PixelView;
amino.RichTextView = prims.RichTextView;
